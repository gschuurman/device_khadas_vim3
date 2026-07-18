# VIM3 AAOS Device Tree — Handoff

**Last updated:** 2026-05-28  
**Build:** LineageOS 23.2 / Android 16 (BP4A.251205.006)  
**Target device:** Khadas VIM3 (Amlogic A311D / S922X, 4 GB RAM)  
**Use case:** Android Automotive OS (AAOS) in-vehicle infotainment / navigation computer

---

## Repositories

| Repo | Remote | Branch | Path |
|---|---|---|---|
| Device tree | `gschuurman/device_khadas_vim3` | `lineage-23.2` | `device/khadas/vim3` |
| Kernel | `android_kernel_khadas_vim3` | `android16-6.12` | `kernel/khadas/vim3` |
| Kernel overlay (config/modules) | `android_kernel_khadas_vim3_overlay` | `lineage-23.2` | `kernel/khadas/vim3_overlay` |
| Custom HALs & apps | `android_vendor_gschuurman_vehicle_interfaces` | `android-16` | `vendor/gschuurman/vehicle_interfaces` |
| GApps Automotive | `propriatary_vendor_google_gapps_auto` | `main` | `vendor/google/gapps_auto` |
| Vendor binaries | *(local, fetch-vendor-package.sh)* | — | `vendor/amlogic/yukawa` |

Build command: `source build/envsetup.sh && brunch vim3`

---

## Device tree structure

```
device/khadas/vim3/
├── BoardConfig.mk           — board-level config (partitions, kernel, AVB, sepolicy dirs)
├── device.mk                — platform core: entry point, inherits all feature files
├── car.mk                   — AAOS platform stack (car service, car apps, car sepolicy)
├── vehicle.mk               — vehicle hardware (VHAL, GPIO, PWM, power policy, UX restrictions)
├── wireless.mk              — WiFi regulatory (NL/ETSI), Android Auto, WiFi Aware
├── gnss.mk                  — USB GPS HAL
├── telephony.mk             — no-SIM stubs required by CarService
├── developer.mk             — ADB root, timezone, debug properties
├── lineage_vim3.mk          — product identity, GApps, LineageOS common
├── vendor-package-ver.mk    — vendor binary version pin (20251218)
│
├── hal/
│   ├── audio/               — BayLibre Generic Audio HAL, car audio config, mixer paths
│   ├── camera/              — USB external camera + Schuurman RVC app
│   ├── connectivity/        — BCM4359 BT service + wpa_supplicant WiFi
│   ├── display/             — WaveShare touchscreen IDC, display wake
│   ├── graphics/            — Mesa a73 / minigbm (card1 = HDMI)
│   └── media/               — Google C2 media codecs
│
├── overlay/
│   ├── frameworks/base/     — power profile, WiFi P2P config, display/audio defaults
│   ├── packages/apps/Car/   — CarSystemUI bottom bar layout, CarSettings audio page
│   ├── packages/modules/    — Bluetooth profile config
│   └── packages/services/   — CarService audio routing, EVS camera ID, cluster off
│
├── sepolicy/                — vim3-specific public SELinux policy (BOARD_SEPOLICY_DIRS)
├── sepolicy-vendor/         — platform vendor SELinux policy (BOARD_VENDOR_SEPOLICY_DIRS)
├── sepolicy-private/        — private SELinux macros (PRODUCT_PRIVATE_SEPOLICY_DIRS)
│
├── build/
│   ├── dtboimage.mk         — packs DTBO from inline kernel build output
│   └── fstab/               — generates fstab.vim3.mmc.avb for first-stage init
│
├── board-info/              — bootloader version constraint
├── input/                   — Generic.kl keylayout
├── permissions/             — vim3.xml sysconfig, android.software.xml vendor perms
└── power_policy.xml         — automotive power policy (WaitForVHAL → On)
```

---

## Custom apps and HALs (vendor/gschuurman/vehicle_interfaces/automotive/)

| Directory | Module | Package | Function |
|---|---|---|---|
| `vhal/` | `android.hardware.automotive.vehicle@schuurman-service` | — | Custom VHAL: GPIO backlight (GPIOA_4, offset 53), reverse gear (GPIOA_2, offset 51), PWM (30518 ns), DPMS display power, BH1750 light sensor, touch-to-wake |
| `audiocontrol/` | `android.hardware.automotive.audiocontrol-service` | — | AIDL AudioControl HAL with fade/balance support; fade config at `vendor/etc/car_audio_fade_configuration.xml` |
| `audiocontrol/app/CarAudioTuner/` | `CarAudioTuner` | `com.gschuurman.caraudiotuner` | UI for fade/balance — **currently disabled**, not in PRODUCT_PACKAGES |
| `screenoff/` | `ScreenOffService` | `com.schuurman.vim3.screenoff` | Turns off display via HDMI DPMS on parking/sleep |
| `volumecontrol/` | `VolumeControl` | `com.schuurman.vim3.volumecontrol` | Broadcast receiver for system bar volume buttons (no slider shown) |
| `usb_gnss_hal/` | `android.hardware.gnss-service.usb` | — | USB GPS receiver HAL |

---

## System bar layout (bottom bar, left → right)

```
[Home]  [·····]  [Apps]  [Dock]  [·····]  [Vol−]  [Mute]  [Vol+]  [Mic/Assistant]
```

Volume buttons fire package-targeted broadcasts to `com.schuurman.vim3.volumecontrol`  
with flag `0` — **no volume panel appears**, single-step adjust only (safe for driving).

---

## Key hardware configuration

| Item | Value |
|---|---|
| SoC platform name | `yukawa` (used by hardware HAL conditionals — do not change) |
| WiFi/BT chip | BCM4359 via brcmfmac kernel driver |
| WiFi brcmfmac features disabled | `0x82008` — disables FWSUP (WPA2 fix), SAE, WOWL |
| WiFi country code | NL (Netherlands, ETSI 5 GHz) |
| Regulatory DB | `external/linux-firmware-mainline/wireless-regdb` |
| Display | HDMI-A-1 on `/dev/dri/card1` |
| GPU | Mesa a73 (Cortex-A73), GBM backend |
| GNSS | USB receiver via android.hardware.gnss-service.usb |
| Audio HAL | BayLibre Generic AIDL, dynamic routing enabled |
| Audio zones | 1 primary zone, 4 volume groups (media, nav, voice, alarm) |
| EVS / rear camera | EVS disabled; RVC via custom `com.schuurman.rvc` app on `/dev/video10` |
| SELinux | **permissive** (`TARGET_SELINUX_ENFORCE := false`) |
| Kernel | GKI android16-6.12, inline build from `kernel/khadas/vim3` |
| Kernel config | `gki_defconfig` + `amlogic_gki.config` (symlink → upstream `.fragment`) + `vim3_extra.config` |
| Fstab suffix | `vim3.mmc.avb` |
| `ro.hardware` | `vim3` |

---

## Vendor binary package (vendor/amlogic/yukawa @ 20251218)

Contains: WiFi/BT firmware, GPU (Mesa a73), video firmware, bootloader.  
To refresh: `./device/khadas/vim3/fetch-vendor-package.sh`  
Version pin: `device/khadas/vim3/vendor-package-ver.mk`

The variable names `YUKAWA_VENDOR_PATH` and `EXPECTED_YUKAWA_VENDOR_VERSION` must stay as-is — the vendor package `.mk` files use them directly.

---

## Notable intentional omissions

| Feature | Status | Reason |
|---|---|---|
| SELinux enforcing | Permissive | Development; set `TARGET_SELINUX_ENFORCE := true` and fix denials to harden |
| EVS / camera surround | Disabled | No multi-camera rig; RVC handled by custom app |
| Instrument cluster | Disabled | No secondary display |
| Car Telematics | Disabled | No telematics hardware |
| USB audio | Disabled | Design choice; `usb_audio_policy_configuration_vim3.xml` commented out in `hal/audio/device_vendor.mk` |
| CarAudioTuner app | Disabled | Built but not in PRODUCT_PACKAGES; add `CarAudioTuner` to `car.mk` to enable |
| Android Auto projection app | Removed | `com_google_android_embedded_projection` removed from `vendor/google/gapps_auto` |
| Debug apps (KitchenSink etc.) | Eng-only | Base `car.mk` includes them for `eng` builds only (`PRODUCT_IS_AUTOMOTIVE_SDK := true`) |
| NFC | Not declared | VIM3 has no NFC hardware |
| Onboard IMU/sensors | Not present | VIM3 SBC has no accelerometer/gyroscope; BH1750 light sensor handled via VHAL |

---

## Known gaps (not yet fixed)

1. **`android.hardware.location.gps.xml`** — USB GNSS is running but GPS capability not declared via permission XML; navigation apps may query `hasSystemFeature(FEATURE_LOCATION_GPS)` and fail.

2. **`android.hardware.microphone.xml`** — Mic channels exist in the audio HAL but the feature is not declared; Google Assistant / voice commands may be affected.

3. **`CarAudioTuner`** — Already built in `vendor/gschuurman`. To enable: add `CarAudioTuner` to `car.mk` `PRODUCT_PACKAGES`.

4. **Power policy incomplete** — `power_policy.xml` only defines initial ON state. Suspend/Shutdown transitions use Android defaults. Add `NoUserInteraction` and `SuspendPrep` states for proper automotive sleep behaviour.

5. **VINTF manifest sparse** — `device/khadas/vim3/manifest.xml` only declares the external camera HAL. All other running HALs (audio, GNSS, thermal, USB, graphics, etc.) are not manifested. Harmless for daily use; will fail VTS compatibility tests.

---

## Build history (lineage-23.2 branch)

| Commit | Change |
|---|---|
| `8ec5c00` | Add volume up/down/mute buttons to system bar |
| `cd884eb` | Remove unconditional debug/test apps (KitchenSink etc.) |
| `8fc51b3` | Remove AndroidAutoProjectionRro |
| `365a5f3` | Add CarNotification, WiFi Aware, audio low-latency, dual-pane Settings |
| `f440897` | Restructure into per-feature mk files |
| `a5483c2` | Remove vim3l references |
| `e69da39` | Fix post-consolidation build (TARGET_DEV_BOARD, sepolicy te_macros) |
| `df81008` | Rename yukawa identifiers to vim3 |
| `0da3598` | Consolidate device/amlogic/yukawa into device/khadas/vim3 |

---

## Flash procedure

```bash
# From the build host after a successful brunch:
adb reboot bootloader
fastboot flashall -w           # or use flash.sh for selective partition flashing
```

`flash.sh` is installed to `$(TARGET_OUT)/flash.sh` and copied to the device during build.

---

## Quick start for a new session

```bash
cd /home/glenn/android/lineage23
source build/envsetup.sh
brunch vim3
```

The device is at `192.168.2.205` (adb over TCP port 5555 — `adb connect 192.168.2.205:5555`).
