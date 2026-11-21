# Building KCHEAT KERNEL NEO 2

How to build this kernel from source, step by step. Tested on Ubuntu 24.04.

## Requirements

- ~15 GB free disk (source + build output)
- Proton Clang **13.0.0** (the toolchain this kernel was built with)
- Standard kernel build tools: `bc`, `cpio`, `flex`, `bison`, `m4`, `gcc`,
  `make`, `git`, `zip`, `python3`

```sh
sudo apt install bc cpio flex bison m4 gcc make git zip python3
```

## 1. Get the toolchain

```sh
git clone --depth=1 https://github.com/kdrag0n/proton-clang.git ~/proton-clang
# or use any proton-clang 13 checkout
export CLANG=~/proton-clang/bin
```

## 2. Configure

```sh
export ARCH=arm64 SUBARCH=arm64
export KBUILD_BUILD_USER=Amog KBUILD_BUILD_HOST=Us

make O=out vendor/sm8250_defconfig
```

This produces `out/.config` with:

- `CONFIG_OPLUS_UX_IM_FLAG=y`
- `CONFIG_LOCKING_PROTECT=y`
- `CONFIG_KSU=y`, `CONFIG_KSU_MANUAL_HOOK=y` (ReSukiSu, no SUSFS)
- `CONFIG_MODULE_SIG` **not set** (so the unsigned RT driver loads)
- `CONFIG_LOCALVERSION="-perf"` — see the trap below

To change a config option, use `./scripts/config` and re-run
`olddefconfig` with the same flags:

```sh
./scripts/config --file out/.config --disable CONFIG_FOO
make O=out olddefconfig
```

## 3. Build

```sh
make O=out -j$(nproc) Image \
  HOSTCC=gcc HOSTCXX=g++ HOSTLD=/usr/bin/ld \
  CC=$CLANG/clang \
  LD=$CLANG/aarch64-linux-gnu-ld \
  AR=$CLANG/llvm-ar NM=$CLANG/llvm-nm \
  OBJCOPY=$CLANG/llvm-objcopy OBJDUMP=$CLANG/llvm-objdump \
  STRIP=$CLANG/llvm-strip \
  LLVM=1 \
  CROSS_COMPILE=$CLANG/aarch64-linux-gnu- \
  CROSS_COMPILE_ARM32=$CLANG/arm-linux-gnueabi-
```

A full build takes ~40 minutes on 2 cores (faster with more cores).
The output is `out/arch/arm64/boot/Image` (~56 MB).

### The LOCALVERSION trap

This is a dirty git tree, so the kernel build appends `+` to the version.
`CONFIG_LOCALVERSION` must be **`-perf`**, so the resulting version is exactly
`4.19.157-perf+` — which is what the RT driver was built against
(its `vermagic`). If you set `-perf+`, the version becomes `4.19.157-perf++`
and the driver will refuse to load.

Verify after the build:

```sh
strings out/arch/arm64/boot/Image | grep -m1 "Linux version 4.19.157"
# Linux version 4.19.157-perf+ (Amog@Us) ... SMP PREEMPT ...
```

Also check the ReSukiSu strings are present (`strings Image | grep -c -i kernelsu`
should be ~364) and that the OPLUS options made it into the real config:

```sh
grep -E "CONFIG_OPLUS_UX_IM_FLAG|CONFIG_LOCKING_PROTECT" out/include/generated/autoconf.h
```

> Do **not** trust `/proc/config.gz` (IKCONFIG) for what was compiled — on
> this tree it is generated from the defconfig, not from `.config`.
> `out/include/generated/autoconf.h` is the real config.

## 4. Package (AnyKernel3)

```sh
cp out/arch/arm64/boot/Image anykernel/Image
cd anykernel
zip -r ../KCHEAT-Kernel-Neo2.zip . -x "*.git*"
cd ..
```

The zip is flashed in recovery. Notes:

- The zip is built from **inside** `anykernel/`, so all files are at the
  zip root.
- `anykernel.sh` has `do.devicecheck=0` — the phone reports
  `ro.product.device=RE879AL1`, not `RMX3370`, so a device check would fail.
- The flashing banner reads **KCHEAT KERNEL NEO 2** (`anykernel/Banner`
  and `kernel.string` in `anykernel.sh`).
- `do.modules=0` — no modules are shipped; the RT driver is loaded
  separately with `insmod`.

## 5. Flash and test

1. Flash the zip in recovery, reboot.
2. Check the version: `uname -a` → `4.19.157-perf+`.
3. Install the ReSukiSu Manager APK (v4.2.0-rc3, 35171) and confirm root.
4. Load the RT driver: `insmod /data/local/tmp/drv-c.ko` — it should
   create a random 6-character `/dev` node. (Push the `.ko` to
   `/data/local/tmp/`, not `/sdcard` — `/sdcard` may not be mounted
   right after boot.)
5. Run the tool and confirm it works, then confirm MT Manager still has
   root after a tool run.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `insmod` fails: "Required key not available" | `CONFIG_MODULE_SIG` got turned back on. Disable it in `out/.config` and rebuild. |
| Driver loads but no `/dev` node appears | The module init offset does not match this kernel layout. Do not flash a kernel built from a different tree — this source has the fix. |
| `ioctl 0x801` returns `-EINVAL` | The ioctl translate is missing. This source has it in `fs/ioctl.c`. |
| Build fails at `prepare0` / `bc: not found` | Build tools missing (common after a VM/container reset). Reinstall `bc cpio flex bison m4`. |
| Build fails with "No space left on device" | Do not put the source or build output in `/tmp` (512 MB tmpfs). Keep everything under the home directory. |
| `perf++` in the version string | `CONFIG_LOCALVERSION` is `-perf+`. Set it to `-perf` (see the trap above). |
| Tool works but MT Manager loses root | That was the driver3 bug. This source has the watermark fix in `mm/page_alloc.c` — make sure you built from this tree, not an older one. |
