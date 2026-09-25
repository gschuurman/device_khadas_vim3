# Handoff: Mesa 26.2.3 (done) + kernel move to GKI android17-6.18 (not started)

Plan (approved 2026-09-25): `~/.claude/plans/mutable-crafting-meteor.md`. Part A = Mesa, Part B = kernel.

## Safety net (Step 0, done 2026-09-25)
- The local tag `baseline-pre-mesa26-6.18` is on the pre-work HEAD of kernel/khadas/vim3, kernel/khadas/vim3_overlay,
  device/khadas/vim3, vendor/khadas/vim3, external/minigbm and .repo/local_manifests.
- The known-good firmware (the build before Mesa 26, kernel 299b95d96c67e, OP-TEE ef401075e) is in `~/android/known-good-20260925/`:
  all images + u-boot_kvim3_ab_optee.bin + flash.sh, plus MANIFEST.txt (commit per repo) and SHA256SUMS.
  To restore: fastboot flash (non-clean, keeps data), or `flash.sh` without --clean.
- Before the first 6.18 boot: take a fresh /metadata dump (U-Boot `pci enum; nvme scan; ums 0 nvme 0:13` + dd).
  It holds the KeyMint/OP-TEE storage.

## Part A — Mesa 26.2.3: DONE
- **Source:** `vendor/mesa3d-upstream` = Mesa tag mesa-26.2.3 + 2 commits on branch `vim3-26.2.3`:
  - `android: allow overriding the host meson, ninja and PATH` (MESA3D_MESON/NINJA/HOST_PATH)
  - `meson: build gallium gfx/compute helpers for Teflon-only builds`
  It lives under `vendor/`, because Android 16 blocks Android.mk files under `external/` (androidmk_denylist.go).
  **TODO:** fork mesa to gschuurman/mesa, push `vim3-26.2.3`, and add the project to `.repo/local_manifests/gschuurman.xml`.
  Until then the repo exists only locally.
- **Prebuilts:** `vendor/khadas/vim3/gpu/mesa/a73` was regenerated with the upstream Android layout:
  `egl/libEGL_mesa.so`, `egl/libGLESv{1_CM,2}_mesa.so`, `lib64/libgallium_dri.so`, `libgbm_mesa`, `gbm/dri_gbm`,
  `hw/vulkan.mesa.so` (panvk), plus the new `libteflon.so` (NPU TFLite delegate).
  The full recipe is in `vendor/khadas/vim3/gpu/mesa/README.md`.
  Unstripped copies are in `~/android/mesa26-unstripped/`.
- **Regeneration switch:** `VIM3_MESA_FROM_SOURCE=true` (BoardConfig.mk, hal/graphics/device_vendor.mk, gpu/vendor.mk). Never
  flash a build made with it on.
- **Host tools:** `~/android/mesa-host-tools` holds mesa_clc, vtn_bindgen2 and panfrost_compile, built natively with LLVM 19,
  plus native.ini and cross-python.ini. Meson/mako live in the venv `~/android/teflon/venv`.
  Teflon is built from the git worktree `~/android/teflon/mesa-vim3-worktree`, outside the Android tree: a wrap subproject
  inside the tree adds a libdrm Android.bp, which breaks Soong.
- **Key bug found:** with etnaviv in `libgallium_dri`, Mesa's Android EGL chose the NPU (GC8000, renderD129, no 3D
  pipe) as the GL device, and the whole UI rendered black. Fix: the GL stack is panfrost-only, and Teflon is built standalone.
  Runtime override if it ever recurs: `setprop drm.gpu.vendor_name panfrost`.
- **Verified on the board (slot _b, 2026-09-25):** SurfaceFlinger `GLES: Mali-G52 MC2 (Panfrost), OpenGL ES 3.1 Mesa 26.2.3`.
  The UI renders (screenshot OK), and GL/AHB render + CPU readback test binaries pass.
  The test binaries are in scratchpad: ahbtest.c, vkenum.c. Rebuild with NDK r29 `aarch64-linux-android34-clang`.

### Vulkan (panvk) findings
- The old 25.3 prebuilt exposed no Vulkan device at all (`cmd gpu vkjson` → `devices: []`). panvk refuses Bifrost v7
  unless `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1` is set (Android property: `vendor.mesa.pan.i.want.a.broken.vulkan.driver=1`.
  Mesa raises PROP_NAME_MAX to 128, so there is no truncation).
- With it set: `Mali-G52 MC2`, Vulkan **1.0**.354, 130 extensions, AHB + swapchain present. Mesa exposes 1.4 only for v10+.
  HWUI and RenderEngine (skiavk) need Vulkan 1.1, so they can't use it. Don't force `MESA_VK_VERSION_OVERRIDE` system-wide.
  The Mesa docs say v7 panvk "may require newer kernel driver versions" → re-test after Part B.
- `cmd gpu vkjson` runs inside the long-lived gpuservice, and Mesa caches option lookups. Test with a fresh process (vkenum).

### Open follow-ups (Part A)
1. **Feature declarations are wrong** (hal/graphics/device_vendor.mk):
   - `ro.opengles.version=196864` (0x30100) should be `196609` (GLES 3.1).
   - `android.hardware.opengles.aep.xml` is claimed, but G52/panfrost has no geometry/tessellation. Drop it.
   - The Vulkan feature XMLs (`version-1_0_3`, `level-1`, `compute-0`) are declared while no device is exposed. Either remove them, or, if
     panvk gets enabled, declare version 1.0 / level-0 only.
2. Pre-existing and harmless: minigbm in app processes logs "Unable to open /dev/dri/card2 … Failed to pre-initialize
   GBM Mesa driver", because apps can't open card nodes.
3. Teflon: re-run the delegate smoke test against `/vendor/lib64/libteflon.so`. It links the platform libdrm 2.4.124.
   Next step: a real TFLite model (MobileNet v1 uint8), which needs a TFLite runtime for Android arm64.
   For app use, libteflon also needs a public.libraries entry + a file_contexts label.
4. Soak test on the car screen: CarLauncher, Organic Maps (GL), RVC TextureView, 10+ minutes, and `dmesg | grep panfrost`.

## Part B — kernel → GKI android17-6.18.32_r00 (NOT STARTED)
Goal: panfrost uapi ≥1.4 (currently 1.2 on 6.12.93), from BayLibre's mainline work instead of backporting.
- BayLibre patches: `https://gitlab.baylibre.com/baylibre/amlogic/atv/linux` branch `integ/amlogic-android-mainline`
  (6.18-rc5, da658be, 2025-11-25). Their 6.12 equivalent is `integ/amlogic-android16-6.12`.
  Their Kleaf manifest (pasted by the user) also uses `aosp/kernel/yukawa-device` branch `common-android-mainline`
  for the config fragments / module lists.
- Our changes: 11 own commits in `backup-pre-squash`, the post-squash delta `git diff backup-pre-squash lineage-23.0~1`
  (NVMe/M2X PCIe, OP-TEE node, NPU node, i2c strip, console), and `299b95d96c67e` (etnaviv IRQ fix).
- Work on a NEW branch `lineage-23.2-6.18` and keep `lineage-23.0`. Keep the LineageOS inline build (TARGET_KERNEL_SOURCE).
- Things to watch:
  - The VINTF kernel check (Android 16 matrices list only 6.12) → `PRODUCT_OTA_ENFORCE_VINTF_KERNEL_REQUIREMENTS := false`.
  - The clang/Rust requirements of android17-6.18.
  - The drm_sched API (for the etnaviv fix).
  - The aic8800 external module.
  - The modules.load / blocklist (keep etnaviv insmod'ed `on boot` so panfrost stays renderD128).
  - The ueventd rule for tap-to-wake.
- Bring-up order and checks: see plan B4.

## Latest state (end of session 2026-09-25) — START HERE
- The committed prebuilts (panfrost-only GL + standalone Teflon) **build cleanly** (`m` succeeded). The OTA packaging was
  stopped on request, so this final version is **NOT on the board yet**.
- The board runs slot `_b` with the *earlier* Mesa 26 build (etnaviv inside libgallium_dri). It only renders because
  of the runtime property `drm.gpu.vendor_name=panfrost`, which is lost on reboot: after a reboot the UI is black until you set it
  again or install the new OTA. Slot `_a` = the old 25.3 build (fallback).
- Next: `m otapackage` → stream it to the inactive slot (`_a`) → reboot → verify WITHOUT the property:
  - the SF GLES string reads Panfrost Mesa 26.2.3
  - `ahbtest` passes
  - a screenshot shows the UI
  - the Teflon smoke test passes from /vendor
  Then fix the feature declarations (follow-up 1).

## Deploy notes
- The board is on the bench with no network, so OTAs are streamed over USB: `adb reverse tcp:8765 tcp:8765` + a Range HTTP server
  (`~/android/ota-mesa26/rangeserver.py`) + `update_engine_client --update --follow --payload=http://127.0.0.1:8765/payload.bin
  --headers="<payload_properties.txt>"`. It takes about 3 minutes and writes the inactive slot.
- After every boot: `input keyevent 223` (sleep) to turn the screen off (burn-in). Use 224/223 for deterministic wake/sleep.
- `stop; start` does NOT reset sys.boot_completed. Wait for SystemUI before screenshots.
