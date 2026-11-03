// SPDX-License-Identifier: GPL-2.0-only
/*
 * Allwinner V861 SoC r-pinctrl driver.
 *
 * Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn>
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-sunxi.h"

static const u8 v861_r_nr_bank_pins[SUNXI_PINCTRL_MAX_BANKS] =
/*	  PL */
	{  7 };

static const u8 v861_r_irq_bank_muxes[SUNXI_PINCTRL_MAX_BANKS] =
/*	  PL */
	{ 14 };

static struct sunxi_pinctrl_desc v861_r_pinctrl_data = {
	.irq_banks = 1,
	.irq_reg_base = 0x40,
	.pin_base = PL_BASE,
};

static int v861_r_pinctrl_probe(struct platform_device *pdev)
{
	return sunxi_pinctrl_dt_table_init(pdev, v861_r_nr_bank_pins,
					   v861_r_irq_bank_muxes,
					   &v861_r_pinctrl_data,
					   SUNXI_PINCTRL_NCAT2_REG_LAYOUT);
}

static const struct of_device_id v861_r_pinctrl_match[] = {
	{ .compatible = "allwinner,sun252i-v861-r-pinctrl", },
	{}
};

static struct platform_driver v861_r_pinctrl_driver = {
	.probe	= v861_r_pinctrl_probe,
	.driver	= {
		.name		= "sun252i-v861-r-pinctrl",
		.of_match_table	= v861_r_pinctrl_match,
	},
};
builtin_platform_driver(v861_r_pinctrl_driver);
