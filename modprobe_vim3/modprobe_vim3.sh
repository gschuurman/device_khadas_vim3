#!/system/bin/sh
# CONFIG_MODPROBE_PATH target for kernel-initiated request_module() calls.
#
# Android splits kernel modules across /vendor_dlkm/lib/modules and
# /system_dlkm/lib/modules rather than a single /lib/modules, but the kernel
# always invokes CONFIG_MODPROBE_PATH as `modprobe -q -- <name>` with no way
# to pass -d. Toolbox's modprobe only searches /lib/modules by default, which
# doesn't exist on Android, so any driver that relies on request_module() at
# runtime (e.g. brcmfmac's per-vendor fwvid plugin, brcmfmac_wcc for this
# board's BCM4359) silently fails to load with -2 and the caller sees a probe
# failure. This wrapper supplies the missing search paths and delegates to
# the real modprobe applet.
exec /system/bin/toolbox modprobe -d /vendor_dlkm/lib/modules -d /system_dlkm/lib/modules "$@"
