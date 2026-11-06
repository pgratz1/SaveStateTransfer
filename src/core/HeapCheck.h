#pragma once

namespace sst::core::HeapCheck {
    // Logs the game allocator singleton and allocates a batch of small blocks through RE::calloc, checking they are
    // distinct, non-overlapping and still zero after some string-pool activity. Diagnostic for VR, where a bad
    // MemoryManager mapping hands out memory the engine still owns. Blocks are intentionally leaked.
    void Run();
}
