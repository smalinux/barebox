// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) "rk3562-kickpi-k3: " fmt

#include <bootsource.h>
#include <common.h>
#include <init.h>
#include <mach/rockchip/bbu.h>
#include <globalvar.h>
#include <deep-probe.h>

struct kickpi_k3_model {
	const char *name;
	const char *shortname;
};

static int rk3562_kickpi_k3_probe(struct device *dev)
{
	int ret = 0;
	enum bootsource bootsource = bootsource_get();
	int instance = bootsource_get_instance();
	const struct kickpi_k3_model *model;

	model = device_get_match_data(dev);

	barebox_set_model(model->name);
	barebox_set_hostname(model->shortname);

	if (bootsource == BOOTSOURCE_MMC && instance == 1)
		of_device_enable_path("/chosen/environment-sd");
	else
		of_device_enable_path("/chosen/environment-emmc");

	rockchip_bbu_mmc_register("emmc", BBU_HANDLER_FLAG_DEFAULT, "/dev/disk0");
	rockchip_bbu_mmc_register("sd", 0, "/dev/disk1");

	return ret;
}

static const struct kickpi_k3_model kickpi_k3 = {
	.name = "Kickpi K3 Board",
	.shortname = "kickpi_k3",
};

static const struct of_device_id rk3562_kickpi_k3_of_match[] = {
	// TODO: change this compatible to: "rockchip,rk3562-kickpi-k3"
	{
		.compatible = "rockchip,rk3562-kickpi-k3",
		.data = &kickpi_k3,
	},
	{ /* Sentinel */},
};

static struct driver rk3562_kickpi_k3_board_driver = {
	.name = "board-rk3562-kickpi-k3",
	.probe = rk3562_kickpi_k3_probe,
	.of_compatible = rk3562_kickpi_k3_of_match,
};
coredevice_platform_driver(rk3562_kickpi_k3_board_driver);

BAREBOX_DEEP_PROBE_ENABLE(rk3562_kickpi_k3_of_match);
