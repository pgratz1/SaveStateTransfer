#pragma once

namespace sst::trigger {
    // Polls <root>/commands/ for export.cmd, import.cmd, dryrun.cmd. The file's first line (optional)
    // is passed as the request argument (e.g. an export file name). The file is deleted once picked up.
    // Lets a Linux shell drive the plugin while the headset is off.
    void StartTriggerFileWatcher();
    void StopTriggerFileWatcher();
}
