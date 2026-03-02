# Nerves System for Radxa Rock 5T (RK3588)

Custom [Nerves](https://nerves-project.org/) system for the
[Radxa Rock 5T](https://radxa.com/products/rock5/5t/) single-board computer,
with hardware-accelerated GPU, NPU, VPU, camera ISP, and display support.

## Hardware Support

| Subsystem | Implementation | Details |
|-----------|---------------|---------|
| **CPU** | Cortex-A76 + A55 (big.LITTLE) | RK3588 SoC |
| **GPU** | Mali G610 (proprietary blob) | Wayland EGL/GBM via libmali |
| **NPU** | RKNPU (in-tree BSP driver) | 6 TOPS via librknnrt.so + .rknn models |
| **VPU** | Rockchip MPP | H.264/H.265 hardware encode/decode |
| **ISP** | rkisp + rkisp1 + ispp + vpss | Image signal processing for MIPI cameras |
| **CSI** | rkcif + MIPI D-PHY/C-PHY | Two 4-lane MIPI CSI-2 camera inputs |
| **DSI** | Rockchip DRM + MIPI DC-PHY | One 4-lane MIPI DSI display output |
| **Ethernet** | Dual 2.5GbE (RTL8125B) | Both ports supported |
| **WiFi/BT** | RTL8852BE | Out-of-tree rtw89 driver |
| **Audio** | ALSA (HDMI + ES8316) | HDMI output, headphone jack |
| **Display** | DRM/KMS | HDMI output via Rockchip DRM |
| **HDMI Input** | V4L2 (rk_hdmirx) | Up to 4K60 capture, BGR3/NV24/NV16/NV12 |
| **GPIO** | sysfs / libgpiod | 40-pin header with SPI, I2C, I2S, PWM, ADC |

## Kernel & Bootloader

- **Kernel**: Rockchip BSP 6.1 (`linux-6.1-stan-rkr4.1`)
- **U-Boot**: Radxa BSP fork (2024.10-based)
- **ATF**: ARM Trusted Firmware v2.12

All sources are downloaded at build time from GitHub — no local
tarballs or vendor blobs to manage.

## 40-Pin GPIO Header

The 40-pin header exposes SPI, I2C, I2S, PWM, ADC, and GPIO. All bus
peripherals are enabled in the device tree by default.

### Pin Table

| Pin | Assignment | GPIO | Linux # | Device |
|-----|-----------|------|---------|--------|
| 1 | 3.3V | — | — | Power |
| 2 | 5V | — | — | Power |
| 3 | GPIO / LED0 | GPIO4_B3 | 139 | LED (active high) |
| 4 | 5V | — | — | Power |
| 5 | GPIO / LED1 | GPIO4_B2 | 138 | LED (active high) |
| 6 | GND | — | — | Ground |
| 7 | I2C8 SDA | GPIO1_D7 | 63 | `/dev/i2c-8` |
| 8 | UART2 TX | GPIO0_B5 | 13 | Serial console |
| 9 | GND | — | — | Ground |
| 10 | UART2 RX | GPIO0_B6 | 14 | Serial console |
| 11 | GPIO / LED2 | GPIO4_A3 | 131 | LED (active high) |
| 12 | I2S2 SCLK | GPIO3_B5 | 109 | I2S audio clock |
| 13 | I2C3 SCL | GPIO4_C5 | 149 | `/dev/i2c-3` |
| 14 | GND | — | — | Ground |
| 15 | I2C3 SDA | GPIO4_C4 | 148 | `/dev/i2c-3` |
| 16 | GPIO / LED3 | GPIO1_D6 | 62 | LED (active high) |
| 17 | 3.3V | — | — | Power |
| 18 | GPIO / BTN0 | GPIO1_B5 | 45 | Button (active low) |
| 19 | SPI0 MOSI | GPIO4_A1 | 129 | `/dev/spidev0.0` |
| 20 | GND | — | — | Ground |
| 21 | SPI0 MISO | GPIO4_A0 | 128 | `/dev/spidev0.0` |
| 22 | ADC IN6 | SARADC_VIN6 | — | `/sys/bus/iio/...` (1.8V max) |
| 23 | SPI0 CLK | GPIO4_A2 | 130 | `/dev/spidev0.0` |
| 24 | SPI0 CS0 | GPIO4_B1 | 137 | `/dev/spidev0.0` |
| 25 | GND | — | — | Ground |
| 26 | GPIO / BTN1 | GPIO1_A4 | 36 | Button (active low) |
| 27 | GPIO / BTN2 | GPIO1_B0 | 40 | Button (active low) |
| 28 | GPIO / BTN3 | GPIO1_A7 | 39 | Button (active low) |
| 29 | GPIO / BTN4 | GPIO4_A4 | 132 | Button (active low) |
| 30 | GND | — | — | Ground |
| 31 | PWM0 | GPIO1_A2 | 34 | `pwmchip0/pwm0` |
| 32 | I2C8 SCL | GPIO1_D5 | 61 | `/dev/i2c-8` |
| 33 | PWM8 | GPIO3_A7 | 103 | `pwmchip2/pwm0` |
| 34 | GND | — | — | Ground |
| 35 | I2S2 LRCK | GPIO3_B7 | 111 | I2S word clock |
| 36 | PWM2 | GPIO3_B1 | 105 | `pwmchip0/pwm2` |
| 37 | PWM3 | GPIO1_A7 | 39 | `pwmchip0/pwm3` |
| 38 | I2S2 SDI | GPIO3_C0 | 112 | I2S data in |
| 39 | GND | — | — | Ground |
| 40 | I2S2 SDO | GPIO3_B6 | 110 | I2S data out |

### Serial Console (Pins 8, 10)

UART2 is the default serial console at 1,500,000 baud:

```bash
picocom -b 1500000 /dev/ttyUSB0
```

### SPI0 (Pins 19, 21, 23, 24)

SPI bus 0 is enabled with one chip-select. A `spidev` device is
configured at `/dev/spidev0.0` (max 10 MHz default).

### I2C3 (Pins 13, 15) and I2C8 (Pins 7, 32)

Two I2C buses on the header. Detect connected devices:

```bash
i2cdetect -y 3
i2cdetect -y 8
```

### I2S2 Audio (Pins 12, 35, 38, 40)

I2S 2-channel audio interface for external DAC/ADC boards (SCLK,
LRCK, SDI, SDO).

### PWM (Pins 31, 33, 36, 37)

Four PWM channels are available via sysfs:

| Pin | PWM | sysfs chip | Channel |
|-----|-----|-----------|---------|
| 31 | PWM0 | `pwmchip0` | `pwm0` |
| 33 | PWM8 | `pwmchip2` | `pwm0` |
| 36 | PWM2 | `pwmchip0` | `pwm2` |
| 37 | PWM3 | `pwmchip0` | `pwm3` |

Example — 1 kHz at 50% duty cycle:

```bash
echo 0 > /sys/class/pwm/pwmchip0/export
echo 1000000 > /sys/class/pwm/pwmchip0/pwm0/period
echo 500000 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle
echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable
```

### GPIO LEDs (Pins 3, 5, 11, 16) and Buttons (Pins 18, 26, 27, 28, 29)

Four pins are configured as LED outputs (active high) and five as
button inputs (active low, directly readable via `/sys/class/gpio/`
or libgpiod).

### ADC (Pin 22)

SARADC input channel 6. Maximum input voltage is **1.8V** — exceeding
this will damage the SoC. Read via the IIO subsystem:

```bash
cat /sys/bus/iio/devices/iio:device0/in_voltage6_raw
```

### PDM Microphone (25-Pin FPC — Not on 40-Pin Header)

A separate 25-pin FPC connector provides PDM microphone input
(GPIO4_PD0–PD5). The PDM1 node is commented out in the device tree;
uncomment it when a microphone board is connected.

## Camera (MIPI CSI)

The Rock 5T has two 4-lane MIPI CSI-2 camera connectors:

| Connector | FPC | Pitch | Lanes | D-PHY |
|-----------|-----|-------|-------|-------|
| **CAM0** | 31-pin (Hirose FH35C-31S-0.3SHW) | 0.3 mm | 4 | csi2_dphy0 |
| **CAM1** | 31-pin (Hirose FH35C-31S-0.3SHW) | 0.3 mm | 4 | csi2_dphy1 |

### Supported Sensor Drivers

All 17 drivers are compiled into the kernel:

| Driver | Sensor | Resolution | Common Module |
|--------|--------|-----------|---------------|
| imx219 | Sony IMX219 | 8 MP | RPi Camera v2, Radxa Camera 8M |
| imx214 | Sony IMX214 | 13 MP | Various |
| imx415 | Sony IMX415 | 8 MP 4K | Radxa Camera 4K |
| imx464 | Sony IMX464 | 4 MP | Low-light |
| imx577 | Sony IMX577 | 12 MP | RPi HQ Camera |
| ov5647 | OmniVision OV5647 | 5 MP | RPi Camera v1, OKDO 5MP |
| ov4689 | OmniVision OV4689 | 4 MP | Security |
| ov5695 | OmniVision OV5695 | 5 MP | Laptop |
| ov7251 | OmniVision OV7251 | VGA | IR/depth |
| ov13850 | OmniVision OV13850 | 13 MP | Mobile |
| ov13855 | OmniVision OV13855 | 13 MP | Mobile |
| ov50c40 | OmniVision OV50C40 | 50 MP | High-res |
| gc2053 | GalaxyCore GC2053 | 2 MP | Budget |
| gc2093 | GalaxyCore GC2093 | 2 MP | Budget |
| gc8034 | GalaxyCore GC8034 | 8 MP | Budget |
| sc4336 | SmartSens SC4336 | 4 MP | Security |
| os04a10 | OmniVision OS04A10 | 4 MP | Security |

### Enabling a Camera

Cameras require a device tree overlay (or DTS patch) to connect the
sensor, D-PHY, CIF, and ISP nodes. Example overlays are provided in
[`dts-overlays/`](dts-overlays/):

- `rock5t-camera-imx219.dts` — IMX219 (RPi Camera v2) on CAM0
- `rock5t-camera-ov5647.dts` — OV5647 (RPi Camera v1) on CAM0

See [`dts-overlays/README.md`](dts-overlays/README.md) for compilation
and integration instructions.

## Display (MIPI DSI)

One 4-lane MIPI DSI connector for LCD panels:

| Connector | FPC | Pitch | Lanes | PHY |
|-----------|-----|-------|-------|-----|
| **DSI0** | 39-pin (Hirose FH35C-39S-0.3SHW) | 0.3 mm | 4 | mipi_dcphy0 |

The DSI controller and DC-PHY are compiled in but disabled by default
(no panel attached). To enable a panel, apply a device tree overlay.
An example is provided in
[`dts-overlays/rock5t-display-dsi.dts`](dts-overlays/rock5t-display-dsi.dts).

Panel-specific initialization sequences, timings, and GPIO wiring must
be adapted from the panel datasheet.

## ISP (Image Signal Processor)

The following Rockchip ISP and video pipeline drivers are enabled:

| Driver | Purpose |
|--------|---------|
| rkisp | Rockchip ISP v2 (3A, denoising, HDR) |
| rkisp1 | Rockchip ISP v1 (legacy compat) |
| ispp | ISP post-processor (NR, sharpening) |
| rkcif | CIF MIPI/LVDS/DVP receiver |
| vpss | Video process subsystem |

These provide the full hardware pipeline from MIPI sensor input
through ISP processing to V4L2 video output.

## Buildroot Configuration

The system uses `nerves_defconfig` which includes: kernel, GPU, VPU, NPU,
camera ISP, networking, audio HAL, and Wayland libs.

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

## License

This system configuration is provided as-is. Individual components have
their own licenses (Linux kernel: GPL-2.0, Mali blob: proprietary ARM
license, etc.).
