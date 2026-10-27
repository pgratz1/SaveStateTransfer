#pragma once

namespace sst::app {
    // Writes exports/<name>.json (+ reports/<name>.report.json). Empty name = "<player>_<timestamp>".
    // Game thread only. Returns the export path on success.
    std::optional<std::filesystem::path> RunExport(std::string a_name);
}
