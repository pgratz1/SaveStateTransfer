#pragma once

namespace sst::trigger {
    // Keyboard hotkeys (DirectInput scan codes from the INI). Optional modifier key and a
    // confirm-by-second-press window so controller/keyboard noise cannot fire an import in VR.
    void InstallHotkeys();
}
