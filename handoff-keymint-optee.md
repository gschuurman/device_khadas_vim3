# Handoff: KeyMint TA on OP-TEE (2026-09-21)

Goal: hardware-backed KeyMint via OP-TEE, replacing the in-process `com.android.hardware.keymint.rust_nonsecure` APEX.
Nothing below is committed or pushed. Nothing has been run on the device (192.168.2.205 was unreachable).

## What works (verified)
- `m android.hardware.security.keymint-service.optee optee_ta_keymint tee-supplicant.rc` succeeds for
  `lineage_vim3_nvme-bp4a-userdebug` (`bash -c 'source build/envsetup.sh; lunch ...; m ...'`; lunch fails under zsh).
- Outputs in `out/target/product/vim3`:
  - `vendor/lib/optee_armtz/dc274baf-5f8c-48cd-89ab-cd9a4d38ed12.ta` (signed with the OP-TEE default TA key)
  - `vendor/bin/hw/android.hardware.security.keymint-service.optee`
  - `vendor/etc/init/{android.hardware.security.keymint-service.optee.rc,tee-supplicant.rc}`
- The TA builds standalone in ~25s with the tree's own Rust (`prebuilts/rust/linux-x86/1.88.0`).

## Update: vendor image now builds (2026-09-21)
`m precompiled_sepolicy vendorimage` succeeds and `vendor.img` contains `lib/optee_armtz/dc274baf-...ta`,
`bin/hw/android.hardware.security.keymint-service.optee`, both init rcs, the three keymint/secureclock/sharedsecret
VINTF fragments and `libteec.so`. Two fixes were needed:
- `sepolicy-vendor/tee.te` redeclared `tee_exec`/`init_daemon_domain(tee)` that `system/sepolicy/vendor/tee.te`
  already provides; it now only adds the vendor_file and /metadata rules.
- xtest's `os_test` TA links `libos_test.so` from another TA: `build_optee_ta.mk` now builds `local_module_deps`
  first and `build-optee-ta.sh` stages their `lib*.so` (and `local_module_deps` is cleared after each TA so it
  doesn't leak into the next Android.mk).

## Still not verified
Nothing has run successfully on the device: the TA has never been loaded by the real OP-TEE core, and
keystore2/vold have not been seen using the HAL.

## Incident: flashing the OP-TEE FIP hangs boot, then a "fix" crashed the kernel (2026-09-22)
First flash: KeyMint HAL retries opening the TA session in a loop; each attempt fails to load the TA with
`get_rpc_alloc_res: RPC allocation failed. Non-secure world result: ret=0xffff000c` (TEE_ERROR_OUT_OF_MEMORY,
origin COMMS) from OP-TEE core. This blocks vold (needs keystore2 before FBE unlock), so the device never
finishes booting. Root cause: `CFG_CORE_DYN_SHM` defaults to `y`; the Linux optee driver's dynamic-shm RPC
buffer allocation fails on this platform.

I "fixed" this by forcing `CFG_CORE_DYN_SHM=n` (fall back to the static `CFG_SHMEM_START/SIZE` pool) and
bumping `CFG_SHMEM_SIZE` to 2MB. **This was wrong and crashed the hardware**: an asynchronous SError
(external abort) panicked the kernel in `tee_shm_alloc_user_buf -> shm_alloc_helper -> pool_op_gen_alloc ->
memset`, i.e. the moment Linux actually touched the ioremapped static SHM pool. Reverted immediately
(`bootloader/optee_os/core/arch/arm/plat-amlogic/conf.mk` back to `CFG_CORE_DYN_SHM=y`, `CFG_SHMEM_SIZE
0x00100000`); FIP rebuilt to confirm the config (`CFG_CORE_DYN_SHM=y`, `CFG_SHMEM_SIZE=0x00100000` in the
exported `optee/conf.mk`). **Not flashed** — do not flash without RAM-booting first (see below).

Likely real cause: `CFG_SHMEM_START/SIZE` (0x05000000, in the gap between there and `CFG_TZDRAM_START`
0x05300000) is not covered by a `reserved-memory`/`no-map` node in the kernel DTS. Only the bare
`firmware/optee {...}` node was ever added (see [[project_tee_dts]]) — no matching memory reservation. So
Linux's own memblock treats that physical range as ordinary free DRAM, and the optee driver's separate
`ioremap` of the same range (as static SHM, uncached/device-type) either races with whatever else the
allocator put there or the range is hardware-secure-only and had simply never been touched before (dynamic
shm was always used in practice, so the static pool's address was advertised but never actually written
until this change forced it into use).

**Before touching this again**: add a `reserved-memory` node (`no-map`) covering at least
`[CFG_SHMEM_START, CFG_SHMEM_START+CFG_SHMEM_SIZE)`, ideally also `[CFG_TZDRAM_START,
CFG_TZDRAM_START+CFG_TZDRAM_SIZE)`, to the kernel DTS, matching how other OP-TEE Linux ports declare it. Then
**RAM-boot (`boot-g12.py`) the new FIP and verify before ever flashing it** — `vendor/khadas/vim3/bootloader/Android.mk`'s
own comment on the OP-TEE module already says this; the crash above happened because a flash went out
without that step. Do not attempt another SHM config change without doing this.

The actual OOM-under-dynamic-shm problem (why `get_rpc_alloc_res` fails) is still open and still blocks the
KeyMint TA; it just no longer crashes the kernel.

## RESOLVED 2026-09-22: root cause was CFG_SHMEM_SIZE, not dynamic SHM at all
Long investigation, but the actual bug was simple. With adb finally reachable (KeyMint decoupled from boot,
see below) and a `tee_client_test` CLI added for interactive testing, live kprobes on the running kernel
(`p:/r:` events under `/sys/kernel/debug/tracing/kprobe_events` -- **remember to `echo 1 >
.../tracing_on`**, the master switch, or every probe silently produces zero hits) proved definitively:

- `pool_op_gen_alloc` (the **static** SHM pool's allocator, `drivers/tee/tee_shm_pool.c`) is what actually
  runs on every TA-load attempt -- NOT the dynamic-SHM path (`pool_op_alloc`/`tee_dyn_shm_alloc_helper` in
  `drivers/tee/optee/smc_abi.c`), despite `CFG_CORE_DYN_SHM=y`. optee_os only advertises
  `OPTEE_SMC_SEC_CAP_DYNAMIC_SHM` when `core_mmu_nsec_ddr_is_defined()` is true
  (`core/arch/arm/tee/entry_fast.c`) -- our g12b platform port never registers a non-secure DDR map, so
  dynamic SHM is silently gated off regardless of the compile flag. All of the CMA/`alloc_pages_exact`
  fragmentation investigation, the `cma=768M` bump, and a whole kernel driver patch to make the dynamic
  pool CMA-backed (`dma_alloc_coherent`, a dedicated `memory-region` reserved-memory carve-out) were
  chasing a code path that was never actually being exercised. All reverted (kernel `git checkout` on
  `smc_abi.c`; DTS carve-out removed -- see git history in this session for the false-start commits).
- The static pool (`CFG_SHMEM_SIZE`, `plat-amlogic/conf.mk`) was only **1MB**. A `tee_shm_alloc_user_buf`
  kretprobe showed the exact failing request: `size=0x171378` (~1.45MB, the KeyMint TA), `ret=-ENOMEM`,
  while a preceding `size=0x1000` request succeeded -- the pool was just too small, and genalloc correctly
  and safely reports "no space" when full (no crash -- that's a different, unrelated incident, see below).
  Fix: bumped `CFG_SHMEM_SIZE` to 3MB (`0x00300000`), which still fits entirely inside the *existing*
  `secmon@5000000` no-map DTS reservation (`0x05000000`-`0x05300000`, up to `CFG_TZDRAM_START`) -- no DTS
  change needed, and the base address is the same one a live successful small allocation just proved safe.
- Also added a `pr_err` in `drivers/tee/tee_core.c`'s `tee_ioctl_shm_alloc()` (kept; harmless, useful for
  future failures of this kind, shows size+errno in dmesg without needing kprobes or serial).
- The earlier SError crash (this file, above) was a **different, compounded** mistake (forcing
  `CFG_CORE_DYN_SHM=n` *and* bumping size in the same change) -- not reproduced by this fix, which only
  changes `CFG_SHMEM_SIZE` and leaves `CFG_CORE_DYN_SHM` alone. Still, RAM-boot (`boot-g12.py`) before
  flashing a new FIP as a matter of discipline.

## SUPERSEDED 2026-09-22: raising CFG_SHMEM_SIZE caused a SECOND SError crash -- real fix is dynamic SHM
The "bump CFG_SHMEM_SIZE to 3MB" fix above was wrong and caused a second hardware crash (SError inside
tee-supplicant's `read()` of the TA file into the enlarged mmap'd buffer -- `copy_to_user` faulted). Reverted
immediately to 1MB. But **the 1MB size was never actually safe either** -- decompiled `bl2.bin` (Ghidra
headless, JDK downloaded portably since no root/apt access; see scratchpad) and found the real cause with
hard evidence: BL2's image-loading function programs Amlogic's AO secure-region protect registers
(`0xff800250`/`0xff80024c`-ish, "AO_SEC_GP_CFG"-style) with:
- `base=0x05000000 size=0x300000` (covers **all** of `CFG_SHMEM_START/SIZE`, any size up to 3MiB)
- `base=0x05300000 size=0x2000000` (covers all of `CFG_TZDRAM_START/SIZE`)

Both exactly match the DTS's own `secmon@5000000` (3MiB) + `secmon@5300000` (32MiB) reservations. **The
entire `[0x05000000, 0x07300000)` range is genuinely hardware-secured, for its full extent, at every size
tried.** The "small 4KB alloc succeeds" observation that looked like partial safety was misleading: SError is
an async/imprecise ARM64 exception -- the fault can be reported against a later, unrelated instruction, which
is exactly what happened (one crash at `memset` inside the allocator, the other at an unrelated `read()` much
later). There is no safe size or sub-offset within this range to use for SHM.

**Real fix implemented**: register this platform's actual non-secure DDR ranges so OP-TEE's dynamic-SHM
capability negotiation (`core_mmu_nsec_ddr_is_defined()`, gating `OPTEE_SMC_SEC_CAP_DYNAMIC_SHM` in
`core/arch/arm/tee/entry_fast.c`) actually succeeds, instead of silently falling back to the hardware-secured
static pool:
- `bootloader/optee_os/core/arch/arm/plat-amlogic/main.c`: added
  `register_ddr(0x0, 0x05000000); register_ddr(0x07300000, 0xf5800000 - 0x07300000);` -- explicitly
  excluding the full `[0x05000000, 0x07300000)` secure span (both windows). Upper bound `0xf5800000` is this
  unit's actual detected DDR size from BL2's own serial log ("DDR size: 3928MB"); revisit for different RAM.
- `bootloader/optee_os/core/arch/arm/plat-amlogic/conf.mk`: `CFG_CORE_RESERVED_SHM` force-disabled (`n`) for
  g12b, and `CFG_SHMEM_START`/`SIZE` left **unset** (guarded `ifneq ($(PLATFORM_FLAVOR),g12b)`) -- so Linux
  can never fall back to the proven-unsafe static pool again, even if dynamic-shm negotiation ever fails for
  some other reason. `CFG_CORE_DYN_SHM` stays at its default `y`.
- Builds clean; confirmed in the exported `optee/conf.mk`: `CFG_CORE_DYN_SHM=y`, `CFG_CORE_RESERVED_SHM=n`,
  no `CFG_SHMEM_*` lines at all.

## CONFIRMED WORKING 2026-09-22 (RAM-boot test)
`boot-g12.py`/pyamlboot RAM-boot of this FIP, no eMMC write. Result:
- `dmesg`: `optee: dynamic shared memory is enabled` (previously always "static"/never dynamic).
- `tee_client_test dc274baf-5f8c-48cd-89ab-cd9a4d38ed12`: `OK: session opened -- TA loaded and running.`
  -- ran twice, both succeeded, no crash, no SError, clean dmesg otherwise. **First successful KeyMint TA
  load in this entire project.**

Given this was only RAM-booted, **the eMMC bootloader is still the OLD (1MB static-pool, non-crashing but
non-functional-KeyMint) FIP** -- `fastboot flash bootloader out/target/product/vim3/u-boot_kvim3_ab_optee.bin`
(this same binary, already built) is what makes it permanent. Do that once satisfied with this result.

Next steps once flashed:
1. Re-run `tee_client_test` a few more times across reboots for confidence (not just this one RAM-boot session).
2. Consider whether to flip KeyMint back to being the *active* boot-time HAL (currently still decoupled --
   `com.android.hardware.keymint.rust_nonsecure` in device.mk, OP-TEE HAL is `vendor.keymint-optee-test`,
   disabled/manual-start). That's a deliberate, separate decision now that the TA is proven to load; not done
   automatically here.
3. Hardware-backed attestation still needs more: HUK/RNG platform support, real attestation key
   provisioning -- this only gets the TA loading and running, not full attestation yet.

**Tooling note**: no root/`apt` access in this environment for `analyzeHeadless`'s Java dependency; a portable
Temurin JDK 21 tarball was downloaded and pointed to via `JAVA_HOME` instead of installing anything system-wide.
Ghidra 12.1.3 headless scripting also dropped Jython/PyGhidra by default here -- used a compiled Java
`GhidraScript` (`-postScript Foo.java -scriptPath <dir>`) instead of `.py`.

**Key methodology note for future debugging**: OP-TEE core's own trace (`I/TC:`, `E/TC:`, `E/LD:`) goes
straight to the physical UART and is invisible to `dmesg`/`logcat` no matter what -- confirmed empirically
(cleared both, ran the failing op, both stayed empty while serial showed the messages). For anything on the
Linux kernel side, live kprobes (`kprobe_events` + kretprobes reading `%x0`/`%xN` registers) are far faster
than editing kernel source and reflashing: `mount -t debugfs debugfs /sys/kernel/debug`, register probes,
**enable `tracing_on`**, reproduce, read `trace`. Costs nothing, needs no reboot, works over plain adb.

**Also decoupled from boot** (separate, unrelated fix, keep this regardless of the above):
`device.mk` ships `com.android.hardware.keymint.rust_nonsecure` (the safe in-process HAL) as the real
boot-time KeyMint HAL. The OP-TEE HAL (`android.hardware.security.keymint-service.optee`) is still built,
but disabled/manual-start under a different service name (`vendor.keymint-optee-test`) with no
`vintf_fragment_modules` (an earlier version of this HAL declared the same fragments as the real HAL, which
broke keystore2 resolution even while the service itself was disabled -- fixed by removing them). A
standalone `tee_client_test` CLI (`vendor/khadas/vim3/optee/testclient/`) tests OP-TEE communication
directly without any HAL/binder involved: `tee_client_test <uuid>`.

## Update: KeyMint decoupled from boot, plus a manual test client (2026-09-22)
Traced the failure to its exact dead end: `handle_rpc_func_cmd_shm_alloc` (kernel,
`drivers/tee/optee/smc_abi.c`) → `optee_rpc_cmd_alloc_suppl` → `optee_supp_thrd_req` (`drivers/tee/optee/rpc.c`)
forwards to tee-supplicant's `process_alloc()` (`external/optee_client/tee-supplicant/src/tee_supplicant.c`),
which calls `alloc_shm()` → `ioctl(TEE_IOC_SHM_ALLOC)`. **None of these three layers log anything on
failure** — confirmed by reading all three sources — so raising tee-supplicant's trace level (this session's
earlier `cfg_tee_supp_log_level=3` + `stdio_to_kmsg`) was structurally never going to show more; that matched
what was observed (only the one-time init-side `/dev/kmsg_debug` AVC lines, never per-retry). The real
allocation failure is inside the kernel's dynamic-SHM `alloc_pages` pool (`pool_op_alloc` /
`optee_shm_pool_alloc_pages`, same file) or in `tee_shm_alloc_user_buf`/`tee_ioctl_shm_alloc`
(`drivers/tee/tee_shm.c` / `tee_core.c`) — next step if this is revisited: add a `pr_err` there, since
nothing upstream will ever surface it otherwise.

Per the user's call: **stopped blocking boot on this.** `device.mk` now installs
`com.android.hardware.keymint.rust_nonsecure` (the reference in-process HAL) again, so vold/keystore2 have a
working KeyMint immediately and boot completes normally. The OP-TEE HAL
(`android.hardware.security.keymint-service.optee`) is still built and installed, but its `.rc` is now
`disabled`+`oneshot` under a different service name (`vendor.keymint-optee-test`, not `vendor.keymint-default`
— the two would otherwise collide, both trying to claim the same AIDL `.../default` instance). Once adb is up:
```
adb shell start vendor.keymint-optee-test
adb logcat -s keymint-hal-optee
```

Also added a minimal standalone `libteec` CLI, `vendor/khadas/vim3/optee/testclient/tee_client_test.c`
(module `tee_client_test`, always installed), for testing without any HAL/binder involved:
```
adb shell tee_client_test                                          # just /dev/tee0 + driver
adb shell tee_client_test dc274baf-5f8c-48cd-89ab-cd9a4d38ed12      # + TA load (KeyMint TA)
```
The second form exercises the exact `sys_open_ta_bin`/ldelf path that fails at boot, but now as a plain
command you can retry, not something wedging vold.

**Not yet flashed or tested on hardware** — `vendor.img` rebuilt successfully with this change
(`m vendorimage`, no boot/bootloader changes needed this time). Flash `vendor.img` only, boot, confirm adb
comes up and `/data` mounts normally, then try `tee_client_test`.

## Files (all uncommitted)
- `.repo/local_manifests/gschuurman.xml`: remote `apache`; projects `tee/optee/ta/keymint` (aosp, pinned ffced1f...) and
  `vendor/optee/teaclave-trustzone-sdk` (pinned a2491db...). Both already synced into the tree.
- `vendor/khadas/vim3/optee/keymint/`:
  - `Android.mk`: `optee_ta_keymint` module (features `dev` on non-user builds)
  - `scripts/build-keymint-ta.sh`: assembles a workdir under `out/.../OPTEE_TA/keymint`, builds and signs
  - `overlay/`: `utee-uuid.patch` (Uuid::from_raw + PartialEq), `optee-logger/` shim crate
  - `Cargo.lock`: pinned; first build needs network for crates.io
  - `hal/`: `Android.bp`, `main.rs` (libteec `SerializedChannel`), `*.optee.rc`, `teec_wrapper.h`
- `vendor/khadas/vim3/optee/optee.mk`: adds the two packages; sets `optee_client cfg_tee_fs_parent_path=/metadata/vendor/tee`
- `vendor/khadas/vim3/optee/tee-supplicant.rc`: `class early_hal`; creates `/metadata/vendor/tee` on `post-fs`
- `device/khadas/vim3/device.mk`: removed `com.android.hardware.keymint.rust_nonsecure`
- `device/khadas/vim3/sepolicy-vendor/`: `tee.te` (+/metadata rules), `hal_keymint_default.te` (+tee_device), `file.te` (`tee_metadata_file`), `file_contexts` (`keymint-service.optee`, `/metadata/vendor/tee`)
- Earlier uncommitted OP-TEE userspace work is also still there: `BoardConfig.mk` OP-TEE block, `sepolicy-vendor/tee.te`, `vendor/khadas/vim3/optee/*`, optee_client/optee_test manifest entries.
- Memory: `project_keymint_optee_ta.md` (indexed in MEMORY.md).

## Design decisions to remember
- TA is Google's `tee/optee/ta/keymint` (Rust, Teaclave `optee-utee`). Protocol: cmd 1024 WRITE (memref in), 1025 READ (memref out, first byte = more-packets flag).
- Binary is named `...keymint-service.optee`, not plain `...keymint-service`: the C++ module of that name installs to the same path and Kati errors on the clash.
- REE FS is in `/metadata`, not `/data`: vold needs KeyMint before /data is mounted. tee-supplicant and the HAL are both `early_hal`; the HAL retries opening the TA session for ~60s.
- Upstream `system/keymint/wire/Cargo.toml` has a duplicate `hal_v4` key; the build script copies KMR and renames it to `hal_v5` rather than editing the tree.

## Known gaps (security, not build)
- ~~HUK: stubbed/zero~~ **RESOLVED 2026-09-22**, see the dated section below: real HUK derived from the
  Amlogic efuse AES-key field (SHA-256 whitened), real hardware RNG. Documented, deliberately-accepted
  caveat: this efuse field is not hardware-locked on this device (matches any unlocked-bootloader retail
  device's threat model) -- see `bootloader/optee_os/core/arch/arm/plat-amlogic/huk.c`'s header comment.
- RNG: **RESOLVED 2026-09-22**, same section -- real MMIO TRNG (`0xff630218`), `CFG_WITH_SOFTWARE_PRNG=n`.
- Root of trust / boot info still comes from Android properties via the reference helper, not from the
  bootloader. Unchanged, still a real gap.
- Attestation keys are software and not Google-provisioned, so Play Integrity will not trust the chain.
  Unchanged, still the main remaining gap for real attestation.

## 2026-09-22: real TeeChannel bug found and fixed -- KeyMint AIDL surface now fully validated

**Context**: after the HUK/RNG work above landed, the OP-TEE KeyMint HAL was promoted to be the real
boot-time HAL (`device.mk`: removed `com.android.hardware.keymint.rust_nonsecure`, OP-TEE HAL's `.rc`
made boot-time). A full clean build+flash then hung forever at `keystore2`/`vold` startup. Root cause
turned out to be a real, previously-undetected bug -- not a HUK/RNG/attestation-provisioning limitation --
because `tee_client_test` (the existing manual test tool) only ever did `TEEC_InitializeContext` +
`TEEC_OpenSession`, never a real KeyMint AIDL command round-trip, so this bug had never been exercised.

**Symptom, reproduced via the disabled/manual-start `vendor.keymint-optee-test` instance (not the boot
path) so the device stayed usable while debugging**: every `getHardwareInfo()` call (and, transitively,
every real KeyMint operation) failed with `TEE_ERROR_OUT_OF_MEMORY` (`0xffff000c`), deterministically,
right after the HAL's own 3 startup messages (boot info, attestation IDs, HAL info) succeeded.

**Two false leads, ruled out empirically before finding the real cause** (kept as the tale for future
"OOM" reports on this HAL -- don't re-chase these):
1. `CFG_TZDRAM_SIZE` was only 12 MiB although BL2 hardware-protects a full 32 MiB window at
   `CFG_TZDRAM_START` (confirmed by decompiling `bl2.bin`, see the comment in
   `bootloader/optee_os/core/arch/arm/plat-amlogic/conf.mk`) -- 20 MiB sat unused. Raised to 32 MiB
   (`0x02000000`) in `conf.mk` and the matching `sec_mem_size`/`res_mem_size` in `link.mk`. **Made zero
   difference** -- identical failure, byte-for-byte, before and after. Kept anyway (real, harmless
   headroom win now that the TA has more room), but it was not the fix.
2. The KeyMint TA's own heap (`tee/optee/ta/keymint/src/config.rs`, `HEAP_SIZE`) was 512 KiB, a value
   fixed at TA-build time regardless of platform TZDRAM size. Bumped 8x to 4 MiB. **Also made zero
   difference.** Kept anyway (real headroom for the actual crypto workload once attestation is added),
   but likewise not the fix. Together, (1) and (2) proved the failure was NOT a real memory-exhaustion
   problem, just mislabeled as one.
3. Bumped `CFG_TEE_CORE_LOG_LEVEL` to 4 (`TRACE_DEBUG`) and `CFG_TEE_CORE_MALLOC_DEBUG=y` in `conf.mk` to
   get OP-TEE's own trace on the physical UART. **The trace stayed completely silent** across the failing
   call -- proof the failure never reached secure world at all. **This debug bump is still in the
   currently-flashed `u-boot_kvim3_ab_optee.bin` and should be reverted to defaults before the final
   commit** (revert both lines, or just delete them -- `?=` means the platform default of 2 comes back).

**Real root cause, confirmed with kprobes + strace** (methodology: `mount -t debugfs debugfs
/sys/kernel/debug`; kretprobe on `tee_ioctl` -- the single `/dev/tee0` ioctl entry point -- showed **zero
hits** during the failing call, proving the error never reached the kernel driver either; `strace -f -p
<hal_pid>` then caught the real syscall: `ioctl(528018950, TEE_IOC_SHM_ALLOC, ...) = -1 EBADF`. `528018950`
is not a real file descriptor -- it's garbage read from a corrupted `TEEC_Context.imp.fd` field, and
libteec's `teec_shm_alloc()` maps any `ioctl()` failure, including `EBADF`, to `TEEC_ERROR_OUT_OF_MEMORY`,
which is why it looked like a memory bug for so long).

The actual bug: `TEEC_Session.imp.ctx` (see `external/optee_client/libteec/include/tee_client_api.h`) is a
**raw C pointer** (`TEEC_Context *ctx`) that `TEEC_OpenSession()` sets to point at wherever the
`TEEC_Context` argument lives *at the moment of the call*. `vendor/khadas/vim3/optee/keymint/hal/main.rs`'s
old `TeeChannel::try_connect()` held `ctx` **by value on the stack**, called `TEEC_OpenSession(&mut ctx,
...)`, then did `Ok(Self { ctx, session })` -- a Rust move that relocates `ctx`'s bytes to their final
home inside the `Arc<Mutex<TeeChannel>>`. `session.imp.ctx` is an opaque-to-Rust raw pointer, so the move
does **not** update it: it's left pointing at the now-dead stack frame inside `try_connect()`. Every
subsequent function call reuses that stack region, progressively clobbering it, until enough of it had
been overwritten that `ctx.imp.fd` read back as garbage on the 4th real call. This also explains why the
3 startup messages "worked": not enough intervening stack use had happened yet to corrupt that memory.

**Fix** (`vendor/khadas/vim3/optee/keymint/hal/main.rs`): box `ctx` (`Box<teec::TEEC_Context>`), allocated
on the heap *before* `TEEC_InitializeContext`/`TEEC_OpenSession` ever run, so its address is final and
stable for the `TeeChannel`'s entire lifetime regardless of later Rust-level moves. `TeeChannel.ctx` is now
`Box<teec::TEEC_Context>`; `try_connect()`, `Drop::drop()` updated to call `.as_mut()` where a `&mut
TEEC_Context` is needed. Full doc comment on the struct explains why, for future readers.

**Validated end-to-end** with a new standalone AIDL client (not just `tee_client_test`'s bare
`OpenSession`), added at `vendor/khadas/vim3/optee/keymint/aidltest/` (`keymint_aidl_test`,
`PRODUCT_PACKAGES_DEBUG`, links `android.hardware.security.keymint-V4-rust` + `libbinder_rs` directly,
calls the real `IKeyMintDevice` binder interface). Deployed via `adb push` to `/data/local/tmp` (not
installed into any image yet) against the manual-start `vendor.keymint-optee-test` instance. Full run,
all steps OK:
```
getHardwareInfo   -> versionNumber=3, TRUSTED_ENVIRONMENT, "TEE KeyMint in Rust" / "Google"
earlyBootEnded    -> OK
generateKey       -> AES-128-ECB-NoPadding, 191B key blob, 1 characteristics entry
begin/update/finish -> real AES-ECB encrypt round trip, 16B in -> 16B ciphertext out
deleteKey         -> OK
importKey         -> raw AES-128 import, 184B key blob, then deleted
```
This is the first time in the project any real KeyMint AIDL operation (not just TA session-open) has been
exercised successfully.

**Deployment method used while iterating** (fast, no reboot needed, safe): `adb root`; `adb push` the
rebuilt `.ta` or HAL binary to `/data/local/tmp/`; `adb shell mount --bind <pushed file>
/vendor/lib/optee_armtz/dc274baf-....ta` (or `/vendor/bin/hw/android.hardware.security.keymint-service.optee`
for the HAL); `stop`/`start vendor.keymint-optee-test` (HAL) or just reboot (TA, since tee-supplicant
wouldn't accept a plain `stop`/`start` cycle here -- "Unable to stop/start service", needs a real reboot to
reload). **Bind-mounts do not survive reboot** -- redo them after every reboot while testing uncommitted
changes; this matches the project's standing rule of bind-mount-only, never remount/disable-verity.

**Current state of the fix, as of stopping this session -- NOTHING beyond the bootloader flash below is
committed or baked into any image**:
- `bootloader/optee_os/core/arch/arm/plat-amlogic/conf.mk` (TZDRAM 32 MiB + debug log level/malloc debug),
  `link.mk` (matching sec_mem_size/res_mem_size) -- **baked into the currently-flashed
  `u-boot_kvim3_ab_optee.bin`** (flashed to `bootloader` this session). Debug log level should be reverted
  before committing (see above).
- `kernel/khadas/vim3_overlay/.../meson-g12b-a311d-khadas-vim3-android.dts` -- comment-only fix (12->32
  MiB), **not yet baked into a flashed dtbo/boot image, harmless either way** (was never functional).
- `tee/optee/ta/keymint/src/config.rs` (`HEAP_SIZE` 512 KiB -> 4 MiB) -- rebuilt, currently **only
  bind-mounted**, not in the flashed vendor image; needs `m optee_ta_keymint` + reflash (or another
  bind-mount) to persist across the next real reboot cycle if kept.
- `vendor/khadas/vim3/optee/keymint/hal/main.rs` (**the actual fix**, boxed `ctx`) -- rebuilt, currently
  **only bind-mounted**, not in the flashed vendor image.
- `vendor/khadas/vim3/optee/keymint/aidltest/` (new `keymint_aidl_test` tool + `optee.mk`
  `PRODUCT_PACKAGES_DEBUG` entry) -- built, only pushed to `/data/local/tmp`, not installed into any image.
- The device's *actual currently-running config* (device.mk / sepolicy-vendor / vendor Android.bp+rc) is
  still the safe **rollback** state from earlier this session: `com.android.hardware.keymint.rust_nonsecure`
  is the real boot-time HAL; the OP-TEE HAL is `vendor.keymint-optee-test`, disabled/manual-start, no
  `vintf_fragment_modules`. **keystore2/vold do not use OP-TEE at all right now** -- the AIDL validation
  above was against the manual-start instance only, reached by a brand-new process's fresh binder lookup
  (existing keystore2 binder connections aren't redirected just because servicemanager's `.../default`
  registration briefly pointed at both HALs at once during testing).

## Next steps (in order)
1. Revert `CFG_TEE_CORE_LOG_LEVEL`/`CFG_TEE_CORE_MALLOC_DEBUG` in `plat-amlogic/conf.mk` to defaults.
2. Decide whether to keep the TZDRAM 32 MiB bump and TA heap 4 MiB bump (recommended: yes, both are real,
   harmless headroom) or revert them now that the real bug is fixed and they're not strictly needed.
3. Commit: `optee_os` (TZDRAM/log-level), `tee/optee/ta/keymint` (heap size), `vendor_khadas_vim3` (the
   real `main.rs` fix + the new `keymint_aidl_test` tool), `kernel_overlay` (DTS comment). Rebuild+reflash
   `bootloader` (for the reverted log level) and either `vendorimage`+bind-mount test again or a full
   flash to get everything installed for real (not just bind-mounted) before the next step.
4. Once (3) is flashed and confirmed clean via `keymint_aidl_test` again post-reboot (bind-mounts don't
   survive, so this also re-validates the fix survived a real, non-bind-mounted rebuild): re-promote the
   OP-TEE KeyMint HAL to boot-time (reverse the `device.mk`/sepolicy-vendor/vendor Android.bp+rc rollback
   from earlier this session) and do a full clean boot test, watching serial for the keystore2/vold hang
   that started all of this -- expected to be gone now.
5. Attestation key provisioning remains the next real phase after that (see "Known gaps" above).

## Correction to an earlier claim
I said the Khadas/Amlogic `.ta` files use a different container magic than OP-TEE. Wrong: both start with `48 53 54 4f` ("HSTO" = SHDR_MAGIC). The header fields after the magic differ, and the Amlogic signing key/TDK ABI still make them unusable here. Conclusion (don't ship them) stands.

## Next steps
1. Full build (`m`), flash. On device check: `ls -l /dev/tee0`; `ps -A | grep tee-supplicant`; `logcat -s keymint-hal-optee`; `dmesg | grep -i optee`; `ls -l /metadata/vendor/tee`.
2. Run the TA self-test (dev build): invoke command 2048 (`RUN_TEST`) on `dc274baf-...` with a small CA or xtest-style helper; check logs for crypto/sdsm test results.
3. Verify keystore2 picks up the HAL: `dumpsys android.security.maintenance`, `cmd -l | grep keymint`, `getprop`/logcat for keystore2 and vold (FBE unlock works).
4. Then: real HUK (Amlogic efuse via SMC, or a provisioned secret), hardware RNG seeding, bootloader-provided RoT.
5. Commit locally per repo (device/khadas/vim3, vendor/khadas/vim3, .repo/local_manifests); ask before pushing (standing preference).

## 2026-09-23: fix validated on a real flash; OP-TEE KeyMint re-promoted to boot-time
- Flashed build (no bind-mounts; on-device HAL + TA sha256 == `out/`): `tee_client_test <uuid>` and the full
  `keymint_aidl_test` round trip passed once, then 3/3 again after a reboot, against the manual-start instance.
- Re-promoted: the committed promotion (device.mk without `rust_nonsecure`, `vendor.keymint-default` early_hal
  with the VINTF fragments) is back in effect, with one real fix -- the HAL now runs as `user system`/`group
  system`, since `nobody` can't open `/dev/tee0` (system:system 0660).
- `CFG_TEE_CORE_LOG_LEVEL=4`/`CFG_TEE_CORE_MALLOC_DEBUG=y` removed from `plat-amlogic/conf.mk`.
- TA heap 4 MiB now comes from `vendor/khadas/vim3/optee/keymint/overlay/ta-heap-size.patch` (applied by
  `build-keymint-ta.sh`), so `tee/optee/ta/keymint` stays pristine at its pinned revision.
- Found while testing: `tee-supplicant` ran as `u:r:init:s0` (only worked because SELinux is permissive) --
  the rollback had dropped its `tee_exec` label. HEAD's `file_contexts` has the labels again.
- **Flash with `./flash.sh --clean`**: existing /data + /metadata keys were made by the nonsecure KeyMint and
  can't be decrypted by the TA. Also RAM-boot the rebuilt FIP before flashing it.
- Next: clean boot test (watch serial for the keystore2/vold hang and tee/hal_keymint_default denials), then
  attestation key provisioning.
