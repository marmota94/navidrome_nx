#include "release_update.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

int main() {
    assert(releaseVersion("v1.10.0") > releaseVersion("1.9.9"));
    assert(!releaseVersion("1.1.0-beta") && !releaseVersion("v1.1.") && !releaseVersion("v01.2.3"));
    auto release = jsonOwner(json_loads(R"({
        "tag_name": "v1.2.0",
        "assets": [
            {"name": "NavidromeNX.ovl", "state": "uploaded"},
            {"name": "navidrome_nx.nro", "state": "uploaded", "size": 184,
             "digest": "sha256:41574d81782b74d3720c22a079587e02ebc83cac6e3a63e075085c9b2d712c86"}
        ]
    })", 0, nullptr));
    auto update = findReleaseUpdate(release.get(), "1.1.0");
    assert(update && update->size == 184);
    assert(!findReleaseUpdate(release.get(), "1.2.0"));
    json_object_set_new(release.get(), "prerelease", json_true());
    assert(!findReleaseUpdate(release.get(), "1.1.0"));
    json_object_set_new(release.get(), "prerelease", json_false());
    auto releases = jsonOwner(json_array());
    auto overlayOnly = jsonOwner(json_loads(R"({"tag_name":"v1.3.0","assets":[{"name":"NavidromeNX.ovl","state":"uploaded"}]})", 0, nullptr));
    json_array_append(releases.get(), overlayOnly.get());
    json_array_append(releases.get(), release.get());
    assert(selectReleaseUpdate(releases.get(), "1.1.0")->tag == "v1.2.0");
    json_array_remove(json_object_get(release.get(), "assets"), 1);
    assert(!findReleaseUpdate(release.get(), "1.1.0"));

    std::vector<char> nro(184);
    std::memcpy(nro.data() + 16, "NRO0", 4);
    nro[24] = static_cast<char>(128);
    std::memcpy(nro.data() + 128, "ASET", 4);
    verifyReleaseUpdate(nro, *update);
    nro.back() = 1;
    bool rejected = false;
    try { verifyReleaseUpdate(nro, *update); }
    catch (const ApiError& error) { rejected = error.key == "update_invalid"; }
    assert(rejected);

    const auto dir = std::filesystem::temp_directory_path() / ("navidrome-update-" + std::to_string(getpid()));
    std::filesystem::create_directory(dir);
    const auto path = (dir / "navidrome_nx.nro").string();
    writeAtomicFile(path, "old");
    writeAtomicFile(path + ".update", "new image");
    assert(std::filesystem::file_size(path) == 3);
    replaceFileWithBackup(path, path + ".update", true);
    assert(std::filesystem::file_size(path) == 9 && std::filesystem::file_size(path + ".bak") == 3);
    bool installFailed = false;
    try { replaceFileWithBackup(path, path + ".missing", true); }
    catch (const std::runtime_error&) { installFailed = true; }
    assert(installFailed && std::filesystem::file_size(path) == 9);
    std::filesystem::remove_all(dir);
    std::cout << "Release selection, update integrity and rollback backup checks passed\n";
}
