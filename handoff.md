# VIM3 AAOS — Handoff

**Consolidated 2026-09-29.** Replaces the older topic handoffs (`handoff-aaos-apps.md`, `handoff-keymint-optee.md`,
`handoff-nvme.md`, `handoff-mesa26-kernel618.md`, `handoff-vulkan-android-profiles.md`, `handoff-2026-09-29.md`);
their full history is in git (`git log -- device/khadas/vim3/handoff-*.md`). Longer background per topic lives in the
Claude memory notes (`~/.claude/projects/-home-glenn-android-lineage23/memory/`).

Pick work from **§2**. §1 is the current state, §3 the reference for how to work on this board.

---

## 0. RESUME HERE (state at end of 2026-10-13, evening)

**Device:** slot `_a` = 2026-10-13 build (Khadas fingerprint, no spoof, enforcing), OTA'd over a factory reset done
the same day (setup + Google sign-in done, certified; GSF IDs user 0 `4189755791819895375`, user 10
`4549620347983112952`). Friendly name **"vim 3"** (Bluetooth `bluetooth.device.default_name` via `device_name.prop`,
Settings `def_device_name_simple` overlay); codename stays `vim3`, model string unchanged (GMS identity).
Assistant role for user 10 switched to Google Assistant by hand (`cmd role add-role-holder`; build default only
applies to unset roles). Old FIDO `pm disable` workaround gone with the wipe.
**Never `fastboot oem format` right after `apply_ota`:** Virtual A/B COW lives in /data → new slot dm-verity
corrupt. Boot new slot, `bootctl mark-boot-successful`, wait `snapshotctl dump` = none, THEN oem format.

**In this build (all committed 2026-10-13):** nav/GAS toggles `nav.mk` + `nav_sku.mk` (`VIM3_NAV_APP=google|organic`,
`VIM3_GOOGLE_ASSISTANT=true|false`; default Google Maps + Assistant = SKU `gas_full`, Dicio only when Assistant off);
NlpConsentKeeper (consent persisted, verified); `com.android.oem.tokens` lib; HUR 3.5.0-beta4 with patches 0001
(APP_MAPS filter, D4 fixed+verified) + 0002 (NAVIGATE/geo/maps link filters); SystemUI D7 fixes (frameworks/base
Transitions + DisplayPolicy stale singleton window) and D8 (gmscore_app USBDEVFS disconnect ioctls denied) — NOT yet
tested on HW; RTL-SDR HAL sepolicy (`radio_loopback_device`, verified label); ACC: kernel gpio_keys of_irq_get +
meson-gpio irq_set_wake (wake source registered, verified) + VHAL resume decision/debounce/IGNITION restore (untested)
+ `sysfs_wakeup` genfs label; VHAL touch/ACC poll loops reopen on POLLHUP (was 100% CPU spin after USB re-enum —
verified 0.8%); governor conservative freq_step 25 / up_threshold 60 / sampling_down_factor 2 (cold starts −8..20%,
= performance governor); libteflon from Mesa 6fb80f48f7c; triple buffering (=3).

**Afternoon 2026-10-13 (uncommitted unless noted):**
- **Kernel moved to the CI-rebased base** (GKI android16-6.12.94, `ca3f1d12d1f3`, 32 fork commits incl. ACC fixes;
  old local state = branch `backup/pre-ci-rebase-2026-10-13`). Full brunch with it SUCCEEDED, NOT yet OTA'd/tested
  (check NVMe/PCIe, panfrost+Vulkan, Wi-Fi/BT, USB audio + touch, OP-TEE/KeyMint, ACC wake source, avc, dmesg).
- **Map card red/black = AOSP debug behaviour:** `CarLauncherViewModel.sAutoRestartOnCrash = Build.IS_USER` — on
  userdebug the card does NOT restart Maps when its task vanishes (Play update, LMK, crash, Maps opened elsewhere)
  and paints it RED. Fixed in packages/apps/Car/Launcher (always auto-restart; unused Build import dropped).
  Launcher is plain AOSP `android-16.0.0_r4` = newest tag = what the LineageOS manifest pins (nothing newer
  upstream). **Needs a fork** (gschuurman/android_packages_apps_Car_Launcher + local_manifest entry) before commit —
  ask the user before creating the GitHub repo. Workaround until then: force-stop Maps + Home.
- **Dicio removed from the image** (car.mk package + models copy, nav.mk DicioAssistantOverlay;
  `VIM3_GOOGLE_ASSISTANT=false` now = no assistant). Module/models stay in vehicle_interfaces.
- **Keyboard:** Driver user fell back to CarLatinIME after Play updated the Google car keyboard (09:10). Set back with
  `ime set --user 10 com.google.android.apps.automotive.inputmethod/.InputMethodService`. The Google keyboard module
  (vendor/google/gapps_auto Android.bp) now `overrides: ["CarLatinIME", "LatinIME"]`, so they're not installed (Google's
  car image ships only the Google keyboard). A BoardConfig filter-out does NOT work: PRODUCT_PACKAGES is readonly there.
- **Maps not listed in Play search / Mijn apps** while Assistant, keyboard and templates host are: Play's local DBs
  (localappstate.db, library.db, user 10) are identical for all four (owned, auto_update=1, updated by Play — Maps
  is now 26.09.280001.E from Play). So it's Google's server-side car catalogue (Maps shares the phone listing), not
  the build. Direct link `market://details?id=com.google.android.apps.maps` works and offers updates. Not fixable here.
- Build helper lesson: never put the pgrep pattern in a watcher's own command line (`pgrep -f "brunch vim3_nvme"`
  matched the watcher itself and dead-locked vim3-m).

**Evening 2026-10-13 — installed + tested:**
- Slot `_a` = full build incl. CI kernel 6.12.94, CarLauncher auto-restart, Dicio removal (0852925), Google keyboard
  only, `pci=pcie_bus_perf` (0bbba06). Slot `_b` = same minus pcie_bus_perf. Both booted; on `_b`: enforcing, root,
  0 avc in dmesg, NVMe/OP-TEE/keystore2/gatekeeper/panfrost up, only Google IME for user 10, no Dicio.
- **D3 display glitch SOLVED:** triggered by NVMe *writes* only (not CPU load, not reads). Not fixed by: pinned CPU
  freq, NVMe PS2 (1.7 W, tool `nvmeps` = small static aarch64 admin-cmd binary in the session scratchpad), live
  config-space MPS/MRRS 128/256/512/2048. Fixed by booting with `pci=pcie_bus_perf` (root port + SN520 MPS 256 /
  MRRS 256), user-verified with a 3 GB dd. Watch for the old heavy-I/O memory corruption that peer2peer was once
  added for (may have been bring-up/power); if oopses return, revert 0bbba06.
- **"Maps bailed out" = USB hub 1-1.4 reset**, not a Maps crash: during NVMe writes the hub (Terminus 1a40:0101:
  touch, sound card = HID keyboard, capture) drops → keyboard/touchscreen config change relaunches all activities →
  on re-attach `android.car.usb.handler/.UsbHostManagementActivity` pops over Maps. Cause NOT confirmed: 5 V sag
  (12 V → buck/boost → 5 V into VIN), hub backfeed, or an interaction with the shared USB3/PCIe PHY under PCIe
  traffic. User: will be addressed in the next hardware design revision. Software mitigations worth doing: suppress the car USB handler for these known devices.

**Next steps (in order):**
1. Hub 1-1.4 resets under NVMe writes (above): next HW revision; recheck with dd + `dmesg | grep "1-1.4: USB disconnect"`.
2. Verify map card auto-restart with the screen on (`am force-stop --user 10 com.google.android.apps.maps`).
3. **Map card does NOT auto-restart Maps (user, 2026-10-14) — open.** Pushed 2026-10-13: device tree + gapps_auto
   `b9072b4`. The CarLauncher `sAutoRestartOnCrash = true` patch is in the image but did not fix it; it stays only in
   the working tree (+ `~/android/carlauncher-autorestart.patch`). **User does NOT want a Car/Launcher fork** for
   this — find a fix in repos we already own (frameworks/base, Car/SystemUI, vehicle_interfaces) or drop the patch.
   Restart logic (packages/services/Car/car-lib `ControlledRemoteCarTaskView`): restart on `onTaskVanished` only while
   the host is visible; else `showEmbeddedTask()` on host-appeared restarts it if `getTaskInfo()==null`.
   `startActivityInternal` silently gives up (no retry) when the user isn't unlocked or the display is not ON.
   Next: reproduce with logcat (`CarTaskView|ControlledRemoteCarTaskView|MapsTaskView|wm_` + `logcat -b events`) and
   decide which case it is: (a) Maps task still present but its surface black — e.g. after the activity relaunch caused
   by the hub 1-1.4 reset (config change keyboard/touchscreen); no vanish → no restart; (b) restart attempted with
   display off, never retried; (c) start failed (intent → chooser / BAL block). Original "black screen on the home
   screen" report fits (a).
4. Hardware tests with the user: D7 (SystemUI crash recovery) / D8 (FIDO, re-enable flow), ACC desk test (POWER
   button, USB adb; procedure in the 2026-10-13 ACC notes below/C1), USB + wireless Android Auto with the phone
   (HUR 3.5.0-beta4), RTL-SDR FM/DAB under enforcing (plug into a different hub than the sound card, D2).
5. GMS LocationAccuracyNonwearableActivity still crashes (H10) — low priority.

**Remaining feature requests (user's list, in priority order):**
- **15. 15-band graphic EQ** (E2): preamp + 15 bands 25 Hz–16 kHz with response curve and Reset (like
  igogrek/equalizer-apo-ui); HAL `kEqBands` 5 → 15, new `car.eq.preamp` + persist props
  (`persist.vendor.audio.car.*`), Equalizer screen in CarAudioTuner. See memory "Car audio: vol buttons + curve/EQ".
- **16. Local music player** (E6): play the Music folder on the NVMe as a car media source (MediaBrowserService,
  shows in the car Media app / media card).
- Only after 15 + 16: **17. Radio polish** (E3: per-block DAB scan progress in the wizard, UI polish) and
  **18. FM/DAB stereo** (FM stereo pilot decode + RDS; DAB audio already stereo — verify end to end).

## 1. Current state

**Board:** Khadas VIM3 (A311D, 4 GB), Android 16 / LineageOS 23.2 AAOS, product `lineage_vim3_nvme`, boots from NVMe
(M2X + WD SN520). USB adb serial `CE88ECF8D115`. Driver user = 10, headless system user = 0.
- Slot `_b` (OTA 2026-09-30, active): same kernel/Mesa + gralloc leak fix, CarRadioApp fix, Dicio with Whisper.
- Slot `_a` (OTA 2026-09-29): kernel `6.12.93-4k-g0dd64554038e` (GKI android16-6.12.93 + fork, panfrost uapi 1.6),
  Mesa 26.2.3 + our patches `a67490155ea` (prebuilts vendor `692793c`). Slot `_b` = previous OTA (same kernel,
  older Mesa) as fallback.
- Bootloader: inline-built `u-boot_kvim3_ab_optee.bin` (U-Boot + from-source TF-A g12b + OP-TEE 4.10) in eMMC boot0.
- Security: KeyMint on OP-TEE (Rust TA), OP-TEE gatekeeper with persisted failure records, HUK from efuse, HW RNG.
  SELinux **permissive** on the running build; the next build boots **enforcing** (see G2).
- Google: Play Store works; device registered as uncertified-exempt for user 0 and user 10 (2026-10-11). Google Maps
  draws a black map — blocked on the GMS device key, see §2 H.
- GPU: GLES 3.1 (panfrost), Vulkan 1.4 (panvk v7); `VP_ANDROID_15_minimums` SUPPORTED, `16_minimums` fails only on
  protected memory. Installed-system CTS: subset14 0 Fail, reconvergence 6237/6237, GLES = baseline.
- Apps: LineageOS SetupWizard (car flow), Organic Maps (map card + nav banner, release-signed), Dicio assistant,
  CarRadioApp (FM + DAB via RTL-SDR, redesigned UI baked in), HeadUnit Revived. Google Maps currently only as a
  Play Store install for user 10; the next build adds MapsCarPrebuilt back (see G2).

**Repos** (pushed 2026-09-30; **uncommitted as of 2026-10-11:** device/khadas/vim3 SELinux/enforcing/ro.secure/SKU
changes + this handoff, vendor/google/gapps_auto `gapps-core.mk` — see G2):
| Path | Remote / branch | Notes |
|---|---|---|
| device/khadas/vim3 | gschuurman/device_khadas_vim3 `lineage-23.2` | |
| vendor/khadas/vim3 | gschuurman/vendor_khadas_vim3 `lineage-23.2` | **plain checkout** (not repo-managed) |
| kernel/khadas/vim3 | gschuurman/android_kernel_yukawa `lineage-23.0` @ 0dd64554038e9 | safety branch `backup-pre-panfrost16` |
| kernel/khadas/vim3_overlay | gschuurman/kernel_overlay_amlogic_yukawa `main` | config fragment, aic8800 |
| u-boot/khadas/vim3 | gschuurman/u-boot `master` @ 7428383a2ba | **plain checkout**; local branch name `optee-bl31-2019-ramboot-test` = master |
| bootloader/arm-trusted-firmware, bootloader/optee_os | gschuurman forks `g12b-vim3` | |
| vendor/mesa3d-upstream | gschuurman/mesa `vim3-26.2.3` @ 6fb80f48f7c | 28 commits on mesa-26.2.3 (latest two: etnaviv ML padding, not in prebuilts: the image's libteflon.so is vendor 27ccf94 from 2026-09-25, rebuild it from 6fb80f48f7c) |
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
2. **JM index min/max remainder upstream** 🖥 — repro done 2026-09-30 (`~/android/vk12-wip/indexbounds-test`):
   `indexCount = 0` indirect draws hang the GPU on upstream main (sched timeout + reset each); fixed by the null-job
   part of our 28e01e5eff9 → new MR branch `mr/panvk-jm-indexed-indirect-empty` (fbdaa938e08, local in
   ~/android/mesa-mr, TODO body like the others; notes §8). The bounded-loads part isn't MR material: indices past the
   buffer are invalid usage without robustBufferAccess2 (panvk: v11+), and the tiler faults on them anyway.
3. ~~GPU soak test~~ — DONE 2026-09-30: 12 min CarLauncher + Organic Maps + RVC settings + radio, 0 GPU faults.
   It exposed a gralloc leak: minigbm gbm_mesa left `bo->handle` at 0, so all buffers shared one refcount and were
   never freed (1.99 GB of dma-bufs, CmaFree 0, lmkd kills). Fixed in minigbm `113f471` (per-bo handle); re-soak:
   58 MB dma-bufs, CMA ~220 MB free. Also fixed a CarRadioApp NPE on every relaunch (`a987a34`). Both verified via
   bind-mount only → **need an OTA** (see G1). Soak script: `soak.sh` in the 2026-09-30 session scratchpad (cycle:
   home 8s, OM + 8 swipes, RVC settings, radio; samples dmesg/crash/GPU freq/temps/CmaFree).
4. **NPU (etnaviv + Teflon)** 🖥 — classification works (MobileNet v1 6.7 ms, 4.9× CPU). All three V7 coefficient
   encoders now pad their phantom kernels (Mesa fork 6fb80f48f7c; patch `~/android/teflon/etnaviv-v7-pad-kernel.patch`).
   **Detection is broken:** SSD MobileNet v1 and SSDLite MobileDet run (MobileDet 23.5 ms with upstream main) but give
   wrong boxes. Per-layer CPU-vs-NPU harness `~/android/npu-run/layercmp.c` (+ Mesa's per-layer test models): every
   failing layer is a standalone residual ADD, flaky ~80%; Mesa's own CI skips exactly those layers and the full
   MobileDet model (`src/etnaviv/ci/etnaviv-vipnano-skips.txt`). ADD on the CPU still leaves the full model wrong →
   cross-partition issue. Kernel etnaviv and the NPU DT node already match mainline. Next: bisect partitions of the full
   model (dump intermediate tensors), or report upstream. App exposure of libteflon (public.libraries + renderD129
   label) parked until detection works.
5. **Protected memory (Vulkan item 6)** — parked. Research only: stock BL2 decompiled
   (`~/android/optee-fip-test/re/stock_bl2_decomp.c`, Ghidra project `re/proj2`, JDK `prebuilts/jdk/jdk21`),
   DMC secure ranges at 0xff639000 programmed by BL2, Mali = DMC port 1, Amlogic's kbase has no protected-mode glue;
   whether G52 protected-mode traffic is distinguishable at the DMC is unproven. Not CTS-enforced on Android 16.
   Memory note `project_vulkan_protected_memory`.
6. **Weekly GKI CI** — reworked 2026-09-30: the workflow moved to the new **`ci` branch** (de474ea54aadf, orphan,
   only `.github` + README); `lineage-23.0` no longer carries it (397331d7446ad). It checks out `lineage-23.0` into
   `kernel/`, replays the fork range without `.github` (the old push failed: plain push after a rebase, and replayed
   workflow-file commits the default token may not push) and force-pushes with a lease. Local dry run against today's
   upstream (353 commits): 27 patches, clean `git am`, verify ok. **You: set the repo's default branch to `ci`**
   (GitHub → Settings → General → Default branch), otherwise the schedule won't find the workflow. Then run it once by
   hand (Actions → Weekly GKI rebase → Run workflow). After it pushes: build + test the new GKI kernel before the next
   OTA, and `git reset --hard gschuurman/lineage-23.0` locally.

7. **panvk renders Organic Maps wrong** 🖥 — found 2026-09-30 in the head-unit harness. OM picks Vulkan on its own
   (`support_manager.cpp:33 Init(): Renderer = Mali-G52 MC2 | Api = Vulkan | Version = API:1.4.354/Driver:26.2.3`).
   - Symptom: map card at boot fully red, or top half black, or big green/yellow/cyan garbage polygons over
     world-level geometry (North America, Caribbean, Pacific, Antarctica). Intermittent, on any launch (seen on
     session 75), not only the first.
   - A/B same boot, relaunching the card (force-stop OM + CarLauncher, HOME): Vulkan 3/3 corrupt, OpenGL ES 3.1
     (panfrost) clean. No GPU faults / sched timeouts in dmesg, no VK errors in logcat → silent wrong output, not a
     hang. Only noise: minigbm `Unsupported format 0x3b` (R10G10B10A10 probe during swapchain setup, harmless).
   - This replaces the 2026-09-29 "first-launch corruption" analysis: everything ruled out then was a GLES library
     (Mesa GL prebuilts, Mesa 25.3 GL, ANGLE) that OM wasn't using. The OM fork branch
     `aaos-issue-first-launch-corruption` blames our AAOS surface changes — that is wrong; fix or delete its README.
     Still possibly useful from that session: `debug.mesa.pan.mesa.debug nocache` raised the rate, clearing the
     shader cache once gave a clean run; scoring script (count of g>200,b<80 pixels) + screens in
     `~/android/vk12-wip/soak/`.
   - **Workaround (device only):** `VulkanForbidden=true` appended to `/data/user/10/app.organicmaps/files/settings.ini`
     (backup `/data/local/tmp/om_settings.bak`). Lost on `oem format`. For the image: add Mali-G52 + panvk to OM's
     ban list in `libs/drape/support_manager.cpp` (`IsVulkanForbidden`, kBannedDevices/kBannedConfigurations) in our
     OM fork.
   - Next steps to find the driver bug: (1) repro loop = toggle the settings.ini key + relaunch + screencap +
     pixel score; (2) swap only `vulkan.panfrost` via bind-mount (see "Mesa / GPU loop"): upstream main
     (`~/android/mesa-mr-build`) vs our fork, to see whether one of our local panvk patches causes it; then older
     Mesa panvk; (3) run OM with the Khronos validation layer (app-side misuse vs driver); (4) gfxreconstruct capture
     of a bad frame → replay offline on fixed drivers and bisect draw calls; OM drape Vulkan code is in
     `~/android/organicmaps/libs/drape/vulkan/`. Suspects given the world-level geometry: vertex/index buffer
     updates in place (buffer sync / JM batch flushing), texture-array fallback, pipeline-cache reuse.

### B. Security / firmware
1. ~~Cold-boot HUK fix~~ — confirmed fixed from flash (2026-10-11; optee_os dea8b8656).
2. **SELinux enforcing** 🚗 — started 2026-10-11. Denials from one boot (graphics, apps, storage) fixed in the tree:
   `/dev/dri/card2` + `renderD129` (etnaviv NPU) → `gpu_device`; vendor `libui.so` + `android.hardware.graphics.allocator@[234].0.so`
   → `same_process_hal_file` (libEGL_mesa/vulkan.mesa → libui → HIDL allocators; zygote/surfaceflinger/bootanim were
   denied); appdomain `sysfs_gpu` dir search + lnk_file read; vold `allowxperm` NVMe ioctl `0x4e41` on userdata;
   dontaudit gmscore_app→adbd_prop and untrusted_app_all→custom_version_prop (`sepolicy-private/`). BoardConfig
   `TARGET_SELINUX_ENFORCE := true`. **Next:** after the build, sweep with peripherals attached (audio, RTL-SDR radio,
   GNSS, camera/RVC, BT, wireless AA, OP-TEE/KeyMint, Dicio); `adb shell setenforce 0` restores function meanwhile.
   Prerequisite for a later user build (wanted, but postponed while root is still needed).
3. **Root of trust / AVB / RPMB / TA key / attestation** — state 2026-09-30: AVB on with our own key
   (`avb/vim3_avb.pem`, RSA-4096) but the device is **unlocked** (orange); KeyMint RoT from HAL props; TAs signed with
   OP-TEE's public `default_ta.pem`; no RPMB device exposed. Plan, in order (each needs a new FIP → RAM-boot → you at the
   board; nothing irreversible until step 4):
   1. Own TA signing key — **DONE in tree 2026-10-12 (uncommitted), build pending RAM-boot verify.** Keys
      `vendor/khadas/vim3/optee/keys/vim3_ta_signing.pem` (RSA-4096, private, **gitignored**) + `_pub.pem`
      (committed); user generated them in `~/tee_signing`. Wiring: `BoardConfig.mk` `VIM3_TA_SIGN_KEY` (abspath of
      the gitignored key, empty if absent → stock key) → `bootloader/Android.mk` passes it as arg 8 to
      `build-g12b-optee-fip.sh`, which builds OP-TEE core with `TA_SIGN_KEY=/TA_PUBLIC_KEY=<key>` and copies the key
      over the exported dev kit's `keys/default_ta.pem` so **every** from-source TA (KeyMint, gatekeeper, xtest) is
      re-signed with it — downstream scripts (`build-optee-ta.sh`, `build-keymint-ta.sh`) unchanged. All-or-nothing:
      OP-TEE loads only TAs signed with this key, and all our TAs are built from source each build, so they stay
      consistent. TA secure storage is keyed by HUK+UUID, not the signing key → `/metadata/vendor/tee` stays
      readable, no re-provision. Key added as a FIP prerequisite so it retriggers the build. **Verify at board:**
      RAM-boot the new `u-boot_kvim3_ab_optee.bin` (do NOT flash first), confirm `/dev/tee0`, `keymint_aidl_test`,
      lockscreen (gatekeeper) and FBE unlock all still work; power-cycle reverts to the current good FIP if a TA
      fails to load.
   2. RoT from the bootloader: U-Boot passes boot state + vbmeta digest + OS version/patch level to OP-TEE (DT node
      or SMC), KeyMint TA reads it instead of HAL props (today `…/keymint/aidl/default/hal/lib.rs` reads them from
      `ro.boot.vbmeta.*`/`verifiedbootstate` — userspace, and `public_key_digest` is missing → zero key).
   3. Lock/unlock mechanism — **IMPLEMENTED in tree 2026-10-12 (uncommitted U-Boot edits), build/RAM-boot verify
      pending.** The green/orange decision in `boot/bootmeth_android.c` was already complete and gated solely on
      `read_is_device_unlocked()`, and the baked-in `avb_root_pub` (common/avb_verify.c) is confirmed **byte-identical
      to our `device/khadas/vim3/avb/vim3_avb_pub.bin`** — so locking genuinely verifies images against our key and
      earns green honestly. Changes (u-boot/khadas/vim3, a plain checkout):
      - `common/avb_verify.c` `read_is_device_unlocked()`: reads `env_get_yesno("avb_device_locked")` (added
        `#include <env.h>`); default (unset) = unlocked, so an un-provisioned board is unchanged until locked.
      - `drivers/fastboot/fb_command.c` + `include/fastboot.h`: new `fastboot flashing lock` / `flashing unlock`
        commands (enum entries, table entries gated on `CONFIG_IS_ENABLED(AVB_VERIFY)`, handlers `flashing_lock`/
        `flashing_unlock`) that set `avb_device_locked` in the env (`CONFIG_ENV_IS_IN_MMC`, persists) + `env_save()`.
      - **DEV DEVIATION (requested):** `flashing unlock` does **NOT wipe userdata**, so the full system can be tested
        with data intact. This breaks Android's unlock-wipe protection; re-enable before any production/locked ship.
      - Validated: standalone U-Boot build compiles clean and `u-boot.bin` contains the new commands/strings.
      - Storage caveat: env is not tamper-resistant → RPMB-backed lock state is the step-4 hardening.
      - **Userdata-intact implications (checked):** FBE class keys wrap via HUK + credential (gatekeeper), not the
        verified-boot RoT, so flipping lock state does NOT orphan userdata; and TA secure storage is HUK+UUID-keyed so
        the step-1 TA-key swap keeps `/metadata/vendor/tee` readable. Locking flips KeyMint's RoT device_locked→locked
        and verifiedbootstate→green (correct, feeds attestation) without re-wrapping FBE → data survives.
      - **Build note:** uncommitted U-Boot edits don't retrigger the Android u-boot/FIP module (keyed on `.git`
        refs); force it with `rm -rf $OUT/obj/UBOOT_OBJ $OUT/obj/UBOOT_OPTEE_OBJ` + the FIP intermediate, then
        `m u-boot_kvim3_ab_optee`.
      - **Verify at board:** RAM-boot the FIP, `fastboot flashing lock` → reboot → `getprop ro.boot.verifiedbootstate`
        = `green` and `ro.boot.vbmeta.device_state` = `locked`; `fastboot flashing unlock` → back to `orange`/`unlocked`
        with data intact. A locked device that fails verification refuses to boot — keep the unlock path proven first.
      - **2026-10-12 HW result: step 1 VERIFIED, lock BRICKED the device (accepted risk).** Flashed the new FIP
        (`fastboot flash bootloader`, no RAM-boot — user accepted brick, has maskrom) → booted slot `_a` ORANGE/unlocked,
        **KeyMint + gatekeeper + keystore2 all running, FBE unlocked, 0 denials** → step 1 (own TA key) proven on HW.
        Then `fastboot flashing lock` → OKAY → reboot → **device does not enumerate at all** (no adb, no fastboot, no
        USB) = hard brick, needs maskrom.
      - **Cause analysis (not fully pinned, needs serial which user lacks right now):** NOT the AVB key — the signing
        key's public key (`avbtool extract_public_key device/khadas/vim3/avb/vim3_avb.pem`) is byte-identical to
        baked-in `avb_root_pub`/`vim3_avb_pub.bin`. `read_rollback_index` returns 0/OK without the TA, so not rollback.
        Two live hypotheses: (A) strict AVB (`allow_verification_error=false`) of `_a` returns non-OK so U-Boot refuses
        and reset-loops before USB (a clean refusal should still give fastboot — it didn't, which argues against A);
        (B) `env_save()` to eMMC **boot1** (CONFIG_ENV_MMC_EMMC_HW_PARTITION=2) during `flashing lock` corrupted a
        boot-critical region → BL2/U-Boot dies before USB. `_a` boots fine UNLOCKED (orange tolerates
        ERROR_VERIFICATION/PUBLIC_KEY_REJECTED), only LOCKED dies.
      - **Recovery (maskrom, physical):** Function ×3 via USB-2.0 hub → `~/.local/bin/boot-g12.py <FIP>` RAM-boots
        U-Boot bypassing eMMC → get to fastboot → `fastboot flashing unlock` (clears the env flag) and re-`fastboot
        flash bootloader`. Safest to RAM-boot a recovery FIP that forces unlocked (revert the read_is_device_unlocked
        env read) so it boots regardless of the stuck lock flag.
      - **Build AVB proven clean (host, 2026-10-12):** `avbtool verify_image --image vbmeta.img --key vim3_avb.pem
        --follow_chain_partitions` verifies the whole chain OK (vbmeta struct; chained boot/init_boot/vbmeta_system_dlkm/
        vbmeta_vendor_dlkm all our key; hash/hashtree for dtbo/vendor_boot/system/vendor/system_dlkm/vendor_dlkm). So
        the lock brick is NOT a signing/vbmeta fault — it is device-side U-Boot `avb_slot_verify` returning non-OK
        (tolerated→orange unlocked, fatal→refuse locked). `requested_partitions = {boot, vendor_boot}`; NVMe reads work
        (unlocked produced a result, so not an IO error). Exact failing result is unknown without serial (it prints via
        `str_avb_slot_error`). Host-side causes RULED OUT: key matches; `read_rollback_index` returns 0/OK; avbtool
        passes the whole chain; `read/write_persistent_value` + their op-wiring are both inside `#ifdef
        CONFIG_OPTEE_TA_AVB` (NOT set) → those ops are NULL and uninvolved for our non-persistent-digest descriptors.
        The failing result must be read from the serial console on the next locked boot.
      - **Before locking again (MANDATORY): attach the serial console** (Bus Pirate on /dev/ttyACM0, 115200) to read
        the `avb_slot_verify` result string on the locked boot. Do NOT lock blind again.
      - **Before locking again:** confirm `_a` strictly verifies
        (RESULT_OK, not just tolerated-orange); and reconsider lock-flag storage if env_save to boot1 is implicated
        (hypothesis B) — e.g. a dedicated store, not the eMMC env.
      - Not done: `fastboot getvar unlocked`/`get_unlock_ability`; and the latent `avb_verify.c` `AVB_ORANGE`
        fall-through to `AVB_RED` (missing `break`) in `avb_set_state` is still there (cosmetic — ORANGE still sets its
        cmdline before falling through).
   4. RPMB rollback protection (**irreversible**: programs the eMMC RPMB key once): expose `mmcblk0rpmb`, OP-TEE
      `CFG_RPMB_FS=y` with a derived key. Decide first whether the eMMC stays in use.
   5. Attestation: without Google RKP provisioning only a self-signed chain is possible (limited value).
4. ~~U-Boot USB3/PCIe PHY fix~~ (`bd205d88d25`, `68e5d22a4e8`) — confirmed in practice (2026-09-30): the NVMe was
   provisioned and flashed through U-Boot fastboot (incl. `oem format`, `7428383a2ba`) and has booted from it since —
   the exact path (`fastboot usb 0` killing the PCIe link) the fix addresses.

### C. Car features — verify on hardware
1. **ACC-line suspend-to-RAM** 🚗 — VHAL implementation (vehicle_interfaces `1ba64cf`) + kernel wakeup-source DTS;
   never tested (needs the Pico / ACC line, or the power button as stand-in).
2. **OpenHeadunit** (USB Android Auto picker, `packages/apps/OpenHeadunit`) 🚗 — wired into the build but untested;
   currently not installed on the board.
3. **Wireless Android Auto** 🚗 — live test with the phone as P2P group owner (both BoardConfig fixes are in).
4. **Rear view camera with the real grabber** 🚗 — 2026-09-30, MS210x grabber attached, no camera on it: camera 100
   opens, Camera2 streams 720×480@30 (381 frames, two buffer-request timeouts at start only), black image as expected,
   clean close. Settings panel fixed (RearViewCamera `1dd9592`, bind-mount verified, needs OTA): back button over the
   image, back arrow in the panel header, list settings as tap-to-apply pick lists (the car UI list page only saved on
   Back, and the panel's back handler swallowed it → choices were lost). Open: a real camera image, and the reverse
   trigger itself: `cmd car_service inject-vhal-event GEAR_SELECTION 2` does NOT start it — GearMonitorService polls
   the VHAL, which reads only the GPIO (`ro.vendor.vehicle.gear.gpio.*` = gpiochip0/51, header pin 32 high = reverse).
5. **Native FM radio (RTL-SDR)** 🚗 — live FM listening test; stereo decode and RDS are TODO.
6. **DAB** 🚗 — app-side "tune to default" built+deployed but the verify tap was never done; physical antenna unplug
   path untested.

### D. Bugs
0. ~~Organic Maps first-launch corruption~~ — root-caused 2026-09-30 to panvk (Vulkan), see A.7.
1. ~~Bluetooth~~ — appears fixed (2026-10-11, user: A2DP stop / call audio / MapClient after reboot OK; wireless AA
   stays under C3). Watch for regressions. History: 2026-09-30 with the Pixel 9 Pro XL: pairing, A2DP sink, AVRCP, HFP, MAP, PBAP and PAN
   (internet via the phone's Bluetooth tethering) all connect — verified via bind-mount + setprop, **needs an OTA**:
   - HFP never connected: RFCOMM collision left the HF client block half-open and every later incoming connection was
     refused. Fixed in the BT fork `8c79330045` (take the block over, drop our own outgoing port).
   - BT process abort on reconnect ("No prior connection request", classic_impl.h): the BCM4359 reports our page
     as failed + the phone's as success; fixed `a10e0318f6` (unmatched success = incoming connection).
   - The head unit also advertised phone-side roles (HFP AG, LE Audio servers, …) → collisions; removed in device
     `1c2f217` (car_product/properties/bluetooth.prop provides the car-side roles).
   - MapClient crash-loop fix `ed6655a2ec`.
2. **DAB choppy audio** 🚗 — RTL-SDR (4 MB/s) starves the 8-ch USB sound card on the shared hub 1-1.4: move the
   dongle to another hub (audio code proven clean).
3. ~~**Video glitching**~~ — SOLVED 2026-10-13: NVMe writes; fixed by `pci=pcie_bus_perf` (§0). History: seen 2026-09-21 under first-boot load (CmaFree ~6 MB of 576 MB during dexopt/Play
   Store) and again 2026-10-12 (user). 2026-10-12 check: no meson-drm/HDMI errors in dmesg, CmaFree 230 of 768 MB,
   no allocation failures (only two `binder_alloc ... no address space`, unrelated). Still needs a symptom
   description/photo to split: sparkles/snow/lines/whole-frame dropouts = HDMI signal (cable/connector/EMI, try
   another cable + ferrite); wrong/stale blocks or one app only (e.g. map) = GPU/driver (cf. panvk OM corruption A.7).
   RAM corruption would also show crashes/reboots — none seen.
   **2026-10-12 later:** new HDMI cable/routing → still twitches. **scrcpy (SurfaceFlinger composition) shows NO
   glitch** → frames are clean, problem is after composition (scanout). Ruled out: cable, late vsync IRQ (pinned
   IRQ 43 `meson` to CPU5 via `/proc/irq/43/smp_affinity` → no change, reverted), OP-TEE memory overlap
   (`/proc/iomem` reserves 0x05000000-0x072fffff = TZDRAM+SHM; CMA at 0xbda5a000+). All layers are CLIENT
   (GPU-composed), only the FB target is scanned out. meson_crtc.c updates the OSD1 canvas address in the vsync IRQ
   (takes effect immediately) and sends the flip event in the same IRQ. Panel = Waveshare 7" HDMI LCD (C) (touch
   WS170120, EDID ADA/0004, one DTD 1024x600@59.85 49.00 MHz, htotal 1312, vtotal 624 → **vblank only 24 lines /
   0.64 ms**); output matches exactly (49.10 MHz PLL). Remaining suspects: FB-target reuse race with only 2 buffers
   (test: triple buffer, in tree), display underflow (DDR bandwidth), panel receiver. Next if triple buffer
   doesn't help: static-screen test (no redraws), another HDMI monitor. First seen 2026-09-21 = day the TF-A+OP-TEE
   FIP went in (BL2/DDR blob unchanged though).
4. **Default app choice ("Always") doesn't stick** 🖥 — reported 2026-10-12: choosing Google Maps or Organic Maps as
   default with "Always" in the resolver is forgotten; the next time the home screen opens (CarLauncher map card) the
   same chooser appears again. **Mechanism found (2026-10-12):** "Always" IS saved (user 10 package-restrictions:
   APP_MAPS → Google Maps, `always="true"`, set = OM MapCardActivity + Google Maps + MapsPlaceholder). But any APP_MAPS
   query whose result is not a subset of that set makes PMS downgrade it in memory to a "last chosen" entry
   (`ComputerEngine.java` ~3474, logs `Result set changed, dropping preferred activity`; debug-resolution then shows
   `Skipping mAlways=false entry` → CarResolverActivity in the map card). Extra candidate: OpenHeadunit/HUR
   `MainActivity` declares MAIN + APP_MAPS + APP_NAVIGATION **without** DEFAULT, so it shows up in every query without
   `MATCH_DEFAULT_ONLY` (e.g. `cmd package resolve-activity` with flags 0 — which also triggered it once during the
   investigation). CarLauncher itself resolves with MATCH_DEFAULT_ONLY; the real-world flags-0 caller is still unknown
   (not in Car SystemUI/Launcher/CarService/Settings source → likely a prebuilt). Disk keeps `always=true`, so a reboot
   restores it until the next such query. Fix options: drop APP_MAPS/APP_NAVIGATION from HUR's filter (repackage
   `vendor/gschuurman/vehicle_interfaces/automotive/openheadunit/OpenHeadunit.apk` or patch the source), and/or find
   the caller: pick "Always", then `logcat | grep "Result set changed"` and match the pid/time against the event log.

5. **Brightness control dead after boot** 🚗 (note 2026-10-12: the backlight chip number moves between boots —
   `pwmchip1/pwm1` one boot, `pwmchip0/pwm1` the next; both scripts look it up by the ffd19000 address) — 2026-10-12: backlight PWM (`/sys/class/pwm/pwmchip1/pwm1`) found
   with period 3666 ns + enable 0 after boot although `init.vehicle.sh` (exit 0 at ~24 s) and the VHAL's `initPwm()`
   both write 30518 / 1; every brightness write (duty > period) is then rejected by the kernel. Reproduced on reboot;
   a display off/on cycle later restored it. Writer of 3666 not found (nothing in our tree). Fix pushed:
   vehicle_interfaces `26e11f0` — `writePwm()` re-runs `initPwm()` when period/enable are wrong. Not on the device
   until the next OTA; verify, and still find the culprit (`cat period` right after boot, then after each step).
6. **App performance / Google Maps laggy** 🖥 — reported 2026-10-12. To do: profile Maps (`dumpsys gfxinfo
   com.google.android.apps.maps framestats`, Perfetto trace with sched/freq/gpu), check CPU governor + big-core
   use, GPU freq (min is pinned 285 MHz in init.vim3.rc), whether Maps runs GL or Vulkan (panvk) and on which
   renderer (`dumpsys SurfaceFlinger`, HWC overlay vs client composition), memory/LMK pressure (4 GB, zram),
   dex2oat state of Maps (`cmd package compile -m speed-profile`), and app density/scaling (CarDisplayCompat 0.7).

7. **SystemUI crash → RescueParty reboot** 🖥 — 2026-10-12: one SystemUI crash in WM Shell
   (`Transitions.onFinish` IndexOutOfBounds -1, under post-boot load while launching an app) → every restart fails
   `KeyguardService` create: `BadTokenException ... another window of type 2040 already exists`
   (`SystemUIOverlayWindowController.attach`): the dead instance's car overlay window is still registered → 5 crashes
   → `reboot,rescueparty`. Any single SystemUI crash becomes a reboot. Look at CarSystemUI overlay window removal on
   process death / WM token cleanup (packages/apps/Car/SystemUI fork). dropbox `system_app_crash` has both traces.
8. **GMS FIDO grabs USB HID devices → touchscreen drops, all activities relaunch** 🖥 — 2026-10-12: when a sign-in
   page offers a passkey, GMS `Fido2FullScreenActivity`/`UsbRequestController` probes every USB device for a security
   key and detaches the kernel HID driver (`usbfs: process … did not claim interface`): WaveShare touchscreen and the
   USB sound card's HID buttons disappear/reappear every ~2.3 s → global config change → every activity relaunches →
   `CarMinuteMaidActivity` (sign-in) crashes "Required value was null". Workaround: disable that activity for user 10.
   Real fix ideas: deny GMS USB host access to HID devices (USB permission/denylist), or make those devices not look
   like FIDO candidates.

### E. Features to implement
1. ~~Bake the CarRadioApp redesign into the image~~ — already done (checked 2026-09-29): repo-managed
   (`gschuurman/packages_apps_Car_Radio` `main` @ 9f792f5, manifest line 12), built into `/system/priv-app/CarRadioApp`
   (on-device APK == build output), installed for driver user 10. Open: DAB reception (see C6/D2).
2. ~~Car audio EQ + balance/fader~~ — DONE 2026-09-29 (audio HAL 80b5a81, device f8be257/b778e6f, vehicle_interfaces
   9d106fc): parameter parser service, 5-band EQ in the HAL, persist.vendor.audio.car.*, Settings → Sound entries.
   Listening test DONE 2026-09-30 in the harness (USB card): EQ audible on a steady tone (a cycling tone sweep was too
   short to judge), balance + fader follow. Tone tool: `Tone.java` in the 2026-09-30 session scratchpad (AudioTrack
   USAGE_MEDIA via `CLASSPATH=/data/local/tmp/tone.dex app_process /system/bin Tone <secs> <Hz>[,<Hz>...]`).
   **Next: graphic EQ UI** like github.com/igogrek/equalizer-apo-ui (user's reference, 2026-09-30): preamp slider +
   15 vertical band sliders at 25/40/63/100/160/250/400/630/1k/1.6k/2.5k/4k/6.3k/10k/16k Hz with the combined
   response curve drawn filled behind them, Reset button. Needs HAL: `PrimaryMixer::kEqBands` 5 → 15 at those
   frequencies (Q ≈ 2.15 for 2/3-octave spacing), a manual `car.eq.preamp` (dB) alongside the auto headroom,
   persist props for the new bands (migrate the 5 old ones or reset). UI: new Equalizer screen in CarAudioTuner
   (touch-sized for 1024×600, curve from `CarEqualizer::responseDb` logic reimplemented in the app).
3. **Radio polish** — stereo/RDS (C5), per-block DAB scan progress in the scan wizard.
4. **Whisper speech recognition in Dicio** 🚗 — built 2026-09-30 for mixed-language commands (Dutch with English
   names; Vosk models are one language each). Dicio fork `gschuurman/dicio-android` `preinstalled-models` @ 3a6a897
   (whisper.cpp submodule + JNI, `WhisperInputDevice` with energy end-of-speech, default when
   `/product/usr/share/dicio/whisper/*.bin` exists), vehicle_interfaces 04ca5de (APK + ggml-base-q5_1, 60 MB).
   ~1.3 s per command on the A311D (greedy, audio_ctx ≥ 384, 64-token cap). Verified via bind-mount up to the
   microphone (none attached). Open: **live test** with a mic — set Dicio's language to Dutch (it follows the system
   language, English on the board), say "Speel Bohemian Rhapsody van Queen"; tune the end-of-speech thresholds in car
   noise if needed. Benchmarks + test tool: `~/android/whisper`, `dicio_whisper_test` (app/src/main/cpp).
5. **Phone GPS into AAOS** — deferred; options A (BT-NMEA → mock provider) / B (2nd NMEA source in the GNSS HAL),
   memory `project_phone_gps_into_aaos`.

6. **Local music player** (wish, 2026-09-30) — play music from the Music folder on the NVMe (plenty of space);
   should show up as a media source in the car media UI (MediaBrowserService), like the radio.

### H. Google Maps / GMS (2026-10-11) — detail in `~/android/lineage23/handoff.md`
1. **Certification — fixed without a build.** GMS certification is per Android user: Driver (user 10) GSF ID
   `4293328587998073264`, user 0 `4567343427325516544`; both registered at google.com/android/uncertified, then
   `adb shell am broadcast --user <u> -a android.server.checkin.CHECKIN` → `uncertified_status=1`. Re-register both
   after a wipe. The Android-15 `gms_spoof_*.prop` route stays rejected (`0a998b8`).
2. **Black map = no location → no tiles.** The u-blox clone has no fix. Google location accuracy consent is
   `network_location_opt_in` in `content://com.google.settings/partner`; GmsCoreAuto 26.18.33 resets it to 0 on every
   location-module init (`LocationModuleInitIntentOperation` → `bpei.k(ctx,false,…)`), and its settings screen crashes
   in the car UI. Workaround until the next GMS restart:
   `adb shell content insert --user 10 --uri content://com.google.settings/partner --bind name:s:network_location_opt_in --bind value:s:1`.
3. **Real blocker: no GMS device key.** The location server answers "Application credential header not valid"
   (`X-Goog-Spatula`, from `IAppCertService`), because `android.googleapis.com/auth/devicekey` (DroidGuard-attested)
   returns an HTML error. Runtime `setenforce 1` didn't help. Suspects: `ro.boot.selinux=permissive`, `ro.secure=0`
   (both changed in the next build), unlocked bootloader (B3), userdebug/test-keys.
   **2026-10-12 build (slot `_b`) did NOT fix it:** enforcing, `ro.secure=1`, `ro.adb.secure=0` (product),
   GAS SKU features (google.software.gas.maps/playstore/components), `ro.com.google.locationfeatures=1`, GMS framework
   RRO (persistentData/integrity/activity recognizer → gms), A15 GAS configs + default grants — devicekey still
   returns HTML ("Parsing Exception", ~80 ms). Remaining suspects: KeyMint attestation chain (AOSP software root in
   our TA, `tee/optee/ta/keymint/src/software/attest.rs`), unlocked/orange bootloader (B3), userdebug/test-keys,
   missing `ro.boot.vbmeta.public_key_digest` / `androidboot.flash.locked` (U-Boot). Cheapest next test: KeyMint
   without attestation keys.
5. **2026-10-12 results.** Device now locked + green (`5e5d3b9cd76`), enforcing, `ro.secure=1`. After reflash +
   account setup Maps draws tiles and routes with live traffic; new GSF IDs user 10 `4555187659033306144`, user 0
   `4468879502998419076`, both `uncertified_status=1` without registering. Device key STILL refused, so network
   location stays dead (consent set → provider enabled, Wi-Fi scans sent, GLS rejects the header). Play Integrity
   (SPIC, sideloaded with `verifier_verify_adb_installs=0`): no `deviceRecognitionVerdict` at all, everything
   UNEVALUATED. Runtime spoof test (resetprop, all partitions → A15 sdk_gcar identity, framework restart, GMS NOT
   cleared): no change. Play attaches a KeyMint key attestation (our OP-TEE root) to each request.
   Mock location works fully in Maps (`cmd location providers add-test-provider gps|network|fused` +
   `set-test-provider-location`; lost on framework restart).
6. **Build-time spoof test (2026-10-12, uncommitted):** `gcar_identity.mk` (included last by lineage_vim3_nvme.mk,
   flag `VIM3_GCAR_IDENTITY` read by device.mk) + `gcar_identity.prop` (BoardConfig, `PRODUCT_BRAND=google`) make
   every partition report the Google car image identity (values extracted from its partitions; SDK 36 / release 16 /
   patch kept real — KeyMint rollback otherwise). Verified on the generated build.props, no Khadas leaks. Plan: OTA →
   clear GMS/GSF/Play Store data (users 0+10) → reboot → test devicekey + Play Integrity. If it fails, the
   attestation chain is the wall; Magisk-in-init_boot + PIF can at best reach basic integrity (no leaked keyboxes).
7. **Build-time spoof RESULT (2026-10-12): no effect.** Spoofed build on slot `_b`, verified on-device (no Khadas
   leaks), `pm clear` GMS/GSF/Play (users 0+10) + reboot, later full `fastboot oem format`: devicekey still refused
   (same Parsing Exception), Play Integrity still no deviceRecognitionVerdict. User 10 kept GSF ID
   `4555187659033306144` across the pm clear. Conclusion: build identity is not the blocker → KeyMint attestation
   chain (own OP-TEE root). Magisk+PIF spoofs the same values → not expected to help. And the spoof BROKE the QR
   sign-in (H8) → spoof removed from the product (files kept).
8. **QR ("sign in with your phone") sign-in** — worked with the real identity on builds WITHOUT CompanionDevice
   (that APK was never in gapps_auto, only its googlecarui RRO); with the emulator identity there was no QR → the
   spoofed identity is what hid it (server-side choice). GMS also logs `SecondDeviceSignInUrlFeature: Connector could
   not connect` (no service for `com.google.android.connecteddevice.api.BIND_FEATURE_COORDINATOR_FG` = Google
   CompanionDevice, product/priv-app/CompanionDevice in the A15 car image); that is NOT what hides the QR.
   adb-installing CompanionDevice made the feature connect but brought no QR on the spoofed build. Expect the QR back
   with the real identity; shipping CompanionDevice is optional (phone companion features), not required for QR.
9. Follow-ups: persist the consent at boot once the key works; decide Google Maps vs Organic Maps as default nav
   (`gas_maps_playstore` makes Maps the default); Google keyboard gets SERVICE_INVALID (not investigated).
10. **2026-10-13 status.** Consent persistence DONE + verified on HW: `NlpConsentKeeper` (device/khadas/vim3/apps,
    system uid, persistent) rewrites `network_location_opt_in=1` ~2 s after GMS resets it (tested: force-stop GMS +
    phenotype COMMITTED → 0 → 1, stable). Keyboard SERVICE_INVALID did not reproduce on the fresh build.
    **LocationAccuracyNonwearableActivity still crashes** even with `com.android.oem.tokens`
    (oem_token_shared_lib_vendor, now linked into GMS + SystemUI). Real error = "Failed to resolve attribute"
    for TextView textColor `?attr/common_automotive_color_on_surface_variant` → `?attr/oemColorOnSurfaceVariant`
    (unset in the theme GMS picks in code); the `0x7f06004a` NotFoundException is a red herring thrown by
    `Resources.Theme.toString()` while building that error message. GMS's `TokenInstaller` lifecycle callback only
    applies tokens if the theme has `oemTokenOverrideEnabled=true` (only `OemTokensBase` sets it). Google's
    `demo-token-rro-vendor.apk` (oem.brand.model.rro) is an EMPTY overlay — not the missing piece. Next: find which
    theme the activity sets in code and where Google's image gets oemTokenOverrideEnabled/oem* attrs (phenotype flag?
    car framework theme?). Low priority — consent is persisted without this screen.

### F. Housekeeping
1. ~~Commit this handoff~~, ~~kernel `tmp_pack_*` cleanup~~ — both done.

### G. Next build
1. ~~OTA with the 2026-09-30 fixes~~ — DONE: installed to slot `_b` and verified (gralloc leak gone: 36 MB dma-buf
   after 10 app switches, CMA 200 MB free; CarRadioApp + Dicio/Whisper from the image). Slot `_a` = previous build.
2. **Next build (planned 2026-10-12, uncommitted)** — `brunch vim3_nvme`, OTA via update_engine:
   - etnaviv sysfs label (`sepolicy/genfs_contexts`, fcb19af, committed).
   - SELinux fixes from B2 + **boot enforcing** (`TARGET_SELINUX_ENFORCE := true`).
   - `developer.mk`: `ro.secure=0` removed; root comes from `service.adb.root=1` (adbd is compiled debuggable on
     userdebug). If `adb shell id` isn't uid 0 after boot, roll back / restore `ro.secure=0`.
   - GAS SKU `gas_maps_playstore` (was `gas_playstore`) + `vendor/google/gapps_auto/gapps-core.mk`: MapsCarPrebuilt
     (priv-app) + its privapp perms, GoogleTemplatesHost + car-UI RRO, `gas_maps_playstore_overlay` instead of the
     gas_playstore + car_launcher overlays.
   - **Installed 2026-10-12 on slot `_a`: adb was off after boot.** With enforcing, init drops `vendor/build.prop`
     props that `vendor_init` may not set, so `ro.adb.secure=0`, `persist.sys.usb.config=adb`, `service.adb.root=1`
     and the tcp port were lost (permissive had hidden this). Also: Lineage's common.mk sets `ro.adb.secure=1` in
     system_ext and `ro.debuggable=0` on userdebug (adbd still allows `adb root` via compiled-in ANDROID_DEBUGGABLE).
     Fixed in tree: `developer.mk` uses `PRODUCT_PRODUCT_PROPERTIES` (product loads last, init context) → needs a
     rebuild. 17 vendor props were dropped in total (`logcat | grep "Do not have permissions to set"`): besides the
     adb ones also `persist.sys.timezone`, `persist.sys.powerstats.enabled` (developer.mk), `android.car.drawer.unlimited`,
     `android.car.hvac.demo`, `com.android.car.radio.demo[.dual]` (car.mk), `audio.usb.enabled`,
     `persist.audio.car_audio_service.enabled` (hal/audio/audio_properties.mk), `persist.wm.debug.predictive_back[_anim]`,
     `ro.product.device` (device.mk), `ro.surface_flinger.max_graphics_buffers` (hal/graphics/device_vendor.mk),
     `service.sf.prime_shader_cache` (source not found yet), `wifi.supplicant_scan_interval`
     (hal/connectivity/device_vendor.mk) → move to system/product props (or add vendor property_contexts where the
     prop really is vendor-owned). Only developer.mk is fixed so far.
   - USB adb on this board only comes up via init (`init.usb.rc:109` copies `persist.sys.usb.config` →
     `sys.usb.config` at boot; vendor rc then sets role `device`). The gadget HAL
     (hardware/amlogic/yukawa/usb/aidl) is a stub that never answers for ADB → toggling USB debugging in Settings
     does nothing ("Set functions timed out! no reply from usb hal"). Runtime recovery: wireless debugging →
     `adb root` → `setprop sys.usb.config adb`, then add the host key to `/data/misc/adb/adb_keys`.
   - Results on slot `_a`: Enforcing, `ro.secure=1`, SKU `gas_maps_playstore`, 0 avc denials so far (auditd works; no
     peripherals exercised yet). **GMS device key still fails** (`AppCertManager Exception while requesting key`).
     Device got IP 192.168.1.172 this boot.
   - OpenHeadunit (HUR) removed from `car.mk` PRODUCT_PACKAGES for now (D4 cause; module kept). Wireless/USB
     Android Auto (C2/C3) needs it back later.
   - After boot check: root, `getenforce`, new denials (sweep), `ro.secure`=1, `ro.boot.hardware.sku`, GMS device key
     (`logcat | grep -E "DeviceKeyStore|AppCertManager|Storing device key"`), then H2 workaround + Maps tiles.

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

### Dicio (prebuilt APK from our fork)
- `~/android/dicio-src` (branch `preinstalled-models`, submodules!): `JAVA_HOME=prebuilts/jdk/jdk21/linux-x86
  ANDROID_HOME=~/android/sdk ./gradlew assembleRelease -Porg.gradle.java.installations.paths=~/android/jdk17`
  (a subproject needs a JDK 17 toolchain), copy `app/build/outputs/apk/release/app-release-unsigned.apk` to
  `vendor/gschuurman/vehicle_interfaces/automotive/dicio/Dicio.apk`, `m Dicio` (signs + stores native libs
  uncompressed; output `system/product/app/Dicio/Dicio.apk`). Test: bind-mount it over `/product/app/Dicio/Dicio.apk`
  (`umount -l` first if already mounted) + `stop; start`.

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

### Reverse engineering GMS / APKs
- REA (github.com/morluto/rea) CLI, local install: `~/.local/bin/rea` (Node 24 in `~/.local/opt/node`, JDK
  `prebuilts/jdk/jdk21/linux-x86`, JADX bridge `~/.local/opt/rea/jadx-headless-mcp-0.7.1-all.jar`). Obfuscated classes
  are `defpackage.<name>`: `rea inspect-android-method <apk> defpackage.bpei k --json`, `trace-android-references`,
  `search-android-classes`. Huge methods JADX can't decompile → `out/host/linux-x86/bin/dexdump -d classesN.dex`.
- GMS APK: `adb pull /system/product/priv-app/com_google_android_gms/com_google_android_gms.apk`.

### Gotchas
- zsh: no word-splitting of `$var`, no `declare -A` → use bash for scripts. Never `pkill -f` a pattern that matches
  your own command line.
- Gitiles returns 503 for file fetches; use a `--filter=blob:none --sparse` clone.
- Mesa worktrees must live outside the Android tree (Soong picks up Android.bp in meson subprojects).
