// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "api.hpp"
#include "../../vendor/aurore/library/include/aurore/extern/nlohmann/json.hpp"
#include <limits>

namespace overlay {
// getStarred2 has no pagination. Consume the document without building a DOM
// or retaining songs outside this page (including the unused albums/artists).
class FavoritesParser : public nlohmann::json_sax<nlohmann::json> {
    struct Frame { std::string name, key; bool array; };
    std::vector<Frame> stack;
    size_t offset, limit, seen = 0;
    Entry song;
    bool hasId = false, hasTitle = false, body = false, ok = false;
    long long errorCode = 0;
    bool at(std::initializer_list<const char*> names) const {
        if (stack.size() != names.size()) return false;
        size_t i = 0;
        for (auto name : names) if (stack[i++].name != name) return false;
        return true;
    }
    bool entry() const { return at({"", "subsonic-response", "starred2", "song", "#"}); }
    bool retain() const { return seen >= offset && items.size() < limit; }
    bool begin(bool array) {
        if (stack.size() >= 32) return false;
        auto name = stack.empty() ? "" : stack.back().array ? "#" : stack.back().key;
        stack.push_back({name, "", array});
        if (!array && at({"", "subsonic-response"})) body = true;
        if (!array && entry()) { song = {}; hasId = hasTitle = false; }
        return true;
    }
    bool number(long long value) {
        if (entry() && retain() && stack.back().key == "duration") song.duration = std::max(0LL, value);
        if (at({"", "subsonic-response", "error"}) && stack.back().key == "code") errorCode = value;
        return true;
    }
public:
    std::vector<Entry> items;
    bool more = false;
    FavoritesParser(size_t offset, size_t limit) : offset(offset), limit(limit) {}
    bool null() override { return true; }
    bool boolean(bool) override { return true; }
    bool number_integer(number_integer_t value) override { return number(value); }
    bool number_unsigned(number_unsigned_t value) override {
        return number(static_cast<long long>(std::min<uint64_t>(value, std::numeric_limits<long long>::max())));
    }
    bool number_float(number_float_t, const string_t&) override { return true; }
    bool binary(binary_t&) override { return false; }
    bool string(string_t& value) override {
        if (at({"", "subsonic-response"}) && stack.back().key == "status") ok = value == "ok";
        if (!entry() || !retain()) return true;
        const auto& key = stack.back().key;
        if (key == "id") { song.id = value; hasId = true; }
        else if (key == "title") { song.title = value; hasTitle = true; }
        else if (key == "artist") song.artist = value;
        else if (key == "coverArt") song.coverArt = value;
        return true;
    }
    bool key(string_t& value) override { stack.back().key = value; return true; }
    bool start_object(std::size_t) override { return begin(false); }
    bool start_array(std::size_t) override { return begin(true); }
    bool end_object() override {
        if (entry()) {
            if (retain()) {
                if (!hasId || !hasTitle) throw ApiError("invalid_library");
                items.push_back(std::move(song));
            } else if (seen >= offset) more = true;
            ++seen;
        }
        stack.pop_back(); return true;
    }
    bool end_array() override { stack.pop_back(); return true; }
    bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) override { return false; }
    void validate() const {
        if (!body) throw ApiError("invalid_response");
        if (!ok) throw ApiError("subsonic", std::to_string(errorCode));
    }
};
inline std::vector<Entry> readFavorites(FILE* file, size_t offset, size_t count, bool& more) {
    FavoritesParser parser(offset, count);
    if (!nlohmann::json::sax_parse(file, &parser)) throw ApiError("invalid_response");
    parser.validate(); more = parser.more;
    return std::move(parser.items);
}
}
