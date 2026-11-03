// SPDX-License-Identifier: GPL-2.0-only
/*
 * Allwinner V861 SoC pinctrl driver.
 *
 * Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn>
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-sunxi.h"

static const u8 v861_nr_bank_pins[SUNXI_PINCTRL_MAX_BANKS] =
/*	  PA  PB  PC  PD  PE  PF  PG  PH */
	{ 22,  0, 12, 23, 18,  7,  8, 16 };

static const unsigned int v861_irq_bank_map[] = { 0, 2, 3, 4, 5, 6, 7 };

static const u8 v861_irq_bank_muxes[SUNXI_PINCTRL_MAX_BANKS] =
/*	  PA  PB  PC  PD  PE  PF  PG  PH */
	{ 14,  0, 14, 14, 14, 14, 14, 14 };

static struct sunxi_pinctrl_desc v861_pinctrl_data = {
	.irq_banks = ARRAY_SIZE(v861_irq_bank_map),
	.irq_bank_map = v861_irq_bank_map,
};

static int v861_pinctrl_probe(struct platform_device *pdev)
{
	return sunxi_pinctrl_dt_table_init(pdev, v861_nr_bank_pins,
					   v861_irq_bank_muxes,
					   &v861_pinctrl_data,
					   SUNXI_PINCTRL_NCAT2_REG_LAYOUT);
}

static const struct of_device_id v861_pinctrl_match[] = {
	{ .compatible = "allwinner,sun252i-v861-pinctrl", },
	{}
};

static struct platform_driver v861_pinctrl_driver = {
	.probe	= v861_pinctrl_probe,
	.driver	= {
		.name		= "sun252i-v861-pinctrl",
		.of_match_table	= v861_pinctrl_match,
	},
};
builtin_platform_driver(v861_pinctrl_driver);
