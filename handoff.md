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
- Slot `_b` (OTA 2026-09-30, active): same kernel/Mesa + gralloc leak fix, CarRadioApp fix, Dicio with Whisper.
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
| vendor/mesa3d-upstream | gschuurman/mesa `vim3-26.2.3` @ 6fb80f48f7c | 28 commits on mesa-26.2.3 (latest two: etnaviv ML padding, not in prebuilts; Teflon isn't in the image) |
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
1. **Cold-boot HUK fix** 🖥 — optee_os dea8b8656 verified via RAM-boot only; confirm from flash: power off for
   minutes, boot, grep serial for "huk: uncached view of the efuse buffer was stale", keystore2 up.
2. **SELinux enforcing** 🚗 — sweep needs the peripherals attached (audio, RTL-SDR, GNSS, camera, BT).
3. **Root of trust / AVB / RPMB / TA key / attestation** — state 2026-09-30: AVB on with our own key
   (`avb/vim3_avb.pem`, RSA-4096) but the device is **unlocked** (orange); KeyMint RoT from HAL props; TAs signed with
   OP-TEE's public `default_ta.pem`; no RPMB device exposed. Plan, in order (each needs a new FIP → RAM-boot → you at the
   board; nothing irreversible until step 4):
   1. Own TA signing key: generate `optee/keys/vim3_ta.pem` (keep private key out of git or encrypted), build OP-TEE
      with `TA_SIGN_KEY`/`TA_PUBLIC_KEY`, re-sign KeyMint + gatekeeper TAs.
   2. RoT from the bootloader: U-Boot passes boot state + vbmeta digest + OS version/patch level to OP-TEE (DT node
      or SMC), KeyMint TA reads it instead of HAL props.
   3. Lock with our AVB key: embed the vbmeta public key in U-Boot, `fastboot flashing lock` (wipes data) → green
      state; keep the unlock path tested before locking.
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

### F. Housekeeping
1. ~~Commit this handoff~~, ~~kernel `tmp_pack_*` cleanup~~ — both done.

### G. Next build
1. ~~OTA with the 2026-09-30 fixes~~ — DONE: installed to slot `_b` and verified (gralloc leak gone: 36 MB dma-buf
   after 10 app switches, CMA 200 MB free; CarRadioApp + Dicio/Whisper from the image). Slot `_a` = previous build.
2. **Next OTA** will add the etnaviv sysfs label (`sepolicy/genfs_contexts`, fcb19af) — check no more
   `/sys/devices/platform/etnaviv/uevent` denials in dmesg.

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

### Gotchas
- zsh: no word-splitting of `$var`, no `declare -A` → use bash for scripts. Never `pkill -f` a pattern that matches
  your own command line.
- Gitiles returns 503 for file fetches; use a `--filter=blob:none --sparse` clone.
- Mesa worktrees must live outside the Android tree (Soong picks up Android.bp in meson subprojects).
