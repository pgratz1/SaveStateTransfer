#pragma once

// Minimal crash localisation: Crash Logger VR does not load under this Proton setup, so on a fatal exception we
// log the faulting module+offset, a probable call stack (module-resident return addresses found on the stack)
// and the last import step that was announced with SetStep(). The exception is then passed on untouched.
namespace sst::core::CrashProbe {
    void Install();
    void SetStep(std::string a_step);
    void ClearStep();
}
