// SPDX-License-Identifier: GPL-3.0-or-later
#include "port/community_trainer_art.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
using namespace CommunityTrainerArt;
using Bytes=std::vector<std::uint8_t>;
static void W16(Bytes& b,std::size_t at,unsigned value){b[at]=value&255;b[at+1]=(value>>8)&255;}
static void W32(Bytes& b,std::size_t at,std::uint32_t value){W16(b,at,value&65535);W16(b,at+2,value>>16);}
static std::size_t U32(const Bytes& b,std::size_t at){return std::size_t(b[at])|(std::size_t(b[at+1])<<8)|(std::size_t(b[at+2])<<16)|(std::size_t(b[at+3])<<24);}
static Bytes Fixture(unsigned colors=3,unsigned poses=2,bool big=true)
{
    // Original synthetic pixels only. These are complete semantic sections,
    // deliberately unlike the backend's four-byte structural-only fixtures.
    Bytes pal((colors+1)*4);W16(pal,0,colors);
    for(unsigned i=1;i<colors;i++){pal[4+i*4]=i;pal[5+i*4]=i^0xa5;pal[6+i*4]=i^0x5a;pal[7+i*4]=255;}
    Bytes field(FieldFrames*FieldPixels);for(unsigned i=0;i<field.size();i++)field[i]=1+i%(colors-1);
    Bytes pose(4+poses*(4+FieldPixels));W16(pose,0,poses);
    for(unsigned i=0;i<poses;i++){W32(pose,4+i*(4+FieldPixels),i*100+9);std::memset(pose.data()+8+i*(4+FieldPixels),colors-1,FieldPixels);}
    Bytes front(6400*4),large(8192*4),tiles(26976);
    for(std::size_t i=0;i<front.size();i+=4){front[i]=23;front[i+1]=45;front[i+2]=67;front[i+3]=255;}
    for(std::size_t i=0;i<large.size();i+=4){large[i]=78;large[i+1]=90;large[i+2]=12;large[i+3]=255;}
    std::size_t off=0;for(unsigned i=0;i<7;i++){const unsigned n=i<5?4096:i==5?3200:3072;std::memset(tiles.data()+off,i+1,n);off+=n;for(unsigned j=0;j<16;j++)W16(tiles,off+j*2,j*100+i);off+=32;}assert(off==tiles.size());
    struct Section{const char* tag;Bytes bytes;};std::vector<Section> sections={{"PAL ",pal},{"FELD",field}};
    if(poses)sections.push_back({"POSE",pose});
    sections.push_back({"FRNT",front});
    if(big)sections.push_back({"BIG ",large});
    sections.push_back({"TILE",tiles});
    Bytes pack(64+sections.size()*12);std::memcpy(pack.data(),"PTTR",4);W16(pack,4,1);pack[13]=23;W16(pack,14,sections.size());std::memcpy(pack.data()+16,"platinum-trainer-209-v1",23);
    for(unsigned i=0;i<sections.size();i++){const auto& s=sections[i];std::memcpy(pack.data()+64+i*12,s.tag,4);W32(pack,68+i*12,pack.size());W32(pack,72+i*12,s.bytes.size());pack.insert(pack.end(),s.bytes.begin(),s.bytes.end());}
    W32(pack,8,pack.size());return pack;
}
static std::size_t Section(const Bytes& pack,const char* tag){for(unsigned i=0;i<pack[14];i++)if(!std::memcmp(pack.data()+64+i*12,tag,4))return U32(pack,68+i*12);assert(false);return 0;}
int main()
{
    auto input=Fixture();auto result=Decode(input.data(),input.size());assert(result&&result.pack->paletteCount==3&&result.pack->poses.size()==2&&result.pack->big.size()==32768);
    assert(result.pack->version==1&&!result.pack->female&&!result.pack->fallback);
    struct Guard{std::uint64_t before=0xabcdef1234567890ULL;RgbaFrame frame{};std::uint64_t after=0x1234567890abcdefULL;} guarded;
    for(unsigned i=0;i<FieldFrames;i++){assert(result.pack->FieldFrame(i,guarded.frame));assert(!std::memcmp(guarded.frame.data(),result.pack->palette[1].data(),4));}
    assert(!result.pack->FieldFrame(36,guarded.frame));for(auto b:guarded.frame)assert(!b);
    assert(result.pack->PoseFrame(9,guarded.frame));assert(!std::memcmp(guarded.frame.data(),result.pack->palette[2].data(),4));assert(!result.pack->PoseFrame(10,guarded.frame));
    assert(guarded.before==0xabcdef1234567890ULL&&guarded.after==0x1234567890abcdefULL);
    for(unsigned i=0;i<7;i++){TileView tile;assert(result.pack->Tile(i,tile));assert(tile.bytes==(i<5?4096:i==5?3200:3072));assert(tile.pixels[0]==i+1&&tile.palette[15]==1500+i);}
    TileView tile;assert(!result.pack->Tile(7,tile)&&!tile.pixels&&!tile.bytes);
    std::fill(input.begin(),input.end(),0);assert(result.pack->FieldFrame(0,guarded.frame)&&guarded.frame[3]==255); // Owned, not dangling input views.
    input=Fixture(256,256,false);result=Decode(input.data(),input.size());assert(result&&result.pack->paletteCount==256&&result.pack->poses.size()==256&&result.pack->big.empty());assert(result.pack->PoseFrame(25509,guarded.frame)&&guarded.frame[0]==255);
    input=Fixture(2,0,false);assert(Decode(input.data(),input.size()));
    auto reject=[&](Bytes bad,Error error=Error::None){auto r=Decode(bad.data(),bad.size());assert(!r&&!r.pack);if(error!=Error::None)assert(r.error==error);};
    const auto source=Fixture();
    auto mutate=[&](auto change,Error error=Error::None){auto b=source;change(b);reject(std::move(b),error);};
    for(std::size_t n=0;n<64;n++)assert(!Decode(source.data(),n));
    assert(!Decode(nullptr,64));Bytes huge(MaxBytes+1);reject(std::move(huge),Error::Size);
    mutate([](Bytes& b){b[0]='X';},Error::Header);mutate([](Bytes& b){W16(b,4,3);},Error::Header);
    mutate([](Bytes& b){b[6]=1;},Error::Header);mutate([](Bytes& b){b[12]=2;},Error::Header);mutate([](Bytes& b){W32(b,8,0xffffffff);},Error::Header);
    mutate([](Bytes& b){b[13]=48;},Error::Template);mutate([](Bytes& b){b[16]='X';},Error::Template);mutate([](Bytes& b){b[63]=1;},Error::Template);
    mutate([](Bytes& b){W16(b,14,7);},Error::Sections);mutate([](Bytes& b){b[64]='X';},Error::Sections);mutate([](Bytes& b){std::memcpy(b.data()+76,"PAL ",4);},Error::Sections);
    mutate([](Bytes& b){W32(b,68,0xffffffff);},Error::Sections);mutate([](Bytes& b){W32(b,72,0xffffffff);},Error::Sections);mutate([](Bytes& b){b.push_back(0);W32(b,8,b.size());},Error::Sections);
    mutate([](Bytes& b){W16(b,Section(b,"PAL "),1);},Error::Palette);mutate([](Bytes& b){b[Section(b,"PAL ")+2]=1;},Error::Palette);
    mutate([](Bytes& b){b[Section(b,"PAL ")+4]=1;},Error::Palette);mutate([](Bytes& b){b[Section(b,"PAL ")+11]=254;},Error::Palette);
    mutate([](Bytes& b){b[Section(b,"FELD")]=3;},Error::Pixels);mutate([](Bytes& b){std::memset(b.data()+Section(b,"FELD"),0,1024);},Error::Empty);
    mutate([](Bytes& b){b[Section(b,"FRNT")+3]=0;},Error::Pixels);mutate([](Bytes& b){b[Section(b,"FRNT")+3]=128;},Error::Pixels);
    mutate([](Bytes& b){std::memset(b.data()+Section(b,"FRNT"),0,25600);},Error::Empty);mutate([](Bytes& b){std::memset(b.data()+Section(b,"BIG "),0,32768);},Error::Empty);
    mutate([](Bytes& b){W16(b,Section(b,"POSE"),257);},Error::Poses);mutate([](Bytes& b){b[Section(b,"POSE")+2]=1;},Error::Poses);
    mutate([](Bytes& b){W32(b,Section(b,"POSE")+4+1028,9);},Error::Poses);mutate([](Bytes& b){std::memset(b.data()+Section(b,"POSE")+8,0,1024);},Error::Poses);
    mutate([](Bytes& b){b[Section(b,"POSE")+8]=3;},Error::Pixels);mutate([](Bytes& b){b[Section(b,"TILE")+4096+1]|=128;},Error::Tiles);
    for(unsigned mode=0;mode<3;mode++){
        auto b=source;W16(b,4,2);b[6]=mode;b[12]=1;auto r=Decode(b.data(),b.size());assert(r&&r.pack->fallback==mode&&r.pack->female);
        std::memset(b.data()+Section(b,"FELD"),0,36*1024);std::memset(b.data()+Section(b,"FRNT"),0,25600);r=Decode(b.data(),b.size());assert(static_cast<bool>(r)==(mode!=0));
        if(r)assert(!r.pack->FieldFrame(0,guarded.frame));
    }
    mutate([](Bytes& b){W16(b,4,2);b[6]=3;},Error::Header);mutate([](Bytes& b){W16(b,4,2);b[7]=1;},Error::Header);
    std::mt19937 random(12345);for(unsigned i=0;i<1000;i++){auto b=source;for(unsigned j=0;j<4;j++)b[random()%b.size()]=random()&255;auto r=Decode(b.data(),b.size());if(r){r.pack->FieldFrame(random()%40,guarded.frame);r.pack->PoseFrame(random(),guarded.frame);}}
    std::puts("PTTR semantic decoder: full original synthetic packs, palette/frames/CRC poses/RGBA/tiles, sparse v2, bounds and mutations PASS");
}
