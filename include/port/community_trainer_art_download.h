// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef COMMUNITY_TRAINER_ART_DOWNLOAD_H
#define COMMUNITY_TRAINER_ART_DOWNLOAD_H
#include "port/community_online.h"
#include "port/community_trainer_art.h"
#include <memory>
#include <string>

namespace CommunityTrainerArt {
struct Request {
    std::uint64_t id=0,connection=0,epoch=0;
    std::string session,player;
    CommunityOnline::ArtReference art;
};
struct DownloadResult {
    Request request;
    std::shared_ptr<const Pack> pack;
    std::string error;
};
// Internal content worker. Service authorizes requests against its authenticated
// roster and rechecks results before handing them to the game thread. This worker
// holds no Service pointer, account key, CURL handle from another thread or GPU state.
class Downloads {
public:
    Downloads();
    ~Downloads();
    Downloads(const Downloads&)=delete;
    Downloads& operator=(const Downloads&)=delete;
    bool Queue(Request request,const std::string& canonicalOrigin);
    bool Cancel(std::uint64_t id);
    void Clear(); // Signals/drops only; never joins or waits for HTTP.
    bool Poll(DownloadResult& result);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
#endif
