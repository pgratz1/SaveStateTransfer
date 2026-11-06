#include "core/CrashProbe.h"

#include <Windows.h>

namespace sst::core::CrashProbe {
    namespace {
        std::mutex       g_mutex;
        std::string      g_step;
        std::atomic<int> g_loggedIdle{ 0 };  // exceptions with no import step active (other mods, often handled)
        std::atomic<int> g_loggedStep{ 0 };
        bool             g_installed = false;

        bool IsFatal(DWORD a_code)
        {
            switch (a_code) {
            case EXCEPTION_ACCESS_VIOLATION:
            case EXCEPTION_ILLEGAL_INSTRUCTION:
            case EXCEPTION_STACK_OVERFLOW:
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
            case EXCEPTION_PRIV_INSTRUCTION:
            case EXCEPTION_IN_PAGE_ERROR:
            case EXCEPTION_DATATYPE_MISALIGNMENT:
                return true;
            default:
                return false;
            }
        }

        // "module+0xRVA" for an address, or "" when it is not inside a loaded module.
        std::string Describe(std::uintptr_t a_addr)
        {
            HMODULE mod = nullptr;
            if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCSTR>(a_addr), &mod) || !mod) {
                return {};
            }
            char name[MAX_PATH] = "?";
            ::GetModuleFileNameA(mod, name, MAX_PATH);
            const auto base = reinterpret_cast<std::uintptr_t>(mod);
            return std::format("{}+0x{:X}", std::filesystem::path(name).filename().string(), a_addr - base);
        }

        bool Readable(const void* a_ptr, std::size_t a_size)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (!::VirtualQuery(a_ptr, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD) ||
                (mbi.Protect & PAGE_NOACCESS)) {
                return false;
            }
            const auto end = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            return reinterpret_cast<std::uintptr_t>(a_ptr) + a_size <= end;
        }

        LONG CALLBACK Handler(EXCEPTION_POINTERS* a_ep)
        {
            if (!a_ep || !a_ep->ExceptionRecord || !IsFatal(a_ep->ExceptionRecord->ExceptionCode)) {
                return EXCEPTION_CONTINUE_SEARCH;
            }
            std::string step;
            {
                std::lock_guard lock(g_mutex);
                step = g_step;
            }
            if ((step.empty() ? g_loggedIdle : g_loggedStep).fetch_add(1) >= 3) {
                return EXCEPTION_CONTINUE_SEARCH;
            }
            const auto& rec = *a_ep->ExceptionRecord;
            const auto  addr = reinterpret_cast<std::uintptr_t>(rec.ExceptionAddress);
            std::string detail;
            if (rec.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec.NumberParameters >= 2) {
                const char* kind = rec.ExceptionInformation[0] == 0 ? "read" : rec.ExceptionInformation[0] == 1 ? "write" : "execute";
                detail = std::format(" ({} of 0x{:X})", kind, rec.ExceptionInformation[1]);
            }
            auto where = Describe(addr);
            logger::critical("FATAL exception 0x{:08X} at 0x{:016X} {}{}; last step: {}", rec.ExceptionCode, addr,
                where.empty() ? "<no module>" : where, detail, step.empty() ? "<none>" : step);

            // Probable call stack: scan the faulting thread's stack for addresses inside loaded modules.
            if (a_ep->ContextRecord) {
                const auto* sp = reinterpret_cast<const std::uintptr_t*>(a_ep->ContextRecord->Rsp);
                int found = 0;
                for (int i = 0; i < 512 && found < 24; ++i) {
                    if (!Readable(sp + i, sizeof(std::uintptr_t))) {
                        break;
                    }
                    const auto v = sp[i];
                    if (v < 0x10000) {
                        continue;
                    }
                    auto d = Describe(v);
                    if (!d.empty()) {
                        logger::critical("  stack[{:3}] {}", i, d);
                        ++found;
                    }
                }
            }
            spdlog::default_logger()->flush();
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }

    void Install()
    {
        if (g_installed) {
            return;
        }
        g_installed = ::AddVectoredExceptionHandler(1, Handler) != nullptr;
        logger::info("crash probe {}", g_installed ? "installed" : "NOT installed");
    }

    void SetStep(std::string a_step)
    {
        std::lock_guard lock(g_mutex);
        g_step = std::move(a_step);
    }

    void ClearStep()
    {
        std::lock_guard lock(g_mutex);
        g_step.clear();
    }
}
