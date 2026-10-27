#pragma once

namespace sst::core {
    // Minimal, dependency-free trace used only during plugin load, before spdlog exists. Appends to
    // Data/SKSE/Plugins/SaveStateTransfer.boot.log (via MO2 this lands in overwrite/). Compiled out of
    // release builds later; for now it is the tool for diagnosing load failures.
    void BootTrace(const char* a_step);
}
