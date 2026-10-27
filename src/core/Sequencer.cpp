#include "core/Sequencer.h"

namespace sst::core {
    void Sequencer::Add(Step a_step, std::uint32_t a_framesAfter)
    {
        _steps.push_back({ std::move(a_step), a_framesAfter });
    }

    void Sequencer::Start(std::string a_label)
    {
        if (_running) {
            return;
        }
        _label = std::move(a_label);
        _running = true;
        logger::info("{}: {} steps queued", _label, _steps.size());
        SKSE::GetTaskInterface()->AddTask([this] { Pump(); });
    }

    void Sequencer::Pump()
    {
        if (!_running) {
            return;
        }
        auto* ui = RE::UI::GetSingleton();
        if (ui && ui->GameIsPaused()) {
            if (!_pausedLogged) {
                logger::info("{}: game is paused, waiting (unpause / put the headset on to continue)", _label);
                _pausedLogged = true;
            }
            SKSE::GetTaskInterface()->AddTask([this] { Pump(); });
            return;
        }
        _pausedLogged = false;
        if (_wait > 0) {
            --_wait;
            SKSE::GetTaskInterface()->AddTask([this] { Pump(); });
            return;
        }
        if (_steps.empty()) {
            _running = false;
            logger::info("{}: done", _label);
            return;
        }
        auto entry = std::move(_steps.front());
        _steps.pop_front();
        try {
            entry.fn();
        } catch (const std::exception& e) {
            logger::error("{}: step threw: {}", _label, e.what());
        }
        _wait = entry.framesAfter;
        SKSE::GetTaskInterface()->AddTask([this] { Pump(); });
    }
}
