// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef COMMUNITY_TRAINER_ART_H
#define COMMUNITY_TRAINER_ART_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace CommunityTrainerArt {
constexpr std::size_t MaxBytes=2*1024*1024;
constexpr unsigned FieldFrames=36,FieldSide=32,FieldPixels=1024;
using RgbaFrame=std::array<std::uint8_t,FieldPixels*4>;
struct Pose {std::uint32_t crc=0;std::array<std::uint8_t,FieldPixels> indices{};};
struct TileView {const std::uint8_t* pixels=nullptr;std::size_t bytes=0;std::array<std::uint16_t,16> palette{};};
struct Pack {
    std::uint16_t version=0,paletteCount=0;
    bool female=false;
    std::uint8_t fallback=0; // Original v2 fallback selector; never a Together look/outfit ID.
    std::array<std::array<std::uint8_t,4>,256> palette{};
    std::array<std::uint8_t,FieldFrames*FieldPixels> field{};
    std::vector<Pose> poses; // Strictly increasing native pose CRC32 keys.
    std::array<std::uint8_t,6400*4> front{};
    std::vector<std::uint8_t> big; // Optional flat 8192 RGBA pixels; no inferred dimensions.
    std::array<std::uint8_t,26976> tiles{};
    bool FieldFrame(unsigned index,RgbaFrame& output) const noexcept;
    bool PoseFrame(std::uint32_t crc,RgbaFrame& output) const noexcept;
    bool Tile(unsigned index,TileView& output) const noexcept; // Borrowed until this Pack dies.
};
enum class Error {None,Size,Header,Template,Sections,Palette,Pixels,Empty,Poses,Tiles,Allocation};
struct Result {
    Error error=Error::None;
    std::unique_ptr<Pack> pack;
    explicit operator bool() const noexcept{return error==Error::None&&pack!=nullptr;}
};
// Copies validated data into one owned pack. No URLs, files, network, native IDs,
// graphics managers, native pointer-bearing blobs, or global caches are touched.
Result Decode(const std::uint8_t* bytes,std::size_t length) noexcept;
const char* ErrorName(Error error) noexcept;
}
#endif
