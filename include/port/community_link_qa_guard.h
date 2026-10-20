// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef COMMUNITY_LINK_QA_GUARD_H
#define COMMUNITY_LINK_QA_GUARD_H
#include <filesystem>
#include <string>
namespace CommunityLinkQAGuard {
struct Directory {
    std::filesystem::path path, partyFile, guestPreparedFile;
    std::string nonce, role;
};
bool Validate(const std::string& requestedDirectory, const std::string& role, Directory& out, std::string& error);
bool WriteNew(const std::filesystem::path& file, const std::string& content);
std::string ReadSmall(const std::filesystem::path& file, std::size_t maxBytes);
}
#endif
