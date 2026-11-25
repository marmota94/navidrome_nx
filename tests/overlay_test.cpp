#include "../overlay/source/cache.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>

int main(int argc, char** argv) {
    assert(argc == 2);
    auto config = jsonOwner(json_pack("{s:s,s:s,s:s}", "server", argv[1], "username", "a user+&", "password", "s3cret"));
    Api api(config.get());
    auto root = std::filesystem::temp_directory_path() / ("navidrome-overlay-" + std::to_string(getpid()));
    std::filesystem::remove_all(root); std::filesystem::create_directory(root);
    std::set<std::string> protectedFiles;
    Entry entry; entry.id = "../../track /&"; entry.title = "Song / á"; entry.artist = "Artist"; entry.coverArt = "cover /&";
    auto path = overlay::cacheTrack(api, entry, protectedFiles, root.string());
    assert(std::filesystem::path(path).parent_path() == root);
    assert(overlay::cacheName(std::filesystem::path(path).filename()));
    assert(!overlay::cacheName("../../track.mp3") && !overlay::cacheName(std::string(32, 'G') + ".mp3"));
    assert(protectedFiles.count(path));
    std::ifstream file(path); std::string bytes((std::istreambuf_iterator<char>(file)), {});
    assert(bytes == "fake mp3");
    auto metadata = overlay::readMetadata(path, root.string());
    assert(metadata.id == entry.id && metadata.title == entry.title && metadata.coverArt == entry.coverArt);
    assert(overlay::readMetadata(root.string() + "/../secret.mp3", root.string()).id.empty());
    assert(overlay::readMetadata("/etc/passwd", root.string()).id.empty());
    auto samePath = overlay::cacheTrack(api, entry, protectedFiles, root.string());
    assert(samePath == path);
    auto originalKey = overlay::cacheKey(api, entry.id);
    json_object_set_new(config.get(), "username", json_string("other")); Api other(config.get());
    assert(overlay::cacheKey(other, entry.id) != originalKey);
    bool full = false;
    try { overlay::reserveCache(root.string(), protectedFiles, overlay::CacheLimit); } catch (...) { full = true; }
    assert(full && std::filesystem::exists(path));
    std::ofstream(root / "keep.txt") << "unrelated";
    std::ofstream(path + ".part") << "interrupted download";
    overlay::reserveCache(root.string(), {}, overlay::CacheLimit);
    assert(!std::filesystem::exists(path + ".part"));
    assert(!std::filesystem::exists(path) && !std::filesystem::exists(path + ".json"));
    assert(std::filesystem::exists(root / "keep.txt"));
    api.cancel(); bool cancelled = false;
    try { overlay::cacheTrack(api, entry, protectedFiles, root.string()); } catch (...) { cancelled = true; }
    assert(cancelled && !std::filesystem::exists(path) && !std::filesystem::exists(path + ".part"));
    json_object_set_new(config.get(), "username", json_string("a user+&")); Api library(config.get());
    overlay::Page page; page.collection = overlay::Collection::Favorites;
    const auto favoritesPath = (root / "favorites.tmp").string();
    auto favorites = overlay::browse(library, page, favoritesPath.c_str());
    assert(favorites.items.size() == 1 && favorites.items[0].id == "favorite-track" && !favorites.more);
    assert(!std::filesystem::exists(favoritesPath));
    json_object_set_new(config.get(), "username", json_string("large-favorites"));
    Api largeLibrary(config.get()); page.offset = 4995;
    const auto largeFavorites = overlay::browse(largeLibrary, page, favoritesPath.c_str());
    assert(largeFavorites.items.size() == 5 && !largeFavorites.more && largeFavorites.items.front().id == "favorite-4995");
    assert(!std::filesystem::exists(favoritesPath));
    largeLibrary.cancel(); bool favoritesCancelled = false;
    try { overlay::browse(largeLibrary, page, favoritesPath.c_str()); }
    catch (const ApiError&) { favoritesCancelled = true; }
    assert(favoritesCancelled && !std::filesystem::exists(favoritesPath));
    page.offset = 0;
    // A response much larger than the old 512 KiB cap must retain only a page.
    auto large = std::unique_ptr<FILE, decltype(&std::fclose)>(std::tmpfile(), std::fclose);
    assert(large);
    std::fputs("{\"subsonic-response\":{\"status\":\"ok\",\"starred2\":{\"album\":[{\"id\":\"ignored\"}],\"song\":[", large.get());
    const std::string padding(1024, 'x');
    for (int i = 0; i < 5000; ++i)
        std::fprintf(large.get(), "%s{\"id\":\"song-%d\",\"title\":\"Title %d\",\"artist\":\"Artist\",\"duration\":123,\"ignored\":\"%s\"}", i ? "," : "", i, i, padding.c_str());
    std::fputs("]}}}", large.get());
    std::rewind(large.get()); bool largeMore = false;
    auto largePage = overlay::readFavorites(large.get(), 4980, 10, largeMore);
    assert(largePage.size() == 10 && largeMore && largePage.front().id == "song-4980" && largePage.back().id == "song-4989");
    assert(largePage.front().duration == 123 && largePage.front().artist == "Artist");
    std::rewind(large.get());
    auto lastPage = overlay::readFavorites(large.get(), 4995, 10, largeMore);
    assert(lastPage.size() == 5 && !largeMore && lastPage.front().id == "song-4995");
    auto invalid = std::unique_ptr<FILE, decltype(&std::fclose)>(std::tmpfile(), std::fclose);
    std::fputs("{\"subsonic-response\":{\"status\":\"failed\",\"error\":{\"code\":40}}}", invalid.get());
    std::rewind(invalid.get()); bool denied = false;
    try { overlay::readFavorites(invalid.get(), 0, 10, largeMore); }
    catch (const ApiError& error) { denied = error.key == "subsonic" && error.detail == "40"; }
    assert(denied);
    page.collection = overlay::Collection::Albums;
    assert(overlay::browse(library, page).items[0].id == "id /&");
    page.collection = overlay::Collection::Artists;
    auto artists = overlay::browse(library, page);
    assert(artists.items.size() == 10 && artists.more && artists.items[0].kind == EntryKind::Artist);
    page.offset = 10; auto second = overlay::browse(library, page);
    assert(second.items.size() == 3 && !second.more && second.items[0].id == "artist-10");
    page = {}; page.collection = overlay::Collection::ArtistAlbums; page.id = "artist /&";
    assert(overlay::browse(library, page).items[0].kind == EntryKind::Album);
    page.collection = overlay::Collection::AlbumSongs; page.id = "id /&"; page.cover = "fallback-cover";
    auto songs = overlay::browse(library, page);
    assert(songs.items[0].id == "track" && songs.items[0].coverArt == "fallback-cover");
    auto single = overlay::playbackWindow(songs, 0);
    assert(single.size() == 1 && single[0].id == "track");
    assert(overlay::playbackWindow(songs, 1).empty());
    assert(overlay::playbackWindow(artists, 0).empty());
    page.id = "overlay-long";
    auto firstPage = overlay::browse(library, page);
    assert(firstPage.items.size() == 10 && firstPage.lookAhead.size() == 2);
    auto window = overlay::playbackWindow(firstPage, 4);
    assert(window.size() == 3 && window[0].id == "track-4" && window[2].id == "track-6");
    window = overlay::playbackWindow(firstPage, 9);
    assert(window.size() == 3 && window[0].id == "track-9");
    assert(window[1].id == "track-10" && window[2].id == "track-11" && window[2].coverArt == "fallback-cover");
    page.offset = overlay::PageSize;
    auto lastAlbumPage = overlay::browse(library, page);
    assert(lastAlbumPage.lookAhead.empty() && overlay::playbackWindow(lastAlbumPage, 1).size() == 1);
    bool more = false; assert(overlay::slice(songs.items, SIZE_MAX, more).empty() && !more);
    assert(overlay::elapsed(44100 * 65, 44100) == "1:05" && overlay::elapsed(4000, 0) == "0:00");
    std::filesystem::remove_all(root);
    std::cout << "Overlay browsing, cache, account separation and cancellation checks passed\n";
}
