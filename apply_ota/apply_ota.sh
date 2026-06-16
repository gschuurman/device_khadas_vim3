#!/system/bin/sh
#
# apply_ota - apply a LineageOS A/B (seamless) OTA zip via update_engine,
# with no recovery and no UI interaction.
#
# Run it AS ROOT (`adb root` first - on a userdebug build the su domain works
# under both permissive and enforcing SELinux):
#     adb root
#     adb push lineage_vim3-ota.zip /data/local/tmp/
#     adb shell apply_ota /data/local/tmp/lineage_vim3-ota.zip --reboot
#
# Why root: update_engine's binder service and the pushed package in
# /data/local/tmp are reachable only by core/allowlisted SELinux domains, and
# that allowlist cannot be extended from the device tree, so a non-root shell
# cannot apply an OTA under ENFORCING. (Under permissive it happens to work
# without root, but don't rely on that once you switch to enforcing.) The
# sanctioned no-root path is the LineageOS Updater app (org.lineageos.updater).
#
# Usage:
#     apply_ota [/path/to/ota.zip] [--reboot]
#
# How it works: payload.bin + payload_properties.txt are extracted to
# /data/ota_package and handed to update_engine, which writes the *inactive* A/B
# slot. The active slot is untouched until the next reboot, so a bad update rolls
# back automatically.
#
# NOTE: needs root (update_engine binder access - provided either by `adb root`
# or by the init service) and roughly the zip's own size free on /data for the
# extracted payload. It is intentionally simple and is NOT fault tolerant - do
# not interrupt it while it is applying.

WORK=/data/ota_package
TAG=apply_ota

say()    { echo "$*";        log -t "$TAG" "$*" 2>/dev/null; }
sayerr() { echo "$*" >&2;    log -p e -t "$TAG" "$*" 2>/dev/null; }

ZIP=""
DO_REBOOT=0
for arg in "$@"; do
    case "$arg" in
        --reboot) DO_REBOOT=1 ;;
        -h|--help)
            echo "Usage: apply_ota [/path/to/ota.zip] [--reboot]"
            exit 0 ;;
        -*)
            sayerr "Unknown option: $arg"
            sayerr "Usage: apply_ota [/path/to/ota.zip] [--reboot]"
            exit 2 ;;
        *) ZIP="$arg" ;;
    esac
done

# Locate an OTA zip if none was given.
if [ -z "$ZIP" ]; then
    for d in /data/local/tmp /sdcard/Download /data/media/0/Download "$WORK"; do
        cand=$(ls -t "$d"/*.zip 2>/dev/null | head -n 1)
        if [ -n "$cand" ]; then ZIP="$cand"; break; fi
    done
fi

if [ -z "$ZIP" ] || [ ! -f "$ZIP" ]; then
    sayerr "ERROR: no OTA zip found."
    sayerr "Usage: apply_ota /path/to/ota.zip [--reboot]"
    exit 1
fi

if [ "$(id -u)" != "0" ]; then
    sayerr "ERROR: must run as root - run 'adb root' first, then apply_ota ..."
    exit 1
fi

say "==> OTA package : $ZIP"
say "==> Active slot : $(getprop ro.boot.slot_suffix)  (update writes the OTHER slot)"

mkdir -p "$WORK"
rm -f "$WORK/payload.bin" "$WORK/payload_properties.txt"

say "==> Extracting payload_properties.txt"
if ! unzip -p "$ZIP" payload_properties.txt > "$WORK/payload_properties.txt" 2>/dev/null \
        || [ ! -s "$WORK/payload_properties.txt" ]; then
    sayerr "ERROR: could not read payload_properties.txt - not an A/B OTA zip?"
    rm -f "$WORK/payload_properties.txt"
    exit 1
fi

say "==> Extracting payload.bin (large - this takes a minute)"
if ! unzip -p "$ZIP" payload.bin > "$WORK/payload.bin" 2>/dev/null \
        || [ ! -s "$WORK/payload.bin" ]; then
    sayerr "ERROR: failed to extract payload.bin (out of space on /data?)."
    rm -f "$WORK/payload.bin"
    exit 1
fi

# Best-effort relabel so update_engine can read these when SELinux is enforcing.
restorecon -RF "$WORK" 2>/dev/null

HEADERS=$(cat "$WORK/payload_properties.txt")

say "==> Applying update via update_engine (do not interrupt) ..."
update_engine_client --update --follow \
    --payload="file://$WORK/payload.bin" \
    --headers="$HEADERS"
RC=$?

# The big file is no longer needed once staging is done (or has failed).
rm -f "$WORK/payload.bin"

if [ "$RC" -ne 0 ]; then
    sayerr "ERROR: update_engine failed (exit $RC); slot NOT switched."
    sayerr "       If it says an update is already pending, clear it with:"
    sayerr "         update_engine_client --reset_status"
    exit "$RC"
fi

say "==> Update staged on the inactive slot."
if [ "$DO_REBOOT" = "1" ]; then
    say "==> Rebooting into the new slot ..."
    sync
    setprop sys.powerctl reboot
else
    say "==> Done. Reboot to boot the new build:  adb reboot"
fi
