#!/bin/bash
#
#

SD_CARD_DEVICE="${1}"                       # SD card device (e.g., /dev/sdb)
CROSS_COMPILE="arm-linux-gnueabihf-"              # Cross-compiler prefix
ARCH=arm

export CROSS_COMPILE=$CROSS_COMPILE
export ARCH=$ARCH


make am335x_mlo_defconfig
make -j12

make  omap_defconfig
make -j12


# TFTP
cp ./images/barebox-am33xx-beaglebone.img /tftpboot/barebox.bin
cp ./images/barebox-am33xx-beaglebone-mlo.img /tftpboot/MLO

#==============================================================================





# Setup Barebox defconfig based on the base configuration
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

# Copy Barebox and environment files to the SD card
copy_barebox_to_fat() {
   echo "Copying Barebox components to FAT partition..."

   pushd uboot_standalone
      pushd barebox
         # Copy MLO (SPL) to the SD card partition
         sudo cp images/barebox-am33xx-beaglebone-mlo.img $MOUNT_DIR/MLO
         # Copy barebox.bin to the SD card partition
         sudo cp images/barebox-am33xx-beaglebone.img $MOUNT_DIR/barebox.bin
      popd
   popd
}





#==============================================================================
#prepare_sdcard
#label_partition
#mount_sdcard
#copy_barebox_to_fat
#cleanup
