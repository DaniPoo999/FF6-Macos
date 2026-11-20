#pragma once
#include "ff6/content.hpp"
#include "ff6/raster.hpp"

namespace ff6 {
// Prepare the opening's indexed landscape from extracted data. Colors are the
// target snow palettes; the future presentation owner applies its actual fades
// and row transforms. Rows start disabled: this is not a complete credits view.
AffineLayer load_opening_landscape(const ContentPack& pack);
} // namespace ff6
