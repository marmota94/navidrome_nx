#pragma once
#include "api.hpp"
#include "atomic_file.hpp"
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>

class SongCache {
    std::filesystem::path root;
    static constexpr size_t ByteLimit = 64 * 1024 * 1024;
    static bool songFile(const std::string& name) {
        return name.size() == 36 && name.substr(32) == ".mp3" && name.find_first_not_of("0123456789abcdef") == 32;
    }
    std::filesystem::path path(const Api& api, const std::string& id) const {
        const auto input = std::to_string(api.serverUrl().size()) + ":" + api.serverUrl() + std::to_string(api.userName().size()) + ":" + api.userName() + id;
        unsigned char hash[16];
        if (mbedtls_md5_ret(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash)) throw std::runtime_error("cache_key");
        std::string name;
        for (auto value : hash) { name += "0123456789abcdef"[value >> 4]; name += "0123456789abcdef"[value & 15]; }
        return root / (name + ".mp3");
    }
    void removeSong(const std::filesystem::path& file) const {
        std::filesystem::remove(file);
        std::filesystem::remove(file.string() + ".json");
    }
    void trim(size_t keep) const {
        std::vector<std::filesystem::directory_entry> files;
        for (const auto& file : std::filesystem::directory_iterator(root)) {
            const auto name = file.path().filename().string();
            if (std::filesystem::is_regular_file(file.symlink_status()) && songFile(name)) files.push_back(file);
            else if (file.is_regular_file() && name.size() == 40 && name.substr(36) == ".tmp" && songFile(name.substr(0, 36))) std::filesystem::remove(file.path());
        }
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.last_write_time() < b.last_write_time(); });
        for (size_t i = 0; i + keep < files.size(); ++i) removeSong(files[i].path());
    }
public:
    static constexpr size_t MaxSongs = 15;
    explicit SongCache(std::filesystem::path root = "sdmc:/switch/navidrome_nx/song-cache") : root(std::move(root)) {}
    struct Download { std::string filename, title; uintmax_t bytes; };
    void erase(const Api& api, const std::string& id) const { removeSong(path(api, id)); }
    void eraseDownload(const std::string& filename) const {
        if (!songFile(filename)) throw std::runtime_error("cache_key");
        const auto file = root / filename;
        const auto status = std::filesystem::symlink_status(file);
        if (!std::filesystem::is_regular_file(status)) throw std::runtime_error("cache_key");
        removeSong(file);
    }
    std::vector<Download> downloads() const {
        std::vector<Download> result;
        if (!std::filesystem::exists(root)) return result;
        for (const auto& file : std::filesystem::directory_iterator(root)) {
            const auto name = file.path().filename().string();
            if (!songFile(name) || !std::filesystem::is_regular_file(file.symlink_status())) continue;
            std::string title = name;
            const auto detail = file.path().string() + ".json";
            if (std::filesystem::is_regular_file(std::filesystem::symlink_status(detail)) && std::filesystem::file_size(detail) <= 8192) {
                const auto metadata = jsonOwner(json_load_file(detail.c_str(), JSON_REJECT_DUPLICATES, nullptr));
                if (const auto* text = json_string_value(json_object_get(metadata.get(), "title")); text && *text) title = text;
                if (const auto* artist = json_string_value(json_object_get(metadata.get(), "artist")); artist && *artist)
                    title += " · " + std::string(artist);
            }
            result.push_back({name, std::move(title), file.file_size()});
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.title < b.title; });
        return result;
    }
    void describe(const Api& api, const Entry& track) const {
        const auto detail = path(api, track.id).string() + ".json";
        if (std::filesystem::exists(detail)) return;
        const auto metadata = jsonOwner(json_pack("{s:s,s:s}", "title", track.title.c_str(), "artist", track.artist.c_str()));
        const auto json = std::unique_ptr<char, decltype(&std::free)>(metadata ? json_dumps(metadata.get(), JSON_COMPACT) : nullptr, std::free);
        if (!json) throw std::runtime_error("cache_write");
        writeAtomicFile(detail, json.get());
    }
    std::vector<char> load(const Api& api, const std::string& id) const {
        const auto file = path(api, id);
        if (!std::filesystem::exists(file)) return {};
        const auto size = std::filesystem::file_size(file);
        if (!size || size > ByteLimit) { removeSong(file); return {}; }
        std::ifstream input(file, std::ios::binary);
        std::vector<char> bytes(size);
        if (!input.read(bytes.data(), size)) { removeSong(file); return {}; }
        ::utime(file.c_str(), nullptr);
        return bytes;
    }
    void save(const Api& api, const std::string& id, const std::vector<char>& bytes) const {
        if (bytes.empty() || bytes.size() > ByteLimit) throw std::runtime_error("cache_size");
        std::filesystem::create_directories(root);
        const auto file = path(api, id);
        if (std::filesystem::exists(file)) { ::utime(file.c_str(), nullptr); trim(MaxSongs); return; }
        trim(MaxSongs - 1);
        const auto temp = file.string() + ".tmp";
        const int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd < 0) throw std::runtime_error("cache_write");
        bool ok = true;
        for (size_t offset = 0; offset < bytes.size();) {
            const auto count = ::write(fd, bytes.data() + offset, bytes.size() - offset);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) { ok = false; break; }
            offset += count;
        }
        if (::fsync(fd)) ok = false;
        if (::close(fd)) ok = false;
        if (!ok || ::rename(temp.c_str(), file.c_str())) {
            ::unlink(temp.c_str()); throw std::runtime_error("cache_write");
        }
    }
};
