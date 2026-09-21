# VIM3 M2X/NVMe Boot — Handoff

**Last updated:** 2026-09-21 (evening)
**Status:** Root cause of the PCIe-link-dropping problem found (shared USB3/PCIe PHY reset by U-Boot's USB probe) and fixed in u-boot `bd205d88d25` + `872ea6d4a05`; awaiting hardware verification. Everything else needed to boot Android from the NVMe SSD is in place and pushed.

---

## Where things stand

- Hardware path (M2X + WD SN520 128GB NVMe SSD) verified working: MCU USB3/PCIe mux switch, PCIe Gen2-x1 link, NVMe detection, GPT partitioning with dynamic (`size=-`) userdata.
- `lineage_vim3_nvme` product + automatic U-Boot-from-source build wiring are in place and building successfully (`m` with `lineage_vim3_nvme-bp4a-userdebug` lunch target).
- Three real bugs found and fixed in `u-boot/khadas/vim3` (all pushed to `origin/master`, currently-flashed bootloader has all three):
  1. **NVMe MDTS fallback too large** (`drivers/nvme/nvme.c`, commit `89cedacda90`) — this SSD doesn't report MDTS; the driver's 1MB fallback was silently rejected by the drive above 128KB. Fixed fallback to 128KB (matches common conservative practice elsewhere). Confirmed via bisection on real hardware: 128KB writes succeed, 256KB+ failed identically at LBA 0 and mid-disk before the fix.
  2. **Fastboot mode never probed PCI/NVMe** (`include/configs/khadas-vim3_android.h`, commit `b28bb7e8eef`) — `bootflow scan`'s NVMe hunter auto-calls `pci_init()`, but `fastboot usb 0` is a separate code path that never did. Added `pci enum; nvme scan;` before it.
  3. **`fastboot flash bootloader` impossible once the block backend targets NVMe** (`drivers/fastboot/fb_block.c`, commit `3e6b696054e`) — `fastboot_raw_partition_bootloader` (the mechanism that lets `fastboot flash bootloader` write to eMMC's boot0 hardware partition) only ever existed in `fb_mmc.c`. Ported an equivalent raw-partition fallback into `fb_block.c`, hardcoded to eMMC device 2 regardless of what the block backend targets for Android's own partitions. Also needed `mmc dev 2;` added before `fastboot usb 0` (commit `fd482e76a0a`) — without it, `blk_get_dev("mmc", 2)` inside that new fallback destabilized the USB gadget stack badly enough to kill fastboot's USB transport entirely (confirmed live: adding `mmc dev 2` at the console before manually invoking `fastboot usb 0` fixed it immediately).

## Root cause of the "PCIe link dies" problem: shared USB3/PCIe PHY (NOT power)

Earlier revisions of this doc blamed power delivery. That was wrong: a bench 5V/5A supply on V-IN changed nothing.
The real cause (found 2026-09-21, from reading the driver code):

- The USB3.0 Type-A port and the M.2 slot share ONE combo PHY (`usb3_pcie_phy`) behind the MCU-controlled FUSB340 mux.
- U-Boot's USB glue (`meson-g12a-usb-ctrl`) lists it as `usb3-phy0`. When the glue probes (`fastboot usb 0`, `usb start`,
  bootflow's usb hunter, usbkbd), `phy_meson_g12a_usb3_init()` does `reset_assert_bulk()` + `reset_deassert_bulk()` on the
  shared PHY and reprograms it for USB3; the glue's remove path asserts the resets again. That is the PHY the PCIe
  controller is running on -> link dies, `pci` shows `01.00.0 0xffff:0xffff`, every flash fails with
  "failed to get partition info". `nvme info` keeps printing stale cached identify data, so it looks alive.
- Only the *kernel* DT was ever fixed up for this (`meson_ft_board_setup`); U-Boot never edited its own control DT.
- Everything that looked like "power" / "size" / "timing" was this: the drop happens when USB comes up, and big transfers
  just gave us time to notice. The small (bootloader -> eMMC) flash "worked" only because it never needed the SSD.
- Khadas' own patches (khadas-uboot `7003`/`7004`/`CC01`) do NOT fix this -- 7003/CC01 are already upstream (don't tear
  down clocks on link failure), and they mask the shared-PHY reset by running USB *before* `pci enum` in distro boot.
- Fix: commits `bd205d88d25` + `872ea6d4a05` + `68e5d22a4e8` (u-boot, pushed; `68e5d22a4e8` makes the edit same-length/in-place -- the first version resized the live control DT, which shifted 105 nodes (xtal-clk, regulators, ...) that DM had already bound by offset, so PCIe PHY and USB both failed with -ENODEV (`failed to get pcie phy (ret=-19)`, `USB init failed: -19`). **Never fdt_setprop-resize U-Boot's own control DT; same-length in-place edits only.**; the second one enables `CONFIG_BOARD_EARLY_INIT_R` -- without it the hook is compiled but never called, which is what the first hardware test on 2026-09-21 hit): when the MCU says PCIe mode, drop `usb3-phy0` from U-Boot's own control DT in
  `board_early_init_r()`. Expect `vim3: PCIe mode, USB3 PHY left to PCIe` early in the boot log, and a clean `U-Boot 2026.07-g68e5d22a4e80` banner (no `-dirty`). **Built only -- not yet
  verified on hardware.** Bootloader: `out/target/product/vim3/bootloader/u-boot_kvim3_ab-nvme-usb3phy-fix3.bin`.

Do NOT trust `nvme info` as a liveness check (it prints cached data) -- use `pci` and look for `0xffff`.

## Picking this up tomorrow

1. Flash the new bootloader (`fastboot flash bootloader ...usb3phy-fix.bin`, via the Function-key fastboot path so `mmc dev 2` is done first), then `env default -a; saveenv`. Check the boot log for `vim3: PCIe mode, USB3 PHY left to PCIe`. If it's missing, the MCU read failed or the mux is back in USB3 mode -- redo `i2c dev i2c@5000; i2c mw 0x18 0x33 1` and full power cycle. Bench PSU on V-IN is no longer required.
2. Redo the GPT write if `part list nvme 0` comes back empty (the last one may not have stuck, since the link kept dropping around it):
   ```
   pci enum
   nvme scan
   gpt write nvme 0 $partitions
   part list nvme 0
   ```
3. Retry the fastboot flash sequence (from `out/target/product/vim3/`):
   ```
   fastboot flash boot_a boot.img
   fastboot flash boot_b boot.img
   fastboot flash vendor_boot_a vendor_boot.img
   fastboot flash vendor_boot_b vendor_boot.img
   fastboot flash init_boot_a init_boot.img
   fastboot flash init_boot_b init_boot.img
   fastboot flash dtbo_a dtbo.img
   fastboot flash dtbo_b dtbo.img
   fastboot flash vbmeta_a vbmeta.img
   fastboot flash vbmeta_b vbmeta.img
   fastboot flash vbmeta_vendor_dlkm_a vbmeta_vendor_dlkm.img
   fastboot flash vbmeta_vendor_dlkm_b vbmeta_vendor_dlkm.img
   fastboot flash vbmeta_system_dlkm_a vbmeta_system_dlkm.img
   fastboot flash vbmeta_system_dlkm_b vbmeta_system_dlkm.img
   fastboot flash super super.img
   fastboot erase userdata
   fastboot reboot
   ```
4. Verify the fix directly: `pci enum`, `pci` (expect 15b7:5003), then run `fastboot usb 0`, do one `fastboot getvar partition-size:boot_a` from the host, Ctrl+C, and `pci` again -- it must still show 15b7:5003. If it still shows 0xffff, the USB glue is being probed before `board_early_init_r` (or the MCU read failed) and we need to look at what else touches the PHY.
5. Once Android boots from the SSD: confirm `bootflow scan` picks it over eMMC, and revisit the ACC/suspend-to-RAM VHAL work (`vendor/gschuurman/vehicle_interfaces`, commit `1ba64cf`) on real hardware — that was implemented and pushed today but never tested live (needs the Pico reconnected, or the onboard power button as a stand-in per earlier testing notes).

## Bootloader flashing safety (learned the hard way, 2026-09-21)

- The FIP size changes every build. A raw `mmc write ... 0x1 <count>` needs `count = ceil(size/512)` computed from the *actual* file -- reusing the previous build's count truncated BL33 and bricked boot0 (BL2 loops `BL33 CHK: 0x000000ff`; recover via maskrom, see the `project_bootloader_brick_recovery` memory).
- Always verify: after `mmc write`, `mmc read` it back to another RAM address and compare `crc32` against the file's CRC32 (U-Boot has `crc32` but no md5sum/sha256sum command).
- Prefer `fastboot flash bootloader` once the fixed U-Boot is running (it sizes the write itself); use `stage` + `mmc write` only as a last resort.

## Loose ends / things not yet done

- `env default -a && saveenv` needed once more after whatever bootloader ends up flashed tomorrow, to sync `check_func_key` (now includes `mmc dev 2; pci enum; nvme scan;` before `fastboot usb 0`).
- The ACC-line VHAL suspend-to-RAM implementation is untested on hardware (see `vendor/gschuurman/vehicle_interfaces` commit `1ba64cf` and the kernel wakeup-source DTS fix `994a4876d94b9`).
- `packages/apps/OpenHeadunit` (system app for the USB Android Auto picker) is wired in but also untested since the device has been fully occupied with bootloader/NVMe work.

## Update 2026-09-21 evening: U-Boot now boots Android from NVMe (userspace still to verify)

Three more U-Boot bugs found on hardware, all fixed and pushed (u-boot master `45584307a3e`):
1. **DTB alignment (`3d1498a2b3e`)** -- mainline libfdt rejects DTBs at non-8-byte-aligned addresses. vendor_boot packs the DTBs
   back to back, so idx 1 (VIM3) is unaligned -> `fdt_check_header` failed silently -> "FDT and ATAGS support not compiled in".
   `boot_get_fdt` now copies an unaligned Android DTB to an aligned buffer.
2. **Android bootmeth was eMMC-only (`cb34cdad806`)** -- `android_check()` rejected non-MMC bootdevs (silently), `bcb` load hard-coded
   `"mmc"`, and AVB (`get_partition`) used `find_mmc_device`. Now NVMe is accepted; AVB has `avb_ops_alloc_blk()` for non-MMC media.
3. **Boot order (`58852b00707`)** -- NVMe build defaults to `boot_targets=nvme0 mmc2` (eMMC build: `mmc2`). The SAVED env keeps the
   old value, so run `env default -a; saveenv` after flashing a new bootloader.

Debug notes: the NVMe defconfig now has `CONFIG_BOOTSTD_FULL` (`bootflow scan -lae`, `bootdev list`, `bootdev hunt -l`); `check_func_key`
uses `bootflow scan -b` accordingly. With BOOTSTD_FULL + `bootmeths=android`, `bootflow scan <label>` silently fails
(global bootmeths can't be skipped) -- select the device with `setenv boot_targets nvme0` instead. Debug binary block count changed
(FIP now 0xa06 blocks) -- always recompute `ceil(size/512)`.

Verified on hardware: `bootflow scan -lae` shows `nvme#0.blk#1.bootdev.whole` as a valid android bootflow; `bootflow scan -b` loads kernel,
ramdisk and DTB from the SSD and starts the kernel. It then reset because the images on the SSD were an OLD Android build (no
`boot_devices=soc/fc000000.pcie`, no NVMe fstab, no pci-meson in first-stage modules). NEXT: rebuild `lineage_vim3_nvme-bp4a-userdebug`,
reflash vendor_boot/boot/init_boot/dtbo/vbmeta*/super to the SSD, boot, and check `ro.boot.boot_devices` and `/dev/block/platform/soc/fc000000.pcie/by-name`.
Serial kernel console for debugging: `setenv bootargs "no_console_suspend console=ttyAML0,115200 earlycon loglevel=8"` before `bootflow scan -b`.
