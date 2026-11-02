// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */

#include "hantro_vc8000.h"

#define VC9000D_ASIC_ID		0x9001
#define VC9000D_BUILD_ID	0x80002014
#define VC9000D_BUILD_REG	(309 * 4)

static const struct hantro_fmt a210_vdec_fmts[] = {
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
	},
};

static int a210_vdec_runtime_resume(struct hantro_dev *vpu)
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
	build = vdpu_read(vpu, VC9000D_BUILD_REG);
	if (asic >> 16 != VC9000D_ASIC_ID || build != VC9000D_BUILD_ID) {
		dev_err(vpu->dev, "unsupported decoder: ASIC %#x, build %#x\n",
			asic, build);
		ret = -ENODEV;
	}
out:
	clk_bulk_disable(vpu->variant->num_clocks, vpu->clocks);
	return ret;
}

static const struct hantro_codec_ops a210_vdec_codec_ops[] = {
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
};

static const struct hantro_irq a210_vdec_irqs[] = {
	{ NULL, hantro_vc8000_irq },
};

static const char * const a210_vdec_clk_names[] = { "aclk", "cclk", "pclk" };

const struct hantro_variant a210_vdec_variant = {
	.dec_fmts = a210_vdec_fmts,
	.num_dec_fmts = ARRAY_SIZE(a210_vdec_fmts),
	.postproc_ops = &hantro_vc8000_postproc_ops,
	.codec = HANTRO_H264_DECODER | HANTRO_HEVC_DECODER,
	.codec_ops = a210_vdec_codec_ops,
	.runtime_resume = a210_vdec_runtime_resume,
	.irqs = a210_vdec_irqs,
	.num_irqs = ARRAY_SIZE(a210_vdec_irqs),
	.clk_names = a210_vdec_clk_names,
	.num_clocks = ARRAY_SIZE(a210_vdec_clk_names),
	.late_postproc = true,
	.vc9000d = true,
};
