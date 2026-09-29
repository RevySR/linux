// SPDX-License-Identifier: GPL-2.0-only
/*
 * TH1520 VC8000D register submission, post-processing and interrupts.
 *
 * Copyright (C) 2026 th1520-v4l2 contributors
 *
 * Each job prepares a complete shadow register block. Configuration
 * registers are submitted before the control register that starts decoding.
 * The read-only ASIC identifier is preserved.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/string.h>
#include <media/videobuf2-dma-contig.h>

#include "hantro_vc8000.h"

static void hantro_vc8000_regs_reset(struct hantro_ctx *ctx)
{
	memset(ctx->vc8000_dec->regs, 0, sizeof(ctx->vc8000_dec->regs));
}

void hantro_vc8000_reg_write(struct hantro_ctx *ctx,
			     const struct hantro_reg *reg, u32 val)
{
	u32 *slot;

	if (WARN_ON(reg->base / 4 >= VC8000_VDEC_REG_COUNT))
		return;

	slot = &ctx->vc8000_dec->regs[reg->base / 4];
	*slot &= ~(reg->mask << reg->shift);
	*slot |= (val & reg->mask) << reg->shift;
}

void hantro_vc8000_reg_write_raw(struct hantro_ctx *ctx, u16 swreg, u32 val)
{
	if (WARN_ON(swreg >= VC8000_VDEC_REG_COUNT))
		return;

	ctx->vc8000_dec->regs[swreg] = val;
}

void hantro_vc8000_write_addr(struct hantro_ctx *ctx, u16 lsb_swreg,
			      u16 msb_swreg, dma_addr_t addr)
{
	hantro_vc8000_reg_write_raw(ctx, lsb_swreg, lower_32_bits(addr));
	hantro_vc8000_reg_write_raw(ctx, msb_swreg, upper_32_bits(addr));
}

void hantro_vc8000_write_addr_pair(struct hantro_ctx *ctx, u16 msb_swreg,
				   dma_addr_t addr)
{
	hantro_vc8000_write_addr(ctx, msb_swreg + 1, msb_swreg, addr);
}

/*
 * Both codecs use a 128-bit AXI interface and a burst length of 16.
 * The validated field encodings reside in register 58.
 */
#define VC8000_MAX_BURST		16

/*
 * Both hardware timeout counters use 5242880 cycles with their override
 * bits enabled. A separate software watchdog bounds request completion.
 */
#define VC8000_TIMEOUT_OVERRIDE		0x80500000U

static void hantro_vc8000_common_config_h264(struct hantro_ctx *ctx)
{
	/*
	 * H.264 uses table byte-order value 3 and tiled native references.
	 * The remaining byte-order fields are zero; register clock gating is on.
	 */
	hantro_vc8000_reg_write_raw(ctx, 2, 0x00000400);
	hantro_vc8000_reg_write(ctx, &h264_table_byte_order, 3);
	/*
	 * Enable the native tiled reference layout.
	 */
	hantro_vc8000_reg_write_raw(ctx, 2, ctx->vc8000_dec->regs[2] | BIT(7));

	/*
	 * Baseline selects mode 0; the other supported profiles select mode 15.
	 */
	hantro_vc8000_reg_write(ctx, &vc8000_dec_mode,
				ctx->vc8000_dec->mode15 ?
					VC8000_DEC_MODE_H264_HIGH10 : VC8000_DEC_MODE_H264);
	hantro_vc8000_reg_write(ctx, &hevc_out_ec_bypass, ctx->vc8000_dec->mode15);

	hantro_vc8000_reg_write(ctx, &h264_rlc_mode_e, 0);

	hantro_vc8000_reg_write(ctx, &h264_dec_out_dis, 0);
	hantro_vc8000_reg_write(ctx, &h264_filtering_dis, 0);
	hantro_vc8000_reg_write(ctx, &h264_mvc_e, 0);
	/*
	 * Configure complete-frame VLC input and the input-exhausted interrupt.
	 */
	hantro_vc8000_reg_write(ctx, &h264_input_exhausted_irq_enable, 1);
	hantro_vc8000_reg_write(ctx, &h264_input_block_mode, 0);

	/*
	 * Register 58 configures the AXI burst length, bus width and ID controls.
	 */
	hantro_vc8000_reg_write(ctx, &h264_max_burst_sw58, VC8000_MAX_BURST);
	hantro_vc8000_reg_write(ctx, &h264_buswidth_sw58, VC8000_BUS_WIDTH_128);
	hantro_vc8000_reg_write(ctx, &h264_axi_rd_id_e_sw58, 1);
	hantro_vc8000_reg_write(ctx, &h264_axi_wd_id_e_sw58, 0);

	hantro_vc8000_reg_write_raw(ctx, VC8000_H264_SWREG_ERR_CONC, 0);

	hantro_vc8000_reg_write(ctx, &h264_pred_bc_tap_0_0, 1);
	hantro_vc8000_reg_write(ctx, &h264_pred_bc_tap_0_1, (u32)(-5) & 0x3ff);
	hantro_vc8000_reg_write(ctx, &h264_pred_bc_tap_0_2, 20);

	hantro_vc8000_reg_write(ctx, &h264_apf_threshold, 8);

	hantro_vc8000_reg_write(ctx, &h264_error_control_bit31, 0);

	hantro_vc8000_reg_write_raw(ctx, 318, VC8000_TIMEOUT_OVERRIDE);
	hantro_vc8000_reg_write_raw(ctx, 319, VC8000_TIMEOUT_OVERRIDE);

	hantro_vc8000_reg_write(ctx, &vc8000_dec_irq_dis, 0);
}

static void hantro_vc8000_common_config_hevc(struct hantro_ctx *ctx)
{
	/*
	 * HEVC uses zero byte-order fields and enables register clock gating.
	 */
	hantro_vc8000_reg_write_raw(ctx, 2, 0x00000400);

	hantro_vc8000_reg_write(ctx, &vc8000_dec_mode, VC8000_DEC_MODE_HEVC);
	/*
	 * Bypass native-reference compression. The decoder writes tiled native
	 * images; the post-processor converts them to linear CAPTURE output.
	 */
	hantro_vc8000_reg_write(ctx, &hevc_out_ec_bypass, 1);
	/*
	 * Keep reference reads enabled and leave unused control bits clear.
	 */
	hantro_vc8000_reg_write(ctx, &hevc_control_bit7, 0);
	hantro_vc8000_reg_write(ctx, &hevc_skip_reference_reads, 0);
	hantro_vc8000_reg_write(ctx, &hevc_control_bit5, 0);
	/*
	 * Report exhausted input through an interrupt for each complete-frame job.
	 */
	hantro_vc8000_reg_write(ctx, &hevc_input_exhausted_irq_enable, 1);
	hantro_vc8000_reg_write(ctx, &hevc_input_block_mode, 0);
	hantro_vc8000_reg_write(ctx, &hevc_final_input_buffer, 0);

	hantro_vc8000_reg_write(ctx, &hevc_out_dis, 0);

	/*
	 * Configure the validated 128-bit AXI bus, burst length and ID controls.
	 */
	hantro_vc8000_reg_write(ctx, &hevc_refer_doublebuffer_e, 0);
	hantro_vc8000_reg_write(ctx, &hevc_axi_rd_id_e, 1);
	hantro_vc8000_reg_write(ctx, &hevc_axi_wd_id_e, 0);
	hantro_vc8000_reg_write(ctx, &hevc_buswidth, VC8000_BUS_WIDTH_128);
	hantro_vc8000_reg_write(ctx, &hevc_max_burst, VC8000_MAX_BURST);

	/*
	 * Use zero AXI read and write identifiers.
	 */
	hantro_vc8000_reg_write(ctx, &hevc_axi_rd_id, 0);
	hantro_vc8000_reg_write(ctx, &hevc_axi_wr_id, 0);

	hantro_vc8000_reg_write(ctx, &hevc_apf_disable, 0);
	hantro_vc8000_reg_write(ctx, &hevc_apf_threshold, 8);

	hantro_vc8000_reg_write(ctx, &hevc_down_scale_e, 0);

	hantro_vc8000_reg_write_raw(ctx, 318, VC8000_TIMEOUT_OVERRIDE);
	hantro_vc8000_reg_write_raw(ctx, 319, VC8000_TIMEOUT_OVERRIDE);

	/* Register 265 fields 27:18 and 17:8 use the validated value 64.
	 * Keep master bit 31 clear while using uncompressed native buffers.
	 */
	hantro_vc8000_reg_write_raw(ctx, 265, 0x01004000);

	/* Each request submits a complete frame; intermediate tile IRQs stay off. */
	hantro_vc8000_reg_write(ctx, &vc8000_dec_tile_int_e, 0);

	hantro_vc8000_reg_write(ctx, &vc8000_dec_irq_dis, 0);
}

void hantro_vc8000_set_common_config(struct hantro_ctx *ctx)
{
	hantro_vc8000_regs_reset(ctx);
	switch (ctx->vpu_src_fmt->codec_mode) {
	case HANTRO_MODE_H264_DEC:
		hantro_vc8000_common_config_h264(ctx);
		break;
	case HANTRO_MODE_HEVC_DEC:
	case HANTRO_MODE_VP9_DEC:
		hantro_vc8000_common_config_hevc(ctx);
		/* SDK Vp9AsicInit uses the same SetCommonConfigRegs branch. */
		if (ctx->vpu_src_fmt->codec_mode == HANTRO_MODE_VP9_DEC)
			hantro_vc8000_reg_write(ctx, &vc8000_dec_mode,
						VC8000_DEC_MODE_VP9);
		break;
	default:
		break;
	}
}

/* The PP converts the native tiled decode stream to linear NV12. */
void hantro_vc8000_set_postproc(struct hantro_ctx *ctx, u32 width, u32 height)
{
	struct vb2_v4l2_buffer *dst = hantro_get_dst_buf(ctx);
	dma_addr_t dma = vb2_dma_contig_plane_dma_addr(&dst->vb2_buf, 0);
	u32 stride = ctx->dst_fmt.plane_fmt[0].bytesperline;
	u32 chroma_offset = stride * ctx->dst_fmt.height;

	hantro_vc8000_reg_write_raw(ctx, 320, 1);
	hantro_vc8000_reg_write_raw(ctx, 322, 1U << 27);
	hantro_vc8000_write_addr_pair(ctx, 325, dma);
	hantro_vc8000_write_addr_pair(ctx, 327, dma + chroma_offset);
	hantro_vc8000_reg_write_raw(ctx, 329, (stride << 16) | stride);
	hantro_vc8000_reg_write_raw(ctx, 331, ((width / 2) << 16) | (height / 2));
	hantro_vc8000_reg_write_raw(ctx, 332, (width << 16) | height);
	hantro_vc8000_reg_write_raw(ctx, 394, 0x01010000);
}

/*
 * Submit registers 3 through 511, then register 2, and finally register 1.
 * The decoder start bit is written last. Register 0 is read-only.
 */

void hantro_vc8000_start(struct hantro_ctx *ctx)
{
	struct hantro_dev *vpu = ctx->dev;
	unsigned int i;

	hantro_vc8000_reg_write(ctx, &vc8000_dec_e, 1);

	for (i = 3; i < VC8000_VDEC_REG_COUNT; i++)
		vdpu_write(vpu, ctx->vc8000_dec->regs[i], VC8000_VDEC_REG_OFF(i));

	vdpu_write(vpu, ctx->vc8000_dec->regs[2], VC8000_VDEC_REG_OFF(2));

	/* Publish DMA data and configuration before the ordered start write. */
	wmb();
	vdpu_write(vpu, ctx->vc8000_dec->regs[VC8000_VDEC_SWREG_IRQ],
		   VC8000_VDEC_REG_OFF(VC8000_VDEC_SWREG_IRQ));
}

/*
 * hantro_vc8000_hw_reset - abort a running job and clear decoder state
 *
 * Request abort with interrupts disabled, wait for the decoder enable bit
 * to clear, then clear the configuration block before the next request.
 */
void hantro_vc8000_hw_reset(struct hantro_ctx *ctx)
{
	struct hantro_dev *vpu = ctx->dev;
	unsigned int i;
	u32 status;
	int ret;

	status = vdpu_read(vpu, VC8000_VDEC_REG_OFF(VC8000_VDEC_SWREG_IRQ));
	if (status & VC8000_IRQ_DEC_E) {
		dev_warn(vpu->dev, "device still running, aborting\n");
		vdpu_write(vpu,
			   VC8000_IRQ_DEC_ABORT_E | VC8000_IRQ_DEC_IRQ_DIS,
			   VC8000_VDEC_REG_OFF(VC8000_VDEC_SWREG_IRQ));

		ret = readl_poll_timeout(vpu->dec_base +
					 VC8000_VDEC_REG_OFF(VC8000_VDEC_SWREG_IRQ),
					 status, !(status & VC8000_IRQ_DEC_E),
					 1000, 100000);
		if (ret) {
			ret = reset_control_assert(vpu->resets);
			if (!ret)
				ret = reset_control_deassert(vpu->resets);
			if (ret) {
				dev_err(vpu->dev, "decoder reset failed: %d\n", ret);
				return;
			}
		}
	}

	for (i = VC8000_VDEC_SWREG_IRQ; i < VC8000_VDEC_REG_COUNT; i++)
		vdpu_write(vpu, 0, VC8000_VDEC_REG_OFF(i));

	hantro_vc8000_regs_reset(ctx);
}

irqreturn_t hantro_vc8000_irq(int irq, void *data)
{
	struct hantro_dev *vpu = data;
	struct hantro_ctx *ctx;
	enum vb2_buffer_state state;
	u32 status = vdpu_read(vpu, VC8000_VDEC_REG_OFF(VC8000_VDEC_SWREG_IRQ));

	if (!(status & VC8000_IRQ_DEC_IRQ))
		return IRQ_NONE;
	vdpu_write(vpu, status & ~(VC8000_IRQ_STAT_MASK | VC8000_IRQ_DEC_IRQ |
				  VC8000_IRQ_DEC_E),
		   VC8000_VDEC_REG_OFF(VC8000_VDEC_SWREG_IRQ));
	ctx = v4l2_m2m_get_curr_priv(vpu->m2m_dev);
	if (!ctx)
		return IRQ_HANDLED;
	state = (status & VC8000_IRQ_DEC_RDY_INT) &&
		!(status & (VC8000_IRQ_ERROR_MASK | VC8000_IRQ_DEC_ASO_INT |
			    VC8000_IRQ_DEC_ABORT_INT |
			    VC8000_IRQ_DEC_STRM_CORRUPTED)) ?
		VB2_BUF_STATE_DONE : VB2_BUF_STATE_ERROR;
	hantro_irq_done(vpu, state);
	return IRQ_HANDLED;
}
