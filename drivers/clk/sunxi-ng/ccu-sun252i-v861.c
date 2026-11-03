// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn>
 *
 * Based on the A523 CCU driver:
 *   Copyright (C) 2023-2024 Arm Ltd.
 */

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <dt-bindings/clock/sun252i-v861-ccu.h>
#include <dt-bindings/reset/sun252i-v861-ccu.h>

#include "ccu_common.h"
#include "ccu_reset.h"

#include "ccu_div.h"
#include "ccu_gate.h"
#include "ccu_mp.h"
#include "ccu_mux.h"
#include "ccu_nkmp.h"
#include "ccu_nm.h"

static const struct clk_parent_data osc24M[] = {
	{ .fw_name = "hosc" }
};

/**************************************************************************
 *                              PLLs                                      *
 **************************************************************************/

/* The CPU PLL at 0x000 is left to the firmware. */

#define SUN252I_V861_PLL_DDR_REG	0x010
static struct ccu_nkmp pll_ddr_clk = {
	.enable		= BIT(27),
	.lock		= BIT(28),
	.n		= _SUNXI_CCU_MULT_MIN(8, 8, 11),
	.m		= _SUNXI_CCU_DIV(1, 1), /* input divider */
	.p		= _SUNXI_CCU_DIV(0, 1), /* output divider */
	.common		= {
		.reg		= 0x010,
		.hw.init	= CLK_HW_INIT_PARENTS_DATA("pll-ddr", osc24M,
							   &ccu_nkmp_ops,
							   CLK_SET_RATE_GATE |
							   CLK_IS_CRITICAL),
	},
};

/*
 * There is no clock output at the VCO rate (2.4 GHz). The outputs have their
 * own dividers, so model them as divider clocks of the VCO.
 */
#define SUN252I_V861_PLL_PERIPH0_REG	0x020
static struct ccu_nm pll_periph0_4x_clk = {
	.enable		= BIT(27),
	.lock		= BIT(28),
	.n		= _SUNXI_CCU_MULT_MIN(8, 8, 11),
	.m		= _SUNXI_CCU_DIV(1, 1), /* input divider */
	.common		= {
		.reg		= 0x020,
		.hw.init	= CLK_HW_INIT_PARENTS_DATA("pll-periph0-4x",
							   osc24M, &ccu_nm_ops,
							   CLK_SET_RATE_GATE),
	},
};

static const struct clk_hw *pll_periph0_4x_hws[] = {
	&pll_periph0_4x_clk.common.hw
};

static SUNXI_CCU_M_HWS(pll_periph0_2x_clk, "pll-periph0-2x",
		       pll_periph0_4x_hws, 0x020, 16, 3, 0);
static const struct clk_hw *pll_periph0_2x_hws[] = {
	&pll_periph0_2x_clk.common.hw
};

static SUNXI_CCU_M_HWS(pll_periph0_800M_clk, "pll-periph0-800M",
		       pll_periph0_4x_hws, 0x020, 20, 3, 0);
static SUNXI_CCU_M_HWS(pll_periph0_480M_clk, "pll-periph0-480M",
		       pll_periph0_4x_hws, 0x020, 2, 3, 0);
static const struct clk_hw *pll_periph0_480M_hws[] = {
	&pll_periph0_480M_clk.common.hw
};

static CLK_FIXED_FACTOR_HWS(pll_periph0_600M_clk, "pll-periph0-600M",
			    pll_periph0_2x_hws, 2, 1, 0);
static CLK_FIXED_FACTOR_HWS(pll_periph0_400M_clk, "pll-periph0-400M",
			    pll_periph0_2x_hws, 3, 1, 0);
static CLK_FIXED_FACTOR_HWS(pll_periph0_300M_clk, "pll-periph0-300M",
			    pll_periph0_2x_hws, 4, 1, 0);
static CLK_FIXED_FACTOR_HWS(pll_periph0_200M_clk, "pll-periph0-200M",
			    pll_periph0_2x_hws, 6, 1, 0);
static CLK_FIXED_FACTOR_HWS(pll_periph0_150M_clk, "pll-periph0-150M",
			    pll_periph0_2x_hws, 8, 1, 0);
static CLK_FIXED_FACTOR_HWS(pll_periph0_160M_clk, "pll-periph0-160M",
			    pll_periph0_480M_hws, 3, 1, 0);
static const struct clk_hw *pll_periph0_150M_hws[] = {
	&pll_periph0_150M_clk.hw
};

/*
 * The video, CSI and NPU PLLs share one layout: the 4x output divides the
 * VCO by one or two, while the 2x and 1x outputs divide it by two and four.
 */
#define SUN252I_V861_PLL_VIDEO_REG	0x040
static struct ccu_nm pll_video_8x_clk = {
	.enable		= BIT(27),
	.lock		= BIT(28),
	.n		= _SUNXI_CCU_MULT_MIN(8, 8, 11),
	.m		= _SUNXI_CCU_DIV(1, 1), /* input divider */
	.common		= {
		.reg		= 0x040,
		.hw.init	= CLK_HW_INIT_PARENTS_DATA("pll-video-8x",
							   osc24M, &ccu_nm_ops,
							   CLK_SET_RATE_GATE),
	},
};

static const struct clk_hw *pll_video_8x_hws[] = {
	&pll_video_8x_clk.common.hw
};

static SUNXI_CCU_M_HWS(pll_video_4x_clk, "pll-video-4x",
		       pll_video_8x_hws, 0x040, 0, 1, 0);
static CLK_FIXED_FACTOR_HWS(pll_video_2x_clk, "pll-video-2x",
			    pll_video_8x_hws, 2, 1, 0);
static CLK_FIXED_FACTOR_HWS(pll_video_1x_clk, "pll-video-1x",
			    pll_video_8x_hws, 4, 1, 0);

#define SUN252I_V861_PLL_CSI_REG	0x048
static struct ccu_nm pll_csi_8x_clk = {
	.enable		= BIT(27),
	.lock		= BIT(28),
	.n		= _SUNXI_CCU_MULT_MIN(8, 8, 11),
	.m		= _SUNXI_CCU_DIV(1, 1), /* input divider */
	.common		= {
		.reg		= 0x048,
		.hw.init	= CLK_HW_INIT_PARENTS_DATA("pll-csi-8x",
							   osc24M, &ccu_nm_ops,
							   CLK_SET_RATE_GATE),
	},
};

static const struct clk_hw *pll_csi_8x_hws[] = {
	&pll_csi_8x_clk.common.hw
};

static SUNXI_CCU_M_HWS(pll_csi_4x_clk, "pll-csi-4x",
		       pll_csi_8x_hws, 0x048, 0, 1, 0);
static CLK_FIXED_FACTOR_HWS(pll_csi_2x_clk, "pll-csi-2x",
			    pll_csi_8x_hws, 2, 1, 0);
static CLK_FIXED_FACTOR_HWS(pll_csi_1x_clk, "pll-csi-1x",
			    pll_csi_8x_hws, 4, 1, 0);

/*
 * The audio PLL VCO feeds three output dividers. Two further dividers, in
 * the pre-divider register at 0xe00, derive the 4x and 1x audio clocks.
 * Their alternative fractional dividers are not used, see the probe function.
 */
#define SUN252I_V861_PLL_AUDIO_REG	0x078
static struct ccu_nm pll_audio_clk = {
	.enable		= BIT(27),
	.lock		= BIT(28),
	.n		= _SUNXI_CCU_MULT_MIN(8, 8, 11),
	.m		= _SUNXI_CCU_DIV(1, 1), /* input divider */
	.common		= {
		.reg		= 0x078,
		.hw.init	= CLK_HW_INIT_PARENTS_DATA("pll-audio",
							   osc24M, &ccu_nm_ops,
							   CLK_SET_RATE_GATE),
	},
};

static const struct clk_hw *pll_audio_hws[] = {
	&pll_audio_clk.common.hw
};

static SUNXI_CCU_M_HWS(pll_audio_div2_clk, "pll-audio-div2",
		       pll_audio_hws, 0x078, 16, 3, 0);
static SUNXI_CCU_M_HWS(pll_audio_div3_clk, "pll-audio-div3",
		       pll_audio_hws, 0x078, 20, 3, 0);
static SUNXI_CCU_M_HWS(pll_audio_div5_clk, "pll-audio-div5",
		       pll_audio_hws, 0x078, 2, 3, 0);
static const struct clk_hw *pll_audio_div2_hws[] = {
	&pll_audio_div2_clk.common.hw
};

static const struct clk_hw *pll_audio_div5_hws[] = {
	&pll_audio_div5_clk.common.hw
};

#define SUN252I_V861_PLL_AUDIO_PRE_DIV_REG	0xe00
static SUNXI_CCU_M_HWS(pll_audio_4x_clk, "pll-audio-4x",
		       pll_audio_div2_hws, 0xe00, 5, 5, CLK_SET_RATE_PARENT);
static SUNXI_CCU_M_HWS(pll_audio_1x_clk, "pll-audio-1x",
		       pll_audio_div5_hws, 0xe00, 0, 5, CLK_SET_RATE_PARENT);

#define SUN252I_V861_PLL_NPU_REG	0x080
static struct ccu_nm pll_npu_8x_clk = {
	.enable		= BIT(27),
	.lock		= BIT(28),
	.n		= _SUNXI_CCU_MULT_MIN(8, 8, 11),
	.m		= _SUNXI_CCU_DIV(1, 1), /* input divider */
	.common		= {
		.reg		= 0x080,
		.hw.init	= CLK_HW_INIT_PARENTS_DATA("pll-npu-8x",
							   osc24M, &ccu_nm_ops,
							   CLK_SET_RATE_GATE),
	},
};

static const struct clk_hw *pll_npu_8x_hws[] = {
	&pll_npu_8x_clk.common.hw
};

static SUNXI_CCU_M_HWS(pll_npu_4x_clk, "pll-npu-4x",
		       pll_npu_8x_hws, 0x080, 0, 1, 0);
static CLK_FIXED_FACTOR_HWS(pll_npu_2x_clk, "pll-npu-2x",
			    pll_npu_8x_hws, 2, 1, 0);
static CLK_FIXED_FACTOR_HWS(pll_npu_1x_clk, "pll-npu-1x",
			    pll_npu_8x_hws, 4, 1, 0);

/**************************************************************************
 *                           bus clocks                                   *
 **************************************************************************/

/*
 * The CPU subsystem clocks between 0x4fc and 0x50c, including the IPMC used
 * for CPU power management, are left to the firmware.
 */

static const struct clk_parent_data ahb_apb_parents[] = {
	{ .fw_name = "hosc" },
	{ .fw_name = "losc" },
	{ .fw_name = "iosc" },
	{ .hw = &pll_periph0_600M_clk.hw },
};

static SUNXI_CCU_MP_DATA_WITH_MUX(ahb_clk, "ahb", ahb_apb_parents, 0x510,
				  0, 5,		/* M */
				  8, 2,		/* P */
				  24, 2,	/* mux */
				  CLK_IS_CRITICAL);
static const struct clk_hw *ahb_hws[] = { &ahb_clk.common.hw };

static SUNXI_CCU_MP_DATA_WITH_MUX(apb0_clk, "apb0", ahb_apb_parents, 0x520,
				  0, 5,		/* M */
				  8, 2,		/* P */
				  24, 2,	/* mux */
				  0);
static const struct clk_hw *apb0_hws[] = { &apb0_clk.common.hw };

static SUNXI_CCU_MP_DATA_WITH_MUX(apb1_clk, "apb1", ahb_apb_parents, 0x524,
				  0, 5,		/* M */
				  8, 2,		/* P */
				  24, 2,	/* mux */
				  0);
static const struct clk_hw *apb1_hws[] = { &apb1_clk.common.hw };

static const struct clk_parent_data apb_uart_parents[] = {
	{ .fw_name = "hosc" },
	{ .fw_name = "losc" },
	{ .fw_name = "iosc" },
	{ .hw = &pll_periph0_600M_clk.hw },
	{ .hw = &pll_audio_div2_clk.common.hw },
};

static SUNXI_CCU_MP_DATA_WITH_MUX(apb_uart_clk, "apb-uart", apb_uart_parents,
				  0x528,
				  0, 5,		/* M */
				  8, 2,		/* P */
				  24, 3,	/* mux */
				  0);
static const struct clk_hw *apb_uart_hws[] = { &apb_uart_clk.common.hw };

/**************************************************************************
 *                          mod clocks with gates                         *
 **************************************************************************/

static const struct clk_hw *de_g2d_parents[] = {
	&pll_periph0_300M_clk.hw,
	&pll_video_1x_clk.hw,
};

static SUNXI_CCU_M_HW_WITH_MUX_GATE(de_clk, "de", de_g2d_parents, 0x600,
				    0, 5,	/* M */
				    24, 1,	/* mux */
				    BIT(31),	/* gate */
				    0);

static SUNXI_CCU_GATE_HWS(bus_de_clk, "bus-de", ahb_hws, 0x60c, BIT(0), 0);

static SUNXI_CCU_M_HW_WITH_MUX_GATE(g2d_clk, "g2d", de_g2d_parents, 0x630,
				    0, 5,	/* M */
				    24, 1,	/* mux */
				    BIT(31),	/* gate */
				    0);

static SUNXI_CCU_GATE_HWS(bus_g2d_clk, "bus-g2d", ahb_hws, 0x63c, BIT(0), 0);

static const struct clk_parent_data ce_parents[] = {
	{ .fw_name = "hosc" },
	{ .hw = &pll_periph0_400M_clk.hw },
	{ .hw = &pll_periph0_300M_clk.hw },
};

static SUNXI_CCU_M_DATA_WITH_MUX_GATE(ce_clk, "ce", ce_parents, 0x680,
				      0, 4,	/* M */
				      24, 3,	/* mux */
				      BIT(31),	/* gate */
				      0);

static SUNXI_CCU_GATE_HWS(bus_ce_clk, "bus-ce", ahb_hws, 0x68c, BIT(0), 0);
static SUNXI_CCU_GATE_HWS(bus_ce_sys_clk, "bus-ce-sys", ahb_hws, 0x68c,
			  BIT(1), 0);

static const struct clk_hw *ve_parents[] = {
	&pll_periph0_300M_clk.hw,
	&pll_periph0_400M_clk.hw,
	&pll_periph0_480M_clk.common.hw,
	&pll_periph0_600M_clk.hw,
	&pll_video_4x_clk.common.hw,
	&pll_csi_4x_clk.common.hw,
	&pll_audio_div3_clk.common.hw,
};

static SUNXI_CCU_M_HW_WITH_MUX_GATE(ve_clk, "ve", ve_parents, 0x690,
				    0, 5,	/* M */
				    24, 3,	/* mux */
				    BIT(31),	/* gate */
				    0);

static SUNXI_CCU_GATE_HWS(bus_ve_clk, "bus-ve", ahb_hws, 0x69c, BIT(0), 0);

static const struct clk_hw *npu_parents[] = {
	&pll_periph0_800M_clk.common.hw,
	&pll_periph0_600M_clk.hw,
	&pll_npu_4x_clk.common.hw,
};

static SUNXI_CCU_M_HW_WITH_MUX_GATE(npu_clk, "npu", npu_parents, 0x6e0,
				    0, 5,	/* M */
				    24, 2,	/* mux */
				    BIT(31),	/* gate */
				    0);

static SUNXI_CCU_GATE_HWS(bus_npu_clk, "bus-npu", ahb_hws, 0x6ec, BIT(0), 0);

static SUNXI_CCU_GATE_HWS(bus_sgdma_clk, "bus-sgdma", ahb_hws, 0x70c,
			  BIT(0), 0);
static SUNXI_CCU_GATE_HWS(bus_ndma_clk, "bus-ndma", ahb_hws, 0x70c,
			  BIT(1), 0);

static SUNXI_CCU_GATE_HWS(bus_msgbox0_clk, "bus-msgbox0", ahb_hws, 0x71c,
			  BIT(0), 0);
static SUNXI_CCU_GATE_HWS(bus_msgbox1_clk, "bus-msgbox1", ahb_hws, 0x71c,
			  BIT(1), 0);

static SUNXI_CCU_GATE_HWS(bus_spinlock_clk, "bus-spinlock", ahb_hws, 0x72c,
			  BIT(0), 0);

static SUNXI_CCU_GATE_HWS(bus_hstimer_clk, "bus-hstimer", ahb_hws, 0x73c,
			  BIT(0), 0);

static SUNXI_CCU_GATE_DATA(avs_clk, "avs", osc24M, 0x740, BIT(31), 0);

/* The timer bus clock gate has its own register, apart from the reset. */
static SUNXI_CCU_GATE_HWS(bus_timer_clk, "bus-timer", apb0_hws, 0x744,
			  BIT(31), 0);

static SUNXI_CCU_GATE_HWS(bus_dbg_clk, "bus-dbg", ahb_hws, 0x78c, BIT(0), 0);

static SUNXI_CCU_GATE_HWS(bus_pwm_clk, "bus-pwm", apb0_hws, 0x7ac, BIT(0), 0);

static const struct clk_parent_data dram_parents[] = {
	{ .fw_name = "hosc" },
	{ .hw = &pll_ddr_clk.common.hw },
	{ .hw = &pll_periph0_2x_clk.common.hw },
	{ .hw = &pll_periph0_800M_clk.common.hw },
};

static SUNXI_CCU_MP_DATA_WITH_MUX_GATE_FEAT(dram_clk, "dram", dram_parents,
					    0x800,
					    0, 5,	/* M */
					    8, 2,	/* P */
					    24, 3,	/* mux */
					    BIT(31),	/* gate */
					    CLK_IS_CRITICAL,
					    CCU_FEATURE_UPDATE_BIT);

/* MBUS runs at a quarter of the DRAM clock. */
static CLK_FIXED_FACTOR_HW(mbus_clk, "mbus", &dram_clk.common.hw, 4, 1, 0);
static const struct clk_hw *mbus_hws[] = { &mbus_clk.hw };

static SUNXI_CCU_GATE_HWS(mbus_dma_clk, "mbus-dma", mbus_hws,
			  0x804, BIT(0), 0);
static SUNXI_CCU_GATE_HWS(mbus_ve_clk, "mbus-ve", mbus_hws,
			  0x804, BIT(1), 0);
static SUNXI_CCU_GATE_HWS(mbus_ce_clk, "mbus-ce", mbus_hws,
			  0x804, BIT(2), 0);
static SUNXI_CCU_GATE_HWS(mbus_csi_clk, "mbus-csi", mbus_hws,
			  0x804, BIT(8), 0);
static SUNXI_CCU_GATE_HWS(mbus_isp_clk, "mbus-isp", mbus_hws,
			  0x804, BIT(9), 0);
static SUNXI_CCU_GATE_HWS(mbus_g2d_clk, "mbus-g2d", mbus_hws,
			  0x804, BIT(10), 0);

static SUNXI_CCU_GATE_HWS(bus_dram_clk, "bus-dram", ahb_hws, 0x80c,
			  BIT(0), CLK_IS_CRITICAL);

/*
 * The MMC controllers use timing mode 1, in which the card clock is half
 * of the module clock, so model that with a fixed post-divider.
 */
static const struct clk_parent_data mmc0_mmc1_parents[] = {
	{ .fw_name = "hosc" },
	{ .hw = &pll_periph0_400M_clk.hw },
	{ .hw = &pll_periph0_300M_clk.hw },
	{ .hw = &pll_video_4x_clk.common.hw },
	{ .hw = &pll_ddr_clk.common.hw },
};

static SUNXI_CCU_MP_DATA_WITH_MUX_GATE_POSTDIV(mmc0_clk, "mmc0",
					       mmc0_mmc1_parents, 0x830,
					       0, 4,	/* M */
					       8, 2,	/* P */
					       24, 3,	/* mux */
					       BIT(31),	/* gate */
					       2,	/* post-div */
					       0);

static SUNXI_CCU_MP_DATA_WITH_MUX_GATE_POSTDIV(mmc1_clk, "mmc1",
					       mmc0_mmc1_parents, 0x834,
					       0, 4,	/* M */
					       8, 2,	/* P */
					       24, 3,	/* mux */
					       BIT(31),	/* gate */
					       2,	/* post-div */
					       0);

static const struct clk_parent_data mmc2_parents[] = {
	{ .fw_name = "hosc" },
	{ .hw = &pll_periph0_600M_clk.hw },
	{ .hw = &pll_periph0_800M_clk.common.hw },
	{ .hw = &pll_video_4x_clk.common.hw },
	{ .hw = &pll_ddr_clk.common.hw },
};

static SUNXI_CCU_MP_DATA_WITH_MUX_GATE_POSTDIV(mmc2_clk, "mmc2",
					       mmc2_parents, 0x838,
					       0, 4,	/* M */
					       8, 2,	/* P */
					       24, 3,	/* mux */
					       BIT(31),	/* gate */
					       2,	/* post-div */
					       0);

static SUNXI_CCU_GATE_HWS(bus_mmc0_clk, "bus-mmc0", ahb_hws, 0x84c, BIT(0), 0);
static SUNXI_CCU_GATE_HWS(bus_mmc1_clk, "bus-mmc1", ahb_hws, 0x84c, BIT(1), 0);
static SUNXI_CCU_GATE_HWS(bus_mmc2_clk, "bus-mmc2", ahb_hws, 0x84c, BIT(2), 0);

static const struct clk_parent_data psram_parents[] = {
	{ .fw_name = "hosc" },
	{ .hw = &pll_periph0_480M_clk.common.hw },
	{ .hw = &pll_periph0_400M_clk.hw },
	{ .hw = &pll_periph0_300M_clk.hw },
};

static SUNXI_CCU_M_DATA_WITH_MUX_GATE(psram_clk, "psram", psram_parents, 0x850,
				      0, 5,	/* M */
				      24, 3,	/* mux */
				      BIT(31),	/* gate */
				      0);

static SUNXI_CCU_GATE_HWS(bus_psram_clk, "bus-psram", ahb_hws, 0x85c,
			  BIT(0), 0);

static SUNXI_CCU_GATE_HWS(bus_uart0_clk, "bus-uart0", apb_uart_hws, 0x90c,
			  BIT(0), 0);
static SUNXI_CCU_GATE_HWS(bus_uart1_clk, "bus-uart1", apb_uart_hws, 0x90c,
			  BIT(1), 0);
static SUNXI_CCU_GATE_HWS(bus_uart2_clk, "bus-uart2", apb_uart_hws, 0x90c,
			  BIT(2), 0);
static SUNXI_CCU_GATE_HWS(bus_uart3_clk, "bus-uart3", apb_uart_hws, 0x90c,
			  BIT(3), 0);

static SUNXI_CCU_GATE_HWS(bus_i2c0_clk, "bus-i2c0", apb1_hws, 0x91c,
			  BIT(0), 0);
static SUNXI_CCU_GATE_HWS(bus_i2c1_clk, "bus-i2c1", apb1_hws, 0x91c,
			  BIT(1), 0);
static SUNXI_CCU_GATE_HWS(bus_i2c2_clk, "bus-i2c2", apb1_hws, 0x91c,
			  BIT(2), 0);
static SUNXI_CCU_GATE_HWS(bus_i2c3_clk, "bus-i2c3", apb1_hws, 0x91c,
			  BIT(3), 0);

static const struct clk_parent_data spi_parents[] = {
	{ .fw_name = "hosc" },
	{ .hw = &pll_periph0_300M_clk.hw },
	{ .hw = &pll_periph0_200M_clk.hw },
};

static SUNXI_CCU_MP_DATA_WITH_MUX_GATE(spi0_clk, "spi0", spi_parents, 0x940,
				       0, 4,	/* M */
				       8, 2,	/* P */
				       24, 3,	/* mux */
				       BIT(31),	/* gate */
				       0);
static SUNXI_CCU_MP_DATA_WITH_MUX_GATE(spi1_clk, "spi1", spi_parents, 0x944,
				       0, 4,	/* M */
				       8, 2,	/* P */
				       24, 3,	/* mux */
				       BIT(31),	/* gate */
				       0);
static SUNXI_CCU_MP_DATA_WITH_MUX_GATE(spi2_clk, "spi2", spi_parents, 0x948,
				       0, 4,	/* M */
				       8, 2,	/* P */
				       24, 3,	/* mux */
				       BIT(31),	/* gate */
				       0);

static const struct clk_parent_data spif_parents[] = {
	{ .fw_name = "hosc" },
	{ .hw = &pll_periph0_400M_clk.hw },
	{ .hw = &pll_periph0_300M_clk.hw },
};

static SUNXI_CCU_MP_DATA_WITH_MUX_GATE(spif_clk, "spif", spif_parents, 0x950,
				       0, 4,	/* M */
				       8, 2,	/* P */
				       24, 3,	/* mux */
				       BIT(31),	/* gate */
				       0);

static SUNXI_CCU_GATE_HWS(bus_spi0_clk, "bus-spi0", ahb_hws, 0x96c, BIT(0), 0);
static SUNXI_CCU_GATE_HWS(bus_spi1_clk, "bus-spi1", ahb_hws, 0x96c, BIT(1), 0);
static SUNXI_CCU_GATE_HWS(bus_spi2_clk, "bus-spi2", ahb_hws, 0x96c, BIT(2), 0);
static SUNXI_CCU_GATE_HWS(bus_spif_clk, "bus-spif", ahb_hws, 0x96c, BIT(4), 0);

static SUNXI_CCU_GATE_HWS_WITH_PREDIV(gmac_25M_clk, "gmac-25M",
				      pll_periph0_150M_hws,
				      0x970, BIT(31) | BIT(30), 6, 0);
static SUNXI_CCU_GATE_HWS(bus_gmac_clk, "bus-gmac", ahb_hws, 0x97c,
			  BIT(0), 0);

static SUNXI_CCU_GATE_HWS(bus_gpadc_clk, "bus-gpadc", apb0_hws, 0x9ec,
			  BIT(0), 0);

static SUNXI_CCU_GATE_HWS(bus_ths_clk, "bus-ths", apb0_hws, 0x9fc, BIT(0), 0);

static const struct clk_hw *audio_parents[] = {
	&pll_audio_1x_clk.common.hw,
	&pll_audio_4x_clk.common.hw,
};

static SUNXI_CCU_M_HW_WITH_MUX_GATE(i2s0_clk, "i2s0", audio_parents, 0xa10,
				    0, 4,	/* M */
				    24, 1,	/* mux */
				    BIT(31),	/* gate */
				    CLK_SET_RATE_PARENT);

static SUNXI_CCU_GATE_HWS(bus_i2s0_clk, "bus-i2s0", apb0_hws, 0xa2c,
			  BIT(0), 0);

static SUNXI_CCU_M_HW_WITH_MUX_GATE(audio_codec_dac_clk, "audio-codec-dac",
				    audio_parents, 0xa50,
				    0, 4,	/* M */
				    24, 1,	/* mux */
				    BIT(31),	/* gate */
				    CLK_SET_RATE_PARENT);

static SUNXI_CCU_M_HW_WITH_MUX_GATE(audio_codec_adc_clk, "audio-codec-adc",
				    audio_parents, 0xa54,
				    0, 4,	/* M */
				    24, 1,	/* mux */
				    BIT(31),	/* gate */
				    CLK_SET_RATE_PARENT);

static SUNXI_CCU_GATE_HWS(bus_audio_codec_clk, "bus-audio-codec", apb0_hws,
			  0xa5c, BIT(0), 0);

/*
 * The first two parents provide 12 MHz, from a 48 MHz clock synchronized by
 * pll-periph0 and from the 24 MHz oscillator.
 */
static const struct clk_parent_data usb_ohci_parents[] = {
	{ .hw = &pll_periph0_4x_clk.common.hw },
	{ .fw_name = "hosc" },
	{ .fw_name = "losc" },
	{ .fw_name = "iosc" },
};

static const struct ccu_mux_fixed_prediv usb_ohci_predivs[] = {
	{ .index = 0, .div = 200 },
	{ .index = 1, .div = 2 },
};

static struct ccu_mux usb_ohci0_clk = {
	.enable		= BIT(31),
	.mux		= {
		.shift		= 24,
		.width		= 2,
		.fixed_predivs	= usb_ohci_predivs,
		.n_predivs	= ARRAY_SIZE(usb_ohci_predivs),
	},
	.common		= {
		.reg		= 0xa70,
		.features	= CCU_FEATURE_FIXED_PREDIV,
		.hw.init	= CLK_HW_INIT_PARENTS_DATA("usb-ohci0",
							   usb_ohci_parents,
							   &ccu_mux_ops,
							   0),
	},
};

static SUNXI_CCU_GATE_HWS(bus_ohci0_clk, "bus-ohci0", ahb_hws, 0xa8c,
			  BIT(0), 0);
static SUNXI_CCU_GATE_HWS(bus_ehci0_clk, "bus-ehci0", ahb_hws, 0xa8c,
			  BIT(4), 0);
static SUNXI_CCU_GATE_HWS(bus_otg_clk, "bus-otg", ahb_hws, 0xa8c, BIT(8), 0);

static SUNXI_CCU_GATE_HWS(bus_dpss_top_clk, "bus-dpss-top", ahb_hws, 0xabc,
			  BIT(0), 0);

static const struct clk_hw *tcon_lcd_parents[] = {
	&pll_video_4x_clk.common.hw,
	&pll_periph0_2x_clk.common.hw,
	&pll_csi_4x_clk.common.hw,
};

static SUNXI_CCU_MP_HW_WITH_MUX_GATE(tcon_lcd_clk, "tcon-lcd",
				     tcon_lcd_parents, 0xb60,
				     0, 4,	/* M */
				     8, 2,	/* P */
				     24, 3,	/* mux */
				     BIT(31),	/* gate */
				     CLK_SET_RATE_PARENT);

static SUNXI_CCU_GATE_HWS(bus_tcon_lcd_clk, "bus-tcon-lcd", ahb_hws, 0xb7c,
			  BIT(0), 0);

static const struct clk_hw *csi_parents[] = {
	&pll_periph0_300M_clk.hw,
	&pll_periph0_400M_clk.hw,
	&pll_video_4x_clk.common.hw,
	&pll_csi_4x_clk.common.hw,
};

static SUNXI_CCU_M_HW_WITH_MUX_GATE(csi_clk, "csi", csi_parents, 0xc04,
				    0, 5,	/* M */
				    24, 3,	/* mux */
				    BIT(31),	/* gate */
				    0);

static const struct clk_parent_data csi_mclk_parents[] = {
	{ .fw_name = "hosc" },
	{ .hw = &pll_csi_4x_clk.common.hw },
	{ .hw = &pll_video_4x_clk.common.hw },
	{ .hw = &pll_periph0_2x_clk.common.hw },
};

static SUNXI_CCU_MP_DATA_WITH_MUX_GATE(csi_mclk0_clk, "csi-mclk0",
				       csi_mclk_parents, 0xc08,
				       0, 5,	/* M */
				       8, 2,	/* P */
				       24, 3,	/* mux */
				       BIT(31),	/* gate */
				       0);
static SUNXI_CCU_MP_DATA_WITH_MUX_GATE(csi_mclk1_clk, "csi-mclk1",
				       csi_mclk_parents, 0xc0c,
				       0, 5,	/* M */
				       8, 2,	/* P */
				       24, 3,	/* mux */
				       BIT(31),	/* gate */
				       0);
static SUNXI_CCU_MP_DATA_WITH_MUX_GATE(csi_mclk2_clk, "csi-mclk2",
				       csi_mclk_parents, 0xc10,
				       0, 5,	/* M */
				       8, 2,	/* P */
				       24, 3,	/* mux */
				       BIT(31),	/* gate */
				       0);

static SUNXI_CCU_GATE_HWS(bus_csi_clk, "bus-csi", ahb_hws, 0xc2c, BIT(0), 0);

/* The E907 clocks between 0xd00 and 0xd0c are left to the firmware. */

static SUNXI_CCU_GATE_DATA(fanout_24M_clk, "fanout-24M", osc24M,
			   0xf30, BIT(0), 0);
static SUNXI_CCU_GATE_DATA_WITH_PREDIV(fanout_12M_clk, "fanout-12M", osc24M,
				       0xf30, BIT(1), 2, 0);
static const struct clk_hw *pll_periph0_160M_hws[] = {
	&pll_periph0_160M_clk.hw
};

static SUNXI_CCU_GATE_HWS_WITH_PREDIV(fanout_16M_clk, "fanout-16M",
				      pll_periph0_160M_hws,
				      0xf30, BIT(2), 10, 0);
static SUNXI_CCU_GATE_HWS_WITH_PREDIV(fanout_25M_clk, "fanout-25M",
				      pll_periph0_150M_hws,
				      0xf30, BIT(3), 6, 0);

static const struct clk_parent_data fanout_27M_parents[] = {
	{ .hw = &pll_video_1x_clk.hw },
	{ .hw = &pll_csi_1x_clk.hw },
	{ .hw = &pll_periph0_300M_clk.hw },
};

static SUNXI_CCU_DUALDIV_MUX_GATE(fanout_27M_clk, "fanout-27M",
				  fanout_27M_parents, 0xf34,
				  0, 5,		/* div0 */
				  8, 2,		/* div1 */
				  24, 2,	/* mux */
				  BIT(31),	/* gate */
				  0);

static const struct clk_parent_data fanout_pclk_parents[] = {
	{ .hw = &apb0_clk.common.hw }
};

static SUNXI_CCU_DUALDIV_MUX_GATE(fanout_pclk_clk, "fanout-pclk",
				  fanout_pclk_parents, 0xf38,
				  0, 5,		/* div0 */
				  5, 5,		/* div1 */
				  0, 0,		/* mux */
				  BIT(31),	/* gate */
				  0);

static const struct clk_parent_data fanout_parents[] = {
	{ .fw_name = "losc" },
	{ .hw = &fanout_12M_clk.common.hw, },
	{ .hw = &fanout_16M_clk.common.hw, },
	{ .hw = &fanout_24M_clk.common.hw, },
	{ .hw = &fanout_25M_clk.common.hw, },
	{ .hw = &fanout_27M_clk.common.hw, },
	{ .hw = &fanout_pclk_clk.common.hw, },
};

static SUNXI_CCU_MUX_DATA_WITH_GATE(fanout0_clk, "fanout0", fanout_parents,
				    0xf3c,
				    0, 3,	/* mux */
				    BIT(21),	/* gate */
				    0);
static SUNXI_CCU_MUX_DATA_WITH_GATE(fanout1_clk, "fanout1", fanout_parents,
				    0xf3c,
				    3, 3,	/* mux */
				    BIT(22),	/* gate */
				    0);
static SUNXI_CCU_MUX_DATA_WITH_GATE(fanout2_clk, "fanout2", fanout_parents,
				    0xf3c,
				    6, 3,	/* mux */
				    BIT(23),	/* gate */
				    0);

static struct ccu_common *sun252i_v861_ccu_clks[] = {
	&pll_ddr_clk.common,
	&pll_periph0_4x_clk.common,
	&pll_periph0_2x_clk.common,
	&pll_periph0_800M_clk.common,
	&pll_periph0_480M_clk.common,
	&pll_video_8x_clk.common,
	&pll_video_4x_clk.common,
	&pll_csi_8x_clk.common,
	&pll_csi_4x_clk.common,
	&pll_audio_clk.common,
	&pll_audio_div2_clk.common,
	&pll_audio_div3_clk.common,
	&pll_audio_div5_clk.common,
	&pll_audio_4x_clk.common,
	&pll_audio_1x_clk.common,
	&pll_npu_8x_clk.common,
	&pll_npu_4x_clk.common,
	&ahb_clk.common,
	&apb0_clk.common,
	&apb1_clk.common,
	&apb_uart_clk.common,
	&de_clk.common,
	&bus_de_clk.common,
	&g2d_clk.common,
	&bus_g2d_clk.common,
	&ce_clk.common,
	&bus_ce_clk.common,
	&bus_ce_sys_clk.common,
	&ve_clk.common,
	&bus_ve_clk.common,
	&npu_clk.common,
	&bus_npu_clk.common,
	&bus_sgdma_clk.common,
	&bus_ndma_clk.common,
	&bus_msgbox0_clk.common,
	&bus_msgbox1_clk.common,
	&bus_spinlock_clk.common,
	&bus_hstimer_clk.common,
	&avs_clk.common,
	&bus_timer_clk.common,
	&bus_dbg_clk.common,
	&bus_pwm_clk.common,
	&dram_clk.common,
	&mbus_dma_clk.common,
	&mbus_ve_clk.common,
	&mbus_ce_clk.common,
	&mbus_csi_clk.common,
	&mbus_isp_clk.common,
	&mbus_g2d_clk.common,
	&bus_dram_clk.common,
	&mmc0_clk.common,
	&mmc1_clk.common,
	&mmc2_clk.common,
	&bus_mmc0_clk.common,
	&bus_mmc1_clk.common,
	&bus_mmc2_clk.common,
	&psram_clk.common,
	&bus_psram_clk.common,
	&bus_uart0_clk.common,
	&bus_uart1_clk.common,
	&bus_uart2_clk.common,
	&bus_uart3_clk.common,
	&bus_i2c0_clk.common,
	&bus_i2c1_clk.common,
	&bus_i2c2_clk.common,
	&bus_i2c3_clk.common,
	&spi0_clk.common,
	&spi1_clk.common,
	&spi2_clk.common,
	&spif_clk.common,
	&bus_spi0_clk.common,
	&bus_spi1_clk.common,
	&bus_spi2_clk.common,
	&bus_spif_clk.common,
	&gmac_25M_clk.common,
	&bus_gmac_clk.common,
	&bus_gpadc_clk.common,
	&bus_ths_clk.common,
	&i2s0_clk.common,
	&bus_i2s0_clk.common,
	&audio_codec_dac_clk.common,
	&audio_codec_adc_clk.common,
	&bus_audio_codec_clk.common,
	&usb_ohci0_clk.common,
	&bus_ohci0_clk.common,
	&bus_ehci0_clk.common,
	&bus_otg_clk.common,
	&bus_dpss_top_clk.common,
	&tcon_lcd_clk.common,
	&bus_tcon_lcd_clk.common,
	&csi_clk.common,
	&csi_mclk0_clk.common,
	&csi_mclk1_clk.common,
	&csi_mclk2_clk.common,
	&bus_csi_clk.common,
	&fanout_24M_clk.common,
	&fanout_12M_clk.common,
	&fanout_16M_clk.common,
	&fanout_25M_clk.common,
	&fanout_27M_clk.common,
	&fanout_pclk_clk.common,
	&fanout0_clk.common,
	&fanout1_clk.common,
	&fanout2_clk.common,
};

static struct clk_hw_onecell_data sun252i_v861_hw_clks = {
	.num	= CLK_FANOUT2 + 1,
	.hws	= {
		[CLK_PLL_DDR]		= &pll_ddr_clk.common.hw,
		[CLK_PLL_PERIPH0_4X]	= &pll_periph0_4x_clk.common.hw,
		[CLK_PLL_PERIPH0_2X]	= &pll_periph0_2x_clk.common.hw,
		[CLK_PLL_PERIPH0_800M]	= &pll_periph0_800M_clk.common.hw,
		[CLK_PLL_PERIPH0_480M]	= &pll_periph0_480M_clk.common.hw,
		[CLK_PLL_PERIPH0_600M]	= &pll_periph0_600M_clk.hw,
		[CLK_PLL_PERIPH0_400M]	= &pll_periph0_400M_clk.hw,
		[CLK_PLL_PERIPH0_300M]	= &pll_periph0_300M_clk.hw,
		[CLK_PLL_PERIPH0_200M]	= &pll_periph0_200M_clk.hw,
		[CLK_PLL_PERIPH0_160M]	= &pll_periph0_160M_clk.hw,
		[CLK_PLL_PERIPH0_150M]	= &pll_periph0_150M_clk.hw,
		[CLK_PLL_VIDEO_8X]	= &pll_video_8x_clk.common.hw,
		[CLK_PLL_VIDEO_4X]	= &pll_video_4x_clk.common.hw,
		[CLK_PLL_VIDEO_2X]	= &pll_video_2x_clk.hw,
		[CLK_PLL_VIDEO_1X]	= &pll_video_1x_clk.hw,
		[CLK_PLL_CSI_8X]	= &pll_csi_8x_clk.common.hw,
		[CLK_PLL_CSI_4X]	= &pll_csi_4x_clk.common.hw,
		[CLK_PLL_CSI_2X]	= &pll_csi_2x_clk.hw,
		[CLK_PLL_CSI_1X]	= &pll_csi_1x_clk.hw,
		[CLK_PLL_AUDIO]		= &pll_audio_clk.common.hw,
		[CLK_PLL_AUDIO_DIV2]	= &pll_audio_div2_clk.common.hw,
		[CLK_PLL_AUDIO_DIV3]	= &pll_audio_div3_clk.common.hw,
		[CLK_PLL_AUDIO_DIV5]	= &pll_audio_div5_clk.common.hw,
		[CLK_PLL_AUDIO_4X]	= &pll_audio_4x_clk.common.hw,
		[CLK_PLL_AUDIO_1X]	= &pll_audio_1x_clk.common.hw,
		[CLK_PLL_NPU_8X]	= &pll_npu_8x_clk.common.hw,
		[CLK_PLL_NPU_4X]	= &pll_npu_4x_clk.common.hw,
		[CLK_PLL_NPU_2X]	= &pll_npu_2x_clk.hw,
		[CLK_PLL_NPU_1X]	= &pll_npu_1x_clk.hw,
		[CLK_AHB]		= &ahb_clk.common.hw,
		[CLK_APB0]		= &apb0_clk.common.hw,
		[CLK_APB1]		= &apb1_clk.common.hw,
		[CLK_APB_UART]		= &apb_uart_clk.common.hw,
		[CLK_DE]		= &de_clk.common.hw,
		[CLK_BUS_DE]		= &bus_de_clk.common.hw,
		[CLK_G2D]		= &g2d_clk.common.hw,
		[CLK_BUS_G2D]		= &bus_g2d_clk.common.hw,
		[CLK_CE]		= &ce_clk.common.hw,
		[CLK_BUS_CE]		= &bus_ce_clk.common.hw,
		[CLK_BUS_CE_SYS]	= &bus_ce_sys_clk.common.hw,
		[CLK_VE]		= &ve_clk.common.hw,
		[CLK_BUS_VE]		= &bus_ve_clk.common.hw,
		[CLK_NPU]		= &npu_clk.common.hw,
		[CLK_BUS_NPU]		= &bus_npu_clk.common.hw,
		[CLK_BUS_SGDMA]		= &bus_sgdma_clk.common.hw,
		[CLK_BUS_NDMA]		= &bus_ndma_clk.common.hw,
		[CLK_BUS_MSGBOX0]	= &bus_msgbox0_clk.common.hw,
		[CLK_BUS_MSGBOX1]	= &bus_msgbox1_clk.common.hw,
		[CLK_BUS_SPINLOCK]	= &bus_spinlock_clk.common.hw,
		[CLK_BUS_HSTIMER]	= &bus_hstimer_clk.common.hw,
		[CLK_AVS]		= &avs_clk.common.hw,
		[CLK_BUS_TIMER]		= &bus_timer_clk.common.hw,
		[CLK_BUS_DBG]		= &bus_dbg_clk.common.hw,
		[CLK_BUS_PWM]		= &bus_pwm_clk.common.hw,
		[CLK_DRAM]		= &dram_clk.common.hw,
		[CLK_MBUS]		= &mbus_clk.hw,
		[CLK_MBUS_DMA]		= &mbus_dma_clk.common.hw,
		[CLK_MBUS_VE]		= &mbus_ve_clk.common.hw,
		[CLK_MBUS_CE]		= &mbus_ce_clk.common.hw,
		[CLK_MBUS_CSI]		= &mbus_csi_clk.common.hw,
		[CLK_MBUS_ISP]		= &mbus_isp_clk.common.hw,
		[CLK_MBUS_G2D]		= &mbus_g2d_clk.common.hw,
		[CLK_BUS_DRAM]		= &bus_dram_clk.common.hw,
		[CLK_MMC0]		= &mmc0_clk.common.hw,
		[CLK_MMC1]		= &mmc1_clk.common.hw,
		[CLK_MMC2]		= &mmc2_clk.common.hw,
		[CLK_BUS_MMC0]		= &bus_mmc0_clk.common.hw,
		[CLK_BUS_MMC1]		= &bus_mmc1_clk.common.hw,
		[CLK_BUS_MMC2]		= &bus_mmc2_clk.common.hw,
		[CLK_PSRAM]		= &psram_clk.common.hw,
		[CLK_BUS_PSRAM]		= &bus_psram_clk.common.hw,
		[CLK_BUS_UART0]		= &bus_uart0_clk.common.hw,
		[CLK_BUS_UART1]		= &bus_uart1_clk.common.hw,
		[CLK_BUS_UART2]		= &bus_uart2_clk.common.hw,
		[CLK_BUS_UART3]		= &bus_uart3_clk.common.hw,
		[CLK_BUS_I2C0]		= &bus_i2c0_clk.common.hw,
		[CLK_BUS_I2C1]		= &bus_i2c1_clk.common.hw,
		[CLK_BUS_I2C2]		= &bus_i2c2_clk.common.hw,
		[CLK_BUS_I2C3]		= &bus_i2c3_clk.common.hw,
		[CLK_SPI0]		= &spi0_clk.common.hw,
		[CLK_SPI1]		= &spi1_clk.common.hw,
		[CLK_SPI2]		= &spi2_clk.common.hw,
		[CLK_SPIF]		= &spif_clk.common.hw,
		[CLK_BUS_SPI0]		= &bus_spi0_clk.common.hw,
		[CLK_BUS_SPI1]		= &bus_spi1_clk.common.hw,
		[CLK_BUS_SPI2]		= &bus_spi2_clk.common.hw,
		[CLK_BUS_SPIF]		= &bus_spif_clk.common.hw,
		[CLK_GMAC_25M]		= &gmac_25M_clk.common.hw,
		[CLK_BUS_GMAC]		= &bus_gmac_clk.common.hw,
		[CLK_BUS_GPADC]		= &bus_gpadc_clk.common.hw,
		[CLK_BUS_THS]		= &bus_ths_clk.common.hw,
		[CLK_I2S0]		= &i2s0_clk.common.hw,
		[CLK_BUS_I2S0]		= &bus_i2s0_clk.common.hw,
		[CLK_AUDIO_CODEC_DAC]	= &audio_codec_dac_clk.common.hw,
		[CLK_AUDIO_CODEC_ADC]	= &audio_codec_adc_clk.common.hw,
		[CLK_BUS_AUDIO_CODEC]	= &bus_audio_codec_clk.common.hw,
		[CLK_USB_OHCI0]		= &usb_ohci0_clk.common.hw,
		[CLK_BUS_OHCI0]		= &bus_ohci0_clk.common.hw,
		[CLK_BUS_EHCI0]		= &bus_ehci0_clk.common.hw,
		[CLK_BUS_OTG]		= &bus_otg_clk.common.hw,
		[CLK_BUS_DPSS_TOP]	= &bus_dpss_top_clk.common.hw,
		[CLK_TCON_LCD]		= &tcon_lcd_clk.common.hw,
		[CLK_BUS_TCON_LCD]	= &bus_tcon_lcd_clk.common.hw,
		[CLK_CSI]		= &csi_clk.common.hw,
		[CLK_CSI_MCLK0]		= &csi_mclk0_clk.common.hw,
		[CLK_CSI_MCLK1]		= &csi_mclk1_clk.common.hw,
		[CLK_CSI_MCLK2]		= &csi_mclk2_clk.common.hw,
		[CLK_BUS_CSI]		= &bus_csi_clk.common.hw,
		[CLK_FANOUT_24M]	= &fanout_24M_clk.common.hw,
		[CLK_FANOUT_12M]	= &fanout_12M_clk.common.hw,
		[CLK_FANOUT_16M]	= &fanout_16M_clk.common.hw,
		[CLK_FANOUT_25M]	= &fanout_25M_clk.common.hw,
		[CLK_FANOUT_27M]	= &fanout_27M_clk.common.hw,
		[CLK_FANOUT_PCLK]	= &fanout_pclk_clk.common.hw,
		[CLK_FANOUT0]		= &fanout0_clk.common.hw,
		[CLK_FANOUT1]		= &fanout1_clk.common.hw,
		[CLK_FANOUT2]		= &fanout2_clk.common.hw,
	},
};

static const struct ccu_reset_map sun252i_v861_ccu_resets[] = {
	[RST_MBUS]		= { 0x540, BIT(30) },
	[RST_BUS_DE]		= { 0x60c, BIT(16) },
	[RST_BUS_G2D]		= { 0x63c, BIT(16) },
	[RST_BUS_CE]		= { 0x68c, BIT(16) },
	[RST_BUS_CE_SYS]	= { 0x68c, BIT(17) },
	[RST_BUS_VE]		= { 0x69c, BIT(16) },
	[RST_BUS_NPU]		= { 0x6ec, BIT(16) },
	[RST_BUS_SGDMA]		= { 0x70c, BIT(16) },
	[RST_BUS_NDMA]		= { 0x70c, BIT(17) },
	[RST_BUS_MSGBOX0]	= { 0x71c, BIT(16) },
	[RST_BUS_MSGBOX1]	= { 0x71c, BIT(17) },
	[RST_BUS_SPINLOCK]	= { 0x72c, BIT(16) },
	[RST_BUS_HSTIMER]	= { 0x73c, BIT(16) },
	[RST_BUS_TIMER]		= { 0x74c, BIT(16) },
	[RST_BUS_DBG]		= { 0x78c, BIT(16) },
	[RST_BUS_PWM]		= { 0x7ac, BIT(16) },
	[RST_BUS_DRAM]		= { 0x80c, BIT(16) },
	[RST_BUS_MMC0]		= { 0x84c, BIT(16) },
	[RST_BUS_MMC1]		= { 0x84c, BIT(17) },
	[RST_BUS_MMC2]		= { 0x84c, BIT(18) },
	[RST_BUS_PSRAM]		= { 0x85c, BIT(16) },
	[RST_BUS_UART0]		= { 0x90c, BIT(16) },
	[RST_BUS_UART1]		= { 0x90c, BIT(17) },
	[RST_BUS_UART2]		= { 0x90c, BIT(18) },
	[RST_BUS_UART3]		= { 0x90c, BIT(19) },
	[RST_BUS_I2C0]		= { 0x91c, BIT(16) },
	[RST_BUS_I2C1]		= { 0x91c, BIT(17) },
	[RST_BUS_I2C2]		= { 0x91c, BIT(18) },
	[RST_BUS_I2C3]		= { 0x91c, BIT(19) },
	[RST_BUS_SPI0]		= { 0x96c, BIT(16) },
	[RST_BUS_SPI1]		= { 0x96c, BIT(17) },
	[RST_BUS_SPI2]		= { 0x96c, BIT(18) },
	[RST_BUS_SPIF]		= { 0x96c, BIT(20) },
	[RST_BUS_GMAC]		= { 0x97c, BIT(16) },
	[RST_BUS_GPADC]		= { 0x9ec, BIT(16) },
	[RST_BUS_THS]		= { 0x9fc, BIT(16) },
	[RST_BUS_I2S0]		= { 0xa2c, BIT(16) },
	[RST_BUS_AUDIO_CODEC]	= { 0xa5c, BIT(16) },
	[RST_USB_PHY0]		= { 0xa70, BIT(30) },
	[RST_BUS_OHCI0]		= { 0xa8c, BIT(16) },
	[RST_BUS_EHCI0]		= { 0xa8c, BIT(20) },
	[RST_BUS_OTG]		= { 0xa8c, BIT(24) },
	[RST_BUS_DPSS_TOP]	= { 0xabc, BIT(16) },
	[RST_BUS_TCON_LCD]	= { 0xb7c, BIT(16) },
	[RST_BUS_CSI]		= { 0xc2c, BIT(16) },
};

static const struct sunxi_ccu_desc sun252i_v861_ccu_desc = {
	.ccu_clks	= sun252i_v861_ccu_clks,
	.num_ccu_clks	= ARRAY_SIZE(sun252i_v861_ccu_clks),

	.hw_clks	= &sun252i_v861_hw_clks,

	.resets		= sun252i_v861_ccu_resets,
	.num_resets	= ARRAY_SIZE(sun252i_v861_ccu_resets),
};

static const u32 pll_regs[] = {
	SUN252I_V861_PLL_DDR_REG,
	SUN252I_V861_PLL_PERIPH0_REG,
	SUN252I_V861_PLL_VIDEO_REG,
	SUN252I_V861_PLL_CSI_REG,
	SUN252I_V861_PLL_AUDIO_REG,
	SUN252I_V861_PLL_NPU_REG,
};

static int sun252i_v861_ccu_probe(struct platform_device *pdev)
{
	void __iomem *reg;
	u32 val;
	int i;

	reg = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(reg))
		return PTR_ERR(reg);

	/*
	 * The PLL clock code does not model all bits, for instance it does
	 * not support a separate enable and gate bit. We present the
	 * gate bit(27) as the enable bit, but then have to set the
	 * PLL Enable, LDO Enable, and Lock Enable bits on all PLLs here.
	 */
	for (i = 0; i < ARRAY_SIZE(pll_regs); i++) {
		val = readl(reg + pll_regs[i]);
		val |= BIT(31) | BIT(30) | BIT(29);
		writel(val, reg + pll_regs[i]);
	}

	/* Derive the 4x and 1x audio clocks from the integer PLL outputs. */
	val = readl(reg + SUN252I_V861_PLL_AUDIO_PRE_DIV_REG);
	val &= ~(BIT(28) | BIT(24));
	writel(val, reg + SUN252I_V861_PLL_AUDIO_PRE_DIV_REG);

	return devm_sunxi_ccu_probe(&pdev->dev, reg, &sun252i_v861_ccu_desc);
}

static const struct of_device_id sun252i_v861_ccu_ids[] = {
	{ .compatible = "allwinner,sun252i-v861-ccu" },
	{ }
};
MODULE_DEVICE_TABLE(of, sun252i_v861_ccu_ids);

static struct platform_driver sun252i_v861_ccu_driver = {
	.probe	= sun252i_v861_ccu_probe,
	.driver	= {
		.name			= "sun252i-v861-ccu",
		.suppress_bind_attrs	= true,
		.of_match_table		= sun252i_v861_ccu_ids,
	},
};
module_platform_driver(sun252i_v861_ccu_driver);

MODULE_IMPORT_NS("SUNXI_CCU");
MODULE_DESCRIPTION("Support for the Allwinner V861 CCU");
MODULE_LICENSE("GPL");
