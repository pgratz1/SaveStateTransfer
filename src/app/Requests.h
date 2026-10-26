#pragma once

namespace sst::app {
    enum class RequestKind { kExport, kImport, kDryRun };

    const char* ToString(RequestKind a_kind);

    // Entry point for every trigger (hotkey, command file, console). Must be called on the game
    // thread (use SKSE::GetTaskInterface()->AddTask from other threads). M0: logs only.
    void Dispatch(RequestKind a_kind, std::string a_argument, std::string_view a_source);
}
