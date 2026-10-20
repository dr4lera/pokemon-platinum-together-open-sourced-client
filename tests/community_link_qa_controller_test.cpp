// SPDX-License-Identifier: GPL-3.0-or-later
// Actual QA controller and Service; the field engine is not part of this test.
#include "port/community_wireless.h"
#include "port/community_link_qa_guard.h"
#include "port/community_link_qa_internal.h"
#include <chrono>
#include <stdexcept>
#include <thread>
#include <cstdio>
namespace CommunityWireless {
CommunityOnline::Service& Online() { static CommunityOnline::Service service; return service; }
bool Install() { return true; }
bool Prepare() { return Online().PrepareLink(std::string(64, 'a'), std::string(64, 'b')); }
}
#include "../src/port/community_link_qa.cpp"
int main()
{
    try {
        CommunityLinkQA_Preflight();
        if (!CommunityLinkQA_AuthorizeFresh(TRUE)) throw std::runtime_error("QA not selected");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (!CommunityLinkQA_Update()) {
            if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("QA controller setup timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        const auto link = CommunityWireless::Online().ReadLink();
        if (link.roster.size() != 2 || link.seat != (CommunityLinkQA_IsHost() ? 0 : 1)) throw std::runtime_error("wrong actual link roster");
        CommunityLinkQA_DidEnter();
        const auto parent = sDirectory.path.parent_path();
        const auto stem = "community-link-qa-" + sDirectory.nonce;
        if (!CommunityLinkQAGuard::WriteNew(parent / (stem + "." + sDirectory.role + "-active"), "active\n")) throw std::runtime_error("could not record active");
        for (const auto* role : {"host", "guest"}) {
            while (CommunityLinkQAGuard::ReadSmall(parent / (stem + "." + role + "-active"), 32) != "active\n") {
                if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("other actual controller never reached Active");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        CommunityWireless::Online().Disconnect();
        std::puts("Actual QA controller reached a two-account signed Active link using private atomic party/prepare coordination.");
        return 0;
    } catch (const std::exception& exception) {
        std::fprintf(stderr, "QA controller test failed: %s\n", exception.what());
        return 1;
    }
}
