#include "core/BootTrace.h"

#include <cstdio>

namespace sst::core {
    void BootTrace(const char* a_step)
    {
        if (FILE* f = std::fopen("Data/SKSE/Plugins/SaveStateTransfer.boot.log", "a")) {
            std::fprintf(f, "%s\n", a_step);
            std::fclose(f);
        }
    }
}
