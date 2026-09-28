# Handoff: Vulkan on the Mali-G52 — from 1.4 to the Android 15/16 minimums profiles

Written 2026-09-27 for a clean session. Background (what was done and how): `handoff-mesa26-kernel618.md`,
section "Vulkan 1.4 on Bifrost v7 — DONE" and "Performance".

## Where we are
- Board on slot `_b`, OTA with Mesa 26.2.3 + our patches. panvk reports **Vulkan 1.4.354** on the G52 (Bifrost v7);
  the device declares vulkan.version 1.4 / level 0. Kernel panfrost uapi **1.3** (our backport).
- CTS: all 1.4 requirement checks pass; the 28.9k functional subset has 0 failures; GLES subset unchanged.
- `adb shell cmd gpu vkprofiles`: **VP_ANDROID_baseline_2021/2022 SUPPORTED**; **VP_ANDROID_15_minimums and
  VP_ANDROID_16_minimums NOT supported** (logcat only says "supported = 0"; use the manual diff below).
- OTA 2026-09-28 14:10 (slot `_a`): Mesa prebuilts @ a57ad93c6e6 (vendor beef1c6) + the 285 MHz GPU floor (verified after
  reboot). vkjson: Vulkan 1.4.354, shaderFloat16 = 1; baselines SUPPORTED, 15/16 minimums not yet.

## 0. First thing tomorrow: the full Android dEQP-VK run
Started 2026-09-27 18:18 as a detached host job, so it keeps running after the session ends:
`~/android/vk-cts-full/run-all.sh` runs the seven Android mustpass levels (vk-main-2025 … 2019; the union is all 1,581,608
cases), newest first, with `run.py` (resume after crash, 3-min stall watchdog).
- Progress: `cat ~/android/vk-cts-full/progress.log`, `tail -2 ~/android/vk-cts-full/run-<year>.log`.
- Results: `~/android/vk-cts-full/out-<year>/results.txt` (`<case> <Pass|Fail|NotSupported|Crash|Terminated:…>`),
  raw logs `out-<year>/run*.qpa`.
- So far: 2025 = 8,300 Pass / 78,690 NotSupported / **0 Fail** (4 min). 2024 was at 75k/348k with 0 Fail.
- If the board rebooted or the job died: just rerun `setsid nohup ~/android/vk-cts-full/run-all.sh >/dev/null 2>&1 </dev/null &`;
  finished levels (with results.txt) are skipped. An unfinished level restarts from scratch.
- Triage: `grep -v " Pass$\| NotSupported$" out-*/results.txt | cut -d. -f2-4 | sort | uniq -c | sort -rn`, then read the
  case's `<Result>`/`<Text>` from the qpa (awk on `#beginTestCaseResult <case>`), reproduce one case directly:
  `adb shell 'cd /data/local/tmp/deqp && ./deqp-binary64 --deqp-case=<case> --deqp-log-filename=/data/local/tmp/deqp/x.qpa'`.
- Expected: `dEQP-VK.wsi.android.*` are NotSupported (surfaceless deqp binary, no window); not a driver issue.
- Fix our failures before starting the profile work below.
- Status 2026-09-28: 2025/2024/2023/2022 = 0 Fail; 2021 had 9 Fail, all FIXED (Mesa, local):
  - c1d3dc7a122 pan/bi scheduler: last tuple of a clause wrote 2 registers (a passthrough-only FMA result still gets a
    register write) → INSTR_INVALID_ENC job fault → the 8 `robustness.image_robustness...r32i...cube*` cases (upstream's
    G52 "Crash" list). Affects GL too. Vulkan subset14 0 Fail, GLES subset = baseline.
  - a57ad93c6e6 panvk: exportable AHB memory reported as IMPORT → `device_memory_report...android_hardware_buffer`.
  - 3aa7752492f panvk/jm: timestamp + availability in one batch (latent non-coherent-cache race, same as PGQ).
  - Diagnose job faults with `dmesg | grep fault` (e.g. INSTR_INVALID_ENC): CTS just reports "Fail".
  2020 = 0 Fail. 2019 (finished 14:03): 203,931 Pass, 0 Fail, 3 out-of-memory results in
  `api.object_management.max_concurrent.*`: `device`/`device_group` = ResourceError (VK_ERROR_OUT_OF_DEVICE_MEMORY from
  vkCreateDevice; in upstream's G52 fails list) and `command_buffer_secondary` = Crash (Scudo aborts on "internal map
  failure (Out of memory)" in a malloc inside panvk, so no VK_ERROR_OUT_OF_HOST_MEMORY can be returned). Open question: does
  a JM secondary command buffer preallocate too much host memory? **Full run done: 1.58M cases, no correctness failures left.**

## 1. The gap to the Android minimums profiles
Profiles: `frameworks/native/vulkan/vkprofiles/profiles/VP_ANDROID_{15,16}_minimums.json`. Compare with the device via
`adb shell cmd gpu vkjson` (device extensions, features, limits). Status on 2026-09-27:

### Android 15 minimums (api 1.3.273)
| Requirement | Status | Work |
|---|---|---|
| `shaderFloat16` (Vulkan12 + Float16Int8 features) | **DONE 2026-09-28** (Mesa 00664e27f0d + 4e7ce4fdab8, pushed; not in prebuilts/OTA yet): enabled for v7 + `shaderFmaFloat16`; fixed Bifrost FP16 FTZ on f2f16/f2f32 (per-clause FTZ ignored FP16 mode for ops lowered to FP32). 25.6k fp16 cases: 10716 Pass / 0 Fail (`~/android/vk12-wip/f16.txt`) | — |
| one of `primitivesGeneratedQuery` (+`VK_EXT_primitives_generated_query`) / `pipelineStatisticsQuery` | **Implemented on JM 2026-09-28** (Mesa 59f461824eb), NOT advertised: the extension `depends` on VK_EXT_transform_feedback (upstream reverted advertising it on CSF, 4959f45e99e, for that reason; CTS runs the whole `transform_feedback.*` group only with the extension). Enable it together with XFB (step 6); test-only enable patch: `~/android/vk12-wip/pgq-test-only-enable.diff`. Verified with that patch: pgq no_xfb+concurrent 1788 Pass / 0 Fail, query_pool 385 Pass, restart test `~/android/vk12-wip/pgq-restart/` 11/11 | Enable with XFB |
| `VK_ANDROID_external_format_resolve` | missing | Medium: render/resolve to Android YUV external formats. See how other Mesa drivers (turnip) implement it. |
| `VK_EXT_surface_maintenance1`, `VK_GOOGLE_surfaceless_query` | **OK (checked 2026-09-28)**: instance extensions from the Android loader (vkjson instance list); vpGetInstanceProfileSupport checks them on the instance | — |
| `customBorderColors`, `provokingVertexLast` | extensions present, feature not shown in vkjson | Verify in `panvk_vX_physical_device.c` (quick). |
| subgroup ops BASIC/VOTE/ARITH/BALLOT/SHUFFLE/SHUFFLE_REL, limits, Bresenham lines, 16/8-bit storage, ycbcr, index_uint8, divisor, maintenance5, 4444, … | OK | — |

### Android 16 minimums (api 1.3.276), additionally
| Requirement | Status | Work |
|---|---|---|
| `VK_KHR_shader_subgroup_uniform_control_flow`, `VK_KHR_shader_maximal_reconvergence` | gated `has_vk1_1` (v10+) | Small to enable on v7, but must pass `dEQP-VK.reconvergence.*` and `dEQP-VK.subgroups.*`. Our `pan/bi: reconverge on loop exits` (05f5f33) is the base; expect more reconvergence bugs (breaks/continues, nested loops). |
| `VK_EXT_transform_feedback` + `transformFeedback` | not implemented on JM ("TODO: transform feedback" in jm/panvk_vX_cmd_query.c) | Medium-large. panfrost GL does XFB on Bifrost (shader-side stores), reuse its approach. CTS: `dEQP-VK.transform_feedback.*`. |
| `protectedMemory` + `VK_EXT_pipeline_protected_access` | false | Large / probably infeasible: needs Mali protected mode + secure memory from OP-TEE. Park it. |
| limits, fullDrawIndexUint32, shaderInt16, integer dot product, float controls, host_image_copy, 2d_view_of_3d, MSRTSS, … | OK | — |

Android 17 will likely add a `VP_ANDROID_17_minimums`; the minimums matter for new-device certification, not for apps
(apps/games check the baseline profiles, which pass).

**Suggested order:** (0) full CTS triage → (1) shaderFloat16 → (2) check the surface extensions → (3) primitives
generated query on JM → (4) external_format_resolve → Android 15 minimums done → (5) subgroup uniform control flow +
maximal reconvergence → (6) transform feedback → (protected memory parked).
After each step: `adb shell cmd gpu vkprofiles` + the regression subset.

## 2. Other open items
- Perf: JM turns every vkCmdDrawIndexed into an indirect draw (write-value + GPU min/max search + single-thread patch
  helper); ~0.7 ms/frame extra at 285-400 MHz. A leaner direct indexed path is a bigger project (CPU index reads are
  invalid in Vulkan). Tried and rejected: CPU descriptor copies, inline min/max in the helper (both no gain).
- Everything is committed and **pushed** (2026-09-27): device, vendor, kernel lineage-23.0 (panfrost uapi 1.3 backport
  58b1cbfec05b9), local_manifests (Mesa project), minigbm 6a36df3 (lineage-23.2), Mesa (github.com/gschuurman/mesa
  `vim3-26.2.3`, tip 38098e9).
- Build an OTA to bake in the 285 MHz floor (and whatever is next).
- Upstream MR candidates: 05f5f33 (Bifrost reconvergence), 38098e9 (NIR a + -a fold), JM timestamps/multiview queries,
  2-queue JM, 1.3/1.4 enablement.
- Clean up the board: `/data/local/tmp/deqp` (~200 MB) when testing is finished.

## 3. How to work (tools and gotchas)
- **Fast driver loop** (no OTA): edit `vendor/mesa3d-upstream` → `cp` the changed files to
  `out/target/product/vim3/obj/MESON_MESA3D/<same path>` → `cd` there and
  `PATH=~/android/teflon/venv/bin:/usr/bin:/bin:$PATH ninja -C build src/panfrost/vulkan/libvulkan_panfrost.so` →
  NDK `llvm-strip --strip-unneeded` → `adb push` to a **new, unique file name** in /data/local/tmp →
  `chcon u:object_r:same_process_hal_file:s0` → `mount --bind` over `/vendor/lib64/hw/vulkan.mesa.so`.
  - Before mounting, **umount all previous layers** (`umount` repeatedly until `mount | grep -c vulkan.mesa` = 0); stop
    apps using it first (`am force-stop`). Bind mounts stack otherwise.
  - Watch for inode reuse: check `ls -i`; files pushed over each other can end up as the same inode.
  - Vulkan is loaded per app process (bind mount applies to new apps). **GL (libgallium_dri) is preloaded by zygote**:
    a GL bind mount only reaches apps after `stop; start`.
  - Mesa debug output goes to **logcat** (tag MESA), not stdout. Use `PANVK_DEBUG=nir`, `BIFROST_MESA_DEBUG=shaders`
    with `MESA_SHADER_CACHE_DISABLE=true --deqp-shadercache=disable` to see shaders. In app processes use properties:
    `debug.mesa.panvk.debug=…` (Mesa maps `FOO_BAR` → `debug.mesa.foo.bar`).
  - The out/ copy is replaced by the next `VIM3_MESA_FROM_SOURCE=true` build (removes temporary debug prints).
- **Testing next to a running CTS job**: `~/android/vk12-wip/run-ns.py <tag> <driver.so on device|-> <caselist> <outdir>`
  mounts the test driver in a private mount namespace (`unshare -m` + toybox `mount -o rprivate none /`; without the
  rprivate step the bind mount LEAKS globally), uses its own device files, disables the shader cache and only kills its
  own deqp. Plain `run.py` jobs collide (same rem.txt/run.qpa, `pkill deqp-binary64`).
- **fastboot slot info is wrong on this U-Boot** (`current-slot` said a while booting _b; `set_active` unsupported).
  Check `ro.boot.slot_suffix`; switch with `bootctl`.
- **Prebuilts**: `VIM3_MESA_FROM_SOURCE=true m libgallium_dri libEGL_mesa libGLESv1_CM_mesa libGLESv2_mesa libgbm_mesa
  dri_gbm vulkan.panfrost` (~3 min), then install into `vendor/khadas/vim3/gpu/mesa/a73/lib64` (script: the loop in
  `vendor/khadas/vim3/gpu/mesa/README.md`; it strips and checks NEEDED). Never ship a build made with the switch on.
- **CTS runner**: `~/android/vk12-wip/run.py <caselist> <outdir> [VAR=val …] [--deqp-…]`; caselists in
  `~/android/vk12-wip/` (req14.txt = 1.4 requirement checks, subset14.txt = 28.9k regression, glsubset.txt = GLES;
  GLES needs `--deqp-surface-type=pbuffer --deqp-surface-width=256 --deqp-surface-height=256
  --deqp-gl-config-name=rgba8888d24s8ms0`). dEQP binary: `m deqp-binary`, pushed with its `vulkan/` data to
  `/data/local/tmp/deqp`. Forcing a version: `MESA_VK_VERSION_OVERRIDE=1.x` (good for gap-finding with req14.txt).
- **Perf**: `~/android/vk12-wip/bench2.sh <skiagl|skiavk>` (frame percentiles) and `gputime.sh` (per-engine GPU time via
  panfrost fdinfo, `profiling=1`, plus devfreq residency).
- **OTA**: `m otapackage` (lunch `lineage_vim3_nvme-bp4a-userdebug`, needs bash), unzip payload into
  `~/android/ota-mesa26`, `rangeserver.py 8765` + `adb reverse tcp:8765 tcp:8765` +
  `update_engine_client --update --follow --payload=http://127.0.0.1:8765/payload.bin --headers=…` (~3 min), reboot.
  After boot: `input keyevent 223` (screen off).
- Shell gotchas: never `pkill -f <pattern>` that matches your own command line (it kills the shell, exit 144); zsh
  can't do `declare -A` (use a bash script); `lunch` needs bash.
- Serial console: `screen -ls`, drive U-Boot with `screen -S <id> -X stuff`, log with `-X logfile <path>; -X log on`.
  The rare boot hang (vold waiting on keystore2 after a power loss) is still unexplained; logs go to
  `~/android/console-logs/`.
