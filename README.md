# KCHEAT KERNEL NEO 2

Linux 4.19.157 kernel for the **Realme GT Neo 2 (RMX3370)**, based on
Blackmanx `kernel_realme_sm8250` (`bigshot` branch). The kernel is adapted
at the ABI level so the closed-source RT hardware-breakpoint driver runs
correctly on this device, with ReSukiSu root integrated and memory reclaim
hardened against spurious OOM kills.

- **Version:** `4.19.157-perf+`
- **Base:** [Blackmanx kernel_realme_sm8250](https://github.com/Blackmanx/kernel_realme_sm8250) (`bigshot`)
- **SoC:** Qualcomm SM8250 (Snapdragon 865)
- **Toolchain:** Proton Clang 13.0.0
- **Root:** ReSukiSu v4.2.0-rc3 (KernelSU)
- **Installer:** AnyKernel3, no device check

> Flash the release zip directly, or build from this source. Both produce
> the same kernel.

---

## RT hardware-breakpoint driver — kernel-side ABI compatibility

The RT driver (`suvhwvwrtnb`) is a closed binary. It was built against a
different kernel struct layout, so it writes driver state to fixed offsets
and builds `perf_event_attr` with fixed values. Rather than patching the
binary, the kernel was adapted so those offsets and behaviours match what
the driver expects. The fixes below were all derived from disassembly and
live probing of the driver against this kernel — not from guesswork.

### Struct layout

| Location | Fix | Why |
|---|---|---|
| `include/linux/kobject.h` | One `ANDROID_KABI_RESERVE(4)` removed → `struct kobject` 96 → 88 bytes | The driver writes `cdev->owner` at `cdev+96`. With a 96-byte kobject, offset 96 is `cdev->ops`, so the driver overwrote the ops pointer and `open()` jumped to garbage, hanging the CPU until the watchdog rebooted the phone. |
| `include/linux/cdev.h` | `unsigned long __cdev_owner_pad` added before `owner` | Puts `cdev->owner` at 96 and `cdev->ops` at 104, matching the driver. |
| `include/linux/fs.h` | `private_data` / `f_security` order swapped | The driver writes `private_data` at `file+200`. With the stock order, offset 200 is `f_security` (the security-module pointer) — the driver corrupted it and `close()` hung. Now `private_data` is at 200, `f_security` at 208, `sizeof(struct file)` unchanged at 264. |
| `include/linux/module.h` (layout) | Module `init` at `0x170` | The driver was built expecting `init` at `0x170`; this kernel has it there, so `insmod` runs the driver's init and creates the `/dev` node. A mismatch here loads the module silently with init never running. |

### Hardware breakpoints (`perf_event`)

| Location | Fix | Why |
|---|---|---|
| `kernel/events/hw_breakpoint.c` (`register_wide_hw_breakpoint` only) | `sample_period` forced to 1 when the driver passes 0; `disabled` forced to 0; CPU 7 skipped for install; the returned `__percpu` pointer is held on the CPU 0 slot | The driver builds the event with `sample_period=0` (a counting event, never overflows) and `disabled=1`, so the overflow handler was never called and no hits were ever recorded. CPU 7 never answers the install IPI, so the installer waits forever — the skip applies only to breakpoint installation. |
| `arch/arm64/kernel/hw_breakpoint.c` | Single-step past the faulting instruction, then one-shot disable after a hit | The driver's overflow handler is not the default one, so the arch handler never single-stepped past the fault and re-triggered infinitely, spinning the target in userspace. Disabling the event after recording a hit breaks the loop. |

### Ioctl translation (`fs/ioctl.c`)

The tool speaks `0x801` (READ) and `0x803` (GET MODULE BASE); the driver
speaks `0x03181351` and `0x03181353`. The kernel translates them in
`vfs_ioctl`, gated on the command alone — **not** on the device major,
because the driver takes a dynamic major (460 / 461 / 462 depending on
load order) and a hardcoded major gate silently stops translating.

| Tool calls | Translated to | Driver operation |
|---|---|---|
| `0x801` | `0x03181351` | READ (pid, address, buffer, length) |
| `0x803` | `0x03181353` | GET MODULE BASE BY NAME |

`0x03181352` is WRITE, `0x03181355` is GET_HITS, `0x03181356` is
ADD / REMOVE, `0x03181354` is dead (`-EINVAL`).

### Module signing

`CONFIG_MODULE_SIG` is **off**. The driver is unsigned, so with signature
enforcement on, `insmod` fails with "Required key not available".
`CONFIG_MODVERSIONS` stays on.

The driver itself is **not** included in this repository — it is
third-party. Load your own `drv-c.ko` after flashing.

---

## Root — ReSukiSu

- ReSukiSu **v4.2.0-rc3** (version code 35171) built in, manual hook,
  no SUSFS.
- Use the matching ReSukiSu Manager APK (v4.2.0-rc3, 35171).
- `drivers/kernelsu/` is a real directory in this tree (the ReSukiSu
  kernel source), not a symlink.

---

## Memory management — OOM-killer hardening

`mm/page_alloc.c`:

- `min_free_kbytes` headroom doubled (`*16` → `*64` in the
  `init_per_zone_wmark_min` computation).
- `watermark_scale_factor` raised (`10` → `100`).

Effect: under a transient allocation spike, the kernel reclaims inactive
page cache before the OOM killer fires. Before this change, the OOM killer
(`oom_reaper`) terminated large foreground processes — MT Manager and the
root manager — during tool runs, even though ~3 GB of reclaimable inactive
cache was available. After the change, that no longer reproduces.

---

## What was changed (vs. the Blackmanx base)

| Area | Change |
|---|---|
| `include/linux/kobject.h` | One `ANDROID_KABI_RESERVE(4)` removed → kobject 96 → 88 |
| `include/linux/cdev.h` | `__cdev_owner_pad` added → `cdev->owner` at 96 |
| `include/linux/fs.h` | `private_data` / `f_security` order swapped (200 / 208) |
| `kernel/events/hw_breakpoint.c` | `sample_period=1`, `disabled=0`, skip CPU 7, hold `__percpu` slot |
| `arch/arm64/kernel/hw_breakpoint.c` | Single-step + one-shot disable after a hit |
| `fs/ioctl.c` | `0x801` → `0x03181351`, `0x803` → `0x03181353`, major-agnostic |
| `kernel/sched_assist/Kconfig` | `config LOCKING_PROTECT` added |
| `include/linux/sched.h`, `kernel/sched/sched.h` | `ux_im_flag` / locking fields added |
| `mm/page_alloc.c` | `min_free_kbytes *64`, `watermark_scale_factor 100` |
| `anykernel/` | Banner + `kernel.string` → `KCHEAT KERNEL NEO 2` |
| `drivers/kernelsu/` | ReSukiSu v4.2.0-rc3 kernel (real directory) |
| Config | `CONFIG_MODULE_SIG` off, `CONFIG_OPLUS_UX_IM_FLAG=y`, `CONFIG_LOCKING_PROTECT=y`, `CONFIG_KSU=y`, `CONFIG_KSU_MANUAL_HOOK=y` |

Full history is in [CHANGELOG.md](CHANGELOG.md). Build instructions are in
[BUILD.md](BUILD.md).

---

## Flashing (no build needed)

1. Download the release zip: **`KCHEAT-Kernel-Neo2.zip`**
   (23,726,396 bytes, SHA-256 `b7121d5de1d802e342d1a8d1c2add584a3b718c93fd355cc53666097cf647444`).
2. Reboot to recovery (TWRP / OrangeFox).
3. Flash the zip. No wipe needed. Device check is off — the phone reports
   `ro.product.device=RE879AL1`, not `RMX3370`, so a check would fail.
4. Reboot. The kernel version is `4.19.157-perf+`.

After flashing, install the ReSukiSu Manager APK (v4.2.0-rc3, 35171) for root
access, and load your RT driver (`insmod drv-c.ko`) as usual.

---

## Building from source

See [BUILD.md](BUILD.md) for the full guide. Short version:

```sh
# Proton Clang 13 in ~/proton-clang (or anywhere, adjust CLANG)
export ARCH=arm64 SUBARCH=arm64
export KBUILD_BUILD_USER=Amog KBUILD_BUILD_HOST=Us
make O=out vendor/sm8250_defconfig
make O=out -j$(nproc) Image \
  CC=clang LD=aarch64-linux-gnu-ld AR=llvm-ar NM=llvm-nm \
  OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip \
  LLVM=1 CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_ARM32=arm-linux-gnueabi-
```

Then copy `out/arch/arm64/boot/Image` into `anykernel/` and zip it from
inside that directory. See [BUILD.md](BUILD.md) for the exact flags,
the `LOCALVERSION` trap (`-perf`, not `-perf+`), and the packaging steps.

---

## Known limitations

- **CPU 7 is skipped for breakpoint installation only.** CPU 7 is the prime
  core and works normally for apps and games; the skip applies only to the
  `register_wide_hw_breakpoint` path, where the install IPI to CPU 7 hangs.
  General performance is not affected.
- **`drivers/soc/oplus/oppo_slpirst` is a dangling symlink.** It points
  outside the tree (`../../../../../../msm_amss/...`), to a directory that
  is not part of this source. It is like that in the upstream Blackmanx
  source too. It is only used when `CONFIG_OPLUS_FEATURE_SENSOR_CRASH_MONITOR`
  is enabled, which it is not in this build — the build never touches it.
- **The RT driver is not included.** It is third-party; bring your own
  `drv-c.ko`. The kernel only provides the ABI it needs.
- **Windows checkout: 3 files named `aux.*` are excluded.** Windows
  reserves the name `AUX` (a DOS device name), so `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/aux.c`,
  `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/aux.h` and
  `include/soc/arc/aux.h` cannot exist on an NTFS drive and are listed in
  `.gitignore`. They are not needed for the sm8250 build — Nouveau (NVIDIA
  GPU) and ARC are not enabled in this config. On Linux, they are present
  in the upstream Blackmanx source if you need them.

---

## Credits

- **Blackmanx** — the `kernel_realme_sm8250` base source.
- **ReSukiSu / KernelSU** — root solution.
- **osm0sis** — AnyKernel3.

---

## License

This kernel is licensed under the **GNU General Public License v2.0**
(see [LICENSE](LICENSE), and `COPYING` / `LICENSES/preferred/GPL-2.0` in
the tree). If you redistribute this source or a kernel built from it, keep
the license and the attribution above.
