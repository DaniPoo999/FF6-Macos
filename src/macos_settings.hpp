#pragma once
#include <memory>

namespace presentation {
class GamePresenter;
// Owns the native application-menu action and its reusable Settings window.
// Destroy before the presenter and SDL's application menu.
class SettingsWindow {
  public:
    virtual ~SettingsWindow() = default;
};
std::unique_ptr<SettingsWindow> make_settings_window(GamePresenter& presenter);
} // namespace presentation
