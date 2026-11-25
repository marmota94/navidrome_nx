#include "settings.hpp"
#include "library_logic.hpp"
#include "album_grid.hpp"
#include <cassert>
#include <filesystem>
int main() {
    assert(adjustSetting(0, 0, 3, -1) == 3 && adjustSetting(3, 0, 3, 1) == 0);
    assert(adjustSetting(0, 0, 10, -1) == 10);
    assert(adjustSetting(10, 0, 10, 1) == 0);
    assert(adjustSetting(5, 0, 10, -1) == 4 && adjustSetting(5, 0, 10, 1) == 6);
    assert(adjustSetting(4, 4, 6, -1) == 6 && adjustSetting(6, 4, 6, 1) == 4);
    const std::string path = "/tmp/navidrome-settings-test.json";
    std::remove(path.c_str());
    assert(loadSettings(path.c_str()).columns == 4);
    assert(loadSettings(path.c_str()).backgroundStyle == 0);
    assert(loadSettings(path.c_str()).bubbleCount == 5);
    assert(loadSettings(path.c_str()).backgroundOpacity == 80);
    assert(!loadSettings(path.c_str()).cacheSongs);
    assert(loadSettings(path.c_str()).visualizer == 0);
    assert(loadSettings(path.c_str()).equalizer == EqualizerGains{});
    Settings value; value.cacheSongs = true; value.columns = 6; value.background = 3; value.remoteEnabled = true;
    saveSettings(value, path.c_str());
    assert(loadSettings(path.c_str()).columns == 6);
    assert(loadSettings(path.c_str()).cacheSongs);
    value.equalizer = {-6, 3, 6};
    saveSettings(value, path.c_str());
    assert(loadSettings(path.c_str()).equalizer == value.equalizer);
    value.equalizer[2] = 7;
    bool invalidGain = false;
    try { saveSettings(value, path.c_str()); } catch (...) { invalidGain = true; }
    assert(invalidGain && loadSettings(path.c_str()).equalizer[2] == 6);
    value.equalizer = {};
    value.columns = 5; saveSettings(value, path.c_str());
    assert(loadSettings(path.c_str()).columns == 5);
    assert(std::rename(path.c_str(), (path + ".bak").c_str()) == 0);
    assert(loadSettings(path.c_str()).columns == 5);
    saveSettings(value, path.c_str());
    assert(!std::filesystem::exists(path + ".bak"));
    for (int style = 0; style <= 2; ++style) {
        value.visualizer = style; saveSettings(value, path.c_str());
        assert(loadSettings(path.c_str()).visualizer == style);
    }
    value.visualizer = 3;
    bool badVisualizer = false;
    try { saveSettings(value, path.c_str()); } catch (...) { badVisualizer = true; }
    assert(badVisualizer && loadSettings(path.c_str()).visualizer == 2);
    value.visualizer = 0;
    for (int background = 0; background <= 6; ++background) {
        value.background = background; saveSettings(value, path.c_str());
        assert(loadSettings(path.c_str()).background == background);
    }
    for (int style : {0, 1}) {
        value.backgroundStyle = style; saveSettings(value, path.c_str());
        assert(loadSettings(path.c_str()).backgroundStyle == style);
    }
    for (int count = 0; count <= static_cast<int>(MaxBubbleCount); ++count) {
        value.bubbleCount = count; saveSettings(value, path.c_str());
        assert(loadSettings(path.c_str()).bubbleCount == count);
    }
    for (int invalid : {-1, 11}) {
        value.bubbleCount = invalid;
        bool badCount = false;
        try { saveSettings(value, path.c_str()); } catch (...) { badCount = true; }
        assert(badCount && loadSettings(path.c_str()).bubbleCount == 10);
    }
    value.bubbleCount = 5;
    for (int opacity : {0, 10, 80, 100}) {
        value.backgroundOpacity = opacity; saveSettings(value, path.c_str());
        assert(loadSettings(path.c_str()).backgroundOpacity == opacity);
    }
    for (int opacity : {-1, 101}) {
        value.backgroundOpacity = opacity;
        bool rejectedOpacity = false; try { saveSettings(value, path.c_str()); } catch (...) { rejectedOpacity = true; }
        assert(rejectedOpacity && loadSettings(path.c_str()).backgroundOpacity == 100);
    }
    value.backgroundOpacity = 80;
    value.backgroundStyle = 2;
    bool badStyle = false;
    try { saveSettings(value, path.c_str()); } catch (...) { badStyle = true; }
    assert(badStyle && loadSettings(path.c_str()).backgroundStyle == 1);
    value.backgroundStyle = 1;
    value.background = 7;
    bool badBackground = false;
    try { saveSettings(value, path.c_str()); } catch (...) { badBackground = true; }
    assert(badBackground && loadSettings(path.c_str()).background == 6);
    value.background = 5;
    value.columns = 7;
    bool rejected = false;
    try { saveSettings(value, path.c_str()); } catch (...) { rejected = true; }
    assert(rejected && loadSettings(path.c_str()).columns == 5);
    auto bad = jsonOwner(json_loads("{\"showClock\":1}", 0, nullptr));
    rejected = false; try { parseSettings(bad.get()); } catch (...) { rejected = true; } assert(rejected);
    json_object_set_new(bad.get(), "cacheSongs", json_integer(1));
    json_object_del(bad.get(), "showClock");
    rejected = false; try { parseSettings(bad.get()); } catch (...) { rejected = true; } assert(rejected);
    assert(!std::filesystem::exists(path + ".tmp")); std::remove(path.c_str());
    for (int cols = 4; cols <= 6; ++cols) {
        assert(albumCardWidth(900, cols) > 100);
        assert(albumVerticalNeighbor(0, 50, 1, cols) == cols);
        assert(albumVerticalNeighbor(0, 50, -1, cols) == -1);
    }
    std::vector<Entry> songs(826);
    for (size_t i = 0; i < songs.size(); ++i) songs[i].id = std::to_string(i);
    std::mt19937 generator(42); const auto shuffled = shuffledFavorites(songs, generator);
    assert(shuffled.size() == songs.size() && shuffled.front().id != songs.front().id);
    auto ordered = shuffled; std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return std::stoi(a.id) < std::stoi(b.id); });
    for (size_t i = 0; i < songs.size(); ++i) assert(ordered[i].id == songs[i].id);
    auto body = jsonOwner(json_loads(R"({"searchResult3":{"artist":[{"id":"artist1","name":"Artist"}],"album":[{"id":"album1","name":"Album","created":"2026-11-15"}]}})", 0, nullptr));
    const auto artists = Api::entries(body.get(), "searchResult3", "artist");
    assert(artists.size() == 1 && artists[0].kind == EntryKind::Artist && artists[0].title == "Artist");
    const auto parsedAlbums = Api::entries(body.get(), "searchResult3", "album");
    assert(parsedAlbums[0].kind == EntryKind::Album && parsedAlbums[0].created == "2026-11-15");
    std::vector<Entry> albums(2); albums[0].title="Z"; albums[0].artist="A"; albums[0].created="2025";
    albums[1].title="A"; albums[1].artist="Z"; albums[1].created="2026";
    sortAlbums(albums, 0); assert(albums[0].title == "A");
    sortAlbums(albums, 1); assert(albums[0].artist == "A");
    sortAlbums(albums, 2); assert(albums[0].created == "2026");
}
