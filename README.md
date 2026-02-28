# Nerves System for Radxa Rock 5T (RK3588)

Custom [Nerves](https://nerves-project.org/) system for the
[Radxa Rock 5T](https://radxa.com/products/rock5/5t/) single-board computer,
with hardware-accelerated GPU, NPU, and VPU support.

## Hardware Support

| Subsystem | Implementation | Details |
|-----------|---------------|---------|
| **CPU** | Cortex-A76 + A55 (big.LITTLE) | RK3588 SoC |
| **GPU** | Mali G610 (proprietary blob) | Wayland EGL/GBM via libmali |
| **NPU** | RKNPU (in-tree BSP driver) | 6 TOPS via librknnrt.so + .rknn models |
| **VPU** | Rockchip MPP | H.264/H.265 hardware encode/decode |
| **Ethernet** | Dual 2.5GbE (RTL8125B) | Both ports supported |
| **WiFi/BT** | RTL8852BE | Out-of-tree rtw89 driver |
| **Audio** | ALSA (HDMI + ES8316) | HDMI output, headphone jack |
| **Display** | DRM/KMS | HDMI output via Rockchip DRM |
| **HDMI Input** | V4L2 (rk_hdmirx) | Up to 4K60 capture, BGR3/NV24/NV16/NV12 |

## Kernel & Bootloader

- **Kernel**: Rockchip BSP 6.1 (`linux-6.1-stan-rkr4.1`)
- **U-Boot**: Radxa BSP fork (2024.10-based)
- **ATF**: ARM Trusted Firmware v2.12

All sources are downloaded at build time from GitHub — no local
tarballs or vendor blobs to manage.

## Buildroot Configuration

The system uses `nerves_defconfig` which includes: kernel, GPU, VPU, NPU,
networking, audio HAL, and Wayland libs.

## Custom Buildroot Packages

| Package | Description |
|---------|-------------|
| `rockchip-libmali-g610` | Proprietary Mali G610 userspace + EGL/GBM hook library |
| `rockchip-mpp` | Rockchip Media Process Platform (VPU) |
| `rockchip-rknpu2` | RKNN runtime library (librknnrt.so) |
| `gstreamer-rockchip` | GStreamer plugins for MPP hardware codecs |
| `rtw89-oot` | Out-of-tree RTL8852BE WiFi driver |

## Building

### Prerequisites

- Elixir 1.17+
- Nerves Bootstrap: `mix archive.install hex nerves_bootstrap`
- Standard Nerves host dependencies
  ([installation guide](https://hexdocs.pm/nerves/installation.html))

### Build the System + Test App

```bash
cd test_app
export MIX_TARGET=rock5t
mix deps.get
mix firmware
```

### Flash via Maskrom Mode

With the device in maskrom mode (hold maskrom button, power on):

```bash
./flash_emmc.sh
```

This uses `rkdeveloptool` to write the firmware image to eMMC.

## Partition Layout (GPT)

> **Note:** The partition layout in `fwup.conf` is sized for the Rock 5T's
> 64GB eMMC (58GB actual / 122,142,720 sectors). The app data partition
> count (`APP_PART_COUNT`) is hardcoded to fit this specific eMMC size.
> If using a different eMMC capacity, adjust `APP_PART_COUNT` in `fwup.conf`
> to avoid writing past the end of the device.

| Region | Offset | Size | Description |
|--------|--------|------|-------------|
| idbloader | 32 KB | ~512 KB | DDR init + SPL |
| u-boot.itb | 8 MB | ~4 MB | U-Boot + ATF + DTB |
| U-Boot env | 16 MB | 128 KB | Nerves A/B slot state |
| Boot A/B | 32 MB | 64 MB each | Kernel, DTB, boot.scr (FAT) |
| Rootfs A/B | 160 MB | 1 GB each | SquashFS (read-only) |
| App data | ~2.2 GB | ~55.5 GB | EXT4 (persistent /data) |

## Booting from MicroSD Card

The default configuration targets eMMC (`/dev/mmcblk0`). To boot from a
MicroSD card instead, the following files need changes:

| File | Change |
|------|--------|
| `fwup.conf` | Change `NERVES_FW_DEVPATH` to `/dev/mmcblk1` |
| `fwup.conf` | Adjust `APP_PART_COUNT` for the SD card's capacity |
| `rootfs_overlay/etc/erlinit.config` | Change boot mount from `/dev/mmcblk0p1` to `/dev/mmcblk1p1` |
| `post-createfs.sh` | Change `mmcblk0p2`/`mmcblk0p3` root device references to `mmcblk1p*` |
| `test_app/rootfs_overlay/etc/erlinit.config` | Same boot mount change as above (if using test_app) |

On RK3588, eMMC is `mmcblk0` and MicroSD is `mmcblk1`. Flash to SD card
using `fwup` or `mix burn` instead of `flash_emmc.sh`.

## Serial Console

```bash
picocom -b 1500000 /dev/ttyUSB0
```

## License

This system configuration is provided as-is. Individual components have
their own licenses (Linux kernel: GPL-2.0, Mali blob: proprietary ARM
license, etc.).
