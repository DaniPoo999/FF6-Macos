#pragma once
#include "ff6/bytes.hpp"
#include <stdexcept>
#include <string>
#include <string_view>

namespace ff6 {
struct MaterialSource {
    std::string list;
    std::string first;
    std::string macros;
};

// Developer-only source formatting. Literal macros preserve the expression
// shape seen by the compiler without committing any ROM-derived operands.
// These are data definitions, never translated instructions or control flow.
inline MaterialSource render_material_source(std::string_view id, ByteView bytes,
                                             unsigned width, bool signed_values) {
    if (id.empty() || id.size() > 80 || id.front() < 'a' || id.front() > 'z')
        throw std::runtime_error("Invalid material source identity");
    std::string macro_id;
    for (const char ch : id) {
        if (ch >= 'a' && ch <= 'z') macro_id += static_cast<char>(ch - 'a' + 'A');
        else if ((ch >= '0' && ch <= '9') || ch == '_') macro_id += ch;
        else throw std::runtime_error("Invalid material source identity");
    }
    if ((width != 1 && width != 4) || bytes.empty() || bytes.size() % width ||
        (signed_values && width != 4))
        throw std::runtime_error("Invalid material source extent/type");
    constexpr auto notice = "// Private ROM-derived values. Never commit or distribute this input.\n";
    MaterialSource result{notice, {}, notice};
    for (std::size_t at = 0; at < bytes.size(); at += width) {
        const auto value = width == 4 ? read32(bytes, at) : bytes[at];
        const auto literal = signed_values && value >= 0x80000000U
            ? std::to_string(std::int64_t{value} - 0x100000000LL) : std::to_string(value);
        if (at) result.list += ',';
        else result.first = std::string(notice) + literal + '\n';
        result.list += literal;
        result.macros += "#define FF6_ROM_VALUE_" + macro_id + '_' +
                         std::to_string(at / width) + ' ' + literal + '\n';
    }
    result.list += '\n';
    return result;
}
}
