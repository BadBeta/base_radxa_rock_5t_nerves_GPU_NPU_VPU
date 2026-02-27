#!/bin/bash

# post-createfs.sh - Post image creation script for Rock 5T Nerves system
#
# This script runs after the rootfs image is created.
# It assembles the final firmware image with bootloader components.

set -e

BINARIES_DIR=$1
NERVES_DEFCONFIG_DIR="${0%/*}"

# Find the build directory
BUILD_DIR=$(dirname "${BINARIES_DIR}")/build

echo "Running Rock 5T post-createfs script..."
echo "Binaries directory: ${BINARIES_DIR}"
echo "Build directory: ${BUILD_DIR}"

# Verify required files exist
required_files=(
    "Image"
    "rootfs.squashfs"
)

for file in "${required_files[@]}"; do
    if [ ! -f "${BINARIES_DIR}/${file}" ]; then
        echo "ERROR: Required file missing: ${BINARIES_DIR}/${file}"
        exit 1
    fi
done

# Check for device tree
DTB_FILE="${BINARIES_DIR}/rk3588-rock-5t.dtb"
if [ ! -f "${DTB_FILE}" ]; then
    # Try alternative naming
    DTB_FILE=$(find "${BINARIES_DIR}" -name "*rock*5t*.dtb" 2>/dev/null | head -1)
    if [ -z "${DTB_FILE}" ]; then
        echo "ERROR: Device tree file not found for Rock 5T"
        exit 1
    fi
    echo "Using device tree: ${DTB_FILE}"
fi

# ============================================
# Create Rockchip boot images
# ============================================
# RK3588 boot flow: DDR init (TPL) -> SPL -> ATF (BL31) -> U-Boot
# idbloader.img = DDR blob + SPL
# u-boot.itb = ATF + U-Boot (FIT image)

echo ""
echo "Creating Rockchip boot images..."

# ============================================
# Verify U-Boot images from Buildroot build
# ============================================
# Buildroot builds idbloader.img and u-boot.itb from source
# (Radxa U-Boot fork + ATF + rkbin DDR blob)

if [ ! -f "${BINARIES_DIR}/idbloader.img" ]; then
    echo "ERROR: idbloader.img not found — U-Boot build may have failed"
    exit 1
fi

if [ ! -f "${BINARIES_DIR}/u-boot.itb" ]; then
    echo "ERROR: u-boot.itb not found — U-Boot build may have failed"
    exit 1
fi

echo "U-Boot images present (built from source)"

# Find mkimage for boot script creation
UBOOT_DIR=$(find "${BUILD_DIR}" -maxdepth 1 -type d -name "uboot-*" 2>/dev/null | head -1)
if [ -n "${UBOOT_DIR}" ] && [ -x "${UBOOT_DIR}/tools/mkimage" ]; then
    MKIMAGE="${UBOOT_DIR}/tools/mkimage"
else
    MKIMAGE=""
fi

# ============================================
# Generate boot.scr (U-Boot boot script)
# ============================================
echo ""
echo "Creating boot script..."

BOOT_CMD="${BINARIES_DIR}/boot.cmd"
BOOT_SCR="${BINARIES_DIR}/boot.scr"

# Read kernel command line from cmdline.txt (minus root= which is set dynamically)
CMDLINE_FILE="${NERVES_DEFCONFIG_DIR}/cmdline.txt"
if [ -f "${CMDLINE_FILE}" ]; then
    # Strip root= and rootfstype= from cmdline — boot script sets these dynamically
    KERNEL_CMDLINE=$(cat "${CMDLINE_FILE}" | tr -d '\n' | sed 's/root=[^ ]* *//g; s/rootfstype=[^ ]* *//g')
    echo "Using cmdline.txt (without root=): ${KERNEL_CMDLINE}"
else
    KERNEL_CMDLINE="rootwait ro init=/sbin/init console=ttyS2,1500000n8 loglevel=7 net.ifnames=0 clk_ignore_unused regulator_ignore_unused"
    echo "WARNING: cmdline.txt not found, using default"
fi

# U-Boot env location must match fwup.conf UBOOT_ENV_OFFSET
# UBOOT_ENV_OFFSET = 32768 sectors = 0x8000 hex sectors
# UBOOT_ENV_COUNT = 256 sectors = 0x100 hex sectors = 128KB
UBOOT_ENV_SECTOR="0x8000"
UBOOT_ENV_SECTORS="0x100"
UBOOT_ENV_SIZE="0x20000"

cat > "${BOOT_CMD}" << EOF
# Boot script for Rock 5T Nerves (A/B partition switching)
#
# This script reads nerves_fw_active from U-Boot env to determine
# which rootfs partition to boot from:
#   active=a → root=/dev/mmcblk0p2 (rootfs-a, GPT partition 1)
#   active=b → root=/dev/mmcblk0p3 (rootfs-b, GPT partition 2)
#
# The boot FAT partition (kernel, DTB, boot.scr) is always GPT partition 0
# (mmc 0:1 in U-Boot). fwup rewrites the GPT to point partition 0 at
# either BOOT-A or BOOT-B offset.

echo "Rock 5T Nerves Boot"

# Set memory addresses for RK3588
if test -z "\${kernel_addr_r}"; then
    setenv kernel_addr_r 0x00400000
fi
if test -z "\${fdt_addr_r}"; then
    setenv fdt_addr_r 0x08300000
fi

# Boot device (eMMC = 0)
if test -z "\${devnum}"; then
    setenv devnum 0
fi

# ============================================================
# Determine active slot (A/B) from U-Boot environment
# ============================================================
# Load the Nerves U-Boot env block from eMMC.
# The env is at sector ${UBOOT_ENV_SECTOR} (${UBOOT_ENV_SIZE} bytes).
# fwup writes standard U-Boot env format (CRC32 + key=value pairs).
setenv nerves_fw_active

# Try reading env from eMMC and importing it
if mmc dev \${devnum}; then
    if mmc read 0x02000000 ${UBOOT_ENV_SECTOR} ${UBOOT_ENV_SECTORS}; then
        env import -b 0x02000000 ${UBOOT_ENV_SIZE} nerves_fw_active 2>/dev/null
    fi
fi

# Select rootfs partition based on active slot
if test "\${nerves_fw_active}" = "b"; then
    setenv nerves_root /dev/mmcblk0p3
    echo "Active slot: B (rootfs-b)"
else
    setenv nerves_root /dev/mmcblk0p2
    setenv nerves_fw_active a
    echo "Active slot: A (rootfs-a)"
fi

# Set boot arguments with dynamic root=
setenv bootargs "root=\${nerves_root} rootfstype=squashfs ${KERNEL_CMDLINE}"

# Load kernel from boot FAT partition
fatload mmc \${devnum}:1 \${kernel_addr_r} Image
fatload mmc \${devnum}:1 \${fdt_addr_r} rk3588-rock-5t.dtb

# Boot
booti \${kernel_addr_r} - \${fdt_addr_r}

echo "ERROR: booti failed!"
EOF

# Create boot.scr using mkimage
if [ -x "${MKIMAGE}" ]; then
    "${MKIMAGE}" -A arm64 -O linux -T script -C none -d "${BOOT_CMD}" "${BOOT_SCR}"
    echo "Created boot.scr"
elif command -v mkimage >/dev/null 2>&1; then
    mkimage -A arm64 -O linux -T script -C none -d "${BOOT_CMD}" "${BOOT_SCR}"
    echo "Created boot.scr using host mkimage"
else
    echo "WARNING: mkimage not found, boot.scr not created"
    cp "${BOOT_CMD}" "${BOOT_SCR}"
fi

# ============================================
# Copy additional files
# ============================================
echo ""
echo "Copying additional files..."

# Copy cmdline.txt to binaries
if [ -f "${NERVES_DEFCONFIG_DIR}/cmdline.txt" ]; then
    cp "${NERVES_DEFCONFIG_DIR}/cmdline.txt" "${BINARIES_DIR}/"
fi

# Create symlinks for fwup.conf expected names
ln -sf "rootfs.squashfs" "${BINARIES_DIR}/rootfs.img"

# Copy fwup configuration
cp "${NERVES_DEFCONFIG_DIR}/fwup.conf" "${BINARIES_DIR}/"
cp "${NERVES_DEFCONFIG_DIR}/fwup-revert.conf" "${BINARIES_DIR}/"

echo ""
echo "Rock 5T post-createfs script completed."
echo ""
echo "Output files in ${BINARIES_DIR}:"
ls -la "${BINARIES_DIR}/"*.img "${BINARIES_DIR}/"*.itb "${BINARIES_DIR}/"*.scr "${BINARIES_DIR}/"*.dtb "${BINARIES_DIR}/"*.bin 2>/dev/null | head -20 || true
