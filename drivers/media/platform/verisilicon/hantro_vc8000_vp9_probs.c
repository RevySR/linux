// SPDX-License-Identifier: GPL-2.0-only
/*
 * TH1520 VC8000D VP9 probability helpers.
 * Adapted from Linux hantro_vp9.c, hantro_vp9.h and hantro_g2_vp9_dec.c.
 * Copyright (C) 2021 Collabora Ltd.
 * Copyright (C) 2026 th1520-v4l2 contributors
 * Linux commit 8ba098e6b6ff0db8edf28528d1552be261af30d4.
 */

#include "hantro_vc8000.h"
#include "hantro_vc8000_vp9.h"

#define copy_tx_and_skip(p1, p2)				\
do {								\
	typeof(p1) _dst = (p1);					\
	typeof(p2) _src = (p2);					\
	memcpy(_dst->tx8, _src->tx8, sizeof(_dst->tx8));	\
	memcpy(_dst->tx16, _src->tx16, sizeof(_dst->tx16));	\
	memcpy(_dst->tx32, _src->tx32, sizeof(_dst->tx32));	\
	memcpy(_dst->skip, _src->skip, sizeof(_dst->skip));	\
} while (0)

void vc8000_vp9_update_probs(struct hantro_ctx *ctx)
{
	struct vc8000_vp9_ctx *vp9_ctx = ctx->vc8000_dec->vp9;
	unsigned int fctx_idx;

	if (!(vp9_ctx->cur.flags & V4L2_VP9_FRAME_FLAG_REFRESH_FRAME_CTX))
		return;

	fctx_idx = vp9_ctx->cur.frame_context_idx;

	if (!(vp9_ctx->cur.flags & (V4L2_VP9_FRAME_FLAG_PARALLEL_DEC_MODE |
				    V4L2_VP9_FRAME_FLAG_ERROR_RESILIENT))) {
		/* error_resilient_mode == 0 && frame_parallel_decoding_mode == 0 */
		struct v4l2_vp9_frame_context *probs = &vp9_ctx->probability_tables;
		bool frame_is_intra = vp9_ctx->cur.flags &
		    (V4L2_VP9_FRAME_FLAG_KEY_FRAME | V4L2_VP9_FRAME_FLAG_INTRA_ONLY);
		struct tx_and_skip {
			u8 tx8[2][1];
			u8 tx16[2][2];
			u8 tx32[2][3];
			u8 skip[3];
		} _tx_skip, *tx_skip = &_tx_skip;
		struct v4l2_vp9_frame_symbol_counts *counts;
		struct symbol_counts *hw_counts;
		u32 tx16p[2][4];
		int i;

		/* buffer the forward-updated TX and skip probs */
		if (frame_is_intra)
			copy_tx_and_skip(tx_skip, probs);

		/* 6.1.2 refresh_probs(): load_probs() and load_probs2() */
		*probs = vp9_ctx->working_context[fctx_idx];

		/* if FrameIsIntra then undo the effect of load_probs2() */
		if (frame_is_intra)
			copy_tx_and_skip(probs, tx_skip);

		counts = &vp9_ctx->cnts;
		hw_counts = vp9_ctx->counts.cpu;
		for (i = 0; i < ARRAY_SIZE(tx16p); ++i) {
			memcpy(tx16p[i],
			       hw_counts->tx16x16_count[i],
			       sizeof(hw_counts->tx16x16_count[0]));
			tx16p[i][3] = 0;
		}
		counts->tx16p = &tx16p;

		v4l2_vp9_adapt_coef_probs(probs, counts,
					  !vp9_ctx->last.valid ||
					  vp9_ctx->last.flags & V4L2_VP9_FRAME_FLAG_KEY_FRAME,
					  frame_is_intra);

		if (!frame_is_intra) {
			/* load_probs2() already done */
			u32 mv_mode[7][4];

			for (i = 0; i < ARRAY_SIZE(mv_mode); ++i) {
				mv_mode[i][0] = hw_counts->inter_mode_counts[i][1][0];
				mv_mode[i][1] = hw_counts->inter_mode_counts[i][2][0];
				mv_mode[i][2] = hw_counts->inter_mode_counts[i][0][0];
				mv_mode[i][3] = hw_counts->inter_mode_counts[i][2][1];
			}
			counts->mv_mode = &mv_mode;
			v4l2_vp9_adapt_noncoef_probs(&vp9_ctx->probability_tables, counts,
						     vp9_ctx->cur.reference_mode,
						     vp9_ctx->cur.interpolation_filter,
						     vp9_ctx->cur.tx_mode, vp9_ctx->cur.flags);
		}
	}

	vp9_ctx->working_context[fctx_idx] = vp9_ctx->probability_tables;
}
