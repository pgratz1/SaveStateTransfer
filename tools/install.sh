#!/bin/bash
# Install the staged mod folder into the MO2 "FUS" instance and enable it in a profile.
# Usage: tools/install.sh [stage-dir] [profile-name]
#   stage-dir default: build/linux-xwin-release/stage (or a directory downloaded from CI)
#   profile default:   "FUS RO DAH (Basic + Appearance + Gameplay)"
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="${1:-$REPO/build/linux-xwin-release/stage}"
PROFILE="${2:-FUS RO DAH (Basic + Appearance + Gameplay)}"
MO2="/home/pgratz/ExtraSpace/FUS"
MOD="SaveStateTransfer"
DEST="$MO2/mods/$MOD"

[ -f "$STAGE/SKSE/Plugins/$MOD.dll" ] || { echo "no $MOD.dll under $STAGE" >&2; exit 1; }
if pgrep -fa "ModOrganizer.exe|SkyrimVR.exe|sksevr_loader.exe" >/dev/null; then
    echo "MO2 or the game is running; close them first (MO2 rewrites modlist.txt on exit)." >&2
    exit 2
fi

mkdir -p "$DEST"
rsync -a --delete --exclude meta.ini "$STAGE/" "$DEST/"
[ -f "$DEST/meta.ini" ] || cat > "$DEST/meta.ini" <<META
[General]
modid=0
version=$(grep -oP 'project\(SaveStateTransfer VERSION \K[0-9.]+' "$REPO/CMakeLists.txt")
installationFile=
comments=Built from $REPO
META

LIST="$MO2/profiles/$PROFILE/modlist.txt"
[ -f "$LIST" ] || { echo "profile not found: $LIST" >&2; exit 1; }
if grep -qx "[+-]$MOD" "$LIST"; then
    sed -i "s/^-$MOD\$/+$MOD/" "$LIST"
else
    # modlist.txt is highest priority first, line 1 is MO2's header comment.
    sed -i "1a +$MOD" "$LIST"
fi
echo "installed $(ls "$DEST/SKSE/Plugins" | tr '\n' ' ')-> $DEST; enabled in profile '$PROFILE'"
