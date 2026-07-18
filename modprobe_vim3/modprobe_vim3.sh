#!/system/bin/sh
# CONFIG_MODPROBE_PATH target for kernel-initiated request_module() calls.
#
# Android splits kernel modules across /vendor_dlkm/lib/modules and
# /system_dlkm/lib/modules rather than a single /lib/modules, but the kernel
# always invokes CONFIG_MODPROBE_PATH as `modprobe -q -- <name>` with no way
# to pass -d. Toolbox's modprobe only searches /lib/modules by default, which
# doesn't exist on Android, so any driver that relies on request_module() at
# runtime silently fails to load with -2 and the caller sees a probe
# failure. This wrapper supplies the missing search paths and delegates to
# the real modprobe applet.
#
# Not currently required by anything on this board -- brcmfmac's per-vendor
# fwvid plugin (the original reason this was written) is statically linked
# into brcmfmac.ko now instead of calling request_module(). Kept as a
# general-purpose safety net; see wireless.mk and vim3_extra.fragment.
exec /system/bin/toolbox modprobe -d /vendor_dlkm/lib/modules -d /system_dlkm/lib/modules "$@"
