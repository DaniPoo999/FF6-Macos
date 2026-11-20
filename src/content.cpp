#include "ff6/content.hpp"
#include "ff6/codecs.hpp"
#include <algorithm>
#include <array>
#include <set>
#include <sstream>

namespace ff6 {
namespace {
struct DecodedSourceSpec {
    std::string_view id;
    std::uint32_t offset, size, decoded_size;
    std::string_view digest, decoded_digest;
};
#include "inventory.inc"
constexpr std::array<std::uint8_t, 8> magic{'F', 'F', '6', 'P', 'A', 'C', 'K', '1'};
constexpr std::size_t pack_limit = 16U * 1024U * 1024U;
bool valid_id(std::string_view id) {
    return !id.empty() && id.size() <= 160 && std::ranges::all_of(id, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}
class Reader {
  public:
    explicit Reader(ByteView bytes) : bytes_(bytes) {}
    ByteView take(std::size_t size) {
        auto part = slice(bytes_, position_, size);
        position_ += size;
        return part;
    }
    std::uint16_t u16() {
        return read16(take(2), 0);
    }
    std::uint32_t u32() {
        return read32(take(4), 0);
    }
    Digest digest() {
        Digest value{};
        auto raw = take(32);
        std::ranges::copy(raw, value.begin());
        return value;
    }
    bool finished() const {
        return position_ == bytes_.size();
    }

  private:
    ByteView bytes_;
    std::size_t position_ = 0;
};
} // namespace
std::span<const ResourceSpec> inventory() {
    return resource_specs;
}
std::string_view inventory_digest() {
    return inventory_hash;
}
RomImage validate_rom(Bytes input) {
    bool header = false;
    if (input.size() == rom_size + 512) {
        input.erase(input.begin(), input.begin() + 512);
        header = true;
    }
    if (input.size() != rom_size)
        throw std::runtime_error(
            "Expected a 3 MiB USA Rev 1 ROM, optionally with a 512-byte copier header");
    if (hex(sha256(input)) != rom_hash)
        throw std::runtime_error(
            "Unsupported or modified ROM: normalized SHA-256 does not match USA Rev 1");
    return {std::move(input), header};
}
ByteView ContentPack::get(std::string_view id) const {
    const auto found = std::ranges::find(resources, id, &Resource::id);
    if (found == resources.end())
        throw std::runtime_error("Missing content resource: " + std::string(id));
    return found->bytes;
}
Bytes derive_airship_smoke_parameters(ByteView rom) {
    // EE/5213..534D samples unrelated ROM bytes as deterministic visual noise.
    // Keep only the resulting jitter, emission, lifetime and drift quantities.
    // Cover the entire wrapping word index, without shipping machine code.
    Bytes result;
    result.reserve(32768 * 6);
    for (unsigned index = 0; index < 65536; index += 2) {
        result.push_back(slice(rom, 0x50000 + index, 1)[0] & 3U);
        result.push_back(slice(rom, 0x50001 + index, 1)[0] & 3U);
        result.push_back(slice(rom, 0x20000 + index, 1)[0] >= 32 ? 1 : 0);
        result.push_back(static_cast<std::uint8_t>((slice(rom, 0x2e0000 + index, 1)[0] & 3U) + 1));
        const unsigned sample = read16(rom, 0x10000 + index) & 511U;
        append16(result, static_cast<std::uint16_t>((256U - sample) & 65535U));
    }
    return result;
}
ContentPack extract(const RomImage& rom) {
    // Recheck at the public library boundary, including callers other than CLI.
    if (rom.bytes.size() != rom_size || hex(sha256(rom.bytes)) != rom_hash)
        throw std::runtime_error("Unvalidated ROM supplied to extractor");
    ContentPack pack{parse_digest(rom_hash), parse_digest(inventory_hash), {}};
    // Decode mixed containers only inside extraction. Select reviewed data
    // spans; the surrounding executable and unresolved bytes are discarded.
    std::vector<Bytes> decoded;
    for (const auto& source : decoded_sources) {
        const auto encoded = slice(rom.bytes, source.offset, source.size);
        if (hex(sha256(encoded)) != source.digest)
            throw std::runtime_error("Decoded-source input digest mismatch");
        auto bytes = decompress_lzss(encoded, source.decoded_size);
        if (bytes.size() != source.decoded_size || hex(sha256(bytes)) != source.decoded_digest)
            throw std::runtime_error("Decoded-source output mismatch");
        decoded.push_back(std::move(bytes));
    }
    for (const auto& spec : resource_specs) {
        ByteView source = rom.bytes;
        Bytes transformed;
        if (!spec.transform.empty()) {
            if (spec.transform != "airship_smoke_v1")
                throw std::runtime_error("Unknown content transform");
            transformed = derive_airship_smoke_parameters(rom.bytes);
            source = transformed;
        }
        if (!spec.decoded_source.empty()) {
            const auto found =
                std::ranges::find(decoded_sources, spec.decoded_source, &DecodedSourceSpec::id);
            if (found == decoded_sources.end())
                throw std::runtime_error("Unknown decoded resource source");
            source = decoded[static_cast<std::size_t>(found - decoded_sources.begin())];
        }
        const auto data = slice(source, spec.offset, spec.size);
        if (hex(sha256(data)) != spec.digest)
            throw std::runtime_error("Inventory digest mismatch: " + std::string(spec.id));
        pack.resources.push_back({std::string(spec.id), Bytes(data.begin(), data.end())});
    }
    return pack;
}
Bytes encode_pack(const ContentPack& pack) {
    if (pack.resources.empty() || pack.resources.size() > 1024)
        throw std::runtime_error("Invalid resource count");
    Bytes out;
    append(out, magic);
    append32(out, 1);
    append(out, pack.source_hash);
    append(out, pack.recipe_hash);
    append32(out, static_cast<std::uint32_t>(pack.resources.size()));
    std::set<std::string> seen;
    for (const auto& resource : pack.resources) {
        if (!valid_id(resource.id) || !seen.insert(resource.id).second)
            throw std::runtime_error("Invalid/duplicate resource identity");
        if (resource.bytes.empty() || resource.bytes.size() > rom_size)
            throw std::runtime_error("Invalid resource size");
        append16(out, static_cast<std::uint16_t>(resource.id.size()));
        append32(out, static_cast<std::uint32_t>(resource.bytes.size()));
        append(out, sha256(resource.bytes));
        for (char c : resource.id)
            out.push_back(static_cast<std::uint8_t>(c));
        append(out, resource.bytes);
        if (out.size() > pack_limit)
            throw std::runtime_error("Content package exceeds size limit");
    }
    return out;
}
ContentPack decode_pack(ByteView bytes) {
    if (bytes.size() > pack_limit)
        throw std::runtime_error("Content package exceeds size limit");
    Reader reader(bytes);
    if (!std::ranges::equal(reader.take(magic.size()), magic) || reader.u32() != 1)
        throw std::runtime_error("Unsupported content package format");
    ContentPack pack{reader.digest(), reader.digest(), {}};
    const auto count = reader.u32();
    if (count == 0 || count > 1024)
        throw std::runtime_error("Invalid resource count");
    std::set<std::string> seen;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto id_size = reader.u16();
        const auto size = reader.u32();
        const auto digest = reader.digest();
        if (id_size == 0 || id_size > 160 || size == 0 || size > rom_size)
            throw std::runtime_error("Invalid resource bounds");
        const auto id_bytes = reader.take(id_size);
        const std::string id(id_bytes.begin(), id_bytes.end());
        if (!valid_id(id) || !seen.insert(id).second)
            throw std::runtime_error("Invalid/duplicate resource identity");
        const auto data = reader.take(size);
        if (sha256(data) != digest)
            throw std::runtime_error("Corrupt resource: " + id);
        pack.resources.push_back({id, Bytes(data.begin(), data.end())});
    }
    if (!reader.finished())
        throw std::runtime_error("Trailing data after content package");
    return pack;
}
void verify_canonical(const ContentPack& pack) {
    if (pack.source_hash != parse_digest(rom_hash) ||
        pack.recipe_hash != parse_digest(inventory_hash))
        throw std::runtime_error("Wrong ROM profile or extraction recipe");
    if (pack.resources.size() != std::size(resource_specs))
        throw std::runtime_error("Incomplete resource inventory");
    for (std::size_t i = 0; i < pack.resources.size(); ++i) {
        const auto& spec = resource_specs[i];
        const auto& resource = pack.resources[i];
        if (resource.id != spec.id || resource.bytes.size() != spec.size ||
            hex(sha256(resource.bytes)) != spec.digest)
            throw std::runtime_error("Noncanonical resource: " + std::string(spec.id));
    }
}
std::string manifest_json(const ContentPack& pack) {
    verify_canonical(pack);
    std::ostringstream out;
    out << "{\n  \"schema\": 1,\n  \"extractor_version\": \"" << FF6_EXTRACTOR_VERSION
        << "\",\n  \"stage\": \"encoded-data; mixed-region inventory "
           "incomplete\",\n  \"rom_sha256\": \""
        << hex(pack.source_hash) << "\",\n  \"inventory_sha256\": \"" << hex(pack.recipe_hash)
        << "\",\n  \"resources\": [\n";
    for (std::size_t i = 0; i < pack.resources.size(); ++i) {
        const auto& resource = pack.resources[i];
        out << "    {\"id\": \"" << resource.id << "\", \"size\": " << resource.bytes.size()
            << ", \"sha256\": \"" << hex(sha256(resource.bytes)) << "\"}"
            << (i + 1 == pack.resources.size() ? "\n" : ",\n");
    }
    out << "  ]\n}\n";
    return out.str();
}
} // namespace ff6
