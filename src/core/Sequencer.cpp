#include "core/Sequencer.h"

#include "core/Config.h"
#include "core/CrashProbe.h"

namespace sst::core {
    namespace {
        constexpr auto kFrame = std::chrono::milliseconds(12);        // ~90 Hz VR frame
        constexpr auto kPausedPoll = std::chrono::milliseconds(250);  // re-check interval while the game is paused
    }

    void Sequencer::Add(Step a_step, std::uint32_t a_framesAfter, std::string a_label)
    {
        _state->steps.push_back({ std::move(a_step), a_framesAfter, std::move(a_label) });
    }

    void Sequencer::Start(std::string a_label)
    {
        if (_state->running.load()) {
            return;
        }
        _state->label = std::move(a_label);
        _state->running = true;
        logger::info("{}: {} steps queued", _state->label, _state->steps.size());
        std::thread(&Sequencer::Pacer, _state).detach();
    }

    void Sequencer::Pacer(std::shared_ptr<State> a_state)
    {
        while (a_state->running.load()) {
            a_state->stepDone = false;
            SKSE::GetTaskInterface()->AddTask([a_state] { RunOne(a_state); });
            while (!a_state->stepDone.load() && a_state->running.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (!a_state->running.load()) {
                break;
            }
            if (a_state->wasPaused.load()) {
                std::this_thread::sleep_for(kPausedPoll);
            } else {
                std::this_thread::sleep_for(kFrame * (1 + a_state->waitFrames.load()));
            }
        }
    }

    void Sequencer::RunOne(const std::shared_ptr<State>& a_state)
    {
        auto& s = *a_state;
        if (!s.running.load()) {
            s.stepDone = true;
            return;
        }
        auto* ui = RE::UI::GetSingleton();
        if (ui && ui->GameIsPaused()) {
            if (!s.pausedLogged) {
                logger::info("{}: game is paused, waiting (unpause / put the headset on to continue)", s.label);
                s.pausedLogged = true;
            }
            s.wasPaused = true;
            s.stepDone = true;
            return;
        }
        s.pausedLogged = false;
        s.wasPaused = false;
        if (s.steps.empty()) {
            s.running = false;
            CrashProbe::ClearStep();
            logger::info("{}: done", s.label);
            s.stepDone = true;
            return;
        }
        auto entry = std::move(s.steps.front());
        s.steps.pop_front();
        if (!entry.label.empty()) {
            CrashProbe::SetStep(s.label + ": " + entry.label);
            if (Config::Get().logImportSteps) {
                logger::info("{}: {}", s.label, entry.label);
            }
        }
        try {
            entry.fn();
        } catch (const std::exception& e) {
            logger::error("{}: step '{}' threw: {}", s.label, entry.label, e.what());
        }
        s.waitFrames = entry.framesAfter;
        s.stepDone = true;
    }
}
