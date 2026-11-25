#pragma once
#include <curl/curl.h>
#include <jansson.h>
#include <mbedtls/md5.h>
#include <atomic>
#include <cstdio>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "lyrics.hpp"

using Json = std::unique_ptr<json_t, decltype(&json_decref)>;
inline Json jsonOwner(json_t* value) { return Json(value, json_decref); }
struct ApiError : std::runtime_error {
    std::string key, detail;
    explicit ApiError(std::string key, std::string detail = "")
        : std::runtime_error(key + (detail.empty() ? "" : ": " + detail)), key(std::move(key)), detail(std::move(detail)) {}
};
inline std::string field(json_t* object, const char* key) {
    auto value = json_string_value(json_object_get(object, key));
    if (!value) throw ApiError("missing_field", key);
    return value;
}
enum class EntryKind { Song, Album, Artist, Playlist };
struct Entry {
    std::string id, title, artist;
    long long duration = 0;
    std::string coverArt;
    EntryKind kind = EntryKind::Song;
    std::string created;
    std::string suffix;
    long long bitRate = 0, samplingRate = 0, bitDepth = 0, channelCount = 0, songCount = 0;
};
struct Transfer {
    std::vector<char> bytes;
    size_t limit;
    bool overflow = false;
    FILE* file = nullptr;
    size_t written = 0;
    bool allocationFailed = false;
    static size_t write(char* data, size_t size, size_t count, void* context) {
        auto& out = *static_cast<Transfer*>(context);
        if (size && count > (out.limit - out.written) / size) {
            out.overflow = true;
            return 0;
        }
        if (out.file) {
            const auto bytes = std::fwrite(data, 1, size * count, out.file);
            out.written += bytes;
            return bytes;
        }
        try { out.bytes.insert(out.bytes.end(), data, data + size * count); }
        catch (...) { out.allocationFailed = true; return 0; }
        out.written += size * count;
        return size * count;
    }
};

class Api {
    std::string server, username, password;
    std::atomic<bool> cancelled{false};
    static int progress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
        return static_cast<const Api*>(context)->cancelled.load() ? 1 : 0;
    }
public:
    void cancel() { cancelled.store(true); }
    const std::string& serverUrl() const { return server; }
    const std::string& userName() const { return username; }
    explicit Api(json_t* config) : server(field(config, "server")),
        username(field(config, "username")), password(field(config, "password")) {
        while (!server.empty() && server.back() == '/') server.pop_back();
        if ((server.rfind("https://", 0) != 0 && server.rfind("http://", 0) != 0)
            || server.find_first_of("?#\r\n") != std::string::npos || server.find('@') != std::string::npos
            || username.empty() || password.empty())
            throw ApiError("invalid_config");
    }
    static std::string encode(CURL* curl, const std::string& text) {
        char* encoded = curl_easy_escape(curl, text.c_str(), static_cast<int>(text.size()));
        if (!encoded) throw ApiError("encoding");
        std::string result(encoded);
        curl_free(encoded);
        return result;
    }
    std::vector<char> request(const std::string& endpoint,
        const std::vector<std::pair<std::string, std::string>>& params,
        const std::string& salt, size_t limit = 4 * 1024 * 1024, FILE* file = nullptr) const {
        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
        if (!curl) throw ApiError("http_client");
        unsigned char digest[16];
        const auto input = password + salt;
        if (mbedtls_md5_ret(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest))
            throw ApiError("auth_hash");
        std::string token;
        for (auto byte : digest) {
            token += "0123456789abcdef"[byte >> 4];
            token += "0123456789abcdef"[byte & 15];
        }
        auto url = server + "/rest/" + endpoint + ".view?u=" + encode(curl.get(), username)
            + "&t=" + token + "&s=" + encode(curl.get(), salt) + "&v=1.16.1&c=NavidromeNX&f=json";
        for (const auto& param : params)
            url += "&" + encode(curl.get(), param.first) + "=" + encode(curl.get(), param.second);
        Transfer output{{}, limit, false, file};
        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
#if LIBCURL_VERSION_NUM >= 0x075500
        curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "http,https");
#else
        curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
        // Redirects are deliberately rejected so authentication tokens stay on this server.
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, endpoint == "stream" ? 180L : 30L);
        curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progress);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, Transfer::write);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &output);
        auto result = curl_easy_perform(curl.get());
        if (output.overflow) throw ApiError("response_limit");
        if (output.allocationFailed) throw ApiError("response_memory");
        if (result != CURLE_OK) throw ApiError("network", curl_easy_strerror(result));
        long status = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
        if (status != 200) throw ApiError("http_status", std::to_string(status));
        return std::move(output.bytes);
    }
    static Json response(const std::vector<char>& bytes) {
        auto root = jsonOwner(json_loadb(bytes.data(), bytes.size(), JSON_REJECT_DUPLICATES, nullptr));
        auto body = json_object_get(root.get(), "subsonic-response");
        if (!body) throw ApiError("invalid_response");
        if (field(body, "status") != "ok") {
            auto error = json_object_get(body, "error");
            auto code = json_integer_value(json_object_get(error, "code"));
            throw ApiError("subsonic", std::to_string(code));
        }
        return jsonOwner(json_incref(body));
    }
    static std::vector<Entry> entries(json_t* body, const char* group, const char* item) {
        auto array = json_object_get(json_object_get(body, group), item);
        if (!array) return {};
        if (!json_is_array(array)) throw ApiError("invalid_library");
        std::vector<Entry> result;
        size_t index; json_t* value;
        json_array_foreach(array, index, value) {
            auto artist = json_string_value(json_object_get(value, "artist"));
            auto duration = json_integer_value(json_object_get(value, "duration"));
            auto cover = json_string_value(json_object_get(value, "coverArt"));
            auto created = json_string_value(json_object_get(value, "created"));
            const auto suffix = json_string_value(json_object_get(value, "suffix"));
            auto number = [value](const char* key) { return std::max<json_int_t>(0, json_integer_value(json_object_get(value, key))); };
            const auto kind = std::string(item) == "album" ? EntryKind::Album : std::string(item) == "artist" ? EntryKind::Artist : std::string(item) == "playlist" ? EntryKind::Playlist : EntryKind::Song;
            result.push_back({field(value, "id"), field(value, kind == EntryKind::Song ? "title" : "name"), artist ? artist : "", duration > 0 ? duration : 0, cover ? cover : "", kind, created ? created : "", suffix ? suffix : "", number("bitRate"), number("samplingRate"), number("bitDepth"), number("channelCount"), number("songCount")});
        }
        return result;
    }
    static SongLyrics lyrics(json_t* body) {
        SongLyrics result;
        auto structured = json_object_get(json_object_get(body, "lyricsList"), "structuredLyrics");
        size_t index; json_t* track;
        json_array_foreach(structured, index, track) {
            result.offsetMs = json_integer_value(json_object_get(track, "offset"));
            result.synced = json_is_true(json_object_get(track, "synced"));
            long long previousStart = -1;
            auto lines = json_object_get(track, "line");
            size_t lineIndex; json_t* line;
            json_array_foreach(lines, lineIndex, line) {
                auto text = json_string_value(json_object_get(line, "value"));
                if (text) {
                    if (result.lines.size() >= 2000) throw ApiError("lyrics_limit");
                    auto timestamp = json_object_get(line, "start");
                    const long long start = json_is_integer(timestamp) ? json_integer_value(timestamp) : -1;
                    if (start < 0 || start < previousStart) result.synced = false;
                    previousStart = start;
                    result.lines.push_back({text, start});
                }
            }
            if (!result.empty()) return result; // Display the first available language/version.
        }
        auto value = json_string_value(json_object_get(json_object_get(body, "lyrics"), "value"));
        if (value) {
            std::string text(value);
            size_t start = 0;
            while (start < text.size()) {
                auto end = text.find('\n', start);
                auto line = text.substr(start, end == std::string::npos ? end : end - start);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (result.lines.size() >= 2000) throw ApiError("lyrics_limit");
                result.lines.push_back({line, -1});
                if (end == std::string::npos) break;
                start = end + 1;
            }
        }
        result.synced = false;
        result.offsetMs = 0;
        return result;
    }
};
