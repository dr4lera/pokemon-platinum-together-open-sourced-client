// SPDX-License-Identifier: GPL-3.0-or-later
#include "port/community_online.h"
#include <curl/curl.h>
#include <curl/websockets.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <set>
#include <stdexcept>
#include <thread>

namespace CommunityOnline {
namespace {
using Bytes = std::vector<unsigned char>;
using Doc = rapidjson::Document;
using Value = rapidjson::Value;
using Clock = std::chrono::steady_clock;
struct Error : std::runtime_error { bool retry; Error(const std::string& code, bool retry = false):std::runtime_error(code),retry(retry){} };
void Need(bool condition, const char* code) { if (!condition) throw Error(code); }
std::uint64_t Now() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
Bytes Data(const std::string& s) { return Bytes(s.begin(),s.end()); }
void Append(Bytes& target,const Bytes& value) { target.insert(target.end(),value.begin(),value.end()); }
Bytes Domain(const char* s) { Bytes b=Data(s); b.push_back(0); return b; }
Bytes Hash(const Bytes& b) { Bytes h(32); unsigned n=0; Need(EVP_Digest(b.data(),b.size(),h.data(),&n,EVP_sha256(),nullptr)==1&&n==32,"sha256_failed");return h; }
std::string B64(const Bytes& b) { std::string s(4*((b.size()+2)/3),'\0');EVP_EncodeBlock(reinterpret_cast<unsigned char*>(s.data()),b.data(),static_cast<int>(b.size()));return s; }
Bytes Un64(const std::string& s,std::size_t maximum,std::size_t exact=0) {
    Need(s.size()<=4*((maximum+2)/3)&&s.size()%4==0,"bad_base64");
    Bytes b(s.size()/4*3+3); int n=EVP_DecodeBlock(b.data(),reinterpret_cast<const unsigned char*>(s.data()),static_cast<int>(s.size()));
    Need(n>=0,"bad_base64");if(!s.empty()&&s.back()=='=')--n;if(s.size()>1&&s[s.size()-2]=='=')--n;
    Need(n>=0&&static_cast<std::size_t>(n)<=maximum,"bad_base64");b.resize(n);Need(B64(b)==s&&(!exact||b.size()==exact),"bad_base64");return b;
}
void Unique(const Value& v,unsigned depth=0) {
    Need(depth<32,"json_too_deep");
    if(v.IsObject()){std::set<std::string> keys;for(auto i=v.MemberBegin();i!=v.MemberEnd();++i){Need(keys.insert(std::string(i->name.GetString(),i->name.GetStringLength())).second,"duplicate_json_key");Unique(i->value,depth+1);}}
    if(v.IsArray())for(const auto& item:v.GetArray())Unique(item,depth+1);
}
Doc Parse(const std::string& text) {
    Need(text.size()<=65536,"message_too_large");
    // Reject nesting before allocating a DOM: both parsing and DOM destruction
    // must remain bounded, including when a peer supplies malformed JSON.
    unsigned depth=0;bool quoted=false,escaped=false;
    for(unsigned char c:text){
        if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}
        else if(c=='"')quoted=true;
        else if(c=='{'||c=='['){Need(++depth<=32,"json_too_deep");}
        else if(c=='}'||c==']'){Need(depth>0,"bad_json");--depth;}
    }
    Doc d;d.Parse<rapidjson::kParseValidateEncodingFlag|rapidjson::kParseIterativeFlag>(text.data(),text.size());Need(!d.HasParseError()&&d.IsObject(),"bad_json");Unique(d);return d;
}
std::string String(const Value& v,const char* key,std::size_t max=4096) { Need(v.IsObject()&&v.HasMember(key)&&v[key].IsString(),"bad_string");const auto& s=v[key];Need(s.GetStringLength()<=max,"bad_string");return {s.GetString(),s.GetStringLength()}; }
std::uint64_t Number(const Value& v,const char* key) { Need(v.IsObject()&&v.HasMember(key)&&v[key].IsUint64()&&v[key].GetUint64()<=9007199254740991ULL,"bad_number");return v[key].GetUint64(); }
bool Boolean(const Value& v,const char* key) {Need(v.IsObject()&&v.HasMember(key)&&v[key].IsBool(),"bad_boolean");return v[key].GetBool();}
Doc Object() { Doc d;d.SetObject();return d; }
void Put(Doc& d,const char* key,const std::string& s) { Value k(key,d.GetAllocator()),v(s.data(),static_cast<rapidjson::SizeType>(s.size()),d.GetAllocator());d.AddMember(k,v,d.GetAllocator()); }
std::string Json(const Doc& d) {rapidjson::StringBuffer b;rapidjson::Writer<rapidjson::StringBuffer,rapidjson::UTF8<>,rapidjson::UTF8<>,rapidjson::CrtAllocator,rapidjson::kWriteValidateEncodingFlag> w(b);Need(d.Accept(w),"invalid_utf8");return {b.GetString(),b.GetSize()};}
Doc Command(const char* type) {Doc d=Object();Put(d,"t",type);return d;}
bool Text(const std::string& text,std::size_t max) {return !text.empty()&&text.size()<=max&&text.find_first_not_of(' ')!=std::string::npos&&std::none_of(text.begin(),text.end(),[](unsigned char c){return c<32||c==127;});}
std::string Uuid(Bytes* raw=nullptr) {Bytes b(16);Need(RAND_bytes(b.data(),16)==1,"random_failed");b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;const char* hex="0123456789abcdef";std::string s;for(unsigned i=0;i<16;++i){if(i==4||i==6||i==8||i==10)s+='-';s+=hex[b[i]>>4];s+=hex[b[i]&15];}if(raw)*raw=b;return s;}
struct Key {
    std::array<unsigned char,32> seed{};
    EVP_PKEY* key=nullptr;
    ~Key(){EVP_PKEY_free(key);OPENSSL_cleanse(seed.data(),seed.size());}
    void Open(){key=EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519,nullptr,seed.data(),seed.size());Need(key,"invalid_private_key");}
    Bytes Public(){Bytes b(32);std::size_t n=b.size();Need(EVP_PKEY_get_raw_public_key(key,b.data(),&n)==1&&n==32,"public_key_failed");return b;}
    Bytes Sign(const Bytes& data){EVP_MD_CTX* context=EVP_MD_CTX_new();Need(context,"sign_failed");Bytes signature(64);std::size_t n=64;bool ok=EVP_DigestSignInit(context,nullptr,nullptr,nullptr,key)==1&&EVP_DigestSign(context,signature.data(),&n,data.data(),data.size())==1&&n==64;EVP_MD_CTX_free(context);Need(ok,"sign_failed");return signature;}
};
bool Verify(const Bytes& key,const Bytes& data,const Bytes& signature) {EVP_PKEY* k=EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519,nullptr,key.data(),key.size());EVP_MD_CTX* c=EVP_MD_CTX_new();bool ok=k&&c&&EVP_DigestVerifyInit(c,nullptr,nullptr,nullptr,k)==1&&EVP_DigestVerify(c,signature.data(),signature.size(),data.data(),data.size())==1;EVP_MD_CTX_free(c);EVP_PKEY_free(k);return ok;}
Bytes Protect(const Bytes& value,bool decrypt) {DATA_BLOB input{static_cast<DWORD>(value.size()),const_cast<BYTE*>(value.data())},output{};BOOL ok=decrypt?CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output):CryptProtectData(&input,L"Community client account",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output);Need(ok,"profile_protection_failed");Bytes result(output.pbData,output.pbData+output.cbData);SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);return result;}
struct Handle {HANDLE value=INVALID_HANDLE_VALUE;~Handle(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
std::filesystem::path ProfilePath(const std::string& text) {
    Need(!text.empty()&&text.size()<=131068&&text.find('\0')==std::string::npos,"invalid_profile_directory");
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    Need(count>0&&count<=32767,"invalid_profile_directory");
    std::wstring wide(static_cast<std::size_t>(count),L'\0');
    Need(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),wide.data(),count)==count,"invalid_profile_directory");
    return std::filesystem::path(wide);
}
void Save(const std::filesystem::path& path,const Doc& profile,bool create) {
    auto temporary=path;temporary+=L"."+std::filesystem::path(Uuid()).wstring()+L".tmp";
    std::string bytes=Json(profile);Handle file;file.value=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);Need(file.value!=INVALID_HANDLE_VALUE,"profile_write_failed");
    DWORD written=0;bool ok=WriteFile(file.value,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file.value);CloseHandle(file.value);file.value=INVALID_HANDLE_VALUE;
    if(!ok){DeleteFileW(temporary.c_str());throw Error("profile_write_failed");}
    DWORD flags=MOVEFILE_WRITE_THROUGH|(create?0:MOVEFILE_REPLACE_EXISTING);if(!MoveFileExW(temporary.c_str(),path.c_str(),flags)){DeleteFileW(temporary.c_str());throw Error("profile_commit_failed");}
}
struct Profile {std::filesystem::path path;Handle lock;Key key;std::string publicKey,username,player;bool pending=false;
    void Write(const Options& o,bool create=false){Doc d=Object();Put(d,"schema","community-online-v1");Put(d,"server",o.serverUrl);Put(d,"serverKey",o.serverKey);Put(d,"publicKey",publicKey);Put(d,"seedProtected",B64(Protect(Bytes(key.seed.begin(),key.seed.end()),false)));Put(d,"username",username);Put(d,"player",player);d.AddMember("pending",pending,d.GetAllocator());Save(path,d,create);}
    void Load(const Options& o){auto dir=ProfilePath(o.profileDirectory);std::filesystem::create_directories(dir);path=dir/"community-online.json";auto lockPath=dir/"community-online.lock";lock.value=CreateFileW(lockPath.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);Need(lock.value!=INVALID_HANDLE_VALUE,"profile_in_use");
        if(std::filesystem::exists(path)){Need(std::filesystem::file_size(path)<=8192,"invalid_profile");std::ifstream input(path,std::ios::binary);std::string text((std::istreambuf_iterator<char>(input)),{});Doc d=Parse(text);Need(String(d,"schema")=="community-online-v1"&&String(d,"server")==o.serverUrl&&String(d,"serverKey")==o.serverKey,"profile_identity_mismatch");Bytes secret=Protect(Un64(String(d,"seedProtected"),4096),true);Need(secret.size()==32,"invalid_profile");std::copy(secret.begin(),secret.end(),key.seed.begin());OPENSSL_cleanse(secret.data(),secret.size());key.Open();publicKey=B64(key.Public());Need(publicKey==String(d,"publicKey"),"profile_identity_mismatch");username=String(d,"username",16);player=String(d,"player",20);pending=Boolean(d,"pending");}
        else {Need(std::regex_match(o.username,std::regex("[A-Za-z][A-Za-z0-9_]{2,15}")),"invalid_username");Need(RAND_bytes(key.seed.data(),32)==1,"random_failed");key.Open();publicKey=B64(key.Public());username=o.username;pending=true;Write(o,true);}
        if(pending&&!o.username.empty()&&o.username!=username){Need(std::regex_match(o.username,std::regex("[A-Za-z][A-Za-z0-9_]{2,15}")),"invalid_username");username=o.username;Write(o);}
    }
};
std::string Origin(const std::string& source) {
    CURLU* u=curl_url();Need(u,"invalid_server_url");auto get=[&](CURLUPart part){char* text=nullptr;std::string s;if(curl_url_get(u,part,&text,0)==CURLUE_OK){s=text;curl_free(text);}return s;};
    if(curl_url_set(u,CURLUPART_URL,source.c_str(),0)!=CURLUE_OK){curl_url_cleanup(u);throw Error("invalid_server_url");}
    auto scheme=get(CURLUPART_SCHEME),host=get(CURLUPART_HOST),path=get(CURLUPART_PATH),port=get(CURLUPART_PORT);
    bool ok=(scheme=="https"||(scheme=="http"&&(host=="127.0.0.1"||host=="[::1]")))&&(path.empty()||path=="/")&&get(CURLUPART_USER).empty()&&get(CURLUPART_PASSWORD).empty()&&get(CURLUPART_QUERY).empty()&&get(CURLUPART_FRAGMENT).empty();
    std::string result=scheme+"://"+host+(port.empty()?"":":"+port);curl_url_cleanup(u);Need(ok,"invalid_server_url");return result;
}
struct Token {std::string packed,player;Bytes bytes;std::uint64_t expires=0;};
std::uint64_t LE64(const Bytes& bytes,unsigned offset){std::uint64_t value=0;for(unsigned i=0;i<8;++i)value|=std::uint64_t(bytes[offset+i])<<(8*i);return value;}
Token CheckToken(const std::string& packed,const Options& o,Profile& profile){Token t;t.packed=packed;Bytes envelope=Un64(packed,3072);Doc d=Parse(std::string(envelope.begin(),envelope.end()));t.bytes=Un64(String(d,"bytes"),58,58);Need(Verify(Un64(o.serverKey,32,32),t.bytes,Un64(String(d,"sig"),64,64)),"invalid_server_signature");Need(t.bytes[0]==1&&t.bytes[1]==1&&std::equal(t.bytes.begin()+10,t.bytes.begin()+42,profile.key.Public().begin()),"token_identity_mismatch");auto issued=LE64(t.bytes,42);t.expires=LE64(t.bytes,50);Need(issued<=Now()+5000&&t.expires>Now()&&t.expires>issued&&t.expires-issued<=3600000,"token_expired");t.player=std::to_string(LE64(t.bytes,2));Need(t.player!="0"&&(profile.player.empty()||profile.player==t.player),"token_identity_mismatch");return t;}
std::string Hex(const Bytes& bytes){const char* digits="0123456789abcdef";std::string text;for(auto b:bytes){text+=digits[b>>4];text+=digits[b&15];}return text;}
bool Hex64(const std::string& text){return text.size()==64&&text.find_first_not_of("0123456789abcdef")==std::string::npos;}
bool IsUuid(const std::string& text){return std::regex_match(text,std::regex("[a-f0-9]{8}(-[a-f0-9]{4}){3}-[a-f0-9]{12}"));}
bool TrainerName(const std::string& name){return name.size()<=10&&std::regex_match(name,std::regex("[A-Za-z0-9](?:[A-Za-z0-9 .'-]{0,8}[A-Za-z0-9.])?"));}
bool PlayerId(const std::string& player){return std::regex_match(player,std::regex("[1-9][0-9]{0,19}"));}
bool CityStatus(const std::string& status){return status=="active"||status=="menu"||status=="away"||status=="afk";}
Appearance ReadAppearance(const Value& value){
    Appearance a;auto look=Number(value,"look"),outfit=Number(value,"outfit");Need(look<=255&&outfit<=65535,"bad_appearance");a.look=static_cast<std::uint8_t>(look);a.outfit=static_cast<std::uint16_t>(outfit);
    Need(value.HasMember("art"),"bad_appearance");const auto& art=value["art"];if(!art.IsNull()){
        Need(art.IsObject()&&art.MemberCount()==4,"bad_art_reference");a.art.creation=String(art,"creation",40);a.art.version=String(art,"version",40);a.art.format=String(art,"template",64);a.art.hash=String(art,"hash",64);
        Need(std::regex_match(a.art.creation,std::regex("[A-Za-z0-9_-]{1,40}"))&&std::regex_match(a.art.version,std::regex("[A-Za-z0-9_-]{1,40}"))&&a.art.format=="platinum-trainer-209-v1"&&Hex64(a.art.hash),"bad_art_reference");a.hasArt=true;
    }return a;
}
Trainer ReadTrainer(const Value& value){Trainer t;t.name=String(value,"name",10);Need(TrainerName(t.name),"bad_trainer_name");auto look=Number(value,"look"),outfit=Number(value,"outfit");Need(look<=255&&outfit<=65535,"bad_appearance");t.look=static_cast<std::uint8_t>(look);t.outfit=static_cast<std::uint16_t>(outfit);t.tutorialDone=Boolean(value,"tutorial_done");t.revision=Number(value,"revision");Need(t.revision>0,"bad_trainer_revision");return t;}
CityPlayer ReadCityPlayer(const Value& value){CityPlayer p;p.player=String(value,"player",20);p.name=String(value,"name",10);p.status=String(value,"status",8);auto slot=Number(value,"slot");Need(PlayerId(p.player)&&TrainerName(p.name)&&CityStatus(p.status)&&slot<64,"bad_city_player");p.slot=static_cast<std::uint8_t>(slot);p.appearance=ReadAppearance(value);return p;}
struct LinkOffer {LinkSnapshot info;std::string ticket,url,ownKey,rosterJson;std::uint64_t ticketExpires=0;};
LinkOffer LinkDescriptor(const Doc& d){
    LinkOffer offer;auto& info=offer.info;info.session=String(d,"session",36);info.party=String(d,"party",36);info.epoch=Number(d,"epoch");info.expiresAtMs=Number(d,"expires_at_ms");
    Need(IsUuid(info.session)&&IsUuid(info.party)&&info.epoch>0&&info.expiresAtMs>Now()&&info.expiresAtMs<=Now()+1800000+5000,"link_bad_descriptor");
    info.build=String(d,"build",128);info.catalog=String(d,"catalog",64);info.rules=String(d,"rules",64);info.rosterHash=String(d,"roster_hash",64);
    Need(std::regex_match(info.build,std::regex("community-client-[A-Za-z0-9._-]{1,100}"))&&Hex64(info.catalog)&&Hex64(info.rules)&&Hex64(info.rosterHash),"link_bad_fingerprint");
    Need(d.HasMember("roster")&&d["roster"].IsArray()&&d["roster"].Size()>=2&&d["roster"].Size()<=4,"link_bad_roster");Doc canonical;canonical.SetArray();std::set<std::string> players,keys,macs;
    for(const auto& member:d["roster"].GetArray()){
        auto seat=Number(member,"seat");auto player=String(member,"player",20),key=String(member,"key",44),mac=String(member,"mac",12);Un64(key,32,32);
        Need(seat==info.roster.size()&&std::regex_match(player,std::regex("[1-9][0-9]{0,19}"))&&std::regex_match(mac,std::regex("[a-f0-9]{12}"))&&players.insert(player).second&&keys.insert(key).second&&macs.insert(mac).second,"link_bad_roster");
        Bytes derived=Hash(Data(info.session+std::string(1,'\0')+player));derived.resize(6);derived[0]=(derived[0]|2)&254;Need(Hex(derived)==mac,"link_bad_mac");
        info.roster.push_back({static_cast<std::uint8_t>(seat),player,mac});Doc item=Object();item.AddMember("seat",seat,item.GetAllocator());Put(item,"player",player);Put(item,"key",key);Put(item,"mac",mac);Value copy;copy.CopyFrom(item,canonical.GetAllocator());canonical.PushBack(copy,canonical.GetAllocator());
    }
    offer.rosterJson=Json(canonical);Need(Hex(Hash(Data(offer.rosterJson)))==info.rosterHash,"link_bad_roster_hash");auto seat=Number(d,"seat");Need(seat<info.roster.size(),"link_bad_seat");info.seat=static_cast<std::uint8_t>(seat);offer.ownKey=String(d["roster"][static_cast<rapidjson::SizeType>(seat)],"key",44);return offer;
}
Bytes LinkTail(const LinkOffer& offer){
    const auto& s=offer.info;Doc binding;binding.SetArray();auto string=[&](const std::string& text){Value v(text.data(),static_cast<rapidjson::SizeType>(text.size()),binding.GetAllocator());binding.PushBack(v,binding.GetAllocator());};
    binding.PushBack(1,binding.GetAllocator());string(s.session);binding.PushBack(s.epoch,binding.GetAllocator());string(s.party);string(s.roster[s.seat].player);string(offer.ownKey);binding.PushBack(s.seat,binding.GetAllocator());string(s.build);string(s.catalog);string(s.rules);string(s.rosterHash);string(Hex(Hash(Un64(offer.ticket,32,32))));return Hash(Data(Json(binding)));
}
}

struct Service::Impl {
    mutable std::mutex mutex;
    Snapshot snapshot;
    LinkSnapshot link;
    std::deque<std::string> commands;
    std::deque<Bytes> linkOutgoing;
    std::deque<LinkPacket> linkIncoming;
    std::size_t linkOutgoingBytes=0,linkIncomingBytes=0;
    bool linkWanted=false,linkCancelPending=false,requestLinkCancel=false,linkPreparePending=false;
    bool trainerRequest=false,requestCityLeave=false;
    Clock::time_point trainerDeadline{},cityDeadline{};
    std::atomic<bool> stop{false};
    std::atomic<bool> active{false};
    std::atomic<bool> linkStop{false},linkActive{false};
    std::thread worker;
    std::thread linkWorker;
    CURL* socket=nullptr;
    Options options;
    static int Cancel(void* p,curl_off_t,curl_off_t,curl_off_t,curl_off_t){return static_cast<Impl*>(p)->stop?1:0;}
    static int LinkCancel(void* p,curl_off_t,curl_off_t,curl_off_t,curl_off_t){auto self=static_cast<Impl*>(p);return self->stop||self->linkStop?1:0;}
    void ClearLinkLocked(){linkOutgoing.clear();linkIncoming.clear();linkOutgoingBytes=linkIncomingBytes=0;}
    void EndLink(const std::string& reason,bool failed=false){std::lock_guard<std::mutex> guard(mutex);linkStop=true;linkWanted=false;linkPreparePending=false;ClearLinkLocked();if(link.state!=LinkState::Idle){if(link.state!=LinkState::Failed){link.state=failed?LinkState::Failed:LinkState::Ended;link.error=reason;}}}
    void JoinLink(){linkStop=true;if(linkWorker.joinable())linkWorker.join();std::lock_guard<std::mutex> guard(mutex);linkCancelPending=requestLinkCancel=linkPreparePending=false;}
    void LinkFailure(const std::string& reason){EndLink(reason,true);std::lock_guard<std::mutex> guard(mutex);if(snapshot.state==State::Ready){requestLinkCancel=true;linkCancelPending=true;}}
    void LinkRemoteEnd(const Doc& d){bool current=false;{std::lock_guard<std::mutex> guard(mutex);current=String(d,"session",36)==link.session&&Number(d,"epoch")==link.epoch;}if(current)EndLink(String(d,"reason",64));}
    bool LinkCurrent(const LinkOffer& offer){return link.session==offer.info.session&&link.epoch==offer.info.epoch;}
    void LinkStateTo(const LinkOffer& offer,LinkState state){std::lock_guard<std::mutex> guard(mutex);if(!stop&&!linkStop&&LinkCurrent(offer))link.state=state;}
    void LinkSend(CURL* transport,const Bytes& bytes,unsigned flags){std::size_t offset=0;auto deadline=Clock::now()+std::chrono::seconds(8);while(offset<bytes.size()&&!stop&&!linkStop){std::size_t sent=0;auto code=curl_ws_send(transport,bytes.data()+offset,bytes.size()-offset,&sent,0,flags);offset+=sent;if(code!=CURLE_OK&&code!=CURLE_AGAIN)throw Error("link_send_failed");Need(Clock::now()<=deadline,"link_send_timeout");if(code==CURLE_AGAIN)std::this_thread::sleep_for(std::chrono::milliseconds(5));}}
    void LinkSend(CURL* transport,const Doc& d){LinkSend(transport,Data(Json(d)),CURLWS_TEXT);}
    void LinkPeers(const Doc& d,const LinkOffer& offer){
        Need(String(d,"session",36)==offer.info.session&&Number(d,"epoch")==offer.info.epoch,"link_wrong_session");bool connected=Boolean(d,"active");Need(d.HasMember("connected")&&d["connected"].IsArray()&&d["connected"].Size()<=offer.info.roster.size(),"link_bad_peers");std::set<unsigned> seats;
        for(const auto& item:d["connected"].GetArray())Need(item.IsUint()&&item.GetUint()<offer.info.roster.size()&&seats.insert(item.GetUint()).second,"link_bad_peers");
        Need(seats.count(offer.info.seat)&&(!connected||seats.size()==offer.info.roster.size()),"link_bad_peers");std::lock_guard<std::mutex> guard(mutex);if(!stop&&!linkStop&&LinkCurrent(offer)){Need(link.state!=LinkState::Active||connected,"link_roster_changed");link.state=connected?LinkState::Active:LinkState::Waiting;}
    }
    void LinkRun(LinkOffer offer,std::unique_ptr<Key> key){
        CURL* transport=nullptr;
        try{
            key->Open();transport=curl_easy_init();Need(transport,"curl_init_failed");Setup(transport,offer.url);curl_easy_setopt(transport,CURLOPT_XFERINFOFUNCTION,LinkCancel);curl_easy_setopt(transport,CURLOPT_CONNECT_ONLY,2L);Need(curl_easy_perform(transport)==CURLE_OK,"link_connect_failed");LinkStateTo(offer,LinkState::Authenticating);
            Doc hello=Command("community.link.hello");hello.AddMember("pt",1,hello.GetAllocator());Put(hello,"ticket",offer.ticket);Put(hello,"session",offer.info.session);hello.AddMember("epoch",offer.info.epoch,hello.GetAllocator());Put(hello,"build",offer.info.build);Put(hello,"catalog",offer.info.catalog);Put(hello,"rules",offer.info.rules);LinkSend(transport,hello);
            unsigned phase=0,kind=0;Bytes incoming;auto authDeadline=Clock::now()+std::chrono::seconds(10),lastPing=Clock::now(),pongDeadline=Clock::now(),nextPacket=Clock::now();bool awaitingPong=false;std::uint64_t pingStamp=0;
            while(!stop&&!linkStop){
                Need(offer.info.expiresAtMs>Now(),"link_session_expired");if(phase<2)Need(Clock::now()<=authDeadline,"link_auth_timeout");if(phase==2){
                    bool isActive=false;Bytes packet;{std::lock_guard<std::mutex> guard(mutex);if(LinkCurrent(offer)){isActive=link.state==LinkState::Active;if(isActive&&Clock::now()>=nextPacket&&!linkOutgoing.empty()){packet=std::move(linkOutgoing.front());linkOutgoing.pop_front();linkOutgoingBytes-=packet.size();}}}
                    if(!isActive)Need(Now()<offer.ticketExpires,"link_join_timeout");
                    if(!packet.empty()){LinkSend(transport,packet,CURLWS_BINARY);nextPacket=Clock::now()+std::chrono::milliseconds(std::max<std::size_t>(9,(packet.size()*1000+262143)/262144));}
                    if(awaitingPong)Need(Clock::now()<=pongDeadline,"link_pong_timeout");
                    if(!awaitingPong&&Clock::now()-lastPing>std::chrono::milliseconds(options.pingIntervalMs)){Doc ping=Command("ping");pingStamp=Now();ping.AddMember("ts",pingStamp,ping.GetAllocator());LinkSend(transport,ping);lastPing=Clock::now();pongDeadline=lastPing+std::chrono::milliseconds(options.pongTimeoutMs);awaitingPong=true;}
                }
                char bytes[4096];std::size_t count=0;const curl_ws_frame* meta=nullptr;auto code=curl_ws_recv(transport,bytes,sizeof(bytes),&count,&meta);if(code==CURLE_AGAIN){std::this_thread::sleep_for(std::chrono::milliseconds(2));continue;}Need(code==CURLE_OK&&meta,"link_disconnected");Need(!(meta->flags&CURLWS_CLOSE),"link_disconnected");if(meta->flags&(CURLWS_PING|CURLWS_PONG))continue;
                unsigned current=(meta->flags&CURLWS_BINARY)?2:(meta->flags&CURLWS_TEXT)?1:0;Need(current&&(!kind||kind==current),"link_bad_frame");kind=current;Need(count<=(kind==2?8194U:65536U)-incoming.size(),"link_message_too_large");incoming.insert(incoming.end(),bytes,bytes+count);if(meta->bytesleft||meta->flags&CURLWS_CONT)continue;
                if(kind==2){Need(phase==2&&incoming.size()>=3&&incoming.size()<=8194&&incoming[0]==1&&incoming[1]<offer.info.roster.size()&&incoming[1]!=offer.info.seat,"link_bad_binary");std::lock_guard<std::mutex> guard(mutex);if(!stop&&!linkStop&&LinkCurrent(offer)){Need(link.state==LinkState::Active,"link_binary_before_active");Need(linkIncoming.size()<128&&incoming.size()-2<=262144-linkIncomingBytes,"link_receive_overflow");LinkPacket packet;packet.session=offer.info.session;packet.epoch=offer.info.epoch;packet.sender=incoming[1];packet.payload.assign(incoming.begin()+2,incoming.end());linkIncomingBytes+=packet.payload.size();linkIncoming.push_back(std::move(packet));}}
                else {Doc d=Parse(std::string(incoming.begin(),incoming.end()));auto type=String(d,"t",64);
                    if(type=="err")throw Error(String(d,"code",64));
                    if(type=="community.link.challenge"){Need(phase==0&&String(d,"session",36)==offer.info.session&&Number(d,"epoch")==offer.info.epoch,"link_bad_challenge");Bytes proof=Domain("PT-COMMUNITY-LINK");Append(proof,Un64(String(d,"nonce"),32,32));Append(proof,LinkTail(offer));Doc auth=Command("auth");Put(auth,"sig",B64(key->Sign(proof)));LinkSend(transport,auth);phase=1;}
                    else if(type=="community.link.ready"){Need(phase==1&&Number(d,"pt")==1,"link_bad_ready");auto ready=LinkDescriptor(d);Need(ready.info.session==offer.info.session&&ready.info.epoch==offer.info.epoch&&ready.info.party==offer.info.party&&ready.info.expiresAtMs==offer.info.expiresAtMs&&ready.info.build==offer.info.build&&ready.info.catalog==offer.info.catalog&&ready.info.rules==offer.info.rules&&ready.rosterJson==offer.rosterJson&&ready.info.seat==offer.info.seat,"link_bad_ready");Need(d.HasMember("limits")&&Number(d["limits"],"payload_bytes")==8192&&Number(d["limits"],"frames_per_second")==120&&Number(d["limits"],"bytes_per_second")==262144,"link_bad_limits");phase=2;LinkStateTo(offer,Boolean(d,"active")?LinkState::Active:LinkState::Waiting);}
                    else if(type=="community.link.peers"){Need(phase==2,"link_message_before_ready");LinkPeers(d,offer);}
                    else if(type=="community.link.ended"||type=="community.link.peer_left"){Need(String(d,"session",36)==offer.info.session&&Number(d,"epoch")==offer.info.epoch,"link_wrong_session");LinkRemoteEnd(d);}
                    else if(type=="pong"){if(phase==2&&awaitingPong&&Number(d,"ts")==pingStamp)awaitingPong=false;}
                    else throw Error("link_unexpected_message");
                }
                incoming.clear();kind=0;
            }
        }catch(const std::exception& error){if(!stop&&!linkStop)LinkFailure(error.what());}
        if(transport)curl_easy_cleanup(transport);
        linkActive=false;
    }
    void LinkOffered(const Doc& d,Profile& profile){
        Need(Number(d,"pt")==1,"link_bad_offer");LinkOffer offer=LinkDescriptor(d);offer.ticket=String(d,"ticket",44);Un64(offer.ticket,32,32);offer.ticketExpires=Number(d,"ticket_expires_at_ms");offer.url=String(d,"link_url",4096);
        std::string expectedUrl=options.serverUrl;expectedUrl.replace(0,expectedUrl.find(':'),expectedUrl.rfind("https:",0)==0?"wss":"ws");expectedUrl+="/community-link";
        Need(offer.url==expectedUrl&&offer.ticketExpires>Now()&&offer.ticketExpires<=Now()+30000+5000&&offer.ownKey==profile.publicKey&&String(d,"key",44)==profile.publicKey&&String(d,"player",20)==profile.player&&offer.info.roster[offer.info.seat].player==profile.player,"link_bad_offer");
        {std::lock_guard<std::mutex> guard(mutex);if(LinkCurrent(offer)||!linkWanted)return;Need(link.available&&snapshot.state==State::Ready&&snapshot.partyId==offer.info.party&&offer.info.build==options.build&&offer.info.catalog==link.catalog&&offer.info.rules==link.rules&&snapshot.members.size()==offer.info.roster.size(),"link_offer_mismatch");for(std::size_t i=0;i<snapshot.members.size();i++)Need(snapshot.members[i].player==offer.info.roster[i].player,"link_roster_mismatch");Need(!linkActive,"link_overlap");}
        if(linkWorker.joinable())linkWorker.join();
        auto key=std::make_unique<Key>();key->seed=profile.key.seed;{std::lock_guard<std::mutex> guard(mutex);if(!linkWanted||stop)return;auto request=link.requestId;link=offer.info;link.requestId=request;link.available=true;link.state=LinkState::Connecting;ClearLinkLocked();linkStop=false;linkActive=true;}
        try{linkWorker=std::thread([this,offer=std::move(offer),key=std::move(key)]()mutable{LinkRun(std::move(offer),std::move(key));});}catch(...){linkActive=false;LinkFailure("link_worker_start_failed");}
    }
    static size_t Receive(char* bytes,size_t size,size_t count,void* target){std::string& text=*static_cast<std::string*>(target);std::size_t n=size*count;if(n>65536-text.size())return 0;text.append(bytes,n);return n;}
    void Setup(CURL* curl,const std::string& url){curl_easy_setopt(curl,CURLOPT_URL,url.c_str());curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,8000L);curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,15000L);curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(curl,CURLOPT_SSL_OPTIONS,static_cast<long>(CURLSSLOPT_NATIVE_CA));curl_easy_setopt(curl,CURLOPT_CAINFO,nullptr);curl_easy_setopt(curl,CURLOPT_CAPATH,nullptr);curl_easy_setopt(curl,CURLOPT_PROXY,"");curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,Cancel);curl_easy_setopt(curl,CURLOPT_XFERINFODATA,this);}
    Doc Http(const std::string& path,const Doc* body=nullptr){CURL* c=curl_easy_init();Need(c,"curl_init_failed");Setup(c,options.serverUrl+path);std::string received,payload;curl_slist* headers=nullptr;curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,Receive);curl_easy_setopt(c,CURLOPT_WRITEDATA,&received);if(body){payload=Json(*body);headers=curl_slist_append(headers,"Content-Type: application/json");curl_easy_setopt(c,CURLOPT_HTTPHEADER,headers);curl_easy_setopt(c,CURLOPT_POSTFIELDS,payload.data());curl_easy_setopt(c,CURLOPT_POSTFIELDSIZE,static_cast<long>(payload.size()));}CURLcode code=curl_easy_perform(c);long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);curl_slist_free_all(headers);curl_easy_cleanup(c);if(code!=CURLE_OK)throw Error("http_transport_failed",true);Doc d=Parse(received);if(status!=200)throw Error(d.HasMember("error")?String(d,"error",64):"http_refused");return d;}
    void ClearPresentationLocked(){snapshot.trainer=TrainerSnapshot{};snapshot.city=CitySnapshot{};trainerRequest=requestCityLeave=false;}
    void StateTo(State state,const std::string& error={}){std::lock_guard<std::mutex> guard(mutex);snapshot.state=state;snapshot.error=error;if(state==State::Connecting){linkCancelPending=requestLinkCancel=linkPreparePending=false;ClearPresentationLocked();}if(state==State::Failed)ClearPresentationLocked();}
    bool Queue(Doc d){std::string json;try{json=Json(d);}catch(...){return false;}std::lock_guard<std::mutex> guard(mutex);if(snapshot.state!=State::Ready||commands.size()>=64)return false;commands.push_back(std::move(json));return true;}
    void Send(const Doc& d){Send(Json(d));}
    void Send(const std::string& text){std::size_t offset=0;auto deadline=Clock::now()+std::chrono::seconds(8);while(offset<text.size()&&!stop){std::size_t sent=0;auto code=curl_ws_send(socket,text.data()+offset,text.size()-offset,&sent,0,CURLWS_TEXT);offset+=sent;if(code!=CURLE_OK&&code!=CURLE_AGAIN)throw Error("lobby_send_failed",true);if(Clock::now()>deadline)throw Error("lobby_send_timeout",true);if(code==CURLE_AGAIN)std::this_thread::sleep_for(std::chrono::milliseconds(5));}}
    Token Account(Profile& p){Doc info=Http("/private/server-info");Need(String(info,"hub_key")==options.serverKey,"server_identity_changed");Doc request=Object();std::string action=p.pending?"register":"login";Put(request,"action",action);Put(request,"key",p.publicKey);if(p.pending)Put(request,"username",p.username);Doc challenge;
        try{challenge=Http("/auth/account/challenge",&request);}catch(const Error& e){if(!p.pending||std::string(e.what())!="already_enrolled")throw;action="login";request=Object();Put(request,"action",action);Put(request,"key",p.publicKey);challenge=Http("/auth/account/challenge",&request);}
        auto nonce=String(challenge,"nonce");Bytes proof=Domain("PT-PRIVATE-ACCOUNT");Append(proof,Un64(nonce,32,32));std::string lower=action=="register"?p.username:"";std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});std::string binding=action;binding.push_back(0);binding+=lower;binding.push_back(0);binding+=p.publicKey;Append(proof,Hash(Data(binding)));Put(request,"nonce",nonce);Put(request,"sig",B64(p.key.Sign(proof)));Doc response=Http("/auth/account",&request);Need(String(response,"hub_key")==options.serverKey,"server_identity_changed");Token token=CheckToken(String(response,"token"),options,p);Need(token.player==String(response,"player_id",20)&&token.expires==Number(response,"expires_at_ms"),"account_identity_mismatch");p.player=token.player;p.username=String(response,"username",16);p.pending=false;p.Write(options);return token;
    }
    void Party(const Doc& d){Need(d.HasMember("party"),"bad_party");std::vector<Member> members;std::string id,code;if(!d["party"].IsNull()){const auto& p=d["party"];id=String(p,"id",64);code=String(p,"code",6);Need(p.HasMember("members")&&p["members"].IsArray()&&p["members"].Size()<=4,"bad_party");std::set<std::string> ids;for(const auto& m:p["members"].GetArray()){Member member;member.player=String(m,"player",20);member.name=String(m,"name",10);member.online=Boolean(m,"online");member.appearance=ReadAppearance(m);Need(PlayerId(member.player)&&TrainerName(member.name)&&ids.insert(member.player).second,"bad_party");members.push_back(std::move(member));}}
        std::lock_guard<std::mutex> guard(mutex);snapshot.partyId=id;snapshot.partyCode=code;snapshot.members=std::move(members);}
    void TrainerMessage(const Doc& d){Need(d.HasMember("trainer"),"bad_trainer");bool applied=Boolean(d,"applied");Trainer value;bool available=!d["trainer"].IsNull();if(available)value=ReadTrainer(d["trainer"]);Need(!applied||available,"bad_trainer");std::lock_guard<std::mutex> guard(mutex);if(stop||snapshot.state!=State::Ready)return;snapshot.trainer.conflict=snapshot.trainer.updating&&!applied;snapshot.trainer.updating=false;snapshot.trainer.loaded=true;snapshot.trainer.available=available;snapshot.trainer.value=std::move(value);trainerRequest=false;}
    void CityMessage(const Doc& d,const std::string& type){
        if(type=="city.joined"){
            Need(String(d,"district",16)=="plaza"&&String(d,"instance",64)=="private-plaza"&&Number(d,"cap")==64,"bad_city");auto slot=Number(d,"slot");Need(slot<64&&d.HasMember("players")&&d["players"].IsArray()&&d["players"].Size()<64,"bad_city");std::vector<CityPlayer> players;std::set<unsigned> slots;
            for(const auto& item:d["players"].GetArray()){auto p=ReadCityPlayer(item);Need(p.slot!=slot&&slots.insert(p.slot).second,"bad_city");players.push_back(std::move(p));}
            std::lock_guard<std::mutex> guard(mutex);if(stop||snapshot.state!=State::Ready||snapshot.city.state!=CityState::Entering)return;snapshot.city.state=CityState::Present;snapshot.city.slot=static_cast<std::uint8_t>(slot);snapshot.city.players=std::move(players);return;
        }
        if(type=="city.left"){String(d,"reason",64);std::lock_guard<std::mutex> guard(mutex);if(stop||snapshot.state!=State::Ready)return;snapshot.city=CitySnapshot{};requestCityLeave=false;return;}
        if(type=="city.add"){auto player=ReadCityPlayer(d);std::lock_guard<std::mutex> guard(mutex);if(stop||snapshot.state!=State::Ready||snapshot.city.state!=CityState::Present)return;Need(player.slot!=snapshot.city.slot,"bad_city_slot");auto& players=snapshot.city.players;auto old=std::find_if(players.begin(),players.end(),[&](const CityPlayer& p){return p.slot==player.slot;});if(old!=players.end())*old=std::move(player);else{Need(players.size()<63,"bad_city_size");players.push_back(std::move(player));}return;}
        auto slot=Number(d,"slot");Need(slot<64,"bad_city_slot");std::string status;if(type=="city.status"){status=String(d,"status",8);Need(CityStatus(status),"bad_city_status");}
        std::lock_guard<std::mutex> guard(mutex);if(stop||snapshot.state!=State::Ready||snapshot.city.state!=CityState::Present)return;auto& players=snapshot.city.players;auto old=std::find_if(players.begin(),players.end(),[&](const CityPlayer& p){return p.slot==slot;});if(old!=players.end()){if(type=="city.drop")players.erase(old);else old->status=status;}
    }
    void PresentationErrorLocked(const std::string& error){
        if(stop||snapshot.state!=State::Ready)return;
        if(error=="inverna_server_required"||((error=="city_full"||error=="trainer_required")&&snapshot.city.state==CityState::Entering)){
            snapshot.city=CitySnapshot{};snapshot.city.error=error;requestCityLeave=false;
        }else if((error=="city_full"||error=="trainer_required")&&snapshot.city.state==CityState::Leaving){
            // The enter failed, but its ordered city.leave still needs an ack.
            // Releasing this gate would let that old ack cancel a fresh enter.
            snapshot.city.error=error;
        }
    }
    void Chat(const Value& d){Message m{Number(d,"id"),String(d,"from",64),String(d,"name",64),String(d,"text",1024)};std::lock_guard<std::mutex> guard(mutex);auto found=std::find_if(snapshot.messages.begin(),snapshot.messages.end(),[&](const Message& old){return old.id==m.id;});if(found==snapshot.messages.end()){snapshot.messages.push_back(std::move(m));if(snapshot.messages.size()>200)snapshot.messages.erase(snapshot.messages.begin());}}
    void Lobby(Profile& profile,Token token){std::string url=options.serverUrl;url.replace(0,url.find(':'),url.rfind("https:",0)==0?"wss":"ws");socket=curl_easy_init();Need(socket,"curl_init_failed");Setup(socket,url+"/lobby");curl_easy_setopt(socket,CURLOPT_CONNECT_ONLY,2L);if(curl_easy_perform(socket)!=CURLE_OK)throw Error("lobby_connect_failed",true);StateTo(State::Authenticating);Doc hello=Command("lobby.hello");hello.AddMember("pt",2,hello.GetAllocator());Put(hello,"token",token.packed);Put(hello,"build",options.build);Send(hello);
        int phase=0;auto deadline=Clock::now()+std::chrono::seconds(10),lastPing=Clock::now(),pongDeadline=Clock::now();std::uint64_t renewSent=0,pingStamp=0;bool awaitingPong=false;std::string incoming;bool incomingBinary=false;
        while(!stop){
            // Run deadlines before receive: unrelated/control traffic cannot
            // postpone authentication, renewal or a matched application pong.
            if(phase<2&&Clock::now()>deadline)throw Error("lobby_auth_timeout",true);
            if(phase==2){std::deque<std::string> pending;bool cancel=false,leaveCity=false;{std::lock_guard<std::mutex> guard(mutex);Need(!trainerRequest||Clock::now()<=trainerDeadline,"trainer_response_timeout");Need((snapshot.city.state!=CityState::Entering&&snapshot.city.state!=CityState::Leaving)||Clock::now()<=cityDeadline,"city_response_timeout");pending.swap(commands);cancel=requestLinkCancel;requestLinkCancel=false;leaveCity=requestCityLeave;requestCityLeave=false;}for(const auto& message:pending)Send(message);if(cancel)Send(Command("community.link.cancel"));if(leaveCity)Send(Command("city.leave"));if(awaitingPong&&Clock::now()>pongDeadline)throw Error("lobby_pong_timeout",true);if(!awaitingPong&&Clock::now()-lastPing>std::chrono::milliseconds(options.pingIntervalMs)){Doc ping=Command("ping");pingStamp=Now();ping.AddMember("ts",pingStamp,ping.GetAllocator());Send(ping);lastPing=Clock::now();pongDeadline=lastPing+std::chrono::milliseconds(options.pongTimeoutMs);awaitingPong=true;}
                if(token.expires<=Now()+120000&&!renewSent){Bytes raw;std::string op=Uuid(&raw);Bytes proof=Domain("PT2-RENEW");Append(proof,raw);Append(proof,Hash(token.bytes));Doc renew=Command("auth.renew");Put(renew,"op",op);Put(renew,"sig",B64(profile.key.Sign(proof)));Send(renew);renewSent=Now();}if(renewSent&&Now()-renewSent>15000)throw Error("renewal_timeout",true);}
            char bytes[4096];std::size_t n=0;const curl_ws_frame* meta=nullptr;auto code=curl_ws_recv(socket,bytes,sizeof(bytes),&n,&meta);if(code==CURLE_AGAIN){std::this_thread::sleep_for(std::chrono::milliseconds(5));}else{if(code!=CURLE_OK)throw Error("lobby_disconnected",true);Need(meta,"bad_websocket_frame");if(meta->flags&CURLWS_CLOSE)throw Error("lobby_closed",true);if(meta->flags&(CURLWS_PING|CURLWS_PONG))continue;Need((meta->flags&(CURLWS_TEXT|CURLWS_BINARY))!=0,"bad_websocket_frame");bool binary=(meta->flags&CURLWS_BINARY)!=0;if(incoming.empty())incomingBinary=binary;Need(binary==incomingBinary,"mixed_websocket_message");const std::size_t limit=binary?12:65536;Need(incoming.size()<=limit&&n<=limit-incoming.size(),"message_too_large");incoming.append(bytes,n);if(meta->bytesleft==0&&!(meta->flags&CURLWS_CONT)){
                if(incomingBinary){Need(phase==2&&incoming.size()==12&&static_cast<unsigned char>(incoming[0])==2&&incoming[1]==1&&static_cast<unsigned char>(incoming[2])<64&&incoming[3]==8,"unexpected_binary");incoming.clear();continue;} // Opaque city pose: validated framing only, never game coordinates.
                Doc d=Parse(incoming);incoming.clear();std::string type=String(d,"t",64);
                if(type=="err"){std::string error=String(d,"code",64);if(phase<2||(d.HasMember("fatal")&&Boolean(d,"fatal")))throw Error(error);std::lock_guard<std::mutex> guard(mutex);snapshot.error=error;PresentationErrorLocked(error);if(error.rfind("link_",0)==0||((error=="party_required"||error=="not_leader")&&link.state==LinkState::Opening)){linkPreparePending=false;if(link.state==LinkState::Preparing||link.state==LinkState::Opening){linkWanted=false;linkStop=true;link.state=LinkState::Failed;link.error=error;ClearLinkLocked();}}}
                else if(type=="challenge"){Need(phase==0,"unexpected_challenge");Bytes proof=Domain("PT2-LOBBY");Append(proof,Un64(String(d,"nonce"),32,32));Append(proof,Hash(token.bytes));Doc auth=Command("auth");Put(auth,"sig",B64(profile.key.Sign(proof)));Send(auth);phase=1;}
                else if(type=="lobby.ready"){Need(phase==1&&String(d,"player",20)==token.player&&Number(d,"expires_at_ms")==token.expires,"bad_lobby_ready");phase=2;{std::lock_guard<std::mutex> guard(mutex);snapshot.player=profile.player;snapshot.username=profile.username;snapshot.state=State::Ready;snapshot.error.clear();snapshot.connections++;trainerRequest=true;trainerDeadline=Clock::now()+std::chrono::seconds(15);link.available=d.HasMember("features")&&d["features"].IsObject()&&d["features"].HasMember("community_link")&&Boolean(d["features"],"community_link");}Doc join=Command("chat.join");Put(join,"channel","global");Send(join);Send(Command("trainer.get"));}
                else if(type=="community.link.prepared"){std::lock_guard<std::mutex> guard(mutex);Need(String(d,"build",128)==options.build&&String(d,"catalog",64)==link.catalog&&String(d,"rules",64)==link.rules,"link_bad_prepared");linkPreparePending=false;if(link.state==LinkState::Preparing)link.state=LinkState::Prepared;}
                else if(type=="community.link.cancelled"){std::lock_guard<std::mutex> guard(mutex);if(linkCancelPending){linkCancelPending=requestLinkCancel=linkPreparePending=false;linkWanted=false;}}
                else if(type=="community.link.offer"){try{LinkOffered(d,profile);}catch(const std::exception& error){LinkFailure(error.what());}}
                else if(type=="community.link.ended"||type=="community.link.peer_left"){LinkRemoteEnd(d);}
                else if(type=="trainer"){Need(phase==2,"message_before_ready");TrainerMessage(d);}
                else if(type=="city.joined"||type=="city.add"||type=="city.drop"||type=="city.status"||type=="city.left"){Need(phase==2,"message_before_ready");CityMessage(d,type);}
                else if(type=="party"){Need(phase==2,"message_before_ready");Party(d);}
                else if(type=="chat.new"){Need(phase==2&&String(d,"channel",32)=="global"&&d.HasMember("msg"),"bad_chat");Chat(d["msg"]);}
                else if(type=="chat"){Need(phase==2&&String(d,"channel",32)=="global"&&d.HasMember("msgs")&&d["msgs"].IsArray()&&d["msgs"].Size()<=50,"bad_chat");for(const auto& m:d["msgs"].GetArray())Chat(m);}
                else if(type=="auth.renewed"){Need(phase==2&&renewSent,"unexpected_renewal");Token next=CheckToken(String(d,"token"),options,profile);Need(next.expires==Number(d,"expires_at_ms")&&next.expires>token.expires,"invalid_renewal");token=std::move(next);renewSent=0;}
                else if(type=="pong"){if(phase==2&&awaitingPong&&Number(d,"ts")==pingStamp)awaitingPong=false;}
            }}
        }
    }
    void Run(){try{static std::once_flag init;static CURLcode initialized=CURLE_FAILED_INIT;std::call_once(init,[]{initialized=curl_global_init(CURL_GLOBAL_DEFAULT);});Need(initialized==CURLE_OK,"curl_global_init_failed");options.serverUrl=Origin(options.serverUrl);Un64(options.serverKey,32,32);Need(!options.build.empty()&&options.build.size()<=128,"invalid_build");Doc info=Http("/private/server-info");Need(String(info,"hub_key")==options.serverKey,"server_identity_changed");Profile profile;profile.Load(options);unsigned backoff=1;while(!stop){try{StateTo(State::Connecting);Token token=Account(profile);Lobby(profile,std::move(token));}catch(const Error& error){EndLink("lobby_disconnected");{std::lock_guard<std::mutex> guard(mutex);link.available=false;ClearPresentationLocked();}if(socket){curl_easy_cleanup(socket);socket=nullptr;}if(stop)break;if(!error.retry||!options.reconnect)throw;{std::lock_guard<std::mutex> guard(mutex);commands.clear();snapshot.members.clear();snapshot.partyId.clear();snapshot.partyCode.clear();snapshot.state=State::Reconnecting;snapshot.error=error.what();}for(unsigned i=0;i<backoff*100&&!stop;++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));backoff=std::min(backoff*2,15U);}}}catch(const std::exception& error){if(!stop)StateTo(State::Failed,error.what());}EndLink("stopped");JoinLink();if(socket){curl_easy_cleanup(socket);socket=nullptr;}if(stop){std::lock_guard<std::mutex> guard(mutex);snapshot.state=State::Stopped;snapshot.members.clear();snapshot.partyId.clear();snapshot.partyCode.clear();link.available=false;ClearPresentationLocked();}active=false;}
};
Service::Service():impl(new Impl){}
Service::~Service(){Disconnect();if(impl->worker.joinable())impl->worker.join();}
bool Service::Connect(Options options){if(impl->active)return false;if(options.pingIntervalMs<1000||options.pingIntervalMs>60000||options.pongTimeoutMs<1000||options.pongTimeoutMs>60000){impl->StateTo(State::Failed,"invalid_keepalive_options");return false;}if(impl->worker.joinable())impl->worker.join();{std::lock_guard<std::mutex> guard(impl->mutex);impl->snapshot=Snapshot{};impl->snapshot.state=State::Connecting;impl->options=std::move(options);impl->commands.clear();impl->link=LinkSnapshot{};impl->linkWanted=false;impl->ClearLinkLocked();impl->ClearPresentationLocked();}impl->stop=false;impl->active=true;try{impl->worker=std::thread([this]{impl->Run();});}catch(...){impl->active=false;impl->StateTo(State::Failed,"worker_start_failed");return false;}return true;}
void Service::Disconnect(){impl->stop=true;impl->EndLink("disconnected");std::lock_guard<std::mutex> guard(impl->mutex);impl->commands.clear();impl->ClearPresentationLocked();if(impl->active)impl->snapshot.state=State::Stopping;else{impl->snapshot.state=State::Stopped;impl->snapshot.members.clear();impl->snapshot.partyId.clear();impl->snapshot.partyCode.clear();}}
Snapshot Service::Read() const {std::lock_guard<std::mutex> guard(impl->mutex);return impl->snapshot;}
bool Service::CreateParty(){return impl->Queue(Command("party.create"));}
bool Service::JoinParty(const std::string& code){if(!std::regex_match(code,std::regex("[A-HJKMNP-Z2-9]{6}")))return false;Doc d=Command("party.join");Put(d,"code",code);return impl->Queue(std::move(d));}
bool Service::LeaveParty(){return impl->Queue(Command("party.leave"));}
bool Service::SendGlobalChat(const std::string& text){if(!Text(text,480))return false;Doc d=Command("chat.send");Put(d,"channel","global");Put(d,"text",text);Put(d,"op",Uuid());return impl->Queue(std::move(d));}
bool Service::RefreshTrainer(){std::lock_guard<std::mutex> guard(impl->mutex);if(impl->snapshot.state!=State::Ready||impl->trainerRequest||impl->commands.size()>=64)return false;impl->commands.push_back(Json(Command("trainer.get")));impl->trainerRequest=true;impl->trainerDeadline=Clock::now()+std::chrono::seconds(15);return true;}
bool Service::UpdateTrainer(const Trainer& trainer){
    if(!TrainerName(trainer.name)||trainer.revision>=9007199254740991ULL)return false;
    Doc d=Command("trainer.put");Put(d,"name",trainer.name);d.AddMember("look",trainer.look,d.GetAllocator());d.AddMember("outfit",trainer.outfit,d.GetAllocator());d.AddMember("tutorial_done",trainer.tutorialDone,d.GetAllocator());d.AddMember("revision",trainer.revision,d.GetAllocator());
    std::lock_guard<std::mutex> guard(impl->mutex);if(impl->snapshot.state!=State::Ready||!impl->snapshot.trainer.loaded||(impl->snapshot.trainer.available?trainer.revision==0:trainer.revision!=0)||impl->trainerRequest||impl->commands.size()>=64)return false;impl->commands.push_back(Json(d));impl->trainerRequest=true;impl->trainerDeadline=Clock::now()+std::chrono::seconds(15);impl->snapshot.trainer.updating=true;impl->snapshot.trainer.conflict=false;return true;
}
bool Service::EnterCity(){Doc d=Command("city.enter");Put(d,"district","plaza");std::lock_guard<std::mutex> guard(impl->mutex);if(impl->snapshot.state!=State::Ready||impl->snapshot.city.state!=CityState::Absent||impl->commands.size()>=64)return false;impl->commands.push_back(Json(d));impl->snapshot.city=CitySnapshot{};impl->snapshot.city.state=CityState::Entering;impl->snapshot.city.generation=impl->snapshot.connections;impl->cityDeadline=Clock::now()+std::chrono::seconds(15);return true;}
bool Service::LeaveCity(){std::lock_guard<std::mutex> guard(impl->mutex);if(impl->snapshot.state!=State::Ready||impl->snapshot.city.state==CityState::Absent)return false;if(impl->snapshot.city.state==CityState::Leaving)return true;impl->snapshot.city.state=CityState::Leaving;impl->snapshot.city.players.clear();impl->snapshot.city.slot=255;impl->requestCityLeave=true;impl->cityDeadline=Clock::now()+std::chrono::seconds(15);return true;}
bool Service::SetCityStatus(const std::string& status){if(!CityStatus(status))return false;Doc d=Command("city.status");Put(d,"status",status);std::lock_guard<std::mutex> guard(impl->mutex);if(impl->snapshot.state!=State::Ready||impl->snapshot.city.state!=CityState::Present||impl->commands.size()>=64)return false;impl->commands.push_back(Json(d));return true;}
LinkSnapshot Service::ReadLink() const {std::lock_guard<std::mutex> guard(impl->mutex);auto copy=impl->link;copy.cancelling=impl->linkCancelPending;return copy;}
bool Service::PrepareLink(const std::string& catalogHash,const std::string& rulesHash){
    if(!Hex64(catalogHash)||!Hex64(rulesHash))return false;
    std::lock_guard<std::mutex> guard(impl->mutex);
    if(impl->snapshot.state!=State::Ready||!impl->link.available||impl->linkActive||impl->linkCancelPending||impl->linkPreparePending||impl->commands.size()>=64||impl->link.state==LinkState::Opening||impl->link.state==LinkState::Preparing)return false;
    if(!std::regex_match(impl->options.build,std::regex("community-client-[A-Za-z0-9._-]{1,100}")))return false;
    Doc d=Command("community.link.prepare");Put(d,"build",impl->options.build);Put(d,"catalog",catalogHash);Put(d,"rules",rulesHash);impl->commands.push_back(Json(d));impl->link=LinkSnapshot{};impl->link.available=true;impl->link.state=LinkState::Preparing;impl->link.build=impl->options.build;impl->link.catalog=catalogHash;impl->link.rules=rulesHash;impl->linkWanted=true;impl->linkPreparePending=true;impl->ClearLinkLocked();return true;
}
bool Service::OpenLink(const std::string& requestId){
    std::lock_guard<std::mutex> guard(impl->mutex);if(impl->snapshot.state!=State::Ready||!impl->linkWanted||impl->commands.size()>=64||impl->snapshot.members.size()<2)return false;
    std::string id=requestId;if(id.empty()){if(impl->link.state!=LinkState::Prepared)return false;id=Uuid();}if(!IsUuid(id)||(impl->link.state!=LinkState::Prepared&&!(impl->link.state==LinkState::Opening&&id==impl->link.requestId)))return false;
    Doc d=Command("community.link.open");Put(d,"request_id",id);impl->commands.push_back(Json(d));impl->link.requestId=id;impl->link.state=LinkState::Opening;return true;
}
bool Service::CancelLink(){
    std::lock_guard<std::mutex> guard(impl->mutex);if(impl->link.state==LinkState::Idle)return false;impl->linkStop=true;impl->linkWanted=false;impl->ClearLinkLocked();impl->link.state=LinkState::Ended;impl->link.error="cancelled";
    if(impl->snapshot.state==State::Ready&&!impl->linkCancelPending){impl->requestLinkCancel=true;impl->linkCancelPending=true;}
    return true;
}
bool Service::SendLink(std::uint8_t destination,const std::vector<std::uint8_t>& payload){
    if(payload.empty()||payload.size()>8192)return false;
    std::lock_guard<std::mutex> guard(impl->mutex);
    if(impl->stop||impl->linkStop||impl->snapshot.state!=State::Ready||impl->link.state!=LinkState::Active||destination==impl->link.seat||(destination!=255&&destination>=impl->link.roster.size())||impl->linkOutgoing.size()>=128||payload.size()+2>262144-impl->linkOutgoingBytes)return false;
    Bytes bytes{1,destination};Append(bytes,payload);impl->linkOutgoingBytes+=bytes.size();impl->linkOutgoing.push_back(std::move(bytes));return true;
}
bool Service::ReceiveLink(LinkPacket& packet){std::lock_guard<std::mutex> guard(impl->mutex);if(impl->link.state!=LinkState::Active||impl->linkIncoming.empty())return false;packet=std::move(impl->linkIncoming.front());impl->linkIncoming.pop_front();impl->linkIncomingBytes-=packet.payload.size();return true;}
const char* StateName(State state){switch(state){case State::Stopped:return "Stopped";case State::Connecting:return "Connecting";case State::Authenticating:return "Authenticating";case State::Ready:return "Online";case State::Reconnecting:return "Reconnecting";case State::Stopping:return "Stopping";case State::Failed:return "Failed";}return "Unknown";}
const char* LinkStateName(LinkState state){switch(state){case LinkState::Idle:return "Idle";case LinkState::Preparing:return "Preparing";case LinkState::Prepared:return "Prepared";case LinkState::Opening:return "Opening";case LinkState::Connecting:return "Connecting";case LinkState::Authenticating:return "Authenticating";case LinkState::Waiting:return "Waiting";case LinkState::Active:return "Active";case LinkState::Ended:return "Ended";case LinkState::Failed:return "Failed";}return "Unknown";}
const char* CityStateName(CityState state){switch(state){case CityState::Absent:return "Outside plaza";case CityState::Entering:return "Joining plaza";case CityState::Present:return "In plaza";case CityState::Leaving:return "Leaving plaza";}return "Unknown";}
}
