#!/bin/bash
#
# Barebox SD Card Setup Script
#
SD_CARD_DEVICE="${1}"                       # SD card device (e.g., /dev/sdb)

# Define missing variables
PARTITION_SIZE="+1G"                        # Size for boot partition (adjust as needed)
MOUNT_DIR="/mnt/sdcard_boot"               # Mount point for SD card

#CROSS_COMPILE="arm-linux-gnueabihf-"      # Cross-compiler prefix
#ARCH=arm
#
#export CROSS_COMPILE=$CROSS_COMPILE
#export ARCH=$ARCH
#
#
#make am335x_mlo_defconfig
#make -j12
#
#make  omap_defconfig
#make -j12
#
#
## TFTP
#cp ./images/barebox-am33xx-beaglebone.img /tftpboot/barebox.bin
#cp ./images/barebox-am33xx-beaglebone-mlo.img /tftpboot/MLO

#==============================================================================
# Setup Barebox defconfig based on the base configuration

# Function to check if device exists and is a block device
check_device() {
    if [ -z "$SD_CARD_DEVICE" ]; then
        echo "Error: Please provide SD card device as argument (e.g., ./make.sh /dev/sdb)"
        exit 1
    fi
    
    if [ ! -b "$SD_CARD_DEVICE" ]; then
        echo "Error: $SD_CARD_DEVICE is not a valid block device"
        exit 1
    fi
    
    echo "Warning: This will completely wipe $SD_CARD_DEVICE"
    echo "Press Ctrl+C within 5 seconds to cancel..."
    sleep 5
}

prepare_sdcard() {
    echo "Preparing SD card: $SD_CARD_DEVICE"
    
    # Unmount any mounted partitions
    echo "Unmounting existing partitions on $SD_CARD_DEVICE..."
    sudo umount ${SD_CARD_DEVICE}* 2>/dev/null || true
    
    # Check for existing signatures and wipe them
    echo "Checking for existing filesystem signatures..."
    if sudo blkid ${SD_CARD_DEVICE}1 2>/dev/null | grep -q "vfat"; then
        echo "Found existing VFAT signature. Wiping partition..."
        sudo wipefs -a ${SD_CARD_DEVICE}1 2>/dev/null || true
    fi
    
    # Wipe the beginning of the disk to clear any existing partition table
    echo "Clearing existing partition table..."
    sudo dd if=/dev/zero of=$SD_CARD_DEVICE bs=1M count=1 2>/dev/null
    
    # Recreate the partition table using a more reliable method
    echo "Creating a new partition table and partition..."
    sudo parted -s $SD_CARD_DEVICE mklabel msdos
    sudo parted -s $SD_CARD_DEVICE mkpart primary fat32 1MiB 1GiB
    sudo parted -s $SD_CARD_DEVICE set 1 boot on
    
    # Wait for the system to recognize the new partition
    sleep 2
    sudo partprobe $SD_CARD_DEVICE
    sleep 2
    
    # Format the partition with VFAT
    echo "Formatting the partition as VFAT..."
    sudo mkfs.vfat -F 32 ${SD_CARD_DEVICE}1
}

mount_sdcard() {
    echo "Mounting the SD card partition..."
    sudo mkdir -p $MOUNT_DIR
    sudo mount ${SD_CARD_DEVICE}1 $MOUNT_DIR
    
    # Verify mount was successful
    if ! mountpoint -q $MOUNT_DIR; then
        echo "Error: Failed to mount ${SD_CARD_DEVICE}1 to $MOUNT_DIR"
        exit 1
    fi
    echo "Successfully mounted ${SD_CARD_DEVICE}1 to $MOUNT_DIR"
}

# Function to label the partition as "boot"
label_partition() {
    echo ">> Labeling the partition as 'boot'..."
    sudo fatlabel ${SD_CARD_DEVICE}1 boot || {
        echo "Error: Failed to label partition as 'boot'."
        exit 1
    }
}

# Copy Barebox and environment files to the SD card
copy_barebox_to_fat() {
    echo "Copying Barebox components to FAT partition..."
    
    # Check if build directory exists
    if [ ! -d "build" ]; then
        echo "Error: build directory not found"
        exit 1
    fi
    
    # Check if barebox image exists
    if [ ! -f "build/images/barebox-am33xx-beaglebone.img" ]; then
        echo "Error: barebox-am33xx-beaglebone.img not found in build/images/"
        exit 1
    fi
    
    pushd build > /dev/null
        pushd images > /dev/null
            # Copy barebox.bin to the SD card partition
            sudo cp ./barebox-am33xx-beaglebone.img $MOUNT_DIR/barebox.bin
            echo "Copied barebox-am33xx-beaglebone.img as barebox.bin"
            
            # Copy MLO if it exists
            # barebox-am33xx-beaglebone-mlo.mmc.img

            if [ -f "./barebox-am33xx-beaglebone-mlo.mmc.img" ]; then
                sudo cp ./barebox-am33xx-beaglebone-mlo.mmc.img $MOUNT_DIR/MLO
                echo "Copied barebox-am33xx-beaglebone-mlo.img as MLO"
            fi
        popd > /dev/null
    popd > /dev/null
    
    # Sync to ensure all data is written
    sudo sync
}

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    if mountpoint -q $MOUNT_DIR 2>/dev/null; then
        sudo umount $MOUNT_DIR
        echo "Unmounted $MOUNT_DIR"
    fi
    sudo rmdir $MOUNT_DIR 2>/dev/null || true
    echo "Setup complete!"
}

# Trap to ensure cleanup on exit
trap cleanup EXIT

#==============================================================================
# Main execution
check_device
prepare_sdcard
label_partition
mount_sdcard
copy_barebox_to_fat

echo "SD card setup completed successfully!"
echo "Boot partition created on ${SD_CARD_DEVICE}1"
echo "Barebox components copied to SD card"
