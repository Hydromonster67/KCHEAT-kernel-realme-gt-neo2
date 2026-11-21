# Changelog

## `fb43f9e92` — 2026-11-21

The release commit.

- **Scheduler:** `CONFIG_OPLUS_UX_IM_FLAG=y` + `CONFIG_LOCKING_PROTECT=y`,
  with the missing Kconfig entry and the `ux_im_flag` / locking fields
  added to `struct task_struct` (`include/linux/sched.h`) and `struct rq`
  (`kernel/sched/sched.h`).
- **Memory management:** `mm/page_alloc.c` — `min_free_kbytes` headroom
  `*16` → `*64`, `watermark_scale_factor` `10` → `100`. The OOM killer
  no longer terminates MT Manager and the root manager during tool runs.
- **Installer:** AnyKernel `Banner` and `kernel.string` →
  `KCHEAT KERNEL NEO 2`.
- Image `56,080,400` bytes, md5 `c85715dc9ab9cb1dbbbe6bdd70ad4820`,
  built 2026-11-21 11:56:57 +06. Zip `KCHEAT-Kernel-Neo2.zip`
  (23,726,396 bytes, SHA-256 `b7121d5de1d802e342d1a8d1c2add584a3b718c93fd355cc53666097cf647444`).
- **Verified:** tool works, root works, MT Manager keeps root across a
  tool run.

## `401e09947` — 2026-11-21

- **Ioctl translate made major-agnostic:** the translate in `fs/ioctl.c`
  was gated on hardcoded major `460`, but the driver takes a dynamic major
  (460 / 461 / 462). It now translates on the command alone
  (`0x801` → READ `0x03181351`, `0x803` → GET BASE `0x03181353`).
- **Verified:** first build where the tool works end to end. Remaining
  issue at the time: MT Manager terminated by `oom_reaper` when the tool
  ran (fixed in `fb43f9e92`).

## Earlier — 2026-11-21

- **Module signing off:** `CONFIG_MODULE_SIG` disabled — the unsigned RT
  driver failed `insmod` with "Required key not available" on the stock
  Blackmanx config. **Verified:** driver loads (`insmod` rc=0, `/dev`
  node created); the tool still did not work (the major-gated translate
  — fixed in `401e09947`).
- **RT driver patches applied to the Blackmanx base:** kobject shrink
  (96 → 88), `cdev->owner` at 96, `private_data` at 200, hw-breakpoint
  `sample_period=1` / `disabled=0` / CPU 7 skip, arch single-step +
  one-shot disable, ioctl translate `0x803` → GET BASE / `0x801` → READ.
  ReSukiSu v4.2.0-rc3 built in. **Verified:** driver loaded, but the tool
  failed `insmod` on module signature (fixed by the signing change above).
- **ReSukiSu v4.2.0-rc3 added to the clean Blackmanx base.**
  **Verified:** root manager works.
- **Clean Blackmanx source, no modifications.** **Verified:** boots and
  runs. This established the baseline that the later lag on the old tree
  came from our own modifications there, not from the source.
