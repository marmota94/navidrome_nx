// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "api.hpp"
#include "favorites.hpp"
#include <chrono>
#include <unistd.h>

namespace overlay {
constexpr size_t PageSize = 10;
constexpr size_t PrefetchCount = 2;
enum class Collection { Favorites, Albums, Artists, ArtistAlbums, AlbumSongs };
struct Page {
    Collection collection = Collection::Albums;
    std::string id, title, cover;
    size_t offset = 0, focus = 0;
    bool more = false;
    std::vector<Entry> items;
    std::vector<Entry> lookAhead;
};
inline std::string salt() {
    return std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}
inline std::vector<Entry> slice(const std::vector<Entry>& entries, size_t offset, bool& more) {
    more = offset < entries.size() && entries.size() - offset > PageSize;
    if (offset >= entries.size()) return {};
    return {entries.begin() + offset, entries.begin() + std::min(entries.size(), offset + PageSize)};
}
inline std::vector<Entry> playbackWindow(const Page& page, size_t selected) {
    if (selected >= page.items.size() || page.items[selected].kind != EntryKind::Song) return {};
    const auto visible = std::min(PrefetchCount + 1, page.items.size() - selected);
    std::vector<Entry> tracks(page.items.begin() + selected, page.items.begin() + selected + visible);
    const auto extra = std::min(PrefetchCount + 1 - visible, page.lookAhead.size());
    tracks.insert(tracks.end(), page.lookAhead.begin(), page.lookAhead.begin() + extra);
    return tracks;
}
inline Page browse(const Api& api, Page page, const char* favoritesPath = "sdmc:/switch/navidrome_nx/overlay-library.tmp") {
    std::vector<Entry> all;
    const auto token = salt();
    auto get = [&](const char* endpoint, std::vector<std::pair<std::string, std::string>> params = {}) {
        return Api::response(api.request(endpoint, params, token, 512 * 1024));
    };
    switch (page.collection) {
        case Collection::Favorites: {
            const char* path = favoritesPath;
            auto cleanup = [path](FILE* file) { std::fclose(file); ::unlink(path); };
            std::unique_ptr<FILE, decltype(cleanup)> file(std::fopen(path, "w+b"), cleanup);
            if (!file) throw std::runtime_error("Cannot write temporary Favorites response");
            api.request("getStarred2", {}, token, 8 * 1024 * 1024, file.get());
            if (std::fflush(file.get()) || std::fseek(file.get(), 0, SEEK_SET))
                throw std::runtime_error("Cannot read temporary Favorites response");
            bool beyondWindow = false;
            page.items = readFavorites(file.get(), page.offset, PageSize + PrefetchCount, beyondWindow);
            page.more = beyondWindow || page.items.size() > PageSize;
            page.lookAhead.clear();
            if (page.items.size() > PageSize) {
                page.lookAhead.assign(page.items.begin() + PageSize, page.items.end());
                page.items.resize(PageSize);
            }
            return page;
        }
        case Collection::Albums: {
            auto root = get("getAlbumList2", {{"type", "alphabeticalByName"}, {"size", std::to_string(PageSize + 1)}, {"offset", std::to_string(page.offset)}});
            all = Api::entries(root.get(), "albumList2", "album");
            page.items = slice(all, 0, page.more); return page;
        }
        case Collection::Artists: {
            auto root = get("getArtists");
            auto indices = json_object_get(json_object_get(root.get(), "artists"), "index");
            size_t i; json_t* index;
            json_array_foreach(indices, i, index) {
                auto wrapper = jsonOwner(json_pack("{s:O}", "index", index));
                auto artists = Api::entries(wrapper.get(), "index", "artist");
                all.insert(all.end(), artists.begin(), artists.end());
            }
            break;
        }
        case Collection::ArtistAlbums: {
            auto root = get("getArtist", {{"id", page.id}}); all = Api::entries(root.get(), "artist", "album"); break;
        }
        case Collection::AlbumSongs: {
            auto root = get("getAlbum", {{"id", page.id}}); all = Api::entries(root.get(), "album", "song");
            for (auto& item : all) if (item.coverArt.empty()) item.coverArt = page.cover;
            break;
        }
    }
    page.items = slice(all, page.offset, page.more);
    page.lookAhead.clear();
    if (page.more && page.collection == Collection::AlbumSongs) {
        const size_t next = page.offset + page.items.size();
        const auto count = std::min(PrefetchCount, all.size() - next);
        page.lookAhead.assign(all.begin() + next, all.begin() + next + count);
    }
    return page;
}
inline std::string elapsed(uint32_t frames, uint32_t rate) {
    const auto seconds = rate ? frames / rate : 0;
    return std::to_string(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60);
}
}
