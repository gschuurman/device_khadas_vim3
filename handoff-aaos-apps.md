# Handoff: AAOS apps, setup wizard, maps, VHAL, launcher (2026-09-23, status updated 2026-09-25)

## Status 2026-09-25 (full clean build, `flash.sh --clean`, verified on HW, committed locally, not pushed)
- Bootloop 2026-09-25 (init_user0_failed -> recovery): KeyMint TA lost its factory-reset secret
  (`SecureDeletionSecrets_1`), so vold couldn't unwrap the /metadata key. The TA's `dev` self-test
  (RUN_TEST) deletes that production object; `dev` is now opt-in (`VIM3_KEYMINT_TA_DEV=true`). After the
  clean flash: 4 reboots, /data + user 10 unlock every time.
- Gatekeeper: failure records now persisted in TEE storage. Verified: 5 wrong PINs -> throttled, reboot ->
  still throttled, correct PIN after 30 s -> user 10 RUNNING_UNLOCKED (CE). Test PIN cleared.
- Map card grey after HOME (sect. 7): FIXED in CarSystemUI RemoteCarTaskViewServerImpl.showEmbeddedTask()
  (explicit reorder; setTaskViewVisible() is a no-op when the TaskView state already says visible). The
  CarLauncher workaround was reverted. Verified with 9 HOME presses.
- Organic Maps: nav banner on the card (verified: turn, distance, street, ETA, updates while moving);
  release build signed with our key (~/android/keys) is the ROM prebuilt; pushed to
  git@github.com:gschuurman/organicmaps.git branch aaos-map-provider.
- Dicio = default assistant (static RRO priority 1002 over GasPlaystoreOverlay); the system-bar mic button
  starts its ACTION_ASSIST activity when there is no VoiceInteractionService. Needs internet once for its
  speech model.
- System bars never go immersive (framework config_remoteInsetsControllerControlsSystemBars + CarSystemUI
  config_systemBarPersistency=0).
- Rear view camera: settings screen injected in CarSettings (camera, stream size with PAL/NTSC hint,
  fit/fill/stretch, mirror, preview), values in persist.rvc.*; preview now a TextureView. Not yet tested with
  the grabber (bench board has no USB peripherals).
- Setup wizard car flow, VHAL units, governor: verified on the new build.
- Still open: SELinux enforcing sweep (needs the peripherals attached), early-boot RVC (native
  AID_AUTOMOTIVE_EVS client, proposed), Organic Maps data must be re-pushed after a clean flash
  (~/android/om-world -> /data/media/10/Android/data/app.organicmaps/files/260714, chown to the app uid:1078).

Original notes (2026-09-23) below; "uncommitted" there is superseded by the status above. Device: VIM3 on USB (`ANDROID_SERIAL=CE88ECF8D115`),
driver user = 10, headless system user = 0. Build: `bash -c 'source build/envsetup.sh; lunch
lineage_vim3_nvme-bp4a-userdebug; m <module>'` (lunch fails under zsh).

## 1. OP-TEE Gatekeeper — WORKS on HW
- TA: `vendor/khadas/vim3/optee/gatekeeper/ta/` (from AOSP `platform/external/optee/apps` upstream-master
  @b5a86e5, UUID `4d573443-...`). Local patches: KeyMint TA UUID `dc274baf-...` + cmd `512`
  (GET_AUTH_TOKEN_KEY), mint-token failure fails verify, log level 2, **response paths return
  TEE_SUCCESS** (upstream returned TEE_FALSE on wrong PIN → client lost the response + throttle timeout).
- HAL: `vendor/khadas/vim3/optee/gatekeeper/hal/` — new AIDL `IGatekeeper` (C++, libteec, lazy connect,
  bounds-checked wire format). `deleteUser/deleteAllUsers` → NOT_IMPLEMENTED.
- Wiring: `optee.mk` (+HAL), `device.mk` (-`gatekeeper.nonsecure`), `sepolicy-vendor/file_contexts`,
  `hal_gatekeeper_default.te` (tee_device).
- Verified: PIN enroll/verify, wrong PIN, throttling (5 tries → 30 s), **CE unlock after reboot**
  (keystore2 accepted TA-minted tokens, vold unlocked user 10). Test PIN cleared afterwards.
- The flashed image still has the *pre-fix* TA (wrong-PIN bug); next build fixes it.
- TODO: persist failure records in TEE secure storage (currently RAM only → reboot resets throttling).

## 2. LineageOS SetupWizard with AAOS flow (replaces Vim3Setup) — WORKS, upstreamable
Repo `packages/apps/SetupWizard` (LineageOS). Product: `vehicle.mk` ships `LineageSetupWizard`,
`lineage_vim3.mk` comment updated. Vim3Setup no longer in the product (source still in
vendor/gschuurman/vehicle_interfaces).
- `res/raw-car/lineage_wizard_script{,_user}.xml` (UI mode car; `config_defaultUiModeType=3`).
- `SetupWizardUtils`: `isAutomotive()`, HSUM-aware `isOwner(Context)` (first full user),
  `isHeadlessSystemUser()`, `setCarSetupInProgress()` (`android.car.SETUP_WIZARD_IN_PROGRESS`),
  finish also marks user 0 USER_SETUP_COMPLETE.
- New pages `src/.../car/`: CarNetwork, CarBluetooth, CarScreenLock, GoogleAccount (status + open
  CarSettings screen), CarProfile (UserManager.setUserName), CarUnits (VHAL display units; skips if absent).
- Direct boot: `SetupWizardActivity` directBootAware + `WaitForUnlockActivity` (fixes user 10 stuck in
  BOOTING behind FallbackHome).
- Welcome accessibility → CarSettings; Location AGPS shown for HSUM owner; `distractionOptimized` on all.
- **Translations**: the 26 `car_*` strings added to all 77 locales (local commit — upstream goes via Crowdin).
- Generator scripts for the translations: scratchpad `suwtr/` (not kept).

## 3. Organic Maps AAOS map provider — WORKS (debug build)
Repo `~/android/organicmaps`, branch `aaos-map-provider` (uncommitted; **user must create the GitHub
fork** — no `gh`/token here). Build: `cd android && ./gradlew assembleFdroidDebug -Parm64` with
`JAVA_HOME=prebuilts/jdk/jdk21`, `ANDROID_HOME=~/android/sdk`, cmake 3.31.6 in PATH (do NOT use `-Ppch`).
Installed as `app.organicmaps.debug`.
- `automotive/MapCardActivity` (CATEGORY_APP_MAPS card for CarLauncher, own task affinity): "car"
  DisplayType, search pill + bookmarks, follows position at zoom 16 on first fix, taps open full app.
- `automotive/Automotive`: `isAutomotive()`, `releaseMap()`.
- SDK: `MapController.detachSurface()/attachSurface()`, `Map.isSurfaceAttached()` — explicit engine
  handoff (waiting for surfaceDestroyed() froze the map when the window was already hidden).
- `MwmActivity`: AAOS keeps the activity (no placeholder), takes the map back in onStart,
  `EXTRA_SHOW_SEARCH` (cancels a *planned* route, opens search expanded → focused + keyboard),
  processes intents skipped while the card held the map. `distractionOptimized` on Splash/Mwm.
- Device data: world + all 24 Netherlands mwm (v260714) pushed to
  `/data/media/10/Android/data/app.organicmaps.debug/files/260714/` (copies in `~/android/om-world/`).
- Card is the "Always" default for APP_MAPS (chosen in the chooser).
- Verified flow: card → "Zoek" → Arnhem Centraal → Route (21 min/25 km) → START (disclaimer unaccepted).
- TODO: release build signed with own key + ROM prebuilt; nav banner on the card during navigation
  (next turn icon `CarDirection.getTurnRes(exitNum)`, `distToTurn`, `nextStreet`, ETA via
  `Utils.formatRoutingTime/formatArrivalTime` from `Framework.nativeGetRouteFollowingInfo()`);
  follow Organic Maps' commit rules (CLAUDE.md in repo: `[android]` prefix, `-s`, mention LLM use).

## 4. Google apps
- `vendor/google/gapps_auto/gapps-core.mk`:
  - **Removed** Google Maps + Google car Assistant (+ their privapp XMLs and GAS maps/assistant overlays).
    Maps never downloads map data (0 bytes; uncertified) and Assistant never registers as the voice
    interaction service. Play Store + GMS + GSF kept (they work).
  - **Added** Android Auto receiver *stub* (`com_google_android_embedded_projection_stub` + privapp XML +
    car UI RRO) so a Play Store install becomes a privileged system-app update (CAR_PROJECTION).
    Updated system apps are exempt from the privapp allowlist check → no boot-crash risk.
- `BoardConfig.mk`: `androidboot.hardware.sku=gas_playstore` (was gas_maps_playstore) → activates
  GasPlaystoreOverlay + CarLauncherGasPlaystoreOverlay.
- On the device now: Maps and carassistant `pm uninstall --user 10` (restore: `pm install-existing`).

## 5. VHAL (vendor/gschuurman/vehicle_interfaces/automotive/vhal) — tested, NOT flashed
- `INFO_FUEL_TYPE` = UNLEADED (was ELECTRIC → Maps queried EV props).
- New READ_WRITE `DISTANCE_DISPLAY_UNITS`, `HVAC_TEMPERATURE_DISPLAY_UNITS`, `FUEL_VOLUME_DISPLAY_UNITS`
  (metric defaults), persisted in `persist.vendor.vehicle.*_units`; `sepolicy/property_contexts` adds
  `persist.vendor.vehicle.` → vendor_vehicle_prop.
- **Never restart the VHAL live**: it restarted CarService and put user-10 SystemUI in a crash loop
  ("another window of type 2040 already exists"); only a reboot fixed it.

## 6. Other system fixes
- `init.vim3.rc`: CPU governor **conservative + 100 ms sampling** instead of schedutil (schedutil did
  ~300 freq changes/s/cluster at 50 ms firmware latency; sugov threads ~50% CPU each; OM search 8.8→4.4 s).
  Live on the device until reboot, permanent from next build.
- `packages/apps/Car/SystemUI` `PanelTransitionCoordinator`: tasks without a panel logged at debug, not error.
- `flash.sh`: `--clean` = `fastboot oem format` only (committed earlier).

## 7. OPEN: map card turns grey after Home pressed while already home
Reproducible: `input keyevent 3` twice on the home screen → embedded mapcard task `visible=false`.
Leaving (app grid) and returning restores it. CarLauncher gets the HOME intent (delivered to top) →
`hostAppeared()` in onNewIntent, but the HOME transition then hides the embedded task.
Tried in `packages/apps/Car/Launcher` `CarLauncherViewModel` (currently in the tree, **does not fix it**):
onWindowFocusChanged hook (focus never changes), delayed `hostAppeared()` (500 ms), delayed
disappeared+appeared cycle. `showEmbeddedTask()` via CarSystemUI seems a no-op when the task view already
counts as shown. Next: trace CarSystemUI's TaskView showEmbeddedTask path
(`packages/apps/Car/SystemUI` / wm-shell TaskViewTaskController) or re-launch the maps intent into the
task view after the HOME transition. The CarLauncher change can be reverted if no better fix is found.

## 8. Not started / ideas
- Open-source voice assistant (user question): candidates to evaluate — Dicio (on-device, F-Droid) as
  VoiceInteractionService / ACTION_ASSIST handler; needs `config_defaultAssistant` (now blanked by the
  gas_playstore overlay) or a role grant (`android.app.role.ASSISTANT`).
- Gatekeeper persistent failure records; SELinux enforcing sweep; VTS run.

## Live device state (lost on reboot)
- Bind-mounts: `/system/priv-app/CarLauncher/CarLauncher.apk` → `/data/local/tmp/CarLauncher2.apk`.
- Mock location: test providers gps/network/fused at De Rauwendaal 31, Heteren (51.9570326, 5.7552655),
  refreshed every second by a host-side adb loop (stops with the Claude session or a reboot).
- CPU governor conservative (set live).
