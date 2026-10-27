#include "Version.h"
#include "core/BootTrace.h"
#include "app/Requests.h"
#include "core/Config.h"
#include "core/FormKey.h"
#include "core/LoadOrder.h"
#include "core/Paths.h"
#include "trigger/Hotkeys.h"
#include "trigger/TriggerFile.h"

namespace {
    void InitLogging()
    {
        using sst::core::BootTrace;
        BootTrace("  Module::get");
        const auto& module = REL::Module::get();
        BootTrace("  Module::version");
        const auto ver = module.version();
        BootTrace((std::string("  version ") + ver.string(".")).c_str());
        BootTrace("  Module::IsVR");
        const bool vr = REL::Module::IsVR();
        BootTrace(vr ? "  runtime VR" : "  runtime not VR");
        BootTrace("  log_directory");
        auto path = logger::log_directory();
        if (!path) {
            BootTrace("  log_directory failed");
            SKSE::stl::report_and_fail("SaveStateTransfer: SKSE log directory unavailable"sv);
        }
        *path /= "SaveStateTransfer.log"sv;
        BootTrace((std::string("  log path ") + path->string()).c_str());
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        BootTrace("  sink created");
        auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v"s);
        BootTrace("  logger ready");
    }

    // M0 acceptance check: the load order is visible and forms from full and light plugins round-trip.
    void LogLoadOrderDiagnostics()
    {
        sst::core::LoadOrder::Init();
        if (!sst::core::LoadOrder::IsInitialized()) {
            return;
        }
        using sst::core::FormKey;
        auto test = [](const char* a_label, const RE::TESForm* a_form) {
            if (!a_form) {
                logger::warn("{}: no form", a_label);
                return;
            }
            const auto key = FormKey::From(a_form);
            const auto* back = key.Resolve();
            logger::info("{}: 0x{:08X} -> {} -> {}", a_label, a_form->GetFormID(), key.ToString(),
                back == a_form ? "OK" : (back ? "WRONG FORM" : "UNRESOLVED"));
        };
        test("full-plugin round trip", RE::TESForm::LookupByID(0x13746));  // NordRace, Skyrim.esm

        auto [map, lock] = RE::TESForm::GetAllForms();
        const RE::BSReadLockGuard guard{ lock };
        if (!map) {
            return;
        }
        std::size_t light = 0, lastFull = 0;
        for (const auto& [id, form] : *map) {
            if (!form) {
                continue;
            }
            const auto hi = id >> 24;
            if (hi == 0xFE && light < 3) {
                test("light-plugin round trip", form);
                ++light;
            } else if (hi != 0xFE && hi != 0xFF && hi >= 0x80 && lastFull < 2) {
                test("late full-plugin round trip", form);
                ++lastFull;
            }
            if (light >= 3 && lastFull >= 2) {
                break;
            }
        }
        if (light == 0) {
            logger::warn("no light-plugin (0xFE) forms present");
        }
    }

    void OnMessage(SKSE::MessagingInterface::Message* a_msg)
    {
        switch (a_msg->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            LogLoadOrderDiagnostics();
            sst::trigger::InstallHotkeys();
            sst::trigger::StartTriggerFileWatcher();
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            logger::info("save game loaded");
            break;
        default:
            break;
        }
    }
}

SKSEPluginInfo(
    .Version = REL::Version{ SST_VERSION_MAJOR, SST_VERSION_MINOR, SST_VERSION_PATCH, 0 },
    .Name = "SaveStateTransfer"sv,
    .Author = "Paul Gratz"sv,
    .StructCompatibility = SKSE::StructCompatibility::Independent,
    .RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary)

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    using sst::core::BootTrace;
    BootTrace("--- SKSEPlugin_Load");
    try {
        BootTrace("init logging");
        InitLogging();
        BootTrace("query runtime");
        const auto& module = REL::Module::get();
        const bool isVR = REL::Module::IsVR();
        logger::info("SaveStateTransfer v{} loading; runtime {} ({})", SST_VERSION_STRING, module.version().string("."),
            isVR ? "VR" : (REL::Module::IsAE() ? "AE" : "SE"));

        BootTrace("SKSE::Init");
        SKSE::Init(a_skse);

        BootTrace("paths");
        if (!sst::core::Paths::Init()) {
            logger::error("could not determine data directory");
            return false;
        }
        logger::info("data root: {}", sst::core::Paths::Root().string());
        BootTrace("config");
        sst::core::Config::Load();

        BootTrace("messaging");
        SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
        logger::info("loaded");
        BootTrace("load OK");
        return true;
    } catch (const std::exception& e) {
        BootTrace((std::string("EXCEPTION: ") + e.what()).c_str());
        return false;
    } catch (...) {
        BootTrace("EXCEPTION: unknown");
        return false;
    }
}
