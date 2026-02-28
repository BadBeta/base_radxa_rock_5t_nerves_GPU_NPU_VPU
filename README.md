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

## Kernel & Bootloader

- **Kernel**: Rockchip BSP 6.1 (`linux-6.1-stan-rkr4.1`)
- **U-Boot**: Radxa BSP fork (2024.10-based)
- **ATF**: ARM Trusted Firmware v2.12

All sources are downloaded at build time from GitHub — no local
tarballs or vendor blobs to manage.

## Defconfigs

Two Buildroot configurations are provided:

- **`nerves_defconfig`** (base) — Kernel, GPU, VPU, NPU, networking,
  audio HAL, Wayland libs. No compositor or browser.
- **`nerves_defconfig_full`** — Adds Weston compositor, Cog/WPE WebKit
  kiosk browser, PulseAudio, GStreamer with Rockchip plugins.

Select with:

```bash
# Base (default)
MIX_TARGET=rock5t mix firmware

# Full display/media stack
NERVES_DEFCONFIG=nerves_defconfig_full MIX_TARGET=rock5t mix firmware
```

## Custom Buildroot Packages

| Package | Description |
|---------|-------------|
| `rockchip-libmali-g610` | Proprietary Mali G610 userspace + EGL/GBM hook library |
| `rockchip-mpp` | Rockchip Media Process Platform (VPU) |
| `rockchip-rknpu2` | RKNN runtime library (librknnrt.so) |
| `rknpu` | Out-of-tree RKNPU kernel module (for non-BSP kernels) |
| `rknn-infer` | C binary for RKNN model inference (Port-based protocol) |
| `gstreamer-rockchip` | GStreamer plugins for MPP hardware codecs |
| `cog-ai-extension` | WebKit extension for AI overlay in Cog browser |
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

The first build compiles the entire Buildroot system (~45 min).
Subsequent `mix firmware` runs only rebuild the Elixir release (~30 sec).

### Flash via Maskrom Mode

With the device in maskrom mode (hold maskrom button, power on):

```bash
./flash_emmc.sh
```

This uses `rkdeveloptool` to write the firmware image to eMMC.

### OTA Update via SSH

Once the device is running and on the network:

```bash
cd test_app
mix upload 192.168.200.143
```

## Partition Layout (GPT)

| Region | Offset | Size | Description |
|--------|--------|------|-------------|
| idbloader | 32 KB | ~512 KB | DDR init + SPL |
| u-boot.itb | 8 MB | ~4 MB | U-Boot + ATF + DTB |
| U-Boot env | 16 MB | 128 KB | Nerves A/B slot state |
| Boot A/B | 32 MB | 64 MB each | Kernel, DTB, boot.scr (FAT) |
| Rootfs A/B | 160 MB | 1 GB each | SquashFS (read-only) |
| App data | ~2.2 GB | Remaining | EXT4 (persistent /data) |

## Serial Console

```bash
picocom -b 1500000 /dev/ttyUSB0
```

## Known Issues

**Radxa BSP U-Boot mkimage bug**: The U-Boot fork's `tools/mkimage`
writes `0xFFFFFFFF` as the multi-image terminator in boot.scr instead
of the correct `0x00000000`. This causes U-Boot to misparse the script
payload. The build system works around this by preferring the Buildroot
host mkimage or system mkimage.

## License

This system configuration is provided as-is. Individual components have
their own licenses (Linux kernel: GPL-2.0, Mali blob: proprietary ARM
license, etc.).
