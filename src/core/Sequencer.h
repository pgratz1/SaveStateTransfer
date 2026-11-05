#pragma once

namespace sst::core {
    // Runs steps on the game thread, one per frame, with optional extra idle frames between steps so Papyrus
    // fragments triggered by a step get time to execute. Does not advance while the game is paused (menus,
    // headset off in VR), because the Papyrus VM is frozen then and actor/inventory changes are unsafe.
    class Sequencer {
    public:
        using Step = std::function<void()>;

        // a_label (optional) is logged before the step runs (when [Debug] bLogImportSteps) and handed to the
        // crash probe, so a crash log names the step that was executing.
        void Add(Step a_step, std::uint32_t a_framesAfter = 0, std::string a_label = {});
        void Start(std::string a_label);  // schedules the first step; the object must outlive the run
        bool IsRunning() const { return _running; }

    private:
        void Pump();

        struct Entry {
            Step          fn;
            std::uint32_t framesAfter;
            std::string   label;
        };
        std::deque<Entry> _steps;
        std::uint32_t     _wait = 0;
        bool              _running = false;
        bool              _pausedLogged = false;
        std::string       _label;
    };
}
