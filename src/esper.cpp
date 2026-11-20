#include "ff6/esper.hpp"
namespace ff6 {
EsperData load_espers(const ContentPack& pack) {
    const auto bytes = pack.get("data_d86e00_genju_prop");
    if (bytes.size() != 27 * 11) throw std::invalid_argument("Invalid Esper properties");
    EsperData result;
    for (unsigned i = 0; i < result.size(); ++i) {
        auto& esper = result[i];
        for (unsigned j = 0; j < 5; ++j) {
            esper.rates[j] = bytes[i * 11 + j * 2];
            esper.spells[j] = bytes[i * 11 + j * 2 + 1];
            if (esper.spells[j] >= 54 && esper.spells[j] != 255)
                throw std::invalid_argument("Invalid Esper teaching spell");
        }
        esper.bonus = bytes[i * 11 + 10];
        if (esper.bonus > 16 && esper.bonus != 255)
            throw std::invalid_argument("Invalid Esper level bonus");
    }
    return result;
}
} // namespace ff6
