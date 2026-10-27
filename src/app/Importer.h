#pragma once

namespace sst::app {
    // Reads exports/<name>.json (empty name = newest export) and applies it to the current game.
    // Dry run only resolves every form and writes the predicted missing[] report. Game thread only.
    bool RunImport(std::string a_name, bool a_dryRun);
}
