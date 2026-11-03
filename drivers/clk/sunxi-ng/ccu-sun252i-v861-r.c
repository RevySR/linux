// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn>
 *
 * Based on the A523 PRCM CCU driver:
 *   Copyright (C) 2024 Arm Ltd.
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <dt-bindings/clock/sun252i-v861-r-ccu.h>
#include <dt-bindings/reset/sun252i-v861-r-ccu.h>

#include "ccu_common.h"
#include "ccu_reset.h"

#include "ccu_gate.h"
#include "ccu_mp.h"

static const struct clk_parent_data r_ahb_apb_parents[] = {
	{ .fw_name = "hosc" },
	{ .fw_name = "losc" },
	{ .fw_name = "iosc" },
	{ .fw_name = "pll-periph" },
};

static SUNXI_CCU_MP_DATA_WITH_MUX(r_ahb_clk, "r-ahb", r_ahb_apb_parents, 0x000,
				  0, 5,		/* M */
				  8, 2,		/* P */
				  24, 3,	/* mux */
				  0);
static const struct clk_hw *r_ahb_hws[] = { &r_ahb_clk.common.hw };

static SUNXI_CCU_MP_DATA_WITH_MUX(r_apb0_clk, "r-apb0", r_ahb_apb_parents,
				  0x00c,
				  0, 5,		/* M */
				  8, 2,		/* P */
				  24, 3,	/* mux */
				  0);
static const struct clk_hw *r_apb0_hws[] = { &r_apb0_clk.common.hw };

static SUNXI_CCU_GATE_HWS(bus_r_ppu_clk, "bus-r-ppu", r_apb0_hws, 0x1ac,
			  BIT(0), 0);
static SUNXI_CCU_GATE_HWS(bus_r_rtc_clk, "bus-r-rtc", r_ahb_hws, 0x20c,
			  BIT(0), 0);

static struct ccu_common *sun252i_v861_r_ccu_clks[] = {
	&r_ahb_clk.common,
	&r_apb0_clk.common,
	&bus_r_ppu_clk.common,
	&bus_r_rtc_clk.common,
};

static struct clk_hw_onecell_data sun252i_v861_r_hw_clks = {
	.num	= CLK_BUS_R_RTC + 1,
	.hws	= {
		[CLK_R_AHB]		= &r_ahb_clk.common.hw,
		[CLK_R_APB0]		= &r_apb0_clk.common.hw,
		[CLK_BUS_R_PPU]		= &bus_r_ppu_clk.common.hw,
		[CLK_BUS_R_RTC]		= &bus_r_rtc_clk.common.hw,
	},
};

static const struct ccu_reset_map sun252i_v861_r_ccu_resets[] = {
	[RST_BUS_R_PPU]		= { 0x1ac, BIT(16) },
	[RST_BUS_R_RTC]		= { 0x20c, BIT(16) },
};

static const struct sunxi_ccu_desc sun252i_v861_r_ccu_desc = {
	.ccu_clks	= sun252i_v861_r_ccu_clks,
	.num_ccu_clks	= ARRAY_SIZE(sun252i_v861_r_ccu_clks),

	.hw_clks	= &sun252i_v861_r_hw_clks,

	.resets		= sun252i_v861_r_ccu_resets,
	.num_resets	= ARRAY_SIZE(sun252i_v861_r_ccu_resets),
};

static int sun252i_v861_r_ccu_probe(struct platform_device *pdev)
{
	void __iomem *reg;

	reg = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	return devm_sunxi_ccu_probe(&pdev->dev, reg, &sun252i_v861_r_ccu_desc);
}

static const struct of_device_id sun252i_v861_r_ccu_ids[] = {
	{ .compatible = "allwinner,sun252i-v861-r-ccu" },
	{ }
};
MODULE_DEVICE_TABLE(of, sun252i_v861_r_ccu_ids);

static struct platform_driver sun252i_v861_r_ccu_driver = {
	.probe	= sun252i_v861_r_ccu_probe,
	.driver	= {
		.name			= "sun252i-v861-r-ccu",
		.suppress_bind_attrs	= true,
		.of_match_table		= sun252i_v861_r_ccu_ids,
	},
};
module_platform_driver(sun252i_v861_r_ccu_driver);

MODULE_IMPORT_NS("SUNXI_CCU");
MODULE_DESCRIPTION("Support for the Allwinner V861 PRCM CCU");
MODULE_LICENSE("GPL");
