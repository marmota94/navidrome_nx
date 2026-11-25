#include "song_cache.hpp"
#include <cassert>
#include <iostream>
int main() {
    auto root = std::filesystem::path("/tmp/navidrome-song-cache-test");
    std::filesystem::remove_all(root);
    SongCache cache(root);
    auto config = jsonOwner(json_pack("{s:s,s:s,s:s}", "server", "https://example.test", "username", "user", "password", "secret"));
    Api api(config.get());
    assert(cache.load(api, "missing").empty() && !std::filesystem::exists(root));
    const std::vector<char> bytes{'m','p','3'};
    cache.save(api, "../escape /&", bytes);
    assert(cache.load(api, "../escape /&") == bytes);
    auto first = *std::filesystem::directory_iterator(root);
    std::filesystem::last_write_time(first.path(), std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));
    for (int i = 0; i < 14; ++i) cache.save(api, std::to_string(i), bytes);
    assert(std::distance(std::filesystem::directory_iterator(root), std::filesystem::directory_iterator{}) == 15);
    std::ofstream(root / "keep.txt") << "unrelated";
    cache.save(api, "extra", bytes);
    assert(cache.load(api, "../escape /&").empty());
    assert(cache.load(api, "extra") == bytes && std::filesystem::exists(root / "keep.txt"));
    const auto count = std::distance(std::filesystem::directory_iterator(root), std::filesystem::directory_iterator{});
    cache.save(api, "extra", bytes);
    assert(std::distance(std::filesystem::directory_iterator(root), std::filesystem::directory_iterator{}) == count);
    json_object_set_new(config.get(), "username", json_string("other")); Api other(config.get());
    assert(cache.load(other, "extra").empty());
    json_object_set_new(config.get(), "server", json_string("https://other.test")); Api server(config.get());
    assert(cache.load(server, "extra").empty());
    bool rejected = false;
    try { cache.save(api, "empty", {}); } catch (...) { rejected = true; }
    assert(rejected);
    for (const auto& file : std::filesystem::directory_iterator(root)) {
        if (file.path().extension() == ".mp3") { std::ofstream(file.path()).close(); }
    }
    for (int i = 0; i < 14; ++i) cache.load(api, std::to_string(i));
    assert(cache.load(api, "extra").empty());
    assert(std::distance(std::filesystem::directory_iterator(root), std::filesystem::directory_iterator{}) == 1);
    cache.save(api, "erase", bytes); cache.erase(api, "erase"); assert(cache.load(api, "erase").empty());
    cache.save(api, "named", bytes);
    cache.save(api, "legacy", bytes);
    Entry track; track.id = "named"; track.title = "My track"; track.artist = "An artist";
    cache.describe(api, track);
    auto downloads = cache.downloads();
    assert(downloads.size() == 2);
    auto named = std::find_if(downloads.begin(), downloads.end(), [](const auto& item) { return item.title == "My track · An artist"; });
    assert(named != downloads.end() && named->bytes == bytes.size());
    bool unsafe = false;
    try { cache.eraseDownload("../keep.txt"); } catch (...) { unsafe = true; }
    assert(unsafe && std::filesystem::exists(root / "keep.txt"));
    cache.eraseDownload(named->filename);
    assert(!std::filesystem::exists(root / (named->filename + ".json")));
    assert(cache.downloads().size() == 1 && cache.load(api, "legacy") == bytes);
    std::filesystem::remove_all(root);
    std::ofstream(root) << "not a directory";
    rejected = false; try { cache.save(api, "fail", bytes); } catch (...) { rejected = true; }
    assert(rejected);
    std::filesystem::remove(root);
    std::cout << "15-song cache, eviction, account separation and storage failure checks passed\n";
}
