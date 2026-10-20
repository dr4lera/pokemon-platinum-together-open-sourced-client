// SPDX-License-Identifier: GPL-3.0-or-later
#include "port/community_link_qa_guard.h"
#include <cassert>
#include <cstdio>
#include <fstream>
static std::string Utf8(const std::filesystem::path& path)
{
    const auto encoded = path.u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}
int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    using namespace CommunityLinkQAGuard;
    if (argc == 3) {
        assert(std::string(argv[1]) == "--reject-reparse");
        assert(ReadSmall(fs::u8path(argv[2]) / ".community-link-qa", 128).empty());
        std::puts("Actual QA guard rejects a Windows directory junction ancestor.");
        return 0;
    }
    assert(argc == 2);
    const auto original = fs::current_path();
    const auto parent = fs::absolute(fs::u8path(argv[1]));
    const std::string nonce(32, 'a');
    const auto host = parent / ("community-link-qa-" + nonce + "-host");
    fs::create_directory(host);
    const auto marker = "community-link-qa-v1\nnonce=" + nonce + "\nrole=host\n";
    assert(WriteNew(host / ".community-link-qa", marker));
    assert(!WriteNew(host / ".community-link-qa", "overwrite"));
    assert(ReadSmall(host / ".community-link-qa", 128) == marker);
    assert(ReadSmall(host / ".community-link-qa", 4).empty());
    fs::current_path(host);
    Directory result;
    std::string error;
    assert(Validate(Utf8(host), "host", result, error));
    assert(result.nonce == nonce && result.role == "host" && result.path == host);
    assert(!Validate(".", "host", result, error));
    assert(!Validate(Utf8(parent), "host", result, error));
    assert(!Validate(Utf8(host), "guest", result, error));
    std::ofstream(host / "save.bin") << "existing-save-must-not-open";
    assert(!Validate(Utf8(host), "host", result, error));
    assert(ReadSmall(host / "save.bin", 64) == "existing-save-must-not-open");
    fs::remove(host / "save.bin");
    fs::create_directory(host / "CommunityOnline");
    assert(!Validate(Utf8(host), "host", result, error));
    fs::remove(host / "CommunityOnline");
    const auto party = parent / ("community-link-qa-" + nonce + ".party");
    assert(WriteNew(party, "old-nonce"));
    assert(!Validate(Utf8(host), "host", result, error));
    fs::remove(party);
    std::ofstream(host / ".community-link-qa", std::ios::trunc) << "wrong-marker";
    assert(!Validate(Utf8(host), "host", result, error));
    fs::current_path(original);
    std::error_code ec;
    const auto alias = parent / "alias";
    fs::create_directory_symlink(host, alias, ec);
    if (!ec) {
        assert(ReadSmall(alias / ".community-link-qa", 128).empty());
        std::puts("QA guard rejects a symlink ancestor.");
    } else std::puts("QA symlink creation unavailable; that platform test was skipped.");
    std::puts("Actual QA preflight guard: absolute working directory, role/nonce marker, fresh save/account storage, exclusive coordination writes, bounded reads, and nonce reuse refusal passed.");
}
