#pragma once

namespace sst::core {
    // Runs steps on the game thread, one at a time, with a pause between them so Papyrus fragments triggered by a
    // step get time to execute. Does not advance while the game is paused (menus, headset off in VR), because the
    // Papyrus VM is frozen then and actor/inventory changes are unsafe.
    //
    // Pacing is driven by a background thread that posts one step at a time through SKSE's task interface and
    // sleeps in between. A task must never re-queue itself: SKSE drains its queue until empty within one frame, so
    // a self-re-adding task spins the main thread forever (this froze the game while waiting for unpause).
    class Sequencer {
    public:
        using Step = std::function<void()>;

        // a_label (optional) is logged before the step runs (when [Debug] bLogImportSteps) and handed to the
        // crash probe, so a crash log names the step that was executing.
        void Add(Step a_step, std::uint32_t a_framesAfter = 0, std::string a_label = {});
        void Start(std::string a_label);  // all steps must be added before this
        bool IsRunning() const { return _state->running.load(); }

    private:
        struct Entry {
            Step          fn;
            std::uint32_t framesAfter;
            std::string   label;
        };
        struct State {
            std::deque<Entry>          steps;
            std::string                label;
            std::atomic<bool>          running{ false };
            std::atomic<bool>          stepDone{ false };
            std::atomic<bool>          wasPaused{ false };
            std::atomic<std::uint32_t> waitFrames{ 0 };
            bool                       pausedLogged = false;
        };
        static void RunOne(const std::shared_ptr<State>& a_state);  // game thread
        static void Pacer(std::shared_ptr<State> a_state);          // background thread

        std::shared_ptr<State> _state = std::make_shared<State>();
    };
}
