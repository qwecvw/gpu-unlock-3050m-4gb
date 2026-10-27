# GPU Unlock — GA107 (RTX 3050/3050 Ti Mobile) Fork

Fork of [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock) with **GA107 chip support** added.

The original module unlocks FP16/BF16 tensor core issue rate on GA102, GA103, GA104, and GA106 consumer cards. This fork extends it to **GA107** — the chip in RTX 3050 Mobile, RTX 3050 Ti Mobile, and RTX A500/A1000 Laptop GPUs.

GA107 uses the same ga10x firmware family, SEC2 Booter, and speed selector registers as GA102–GA106. The only change is adding GA107 PCI device IDs to `recipes.c`.

## Benchmark Results

RTX 3050 Mobile 4 GB (GA107, `10de:25a2`), dense 4096x4096 GEMM, YOLO11n training on COCO128 (100 images, 3 epochs):

| Test | Before | After | Change |
|------|--------|-------|--------|
| GEMM FP16 | 14.48 TFLOPS | 22.93 TFLOPS | **+58%** |
| GEMM BF16 | 14.33 TFLOPS | 22.40 TFLOPS | **+56%** |
| GEMM FP32 | 3.75 TFLOPS | 3.77 TFLOPS | ~0% |
| Train FP16 | 40.42s | 30.26s | **-25%** |
| Train FP32 | 44.40s | 42.18s | ~0% |

FP32 is unaffected — the unlock only changes FP16/BF16 tensor core issue rate.

## Supported Cards

All GA107 PCI device IDs are included (plus all original GA102–GA106 cards):

- **RTX 3050 Mobile** (0x25a2, 0x25a5, 0x25ab, 0x25ac, 0x25e2, 0x25e5)
- **RTX 3050 Ti Mobile** (0x25a0, 0x25a7)
- **RTX 3050 6 GB** (0x25a6)
- **RTX A500 / A1000 Laptop** (0x25b6, 0x25b8, 0x25b9, 0x25ba, 0x25bb, 0x25bc, 0x25bd, 0x25be, 0x25bf)
- **T550 / T600 Laptop** (0x25a4, 0x25a9, 0x25aa, 0x25ad, 0x25af)
- Others: 0x25e0, 0x25f9, 0x25fa, 0x25fb

Check your device ID:

```sh
lspci -nn | grep -i nvidia
# Example: 01:00.0 3D controller [0302]: NVIDIA Corporation GA107M [GeForce RTX 3050 Mobile] [10de:25a2]
```

## How to Install

```sh
git clone https://github.com/qwecvw/gpu-unlock-3050m-4gb
cd gpu-unlock-3050m-4gb
make install
```

Add kernel boot parameter (use your PCI address from `lspci -D`):

```sh
# Edit /etc/default/grub — add inside GRUB_CMDLINE_LINUX_DEFAULT:
gpu_unlock.targets=0000:01:00.0

sudo update-grub
```

Then **full power cycle** (see below).

For other boot loaders and distros, see the [Boot flow and opt-in](#boot-flow-and-opt-in) section.

## Laptop-Specific: Full Power Cycle Required

On laptops, the battery keeps the GPU powered even when shut down. A normal reboot does not fully reset the GPU, so the NVIDIA driver may overwrite the speed selectors before the unlock takes effect.

**Required procedure after install or reboot:**

1. Shut down the laptop
2. Unplug the charger
3. Hold the power button for 30 seconds (drains residual power from GPU)
4. Wait 10 seconds
5. Plug charger back in and power on

Without this, the unlock will not apply. Desktops do not need this — a normal reboot is sufficient.

## How Long Does It Last?

- **Until next power loss.** The unlock persists through the session.
- **Reboot:** module re-applies at boot automatically. Laptops need another full power cycle.
- **Suspend to RAM:** GPU powers off, module re-applies on resume.
- **Hibernate:** not covered.

## Verifying the Unlock

```sh
# Check module loaded and applied
dmesg | grep gpu_unlock

# Check kernel parameter is set
cat /proc/cmdline | grep gpu_unlock

# Quick GEMM test (requires PyTorch with CUDA)
python3 -c "
import torch, time
a = torch.randn(4096, 4096, dtype=torch.float16, device='cuda')
b = torch.randn(4096, 4096, dtype=torch.float16, device='cuda')
torch.cuda.synchronize(); t = time.time()
for _ in range(100): c = torch.mm(a, b)
torch.cuda.synchronize()
tflops = 2 * 4096**3 * 100 / (time.time() - t) / 1e12
print(f'FP16 GEMM: {tflops:.1f} TFLOPS')
print('Expected: ~14 locked, ~23 unlocked (RTX 3050M)')
"
```

## What Changed From Upstream

Single file: `kernel/gpu_unlock/recipes.c`

```c
// Added GA107 macro (same ga10x family as GA102-GA106)
#define GA107(id) { id, 0x177, &ga10x }

// Added all GA107 PCI device IDs to cards[] array
GA107(0x25a0), GA107(0x25a2), GA107(0x25a4), GA107(0x25a5),
GA107(0x25a6), GA107(0x25a7), GA107(0x25a9), GA107(0x25aa),
GA107(0x25ab), GA107(0x25ac), GA107(0x25ad), GA107(0x25af),
GA107(0x25b6), GA107(0x25b8), GA107(0x25b9), GA107(0x25ba),
GA107(0x25bb), GA107(0x25bc), GA107(0x25bd), GA107(0x25be),
GA107(0x25bf), GA107(0x25e0), GA107(0x25e2), GA107(0x25e5),
GA107(0x25f9), GA107(0x25fa), GA107(0x25fb),
```

**Why it works:** GA107 shares the same SEC2 Booter firmware, V67 signed payload, speed selector registers (SS0 at `0x0082381c`, SS1 at `0x00823820`), PLM register layout, and Falcon HWCFG signature (`0x80420100`) as the rest of the GA10x family.

## Local Build

```sh
make install        # detect distro and build+install
make install-arch   # Arch Linux
make install-deb    # Debian/Ubuntu/Mint
make install-rpm    # Fedora/RHEL
```

NVIDIA inputs cached in `~/.cache/gpu-unlock/inputs`.

## Boot Flow and Opt-in

The module loads from initramfs before the NVIDIA driver binds:

- **initramfs-tools** (Debian/Ubuntu/Mint): `/usr/share/initramfs-tools/modules.d/gpu-unlock.conf`
- **mkinitcpio** (Arch): `/etc/mkinitcpio.conf.d/zz-gpu-unlock.conf`
- **dracut** (Fedora): `/usr/lib/dracut/dracut.conf.d/gpu-unlock.conf`

Kernel command line options:

```
gpu_unlock.targets=all                          # every supported card
gpu_unlock.targets=0000:01:00.0                 # specific card
gpu_unlock.targets=0000:01:00.0,0000:02:00.0    # multiple cards
gpu_unlock.resume=0                             # disable re-unlock on resume
```

## Uninstalling

```sh
cd gpu-unlock-3050m-4gb
make uninstall
```

Remove `gpu_unlock.targets=...` from `/etc/default/grub` and run `sudo update-grub`.

## Credits

- [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock) — original module for GA102/GA103/GA104/GA106
- [JustAResearcher/gpu-unlock-ada](https://github.com/JustAResearcher/gpu-unlock-ada) — Ada Lovelace (AD10x) fork
- cmpunlocker — original V67/PLM technique

## License

GPL-2.0-only (same as upstream). NVIDIA firmware under its own terms — see `firmware/NOTICE.md`.
