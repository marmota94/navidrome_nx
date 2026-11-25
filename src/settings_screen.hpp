#pragma once
#include <filesystem>
#include "music_surface.hpp"
#include "clock_text.hpp"
#include "pairing_qr.hpp"
#include "release_update.hpp"

class SettingsScreen : public aurore::Activity {
    AppUpdater& updater;
    MusicButton* updateButton = nullptr;
    aurore::Label* updateStatus = nullptr;
    aurore::Label *message = nullptr, *remoteAddress = nullptr, *header = nullptr;
    std::vector<std::pair<MusicButton*, std::function<std::string()>>> values;
    std::function<void()> changed;
    std::function<void()> logout;
    std::string accountServer, accountUser;
    std::function<std::string()> remoteInfo;
    std::string lastClock, lastRemote;
    std::function<std::string()> remotePairing;
    aurore::Box* qrPanel = nullptr;
    PairingQr* qr = nullptr;
    aurore::ScrollingFrame* settingsScroll = nullptr;
    std::array<aurore::Box*, 5> pages{};
    std::array<MusicButton*, 5> tabs{};
    int activeTab = 0;
    bool lastQrVisible = false;
    aurore::Box *bubbleRow = nullptr, *opacityRow = nullptr, *styleRow = nullptr, *speedRow = nullptr;
    aurore::Label* appearanceHint = nullptr;
    void updateAppearance() {
        const bool warp = preferences.background == 6;
        const bool colored = preferences.background != 4 && !warp;
        for (auto row : {opacityRow, styleRow}) row->setVisibility(colored ? aurore::Visibility::VISIBLE : aurore::Visibility::GONE);
        speedRow->setVisibility(colored || warp ? aurore::Visibility::VISIBLE : aurore::Visibility::GONE);
        bubbleRow->setVisibility(colored && preferences.backgroundStyle == 0 ? aurore::Visibility::VISIBLE : aurore::Visibility::GONE);
        appearanceHint->setText(aurore::getStr(warp ? "app/settings_warp_hint" : !colored ? "app/settings_amoled_hint" : preferences.background == 5 ? "app/settings_rainbow_hint" : "app/settings_appearance_hint"));
    }
    void selectTab(int index) {
        activeTab = index;
        for (size_t i = 0; i < pages.size(); ++i) {
            pages[i]->setVisibility(static_cast<int>(i) == index ? aurore::Visibility::VISIBLE : aurore::Visibility::GONE);
            tabs[i]->setStyle(static_cast<int>(i) == index ? &aurore::BUTTONSTYLE_PRIMARY : &aurore::BUTTONSTYLE_DEFAULT);
            assert(static_cast<int>(i) == index || pages[i]->getDefaultFocus() == nullptr);
        }
        if (auto first = pages[index]->getDefaultFocus()) first->setCustomNavigationRoute(aurore::FocusDirection::UP, tabs[index]);
        settingsScroll->willAppear(true);
        if (message) { message->setText(aurore::getStr("app/settings_adjust_hint")); message->setTextColor(nvgRGB(188, 192, 215)); }
    }
    aurore::Label* section(aurore::Box* parent, const char* title, const char* description) {
        auto label = new aurore::Label(); label->setText(aurore::getStr(title)); label->setFontSize(24); label->setHeight(36); label->setShrink(0); parent->addView(label);
        auto detail = new aurore::Label(); detail->setText(aurore::getStr(description)); detail->setFontSize(16); detail->setHeight(40); detail->setShrink(0); detail->setTextColor(nvgRGB(188, 192, 215)); parent->addView(detail); return detail;
    }
    void applyOption(const std::function<void(int)>& change, int direction) {
        const auto previous = preferences;
        change(direction);
        try {
            std::filesystem::create_directories("sdmc:/switch/navidrome_nx");
            saveSettings(preferences);
            changed();
            message->setText(aurore::getStr("app/settings_saved")); message->setTextColor(nvgRGB(147, 224, 196));
        } catch (...) {
            preferences = previous;
            message->setText(aurore::getStr("app/settings_save_error")); message->setTextColor(nvgRGB(255, 153, 164));
        }
        for (auto& item : values) item.first->setText("‹ " + item.second() + " ›");
        updateAppearance();
    }
    aurore::Box* option(aurore::Box* parent, const std::string& name, std::function<std::string()> value,
        std::function<void(int)> change) {
        auto row = musicCard(aurore::Axis::ROW);
        row->setMarginBottom(8); row->setPadding(6, 10, 6, 10); row->setCornerRadius(12); row->setBackgroundColor(nvgRGBA(91, 85, 140, 45));
        auto label = new aurore::Label(); label->setText(name); label->setFontSize(20);
        label->setWidth(0); label->setGrow(1); label->setHeight(42);
        row->addView(label);
        auto button = new MusicButton(); button->setText("‹ " + value() + " ›"); button->setWidth(300); button->setHeight(42); button->setShrink(0); button->applyXMLAttribute("fontSize", "18");
        auto adjust = [this, change](int direction) { applyOption(change, direction); };
        button->registerClickAction([adjust](aurore::View*) { adjust(1); return true; });
        button->registerAction("", aurore::BUTTON_NAV_LEFT, [adjust](aurore::View*) { adjust(-1); return true; }, true, true);
        button->registerAction("", aurore::BUTTON_NAV_RIGHT, [adjust](aurore::View*) { adjust(1); return true; }, true, true);
        values.push_back({button, value}); row->addView(button); parent->addView(row); return row;
    }
    std::string toggle(bool value) { return aurore::getStr(value ? "app/on" : "app/off"); }
public:
    std::function<void()> closed;
    SettingsScreen(std::function<void()> changed, std::function<std::string()> remoteInfo, std::function<std::string()> remotePairing,
        std::function<void()> closed, std::function<void()> logout, std::string accountServer, std::string accountUser, AppUpdater& updater) : updater(updater), changed(std::move(changed)), logout(std::move(logout)), accountServer(std::move(accountServer)), accountUser(std::move(accountUser)), remoteInfo(std::move(remoteInfo)), remotePairing(std::move(remotePairing)), closed(std::move(closed)) {}
    ~SettingsScreen() override { if (closed) closed(); }
    aurore::View* createContentView() override {
        auto root = new MusicSurface(); root->setPadding(24, 48, 20, 48);
        auto headerRow = new aurore::Box(aurore::Axis::ROW); headerRow->setHeight(56); headerRow->setShrink(0);
        header = new aurore::Label(); header->setText(aurore::getStr("app/settings")); header->setFontSize(30); header->setHeight(48); header->setWidth(0); header->setGrow(1); headerRow->addView(header);
        auto back = new MusicButton(); back->setText("\ue5c4"); back->setWidth(52); back->setHeight(44); back->setShrink(0); back->setPadding(6);
        back->applyXMLAttribute("fontSize", "26"); back->updateActionHint(aurore::BUTTON_A, aurore::getStr("app/back"));
        back->setId("settings/back"); back->registerClickAction([](aurore::View*) { aurore::Application::popActivity(); return true; }); headerRow->addView(back); root->addView(headerRow);
        const char* tabKeys[] = {"app/settings_appearance", "app/settings_layout", "app/settings_remote", "app/settings_account", "app/settings_audio"};
        auto tabRow = new aurore::Box(aurore::Axis::ROW); tabRow->setHeight(44); tabRow->setShrink(0); tabRow->setMarginBottom(16);
        for (size_t i = 0; i < tabs.size(); ++i) {
            auto button = new MusicButton(); button->setText(aurore::getStr(tabKeys[i])); button->setId("settings/tab/" + std::to_string(i)); button->setHeight(44); button->setWidth(0); button->setGrow(1); button->applyXMLAttribute("fontSize", "18");
            if (i + 1 < tabs.size()) button->setMarginRight(12);
            button->registerClickAction([this, i](aurore::View*) { selectTab(static_cast<int>(i)); return true; }); tabs[i] = button; tabRow->addView(button);
        }
        root->addView(tabRow);
        auto body = new aurore::Box(aurore::Axis::ROW); body->setHeight(0); body->setGrow(1); root->addView(body);
        settingsScroll = new aurore::ScrollingFrame(); settingsScroll->setWidth(0); settingsScroll->setGrow(1); body->addView(settingsScroll);
        auto options = new aurore::Box(aurore::Axis::COLUMN); options->setWidth(1184); options->setPadding(12);
        for (auto& page : pages) { page = new aurore::Box(aurore::Axis::COLUMN); options->addView(page); }
        appearanceHint = section(pages[0], "app/settings_appearance", "app/settings_appearance_hint");
        option(pages[0], aurore::getStr("app/background"), [] {
            const char* colors[] = {"app/color_purple", "app/color_blue", "app/color_rose", "app/color_emerald", "app/color_amoled", "app/color_rainbow", "app/color_amoled_warp"};
            return aurore::getStr(colors[preferences.background]);
        }, [](int direction) { preferences.background = adjustSetting(preferences.background, 0, 6, direction); });
        opacityRow = option(pages[0], aurore::getStr("app/background_opacity"), [] { return std::to_string(preferences.backgroundOpacity) + "%"; },
            [](int direction) { preferences.backgroundOpacity = std::clamp(preferences.backgroundOpacity + direction * 10, 0, 100); });
        styleRow = option(pages[0], aurore::getStr("app/background_style"), [] { return aurore::getStr(preferences.backgroundStyle == 0 ? "app/style_bubbles" : "app/style_xmb"); },
            [](int direction) { preferences.backgroundStyle = adjustSetting(preferences.backgroundStyle, 0, 1, direction); });
        bubbleRow = option(pages[0], aurore::getStr("app/bubble_count"), [] { return std::to_string(preferences.bubbleCount); },
            [](int direction) { preferences.bubbleCount = adjustSetting(preferences.bubbleCount, 0, MaxBubbleCount, direction); });
        speedRow = option(pages[0], aurore::getStr("app/animation_speed"), [] {
            const char* speeds[] = {"app/off", "app/slow", "app/normal", "app/fast"}; return aurore::getStr(speeds[preferences.bubbleSpeed]);
        }, [](int direction) { preferences.bubbleSpeed = adjustSetting(preferences.bubbleSpeed, 0, 3, direction); });
        section(pages[1], "app/settings_layout", "app/settings_layout_hint");
        option(pages[1], aurore::getStr("app/show_clock"), [this] { return toggle(preferences.showClock); }, [](int) { preferences.showClock = !preferences.showClock; });
        option(pages[1], aurore::getStr("app/items_per_row"), [] { return std::to_string(preferences.columns); }, [](int direction) { preferences.columns = adjustSetting(preferences.columns, 4, 6, direction); });
        option(pages[1], aurore::getStr("app/collapse_menu"), [this] { return toggle(preferences.sidebarCollapsed); }, [](int) { preferences.sidebarCollapsed = !preferences.sidebarCollapsed; });
        option(pages[1], aurore::getStr("app/cache_songs"), [this] { return toggle(preferences.cacheSongs); }, [](int) { preferences.cacheSongs = !preferences.cacheSongs; });
        option(pages[1], aurore::getStr("app/visualizer"), [] {
            const char* styles[] = {"app/visualizer_waveform", "app/visualizer_bars", "app/visualizer_vu"};
            return aurore::getStr(styles[preferences.visualizer]);
        }, [](int direction) { preferences.visualizer = adjustSetting(preferences.visualizer, 0, 2, direction); });
        section(pages[2], "app/settings_remote", "app/settings_remote_hint");
        option(pages[2], aurore::getStr("app/web_remote"), [this] { return toggle(preferences.remoteEnabled); }, [](int) { preferences.remoteEnabled = !preferences.remoteEnabled; });
        remoteAddress = new aurore::Label(); remoteAddress->setFontSize(18); remoteAddress->setHeight(180); remoteAddress->setShrink(0); pages[2]->addView(remoteAddress);
        section(pages[3], "app/settings_account", "app/settings_account_hint");
        const std::pair<const char*, std::string> accountFields[] = {{"app/login_server", accountServer}, {"app/login_username", accountUser}};
        for (const auto& field : accountFields) {
            auto name = new aurore::Label(); name->setText(aurore::getStr(field.first)); name->setFontSize(16); name->setHeight(26); name->setShrink(0); name->setTextColor(nvgRGB(188, 192, 215)); pages[3]->addView(name);
            auto value = new aurore::Label(); value->setText(field.second.empty() ? "—" : field.second); value->setFontSize(20); value->setSingleLine(false); value->setWidthPercentage(100); value->setShrink(0); value->setMarginBottom(16); pages[3]->addView(value);
        }
        auto logoutButton = new MusicButton(); logoutButton->setText(aurore::getStr("app/logout")); logoutButton->setWidth(300); logoutButton->setHeight(46); logoutButton->setMarginTop(12);
        logoutButton->registerClickAction([this](aurore::View*) { try { logout(); } catch (...) { message->setText(aurore::getStr("app/logout_error")); } return true; }); pages[3]->addView(logoutButton);
        updateButton = new MusicButton();
        updateButton->setId("settings/update"); updateButton->setWidth(300); updateButton->setHeight(46); updateButton->setMarginTop(16);
        updateButton->registerClickAction([this](aurore::View*) { updater.start(); return true; });
        pages[3]->addView(updateButton);
        updateStatus = new aurore::Label();
        updateStatus->setFontSize(16); updateStatus->setHeight(34); updateStatus->setShrink(0);
        pages[3]->addView(updateStatus);
        refreshUpdate();
        section(pages[4], "app/equalizer", "app/equalizer_hint");
        const char* bands[] = {"app/eq_bass", "app/eq_mid", "app/eq_treble"};
        for (size_t band = 0; band < preferences.equalizer.size(); ++band) {
            option(pages[4], aurore::getStr(bands[band]), [band] {
                const int gain = preferences.equalizer[band];
                return (gain > 0 ? "+" : "") + std::to_string(gain) + " dB";
            }, [band](int direction) {
                auto& gain = preferences.equalizer[band];
                gain = adjustSetting(gain, -EqualizerGainLimit, EqualizerGainLimit, direction);
            });
        }
        auto resetEq = new MusicButton();
        resetEq->setText(aurore::getStr("app/eq_reset")); resetEq->setId("settings/eq_reset");
        resetEq->setWidth(300); resetEq->setHeight(46);
        resetEq->registerClickAction([this](aurore::View*) {
            applyOption([](int) { preferences.equalizer = {}; }, 0);
            return true;
        });
        pages[4]->addView(resetEq);
        settingsScroll->setContentView(options);
        qrPanel = new aurore::Box(aurore::Axis::COLUMN); qrPanel->setWidth(280); qrPanel->setShrink(0); qrPanel->setMarginLeft(24); qrPanel->setVisibility(aurore::Visibility::GONE);
        auto scan = new aurore::Label(); scan->setText(aurore::getStr("app/scan_remote")); scan->setFontSize(20); scan->setHeight(70); qrPanel->addView(scan);
        qr = new PairingQr(); qrPanel->addView(qr); body->addView(qrPanel);
        message = new aurore::Label(); message->setFontSize(16); message->setHeight(36); message->setShrink(0); message->setText(aurore::getStr("app/settings_adjust_hint")); root->addView(message);
        updateAppearance();
        selectTab(0); return root;
    }
    void onContentAvailable() override {
        auto changeTab = [this](int direction) {
            const int index = adjustSetting(activeTab, 0, static_cast<int>(tabs.size()) - 1, direction);
            aurore::Application::giveFocus(tabs[index]);
            selectTab(index);
            assert(aurore::Application::getCurrentFocus() == tabs[index]);
        };
        registerAction(aurore::getStr("app/previous_tab"), aurore::BUTTON_LB, [changeTab](aurore::View*) { changeTab(-1); return true; });
        registerAction(aurore::getStr("app/next_tab"), aurore::BUTTON_RB, [changeTab](aurore::View*) { changeTab(1); return true; });
        registerAction(aurore::getStr("app/back"), aurore::BUTTON_B, [](aurore::View*) { aurore::Application::popActivity(); return true; });
    }
    void refreshUpdate() {
        updateButton->setText(aurore::getStr(updater.available ? "app/update_install" : "app/update_check"));
        const auto state = updater.busy() || updater.staged ? aurore::ButtonState::DISABLED : aurore::ButtonState::ENABLED;
        if (updateButton->getState() != state) updateButton->setState(state);
        auto text = aurore::getStr(std::string("app/") + updater.status);
        if (updater.available) text += "  " + updater.available->tag;
        updateStatus->setText(text);
    }
    void poll() {
        if (activeTab == 3) refreshUpdate();
        const auto clock = preferences.showClock ? clockText() : "";
        if (clock != lastClock) { lastClock = clock; header->setText((clock.empty() ? "" : clock + "   ") + aurore::getStr("app/settings")); }
        const auto pairing = remotePairing();
        qr->update(pairing);
        const bool visible = activeTab == 2 && !pairing.empty();
        if (visible != lastQrVisible) { lastQrVisible = visible; qrPanel->setVisibility(visible ? aurore::Visibility::VISIBLE : aurore::Visibility::GONE); }
        const auto address = remoteInfo();
        if (address != lastRemote) { lastRemote = address; remoteAddress->setText(address); }
    }
};
