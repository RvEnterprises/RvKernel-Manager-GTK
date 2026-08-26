# RvKernel Manager (GTK)

Take full control of your system's kernel. Tune performance, battery life, and more — now on the Linux desktop.

RvKernel Manager is a native GTK 4 application written in C. It is the desktop
port of [RvKernel-Manager](https://github.com/Rve27/RvKernel-Manager), the
Android kernel manager, bringing the same idea to regular Linux systems: a
clean interface to tweak and monitor a wide range of kernel parameters.

## Features

- **Dashboard** — live CPU usage per core, memory/ZRAM/swap pressure,
  uptime, distro, kernel version and hardware summary.
- **CPU** — per-cluster cpufreq control: governor selection, min/max
  frequency limits, energy performance preference (EPP), live frequencies.
- **GPU** — DRM driver detection plus generic devfreq control
  (governor, min/max frequency) for supported GPUs.
- **Battery** — capacity, status, voltage, temperature, power draw, health,
  cycle count and charge-threshold control where the kernel supports it.
- **Memory** — vm tunables (`swappiness`, `vfs_cache_pressure`,
  `dirty_ratio`, `dirty_background_ratio`), ZRAM size/algorithm/usage and
  TCP congestion-control selection.
- **About** — version info, license and links.

## Requirements

- Linux with GTK 4 (>= 4.10 recommended)
- `gtk4-devel` / `libgtk-4-dev` build packages
- Root privileges are required to launch: the app talks directly to
  sysfs/procfs. Started as a regular user it re-execs itself through
  polkit's `pkexec` (an authentication prompt appears) and exits with an
  error if `pkexec` is not installed. Running it via `sudo` works too.

## Build & Run

```sh
make            # builds bin/rvkernel-manager
make run        # build (if needed) and launch
./bin/rvkernel-manager
```

Install system-wide:

```sh
sudo make install PREFIX=/usr/local
```

## Configuration

Build-time options live in `Kconfig` with their defaults; `configs/config`
holds the chosen values. `make config` regenerates `.config` by merging the
two, and a missing `.config` is generated automatically at build time.

```sh
make config      # regenerate .config from configs/config + Kconfig defaults
make mrproper    # clean build artifacts and .config
```

Enable diagnostic logging in `.config`:

```
CONFIG_DEBUG=y
```

With `CONFIG_DEBUG=y` every compile gets `-DCONFIG_DEBUG` and the app emits
timestamped messages on stderr — startup and privilege handling,
sysfs/procfs access, page construction, refresh ticks and each applied
setting. Set it to `n` (or remove it) for a silent binary; all logging is
compiled out. Changing `.config` triggers a full rebuild.

The compiler is chosen in the same file: set `CONFIG_CC_CLANG=y` (and turn
`CONFIG_CC_GCC` off) to build with clang instead of gcc. An explicit
`CC=` on the command line or in the environment overrides the choice.

## Project structure

```
src/
├── main.c               entry point, pkexec self-elevation
├── application.[ch]     GtkApplication bootstrap + actions
├── ui/                  presentation layer
│   ├── window.[ch]      main window, header bar, toasts, refresh timer
│   ├── sidebar.[ch]     navigation list bound to the page stack
│   ├── theme/style.[ch] application CSS (light + dark palettes)
│   ├── components/
│   │   ├── widgets.[ch] reusable card/row/dropdown builders
│   │   └── circular_progress_indicator.[ch]
│   │                    animated usage rings on the dashboard
│   └── pages/
│       ├── pages.[ch]   page refresh plumbing
│       ├── page_dashboard.c  dashboard page
│       ├── page_cpu.c        cpufreq controls
│       ├── page_gpu.c        DRM/devfreq controls
│       ├── page_battery.c    battery monitoring + charge limit
│       ├── page_memory.c     vm tunables / zram / tcp cc
│       └── page_about.c      about page
├── core/                business logic (no UI types)
│   ├── system_info.[ch] host/kernel/memory snapshots, cpu usage sampler
│   ├── cpu.[ch]         cpufreq policy discovery + setters
│   ├── gpu.[ch]         drm cards + devfreq devices
│   ├── battery.[ch]     power_supply parsing + charge control
│   └── memory.[ch]      vm sysctls, zram, tcp congestion control
└── util/
    ├── sysfs.[ch]       sysfs/procfs read/write helpers
    ├── log.[ch]         diagnostic logging (CONFIG_DEBUG)
    └── format.[ch]      string tokenizing + human formatting
```

## License

GPL-3.0 — see [LICENSE](LICENSE). Original Android project by Rve27.
