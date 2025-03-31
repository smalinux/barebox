// SPDX-License-Identifier: GPL-2.0-only
#include <common.h>
#include <driver.h>
#include <init.h>
#include <malloc.h>
#include <fs.h>
#include <string.h>
#include <command.h>
#include <errno.h>
#include <linux/stat.h>
#include <xfuncs.h>
#include <fcntl.h>
#include <efi.h>
#include <block.h>
#include <efi/efi-payload.h>
#include <efi/efi-device.h>
#include <bootsource.h>

#define EFI_BLOCK_IO_PROTOCOL_REVISION2 0x00020001
#define EFI_BLOCK_IO_PROTOCOL_REVISION3 ((2<<16) | (31))

struct efi_bio_priv {
	struct efi_block_io_protocol *protocol;
	struct device *dev;
	struct block_device blk;
	u32 media_id;
};

static int efi_bio_read(struct block_device *blk, void *buffer, sector_t block,
		blkcnt_t num_blocks)
{
	struct efi_bio_priv *priv = container_of(blk, struct efi_bio_priv, blk);
	efi_status_t efiret;

	efiret = priv->protocol->read(priv->protocol, priv->media_id,
			block, num_blocks * 512, buffer);

	if (EFI_ERROR(efiret))
		return -efi_errno(efiret);

	return 0;
}

static int efi_bio_write(struct block_device *blk,
		const void *buffer, sector_t block, blkcnt_t num_blocks)
{
	struct efi_bio_priv *priv = container_of(blk, struct efi_bio_priv, blk);
	efi_status_t efiret;

	efiret = priv->protocol->write(priv->protocol, priv->media_id,
			block, num_blocks * 512, (void *)buffer);
	if (EFI_ERROR(efiret))
		return -efi_errno(efiret);

	return 0;
}

static int efi_bio_flush(struct block_device *blk)
{
	struct efi_bio_priv *priv = container_of(blk, struct efi_bio_priv, blk);
	efi_status_t efiret;

	efiret = priv->protocol->flush(priv->protocol);
	if (EFI_ERROR(efiret))
		return -efi_errno(efiret);

	return 0;
}

static struct block_device_ops efi_bio_ops = {
	.read = efi_bio_read,
	.write = efi_bio_write,
	.flush = efi_bio_flush,
};

static void efi_bio_print_info(struct device *dev)
{
	struct efi_bio_priv *priv = dev->priv;
	struct efi_block_io_media *media = priv->protocol->media;
	u64 revision = priv->protocol->revision;

	printf("Block I/O Media:\n");
	printf("  revision: 0x%016llx\n", revision);
	printf("  media_id: 0x%08x\n", media->media_id);
	printf("  removable_media: %d\n", media->removable_media);
	printf("  media_present: %d\n", media->media_present);
	printf("  logical_partition: %d\n", media->logical_partition);
	printf("  read_only: %d\n", media->read_only);
	printf("  write_caching: %d\n", media->write_caching);
	printf("  block_size: 0x%08x\n", media->block_size);
	printf("  io_align: 0x%08x\n", media->io_align);
	printf("  last_block: 0x%016llx\n", media->last_block);

	if (revision < EFI_BLOCK_IO_PROTOCOL_REVISION2)
		return;

	printf("  lowest_aligned_lba: 0x%08llx\n",
			media->lowest_aligned_lba);
	printf("  logical_blocks_per_physical_block: 0x%08x\n",
			media->logical_blocks_per_physical_block);

	if (revision < EFI_BLOCK_IO_PROTOCOL_REVISION3)
		return;

	printf("  optimal_transfer_length_granularity: 0x%08x\n",
			media->optimal_transfer_length_granularity);
}

static bool is_bio_usbdev(struct efi_device *efidev)
{
	return efi_device_has_guid(efidev, EFI_USB_IO_PROTOCOL_GUID);
}

static int efi_bio_probe(struct efi_device *efidev)
{
	bool is_usbdev;
	int instance;
	struct efi_bio_priv *priv;
	struct efi_block_io_media *media;
	struct device *dev = &efidev->dev;

	priv = xzalloc(sizeof(*priv));

	BS->handle_protocol(efidev->handle, &efi_block_io_protocol_guid,
			(void **)&priv->protocol);
	if (!priv->protocol)
		return -ENODEV;

	dev->priv = priv;
	devinfo_add(dev, efi_bio_print_info);

	media = priv->protocol->media;
	if (__is_defined(DEBUG))
		efi_bio_print_info(dev);
	priv->dev = &efidev->dev;

	is_usbdev = is_bio_usbdev(efidev);
	if (is_usbdev)
		priv->blk.rootwait = true;

	if (IS_ENABLED(CONFIG_EFI_BLK_SEPARATE_USBDISK) && is_usbdev) {
		instance = cdev_find_free_index("usbdisk");
		priv->blk.cdev.name = xasprintf("usbdisk%d", instance);
	} else {
		instance = cdev_find_free_index("disk");
		priv->blk.cdev.name = xasprintf("disk%d", instance);
	}

	priv->blk.blockbits = ffs(media->block_size) - 1;
	priv->blk.num_blocks = media->last_block + 1;
	priv->blk.ops = &efi_bio_ops;
	priv->blk.dev = &efidev->dev;
	priv->blk.type = BLK_TYPE_VIRTUAL;

	priv->media_id = media->media_id;

	if (efi_get_bootsource() == efidev)
		bootsource_set_raw_instance(instance);

	return blockdevice_register(&priv->blk);
}

static struct efi_driver efi_bio_driver = {
        .driver = {
		.name  = "efi-block-io",
	},
        .probe = efi_bio_probe,
	.guid = EFI_BLOCK_IO_PROTOCOL_GUID,
};
device_efi_driver(efi_bio_driver);
