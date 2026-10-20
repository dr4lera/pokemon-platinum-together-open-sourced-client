// SPDX-License-Identifier: GPL-3.0-or-later
#include "port/community_trainer_art_download.h"
#include <curl/curl.h>
#include <openssl/evp.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <regex>
#include <thread>

namespace CommunityTrainerArt {
namespace {
bool CanonicalOrigin(const std::string& origin)
{
    if(origin.empty()||origin.size()>4096||origin.find('\0')!=std::string::npos)return false;
    CURLU* url=curl_url();if(!url)return false;
    if(curl_url_set(url,CURLUPART_URL,origin.c_str(),0)!=CURLUE_OK){curl_url_cleanup(url);return false;}
    auto get=[&](CURLUPart part){char* p=nullptr;std::string s;if(curl_url_get(url,part,&p,0)==CURLUE_OK){s=p;curl_free(p);}return s;};
    const auto scheme=get(CURLUPART_SCHEME),host=get(CURLUPART_HOST),port=get(CURLUPART_PORT),path=get(CURLUPART_PATH);
    const bool valid=(scheme=="https"||(scheme=="http"&&(host=="127.0.0.1"||host=="[::1]")))
        &&path=="/"&&get(CURLUPART_USER).empty()&&get(CURLUPART_PASSWORD).empty()&&get(CURLUPART_QUERY).empty()&&get(CURLUPART_FRAGMENT).empty()
        &&origin==scheme+"://"+host+(port.empty()?"":":"+port);
    curl_url_cleanup(url);return valid;
}
bool Valid(const Request& r)
{
    return r.id&&r.connection&&r.epoch&&r.session.size()==36&&r.player.size()<=20&&r.art.creation.size()<=40&&r.art.version.size()<=40
        &&std::regex_match(r.session,std::regex("[a-f0-9]{8}(-[a-f0-9]{4}){3}-[a-f0-9]{12}"))
        &&std::regex_match(r.player,std::regex("[1-9][0-9]{0,19}"))&&r.art.format=="platinum-trainer-209-v1"
        &&std::regex_match(r.art.creation,std::regex("[A-Za-z0-9_-]{1,40}"))&&std::regex_match(r.art.version,std::regex("[A-Za-z0-9_-]{1,40}"))
        &&r.art.hash.size()==64&&r.art.hash.find_first_not_of("0123456789abcdef")==std::string::npos;
}
}
struct Downloads::Impl {
    struct Job {
        Request request;
        std::string origin;
        std::atomic<bool> cancelled{false};
        std::vector<std::uint8_t> bytes;
        bool overflow=false,allocation=false;
        DownloadResult result;
    };
    std::mutex mutex;
    std::condition_variable ready;
    std::map<std::uint64_t,std::shared_ptr<Job>> jobs;
    std::deque<std::shared_ptr<Job>> queued,completed;
    bool stopping=false;
    std::thread worker;
    ~Impl(){
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;ClearLocked();}
        ready.notify_all();if(worker.joinable())worker.join();
    }
    void ClearLocked(){for(auto& entry:jobs)entry.second->cancelled=true;jobs.clear();queued.clear();completed.clear();}
    static int Cancelled(void* pointer,curl_off_t,curl_off_t,curl_off_t,curl_off_t){return static_cast<Job*>(pointer)->cancelled?1:0;}
    static std::size_t Write(char* data,std::size_t size,std::size_t count,void* pointer){
        auto& job=*static_cast<Job*>(pointer);if(job.cancelled)return 0;
        if(size&&count>MaxBytes/size){job.overflow=true;return 0;}
        const auto length=size*count;if(length>MaxBytes-job.bytes.size()){job.overflow=true;return 0;}
        try{job.bytes.insert(job.bytes.end(),data,data+length);}catch(...){job.allocation=true;return 0;}return length;
    }
    static DownloadResult Fetch(Job& job){
        DownloadResult output;output.request=job.request;
        CURL* curl=curl_easy_init();if(!curl){output.error="art_transport_init_failed";return output;}
        struct Cleanup{CURL* handle;~Cleanup(){curl_easy_cleanup(handle);}} cleanup{curl};
        const auto url=job.origin+"/api/creators/v1/packs/"+job.request.art.hash+".pttrainer";
        curl_easy_setopt(curl,CURLOPT_URL,url.c_str());curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,8000L);curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,15000L);
        curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
        curl_easy_setopt(curl,CURLOPT_SSL_OPTIONS,static_cast<long>(CURLSSLOPT_NATIVE_CA));curl_easy_setopt(curl,CURLOPT_CAINFO,nullptr);curl_easy_setopt(curl,CURLOPT_CAPATH,nullptr);curl_easy_setopt(curl,CURLOPT_PROXY,"");
        curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,Cancelled);curl_easy_setopt(curl,CURLOPT_XFERINFODATA,&job);
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,Write);curl_easy_setopt(curl,CURLOPT_WRITEDATA,&job);curl_easy_setopt(curl,CURLOPT_MAXFILESIZE_LARGE,static_cast<curl_off_t>(MaxBytes));
        const auto code=curl_easy_perform(curl);long status=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
        if(job.cancelled)return output;
        if(job.overflow||code==CURLE_FILESIZE_EXCEEDED){output.error="art_body_too_large";return output;}
        if(job.allocation){output.error="art_allocation_failed";return output;}
        if(code!=CURLE_OK){output.error="art_transport_failed";return output;}
        if(status!=200){output.error="art_http_refused";return output;}
        std::array<unsigned char,32> digest{};unsigned length=0;
        if(EVP_Digest(job.bytes.data(),job.bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)!=1||length!=32){output.error="art_hash_failed";return output;}
        std::string hash;const char* hex="0123456789abcdef";for(auto b:digest){hash+=hex[b>>4];hash+=hex[b&15];}
        if(hash!=job.request.art.hash){output.error="art_hash_mismatch";return output;}
        auto decoded=Decode(job.bytes.data(),job.bytes.size());
        if(!decoded){output.error=std::string("art_pack_")+ErrorName(decoded.error);return output;}
        output.pack=std::move(decoded.pack);return output;
    }
    void Run(){
        for(;;){
            std::shared_ptr<Job> job;
            {std::unique_lock<std::mutex> lock(mutex);ready.wait(lock,[&]{return stopping||!queued.empty();});if(stopping)return;job=std::move(queued.front());queued.pop_front();}
            if(job->cancelled)continue;
            try{job->result=Fetch(*job);}catch(...){job->result.request=job->request;job->result.error="art_download_failed";}
            std::vector<std::uint8_t>().swap(job->bytes);
            std::lock_guard<std::mutex> lock(mutex);const auto found=jobs.find(job->request.id);
            if(!stopping&&!job->cancelled&&found!=jobs.end()&&found->second==job)completed.push_back(std::move(job));
        }
    }
};
Downloads::Downloads():impl(new Impl){}
Downloads::~Downloads()=default;
bool Downloads::Queue(Request request,const std::string& origin){
    if(!Valid(request)||!CanonicalOrigin(origin))return false;
    std::lock_guard<std::mutex> lock(impl->mutex);if(impl->stopping||impl->jobs.size()>=8||impl->jobs.count(request.id))return false;
    auto job=std::make_shared<Impl::Job>();job->request=std::move(request);job->origin=origin;impl->jobs.emplace(job->request.id,job);impl->queued.push_back(job);
    if(!impl->worker.joinable()){try{impl->worker=std::thread([this]{impl->Run();});}catch(...){impl->jobs.erase(job->request.id);impl->queued.pop_back();return false;}}
    impl->ready.notify_one();return true;
}
bool Downloads::Cancel(std::uint64_t id){
    std::lock_guard<std::mutex> lock(impl->mutex);const auto found=impl->jobs.find(id);if(found==impl->jobs.end())return false;
    found->second->cancelled=true;impl->jobs.erase(found);
    auto remove=[&](auto& queue){for(auto i=queue.begin();i!=queue.end();)if((*i)->request.id==id)i=queue.erase(i);else ++i;};remove(impl->queued);remove(impl->completed);return true;
}
void Downloads::Clear(){std::lock_guard<std::mutex> lock(impl->mutex);impl->ClearLocked();}
bool Downloads::Poll(DownloadResult& output){
    std::lock_guard<std::mutex> lock(impl->mutex);if(impl->completed.empty())return false;
    auto job=std::move(impl->completed.front());impl->completed.pop_front();impl->jobs.erase(job->request.id);output=std::move(job->result);return true;
}
}
