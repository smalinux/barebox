// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2008 Texas Instruments (http://www.ti.com/, Sukumar Ghorai <s-ghorai@ti.com>)

#include <common.h>
#include <pbl/bio.h>
#include <mci.h>
#include <debug_ll.h>
#include <mach/omap/xload.h>

#include "omap_hsmmc.h"

#define SECTOR_SIZE			512
#define SUPPORT_MAX_BLOCKS		16U

static bool highcapacity_card = 1;

static int sd_cmd_stop_transmission(struct omap_hsmmc *hsmmc)
{
	struct mci_cmd cmd = {
		.cmdidx = MMC_CMD_STOP_TRANSMISSION,
		.resp_type = MMC_RSP_R1b,
	};

	return omap_hsmmc_send_cmd(hsmmc, &cmd, NULL);
}

static int sd_cmd_read_multiple_block(struct omap_hsmmc *hsmmc,
				      void *buf,
				      unsigned int start,
				      unsigned int block_count)
{
	u16 block_len = SECTOR_SIZE;
	struct mci_data data;
	struct mci_cmd cmd = {
		.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK,
		.resp_type = MMC_RSP_R1,
		.cmdarg = start,
	};

	if (!highcapacity_card)
		cmd.cmdarg *= block_len;

	data.dest = buf;
	data.flags = MMC_DATA_READ;
	data.blocksize = block_len;
	data.blocks = block_count;

	return omap_hsmmc_send_cmd(hsmmc, &cmd, &data);
}

static int omap_hsmmc_bio_read(struct pbl_bio *bio, off_t start,
			       void *buf, unsigned int nblocks)
{
	struct omap_hsmmc *hsmmc = bio->priv;
	unsigned int blocks_done = 0;
	unsigned int blocks;
	unsigned int block_len = SECTOR_SIZE;
	unsigned int blocks_read;
	int ret;

	while (blocks_done < nblocks) {
		blocks = min(nblocks - blocks_done, SUPPORT_MAX_BLOCKS);

		blocks_read = sd_cmd_read_multiple_block(hsmmc, buf,
							 start + blocks_done,
							 blocks);

		ret = sd_cmd_stop_transmission(hsmmc);
		if (ret)
			return ret;

		blocks_done += blocks_read;

		if (blocks_read != blocks)
			break;

		buf += blocks * block_len;
	}

	return blocks_done;
}

static struct omap_hsmmc omap_sdcard;

int omap_hsmmc_bio_init(struct pbl_bio *bio, void __iomem *iobase,
			unsigned reg_ofs)
{
	struct omap_hsmmc *hsmmc = &omap_sdcard;

	hsmmc->iobase = iobase;
	hsmmc->base = iobase + reg_ofs;

	bio->priv = hsmmc;
	bio->read = omap_hsmmc_bio_read;

	 // FIXME can we determine this without leaving SD transfer mode?
	highcapacity_card = 1;

	return 0;
}
