#!/bin/bash
# Flash Nerves firmware to Rock 5T eMMC via rkdeveloptool
# Device must be in MASKROM mode

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARTIFACT_DIR="$SCRIPT_DIR/.nerves/artifacts/nerves_system_rock5t-portable-0.1.0"
LOADER=$(find "$ARTIFACT_DIR/build" -name "rk3588_spl_loader*.bin" 2>/dev/null | head -1)
FW_FILE="$SCRIPT_DIR/test_app/_build/rock5t_dev/nerves/images/test_app.fw"
IMAGE="/tmp/rock5t_test.img"

echo "=== Rock 5T eMMC Flash (Maskrom Mode) ==="
echo ""

# Check files exist
[ -f "$LOADER" ] || { echo "ERROR: Loader not found: $LOADER"; exit 1; }
[ -f "$FW_FILE" ] || { echo "ERROR: Firmware not found: $FW_FILE"; exit 1; }

echo "Loader: $LOADER"
echo "Firmware: $FW_FILE"
echo ""

# Step 1: Create full image from .fw file
echo "Step 1: Creating disk image from firmware..."
rm -f "$IMAGE"
fwup -a -d "$IMAGE" -i "$FW_FILE" -t complete --unsafe

# Step 2: Truncate image to only include boot + rootfs-a data
echo "Step 2: Truncating image to essential data..."
TRUNCATE_SIZE=$((1200 * 1024 * 1024))
truncate -s $TRUNCATE_SIZE "$IMAGE"
echo "Created: $IMAGE ($(du -h "$IMAGE" | cut -f1))"
echo ""

# Step 3: Check device
echo "Step 3: Checking for Rockchip device..."
rkdeveloptool ld
echo ""

# Step 4: Download loader (initializes DRAM)
echo "Step 4: Downloading loader (initializes DRAM)..."
rkdeveloptool db "$LOADER"
sleep 2
echo ""

# Step 5: Write image to eMMC
echo "Step 5: Writing image to eMMC..."
rkdeveloptool wl 0 "$IMAGE"
echo ""

# Step 6: Reboot
echo "Step 6: Rebooting..."
rkdeveloptool rd
echo ""

echo "=== Flash complete! ==="
echo "Watch serial console: picocom -b 1500000 /dev/ttyUSB0"
