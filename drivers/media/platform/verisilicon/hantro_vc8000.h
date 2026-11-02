/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 th1520-v4l2 contributors */
#ifndef HANTRO_VC8000_H_
#define HANTRO_VC8000_H_

#include <linux/sizes.h>

#include "hantro.h"
#include "hantro_vc8000_regs.h"

#define VC8000_MIN_WIDTH 48
#define VC8000_MIN_HEIGHT 48
#define VC8000_MAX_WIDTH 4096
#define VC8000_MAX_HEIGHT 2304

#define VC8000_FRAMESIZE \
	{ .min_width = VC8000_MIN_WIDTH, .max_width = VC8000_MAX_WIDTH, \
	  .step_width = MB_DIM, .min_height = VC8000_MIN_HEIGHT, \
	  .max_height = VC8000_MAX_HEIGHT, .step_height = MB_DIM }

struct hantro_vc8000_hevc_ctx {
	struct hantro_aux_buf tile_sizes;
	struct hantro_aux_buf scaling_lists;
	struct hantro_aux_buf tile_filter;
	struct hantro_aux_buf tile_sao;
	struct hantro_aux_buf tile_bsd;
	struct hantro_hevc_dec_ctrls ctrls;
	unsigned int num_tile_cols_allocated;
};

struct vc8000_vp9_ctx;
struct hantro_vc8000_ctx {
	u32 regs[VC8000_VDEC_REG_COUNT];
	bool mode15;
	struct hantro_vc8000_hevc_ctx hevc;
	struct vc8000_vp9_ctx *vp9;
};

/*
 * With tile columns, the VC9000D post-processor needs a column buffer of
 * 36 bytes per 16-aligned row and bit of depth, followed by two scaler
 * areas, as sized by PPGetLancozsColumnBufferSize() in the SDK.
 */
#define VC9000D_PP_SCALER0_SIZE		SZ_1M
#define VC9000D_PP_SCALER1_SIZE		(SZ_512K + SZ_256K)

static inline bool hantro_vc8000_is_vc9000d(const struct hantro_ctx *ctx)
{
	return ctx->dev->variant->vc9000d;
}

/*
 * The native tiled stride is the number of bytes in four luma or chroma
 * rows. VC9000D takes it divided by four for luma and by eight for chroma.
 */
static inline u32 hantro_vc8000_luma_stride(const struct hantro_ctx *ctx,
					    u32 stride)
{
	return hantro_vc8000_is_vc9000d(ctx) ? stride / 4 : stride;
}

static inline u32 hantro_vc8000_chroma_stride(const struct hantro_ctx *ctx,
					      u32 stride)
{
	return hantro_vc8000_is_vc9000d(ctx) ? stride / 8 : stride;
}

static inline size_t hantro_vc9000d_pp_column_size(u32 height, u32 depth)
{
	return 36 * ALIGN(height, 16) * depth;
}

static inline size_t hantro_vc9000d_pp_columns_size(u32 height, u32 depth)
{
	return hantro_vc9000d_pp_column_size(height, depth) +
	       VC9000D_PP_SCALER0_SIZE + VC9000D_PP_SCALER1_SIZE;
}

static inline struct hantro_decoded_buffer *
hantro_vc8000_buf(struct vb2_v4l2_buffer *vb)
{
	return container_of(vb, struct hantro_decoded_buffer, base.vb);
}

static inline struct hantro_aux_buf *
hantro_vc8000_native(struct hantro_ctx *ctx, struct vb2_buffer *vb)
{
	return &ctx->postproc.dec_q[vb->index];
}

static inline size_t
hantro_vc8000_hevc_native_chroma_offset(const struct hantro_ctx *ctx)
{
	size_t stride = ALIGN(ctx->src_fmt.width * 4, 64);

	return stride * ALIGN(ctx->src_fmt.height, MB_DIM) / 4;
}

static inline size_t
hantro_vc8000_hevc_native_mv_offset(const struct hantro_ctx *ctx)
{
	size_t luma_size = hantro_vc8000_hevc_native_chroma_offset(ctx);

	return luma_size + ALIGN(luma_size / 2, 64) + 64;
}

static inline size_t
hantro_vc8000_hevc_native_size(const struct hantro_ctx *ctx)
{
	size_t mv_size = DIV_ROUND_UP(ctx->src_fmt.width, 64) *
			 DIV_ROUND_UP(ctx->src_fmt.height, 64) * 256;

	return hantro_vc8000_hevc_native_mv_offset(ctx) + mv_size;
}

/* Mode 15 stores 80 bytes of motion vectors per macroblock. */
static inline size_t
hantro_vc8000_h264_native_chroma_offset(const struct hantro_ctx *ctx)
{
	return hantro_vc8000_hevc_native_chroma_offset(ctx);
}

static inline size_t
hantro_vc8000_h264_native_mv_offset(const struct hantro_ctx *ctx)
{
	if (ctx->vc8000_dec->mode15)
		return hantro_vc8000_hevc_native_mv_offset(ctx);
	return hantro_vc8000_h264_native_chroma_offset(ctx) * 3 / 2;
}

static inline size_t
hantro_vc8000_h264_native_size(const struct hantro_ctx *ctx)
{
	size_t mbs = MB_WIDTH(ctx->src_fmt.width) *
		     MB_HEIGHT(ctx->src_fmt.height);

	/* Allocate before per-request SPS determines mode0/mode15. */
	return hantro_vc8000_hevc_native_mv_offset(ctx) + ALIGN(80 * ALIGN(mbs, 4), 64);
}

extern const struct hantro_postproc_ops hantro_vc8000_postproc_ops;

int hantro_vc8000_ctx_init(struct hantro_ctx *ctx);
void hantro_vc8000_ctx_exit(struct hantro_ctx *ctx);
void hantro_vc8000_reg_write(struct hantro_ctx *ctx,
			     const struct hantro_reg *reg, u32 val);
void hantro_vc8000_reg_write_raw(struct hantro_ctx *ctx, u16 swreg, u32 val);
void hantro_vc8000_write_addr(struct hantro_ctx *ctx, u16 lsb, u16 msb, dma_addr_t addr);
void hantro_vc8000_write_addr_pair(struct hantro_ctx *ctx, u16 msb, dma_addr_t addr);
void hantro_vc8000_set_common_config(struct hantro_ctx *ctx);
void hantro_vc8000_set_postproc(struct hantro_ctx *ctx, u32 width, u32 height);
void hantro_vc9000d_set_tile_edge(struct hantro_ctx *ctx, dma_addr_t addr);
void hantro_vc9000d_set_pp_columns(struct hantro_ctx *ctx, dma_addr_t addr,
				   u32 height, u32 depth);
void hantro_vc8000_start(struct hantro_ctx *ctx);
void hantro_vc8000_hw_reset(struct hantro_ctx *ctx);
irqreturn_t hantro_vc8000_irq(int irq, void *data);
size_t hantro_vc8000_vp9_native_size(const struct hantro_ctx *ctx);
int vc8000_vp9_check_result(struct hantro_ctx *ctx);
int vc8000_h264_init(struct hantro_ctx *ctx);
void vc8000_h264_exit(struct hantro_ctx *ctx);
int vc8000_h264_run(struct hantro_ctx *ctx);
int vc8000_hevc_init(struct hantro_ctx *ctx);
void vc8000_hevc_exit(struct hantro_ctx *ctx);
int vc8000_hevc_run(struct hantro_ctx *ctx);
int vc8000_vp9_init(struct hantro_ctx *ctx);
void vc8000_vp9_exit(struct hantro_ctx *ctx);
int vc8000_vp9_run(struct hantro_ctx *ctx);
void vc8000_vp9_done(struct hantro_ctx *ctx);
void vc8000_vp9_abort(struct hantro_ctx *ctx);
#endif
