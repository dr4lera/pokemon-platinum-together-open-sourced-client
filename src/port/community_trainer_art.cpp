// SPDX-License-Identifier: GPL-3.0-or-later
#include "port/community_trainer_art.h"
#include <algorithm>
#include <cstring>
#include <new>

namespace CommunityTrainerArt {
namespace {
std::uint16_t U16(const std::uint8_t* p){return std::uint16_t(p[0])|(std::uint16_t(p[1])<<8);}
std::uint32_t U32(const std::uint8_t* p){return std::uint32_t(U16(p))|(std::uint32_t(U16(p+2))<<16);}
bool Nonempty(const std::uint8_t* p,std::size_t n){for(std::size_t i=0;i<n;i++)if(p[i])return true;return false;}
bool Indices(const std::uint8_t* p,std::size_t n,unsigned count){for(std::size_t i=0;i<n;i++)if(p[i]>=count)return false;return true;}
bool Rgba(const std::uint8_t* p,std::size_t n){for(std::size_t i=0;i<n;i+=4)if(p[i+3]!=255&&(p[i+3]!=0||p[i]||p[i+1]||p[i+2]))return false;return true;}
Result Fail(Error error){Result r;r.error=error;return r;}
void Expand(const Pack& pack,const std::uint8_t* indices,RgbaFrame& output){for(unsigned i=0;i<FieldPixels;i++)std::memcpy(output.data()+i*4,pack.palette[indices[i]].data(),4);}
constexpr std::size_t TileBytes[7]={4096,4096,4096,4096,4096,3200,3072};
}
bool Pack::FieldFrame(unsigned index,RgbaFrame& output) const noexcept
{
    output.fill(0);
    if(index>=FieldFrames||!Nonempty(field.data()+index*FieldPixels,FieldPixels))return false;
    Expand(*this,field.data()+index*FieldPixels,output);return true;
}
bool Pack::PoseFrame(std::uint32_t crc,RgbaFrame& output) const noexcept
{
    output.fill(0);
    const auto found=std::lower_bound(poses.begin(),poses.end(),crc,[](const Pose& pose,std::uint32_t key){return pose.crc<key;});
    if(found==poses.end()||found->crc!=crc)return false;
    Expand(*this,found->indices.data(),output);return true;
}
bool Pack::Tile(unsigned index,TileView& output) const noexcept
{
    output={};if(index>=7)return false;
    std::size_t offset=0;for(unsigned i=0;i<index;i++)offset+=TileBytes[i]+32;
    output.pixels=tiles.data()+offset;output.bytes=TileBytes[index];
    for(unsigned i=0;i<16;i++)output.palette[i]=U16(output.pixels+output.bytes+i*2);
    return true;
}
Result Decode(const std::uint8_t* bytes,std::size_t length) noexcept
{
    if(!bytes||length<64||length>MaxBytes)return Fail(Error::Size);
    const unsigned version=U16(bytes+4),count=U16(bytes+14),templateLength=bytes[13];
    if(std::memcmp(bytes,"PTTR",4)||(version!=1&&version!=2)||U32(bytes+8)!=length||bytes[12]>1
        ||(version==1?U16(bytes+6)!=0:bytes[6]>2||bytes[7]!=0))return Fail(Error::Header);
    constexpr char Template[]="platinum-trainer-209-v1";
    if(templateLength!=sizeof(Template)-1||std::memcmp(bytes+16,Template,sizeof(Template)-1))return Fail(Error::Template);
    for(unsigned i=16+templateLength;i<64;i++)if(bytes[i])return Fail(Error::Template);
    if(count<1||count>6||64+count*12>length)return Fail(Error::Sections);
    struct Section {const std::uint8_t* data=nullptr;std::size_t size=0;};
    Section sections[6];constexpr char Tags[6][5]={"PAL ","FELD","POSE","FRNT","BIG ","TILE"};
    std::size_t end=64+count*12;int previous=-1;
    for(unsigned i=0;i<count;i++){
        const auto entry=bytes+64+i*12;int tag=-1;
        for(int j=0;j<6;j++)if(!std::memcmp(entry,Tags[j],4)){tag=j;break;}
        const std::size_t offset=U32(entry+4),size=U32(entry+8);
        if(tag<=previous||offset!=end||!size||offset>length||size>length-offset)return Fail(Error::Sections);
        previous=tag;sections[tag]={bytes+offset,size};end=offset+size;
    }
    if(end!=length||!sections[0].data||!sections[1].data||!sections[3].data||!sections[5].data)return Fail(Error::Sections);
    const auto pal=sections[0];
    if(pal.size<4)return Fail(Error::Palette);
    const unsigned colors=U16(pal.data);
    if(colors<2||colors>256||U16(pal.data+2)||pal.size!=(colors+1)*4)return Fail(Error::Palette);
    if(Nonempty(pal.data+4,4))return Fail(Error::Palette);
    for(unsigned i=1;i<colors;i++)if(pal.data[4+i*4+3]!=255)return Fail(Error::Palette);
    if(sections[1].size!=FieldFrames*FieldPixels||sections[3].size!=6400*4
        ||(sections[4].data&&sections[4].size!=8192*4))return Fail(Error::Sections);
    if(!Indices(sections[1].data,sections[1].size,colors)||!Rgba(sections[3].data,sections[3].size)
        ||(sections[4].data&&!Rgba(sections[4].data,sections[4].size)))return Fail(Error::Pixels);
    const unsigned fallback=version==2?bytes[6]:0;
    if(!fallback){
        for(unsigned i=0;i<FieldFrames;i++)if(!Nonempty(sections[1].data+i*FieldPixels,FieldPixels))return Fail(Error::Empty);
        if(!Nonempty(sections[3].data,sections[3].size))return Fail(Error::Empty);
    }
    if(sections[4].data&&!Nonempty(sections[4].data,sections[4].size))return Fail(Error::Empty);
    unsigned poses=0;
    if(sections[2].data){
        const auto pose=sections[2];if(pose.size<4)return Fail(Error::Poses);poses=U16(pose.data);
        if(!poses||poses>256||U16(pose.data+2)||pose.size!=4+poses*(4+FieldPixels))return Fail(Error::Poses);
        std::uint32_t previousCrc=0;
        for(unsigned i=0;i<poses;i++){
            const auto entry=pose.data+4+i*(4+FieldPixels);const auto crc=U32(entry);
            if((i&&crc<=previousCrc)||!Nonempty(entry+4,FieldPixels))return Fail(Error::Poses);
            if(!Indices(entry+4,FieldPixels,colors))return Fail(Error::Pixels);
            previousCrc=crc;
        }
    }
    if(sections[5].size!=26976)return Fail(Error::Tiles);
    std::size_t offset=0;
    for(unsigned i=0;i<7;i++){
        offset+=TileBytes[i];for(unsigned j=0;j<16;j++)if(U16(sections[5].data+offset+j*2)&0x8000)return Fail(Error::Tiles);offset+=32;
    }
    try{
        Result result;result.pack=std::make_unique<Pack>();auto& pack=*result.pack;
        pack.version=static_cast<std::uint16_t>(version);pack.paletteCount=static_cast<std::uint16_t>(colors);pack.female=bytes[12]!=0;pack.fallback=static_cast<std::uint8_t>(fallback);
        for(unsigned i=0;i<colors;i++)std::memcpy(pack.palette[i].data(),pal.data+4+i*4,4);
        std::memcpy(pack.field.data(),sections[1].data,pack.field.size());std::memcpy(pack.front.data(),sections[3].data,pack.front.size());
        pack.poses.resize(poses);for(unsigned i=0;i<poses;i++){const auto entry=sections[2].data+4+i*(4+FieldPixels);pack.poses[i].crc=U32(entry);std::memcpy(pack.poses[i].indices.data(),entry+4,FieldPixels);}
        if(sections[4].data)pack.big.assign(sections[4].data,sections[4].data+sections[4].size);
        std::memcpy(pack.tiles.data(),sections[5].data,pack.tiles.size());return result;
    }catch(...){return Fail(Error::Allocation);}
}
const char* ErrorName(Error error) noexcept
{
    switch(error){case Error::None:return "none";case Error::Size:return "size";case Error::Header:return "header";case Error::Template:return "template";case Error::Sections:return "sections";case Error::Palette:return "palette";case Error::Pixels:return "pixels";case Error::Empty:return "empty";case Error::Poses:return "poses";case Error::Tiles:return "tiles";case Error::Allocation:return "allocation";}return "unknown";
}
}
