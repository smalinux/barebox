// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) "rk3562-evb: " fmt

#include <common.h>
#include <init.h>
#include <mach/rockchip/bbu.h>
#include <globalvar.h>
#include <deep-probe.h>

static int rk3562_evb2_probe(struct device *dev)
{
	rockchip_bbu_mmc_register("sd", 0, "/dev/mmc0");
	rockchip_bbu_mmc_register("emmc", BBU_HANDLER_FLAG_DEFAULT, "/dev/mmc1");

	return 0;
}

static const struct of_device_id rk3562_evb2_of_match[] = {
	{ .compatible = "rockchip,rk3562-evb2-v10" },
	{ /* Sentinel */},
};

static struct driver rk3562_evb2_board_driver = {
	.name = "board-rk3562-evb",
	.probe = rk3562_evb2_probe,
	.of_compatible = rk3562_evb2_of_match,
};
coredevice_platform_driver(rk3562_evb2_board_driver);

BAREBOX_DEEP_PROBE_ENABLE(rk3562_evb2_of_match);
