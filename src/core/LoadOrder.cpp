#include "core/LoadOrder.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace sst::core {
    namespace {
        // Layout of RE::TESFileCollection as returned by skyrimvresl's GetCompiledFileCollectionExtern().
        struct FileCollection {
            RE::BSTArray<RE::TESFile*> files;
            RE::BSTArray<RE::TESFile*> smallFiles;
        };
        static_assert(sizeof(FileCollection) == 0x30);

        std::vector<LoadOrder::Entry> g_entries;
        bool g_initialized = false;
        bool g_vresl = false;

        const FileCollection* GetVRCollection()
        {
            const auto handle = ::GetModuleHandleA("skyrimvresl");
            if (!handle) {
                return nullptr;
            }
            using Fn = const FileCollection* (*)();
            const auto fn = reinterpret_cast<Fn>(::GetProcAddress(handle, "GetCompiledFileCollectionExtern"));
            return fn ? fn() : nullptr;
        }

        bool IEquals(std::string_view a, std::string_view b)
        {
            return a.size() == b.size() &&
                   std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
        }
    }

    void LoadOrder::Init()
    {
        g_entries.clear();
        g_vresl = false;
        auto* dh = RE::TESDataHandler::GetSingleton();
        if (!dh) {
            logger::error("LoadOrder: TESDataHandler unavailable");
            return;
        }

        auto add = [](RE::TESFile* a_file, bool a_light) {
            if (!a_file) {
                return;
            }
            Entry e;
            e.file = a_file;
            e.name = std::string(a_file->GetFilename());
            e.light = a_light;
            e.index = a_light ? a_file->GetSmallFileCompileIndex() : a_file->GetCompileIndex();
            g_entries.push_back(std::move(e));
        };

        if (REL::Module::IsVR()) {
            if (const auto* vr = GetVRCollection()) {
                g_vresl = true;
                for (auto* f : vr->files) {
                    add(f, false);
                }
                for (auto* f : vr->smallFiles) {
                    add(f, true);
                }
            } else {
                // Plain VR engine: every active plugin is a full plugin.
                const auto count = dh->GetLoadedModCount();
                auto** files = dh->GetLoadedMods();
                for (std::size_t i = 0; i < count; ++i) {
                    add(files[i], false);
                }
            }
        } else {
            const auto fullCount = dh->GetLoadedModCount();
            auto** full = dh->GetLoadedMods();
            for (std::size_t i = 0; i < fullCount; ++i) {
                add(full[i], false);
            }
            const auto lightCount = dh->GetLoadedLightModCount();
            auto** light = dh->GetLoadedLightMods();
            for (std::size_t i = 0; i < lightCount; ++i) {
                add(light[i], true);
            }
        }
        g_initialized = true;
        logger::info("LoadOrder: {} full + {} light plugins{}", FullCount(), LightCount(), g_vresl ? " (via skyrimvresl)" : "");
    }

    bool LoadOrder::IsInitialized() { return g_initialized; }
    bool LoadOrder::HasVRESL() { return g_vresl; }
    std::size_t LoadOrder::FullCount() { return std::count_if(g_entries.begin(), g_entries.end(), [](const Entry& e) { return !e.light; }); }
    std::size_t LoadOrder::LightCount() { return std::count_if(g_entries.begin(), g_entries.end(), [](const Entry& e) { return e.light; }); }
    const std::vector<LoadOrder::Entry>& LoadOrder::Entries() { return g_entries; }

    const LoadOrder::Entry* LoadOrder::FindByName(std::string_view a_name)
    {
        for (const auto& e : g_entries) {
            if (IEquals(e.name, a_name)) {
                return &e;
            }
        }
        return nullptr;
    }

    const LoadOrder::Entry* LoadOrder::OwnerOf(RE::FormID a_formID)
    {
        const auto hi = a_formID >> 24;
        if (hi == 0xFF) {
            return nullptr;
        }
        if (hi == 0xFE) {
            const auto small = (a_formID >> 12) & 0xFFF;
            for (const auto& e : g_entries) {
                if (e.light && e.index == small) {
                    return &e;
                }
            }
            return nullptr;
        }
        for (const auto& e : g_entries) {
            if (!e.light && e.index == hi) {
                return &e;
            }
        }
        return nullptr;
    }

    std::vector<std::string> LoadOrder::Names()
    {
        std::vector<std::string> out;
        for (const auto& e : g_entries) {
            if (!e.light) {
                out.push_back(e.name);
            }
        }
        for (const auto& e : g_entries) {
            if (e.light) {
                out.push_back(e.name);
            }
        }
        return out;
    }
}
