#include "core/HeapCheck.h"

namespace sst::core::HeapCheck {
    void Run()
    {
        auto* mm = RE::MemoryManager::GetSingleton();
        logger::info("heapcheck: MemoryManager singleton = {}", static_cast<const void*>(mm));
        if (!mm) {
            return;
        }
        constexpr std::size_t kCount = 64;
        constexpr std::size_t kSize = sizeof(RE::ExtraDataList);
        std::vector<std::byte*> blocks;
        for (std::size_t i = 0; i < kCount; ++i) {
            auto* p = static_cast<std::byte*>(RE::calloc(1, kSize));
            if (!p) {
                logger::error("heapcheck: allocation {} returned null", i);
                return;
            }
            blocks.push_back(p);
        }
        std::sort(blocks.begin(), blocks.end());
        int overlaps = 0, misaligned = 0;
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            if (reinterpret_cast<std::uintptr_t>(blocks[i]) % 8 != 0) {
                ++misaligned;
            }
            if (i > 0 && blocks[i] < blocks[i - 1] + kSize) {
                ++overlaps;
            }
        }
        // Churn the string pool, then verify our blocks are untouched.
        std::vector<RE::BSFixedString> strings;
        for (int i = 0; i < 200; ++i) {
            strings.emplace_back(std::format("sst_heapcheck_{}_{}", i, reinterpret_cast<std::uintptr_t>(blocks[0])));
        }
        int dirty = 0;
        for (auto* b : blocks) {
            for (std::size_t k = 0; k < kSize; ++k) {
                if (b[k] != std::byte{ 0 }) {
                    ++dirty;
                    break;
                }
            }
        }
        logger::info("heapcheck: {} blocks of {} bytes: first={} last={} overlaps={} misaligned={} dirtyAfterStringChurn={}", kCount, kSize,
            static_cast<const void*>(blocks.front()), static_cast<const void*>(blocks.back()), overlaps, misaligned, dirty);
        if (overlaps || dirty) {
            logger::error("heapcheck: GAME ALLOCATOR IS NOT SAFE on this runtime; refusing to create extra data");
        }

        // Replay of the step that froze the import: fresh ExtraDataList, then extra-data objects, then Add.
        auto dump = [](const char* a_label, const void* a_list) {
            const auto* q = static_cast<const std::uint64_t*>(a_list);
            logger::info("heapcheck: {} list@{} = {{ data=0x{:X} presence=0x{:X} lock=0x{:X} }}", a_label, a_list, q[0], q[1], q[2]);
        };
        logger::info("heapcheck: sizeof ExtraDataList={} ExtraTextDisplayData={} ExtraHealth={} ExtraCharge={} ExtraHotkey={}",
            sizeof(RE::ExtraDataList), sizeof(RE::ExtraTextDisplayData), sizeof(RE::ExtraHealth), sizeof(RE::ExtraCharge), sizeof(RE::ExtraHotkey));
        auto* list = RE::calloc<RE::ExtraDataList>(1);
        dump("after calloc", list);
        auto* health = new RE::ExtraHealth(1.5f);
        logger::info("heapcheck: ExtraHealth @{} next={} type={}", static_cast<const void*>(health), static_cast<const void*>(health->next),
            static_cast<int>(health->GetType()));
        dump("after new ExtraHealth", list);
        auto* text = new RE::ExtraTextDisplayData("sst heapcheck custom name");
        logger::info("heapcheck: ExtraTextDisplayData @{} next={} type={} name='{}'", static_cast<const void*>(text),
            static_cast<const void*>(text->next), static_cast<int>(text->GetType()), text->displayName.c_str());
        dump("after new ExtraTextDisplayData", list);
        const auto* q = reinterpret_cast<const std::uint64_t*>(list);
        if (q[0] != 0 || q[1] != 0) {
            logger::error("heapcheck: list was modified before Add; NOT calling Add (would loop)");
            return;
        }
        list->Add(health);
        dump("after Add(health)", list);
        if (q[0] != reinterpret_cast<std::uint64_t>(health) || health->next != nullptr) {
            logger::error("heapcheck: Add(health) produced an unexpected chain; stopping");
            return;
        }
        list->Add(text);
        dump("after Add(text)", list);
        logger::info("heapcheck: chain health->next={} text->next={}", static_cast<const void*>(health->next), static_cast<const void*>(text->next));
        logger::info("heapcheck: replay OK");
    }
}
