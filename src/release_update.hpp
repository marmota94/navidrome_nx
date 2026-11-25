#pragma once
#include "api.hpp"
#include "atomic_file.hpp"
#include <mbedtls/sha256.h>
#include <array>
#include <charconv>
#include <cstring>
#include <optional>
#include <future>
#include <string_view>

inline constexpr char AppVersion[] = NAVIDROME_VERSION;
inline constexpr char AppInstallPath[] = "sdmc:/switch/navidrome_nx/navidrome_nx.nro";
inline constexpr char ReleaseRepo[] = "marmota94/navidrome_nx";
inline constexpr size_t UpdateLimit = 32 * 1024 * 1024;

inline std::optional<std::array<unsigned, 3>> releaseVersion(std::string_view tag) {
    if (!tag.empty() && tag.front() == 'v') tag.remove_prefix(1);
    std::array<unsigned, 3> version{};
    for (size_t i = 0; i < version.size(); ++i) {
        const auto separator = tag.find('.');
        if ((i + 1 == version.size()) != (separator == tag.npos)) return {};
        const auto part = tag.substr(0, separator);
        if (part.empty() || (part.size() > 1 && part.front() == '0')) return {};
        const auto end = part.data() + part.size();
        const auto parsed = std::from_chars(part.data(), end, version[i]);
        if (parsed.ec != std::errc{} || parsed.ptr != end) return {};
        if (separator != tag.npos) tag.remove_prefix(separator + 1);
    }
    return version;
}

struct ReleaseUpdate {
    std::string tag, digest;
    size_t size;
};

inline std::optional<ReleaseUpdate> findReleaseUpdate(json_t* release, std::string_view current) {
    if (json_is_true(json_object_get(release, "draft")) || json_is_true(json_object_get(release, "prerelease"))) return {};
    auto tag = field(release, "tag_name");
    const auto version = releaseVersion(tag), installed = releaseVersion(current);
    if (!version || !installed || *version <= *installed) return {};
    size_t index;
    json_t* asset;
    json_array_foreach(json_object_get(release, "assets"), index, asset) {
        if (field(asset, "name") != "navidrome_nx.nro" || field(asset, "state") != "uploaded") continue;
        auto digest = field(asset, "digest");
        const auto size = json_integer_value(json_object_get(asset, "size"));
        if (size < 0x80 || size > static_cast<json_int_t>(UpdateLimit) || digest.size() != 71 || digest.compare(0, 7, "sha256:") != 0 ||
            digest.find_first_not_of("0123456789abcdef", 7) != std::string::npos) throw ApiError("update_metadata");
        return ReleaseUpdate{std::move(tag), digest.substr(7), static_cast<size_t>(size)};
    }
    return {};
}

inline std::vector<char> updateDownload(const std::string& url, size_t limit, const std::atomic<bool>& cancelled) {
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) throw ApiError("http_client");
    Transfer output{{}, limit};
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif
#ifdef __SWITCH__
    curl_easy_setopt(curl.get(), CURLOPT_CAINFO, "romfs:/github-ca.pem");
#endif
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "NavidromeNX");
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, limit == UpdateLimit ? 120L : 15L);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION,
        +[](void* data, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
            return static_cast<const std::atomic<bool>*>(data)->load() ? 1 : 0;
        });
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &cancelled);
    curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, Transfer::write);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &output);
    const auto result = curl_easy_perform(curl.get());
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    if (result != CURLE_OK || status != 200) throw ApiError("update_download");
    return std::move(output.bytes);
}

inline std::optional<ReleaseUpdate> selectReleaseUpdate(json_t* releases, std::string_view current) {
    if (!json_is_array(releases)) throw ApiError("update_metadata");
    std::optional<ReleaseUpdate> newest;
    size_t index;
    json_t* release;
    json_array_foreach(releases, index, release) {
        auto candidate = findReleaseUpdate(release, newest ? std::string_view(newest->tag) : current);
        if (candidate) newest = std::move(candidate);
    }
    return newest;
}

inline std::optional<ReleaseUpdate> checkReleaseUpdate(const std::atomic<bool>& cancelled) {
    // shortcut: scan the 30 most recent releases; paginate if app releases become infrequent.
    const auto bytes = updateDownload(std::string("https://api.github.com/repos/") + ReleaseRepo + "/releases?per_page=30",
        1024 * 1024, cancelled);
    const auto releases = jsonOwner(json_loadb(bytes.data(), bytes.size(), JSON_REJECT_DUPLICATES, nullptr));
    return selectReleaseUpdate(releases.get(), AppVersion);
}

inline void verifyReleaseUpdate(const std::vector<char>& bytes, const ReleaseUpdate& update) {
    if (bytes.size() != update.size || bytes.size() < 0x80 || std::memcmp(bytes.data() + 0x10, "NRO0", 4) != 0)
        throw ApiError("update_invalid");
    const auto* raw = reinterpret_cast<const unsigned char*>(bytes.data());
    uint32_t nroSize = 0;
    for (unsigned i = 0; i < 4; ++i) nroSize |= uint32_t(raw[0x18 + i]) << (8 * i);
    if (nroSize < 0x80 || nroSize > bytes.size() - 0x38 || std::memcmp(raw + nroSize, "ASET", 4) != 0)
        throw ApiError("update_invalid");
    unsigned char hash[32];
    if (mbedtls_sha256_ret(raw, bytes.size(), hash, 0) != 0) throw ApiError("update_invalid");
    std::string digest;
    for (const auto byte : hash) {
        digest += "0123456789abcdef"[byte >> 4];
        digest += "0123456789abcdef"[byte & 15];
    }
    if (digest != update.digest) throw ApiError("update_invalid");
}

inline void stageReleaseUpdate(const ReleaseUpdate& update, const std::atomic<bool>& cancelled) {
    const auto url = std::string("https://github.com/") + ReleaseRepo + "/releases/download/" + update.tag + "/navidrome_nx.nro";
    const auto bytes = updateDownload(url, UpdateLimit, cancelled);
    verifyReleaseUpdate(bytes, update);
    if (!cancelled) writeAtomicFile(std::string(AppInstallPath) + ".update", {bytes.data(), bytes.size()});
}

class AppUpdater {
    std::atomic<bool> cancelled{false};
    std::future<std::optional<ReleaseUpdate>> job;
public:
    std::optional<ReleaseUpdate> available;
    const char* status = "update_idle";
    bool staged = false;
    bool launchedFromInstallPath = false;
    bool busy() const { return job.valid(); }

    void start() {
        if (busy() || staged) return;
        try {
            if (available) {
                if (!launchedFromInstallPath) { status = "update_path"; return; }
                status = "update_installing";
                job = std::async(std::launch::async, [this, update = *available]() -> std::optional<ReleaseUpdate> {
                    struct stat file{};
                    if (::stat(AppInstallPath, &file) != 0 || !S_ISREG(file.st_mode)) throw ApiError("update_path");
                    stageReleaseUpdate(update, cancelled);
                    return {};
                });
            } else {
                status = "update_checking";
                job = std::async(std::launch::async, [this] { return checkReleaseUpdate(cancelled); });
            }
        } catch (...) { status = "update_failed"; }
    }

    // True when a new release was found so the app can show a notification.
    bool poll() {
        if (!busy() || job.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return false;
        const bool installing = available.has_value();
        try {
            available = job.get();
            staged = installing;
            status = installing ? "update_staged" : available ? "update_available" : "update_current";
            return available.has_value();
        } catch (const ApiError& error) {
            status = error.key == "update_path" ? "update_path" : "update_failed";
        } catch (...) { status = "update_failed"; }
        return false;
    }

    ~AppUpdater() { cancelled = true; if (busy()) job.wait(); }
};
