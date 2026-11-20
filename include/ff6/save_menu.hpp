#pragma once
#include "ff6/menu_canvas.hpp"
#include "ff6/saved_game.hpp"
namespace ff6 {
enum class SaveMenuMode { save, load };
enum class SaveMenuPhase { slots, confirm, request, leaving, complete };
struct SaveMenuChoice {
    bool new_game = false;
    unsigned slot = 0;
};
class SaveMenu {
  public:
    SaveMenu(const ContentPack& pack, SaveMenuMode mode, std::array<SaveSlot, 3> slots,
             MenuWindowAppearance appearance, InputSettings settings, Buttons held = 0);
    std::vector<std::uint8_t> advance(Buttons buttons);
    RasterScene scene() const;
    SaveMenuPhase phase() const {
        return phase_;
    }
    SaveMenuMode mode() const {
        return mode_;
    }
    const std::optional<SaveMenuChoice>& choice() const {
        return choice_;
    }
    const SavedGame& selected_game() const {
        return slots_.at(choice_.value().slot).game.value();
    }
    const std::string& notice() const {
        return notice_;
    }
    void finish_request(std::string error = {});

  private:
    MenuLayoutAssets assets_;
    SaveMenuMode mode_;
    std::array<SaveSlot, 3> slots_;
    MenuWindowAppearance appearance_;
    InputSettings settings_;
    InputState input_;
    SaveMenuPhase phase_ = SaveMenuPhase::slots;
    std::optional<SaveMenuChoice> choice_;
    std::string notice_;
    unsigned cursor_ = 0, answer_ = 0, invalid_ = 0;
    std::uint8_t intensity_ = 0;
    std::array<std::array<Tile, 6>, 22> actors_{};
    std::array<std::array<Rgb555, 16>, 23> actor_colors_{};
    std::array<Rgb555, 16> gray_{};
};
} // namespace ff6
