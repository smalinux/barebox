// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Amlogic Meson Reset Auxiliary driver
 *
 * Copyright (c) 2024 BayLibre, SAS.
 * Author: Jerome Brunet <jbrunet@baylibre.com>
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/device.h>
#include <of.h>
// SMA:
//#include <linux/auxiliary_bus.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

#include "reset-meson.h"

static const struct meson_reset_param meson_a1_audio_param = {
	.reset_ops	= &meson_reset_toggle_ops,
	.reset_num	= 32,
	.level_offset	= 0x28,
};

static const struct meson_reset_param meson_a1_audio_vad_param = {
	.reset_ops	= &meson_reset_toggle_ops,
	.reset_num	= 6,
	.level_offset	= 0x8,
};

static const struct meson_reset_param meson_g12a_audio_param = {
	.reset_ops	= &meson_reset_toggle_ops,
	.reset_num	= 26,
	.level_offset	= 0x24,
};

static const struct meson_reset_param meson_sm1_audio_param = {
	.reset_ops	= &meson_reset_toggle_ops,
	.reset_num	= 39,
	.level_offset	= 0x28,
};

static const struct of_device_id meson_reset_aux_ids[] = {
	{
		.compatible = "a1-audio-clkc.rst-a1",
		.data = &meson_a1_audio_param,
	}, {
		.compatible = "a1-audio-clkc.rst-a1-vad",
		.data = &meson_a1_audio_vad_param,
	}, {
		.compatible = "axg-audio-clkc.rst-g12a",
		.data = &meson_g12a_audio_param,
	}, {
		.compatible = "axg-audio-clkc.rst-sm1",
		.data = &meson_sm1_audio_param,
	}, {}
};
MODULE_DEVICE_TABLE(of, meson_reset_aux_ids);

static int meson_reset_aux_probe(struct device *dev)
{
	// SMA:
	//const struct meson_reset_param *param = (const struct meson_reset_param *)(id->data);
	struct regmap *map;

	map = dev_get_regmap(dev->parent, NULL);
	if (!map)
		return -EINVAL;

	// SMA:
	//return meson_reset_controller_register(dev, map, param);
	return 0;
}

static struct driver meson_reset_aux_driver = {
	.probe		= meson_reset_aux_probe,
	.of_match_table	= meson_reset_aux_ids,
};
core_platform_driver(meson_reset_aux_driver);

MODULE_DESCRIPTION("Amlogic Meson Reset Auxiliary driver");
MODULE_AUTHOR("Jerome Brunet <jbrunet@baylibre.com>");
MODULE_LICENSE("Dual BSD/GPL");
