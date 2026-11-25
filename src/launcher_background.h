// A picture behind a game's launcher menu (launcher_background.cpp says how it is drawn).

#pragma once

#include <span>

namespace recompui {
    class LauncherMenu;
}

namespace mp_launcher_background {
    // Puts the picture, a PNG's bytes, behind the launcher's menu, covering the window, and outlines the menu's text
    // so it reads on it. A game's on_launcher_init calls it, with its own picture.
    void set(recompui::LauncherMenu* menu, std::span<const char> png);
}
