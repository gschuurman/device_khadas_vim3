# RE notes: stock Amlogic BL31/BL30 suspend path (G12B, VIM3)

Written 2026-11-16 as phase A step 1 of `plan-cpu-suspend.md`. Sources:

- stock BL31: `~/android/optee-fip-test/fip-stock/bl31.bin`, loaded at 0x05100000; Ghidra output in
  `~/android/optee-fip-test/re/stock_bl31_decomp.c` (Ghidra merges functions after no-return calls,
  so read the disassembly with
  `aarch64-linux-gnu-objdump -D -b binary -m aarch64 --adjust-vma=0x05100000 bl31.bin`).
- Khadas vendor U-Boot `khadas-vims-pie` (github.com/khadas/u-boot), the source of the stock bl301
  SCP task: `arch/arm/cpu/armv8/g12b/firmware/scp_task/*`, `board/khadas/kvim3/firmware/scp_task/pwr_ctrl.c`.
- Khadas vendor kernel `khadas-vim3-pie` (github.com/khadas/linux): `mesong12b.dtsi`, `drivers/amlogic/pm/gx_pm.c`.

## plat_psci_ops (0x05127da8, TF-A v1.4+ layout, 12 slots)

| slot | op | address | what it does |
|---|---|---|---|
| 0 | cpu_standby | 0x0511967c | assert state == RET(1); SCR_EL3 \|= IRQ\|FIQ; dsb; wfi; restore SCR |
| 1 | pwr_domain_on | 0x05119310 | mailbox[slot] = sec_entry; SCPI 0x04 (cpu ON, cluster ON, css ON); dmb; sev |
| 2 | pwr_domain_off | 0x0511976c | flag[slot] = 0xffffffff; common power-down (below) |
| 3 | pwr_domain_suspend | 0x051197d0 | mailbox[slot] = sec_entry; AO 0xff80025c \|= BIT(slot+8); flag[slot] = 0xffffffff; common power-down |
| 4 | pwr_domain_on_finish | 0x051198c0 | (assert sys level RUN) cluster-on if cluster was OFF; gicv2_pcpu_distif_init; gicv2_cpuif_enable; mailbox[slot] = 0 |
| 5 | pwr_domain_suspend_finish | 0x05119920 | return if core state == RET; clear AO BIT(slot+8); if system OFF: console re-init (UART_AO 0xff803000, 115200) + gicv2_distif_init (if not yet) + pcpu_distif_init + cpuif_enable, cluster-on, mailbox[slot] = 0; else = on_finish |
| 6 | pwr_domain_pwr_down_wfi | 0x05119404 | dsb; flag[slot] = 0; dsb; wfi (mailbox entry is NOT cleared) |
| 7 | system_off | 0x05119428 | reset vectors to ROM, AO flags 8–15 cleared, reboot reason, SCPI 0x08 (0) |
| 8 | system_reset | 0x0511958c | same, SCPI 0x08 (1) |
| 9 | validate_power_state | 0x05119384 | see encoding |
| 10 | validate_ns_entrypoint | 0x05119230 | range check against AO secure regs 0xff800240/24c/250/254, else -9 (INVALID_ADDRESS); *inferred*: DRAM size + secure carve-out (meaning of the registers not verified) |
| 11 | get_sys_suspend_power_state | 0x051192d0 | all three levels = OFF(2), i.e. SYSTEM_SUSPEND is supported |

Common power-down (0x051196d8, used by off and suspend):

1. `gicv2_cpuif_disable()` (GICC_CTLR: clear enable bits, set bypass-disable 0x1e0).
2. If cluster target == OFF and the chip id is 0x22 or 0x29 (G12B): CCI snoop/DVM off for the cluster
   (slave iface reg = 0xfffffffc... i.e. clear bits 0-1, wait for change-pending) and set bit 10 of
   0xff634644 / 0xff634784 (cluster ctrl).
3. SCPI 0x04 SET_CSS_POWER_STATE: `cpu | cluster<<4 (from MPIDR) | cpu_state<<8 | cluster_state<<12 | css_state<<16`
   with cpu_state = 3, **cluster_state = 3 if the cluster target is OFF, else 0**, **css_state = 3 if
   the system target is OFF, else 0**.

`mailbox[slot]` = 0xfffe7f00 + slot*16 (entry, 64-bit); `flag[slot]` = 0xfffe7f08 + slot*16 (32-bit,
0xffffffff while going down, 0 just before the final WFI: the "ready to cut power" handshake for BL30).
slot = AFF0 + AFF1*4.

## Power-state encoding (validate_power_state)

Original format (PSCI_EXTENDED_STATE_ID=0, matches our build default). Bit 16 = type,
bits 24–25 = power level, **StateID bits 0–15 must be 0**, level 3 rejected; any other value gives -2.

| power_state | meaning | local states |
|---|---|---|
| 0x00000000 | core standby (retention) | core=RET |
| 0x00010000 | core power-down | core=OFF |
| 0x01010000 | cluster power-down | core=OFF, cluster=OFF |
| 0x02010000 | system power-down | core=cluster=system=OFF |

Standby with a level above 0 is rejected. No cluster retention state exists.

## Entry stub (RVBAR = 0x05126000 for all 8 slots)

Sets SCTLR_EL3, VBAR_EL3, DAIF and CPTR_EL3. For **slot 0 (CPU0)**: if mailbox[0] == 0 it does `wfi` forever, otherwise
it branches to mailbox[0]. Other slots do `sevl; wfe` until mailbox[slot] != 0, then branch. The literal
0xff80025c sits in the stub's pool and a BIT(slot+8) is computed, but it is never used.

**CPU0 answer:** the stock BL31 has *no* CPU0 special case in off, suspend or pwr_down_wfi, so stock
powers CPU0 down through the SCP like every other core and resumes it through mailbox[0]. Our fork's
"CPU0 can't be turned off" (WFE loop + manual jump) is inherited from upstream gxbb/g12a code and predates
our per-core RVBAR stub. Real CPU0 power-down is a separate bench experiment, and phase B needs it,
because SYSTEM_SUSPEND is entered by CPU0 as the last core.

## BL30 / bl301 (SCP) side

- BL30 (Chromium-EC based, `g12b_v1.1.3394`) handles SCPI 0x04 ("CPU %d off.", "suspend_counter",
  "Enter ddr suspend/resume", "Little/Big core clk suspend rate").
- bl301 = board task built from the vendor U-Boot source (we could build our own). `secure_task()` takes
  `COMMAND_SUSPEND_ENTER` → `enter_suspend()`: `power_off_at_24M` (KVIM3: GPIOH_8 low = VCC5V off,
  GPIOAO_4 low = VCCK_A off, TEST_N low = VCCK_B + VCC3.3V off, VDDEE PWM to sleep voltage),
  `power_off_at_mcu` (bit-banged I2C to the Khadas MCU 0x18: reg 0x80 = 1, **only on power-off**, not on suspend),
  then `detect_key()` polls wake IRQs: CEC, IR, **VRTC (RTC_WAKEUP)**, **AO_GPIO0 IRQ with
  GPIOAO_7 low = POWER_KEY_WAKEUP (falling edge)**, BT (GPIOX_18), ETH PMT/WOL. Then `power_on_at_24M`,
  and the wake reason goes back through `resume_data.method` (the kernel reads it with `scpi_get_wakeup_reason`).
- **GPIOAO_7 is our XPWR/ACC line**, so the stock deep-suspend wake source is the one we need. The
  power rails it cuts are KVIM3-specific. Check them against the car mainboard before phase B; we
  would likely build our own bl301 for it.

## BL30 references to 0xff80025c (checked 2026-11-16)

`bl30.bin` (Thumb, loaded at 0x10000000) has the literal at 4 places, with no 0xfffe7f00/08 mailbox words:
- 0x3axx/0x3c2c: the per-core power-down/power-up sequences (A73: bit 12+core, i.e. slot+8). BL30
  **sets** the bit after powering a core down and **clears** it after powering it up. So bits 8–15
  are a "core is off" status shared by BL30 and BL31. Stock BL31 sets the bit early on suspend and
  clears it in suspend_finish.
- 0x4f92–0x4fe2: the system-resume path clears bit 0, then polls up to 5000 × 1 ms for bit 0 to be set
  (a resume handshake; who sets it is not identified yet). This is relevant to phase B.
- 0xd8a: boot-time check (bit 30).

None of these shows a wake-on-interrupt path for a single core switched off by SCPI. **That question is
still open** and needs the bench (or a deeper RE of the SCPI 0x04 handler and the M3 IRQ table).

## Vendor kernel usage

- `mesong12b.dtsi` defines `CPU_SLEEP_0` (0x0010000, entry/exit 8000 µs, residency 20000 µs) and
  `CLUSTER_SLEEP_0` (0x1010000, 9000/9000/25000 µs, both `local-timer-stop`), **but every
  `cpu-idle-states` line is commented out**. Amlogic never shipped CPU_SUSPEND in cpuidle. With
  8–9 ms latencies it is only useful for long idle periods (s2idle), and nobody in the field
  has validated whether BL30 wakes an SCP-OFF core on a GIC interrupt.
- `gx_pm.c` (`amlogic, pm`): only registers its suspend ops (→ `arm_cpuidle_suspend(max_idle_lvl)`) when PSCI
  is exactly 0.2 *and* cpu0 has `cpu-idle-states`, which never holds here. *Inferred, not read*: on the
  stock PSCI 1.0 BL31 the generic kernel PSCI SYSTEM_SUSPEND path is what the vendor kernel uses. Wake reason via SCPI; SIP 0x82000042 sets the suspend reason.

## Consequences for phase A (implemented in TF-A, unvalidated)

- Accept exactly the stock encodings; reject CPU0 power-down for now (its WFE loop only wakes on
  `g12b_cpu0_go`, never on an interrupt). Implement cpu_standby (stock-identical).
- Do not clear mailbox[slot] in pwr_down_wfi. Only flag[slot] = 0, as stock does.
- Set and clear AO BIT(slot+8) around suspend, as stock does (BL30 may use it to tell suspend from off).
- SCPI cluster_state = OFF when the cluster goes OFF (build flag, cluster 1 only, since CPU0 never really
  powers down and cluster 0 therefore must stay powered).
- **Open risk:** whether BL30 re-powers a core it switched off when an interrupt for it arrives. If it
  does not, the first CPU_SUSPEND power-down hangs that core forever. That is the first thing the bench
  test must show (see the plan, step 5).
