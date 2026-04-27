# embtop
Minimal footprint embedded-focused system resources and processes monitor

`embtop` is a tiny Linux monitor for embedded targets. It intentionally avoids
`ncurses` and htop-style feature breadth: it samples `/proc` and common sysfs
nodes, then paints a compact ANSI text view.

Example output:

```text
embtop - minimal embedded monitor

CPU:   23%   (1.2GHz -> 800MHz scaling)
TEMP: 67C
MEM:  120MB / 256MB

Top processes:
PID      CPU     MEM  NAME
123      15%    10MB  camera
98       10%     8MB  encoder
```

## Build

```sh
make
```

For small embedded userspaces, override the compiler and flags as needed:

```sh
make CC=arm-linux-gnueabihf-gcc CFLAGS="-std=c11 -Os -static"
```

Source layout:

- `src/main.c`: CLI parsing, signal handling, and refresh loop.
- `src/system.c`: CPU, memory, thermal, and cpufreq sampling.
- `src/process.c`: process enumeration, CPU deltas, and sorting.
- `src/render.c`: compact ANSI text UI.
- `src/utils.c`: small file-reading helpers shared by procfs/sysfs readers.
- `include/embtop.h`: shared data structures and module interfaces.

## Usage

```sh
./embtop
./embtop -d 500 -n 3
./embtop -1
```

Options:

- `-d delay_ms`: refresh interval in milliseconds, default `1000`.
- `-n processes`: number of top processes to display, default `5`.
- `-1`: sample once and exit, useful for logs or watchdog scripts.

While running interactively, press `q` to quit.

## Data Sources

- CPU load: `/proc/stat`
- Memory: `/proc/meminfo`
- Processes: `/proc/<pid>/stat` and `/proc/<pid>/comm`
- CPU frequency: `/sys/devices/system/cpu/cpu0/cpufreq/*`
- Temperature: first readable `/sys/class/thermal/thermal_zone*/temp`

Missing sysfs data is displayed as `n/a`, which keeps the tool usable on
minimal boards that do not expose every node.
