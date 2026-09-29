// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */

#include <linux/slab.h>

#include "hantro_vc8000.h"

#define VC8000_ASIC_ID		0x8001
#define VC8000_BUILD_ID		0x1f88
#define VC8000_BUILD_REG		(309 * 4)
#define VC8000_MMU_CONTROL	(226 * 4)
#define VC8000_MMU_ENABLE	BIT(0)

#define VC8000_FRAMESIZE \
	{ .min_width = VC8000_MIN_WIDTH, .max_width = VC8000_MAX_WIDTH, \
	  .step_width = MB_DIM, .min_height = VC8000_MIN_HEIGHT, \
	  .max_height = VC8000_MAX_HEIGHT, .step_height = MB_DIM }

static const struct hantro_fmt th1520_vdec_fmts[] = {
	{
		.fourcc = V4L2_PIX_FMT_NV12,
		.codec_mode = HANTRO_MODE_NONE,
		.postprocessed = true,
		.match_depth = true,
		.frmsize = VC8000_FRAMESIZE,
	}, {
		.fourcc = V4L2_PIX_FMT_H264_SLICE,
		.codec_mode = HANTRO_MODE_H264_DEC,
		.max_depth = 2,
		.frmsize = VC8000_FRAMESIZE,
	}, {
		.fourcc = V4L2_PIX_FMT_HEVC_SLICE,
		.codec_mode = HANTRO_MODE_HEVC_DEC,
		.max_depth = 2,
		.frmsize = VC8000_FRAMESIZE,
	}, {
		.fourcc = V4L2_PIX_FMT_VP9_FRAME,
		.codec_mode = HANTRO_MODE_VP9_DEC,
		.max_depth = 2,
		.frmsize = VC8000_FRAMESIZE,
	},
};

static unsigned int vc8000_buffer_size(struct hantro_ctx *ctx)
{
	switch (ctx->vpu_src_fmt->codec_mode) {
	case HANTRO_MODE_H264_DEC:
		return hantro_vc8000_h264_native_size(ctx);
	case HANTRO_MODE_HEVC_DEC:
		return hantro_vc8000_hevc_native_size(ctx);
	case HANTRO_MODE_VP9_DEC:
		return hantro_vc8000_vp9_native_size(ctx);
	default:
		return 0;
	}
}

static int vc8000_enum_framesizes(struct hantro_ctx *ctx,
				  struct v4l2_frmsizeenum *fsize)
{
	if (fsize->index)
		return -EINVAL;
	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	fsize->stepwise = (struct v4l2_frmsize_stepwise)VC8000_FRAMESIZE;
	return 0;
}

static const struct hantro_postproc_ops vc8000_postproc_ops = {
	.buffer_size = vc8000_buffer_size,
	.cpu_access = true,
	.enum_framesizes = vc8000_enum_framesizes,
};

int hantro_vc8000_ctx_init(struct hantro_ctx *ctx)
{
	ctx->vc8000_dec = kzalloc_obj(*ctx->vc8000_dec, GFP_KERNEL);
	return ctx->vc8000_dec ? 0 : -ENOMEM;
}

void hantro_vc8000_ctx_exit(struct hantro_ctx *ctx)
{
	kfree(ctx->vc8000_dec);
	ctx->vc8000_dec = NULL;
}

static int th1520_vdec_runtime_resume(struct hantro_dev *vpu)
{
	u32 asic, build;
	int ret;

	ret = clk_bulk_enable(vpu->variant->num_clocks, vpu->clocks);
	if (ret)
		return ret;
	ret = reset_control_deassert(vpu->resets);
	if (ret)
		goto out;

	asic = vdpu_read(vpu, 0);
	build = vdpu_read(vpu, VC8000_BUILD_REG);
	if (asic >> 16 != VC8000_ASIC_ID || build != VC8000_BUILD_ID) {
		dev_err(vpu->dev, "unsupported decoder: ASIC %#x, build %#x\n",
			asic, build);
		ret = -ENODEV;
		goto out;
	}

	/* Restore physical DMA addressing after the decoder domain powers on. */
	writel(readl(vpu->reg_bases[1] + VC8000_MMU_CONTROL) &
	       ~VC8000_MMU_ENABLE, vpu->reg_bases[1] + VC8000_MMU_CONTROL);
out:
	clk_bulk_disable(vpu->variant->num_clocks, vpu->clocks);
	return ret;
}

static const struct hantro_codec_ops th1520_vdec_codec_ops[] = {
	[HANTRO_MODE_H264_DEC] = {
		.init = vc8000_h264_init,
		.exit = vc8000_h264_exit,
		.run = vc8000_h264_run,
		.reset = hantro_vc8000_hw_reset,
	},
	[HANTRO_MODE_HEVC_DEC] = {
		.init = vc8000_hevc_init,
		.exit = vc8000_hevc_exit,
		.run = vc8000_hevc_run,
		.reset = hantro_vc8000_hw_reset,
	},
	[HANTRO_MODE_VP9_DEC] = {
		.init = vc8000_vp9_init,
		.exit = vc8000_vp9_exit,
		.run = vc8000_vp9_run,
		.check_result = vc8000_vp9_check_result,
		.done = vc8000_vp9_done,
		.abort = vc8000_vp9_abort,
		.reset = hantro_vc8000_hw_reset,
	},
};

static const struct hantro_irq th1520_vdec_irqs[] = {
	{ NULL, hantro_vc8000_irq },
};

static const char * const th1520_vdec_clk_names[] = { "aclk", "cclk", "pclk" };
static const char * const th1520_vdec_reg_names[] = { "decoder", "mmu" };

const struct hantro_variant th1520_vdec_variant = {
	.dec_fmts = th1520_vdec_fmts,
	.num_dec_fmts = ARRAY_SIZE(th1520_vdec_fmts),
	.postproc_ops = &vc8000_postproc_ops,
	.codec = HANTRO_H264_DECODER | HANTRO_HEVC_DECODER | HANTRO_VP9_DECODER,
	.codec_ops = th1520_vdec_codec_ops,
	.runtime_resume = th1520_vdec_runtime_resume,
	.irqs = th1520_vdec_irqs,
	.num_irqs = ARRAY_SIZE(th1520_vdec_irqs),
	.clk_names = th1520_vdec_clk_names,
	.num_clocks = ARRAY_SIZE(th1520_vdec_clk_names),
	.reg_names = th1520_vdec_reg_names,
	.num_regs = ARRAY_SIZE(th1520_vdec_reg_names),
	.late_postproc = true,
};
