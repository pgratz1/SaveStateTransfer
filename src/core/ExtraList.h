#pragma once

namespace sst::core {
    // CommonLibSSE-NG's RE::ExtraDataList carries no data members in a multi-runtime build, so sizeof() is 1 and
    // RE::calloc<RE::ExtraDataList>(1) hands the engine a one-byte block (it then uses 0x18 bytes of someone else's
    // heap: that froze the first imports). The engine layout is { BSExtraData* data; PresenceBitfield* presence;
    // BSReadWriteLock lock } = 0x18 on SE / VR / AE before 1.6.629, and 0x20 with a vtable on AE 1.6.629+.
    inline RE::ExtraDataList* NewExtraDataList()
    {
        if (REL::Module::IsAE() && REL::Module::get().version().compare(SKSE::RUNTIME_SSE_1_6_629) != std::strong_ordering::less) {
            return nullptr;  // needs the engine vtable; not supported yet
        }
        return static_cast<RE::ExtraDataList*>(RE::calloc(1, 0x18));
    }
}
