# RvKernel Manager (GTK)

Take full control of your system's kernel. Tune performance, battery life, and more — now on the Linux desktop.

RvKernel Manager is a native GTK 4 application written in C. It is the desktop
port of [RvKernel-Manager](https://github.com/Rve27/RvKernel-Manager), the
Android kernel manager, bringing the same idea to regular Linux systems: a
clean interface to tweak and monitor a wide range of kernel parameters.

## Features

- **Dashboard** — live CPU usage per core, memory/swap pressure, uptime,
  load average, distro, kernel version and hardware summary.
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
- Root privileges only needed to *apply* changes; everything is viewable as a
  normal user. Launch with `sudo ./bin/rvkernel-manager` or use the desktop
  file's "Launch as root" action (via pkexec).

## Build & Run

```sh
make            # builds bin/rvkernel-manager
./bin/rvkernel-manager
```

Install system-wide:

```sh
sudo make install PREFIX=/usr/local
```

## Project structure

```
src/
├── main.c               entry point
├── application.[ch]     GtkApplication bootstrap + actions
├── ui/                  presentation layer
│   ├── window.[ch]      main window, navigation, toasts
│   ├── style.[ch]       application CSS
│   ├── widgets.[ch]     reusable card/row builders
│   ├── page_dashboard.c dashboard page
│   ├── page_cpu.c       cpufreq controls
│   ├── page_gpu.c       DRM/devfreq controls
│   ├── page_battery.c   battery monitoring + charge limit
│   ├── page_memory.c    vm tunables / zram / tcp cc
│   └── page_about.c     about page
├── core/                business logic (no UI types)
│   ├── system_info.[ch] host/kernel/memory snapshots, cpu usage sampler
│   ├── cpu.[ch]         cpufreq policy discovery + setters
│   ├── gpu.[ch]         drm cards + devfreq devices
│   ├── battery.[ch]     power_supply parsing + charge control
│   ├── memory.[ch]      vm sysctls, zram, tcp congestion control
└── util/
    ├── sysfs.[ch]       sysfs/procfs read/write helpers
    └── format.[ch]      string tokenizing + human formatting
```

## License

GPL-3.0 — see [LICENSE](LICENSE). Original Android project by Rve27.
