# VIM3 M2X/NVMe Boot — Handoff

**Last updated:** 2026-09-20
**Status:** Blocked on power delivery to the VIM3 while flashing over USB-C. Everything else needed to boot Android from the NVMe SSD is in place and pushed.

---

## Where things stand

- Hardware path (M2X + WD SN520 128GB NVMe SSD) verified working: MCU USB3/PCIe mux switch, PCIe Gen2-x1 link, NVMe detection, GPT partitioning with dynamic (`size=-`) userdata.
- `lineage_vim3_nvme` product + automatic U-Boot-from-source build wiring are in place and building successfully (`m` with `lineage_vim3_nvme-bp4a-userdebug` lunch target).
- Three real bugs found and fixed in `u-boot/khadas/vim3` (all pushed to `origin/master`, currently-flashed bootloader has all three):
  1. **NVMe MDTS fallback too large** (`drivers/nvme/nvme.c`, commit `89cedacda90`) — this SSD doesn't report MDTS; the driver's 1MB fallback was silently rejected by the drive above 128KB. Fixed fallback to 128KB (matches common conservative practice elsewhere). Confirmed via bisection on real hardware: 128KB writes succeed, 256KB+ failed identically at LBA 0 and mid-disk before the fix.
  2. **Fastboot mode never probed PCI/NVMe** (`include/configs/khadas-vim3_android.h`, commit `b28bb7e8eef`) — `bootflow scan`'s NVMe hunter auto-calls `pci_init()`, but `fastboot usb 0` is a separate code path that never did. Added `pci enum; nvme scan;` before it.
  3. **`fastboot flash bootloader` impossible once the block backend targets NVMe** (`drivers/fastboot/fb_block.c`, commit `3e6b696054e`) — `fastboot_raw_partition_bootloader` (the mechanism that lets `fastboot flash bootloader` write to eMMC's boot0 hardware partition) only ever existed in `fb_mmc.c`. Ported an equivalent raw-partition fallback into `fb_block.c`, hardcoded to eMMC device 2 regardless of what the block backend targets for Android's own partitions. Also needed `mmc dev 2;` added before `fastboot usb 0` (commit `fd482e76a0a`) — without it, `blk_get_dev("mmc", 2)` inside that new fallback destabilized the USB gadget stack badly enough to kill fastboot's USB transport entirely (confirmed live: adding `mmc dev 2` at the console before manually invoking `fastboot usb 0` fixed it immediately).

## The actual current blocker: power, not software

Today's flashing attempts kept failing in a way that looked like software bugs (NVMe write hangs, PCIe link dying, "timing" issues) but root-caused to **power delivery**, confirmed by this pattern:
- Small transfers (the ~1.3MB bootloader) reliably succeed.
- Large transfers (`boot_a`, 64MB) reliably kill the PCIe link — `pci` afterward shows `01.00.00 0xffff 0xffff` (classic "device not responding" signature), even when `nvme scan` was never run in that session at all. The drop correlates with USB transfer size, not with any NVMe command sequence.
- The VIM3 has **no separate DC barrel jack** — power is exclusively via the same USB-C port used for fastboot data (5-20V PD; Khadas sells dedicated 24W/30W USB-C PD adapters for it). It was being powered off a laptop's USB-C port during today's testing, which very likely can't sustain the combined VIM3 + M2X + SSD load during a large USB transfer without sagging.
- A bare wall charger doesn't work as a fix by itself (no data lines) — plan was either a laptop-on-AC-power retest, a USB-C "2-in-1 charge+sync" splitter cable, or (what you landed on) injecting power directly via the board's **V-IN** pads/header from a bench PSU, bypassing USB-C power negotiation entirely while keeping USB-C free for fastboot data only.

## Picking this up tomorrow

1. Wire up bench PSU to V-IN (check the correct voltage/polarity for this input before connecting — get this right the first time, this is a direct power injection point with presumably no reverse-polarity protection to rely on).
2. With the board powered from the bench PSU and USB-C connected only to your PC for data, redo the GPT write (if `part list nvme 0` comes back empty — the last one may or may not have stuck given how much power instability happened around it):
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
4. If `boot_a` (64MB) now goes through cleanly where it didn't before, the power theory is confirmed and the rest should follow. If it *still* dies at the same point with stable bench-PSU power, that reopens the "real NVMe driver/hardware bug" investigation — at that point the next move is trying a different NVMe SSD to isolate drive-specific firmware fragility from a genuine driver defect, since we ruled out connection/seating and (about to rule out) power.
5. Once Android boots from the SSD: confirm `bootflow scan` picks it over eMMC, and revisit the ACC/suspend-to-RAM VHAL work (`vendor/gschuurman/vehicle_interfaces`, commit `1ba64cf`) on real hardware — that was implemented and pushed today but never tested live (needs the Pico reconnected, or the onboard power button as a stand-in per earlier testing notes).

## Loose ends / things not yet done

- `env default -a && saveenv` needed once more after whatever bootloader ends up flashed tomorrow, to sync `check_func_key` (now includes `mmc dev 2; pci enum; nvme scan;` before `fastboot usb 0`).
- The ACC-line VHAL suspend-to-RAM implementation is untested on hardware (see `vendor/gschuurman/vehicle_interfaces` commit `1ba64cf` and the kernel wakeup-source DTS fix `994a4876d94b9`).
- `packages/apps/OpenHeadunit` (system app for the USB Android Auto picker) is wired in but also untested since the device has been fully occupied with bootloader/NVMe work.
