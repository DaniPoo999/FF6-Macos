#include "ff6/rom_material.hpp"
#include "ff6/io.hpp"
#include "rom_input_source.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
void generated_file(const std::filesystem::path& file, ff6::ByteView bytes) {
    if (std::filesystem::exists(file) && ff6::read_file(file) == ff6::Bytes(bytes.begin(), bytes.end()))
        return;
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Cannot create private generated input");
    for (auto byte : bytes) stream.put(static_cast<char>(byte));
    stream.close();
    if (!stream) throw std::runtime_error("Cannot finish private generated input");
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("Usage: ff6-rom-inputs ROM PRIVATE_OUTPUT_DIRECTORY");
        const auto rom = ff6::validate_rom(ff6::read_file(argv[1], ff6::rom_size + 512));
        const auto values = ff6::extract_rom_materials(rom);
        const std::filesystem::path folder(argv[2]);
        std::filesystem::create_directories(folder);
        const auto specs = ff6::rom_material_specs();
        for (std::size_t i = 0; i < specs.size(); ++i) {
            const std::string name(specs[i].id);
            const auto text = ff6::render_material_source(name, values[i], specs[i].native_width,
                                                          specs[i].transform == "signed_values");
            generated_file(folder / (name + ".inc"), ff6::Bytes(text.list.begin(), text.list.end()));
            generated_file(folder / (name + ".first.inc"), ff6::Bytes(text.first.begin(), text.first.end()));
            generated_file(folder / (name + ".macros.inc"), ff6::Bytes(text.macros.begin(), text.macros.end()));
            generated_file(folder / (name + ".bin"), values[i]);
            if (specs[i].transform == "attribute_rectangle_words" ||
                    specs[i].transform == "unit_acceleration_motion" ||
                    specs[i].transform == "pairwise_unsigned_differences") {
                // Preserve the original native expression shape using private
                // operand macros. The independently derived pattern still owns
                // removal/restoration of the compiler's folded data copies.
                const auto source = ff6::widen_rom_material(
                    ff6::read_rom_material_source(rom, specs[i]), specs[i].source_width);
                const auto raw = ff6::render_material_source(name + "_source", source,
                                                             4, false);
                generated_file(folder / (name + ".source.macros.inc"),
                               ff6::Bytes(raw.macros.begin(), raw.macros.end()));
            }
        }
        std::cout << "Generated " << values.size() << " private ROM inputs\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
