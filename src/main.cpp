#include "Version.h"
#include "core/BootTrace.h"
#include "app/Requests.h"
#include "core/Config.h"
#include "core/FormKey.h"
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

    // M0 acceptance check: the load order is visible and ESL-flagged plugins round-trip through FormKey.
    void LogLoadOrderDiagnostics()
    {
        auto* dh = RE::TESDataHandler::GetSingleton();
        if (!dh) {
            logger::error("TESDataHandler unavailable");
            return;
        }
        logger::info("data loaded: {} full plugins, {} light plugins", dh->GetLoadedModCount(), dh->GetLoadedLightModCount());

        // Full-plugin round trip.
        if (auto* nord = dh->LookupForm(0x13746, "Skyrim.esm"sv)) {
            const auto key = sst::core::FormKey::From(nord);
            logger::info("round trip Skyrim.esm|0x13746 -> {} -> {}", key.ToString(), key.Resolve() == nord ? "OK" : "MISMATCH");
        } else {
            logger::error("LookupForm(Skyrim.esm, 0x13746) failed");
        }

        // Light-plugin round trip: first form owned by an ESL-flagged file.
        auto [map, lock] = RE::TESForm::GetAllForms();
        const RE::BSReadLockGuard guard{ lock };
        if (!map) {
            return;
        }
        std::size_t checked = 0;
        for (const auto& [id, form] : *map) {
            if (!form || form->IsDynamicForm()) {
                continue;
            }
            const auto* file = form->GetFile(0);
            if (!file || !file->IsLight()) {
                continue;
            }
            const auto key = sst::core::FormKey::From(form);
            const auto* back = key.Resolve();
            logger::info("ESL round trip 0x{:08X} -> {} -> {}", id, key.ToString(), back == form ? "OK" : (back ? "WRONG FORM" : "UNRESOLVED"));
            if (++checked >= 3) {
                break;
            }
        }
        if (checked == 0) {
            logger::warn("no light-plugin forms found to test");
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
