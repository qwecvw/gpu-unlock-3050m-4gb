# GA107 Windows Port — Handoff from the Windows Session

Follow-up to `WINDOWS_PORT_BRIEFING.md`. This file records what the Windows-side session did, what was measured on the hardware before boot, and what is left. The Linux agent's main job is in **§6 Linux agent tasks**.

Status (updated after the first write run, see **§8**): **the UEFI transaction succeeds, but the unlock is not present when Windows runs a workload.** The GA107 profile was first validated read-only on the hardware (3 pre-boot runs). All three runs were read-only: nothing was written to GPU MMIO. The only change to the PCI command register was a temporary MEM-decode enable that was restored afterwards.

## 1. Machine

| Item | Value |
|---|---|
| Laptop | ASUS Vivobook M6500QC (Ryzen 5000H) |
| dGPU | RTX 3050 Laptop 4GB, GA107, `10de:25a2`, subsystem `1043:11d3`, rev `a1`, VBIOS `94.07.5b.00.97` |
| dGPU location | `0000:01:00.0`, behind root port `00:01.1` (`1022:1633`) |
| iGPU (drives panel) | AMD Radeon `1002:1638`, subsystem `1043:11d3`, at `04:00.0` behind `00:08.1` |
| Windows driver | 610.62 |
| Disk | GPT; Windows ESP 100 MB (part 1), C: (part 3), Linux ext4 (part 5) |

## 2. Repositories

- Port target: `https://github.com/impacifist/Windows-GPU-UNLOCK` at commit `e4f988f6993599469df13c01c2c77253e8d2afe6`.
- All changes are in `ga107-port.patch` (next to this file). Apply it to a clean clone at that commit:
  ```sh
  git clone https://github.com/impacifist/Windows-GPU-UNLOCK && cd Windows-GPU-UNLOCK
  git checkout e4f988f6993599469df13c01c2c77253e8d2afe6
  git apply ../ga107-port.patch
  ```
- The Windows working copy is `C:\Users\yura\Desktop\Mgpu-unlock\wgu`. A copy of the Linux fork is in `...\lgu`, used for reference only.
- Toolchain on Windows: LLVM/clang + lld-link **20.1.8** (installed via winget), Python 3.11.

## 3. The briefing was incomplete

The briefing said only BOARD_ID, SUBSYSTEM and BAR0 had to change. The `unlock/` code also hardcodes these:

- the PROM SHA-256 (1 MiB at BAR0+0x300000);
- BOOT0 (chip ID);
- the VRAM size register `0x1183a4`;
- PCI class/revision (`config[0x08]`) and `config[0xc8]`;
- the AMD iGPU GOP: PCI bus, ID/subsystem, and the ACPI ADR of the display.

All of these are profile macros now. Two things are specific to this laptop:

1. **The firmware leaves the dGPU's PCI command register at `0x0000`** (MEM decode off). No UEFI GOP driver binds the GA107 (Optimus), so nothing enables it. The 3090 path assumes nvgop already enabled it.
2. **Class is `0x0302` (3D controller), not `0x0300` (VGA).**

The route used is `unlock/`, the manual USB path. The panel stays on the AMD iGPU, the user presses U, then picks Windows Boot Manager without a reset. `internal-boot/` (auto-chainload from a separate ESP) was not ported. Its profiles assume the console is on the RTX.

## 4. Code changes (summary of the patch)

- **`unlock/port.h`**
  - New profile `GU_PROFILE_GA107_M6500QC`. Every value is pinned from the pre-boot captures (§5).
  - New family switch `GU_EXPERIMENTAL_3050`, parallel to `GU_EXPERIMENTAL_3090`. Both set the internal `GU_EXPERIMENTAL_UNLOCK`; shared code keys on that. `#error` guards stop mixing families or using the wrong stock selector.
  - New fixed stock selector `GU_STOCK_PROFILE_GA107_52600052`. **Do not use it**; see §5, SS0 is not stable. Use `entryLatchedSs1Full`.
  - `GU_TARGET_DECODE_OFF` is 1 for GA107.
  - `GU_PREFLIGHT_CAPTURE`: in a GA107 read-only preflight, identity mismatches are logged and the remaining read-only stages keep running.
  - The default 3090 profile is unchanged. Its constants moved into the `#else` branch.
- **`unlock/app.c`**
  - Literals replaced by profile macros (`GU_CHIP_ID`, `GU_PROM_SHA256`, `GU_CLASS_REVISION`, `GU_CONFIG_C8`, `GU_AMD_GOP_BUS`, `GU_BAR0_END`, `GU_PROFILE_LABEL`).
  - `TargetDecodeEnable()` runs in `FindTarget()` before the first config snapshot. It enables MEMORY only, through `PciIo->Attributes`; it refuses if BME is set and verifies the readback.
  - `TargetDecodeRestore()` runs at `finish`, after the unsafe-return `Halt()` checks, and disables MEMORY again.
  - The preflight log now records the live PROM sha256, the first 8 PROM bytes, and the raw bytes of each GOP device path.
- **`unlock/checks.c`**: the route-1 ACPI ADR is now `GU_AMD_GOP_ADR`.
- **`unlock/app_io.c`**: banner text uses `GU_PROFILE_LABEL`. The USB folder names stay `RTX3090-PREFLIGHT` / `RTX3090-UNLOCK` so they stay compatible with the repo tools.
- **`unlock/build.ps1`**
  - New `-Profile rtx3090-b650m|ga107-m6500qc` (the 3090 default is unchanged).
  - New stock profile `ga107Stock52600052`.
  - Passes `-DGU_TARGET_PCI_ID=0x25a210deU` for GA107.
  - The manifest records the GA107 values.
- **`internal-boot/boot/probe.c`**: the generic read-only probe also allows deep capture for `0x25a210de`. Not used so far.
- **Not run:** host mock tests (`unlock/test-all.ps1`). They need MSVC and fixtures. Only the EFI builds were run (preflight and experimental both compile with `-Werror`).

Build commands (Windows):
```powershell
# read-only preflight
powershell -NoProfile -ExecutionPolicy Bypass -File .\wgu\unlock\build.ps1 -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -Profile ga107-m6500qc
# write build (intended)
powershell -NoProfile -ExecutionPolicy Bypass -File .\wgu\unlock\build.ps1 -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -Profile ga107-m6500qc -ExperimentalUnlock -StockProfile entryLatchedSs1Full
```

## 5. Pre-boot measurements (UEFI, Secure Boot off)

Full logs: `preflight-RUN00.txt`, `preflight-RUN01.txt`, `preflight-RUN02.txt` (next to this file). RUN02 is the validation run, with every value pinned; every check passed.

| Value | Measured | Notes |
|---|---|---|
| Command register at UEFI entry | `0x0000` | MEM decode off; the app enables it and restores it |
| Class/rev `config[0x08]` | `0x030200a1` | 3D controller |
| `config[0xc8]` | `0x00050011` | same as the 3090 |
| Caps | PM `0x60` (D0), MSI `0x68`, PCIe v2 `0x78` (FLR capable), vendor `0xb4` | `GuCheckConfig` passes once MEM is on |
| BAR0 | `0xfb000000`, 16 MiB, 32-bit | the briefing's `0xf5000000` was wrong |
| BOOT0 | `0xb77000a1` | |
| `0x1183a4` (VRAM MiB) | `0x1000` | |
| PROM head / sha256 | `55 aa 7f eb …` / `89d4d78bcae54c007d1410bcd0747a5f0c161940b352543fcd931a0dad96a675` | the PROM window reads fine without a host driver |
| FEAT PLM | `0xffffff8f` | |
| GFW plm/boot/cpuctl | `0x8b8f / 0x3ff / 0x10` | same as the GA102 expectation |
| WPR2_HI | `0` | cold in all runs |
| **SS0** | RUN00 n/a, **RUN01 `0x52600052`**, **RUN02 `0x52600152`** | **bit 8 varies between cold boots.** The Linux dump showed `0x52600152` |
| SS1 | `0x6` | stable |
| AMD GOP | one physical GOP, `PciRoot(0)/Pci(8,1)/Pci(0,0)/AcpiAdr(0x80010400)`, FB `0xff10000000`, 1920×1080 BGR | parent `1002:1638` / `1043:11d3` on bus 4; `console=0`; no NVIDIA GOP |

Because SS0 varies, any fixed-SS0 stock profile would refuse about half of boots. Use **`entryLatchedSs1Full`**. It latches the validated cold entry value once, requires it unchanged until the selector writes, and writes SS0 `0x88888888` and SS1 `0x8` (one guarded write).

## 6. Linux agent tasks

1. **Firmware assets (blocking).**
   - The EFI needs exactly these five files, checked by size and SHA-256 (`unlock/assets.json`):

     | File | Bytes | SHA-256 |
     |---|---|---|
     | `booter_load_ga102.bin` | 61256 | `056cbc064a888f80717726c54d7f64fcd68044d123870c95a74f37f4975dcaa4` |
     | `v67-ga102-610.43.03.bin` | 64000 | `2a0aa1f86d0b6afff5214d641d87d46dc98ef06ed908ff05d0ddbad5ffb7e197` |
     | `gsp_ga10x.bin` | 84277400 | `572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a` |
     | `gsp_boot_ga102.bin` | 24576 | `3df651acb174fd5f5561a1996965b192f50c5d763168446ed0d4ead19d895768` |
     | `gsp_boot_desc_ga102.bin` | 84 | `cc5a3fc1f4c47f2fe107b1b68f0176d8a448e6dec84b575e5560ccef5262bf15` |
   - They are probably under `/lib/firmware/gpu-unlock/610.43.03/`, the path the Linux module loads them from. Run `sha256sum` on them.
   - If they match, copy them to the FAT32 USB stick at `RTX3090-UNLOCK/ASSETS/`. The folder already exists.
   - If they do not match, or are compressed (`.zst`/`.xz`), regenerate them from upstream Zagitta/gpu-unlock at commit `0ebdb1c6308b6a2072be5bf04513472942541956` (`make inputs firmware`, Python 3.14; see `docs/ASSETS.en.md`). Do not substitute other versions; the EFI rejects them.
   - USB stick layout now: `EFI/BOOT/BOOTX64.EFI` (currently the GA107 **preflight**), `RTX3090-PREFLIGHT/RUN00..02`, `RTX3090-UNLOCK/ASSETS/` (empty). Do not touch `hiddify-tun-migrate/`; it belongs to the user.
2. **Cross-check the SS0 bit-8 variance (useful, not blocking).**
   - If possible, read SS0/SS1 (`0x82381c` / `0x823820`) on Linux **before** any NVIDIA or nouveau driver binds after a cold boot.
   - Also read them after the module or driver loads, to confirm whether bit 8 depends on the boot or on driver activity.
3. **Optimus power management (main open risk).**
   - On Windows the unlock happens in UEFI. Windows then boots, and Optimus may put the dGPU into D3cold/GC6 while it is idle.
   - The open question is whether SS0/SS1 survive a D3cold (power removed) or GC6 cycle. If they do not, the unlock will be lost before any workload runs.
   - On Linux you can test this: unlock, check the selectors, force runtime suspend (`echo auto > /sys/bus/pci/devices/0000:01:00.0/power/control` and let it idle with no clients), resume, and re-read the selectors / re-run the FP16 GEMM.
   - Report whether the selectors return to stock after D3cold.

## 7. Next steps on Windows (after assets)

1. Build the write EFI with `-StockProfile entryLatchedSs1Full`, copy it to `EFI/BOOT/BOOTX64.EFI`, and verify the hash.
2. Fully power off, boot the USB stick (Esc boot menu, UEFI entry). The app checks everything, loads the assets, and asks for **U** (Esc or a 30 s timeout cancels).
3. After a successful transaction it returns to the firmware. Pick **Windows Boot Manager without a reset or power-off**.
4. Benchmark FP16 GEMM (the command in the briefing): about 14.5 TFLOPS means stock or lost, about 23 TFLOPS means unlocked.
5. If the app prints **STOP / POWER OFF**, hold the power button. Do not continue to Windows.

## 8. First write run (2026-10-27)

Linux agent report: all five assets were regenerated from upstream `0ebdb1c` (hashes match `unlock/assets.json`) and copied to the USB stick. Files are not in this repository.

Build: `-Profile ga107-m6500qc -ExperimentalUnlock -StockProfile entryLatchedSs1Full`, EFI SHA-256 `017bbef126412d33ba507c7f19b307fcd6ec9e79345eb95591d8588b11291de9`.

UEFI log (`unlock-RUN00.txt`), key lines:

- all checks pass; entry latched `SS0=0x52600152 SS1=0x6`; assets verified; U pressed;
- layout: `fb=0x100000000`, WPR `0xf5400000..0xfff00000`; FWSEC-FRTS ran, WPR2 set at `0xffe00000`;
- Booter/GSP started; `pre-FLR selectors: ss0=0x88888888 ss1=0x8`;
- one FLR; `post-FLR ... SS0=88888888 SS1=00000008`; BME contained, decode restored;
- `transaction result=0 ... fault=0 retained=0 maps=0 FLR=1`, `FINAL status=0 ... clean=1`.

Windows benchmark (`benchmark/bench_gemm.py`, median of 5 × 100 GEMM 4096², same power conditions):

| | Before | After write run |
|---|---|---|
| FP16 | 12.51 | 12.46 |
| BF16 | 12.80 | 12.80 |
| FP32 | 3.05 | 3.00 |

The selectors were at target in UEFI but the Windows throughput is stock. Windows reports the idle dGPU in D3 (`DEVPKEY_Device_PowerData` most-recent state = 4). The hypothesis is that dGPU power management between the handoff and the first workload resets the selectors. This is **unverified**; no workaround is implemented. Open questions: whether the selectors survive D3hot vs D3cold/GC6, and whether they are still at target immediately after Windows boots.
