# VIM3 AAOS — Handoff

**Consolidated 2026-09-29.** Replaces the older topic handoffs (`handoff-aaos-apps.md`, `handoff-keymint-optee.md`,
`handoff-nvme.md`, `handoff-mesa26-kernel618.md`, `handoff-vulkan-android-profiles.md`, `handoff-2026-09-29.md`);
their full history is in git (`git log -- device/khadas/vim3/handoff-*.md`). Longer background per topic lives in the
Claude memory notes (`~/.claude/projects/-home-glenn-android-lineage23/memory/`).

Pick work from **§2**. §1 is the current state, §3 the reference for how to work on this board.

---

## 1. Current state

**Board:** Khadas VIM3 (A311D, 4 GB), Android 16 / LineageOS 23.2 AAOS, product `lineage_vim3_nvme`, boots from NVMe
(M2X + WD SN520). USB adb serial `CE88ECF8D115`. Driver user = 10, headless system user = 0.
- Slot `_a` (OTA 2026-09-29): kernel `6.12.93-4k-g0dd64554038e` (GKI android16-6.12.93 + fork, panfrost uapi 1.6),
  Mesa 26.2.3 + our patches `a67490155ea` (prebuilts vendor `692793c`). Slot `_b` = previous OTA (same kernel,
  older Mesa) as fallback.
- Bootloader: inline-built `u-boot_kvim3_ab_optee.bin` (U-Boot + from-source TF-A g12b + OP-TEE 4.10) in eMMC boot0.
- Security: KeyMint on OP-TEE (Rust TA), OP-TEE gatekeeper with persisted failure records, HUK from efuse, HW RNG.
  SELinux **permissive**.
- GPU: GLES 3.1 (panfrost), Vulkan 1.4 (panvk v7); `VP_ANDROID_15_minimums` SUPPORTED, `16_minimums` fails only on
  protected memory. Installed-system CTS: subset14 0 Fail, reconvergence 6237/6237, GLES = baseline.
- Apps: LineageOS SetupWizard (car flow), Organic Maps (map card + nav banner, release-signed), Dicio assistant,
  CarRadioApp (FM + DAB via RTL-SDR; the redesigned UI is only fast-deployed, see §2), HeadUnit Revived.

**Repos** (everything committed and pushed 2026-09-29, except this handoff and the deletions of the old ones):
| Path | Remote / branch | Notes |
|---|---|---|
| device/khadas/vim3 | gschuurman/device_khadas_vim3 `lineage-23.2` | |
| vendor/khadas/vim3 | gschuurman/vendor_khadas_vim3 `lineage-23.2` | **plain checkout** (not repo-managed) |
| kernel/khadas/vim3 | gschuurman/android_kernel_yukawa `lineage-23.0` @ 0dd64554038e9 | safety branch `backup-pre-panfrost16` |
| kernel/khadas/vim3_overlay | gschuurman/kernel_overlay_amlogic_yukawa `main` | config fragment, aic8800 |
| u-boot/khadas/vim3 | gschuurman/u-boot `master` @ 7428383a2ba | **plain checkout**; local branch name `optee-bl31-2019-ramboot-test` = master |
| bootloader/arm-trusted-firmware, bootloader/optee_os | gschuurman forks `g12b-vim3` | |
| vendor/mesa3d-upstream | gschuurman/mesa `vim3-26.2.3` @ a67490155ea | 26 commits on mesa-26.2.3 |
| external/minigbm | gschuurman/external_minigbm `lineage-23.2` | YUV rendering |
| vendor/gschuurman/vehicle_interfaces | gschuurman `android-16` | VHAL, audiocontrol, GNSS, apps |
| hardware/amlogic/yukawa/audio | gschuurman `lineage-23.0` | audio HAL fork |
| ~/android/mesa-mr (worktree) | gschuurman/mesa `mr/*` | upstream MR branches, see §2 A1 |

Plain checkouts (`vendor/khadas/vim3`, `u-boot/khadas/vim3`): `repo sync --force-sync` would replace their `.git` —
check `git rev-list @{u}..HEAD` is empty first.

---

## 2. Open work — pick from here

Legend: 🖥 doable from the desk over adb · 🚗 needs peripherals / the car / a phone · ⏳ long-running

### A. Graphics / GPU
1. **Upstream Mesa MRs (in progress)** 🖥⏳ — branches `mr/*` on upstream main `02035145136`, pushed to
   github.com/gschuurman/mesa as backup; notes + evidence in `~/android/mesa-mr-notes.md`. Mesa policy
   (`docs/submittingpatches.rst`): commit messages/comments/GitLab text in your own words, disclose AI with
   `Assisted-by:`/`Generated-by:` (branches have `Generated-by: Claude Code (Claude Opus 5.5)` + placeholder bodies),
   no autonomous submission. Priority: bi-last-tuple-writes, cmd-queue-initial-buffer, panvk-jm-instance-offset
   (repro `~/android/vk12-wip/instattr-test`: main 2/8, fixed 8/8), nir-fadd-fneg-nan, bi-fp16-ftz,
   panvk-ahb-memory-report, panvk-bifrost-reconvergence.
   - Testing DONE (results in the notes): upstream main 21 Fail → main+fixes 0 Fail, no GPU faults; upstream's
     8b03ae787f4 covers subgroups.arithmetic, so our 05f5f33 is not needed upstream (drop it at the next Mesa rebase).
     `command_buffer_secondary` is state-dependent: run it alone (unfixed 3/3 Crash, fixed 3/3 Pass).
     **Remaining: you write the commit messages/MR text and submit.**
2. **JM index min/max remainder upstream** 🖥 — upstream 7e6f47400db already skips null-index-buffer indirect draws;
   our remaining parts (bounded index loads; null jobs for indexCount 0 / min>max) need a dedicated repro first.
3. **GPU soak test on the car screen** 🖥 — 10+ min CarLauncher + Organic Maps (GL) + RVC TextureView on the new
   kernel/Mesa, watch `dmesg | grep -i panfrost`.
4. **NPU (etnaviv + Teflon)** 🖥 — first real model run (MobileNet v1 uint8 via a TFLite runtime for Android arm64);
   for apps: public.libraries entry + file_contexts label for libteflon.
5. **Protected memory (Vulkan item 6)** — parked. Research only: stock BL2 decompiled
   (`~/android/optee-fip-test/re/stock_bl2_decomp.c`, Ghidra project `re/proj2`, JDK `prebuilts/jdk/jdk21`),
   DMC secure ranges at 0xff639000 programmed by BL2, Mali = DMC port 1, Amlogic's kbase has no protected-mode glue;
   whether G52 protected-mode traffic is distinguishable at the DMC is unproven. Not CTS-enforced on Android 16.
   Memory note `project_vulkan_protected_memory`.
6. **Weekly GKI CI** 🖥 — `.github/workflows/rebase-gki.yml` now replays 28 fork commits incl. the panfrost 1.6
   backport; check its first run (Mondays 08:00 UTC); after a force-push, reset local `lineage-23.0`.

### B. Security / firmware
1. **Cold-boot HUK fix** 🖥 — optee_os dea8b8656 verified via RAM-boot only; confirm from flash: power off for
   minutes, boot, grep serial for "huk: uncached view of the efuse buffer was stale", keystore2 up.
2. **SELinux enforcing** 🚗 — sweep needs the peripherals attached (audio, RTL-SDR, GNSS, camera, BT).
3. **Root of trust / AVB / RPMB rollback / own TA signing key / attestation** 🖥 — KeyMint compliance list
   (RoT currently from HAL props, attestation keys software).
4. **U-Boot USB3/PCIe PHY fix** (`bd205d88d25`, `68e5d22a4e8`) — notes never recorded a HW confirmation; check the
   boot log for `vim3: PCIe mode, USB3 PHY left to PCIe` and that NVMe survives `fastboot usb 0`.

### C. Car features — verify on hardware
1. **ACC-line suspend-to-RAM** 🚗 — VHAL implementation (vehicle_interfaces `1ba64cf`) + kernel wakeup-source DTS;
   never tested (needs the Pico / ACC line, or the power button as stand-in).
2. **OpenHeadunit** (USB Android Auto picker, `packages/apps/OpenHeadunit`) 🚗 — wired into the build but untested;
   currently not installed on the board.
3. **Wireless Android Auto** 🚗 — live test with the phone as P2P group owner (both BoardConfig fixes are in).
4. **Rear view camera with the real grabber** 🚗 — settings screen + early-boot RVC exist, never tested with the USB
   grabber attached.
5. **Native FM radio (RTL-SDR)** 🚗 — live FM listening test; stereo decode and RDS are TODO.
6. **DAB** 🚗 — app-side "tune to default" built+deployed but the verify tap was never done; physical antenna unplug
   path untested.

### D. Bugs
1. **Bluetooth** 🚗
   - MapClient boot crash loop: FIXED in the Bluetooth fork (`packages/modules/Bluetooth`, gschuurman
     `lineage-23.2` @ ed6655a2ec, pushed 2026-09-29), not yet in an OTA. Only triggers once a phone has connected
     over MAP (remote-SIM subscription record) → verify after pairing + reboot: `logcat -b crash` empty, BT pid stable.
   - A2DP crash when music stops, HFP CONNECTING_TIMEOUT: last seen June; no phone paired since the 09-25 clean
     flash → pair the phone, play/stop music, make a call, capture `logcat -b all` + `logcat -b crash`.
2. **DAB choppy audio** 🚗 — RTL-SDR (4 MB/s) starves the 8-ch USB sound card on the shared hub 1-1.4: move the
   dongle to another hub (audio code proven clean).
3. **"Video buffer" glitching under first-boot load** — seen 2026-09-21 (CmaFree ~6 MB of 576 MB during
   dexopt/Play Store); needs a symptom description / repro before work.

### E. Features to implement
1. ~~Bake the CarRadioApp redesign into the image~~ — already done (checked 2026-09-29): repo-managed
   (`gschuurman/packages_apps_Car_Radio` `main` @ 9f792f5, manifest line 12), built into `/system/priv-app/CarRadioApp`
   (on-device APK == build output), installed for driver user 10. Open: DAB reception (see C6/D2).
2. **Car audio EQ + loudness** 🖥 — DynamicsProcessing plan in memory `project_car_audio_volume_eq`
   (CarAudioTuner app is disabled).
3. **Radio polish** — stereo/RDS (C5), per-block DAB scan progress in the scan wizard.
4. **Phone GPS into AAOS** — deferred; options A (BT-NMEA → mock provider) / B (2nd NMEA source in the GNSS HAL),
   memory `project_phone_gps_into_aaos`.

### F. Housekeeping
1. Commit this handoff (and the deletions of the old ones) when you're happy with it.
2. Kernel repo object store holds 7.25 GiB of `tmp_pack_*` garbage from interrupted fetches
   (`.repo/projects/kernel/khadas/vim3.git/objects/pack/`); remove when no git process runs.

---

## 3. Reference — how to work on this board

### Build
- bash only (zsh breaks envsetup): `bash -c 'source build/envsetup.sh; lunch lineage_vim3_nvme-bp4a-userdebug; m <target>'`.
  Full: `brunch vim3_nvme`. Release token `bp4a` is required; don't use trunk_staging (Baklava apks don't scan).
- `brunch vim3_nvme` builds the kernel (`kernel/khadas/vim3`, config fragment `kernel/khadas/vim3_overlay/vim3_extra.fragment`)
  and both FIPs (`vendor/khadas/vim3/bootloader/Android.mk`: `u-boot_kvim3_ab.bin` stock BL31, `u-boot_kvim3_ab_optee.bin`
  TF-A + OP-TEE = the one flashed). Uncommitted U-Boot edits don't trigger a rebuild (depends on .git refs).
- Targeted image rebuilds: `m bootimage vendorbootimage initbootimage dtboimage superimage vbmetaimage` (never
  `vendorbootimage vbmetaimage` alone: stale super vs new vbmeta breaks AVB).
- OTA: `m otapackage` → `out/target/product/vim3/lineage_vim3_nvme-ota.zip` (~9 min).

### Install
- **OTA over USB (normal path):** `unzip -o payload.bin payload_properties.txt` into `~/android/ota-mesa26`, run
  `python3 rangeserver.py 8765` there as a real background process, `adb reverse tcp:8765 tcp:8765` (lost on every
  reboot), push `payload_properties.txt` to `/data/local/tmp/pp.txt`, then
  `adb shell 'update_engine_client --update --follow --payload=http://127.0.0.1:8765/payload.bin --headers="$(cat /data/local/tmp/pp.txt)"'`
  (~3 min, writes the inactive slot), `adb reboot`. A reboot shows `sys.boot.reason=reboot,ota` — not a crash.
- **fastboot:** `device/khadas/vim3/flash.sh` (images, keeps data) / `--clean` (= `fastboot oem format` only; never
  erase/format partitions by hand; switching KeyMint implementation needs `--clean`).
- **Bootloader:** not in the OTA or flash.sh. RAM-boot every new FIP first (maskrom: Function key 3× within 2 s,
  through a USB-2.0 hub; `~/.local/bin/boot-g12.py <fip>`), then `fastboot flash bootloader
  out/target/product/vim3/u-boot_kvim3_ab_optee.bin`. Raw `mmc write`: block count = ceil(size/512) of the actual
  file, read back + `crc32`. Never `CONFIG_BOOTDELAY=0`. After U-Boot `ums` always `reset`.
- Slots: U-Boot fastboot's current-slot/set_active are unreliable — use `ro.boot.slot_suffix` + `bootctl`.
- After every boot: `adb shell input keyevent 223` (screen off, burn-in).
- Never `adb remount` / `disable-verity` (marks the slot unbootable). Test changes with bind-mounts + `stop`/`start`.
- Never restart the VHAL live (SystemUI crash loop); reboot instead. Don't `pkill` SystemUI (carwatchdog reboot).
- After a clean flash, re-push Organic Maps data (`~/android/om-world` → `/data/media/10/Android/data/app.organicmaps/files/260714`, owner = app uid).
- Serial console: Bus Pirate 5 on `/dev/ttyACM0` (UART mode), `screen /dev/ttyACM0 115200`, logs in `~/android/console-logs/`.

### Mesa / GPU loop
- Fast driver loop: edit `vendor/mesa3d-upstream` → copy changed files to `out/target/product/vim3/obj/MESON_MESA3D/<path>`
  → `PATH=~/android/teflon/venv/bin:/usr/bin:/bin ninja -C out/.../MESON_MESA3D/build src/panfrost/vulkan/libvulkan_panfrost.so`
  → `llvm-strip --strip-unneeded` → push to a new name in `/data/local/tmp` → `chcon u:object_r:same_process_hal_file:s0`.
  Check `diff -rq vendor/mesa3d-upstream/src out/.../MESON_MESA3D/src` first.
- Test without touching the system: `~/android/vk12-wip/run-ns.py <tag> <driver.so[@target]|-> <caselist> <outdir>`
  (private mount namespace). Caselists in `~/android/vk12-wip/` (subset14.txt regression, glsubset.txt GLES with
  `--deqp-surface-type=pbuffer --deqp-surface-width=256 --deqp-surface-height=256 --deqp-gl-config-name=rgba8888d24s8ms0`,
  draw.txt, recon.txt, subgroups.txt, mr.txt), full mustpass in `~/android/vk-cts-full/`. Always also
  `dmesg | grep -i "sched timeout\|fault\|INVALID_ENC"` (job faults are silent in CTS).
- Prebuilts: `VIM3_MESA_FROM_SOURCE=true m libgallium_dri libEGL_mesa libGLESv1_CM_mesa libGLESv2_mesa libgbm_mesa dri_gbm vulkan.panfrost`,
  then strip/copy per `vendor/khadas/vim3/gpu/mesa/README.md`. The build wipes and re-copies MESON_MESA3D/src at
  start — don't edit Mesa while it runs. Never flash a build made with the switch on.
- Upstream main device build: `~/android/mesa-mr-build` (host tools `~/android/mesa-mr-host-tools`).
- Profiles: `cmd gpu vkprofiles` runs in the GPU service — bind-mount the driver globally, `stop gpu; start gpu`,
  query, `umount -l`, restart gpu.
- Lessons: flush JM batches between a value and its availability flag; `suppress_prefetch` on jobs patched by other
  jobs; never load from address 0 in a shader; panvk meta passes replace the render state; JM dispatches can't be 0
  workgroups (size-1 fields).

### Kernel
- New kernel modules don't load into the running kernel (vermagic has the git SHA) → test via OTA.
- GKI update: `git rebase --onto android16-6.12.<N>_r00 $(cat .gki-fork-base) lineage-23.0`, bump `.gki-fork-base`.
- 6.18 move assessed and deferred (android17-6.18 panfrost is only uapi 1.4): memory `project_kernel_618_assessment`.

### OP-TEE / KeyMint
- TA sources: KeyMint `tee/optee/ta/keymint` (pinned) + overlays in `vendor/khadas/vim3/optee/keymint`; gatekeeper
  `vendor/khadas/vim3/optee/gatekeeper`. REE FS at `/metadata/vendor/tee`. TA `dev` self-test is opt-in
  (`VIM3_KEYMINT_TA_DEV=true`) — it deletes the production secure-deletion secret.
- OP-TEE uses dynamic SHM only (the static pool at 0x05000000 lies in a BL2-secured range → SError).
- Checks: `ls -l /dev/tee0`, `logcat -s keymint-hal-optee`, `keymint_aidl_test`.

### Gotchas
- zsh: no word-splitting of `$var`, no `declare -A` → use bash for scripts. Never `pkill -f` a pattern that matches
  your own command line.
- Gitiles returns 503 for file fetches; use a `--filter=blob:none --sparse` clone.
- Mesa worktrees must live outside the Android tree (Soong picks up Android.bp in meson subprojects).
