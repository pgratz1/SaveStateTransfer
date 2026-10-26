#include "trigger/TriggerFile.h"

#include "app/Requests.h"
#include "core/Config.h"
#include "core/Paths.h"

namespace sst::trigger {
    namespace {
        std::atomic_bool g_running{ false };
        std::thread      g_thread;

        struct CommandFile {
            const char*      name;
            app::RequestKind kind;
        };
        constexpr CommandFile kCommands[] = {
            { "export.cmd", app::RequestKind::kExport },
            { "import.cmd", app::RequestKind::kImport },
            { "dryrun.cmd", app::RequestKind::kDryRun },
        };

        std::string ReadFirstLine(const std::filesystem::path& a_path)
        {
            std::ifstream in(a_path);
            std::string line;
            std::getline(in, line);
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
                line.pop_back();
            }
            return line;
        }

        void Poll()
        {
            const auto dir = core::Paths::Commands();
            std::error_code ec;
            for (const auto& cmd : kCommands) {
                const auto path = dir / cmd.name;
                if (!std::filesystem::exists(path, ec)) {
                    continue;
                }
                auto argument = ReadFirstLine(path);
                std::filesystem::remove(path, ec);
                if (ec) {
                    logger::warn("could not remove {}: {}", path.string(), ec.message());
                }
                const auto kind = cmd.kind;
                SKSE::GetTaskInterface()->AddTask([kind, argument = std::move(argument)] {
                    app::Dispatch(kind, argument, "command file");
                });
            }
        }
    }

    void StartTriggerFileWatcher()
    {
        const auto& cfg = core::Config::Get();
        if (!cfg.enableTriggerFiles || g_running.exchange(true)) {
            return;
        }
        // Consume stale command files left over from a previous session.
        std::error_code ec;
        for (const auto& cmd : kCommands) {
            std::filesystem::remove(core::Paths::Commands() / cmd.name, ec);
        }
        const auto period = std::chrono::milliseconds(std::max<std::uint32_t>(cfg.triggerPollMs, 200));
        g_thread = std::thread([period] {
            while (g_running.load()) {
                Poll();
                std::this_thread::sleep_for(period);
            }
        });
        g_thread.detach();
        logger::info("command-file watcher polling {} every {} ms", core::Paths::Commands().string(), period.count());
    }

    void StopTriggerFileWatcher()
    {
        g_running.store(false);
    }
}
