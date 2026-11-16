# Plan: PSCI CPU_SUSPEND and SYSTEM_SUSPEND on the VIM3 (A311D / G12B)

Written 2026-11-16 at the end of the session that got ACC-off suspend-to-RAM working. Meant to be
executed in a fresh session: read this file, `handoff.md` and the memory note
`project_s2ram_acc_sleep` first.

## Why

ACC-off sleep works now, but only as **s2idle**: `/sys/power/mem_sleep` offers `[s2idle]` only,
because our TF-A (`bootloader/arm-trusted-firmware`, gschuurman fork, branch `g12b-vim3`) implements
just CPU_ON/OFF, SYSTEM_OFF and SYSTEM_RESET. In s2idle the six cores only execute WFI. The cores,
the clusters, DDR and VDDEE stay fully powered. The unit sleeps up to 12 h on the car battery, so every
100 mW counts (estimate: 1–2.5 W at 12 V in total, NOT yet measured).

Two levels, in this order:

| Phase | What | Gain (estimate) | Risk / effort |
|---|---|---|---|
| A | PSCI `CPU_SUSPEND`: core and cluster power-down in idle, used by cpuidle and by s2idle | 0.1–0.3 W in sleep, also less heat while running | medium; TF-A + DTS, OP-TEE hooks |
| B | PSCI `SYSTEM_SUSPEND` ("deep"): DDR self-refresh, VDDEE down, BL30 watches the wake sources | SoC part from ~0.5–1 W to ~0.1 W | large; reverse-engineering the BL30 protocol, kernel drivers must survive power loss |

## Phase 0 — measure first (blocking for B, recommended for A)

- **Current draw** on the 12 V feed of the unit (clamp or multimeter in series with the fuse): running
  with the screen off, and in s2idle (contact off, after `DEEP_SLEEP_ENTRY` + 10 s). Without numbers
  nobody can tell whether phase A/B or the peripherals (TPS55288 in forced PWM, hubs, MS210x capture
  card, panel) are the bigger lever.
- **Battery voltage calibration:** compare a multimeter reading at the battery with VHAL
  `0x21400600` (MCU_PROP_BATTERY_MV). It read 13.2 V after 12 h ACC off, which is not a resting lead-acid
  value. The 12.2 V low-battery guard in MCU firmware 0.2.0 depends on this.

## Test infrastructure (read before touching TF-A)

- **Never flash a new FIP before it RAM-boots.** Maskrom RAM-boot via the USB 2.0 hub +
  `boot-g12.py` (memory `project_bootloader_brick_recovery`). `bootdelay=0` bricks. Flash the bootloader
  only with `fastboot flash bootloader $OUT/u-boot_kvim3_ab_optee.bin` after RAM-boot passes.
- **The car unit has no serial console** (debug UART not broken out on the mainboard). TF-A and early
  kernel bring-up therefore needs **a bench VIM3 with a serial console**, or the unit taken out of the
  car. Do phase A on the bench; only take a FIP to the car when it has passed there.
- Kernel console is `ttynull`; crash logs come from pstore (`/sys/fs/pstore/console-ramoops-0`) after a
  warm reboot. `pm_test` levels for s2idle: `freezer devices platform` (no `core`).
- FIP build is inline: `brunch vim3_nvme` builds `u-boot_kvim3_ab_optee` from TF-A + OP-TEE + U-Boot
  (`vendor/khadas/vim3/bootloader/Android.mk`, `scripts/build-g12b-optee-fip.sh`). Release TF-A has
  `assert()` compiled out: check `MAX_XLAT_TABLES`/`MAX_MMAP_REGIONS` whenever an mmap region is added
  (bit us before, memory `project_khadas_bl_blobs_optee`).

## Reference material already on disk

- **Stock Amlogic BL31 decompiled** (Ghidra 12.1.3): `~/android/optee-fip-test/re/stock_bl31_decomp.c`
  (50k lines), headless script `re/scripts/DecompAll.java`, Ghidra in `~/tools/ghidra_12.1.3_PUBLIC`,
  load args `-processor AARCH64:LE:64:v8A -loader BinaryLoader -loader-baseAddr 0x05100000`. It is a
  TF-A v1.3-based BL31 with the full suspend path: strings `aml_validate_power_state` (~line 38865),
  `psci_cpu_suspend_start/finish`, `psci_system_suspend`, `psci_query_sys_suspend_pwrstate`,
  `opteed_cpu_suspend_handler`. Its `plat_psci_ops` table is the map of what Amlogic does per level.
  The stock BL31 blob itself is in `vendor/khadas/vim3/bootloader/fip-collect-g12b-kvim3-khadas-vims-pie-20210111-211833.tar.gz`.
- Earlier RE findings (memory `project_khadas_bl_blobs_optee`): mailbox slot = AFF0 + AFF1*4, CCI-400 at
  0xff3e0000 (cluster0 → S3, cluster1 → S4), cluster power bits at 0xff634644 / 0xff634784 (bit 10),
  chip id 0x29 = G12B. These are already implemented in our TF-A for CPU_ON/OFF.
- Our TF-A: `plat/amlogic/g12b/g12b_pm.c` (ops table at the bottom), `plat/amlogic/common/aml_scpi.c`
  (`aml_scpi_set_css_power_state()` = SCPI 0x04 with cpu/cluster/css states; `aml_scpi_sys_power_state()`
  = SCPI 0x08), `aml_mhu.c`.

## Phase A — CPU_SUSPEND (core / cluster power-down)

**Status 2026-11-16: steps 1, 2 and 4 written, nothing validated on hardware, nothing committed.**
- Step 1 RE → `re-stock-bl31-suspend.md`. Key results: original StateID format (0x00010000 core,
  0x01010000 cluster, StateID 0); stock powers CPU0 down like any core (our WFE-loop restriction
  is ours); the Amlogic vendor dtsi defines these idle states but comments out every `cpu-idle-states`
  (never shipped); stock bl301 deep-suspend wake = GPIOAO_7 falling edge (our ACC line).
- Step 2 TF-A: `g12b_pm.c`, all behind `make G12B_CPU_SUSPEND=1` (+ `G12B_SUSPEND_CLUSTER_OFF=1` for
  SCPI cluster_state=OFF on the A73 cluster). The default build is unchanged and so is the FIP `brunch` builds. To test, add
  the flags to the TF-A make line in `vendor/khadas/vim3/bootloader/scripts/build-g12b-optee-fip.sh`.
  CPU0 gets standby only and cluster 0 never goes off. Builds clean (release + debug).
- Step 3 OP-TEE: no change needed (generic no-op suspend/resume handlers; opteed restores secure EL1 context).
- Step 4 DTS: no kernel DTS change. U-Boot (`vim3_dt_add_idle_states`) adds the idle states to the kernel
  DT only when BL31 reports CPU_SUSPEND (PSCI_FEATURES): the kernel sets `psci_ops.cpu_suspend` without a
  feature check, so states on a BL31 without CPU_SUSPEND would make idle spin without WFI.
- **First bench question:** does BL30 power a core back on when an interrupt arrives for it? Nobody has
  shipped this. If not, the first CPU_SUSPEND power-down loses that core forever.
  Test with a bench-only DTS: `CPU_RET_0` on every CPU and `CPU_SLEEP_0` on one A73 only (e.g. cpu2 = MPIDR 0x100);
  watch that CPU's `cpuidle/state*/usage` and check that it still
  runs work (`taskset -c 2 true` in a loop, and IPIs/timers keep firing).

1. **RE the stock BL31 first** (half a day): find its `plat_psci_ops` and follow
   `validate_power_state`, `pwr_domain_suspend`, `pwr_domain_suspend_finish`,
   `pwr_domain_pwr_down_wfi`. Write down:
   - the power-state encoding it accepts (original vs extended StateID format; our DT says
     `arm,psci-1.0`);
   - the SCPI message per level (core off / cluster off / system), and whether a retention state exists;
   - what it does around the GIC, CCI snoops and the cluster power bits on suspend vs on OFF;
   - **how it suspends CPU0.** Our CPU_OFF cannot power CPU0 down (it emulates OFF with a WFE loop and
     jumps to the warm entrypoint by hand, because RVBAR 0xda834650 / the mailbox did not work for
     CPU0). If the stock BL31 also never powers CPU0 down, CPU0 gets a retention/WFI state only.
2. **TF-A** (`plat/amlogic/g12b/g12b_pm.c`): add `validate_power_state`, `validate_ns_entrypoint`,
   `pwr_domain_suspend`, `pwr_domain_suspend_finish`. Core power-down reuses the CPU_OFF path
   (SCPI 0x04 cpu=OFF, warm entry via the mailbox slot, `gicv2_cpuif_disable/enable`). Cluster
   power-down reuses `g12b_cluster_off()` / the cluster-on sequence (CCI snoop disable/enable, cluster
   power bit). PSCI coordination (last man down) is done by the generic framework; keep
   `PLAT_MAX_PWR_LVL` = cluster for phase A.
3. **OP-TEE**: opteed forwards CPU_SUSPEND/RESUME to OP-TEE (`opteed_cpu_suspend_handler`). Check that
   `bootloader/optee_os` plat-amlogic handles `cpu_suspend`/`cpu_resume` (generic `thread_*` vectors) and
   that secure state (TZDRAM, the HUK cache fix of memory `project_huk_cold_boot_cache`) survives a core
   power-down. Run xtest + KeyMint after suspend cycles.
4. **Kernel DTS** (`kernel/khadas/vim3`, `meson-g12b.dtsi` or the VIM3 dtsi): `idle-states` with
   `arm,idle-state` nodes (`arm,psci-suspend-param` = the encoding from step 1, entry/exit latency,
   min-residency), `cpu-idle-states` on each CPU (A53 and A73 clusters separately).
   `CONFIG_ARM_PSCI_CPUIDLE=y` is already set. s2idle uses the deepest cpuidle state automatically.
5. **Validate on the bench**: RAM-boot; all 6 cores online; `cpuidle/state*/usage` increments on every
   CPU; hotplug stress (`echo 0/1 > cpuN/online` in loops); audio/video playback without glitches;
   `pm_test=platform` and real s2idle cycles with the XPWR wake; OP-TEE xtest; 24 h soak. Then flash on
   the bench, then the car.

### Phase A outcome (bench, 2026-11-16) — parked

- Works: PSCI standby (retention) on every core (~125k entries, 0 rejected); core power-down on up to
  two cores (cpu1 A53, cpu2/3 A73), hundreds of cycles; KeyMint and hotplug fine afterwards. U-Boot
  adds the idle states only when BL31 reports CPU_SUSPEND (vim3_dt_add_idle_states, PSCI_FEATURES).
- Fails: with 5 cores in core power-down, BL30 eventually powers a suspended A73 off and never back on
  (serial: "hard LOCKUP on cpu N"; BL31 stuck-core report: stage 3 = final WFI, SCP acked, mailbox
  entry set, crumb 0 = reset stub never reached). After that even PSCI SYSTEM_RESET (through the SCP)
  never completes: the board hangs until a manual reset. A BL31 "kick" (SCPI on, then off+5 ms+on
  like the CPU_ON retry) revives the core electrically but it then stalls in the reset stub (crumb
  0x81). Amlogic never enabled core power-down on G12B (vendor 4.9: commented out; 5.4: SM1 only;
  5.15: none). Parked in favour of phase B. Code: TF-A G12B_CPU_SUSPEND (+ stuck-core report and
  kick), U-Boot modes off/a73one/a73two/cpu12/a73three/core, all uncommitted.
- Serial: Pico debug probe on the VIM3 debug UART (/dev/ttyACM0), log with `cat` (read-only).

## Phase B — SYSTEM_SUSPEND (deep)

**Status 2026-11-16 (bench, RAM-booted, VIM3 without mainboard): FULL DEEP SUSPEND + RESUME WORKS.**
Linux → BL31 SYSTEM_SUSPEND (CPU0, css OFF) → BL30 DDR self-refresh (15 us) → wake → DDR resume
(229 us) → BL31 "PM: system resume" → CPU1-5 up, NVMe back (294 MB/s read), USB reinit, KeyMint OK,
suspend_stats success 1. Woke immediately (wake reason 14 acc-wake: the ACC line floats without the
mainboard), so a long sleep is not tested yet.
- TF-A: `make G12B_SYSTEM_SUSPEND=1` (PLAT_MAX_PWR_LVL=2, CPU0 powers down at system level, resume
  re-inits console + GIC distributor and sets AO_CPU_FLAGS bit 0 = "AP back" for BL30). Bench build
  also dumps DMC registers (g12b_dmc_dump). Default build unchanged.
- U-Boot: appends `mem_sleep_default=s2idle` when BL31 reports SYSTEM_SUSPEND (the kernel would make
  deep the default by itself); deep is opt-in via /sys/power/mem_sleep.
- **BL30 needs a 2 x 2-byte patch** (bl30.bin from fip-collect-g12b-kvim3-khadas-vims-pie-20210111):
  its DDR suspend and resume wait for DMC_DRAM_STAT (0xff638654) bits 7:4 == 0 or 4 ("access"), but
  the stock BL2 of the same collection leaves the controller in state 2 after command 4 (BL2 waits for
  2 itself). Patch: `bne` at 0x3794 (suspend) and 0x36d2 (resume) -> `nop` (0xbf00); both are the
  loop-back of "cmp r2,#0x40". BL30 is re-signed by the FIP script (aml_encrypt --bl30sig), no key.
  Without the patch: endless "DMC_DRAM_STAT11: 0x24" (suspend) / "DMC_DRAM_STAT3: 0x24" (resume).
- **In the regular build (2026-11-16):** vendor/khadas/vim3 9437876 builds BL31 with G12B_SYSTEM_SUSPEND=1
  and patches BL30 in generate-g12b-optee-fip.sh (original bytes checked); TF-A a138b29bb, U-Boot
  7d30544539f (s2idle guard). Flashed on the bench/car VIM3 2026-11-16 (copy + md5 in
  ~/android/fip-flashed/). Deep is opt-in only: `echo deep > /sys/power/mem_sleep`.
- Open: black HDMI after a deep resume (Android/DRM say the output is on); long sleep test (RTC wake with
  acc-wake disabled, or the real ACC line; the ACC line floats without the mainboard so it wakes at
  once); power-rail behaviour of the KVIM3 bl301 (power_off_at_24M cuts 5V/3V3/VCCK) vs the car
  mainboard; current measurement; then Android opt-in to deep for the ACC-off sleep.

Only after phase 0 shows the SoC is the dominant consumer.

1. **RE**: stock BL31 `psci_system_suspend` → platform suspend at system level → which SCPI message
   tells BL30 to take over (likely 0x08 SET_SYS_POWER_STATE with a suspend value, or a vendor command),
   how DDR self-refresh and the resume entrypoint are handed over, and **how wake sources are
   configured** (the vendor kernel passes them to BL30; we need GPIOAO_7 = XPWR/ACC wake, and possibly
   the meson-vrtc alarm, which is designed for BL30 and does nothing in s2idle).
2. **TF-A**: `get_sys_suspend_power_state`, system level in `pwr_domain_suspend`, the BL30 handshake;
   `PLAT_MAX_PWR_LVL` = system. With PSCI SYSTEM_SUSPEND present the kernel offers `deep` in
   `/sys/power/mem_sleep` by itself; Android then needs `mem_sleep_default=deep` or a write to
   `/sys/power/mem_sleep`.
3. **Kernel drivers must survive the power loss** of the EE domain:
   - **PCIe/NVMe (rootfs!)**: `pci-meson.c` has no suspend/resume at all. In s2idle the link survives
     (fixed this session by kernel `5551e3d3d5362`, link_up accepts L1). In deep the controller and the
     shared USB3/PCIe PHY lose state: needs `dw_pcie_suspend_noirq()/dw_pcie_resume_noirq()` plus a
     meson host re-init (clocks, PHY power on, PERST, link start). Mind the shared PHY: the USB side
     resetting it kills PCIe (memory `project_m2x_nvme_boot`).
   - USB (dwc2/dwc3/xhci, it already does `xHC error in resume, Reinit` in s2idle), ethernet, meson-drm/
     HDMI/DSI panel, audio (axg TDM/FRDDR, the car audio module), the gpio/irq controllers.
   - Test each with `pm_test` first; the rootfs being on NVMe means a broken PCIe resume reboots with
     `vold-failed` (pstore has the log).
4. **MCU**: in deep, `usb_suspended` behaves as in s2idle; the MCU wake (XPWR) and the 12 h off delay need
   no change. Re-measure.

## Other open items from the 2026-11-16 session (not CPU suspend, but same area)

- **Not yet on the car**: vehicle_interfaces `fc41ccd` (suspend-save props, 12 h only for MCU fw 0.2.0)
  and the firmware changes. Needs a build + OTA, then the **MCU firmware 0.2.0 PC flash**
  (`~/android/mcu-fw/carradio_mcu-0.2.0-fc41ccd.uf2`, BOOTSEL + PC or SWD; R72 is a pull-down).
- **Validate the suspend savings one by one** (`mcutool set suspend_save 2` = TPS55288 PFM, check
  `tps_mode` reads 0x?1 with OE kept; then bit 0 = hub park). Hub park is only safe after checking that
  nothing wakes the VIM3 while the hub is parked (USB remote wakeup from the hub disconnect, or any other
  wake): the VHAL can't reach the MCU then and its 10 s fallback publishes ON → Android awake all night.
- **MCU firmware self-update from Android (user's "option 3")** — design agreed, not implemented: stage the
  new image into the inactive RP2350 partition over the existing HID link while Android runs, switch
  slots only when the VIM3 supply is off (MCU state OFF), so nothing needs to hold EN through a reset
  (R72 pull-down). Updater lives in the VHAL (owns the HID link, knows the MCU version), firmware image
  shipped in the vendor image. Take the bootrom partition/TBYB API from the SDK headers in
  `~/android/pico/pico-sdk` and RP2350 datasheet ch. 5 (A/B selection, TBYB deadline, hashed images,
  erase time vs watchdog). Partition sizes are fixed by the one PC flash that installs the partition
  table — decide them before that flash.
- Wi-Fi does not reconnect after a resume while the screen is off (PNO); screen on fixes it.
- VBUS of the VIM3 USB ports in suspend: `USB_PWR` (GPIOA_6) is also `phy-supply` of the PCIe PHY, so it
  can only go off after decoupling that in the DT; touches the rootfs PHY, do last.
