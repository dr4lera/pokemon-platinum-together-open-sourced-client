// SPDX-License-Identifier: GPL-3.0-or-later
#include "port/community_link_qa_guard.h"
#include <cstdio>
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace CommunityLinkQAGuard {
namespace {
bool Plain(const std::filesystem::path& path)
{
    for (auto current = path; !current.empty();) {
        const auto status = std::filesystem::symlink_status(current);
        if (std::filesystem::is_symlink(status)) return false;
#ifdef _WIN32
        const auto attributes = GetFileAttributesW(current.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
#endif
        const auto parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    return true;
}
}
std::string ReadSmall(const std::filesystem::path& file, std::size_t maxBytes)
{
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(file, ec);
    if (ec || !std::filesystem::is_regular_file(status) || !Plain(file)) return {};
    const auto size = std::filesystem::file_size(file, ec);
    if (ec || size > maxBytes) return {};
    std::ifstream input(file, std::ios::binary);
    std::string result(static_cast<std::size_t>(size), '\0');
    if (!input.read(result.data(), static_cast<std::streamsize>(size))) return {};
    if (input.peek() != std::char_traits<char>::eof()) return {};
    return result;
}
bool WriteNew(const std::filesystem::path& file, const std::string& content)
{
    if (!Plain(file.parent_path())) return false;
    auto staged = file;
    staged += ".pending";
#ifdef _WIN32
    FILE* output = _wfopen(staged.c_str(), L"wbx");
#else
    FILE* output = std::fopen(staged.c_str(), "wbx");
#endif
    if (!output) return false;
    const bool written = std::fwrite(content.data(), 1, content.size(), output) == content.size();
    const bool closed = std::fclose(output) == 0;
    std::error_code ec;
    // Publish a complete record atomically without replacing any existing
    // destination. Both names are private files in the same fresh directory.
    if (written && closed) {
#ifdef _WIN32
        if (!MoveFileExW(staged.c_str(), file.c_str(), 0)) ec = std::error_code(GetLastError(), std::system_category());
#else
        std::filesystem::create_hard_link(staged, file, ec);
#endif
    } else ec = std::make_error_code(std::errc::io_error);
    const bool published = !ec;
    std::filesystem::remove(staged, ec);
    return published;
}
bool Validate(const std::string& requestedDirectory, const std::string& role, Directory& out, std::string& error)
{
    try {
        if (role != "host" && role != "guest") throw std::runtime_error("role must be host or guest");
        auto path = std::filesystem::u8path(requestedDirectory);
        if (!path.is_absolute()) throw std::runtime_error("QA directory must be absolute");
        path = path.lexically_normal();
        const auto cwd = std::filesystem::current_path().lexically_normal();
        if (path != cwd || !Plain(path)) throw std::runtime_error("QA directory must equal the plain working directory");
        const std::string prefix = "community-link-qa-", suffix = "-" + role;
        const auto leaf = path.filename().u8string();
        if (leaf.size() != prefix.size() + 32 + suffix.size() || leaf.compare(0, prefix.size(), prefix)
            || leaf.compare(leaf.size() - suffix.size(), suffix.size(), suffix)) throw std::runtime_error("invalid disposable directory name");
        const auto nonce = leaf.substr(prefix.size(), 32);
        if (nonce.find_first_not_of("0123456789abcdef") != std::string::npos) throw std::runtime_error("invalid QA nonce");
        const auto marker = "community-link-qa-v1\nnonce=" + nonce + "\nrole=" + role + "\n";
        if (ReadSmall(path / ".community-link-qa", 128) != marker) throw std::runtime_error("QA marker mismatch");
        for (const auto& name : {"save.bin", "CommunityOnline"}) {
            std::error_code ec;
            const auto status = std::filesystem::symlink_status(path / name, ec);
            if (std::filesystem::exists(status) || (ec && ec != std::errc::no_such_file_or_directory)) throw std::runtime_error("QA save/account storage is not fresh");
        }
        const auto stem = "community-link-qa-" + nonce;
        out = {path, path.parent_path() / (stem + ".party"), path.parent_path() / (stem + ".guest-prepared"), nonce, role};
        if (role == "host" && (std::filesystem::exists(out.partyFile) || std::filesystem::exists(out.guestPreparedFile))) throw std::runtime_error("QA coordination nonce was already used");
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        out = {};
        error = exception.what();
        return false;
    }
}
}
