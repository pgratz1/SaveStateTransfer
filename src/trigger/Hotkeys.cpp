#include "trigger/Hotkeys.h"

#include "app/Requests.h"
#include "core/Config.h"

namespace sst::trigger {
    namespace {
        class HotkeySink : public RE::BSTEventSink<RE::InputEvent*> {
        public:
            static HotkeySink* GetSingleton()
            {
                static HotkeySink singleton;
                return &singleton;
            }

            RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
            {
                if (!a_event) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                const auto& cfg = core::Config::Get();
                for (auto* e = *a_event; e; e = e->next) {
                    if (e->GetEventType() != RE::INPUT_EVENT_TYPE::kButton || e->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
                        continue;
                    }
                    const auto* button = e->AsButtonEvent();
                    if (!button) {
                        continue;
                    }
                    const auto code = button->GetIDCode();
                    if (cfg.modifierKey != 0 && code == cfg.modifierKey) {
                        _modifierHeld = button->IsPressed();
                        continue;
                    }
                    if (!button->IsDown()) {
                        continue;
                    }
                    std::optional<app::RequestKind> kind;
                    if (code == cfg.exportKey) {
                        kind = app::RequestKind::kExport;
                    } else if (code == cfg.importKey) {
                        kind = app::RequestKind::kImport;
                    } else if (code == cfg.dryRunKey) {
                        kind = app::RequestKind::kDryRun;
                    }
                    if (!kind) {
                        continue;
                    }
                    if (cfg.modifierKey != 0 && !_modifierHeld) {
                        logger::debug("hotkey 0x{:X} ignored: modifier 0x{:X} not held", code, cfg.modifierKey);
                        continue;
                    }
                    OnHotkey(*kind, cfg);
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            void OnHotkey(app::RequestKind a_kind, const core::Config& a_cfg)
            {
                const auto now = std::chrono::steady_clock::now();
                if (a_cfg.requireDoublePress) {
                    const bool confirmed = _pending && *_pending == a_kind &&
                                           (now - _pendingAt) <= std::chrono::milliseconds(a_cfg.doublePressWindowMs);
                    if (!confirmed) {
                        _pending = a_kind;
                        _pendingAt = now;
                        logger::info("{} hotkey pressed; press again within {} ms to confirm", app::ToString(a_kind), a_cfg.doublePressWindowMs);
                        RE::DebugNotification(std::format("SaveStateTransfer: press again to {}", app::ToString(a_kind)).c_str());
                        return;
                    }
                    _pending.reset();
                }
                // Input events arrive on the game thread, but go through the task queue anyway so all
                // triggers share one entry path.
                SKSE::GetTaskInterface()->AddTask([a_kind] { app::Dispatch(a_kind, "", "hotkey"); });
            }

            bool                                  _modifierHeld = false;
            std::optional<app::RequestKind>       _pending;
            std::chrono::steady_clock::time_point _pendingAt{};
        };
    }

    void InstallHotkeys()
    {
        auto* manager = RE::BSInputDeviceManager::GetSingleton();
        if (!manager) {
            logger::error("BSInputDeviceManager unavailable; hotkeys disabled");
            return;
        }
        manager->AddEventSink(HotkeySink::GetSingleton());
        logger::info("hotkeys installed");
    }
}
