// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Zhihe Computing Limited.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/reset-controller.h>
#include <linux/spinlock.h>

#include <dt-bindings/reset/zhihe,a210-resets.h>

struct a210_rst_signal {
	u16 offset;
	u32 mask;
	/* Zero terminates a chain of signals that share one reset ID. */
	u16 nextid;
};

struct a210_reset_data {
	const struct a210_rst_signal *signals;
	unsigned int num_signals;
};

struct a210_reset {
	struct reset_controller_dev rcdev;
	void __iomem *base;
	const struct a210_reset_data *data;
	/* Protects register updates and snapshots of linked reset signals. */
	spinlock_t lock;
};

static const struct a210_rst_signal vp_rst_signals[] = {
	[VP_VDEC_PRST] = { 0, BIT(0), 0 },
	[VP_VDEC_CRST] = { 0, BIT(1), 0 },
	[VP_VDEC_ARST] = { 0, BIT(2), 0 },
	[VP_VENC_PRST] = { 0, BIT(3), 0 },
	[VP_VENC_CRST] = { 0, BIT(4), 0 },
	[VP_VENC_ARST] = { 0, BIT(5), 0 },
	[VP_G2D_PRST] = { 0, BIT(6), 0 },
	[VP_G2D_CRST] = { 0, BIT(7), 0 },
	[VP_G2D_ARST] = { 0, BIT(8), 0 },
	[VP_COMP_PRST] = { 0, BIT(9), 0 },
	[VP_COMP_CRST] = { 0, BIT(10), 0 },
	[VP_COMP_ARST] = { 0, BIT(11), 0 },
	[VP_DECOMP_PRST] = { 0, BIT(12), 0 },
	[VP_DECOMP_CRST] = { 0, BIT(13), 0 },
	[VP_DECOMP_ARST] = { 0, BIT(14), 0 },
	[VP_DFMU_PRST] = { 0, BIT(15), 0 },
	[VP_DFMU_ARST] = { 0, BIT(16), 0 },
	[VP_ARB_PRST] = { 0, BIT(18), 0 },
	[VP_ARB_ARST] = { 0, BIT(19), 0 },
	[VP_VDEC_RS_ARST] = { 0, BIT(20), 0 },
	[VP_VENC_RS_ARST] = { 0, BIT(21), 0 },
	[VP_COMP_EXTPRST] = { 0, BIT(22), 0 },
	[VP_DECOMP_EXTPRST] = { 0, BIT(23), 0 },
};

static const struct a210_rst_signal vi_rst_signals[] = {
	[VI_MIPI0_CSI0_PRST] = { 0, BIT(0), 0 },
	[VI_MIPI0_CSI1_PRST] = { 0, BIT(1), 0 },
	[VI_MIPI0_FIFO_RST] = { 0, BIT(2), 0 },
	[VI_MIPI1_CSI0_PRST] = { 0, BIT(3), 0 },
	[VI_MIPI1_CSI1_PRST] = { 0, BIT(4), 0 },
	[VI_MIPI1_FIFO_RST] = { 0, BIT(5), 0 },
	[VI_VIPRE_PRST] = { 0, BIT(6), 0 },
	[VI_VIPRE_I0_PIX_RST] = { 0, BIT(7), 0 },
	[VI_VIPRE_I1_PIX_RST] = { 0, BIT(8), 0 },
	[VI_VIPRE_ISPIF_RST] = { 0, BIT(9), 0 },
	[VI_VIPRE_ARST] = { 0, BIT(10), 0 },
	[VI_ISP_RST] = { 0, BIT(11), 0 },
	[VI_COMP_PRST] = { 0, BIT(12), 0 },
	[VI_COMP_ARST] = { 0, BIT(13), 0 },
	[VI_COMP_ISPOUT_RST] = { 0, BIT(14), 0 },
	[VI_COMP_DECOUT_RST] = { 0, BIT(15), 0 },
	[VI_COMP_VSEOUT_RST] = { 0, BIT(16), 0 },
	[VI_COMP0_RST] = { 0, BIT(17), 0 },
	[VI_COMP1_RST] = { 0, BIT(18), 0 },
	[VI_DECOMP_RST] = { 0, BIT(19), 0 },
	[VI_DW200_RST] = { 0, BIT(20), 0 },
	[VI_DFMU_PRST] = { 0, BIT(21), 0 },
	[VI_DFMU_ARST] = { 0, BIT(22), 0 },
	[VI_ARB_PRST] = { 0, BIT(23), 0 },
	[VI_ARB_ARST] = { 0, BIT(24), 0 },
	[VI_X2H0_HRST] = { 0, BIT(26), 0 },
	[VI_X2H1_HRST] = { 0, BIT(27), 0 },
	[VI_X2H2_HRST] = { 0, BIT(28), 0 },
	[VI_REC_PRST] = { 0, BIT(29), 0 },
	[VI_REC_ARST] = { 0, BIT(30), 0 },
};

static const struct a210_rst_signal npu_rst_signals[] = {
	[NPU_AAB_MST_RST] = { 0, BIT(0), 0 },
	[NPU_AFENCE_RST] = { 0, BIT(1), 0 },
	[NPU_AXI_CCU_PRST] = { 0, BIT(2), 0 },
	[NPU_AXI_CCU_RST] = { 0, BIT(3), 0 },
	[NPU_AXI_M2S1_RST] = { 0, BIT(4), 0 },
	[NPU_AXI_RST] = { 0, BIT(5), 0 },
	[NPU_CLK_CALC_RST] = { 0, BIT(6), 0 },
	[NPU_DFMU_ARST] = { 0, BIT(7), 0 },
	[NPU_DFMU_PRST] = { 0, BIT(8), 0 },
	[NPU_LPC_C_RST] = { 0, BIT(9), 0 },
	[NPU_LPE_C_PRST] = { 0, BIT(10), 0 },
	[NPU_LPE_C_RST] = { 0, BIT(11), 0 },
	[NPU_ARST] = { 0, BIT(12), 0 },
	[NPU_CRST] = { 0, BIT(13), 0 },
	[NPU_HRST] = { 0, BIT(14), 0 },
	[NPU_PCTRL_PB_RST] = { 0, BIT(15), 0 },
	[NPU_PCTRL_PRST] = { 0, BIT(16), 0 },
	[NPU_PCTRL_RST] = { 0, BIT(17), 0 },
	[NPU_PTW_CCU_PRST] = { 0, BIT(18), 0 },
	[NPU_PTW_CCU_RST] = { 0, BIT(19), 0 },
	[NPU_SEMA_ARST] = { 0, BIT(20), 0 },
	[NPU_SEMA_PRST] = { 0, BIT(21), 0 },
	[NPU_DROOP_CCTRL_PRST] = { 0, BIT(22), 0 },
	[NPU_DROOP_CCTRL_RST] = { 0, BIT(23), 0 },
	[NPU_X2H_RST] = { 0, BIT(24), 0 },
	[NPU_X2P_RST] = { 0, BIT(25), 0 },
	[NPU_LDIP_RST] = { 0, BIT(26), 0 },
	[NPU_SRAM_RST] = { 0, BIT(27), 0 },
};

static const struct a210_rst_signal vo_rst_signals[] = {
	[VO_X2H0_RST] = { 0, BIT(0), 0 },
	[VO_X2H1_RST] = { 0, BIT(1), 0 },
	[VO_DPU_HRST] = { 0, BIT(2), 0 },
	[VO_DPU_CRST] = { 0, BIT(3), 0 },
	[VO_DPU_ARST] = { 0, BIT(4), 0 },
	[VO_AUXDISP_PRST] = { 0, BIT(5), 0 },
	[VO_AUXDISP_PIX_RST] = { 0, BIT(6), 0 },
	[VO_AUXDISP_ARST] = { 0, BIT(7), 0 },
	[VO_HDMI_PRST] = { 0, BIT(8), 0 },
	[VO_HDMI_MAIN_RST] = { 0, BIT(9), 0 },
	[VO_MIPI_PRST] = { 0, BIT(10), 0 },
	[VO_DECOMP_PRST] = { 0, BIT(11), 0 },
	[VO_DECOMP0_CRST] = { 0, BIT(12), 0 },
	[VO_DECOMP1_CRST] = { 0, BIT(13), 0 },
	[VO_DECOMP_ARST] = { 0, BIT(14), 0 },
	[VO_ARB_PRST] = { 0, BIT(15), 0 },
	[VO_ARB_ARST] = { 0, BIT(16), 0 },
	[VO_DFMU_PRST] = { 0, BIT(17), 0 },
	[VO_DFMU_ARST] = { 0, BIT(18), 0 },
};

static const struct a210_rst_signal peri0_rst_signals[] = {
	[PERI0_TIMER0_CRST] = { 0, BIT(0), 0 },
	[PERI0_TIMER0_PRST] = { 0, BIT(1), 0 },
	[PERI0_TIMER1_CRST] = { 0, BIT(2), 0 },
	[PERI0_TIMER1_PRST] = { 0, BIT(3), 0 },
	[PERI0_WDT0_PRST] = { 0, BIT(4), 0 },
	[PERI0_MBOX0_PRST] = { 0, BIT(5), 0 },
	[PERI0_MBOX1_PRST] = { 0, BIT(6), 0 },
	[PERI0_TIMER0_RST] = { 0, BIT(0) | BIT(1), 0 },
	[PERI0_TIMER1_RST] = { 0, BIT(2) | BIT(3), 0 },
};

static const struct a210_rst_signal peri1_rst_signals[] = {
	[PERI1_GMAC0_ARST] = { 0, BIT(0), 0 },
	[PERI1_GMAC0_HRST] = { 0, BIT(1), 0 },
	[PERI1_GMAC1_ARST] = { 0, BIT(2), 0 },
	[PERI1_GMAC1_HRST] = { 0, BIT(3), 0 },
	[PERI1_GPIO0_DBRST] = { 0, BIT(4), 0 },
	[PERI1_GPIO0_PRST] = { 0, BIT(5), 0 },
	[PERI1_GPIO1_DBRST] = { 0, BIT(6), 0 },
	[PERI1_GPIO1_PRST] = { 0, BIT(7), 0 },
	[PERI1_I2C0_IC_RST] = { 0, BIT(8), 0 },
	[PERI1_I2C0_PRST] = { 0, BIT(9), 0 },
	[PERI1_I2C1_IC_RST] = { 0, BIT(10), 0 },
	[PERI1_I2C1_PRST] = { 0, BIT(11), 0 },
	[PERI1_I2C2_IC_RST] = { 0, BIT(12), 0 },
	[PERI1_I2C2_PRST] = { 0, BIT(13), 0 },
	[PERI1_I2S0_PRST] = { 0, BIT(14), 0 },
	[PERI1_MST_BUS_ARST] = { 0, BIT(15), 0 },
	[PERI1_MST_BUS_PRST] = { 0, BIT(16), 0 },
	[PERI1_PWM0_CRST] = { 0, BIT(17), 0 },
	[PERI1_PWM0_PRST] = { 0, BIT(18), 0 },
	[PERI1_QSPI0_PRST] = { 0, BIT(19), 0 },
	[PERI1_QSPI0_SSI_RST] = { 0, BIT(20), 0 },
	[PERI1_SPI0_PRST] = { 0, BIT(21), 0 },
	[PERI1_SPI0_SSI_RST] = { 0, BIT(22), 0 },
	[PERI1_UART0_PRST] = { 0, BIT(23), PERI1_UART0_S_RST },
	[PERI1_UART0_S_RST] = { 0, BIT(24), 0 },
	[PERI1_UART1_PRST] = { 0, BIT(25), PERI1_UART1_S_RST },
	[PERI1_UART1_S_RST] = { 0, BIT(26), 0 },
	[PERI1_UART2_PRST] = { 0, BIT(27), PERI1_UART2_S_RST },
	[PERI1_UART2_S_RST] = { 0, BIT(28), 0 },
	[PERI1_UART3_PRST] = { 0, BIT(29), PERI1_UART3_S_RST },
	[PERI1_UART3_S_RST] = { 0, BIT(30), 0 },
	[PERI1_X2H_GMAC0_ARST] = { 0, BIT(31), 0 },
	[PERI1_X2H_GMAC0_HRST] = { 0x4, BIT(0), 0 },
	[PERI1_X2H_GMAC1_ARST] = { 0x4, BIT(1), 0 },
	[PERI1_X2H_GMAC1_HRST] = { 0x4, BIT(2), 0 },
	[PERI1_DFMU_ARESET] = { 0x4, BIT(3), 0 },
	[PERI1_DFMU_PRESET] = { 0x4, BIT(4), 0 },
	[PERI1_PAD_CTRL_PRST] = { 0x4, BIT(5), 0 },
	[PERI1_PDM0_MRST] = { 0x4, BIT(6), 0 },
	[PERI1_PDM0_PRST] = { 0x4, BIT(7), 0 },
	[PERI1_TDM0_RST] = { 0x4, BIT(8), 0 },
	[PERI1_CAN0_IPG_PE_RST] = { 0x4, BIT(9), 0 },
	[PERI1_CAN0_IPG_RST] = { 0x4, BIT(10), 0 },
	[PERI1_CAN0_IPG_SOFT_RST] = { 0x4, BIT(11), 0 },
	[PERI1_CAN0_IPG_TS_RST] = { 0x4, BIT(12), 0 },
	[PERI1_CAN0_PRST] = { 0x4, BIT(13), 0 },
	[PERI1_CAN1_IPG_PE_RST] = { 0x4, BIT(14), 0 },
	[PERI1_CAN1_IPG_RST] = { 0x4, BIT(15), 0 },
	[PERI1_CAN1_IPG_SOFT_RST] = { 0x4, BIT(16), 0 },
	[PERI1_CAN1_IPG_TS_RST] = { 0x4, BIT(17), 0 },
	[PERI1_CAN1_PRST] = { 0x4, BIT(18), 0 },
	[PERI1_CHIP_DBG_ARST] = { 0x4, BIT(19), 0 },
	[PERI1_CHIP_DBG_CRST] = { 0x4, BIT(20), 0 },
	[PERI1_CHIP_DBG_PRST] = { 0x4, BIT(21), 0 },
	[PERI1_GMAC_CRST] = { 0x4, BIT(22), 0 },
	[PERI1_X2H_GMAC2_ARST] = { 0x4, BIT(23), 0 },
	[PERI1_X2H_GMAC2_HRST] = { 0x4, BIT(24), 0 },
	[PERI1_ZGMAC_X2X_ARST] = { 0x4, BIT(25), 0 },
	[PERI1_SPI0_RST] = { 0, BIT(21) | BIT(22), 0 },
	[PERI1_I2C0_RST] = { 0, BIT(8) | BIT(9), 0 },
	[PERI1_I2C1_RST] = { 0, BIT(10) | BIT(11), 0 },
	[PERI1_I2C2_RST] = { 0, BIT(12) | BIT(13), 0 },
	[PERI1_GMAC0_RST] = { 0, BIT(0) | BIT(1) | BIT(31), 0 },
	[PERI1_GMAC0_X2H_RST] = { 0x4, BIT(0), 0 },
	[PERI1_GMAC1_RST] = { 0, BIT(2) | BIT(3), 0 },
	[PERI1_GMAC1_X2H_RST] = { 0x4, BIT(1) | BIT(2), 0 },
};

static const struct a210_rst_signal peri2_rst_signals[] = {
	[PERI2_CAN2_IPG_PE_RST] = { 0, BIT(0), 0 },
	[PERI2_CAN2_IPG_RST] = { 0, BIT(1), 0 },
	[PERI2_CAN2_IPG_SOFT_RST] = { 0, BIT(2), 0 },
	[PERI2_CAN2_IPG_TS_RST] = { 0, BIT(3), 0 },
	[PERI2_CAN2_PRST] = { 0, BIT(4), 0 },
	[PERI2_GPIO2_DBRST] = { 0, BIT(5), 0 },
	[PERI2_GPIO2_PRST] = { 0, BIT(6), 0 },
	[PERI2_I2C4_IC_RST] = { 0, BIT(7), 0 },
	[PERI2_I2C4_PRST] = { 0, BIT(8), 0 },
	[PERI2_I2S2_PRST] = { 0, BIT(9), 0 },
	[PERI2_SPI1_PRST] = { 0, BIT(10), 0 },
	[PERI2_SPI1_SSI_RST] = { 0, BIT(11), 0 },
	[PERI2_UART4_PRST] = { 0, BIT(12), PERI2_UART4_S_RST },
	[PERI2_UART4_S_RST] = { 0, BIT(13), 0 },
	[PERI2_UART5_PRST] = { 0, BIT(14), PERI2_UART5_S_RST },
	[PERI2_UART5_S_RST] = { 0, BIT(15), 0 },
	[PERI2_UART6_PRST] = { 0, BIT(16), PERI2_UART6_S_RST },
	[PERI2_UART6_S_RST] = { 0, BIT(17), 0 },
	[PERI2_PAD_CTRL_PRST] = { 0, BIT(18), 0 },
	[PERI2_GPIO3_DBRST] = { 0, BIT(19), 0 },
	[PERI2_GPIO3_PRST] = { 0, BIT(20), 0 },
	[PERI2_I2C3_IC_RST] = { 0, BIT(21), 0 },
	[PERI2_I2C3_PRST] = { 0, BIT(22), 0 },
	[PERI2_I2C5_IC_RST] = { 0, BIT(23), 0 },
	[PERI2_I2C5_PRST] = { 0, BIT(24), 0 },
	[PERI2_I2C6_IC_RST] = { 0, BIT(25), 0 },
	[PERI2_I2C6_PRST] = { 0, BIT(26), 0 },
	[PERI2_I2C7_IC_RST] = { 0, BIT(27), 0 },
	[PERI2_I2C7_PRST] = { 0, BIT(28), 0 },
	[PERI2_I2S1_PRST] = { 0, BIT(29), 0 },
	[PERI2_I2S3_PRST] = { 0, BIT(30), 0 },
	[PERI2_UART7_PRST] = { 0, BIT(31), PERI2_UART7_S_RST },
	[PERI2_UART7_S_RST] = { 0x4, BIT(0), 0 },
	[PERI2_UART8_PRST] = { 0x4, BIT(1), PERI2_UART8_S_RST },
	[PERI2_UART8_S_RST] = { 0x4, BIT(2), 0 },
	[PERI2_UART9_PRST] = { 0x4, BIT(3), PERI2_UART9_S_RST },
	[PERI2_UART9_S_RST] = { 0x4, BIT(4), 0 },
	[PERI2_QSPI1_PRST] = { 0x4, BIT(5), 0 },
	[PERI2_QSPI1_SSI_RST] = { 0x4, BIT(6), 0 },
	[PERI2_PWM1_CRST] = { 0x4, BIT(7), 0 },
	[PERI2_PWM1_PRST] = { 0x4, BIT(8), 0 },
	[PERI2_PWM2_CRST] = { 0x4, BIT(9), 0 },
	[PERI2_PWM2_PRST] = { 0x4, BIT(10), 0 },
	[PERI2_I2C3_RST] = { 0, BIT(21) | BIT(22), 0 },
	[PERI2_I2C4_RST] = { 0, BIT(7) | BIT(8), 0 },
	[PERI2_I2C5_RST] = { 0, BIT(23) | BIT(24), 0 },
	[PERI2_I2C6_RST] = { 0, BIT(25) | BIT(26), 0 },
	[PERI2_I2C7_RST] = { 0, BIT(27) | BIT(28), 0 },
	[PERI2_SPI1_RST] = { 0, BIT(10) | BIT(11), 0 },
};

static const struct a210_rst_signal peri3_rst_signals[] = {
	[PERI3_DMAC_ARST] = { 0, BIT(0), 0 },
	[PERI3_DMAC_HRST] = { 0, BIT(1), 0 },
	[PERI3_EMMC_SDIO_CLKGEN_RST] = { 0, BIT(2), 0 },
	[PERI3_EMMC_RST] = { 0, BIT(3), 0 },
	[PERI3_EMMC_X2X_ARST_M] = { 0, BIT(4), 0 },
	[PERI3_EMMC_X2X_ARST_S] = { 0, BIT(5), 0 },
	[PERI3_SDIO_RST] = { 0, BIT(6), 0 },
	[PERI3_SDIO_X2X_ARST_M] = { 0, BIT(7), 0 },
	[PERI3_SDIO_X2X_ARST_S] = { 0, BIT(8), 0 },
	[PERI3_AXI_MST_ARST] = { 0, BIT(9), 0 },
	[PERI3_AXI_MST_PRST] = { 0, BIT(10), 0 },
	[PERI3_ADC_PRST] = { 0, BIT(11), 0 },
	[PERI3_TEE_X2X_ARST_M] = { 0, BIT(12), 0 },
	[PERI3_TEE_X2X_ARST_S] = { 0, BIT(13), 0 },
	[PERI3_TEE_H2H_HRST] = { 0, BIT(14), 0 },
	[PERI3_GPIO4_DBRST] = { 0, BIT(15), 0 },
	[PERI3_GPIO4_PRST] = { 0, BIT(16), 0 },
	[PERI3_PAD_CTRL_PRST] = { 0, BIT(17), 0 },
};

static const struct a210_rst_signal pcie_rst_signals[] = {
	[PCIE_X2X_PERI_SLV_ARST] = { 0, BIT(0), 0 },
	[PCIE_X2X_PERI_MST_ARST] = { 0, BIT(4), 0 },
	[PCIE_AXI4_PCIE_MST_ARST] = { 0, BIT(8), 0 },
	[PCIE_AXI4_PCIE_MST_PRST] = { 0, BIT(12), 0 },
	[PCIE_DFMU_PRST] = { 0x4, BIT(0), 0 },
	[PCIE_DFMU_ARST] = { 0x4, BIT(4), 0 },
	[PCIE_E16PHY_PHY_RST] = { 0x8, BIT(0), 0 },
	[PCIE_E16PHY_APBS_PRST] = { 0x8, BIT(4), 0 },
	[PCIE_X2X_SATA_MST_ARST] = { 0x10, BIT(0), 0 },
	[PCIE_X2X_SATA_SLV_ARST] = { 0x10, BIT(4), 0 },
	[PCIE_SATA_ARESET] = { 0x10, BIT(8), 0 },
	[PCIE_SATA_RST_PMALIVE] = { 0x10, BIT(12), 0 },
	[PCIE_SATA_SLV_AFENCE_ARST] = { 0x10, BIT(16), 0 },
	[PCIE_SATA_MST_AFENCE_ARST] = { 0x10, BIT(20), 0 },
	[PCIE_SATA_RST_ASIC0] = { 0x14, BIT(0), 0 },
	[PCIE_SATA_RST_ASIC1] = { 0x14, BIT(4), 0 },
	[PCIE_SATA_RST_RXOOB0] = { 0x14, BIT(8), 0 },
	[PCIE_SATA_RST_RXOOB1] = { 0x14, BIT(12), 0 },
	[PCIE_DM_GEN3X4_APBS_PRST] = { 0x20, BIT(0), 0 },
	[PCIE_DM_GEN3X4_POWER_UP_RST] = { 0x20, BIT(4), 0 },
	[PCIE_DM_SLV_AFENCE_ARST] = { 0x20, BIT(16), 0 },
	[PCIE_DM_MST_AFENCE_ARST] = { 0x20, BIT(20), 0 },
	[PCIE_RP_GEN3X1_APBS_PRST] = { 0x24, BIT(0), 0 },
	[PCIE_RP_GEN3X1_POWER_UP_RST] = { 0x24, BIT(4), 0 },
	[PCIE_RP_SLV_AFENCE_ARST] = { 0x24, BIT(16), 0 },
	[PCIE_RP_MST_AFENCE_ARST] = { 0x24, BIT(20), 0 },
};

static const struct a210_rst_signal usb_rst_signals[] = {
	[USB_DFMU_PRST] = { 0, BIT(0), 0 },
	[USB_DFMU_ARST] = { 0, BIT(4), 0 },
	[USB_AXI4_MST_ARST] = { 0, BIT(8), 0 },
	[USB_AXI4_MST_PRST] = { 0, BIT(12), 0 },
	[USB_DPTX_APBS_PRST] = { 0x4, BIT(0), 0 },
	[USB_DPTX_VCC_RST] = { 0x4, BIT(4), 0 },
	[USB_USB31_APBS_PRST] = { 0x8, BIT(0), 0 },
	[USB_USB31_VCC_RST] = { 0x8, BIT(4), 0 },
	[USB_USB31_PHY_RST] = { 0x8, BIT(8), 0 },
	[USB_USB31_SLV_AFENCE_ARST] = { 0x8, BIT(12), 0 },
	[USB_C10PHY_PHY_RST] = { 0xc, BIT(0), 0 },
	[USB_C10PHY_APBS_PRST] = { 0xc, BIT(4), 0 },
	[USB_USB20_BLK_X2H_HRST] = { 0x10, BIT(0), 0 },
	[USB_USB20_BLK_X2H_ARST] = { 0x10, BIT(4), 0 },
	[USB_USB20_SLV_AFENCE_ARST] = { 0x10, BIT(8), 0 },
	[USB_USB20_BLK_H2P_HRST] = { 0x10, BIT(16), 0 },
	[USB_USB20_BLK_AHB_SLV_HRST] = { 0x10, BIT(20), 0 },
	[USB_USB20_BLK_AXI_MST_ARST] = { 0x10, BIT(24), 0 },
	[USB_USB20_BLK_USB2_SYSREG_PRST] = { 0x10, BIT(28), 0 },
	[USB_USB20_BLK_USB2_WRAP0_PRST] = { 0x14, BIT(0), 0 },
	[USB_USB20_BLK_USB2_WRAP0_HRST] = { 0x14, BIT(4), 0 },
	[USB_USB20_BLK_USB2_WRAP1_PRST] = { 0x14, BIT(8), 0 },
	[USB_USB20_BLK_USB2_WRAP1_HRST] = { 0x14, BIT(12), 0 },
	[USB_USB20_BLK_A2X0_HRST] = { 0x14, BIT(16), 0 },
	[USB_USB20_BLK_A2X0_ARST] = { 0x14, BIT(20), 0 },
	[USB_USB20_BLK_A2X1_HRST] = { 0x14, BIT(24), 0 },
	[USB_USB20_BLK_A2X1_ARST] = { 0x14, BIT(28), 0 },
	[USB_USB20_BLK_USB0_PHY_PON_RESET] = { 0x18, BIT(0), 0 },
	[USB_USB20_BLK_USB1_PHY_PON_RESET] = { 0x18, BIT(4), 0 },
	[USB_PERI2_SS_RST] = { 0x20, BIT(0), 0 },
	[USB_PERI2_SLV_AFENCE_ARST] = { 0x20, BIT(4), 0 },
};

static const struct a210_rst_signal tee_rst_signals[] = {
	[TEE_KEYRAM_PRST] = { 0, BIT(0), 0 },
	[TEE_DS_PRST] = { 0, BIT(1), 0 },
	[TEE_EFUSE_PRST] = { 0, BIT(2), 0 },
	[TEE_OCRAM_HRST] = { 0, BIT(3), 0 },
	[TEE_SYSREG_PRST] = { 0, BIT(4), 0 },
	[TEE_CCU_PRST] = { 0, BIT(5), 0 },
	[TEE_CCU_CRST] = { 0, BIT(6), 0 },
	[TEE_EIP150B_HRST] = { 0, BIT(8), 0 },
	[TEE_EIP120SIII_HRST] = { 0, BIT(9), 0 },
	[TEE_EIP120SIII_ARST] = { 0, BIT(10), 0 },
	[TEE_EIP120SII_HRST] = { 0, BIT(11), 0 },
	[TEE_EIP120SII_ARST] = { 0, BIT(12), 0 },
	[TEE_EIP120SI_HRST] = { 0, BIT(13), 0 },
	[TEE_EIP120SI_ARST] = { 0, BIT(14), 0 },
	[TEE_DMAC_HRST] = { 0, BIT(15), 0 },
	[TEE_DMAC_ARST] = { 0, BIT(16), 0 },
	[TEE_X2P_TEESYS_PRST] = { 0, BIT(24), 0 },
	[TEE_X2P_TEESYS_ARST] = { 0, BIT(25), 0 },
	[TEE_AXI4_TEESYS_ARST] = { 0, BIT(26), 0 },
	[TEE_APB3_TEESYS_PRST] = { 0, BIT(27), 0 },
	[TEE_APB3_TEESYS_HRST] = { 0, BIT(28), 0 },
	[TEE_AHB2_TEESYS_HRST] = { 0, BIT(29), 0 },
};

static const struct a210_rst_signal gpu_rst_signals[] = {
	[GPU_PWR_WRAP_RGX_HOOD_RST] = { 0, BIT(0), 0 },
	[GPU_PWR_WRAP_DFMU_RST] = { 0, BIT(1), 0 },
};

static inline struct a210_reset *to_a210_reset(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct a210_reset, rcdev);
}

static int a210_reset_update(struct reset_controller_dev *rcdev,
			     unsigned long id, bool assert)
{
	struct a210_reset *rst = to_a210_reset(rcdev);
	const struct a210_rst_signal *signal;
	unsigned long flags;
	u32 value;
	int ret = 0;

	if (id >= rst->data->num_signals)
		return -EINVAL;

	spin_lock_irqsave(&rst->lock, flags);
	do {
		signal = &rst->data->signals[id];
		value = readl(rst->base + signal->offset);
		if (assert)
			value &= ~signal->mask;
		else
			value |= signal->mask;
		writel(value, rst->base + signal->offset);
		/* Firmware can lock individual reset controls against AP writes. */
		if ((readl(rst->base + signal->offset) & signal->mask) !=
		    (value & signal->mask)) {
			ret = -EIO;
			break;
		}
		id = signal->nextid;
	} while (id);
	spin_unlock_irqrestore(&rst->lock, flags);

	return ret;
}

static int a210_reset_assert(struct reset_controller_dev *rcdev, unsigned long id)
{
	return a210_reset_update(rcdev, id, true);
}

static int a210_reset_deassert(struct reset_controller_dev *rcdev, unsigned long id)
{
	return a210_reset_update(rcdev, id, false);
}

static int a210_reset_status(struct reset_controller_dev *rcdev, unsigned long id)
{
	struct a210_reset *rst = to_a210_reset(rcdev);
	const struct a210_rst_signal *signal;
	unsigned long flags;
	bool asserted;

	if (id >= rst->data->num_signals)
		return -EINVAL;

	/* The logical reset is asserted if any linked signal is asserted. */
	spin_lock_irqsave(&rst->lock, flags);
	do {
		signal = &rst->data->signals[id];
		asserted = (readl(rst->base + signal->offset) & signal->mask) !=
			   signal->mask;
		if (asserted)
			break;
		id = signal->nextid;
	} while (id);
	spin_unlock_irqrestore(&rst->lock, flags);

	return asserted;
}

static const struct reset_control_ops a210_reset_ops = {
	.assert = a210_reset_assert,
	.deassert = a210_reset_deassert,
	.status = a210_reset_status,
};

static int a210_reset_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct a210_reset *rst;

	rst = devm_kzalloc(dev, sizeof(*rst), GFP_KERNEL);
	if (!rst)
		return -ENOMEM;

	rst->data = device_get_match_data(dev);
	if (!rst->data)
		return -EINVAL;

	rst->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(rst->base))
		return PTR_ERR(rst->base);

	spin_lock_init(&rst->lock);
	rst->rcdev.ops = &a210_reset_ops;
	rst->rcdev.owner = THIS_MODULE;
	rst->rcdev.dev = dev;
	rst->rcdev.of_node = dev->of_node;
	rst->rcdev.nr_resets = rst->data->num_signals;

	return devm_reset_controller_register(dev, &rst->rcdev);
}

static const struct a210_reset_data vp_reset_data = {
	.signals = vp_rst_signals,
	.num_signals = ARRAY_SIZE(vp_rst_signals),
};

static const struct a210_reset_data vi_reset_data = {
	.signals = vi_rst_signals,
	.num_signals = ARRAY_SIZE(vi_rst_signals),
};

static const struct a210_reset_data npu_reset_data = {
	.signals = npu_rst_signals,
	.num_signals = ARRAY_SIZE(npu_rst_signals),
};

static const struct a210_reset_data vo_reset_data = {
	.signals = vo_rst_signals,
	.num_signals = ARRAY_SIZE(vo_rst_signals),
};

static const struct a210_reset_data peri0_reset_data = {
	.signals = peri0_rst_signals,
	.num_signals = ARRAY_SIZE(peri0_rst_signals),
};

static const struct a210_reset_data peri1_reset_data = {
	.signals = peri1_rst_signals,
	.num_signals = ARRAY_SIZE(peri1_rst_signals),
};

static const struct a210_reset_data peri2_reset_data = {
	.signals = peri2_rst_signals,
	.num_signals = ARRAY_SIZE(peri2_rst_signals),
};

static const struct a210_reset_data peri3_reset_data = {
	.signals = peri3_rst_signals,
	.num_signals = ARRAY_SIZE(peri3_rst_signals),
};

static const struct a210_reset_data pcie_reset_data = {
	.signals = pcie_rst_signals,
	.num_signals = ARRAY_SIZE(pcie_rst_signals),
};

static const struct a210_reset_data usb_reset_data = {
	.signals = usb_rst_signals,
	.num_signals = ARRAY_SIZE(usb_rst_signals),
};

static const struct a210_reset_data tee_reset_data = {
	.signals = tee_rst_signals,
	.num_signals = ARRAY_SIZE(tee_rst_signals),
};

static const struct a210_reset_data gpu_reset_data = {
	.signals = gpu_rst_signals,
	.num_signals = ARRAY_SIZE(gpu_rst_signals),
};

static const struct of_device_id a210_reset_of_match[] = {
	{ .compatible = "zhihe,a210-vp-reset", .data = &vp_reset_data },
	{ .compatible = "zhihe,a210-vi-reset", .data = &vi_reset_data },
	{ .compatible = "zhihe,a210-npu-reset", .data = &npu_reset_data },
	{ .compatible = "zhihe,a210-vo-reset", .data = &vo_reset_data },
	{ .compatible = "zhihe,a210-peri0-reset", .data = &peri0_reset_data },
	{ .compatible = "zhihe,a210-peri1-reset", .data = &peri1_reset_data },
	{ .compatible = "zhihe,a210-peri2-reset", .data = &peri2_reset_data },
	{ .compatible = "zhihe,a210-peri3-reset", .data = &peri3_reset_data },
	{ .compatible = "zhihe,a210-pcie-reset", .data = &pcie_reset_data },
	{ .compatible = "zhihe,a210-usb-reset", .data = &usb_reset_data },
	{ .compatible = "zhihe,a210-tee-reset", .data = &tee_reset_data },
	{ .compatible = "zhihe,a210-gpu-reset", .data = &gpu_reset_data },
	{}
};
MODULE_DEVICE_TABLE(of, a210_reset_of_match);

static struct platform_driver a210_reset_driver = {
	.probe = a210_reset_probe,
	.driver = {
		.name = "a210-reset",
		.of_match_table = a210_reset_of_match,
	},
};
module_platform_driver(a210_reset_driver);

MODULE_AUTHOR("Zhiguo Zhu <zhiguo.zhu@linux.alibaba.com>");
MODULE_DESCRIPTION("Zhihe A210 reset controller driver");
MODULE_LICENSE("GPL");
