#define NAVIDROME_UI_TEST
#include "../src/main.cpp"
#include <aurore/test/harness.hpp>
#include <filesystem>
#include <iostream>
#include <unistd.h>

AURORE_TEST(all_app_screens) {
    const auto work = std::filesystem::path("/tmp/navidrome-aurore-ui-" + std::to_string(getpid()));
    std::filesystem::remove_all(work); std::filesystem::create_directories(work / "sdmc:/switch/navidrome_nx");
    std::filesystem::current_path(work);
    preferences = {}; preferences.background = 4; preferences.showClock = false; preferences.bubbleSpeed = 0;
    configureMusicUi();
    aurore::test::Options options; options.locale = std::getenv("NAVIDROME_UI_LANG") ? std::getenv("NAVIDROME_UI_LANG") : "en-US";
    const auto pictures = std::filesystem::path("/app/build.ui/screenshots") / options.locale;
    std::filesystem::create_directories(pictures);
    aurore::test::Harness h(options); loadMusicUiAssets();
    AURORE_CHECK(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    auto metrics = new aurore::Box(aurore::Axis::COLUMN);
    auto fixedText = new aurore::Label(); fixedText->setDimensions(100, 40); fixedText->setFontSize(20); fixedText->setSingleLine(true); fixedText->setText("A long fixed-size label must stay within its width"); metrics->addView(fixedText);
    auto metricsScreen = new aurore::Activity(metrics); h.push(metricsScreen); h.step(40);
    AURORE_CHECK(fixedText->isTruncated());
    fixedText->setText("Short"); h.step(); AURORE_CHECK(!fixedText->isTruncated());
    auto app = new App(); h.push(app);
    auto pump = [&](int count = 8) { for (int i = 0; i < count; ++i) { h.step(); app->poll(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); } };
    auto wait = [&](std::function<bool()> ready) { for (int i = 0; i < 1000; ++i) { pump(1); if (ready()) { pump(); return true; } } return false; };
    auto image = [&](const char* name) { AURORE_CHECK(h.screenshot().savePng((pictures / (std::string(name) + ".png")).string())); };
    auto tap = [&](const std::string& id) { auto view = h.find(id); AURORE_CHECK(view); if (view) { aurore::Application::giveFocus(view); pump(4); AURORE_CHECK(h.tapView(view)); pump(); } };
    auto idle = [&] { return wait([&] { return !app->isBusy(); }); };
    auto hasItem = [&](const std::string& id) { return wait([&] { return h.find("library/item/" + id) != nullptr && !app->isBusy(); }); };
    auto setting = [&](const char* key) {
        auto label = h.findText(aurore::getStr(key)); AURORE_CHECK(label);
        if (label) { auto button = label->getParent()->getChildren().back(); aurore::Application::giveFocus(button); pump(5); AURORE_CHECK(h.tapView(button)); pump(); }
    };

    AURORE_CHECK(wait([&] { return dynamic_cast<LoginScreen*>(h.top()) != nullptr; }));
    AURORE_CHECK(static_cast<MusicButton*>(h.find("login/save"))->getState() == aurore::ButtonState::DISABLED);
    pump(40); image("login");
    const char* fields[] = {std::getenv("NAVIDROME_UI_SERVER"), "test-user", "test-secret"};
    for (int i = 0; i < 3; ++i) { h.setKeyboardAnswer(fields[i]); tap("login/field/" + std::to_string(i)); }
    image("login-filled"); tap("login/test");
    AURORE_CHECK(wait([&] { auto save = dynamic_cast<MusicButton*>(h.find("login/save")); return save && save->getState() == aurore::ButtonState::ENABLED; }));
    if (static_cast<MusicButton*>(h.find("login/save"))->getState() != aurore::ButtonState::ENABLED) { image("login-failed"); return; }
    tap("login/save"); AURORE_CHECK(wait([&] { return h.top() == app && !app->isBusy(); }));
    AURORE_CHECK(h.hasText(aurore::getStr("app/most_played"))); image("home");

    for (const auto& section : {std::string("artists"), std::string("albums"), std::string("songs"), std::string("playlists"), std::string("nav_favorite_songs")}) {
        tap("app/" + section); AURORE_CHECK(idle());
        image(("library-" + section).c_str());
        for (int i = 0; i < 3; ++i) { tap("library/view"); AURORE_CHECK(idle()); image((section + "-view-" + std::to_string(i)).c_str()); }
    }
    tap("app/albums"); AURORE_CHECK(hasItem("album-0"));
    tap("library/next_page"); AURORE_CHECK(hasItem("album-50"));
    tap("library/previous_page"); AURORE_CHECK(hasItem("album-0"));
    tap("app/artists"); AURORE_CHECK(hasItem("artist-0"));
    h.setKeyboardAnswer("Artist 1"); tap("library/search"); AURORE_CHECK(hasItem("artist-1")); AURORE_CHECK(!h.find("library/item/artist-0"));
    tap("library/clear_search"); AURORE_CHECK(hasItem("artist-0"));
    tap("library/order"); AURORE_CHECK(idle()); AURORE_CHECK(aurore::Application::getCurrentFocus() == h.find("library/order"));
    tap("library/item/artist-0"); AURORE_CHECK(hasItem("album-0")); image("artist-albums");
    tap("library/item/album-0"); AURORE_CHECK(hasItem("song-0")); image("album-tracks");
    tap("library/item/song-0"); AURORE_CHECK(idle());
    AURORE_CHECK(Mix_PlayingMusic());
    tap("player/pause"); AURORE_CHECK(Mix_PausedMusic());
    tap("player/lyrics"); AURORE_CHECK(wait([&] { return dynamic_cast<LyricsScreen*>(h.top()) != nullptr && !app->isBusy(); }));
    pump(30); image("lyrics");
    auto pane = dynamic_cast<LyricsPane*>(h.find("lyrics/pane")); AURORE_CHECK(pane && !pane->manual);
    if (pane) {
        const auto selected = pane->selected;
        h.swipe(pane->getX()+80, pane->getY()+120, pane->getX()+80, pane->getY()+30, 300); pump();
        AURORE_CHECK(!pane->manual && pane->selected == selected);
        tap("lyrics/manual"); AURORE_CHECK(pane->manual);
        h.swipe(pane->getX()+80, pane->getY()+220, pane->getX()+80, pane->getY()+80, 300); pump();
        AURORE_CHECK(pane->selected > selected);
        const auto swiped = pane->selected; h.tap(aurore::BUTTON_DOWN); pump(); AURORE_CHECK(pane->selected > swiped);
        h.tap(aurore::BUTTON_B); pump(); AURORE_CHECK(!pane->manual && pane->follow);
    }
    h.tap(aurore::BUTTON_B); pump(30); AURORE_CHECK(h.top() == app);
    tap("app/playlists"); AURORE_CHECK(hasItem("playlist-0")); tap("library/item/playlist-0"); AURORE_CHECK(hasItem("song-0")); image("playlist-tracks");
    tap("library/view"); tap("library/view"); image("playlist-tracks-coverflow");
    auto flow = h.find("library/coverflow"); AURORE_CHECK(flow);
    if (flow) {
        aurore::Application::giveFocus(flow); h.tap(aurore::BUTTON_RIGHT); pump();
        auto caption = static_cast<aurore::Label*>(h.find("coverflow/title")); AURORE_CHECK(caption->getFullText() == "Track 01");
        h.swipe(flow->getX()+500, flow->getY()+70, flow->getX()+330, flow->getY()+70, 350); pump();
        AURORE_CHECK(caption->getFullText() != "Track 01");
    }
    AURORE_CHECK(h.find("app/home")->getCornerRadius() == 8);
    tap("sidebar/collapse"); AURORE_CHECK(preferences.sidebarCollapsed);
    AURORE_CHECK(h.find("app/home")->getCornerRadius() == 8);
    tap("sidebar/collapse"); AURORE_CHECK(!preferences.sidebarCollapsed);
    AURORE_CHECK(h.find("app/home")->getCornerRadius() == 8);

    tap("app/settings"); AURORE_CHECK(dynamic_cast<SettingsScreen*>(h.top())); image("settings-appearance");
    setting("app/background"); AURORE_CHECK(preferences.background == 5); image("settings-rainbow");
    setting("app/background"); AURORE_CHECK(preferences.background == 6); image("settings-warp");
    setting("app/background"); AURORE_CHECK(preferences.background == 0); image("settings-bubbles");
    setting("app/background_style"); AURORE_CHECK(preferences.backgroundStyle == 1); image("settings-xmb");
    h.tap(aurore::BUTTON_RB); pump(); AURORE_CHECK(h.hasText(aurore::getStr("app/visualizer"))); image("settings-layout");
    setting("app/visualizer"); AURORE_CHECK(preferences.visualizer == 1);
    setting("app/visualizer"); AURORE_CHECK(preferences.visualizer == 2);
    h.tap(aurore::BUTTON_RB); pump(); image("settings-remote");
    setting("app/web_remote"); pump(25); AURORE_CHECK(preferences.remoteEnabled); image("settings-remote-paired");
    h.tap(aurore::BUTTON_RB); pump(); AURORE_CHECK(h.hasText("test-user"));
    AURORE_CHECK(h.hasText(aurore::getStr("app/update_check"))); image("settings-account");
    h.tap(aurore::BUTTON_RB); pump(); AURORE_CHECK(h.find("settings/tab/4") == aurore::Application::getCurrentFocus());
    AURORE_CHECK(h.hasText(aurore::getStr("app/equalizer"))); image("settings-audio");
    setting("app/eq_bass"); AURORE_CHECK(loadSettings().equalizer[0] == 1);
    setting("app/eq_treble"); AURORE_CHECK(loadSettings().equalizer[2] == 1);
    tap("settings/eq_reset"); AURORE_CHECK(loadSettings().equalizer == EqualizerGains{});
    auto config = loadLoginConfig();
    Api cacheAccount(config.get());
    SongCache downloads;
    for (const char* title : {"Download One", "Download Two", "Download Three"}) {
        Entry entry; entry.id = title; entry.title = title;
        downloads.save(cacheAccount, entry.id, {'m', 'p', '3'});
        downloads.describe(cacheAccount, entry);
    }
    h.tap(aurore::BUTTON_RB); pump(); image("settings-downloads");
    const auto stored = downloads.downloads();
    AURORE_CHECK(stored.size() == 3);
    if (stored.size() == 3) {
        tap("download/" + stored[0].filename);
        tap("download/" + stored[1].filename);
        tap("settings/delete-downloads");
        pump(40); // Let the dialog finish opening before sending confirmation input.
        auto confirm = h.findText(aurore::getStr("app/download_confirm_action"));
        AURORE_CHECK(confirm);
        if (confirm) {
            aurore::Application::giveFocus(confirm->getParent());
            AURORE_CHECK(h.tapView(confirm->getParent())); pump(40);
        }
        AURORE_CHECK(downloads.downloads().size() == 1);
        AURORE_CHECK(downloads.load(cacheAccount, stored[2].title).size() == 3);
    }
    h.tap(aurore::BUTTON_RB); pump(); AURORE_CHECK(h.find("settings/tab/0") == aurore::Application::getCurrentFocus());
    tap("settings/back"); pump(30); AURORE_CHECK(h.top() == app);
    AURORE_CHECK(loadSettings().visualizer == 2);
    image("final-player");
    app->shutdown();
}
int main(int argc, char** argv) { return aurore::test::runTests(argc, argv); }
