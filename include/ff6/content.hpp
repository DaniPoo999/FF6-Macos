#pragma once
#include "ff6/sha256.hpp"
#include <string_view>

namespace ff6 {
inline constexpr std::size_t rom_size = 3145728;
inline constexpr std::string_view rom_hash =
    "10eccc5d2fab81346dd759f6be478dcb682eef981e8d3d662da176e1f9a996bc";
struct ResourceSpec {
    std::string_view id;
    std::string_view family;
    std::uint32_t offset;
    std::uint32_t size;
    std::string_view digest;
    // Empty for a direct ROM span; otherwise offset addresses reviewed decoded
    // data in a named container. Container bytes never become package resources.
    std::string_view decoded_source{};
    // Reviewed semantic data derived from ROM reads; executable source bytes
    // used as visual noise are never copied into the output package.
    std::string_view transform{};
};
std::span<const ResourceSpec> inventory();
std::string_view inventory_digest();
struct RomImage {
    Bytes bytes;
    bool removed_copier_header;
};
RomImage validate_rom(Bytes input);
struct Resource {
    std::string id;
    Bytes bytes;
};
struct ContentPack {
    Digest source_hash{};
    Digest recipe_hash{};
    std::vector<Resource> resources;
    ByteView get(std::string_view id) const;
};
ContentPack extract(const RomImage& rom);
Bytes derive_airship_smoke_parameters(ByteView rom);
Bytes encode_pack(const ContentPack& pack);
ContentPack decode_pack(ByteView bytes);
void verify_canonical(const ContentPack& pack);
std::string manifest_json(const ContentPack& pack);
// Decoded-resource hashes for independent reference comparisons; no payload text.
std::string audit_json(const ContentPack& pack);
} // namespace ff6
