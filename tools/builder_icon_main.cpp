#include "builder_icon.hpp"
#include "ff6/io.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc != 2 && argc != 3)
            throw std::runtime_error("Usage: ff6-builder-icon-generator OUTPUT.icns [ARTWORK.png]");
        const std::filesystem::path output(argv[1]);
        const auto bytes = argc == 3 ? ff6::make_builder_icon(ff6::read_file(argv[2]))
                                     : ff6::make_builder_icon();
        // Developer packaging output only; no ROM or content package is read.
        if (!output.parent_path().empty())
            std::filesystem::create_directories(output.parent_path());
        std::filesystem::remove(output);
        ff6::write_new_file(output, bytes);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
