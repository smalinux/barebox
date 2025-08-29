#!/bin/bash
#
# barebox_standalone.sh - Flash barebox bootloader to SD card for BeagleBone Black
#
# This script prepares an SD card with barebox bootloader for standalone booting
# on BeagleBone Black. It creates a bootable FAT32 partition and copies the
# necessary barebox components (MLO and barebox.bin).
#
# Usage: ./barebox_standalone.sh [--help] <SD_CARD_DEVICE>
#
# Arguments:
#   SD_CARD_DEVICE    Target SD card device (e.g., /dev/sdb)
#
# Options:
#   --help            Show this help message and exit
#
# Example:
#   ./barebox_standalone.sh /dev/sdb
#
# Prerequisites:
#   - barebox must be built in ./build/images/ directory
#   - Root privileges for SD card operations
#   - Target SD card device
#
# Files copied to SD card:
#   - MLO: barebox-am33xx-beaglebone-mlo.mmc.img -> MLO
#   - barebox.bin: barebox-am33xx-beaglebone.img -> barebox.bin
#
# Warning: This script will destroy all data on the target device!
#

# Show help and exit
show_help() {
    cat << 'EOF'
Usage: ./barebox_standalone.sh [--help] <SD_CARD_DEVICE>

Flash barebox bootloader to SD card for BeagleBone Black standalone booting.

Arguments:
    SD_CARD_DEVICE    Target SD card device (e.g., /dev/sdb)

Options:
    --help            Show this help message and exit

Example:
    ./barebox_standalone.sh /dev/sdb

Warning: This script will destroy all data on the target device!
EOF
    exit 0
}

# Check for help option
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    show_help
fi

# Exit on any error and enable debugging mode
set -e
set -x

# TODO: Make the environment more dynamic
# === Configuration ===
SD_CARD_DEVICE="${1}"                       # SD card device (e.g., /dev/sdb)
CROSS_COMPILE="arm-linux-gnueabihf-"        # Cross-compiler prefix
BASE="am335x_evm"
DEFCONFIG="${BASE}_defconfig"
UENV_FILE="${BASE}.env"                     # Default environment file
PARTITION_SIZE="+64M"                       # Partition size for bootloader
MOUNT_DIR="$(mktemp -d /tmp/sdcard.XXXXXX)"

# === Check for SD card device ===
if [ -z ${SD_CARD_DEVICE} ]; then
    echo -e "\e[31mVar \${1} is empty... Replace with your SD card device (e.g., /dev/sdb)\e[0m"
    exit 1
fi

# Setup bootloader defconfig based on the base configuration
prepare_sdcard() {
    echo "Preparing SD card: $SD_CARD_DEVICE"
    # Unmount any mounted partitions
    echo "Unmounting existing partitions on $SD_CARD_DEVICE..."
    sudo umount ${SD_CARD_DEVICE}* || true
    # Check for existing signatures and wipe them
    echo "Checking for existing filesystem signatures..."
    if sudo blkid ${SD_CARD_DEVICE}1 | grep -q "vfat"; then
        echo "Found existing VFAT signature. Wiping partition..."
        sudo wipefs -a ${SD_CARD_DEVICE}1
    fi
    # Recreate the partition table
    echo "Creating a new partition table and partition..."
    echo -e "o\nn\np\n1\n\n$PARTITION_SIZE\nt\ne\na\n1\nw" | sudo fdisk $SD_CARD_DEVICE
    # Mark the partition as bootable
    echo "Marking partition as bootable..."
    sudo parted $SD_CARD_DEVICE set 1 boot on
    # Format the partition with VFAT
    echo "Formatting the partition as VFAT..."
    sudo mkfs.vfat -F 32 ${SD_CARD_DEVICE}1
}

mount_sdcard() {
    echo "Mounting the SD card partition..."
    sudo mkdir -p $MOUNT_DIR
    sudo mount ${SD_CARD_DEVICE}1 $MOUNT_DIR
}

# Function to label the partition as "boot"
label_partition() {
    echo ">> Labeling the partition as 'boot'..."
    sudo fatlabel ${SD_CARD_DEVICE}1 boot || {
        echo "Error: Failed to label partition as 'boot'."
        exit 1
    }
}

# Copy bootloader and environment files to the SD card
copy_bootloader_to_fat() {
    echo "Copying bootloader components to FAT partition..."
    pushd .
        pushd .
            # Copy MLO (bootloader SPL) to the SD card partition
            sudo cp ./build/images/barebox-am33xx-beaglebone.img $MOUNT_DIR/barebox.bin
            sudo cp ./build/images/barebox-am33xx-beaglebone-mlo.mmc.img $MOUNT_DIR/MLO
        popd
    popd
}

# Clean up build files and unmount the SD card
cleanup() {
    echo "Cleaning up build files..."
    sudo umount $MOUNT_DIR
    sudo rm -r $MOUNT_DIR
}

prepare_sdcard
label_partition
mount_sdcard
copy_bootloader_to_fat
cleanup

echo "Bootloader build and flash complete! Insert the SD card into the BeagleBone Black and boot."
