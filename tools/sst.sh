#!/bin/bash
# Drive the running game from Linux: tools/sst.sh export|import|dryrun [export-name]  |  tools/sst.sh log|exports|report
set -euo pipefail
ROOT="/home/pgratz/ExtraSpace/SteamLibrary/steamapps/compatdata/611670/pfx/drive_c/users/steamuser/Documents/My Games/Skyrim VR"
SST="$ROOT/SaveStateTransfer"
case "${1:-}" in
  export|import|dryrun)
      mkdir -p "$SST/commands"; printf '%s\n' "${2:-}" > "$SST/commands/$1.cmd"
      echo "queued $1 ${2:-} (picked up within ~1 s while a save is loaded)";;
  log)      tail -n "${2:-40}" "$ROOT/SKSE/SaveStateTransfer.log";;
  exports)  ls -lt "$SST/exports" 2>/dev/null;;
  report)   f=$(ls -t "$SST/reports"/*.json 2>/dev/null | head -1); echo "$f"; python3 -I -m json.tool "$f" | head -n "${2:-80}";;
  *) echo "usage: $0 export|import|dryrun [name] | log [n] | exports | report [n]" >&2; exit 1;;
esac
