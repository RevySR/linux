// SPDX-License-Identifier: GPL-2.0-only
/*
 * TH1520 VC8000D H.264 stateless decoding, hardware modes 0 and 15.
 *
 * Copyright (C) 2026 th1520-v4l2 contributors
 *
 * DPB management and auxiliary-table handling adapted from Linux
 * hantro_h264.c and hantro_g1_h264_dec.c:
 * Copyright (c) 2014 Rockchip Electronics Co., Ltd.
 *     Hertz Wong <hertz.wong@rock-chips.com>
 *     Herman Chen <herman.chen@rock-chips.com>
 * Copyright (C) 2014 Google, Inc.
 *
 * Linux commit 8ba098e6b6ff0db8edf28528d1552be261af30d4.
 */

#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/overflow.h>
#include <linux/swab.h>

#include <media/v4l2-h264.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>

#include "hantro_vc8000.h"

/* Product-table dimensions selected by build 0x1f88 in both H.264 engines. */
#define h264_pic_width_in_cbs		VC8000_REG(4, 19, 0x1fff)
#define h264_pic_height_in_cbs		VC8000_REG(4, 6, 0x1fff)
#define h264_min_cb_size			VC8000_REG(12, 13, 0x7)
#define h264_max_cb_size			VC8000_REG(12, 10, 0x7)
#define h264_pic_width_4x4		VC8000_REG(20, 16, 0xfff)
#define h264_pic_height_4x4		VC8000_REG(20, 0, 0xfff)
#define h264_h10_bit_depth_y_minus8	VC8000_REG(8, 6, 0x3)
#define h264_h10_bit_depth_c_minus8	VC8000_REG(8, 4, 0x3)
#define h264_h10_idr_pic_id		VC8000_REG(12, 16, 0xffff)

static void vc8000_h264_set_params(struct hantro_ctx *ctx)
{
	const struct hantro_h264_dec_ctrls *ctrls = &ctx->h264_dec.ctrls;
	const struct v4l2_ctrl_h264_decode_params *dec = ctrls->decode;
	const struct v4l2_ctrl_h264_sps *sps = ctrls->sps;
	const struct v4l2_ctrl_h264_pps *pps = ctrls->pps;
	unsigned int mb_width = MB_WIDTH(ctx->src_fmt.width);
	unsigned int mb_height = MB_HEIGHT(ctx->src_fmt.height);

	hantro_vc8000_reg_write(ctx, &h264_seq_mbaff_e,
				!!(sps->flags & V4L2_H264_SPS_FLAG_MB_ADAPTIVE_FRAME_FIELD));
	if (sps->profile_idc > 66) {
		hantro_vc8000_reg_write(ctx, &h264_picord_count_e, 1);
		if (dec->nal_ref_idc)
			hantro_vc8000_reg_write(ctx, &h264_write_mvs_e, 1);
	}

	if (!(sps->flags & V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY) &&
	    ((sps->flags & V4L2_H264_SPS_FLAG_MB_ADAPTIVE_FRAME_FIELD) ||
	     (dec->flags & V4L2_H264_DECODE_PARAM_FLAG_FIELD_PIC)))
		hantro_vc8000_reg_write(ctx, &h264_pic_interlace_e, 1);

	if (dec->flags & V4L2_H264_DECODE_PARAM_FLAG_FIELD_PIC)
		hantro_vc8000_reg_write(ctx, &h264_pic_fieldmode_e, 1);

	if (!(dec->flags & V4L2_H264_DECODE_PARAM_FLAG_BOTTOM_FIELD))
		hantro_vc8000_reg_write(ctx, &h264_pic_topfield_e, 1);

	/*
	 * Build 0x1f88 uses 8x8 coding-block dimensions within 16x16 macroblocks.
	 * The 4x4 dimensions describe the same image; partial-CTB flags remain
	 * clear because H.264 coded dimensions contain complete macroblocks.
	 */
	hantro_vc8000_reg_write(ctx, &h264_pic_width_in_cbs, mb_width * 2);
	hantro_vc8000_reg_write(ctx, &h264_pic_height_in_cbs, mb_height * 2);
	hantro_vc8000_reg_write(ctx, &h264_ref_frames, sps->max_num_ref_frames);
	hantro_vc8000_reg_write(ctx, &h264_min_cb_size, 3);
	hantro_vc8000_reg_write(ctx, &h264_max_cb_size, 4);
	hantro_vc8000_reg_write(ctx, &h264_pic_width_4x4, mb_width * 4);
	hantro_vc8000_reg_write(ctx, &h264_pic_height_4x4, mb_height * 4);

	hantro_vc8000_reg_write(ctx, &h264_ch_qp_offset,
				pps->chroma_qp_index_offset);
	hantro_vc8000_reg_write(ctx, &h264_ch_qp_offset2,
				pps->second_chroma_qp_index_offset);
	hantro_vc8000_reg_write(ctx, &h264_type1_quant_e,
				!!(pps->flags & V4L2_H264_PPS_FLAG_SCALING_MATRIX_PRESENT));
	hantro_vc8000_reg_write(ctx, &h264_fieldpic_flag_e,
				!(sps->flags & V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY));

	hantro_vc8000_reg_write(ctx, &h264_init_qp,
				pps->pic_init_qp_minus26 + 26);

	hantro_vc8000_reg_write(ctx, &h264_framenum_len,
				sps->log2_max_frame_num_minus4 + 4);
	hantro_vc8000_reg_write(ctx, &h264_framenum, dec->frame_num);
	hantro_vc8000_reg_write(ctx, &h264_weight_bipr_idc,
				pps->weighted_bipred_idc);
	hantro_vc8000_reg_write(ctx, &h264_cabac_e,
				!!(pps->flags & V4L2_H264_PPS_FLAG_ENTROPY_CODING_MODE));
	hantro_vc8000_reg_write(ctx, &h264_dir_8x8_infer_e,
				!!(sps->flags & V4L2_H264_SPS_FLAG_DIRECT_8X8_INFERENCE));
	hantro_vc8000_reg_write(ctx, &h264_blackwhite_e,
				sps->profile_idc >= 100 &&
			      sps->chroma_format_idc == 0);
	hantro_vc8000_reg_write(ctx, &h264_weight_pred_e,
				!!(pps->flags & V4L2_H264_PPS_FLAG_WEIGHTED_PRED));

	hantro_vc8000_reg_write(ctx, &h264_refpic_mk_len,
				dec->dec_ref_pic_marking_bit_size);
	if (ctx->vc8000_dec->mode15) {
		/* H264SetupVlcRegs @0x7b1ce selects id339 in mode 15. */
		hantro_vc8000_reg_write(ctx, &h264_h10_idr_pic_id, dec->idr_pic_id);
		hantro_vc8000_reg_write(ctx, &h264_h10_bit_depth_y_minus8, 0);
		hantro_vc8000_reg_write(ctx, &h264_h10_bit_depth_c_minus8, 0);
	} else {
		hantro_vc8000_reg_write(ctx, &h264_idr_pic_id, dec->idr_pic_id);
	}
	hantro_vc8000_reg_write(ctx, &h264_const_intra_e,
				!!(pps->flags & V4L2_H264_PPS_FLAG_CONSTRAINED_INTRA_PRED));
	hantro_vc8000_reg_write(ctx, &h264_filt_ctrl_pres,
				!!(pps->flags &
				   V4L2_H264_PPS_FLAG_DEBLOCKING_FILTER_CONTROL_PRESENT));
	hantro_vc8000_reg_write(ctx, &h264_rdpic_cnt_pres,
				!!(pps->flags & V4L2_H264_PPS_FLAG_REDUNDANT_PIC_CNT_PRESENT));
	hantro_vc8000_reg_write(ctx, &h264_8x8trans_flag_e,
				!!(pps->flags & V4L2_H264_PPS_FLAG_TRANSFORM_8X8_MODE));
	hantro_vc8000_reg_write(ctx, &h264_idr_pic_e,
				!!(dec->flags & V4L2_H264_DECODE_PARAM_FLAG_IDR_PIC));

	hantro_vc8000_reg_write(ctx, &h264_pps_id, pps->pic_parameter_set_id);
	hantro_vc8000_reg_write(ctx, &h264_refidx0_active,
				pps->num_ref_idx_l0_default_active_minus1 + 1);
	hantro_vc8000_reg_write(ctx, &h264_refidx1_active,
				pps->num_ref_idx_l1_default_active_minus1 + 1);
	hantro_vc8000_reg_write(ctx, &h264_poc_length,
				dec->pic_order_cnt_bit_size);

	hantro_vc8000_reg_write(ctx, &h264_native_luma_stride,
				ctx->dst_fmt.plane_fmt[0].bytesperline);
	hantro_vc8000_reg_write(ctx, &h264_native_chroma_stride,
				ctx->dst_fmt.plane_fmt[0].bytesperline);
}

static void vc8000_h264_set_ref(struct hantro_ctx *ctx)
{
	const struct v4l2_h264_reference *b0, *b1, *p;
	unsigned int i, reg_num;
	u32 reg;

	hantro_vc8000_reg_write_raw(ctx, VC8000_H264_SWREG_VALID_REF,
				    ctx->h264_dec.dpb_valid);
	hantro_vc8000_reg_write_raw(ctx, VC8000_H264_SWREG_LT_REF,
				    ctx->h264_dec.dpb_longterm);

	for (i = 0; i < HANTRO_H264_DPB_SIZE; i += 2) {
		reg = VC8000_H264_REF_NBR_EVEN(hantro_h264_get_ref_nbr(ctx, i)) |
		      VC8000_H264_REF_NBR_ODD(hantro_h264_get_ref_nbr(ctx, i + 1));
		hantro_vc8000_reg_write_raw(ctx,
					    VC8000_H264_SWREG_REF_PIC(i / 2), reg);
	}

	b0 = ctx->h264_dec.reflists.b0;
	b1 = ctx->h264_dec.reflists.b1;
	p = ctx->h264_dec.reflists.p;

	/* H264InitRefPicList @0x7c432: select INIT_RLIST or BINIT_RLIST. */
	reg_num = ctx->vc8000_dec->mode15 ? 14 : 42;
	for (i = 0; i < 15; i += 3) {
		reg = ((b0[i].index     & 0x1f) << 0) |
		      ((b1[i].index     & 0x1f) << 5) |
		      ((b0[i + 1].index & 0x1f) << 10) |
		      ((b1[i + 1].index & 0x1f) << 15) |
		      ((b0[i + 2].index & 0x1f) << 20) |
		      ((b1[i + 2].index & 0x1f) << 25);
		hantro_vc8000_reg_write_raw(ctx, reg_num++, reg);
	}

	/* The final B-list pair is in swreg19 for mode 15, or swreg47 otherwise. */
	reg = ((b0[15].index & 0x1f) << 0) |
	      ((b1[15].index & 0x1f) << 5);
	if (ctx->vc8000_dec->mode15) {
		hantro_vc8000_reg_write_raw(ctx, 19, reg);
		reg = 0;
	}
	/* P-list entries keep their positions in both modes. */
	reg |= ((p[0].index  & 0x1f) << 10) |
	      ((p[1].index   & 0x1f) << 15) |
	      ((p[2].index   & 0x1f) << 20) |
	      ((p[3].index   & 0x1f) << 25);
	hantro_vc8000_reg_write_raw(ctx, VC8000_H264_SWREG_BD_P_REF_PIC, reg);

	reg_num = 0;
	for (i = 4; i < HANTRO_H264_DPB_SIZE; i += 6) {
		reg = ((p[i].index     & 0x1f) << 0) |
		      ((p[i + 1].index & 0x1f) << 5) |
		      ((p[i + 2].index & 0x1f) << 10) |
		      ((p[i + 3].index & 0x1f) << 15) |
		      ((p[i + 4].index & 0x1f) << 20) |
		      ((p[i + 5].index & 0x1f) << 25);
		hantro_vc8000_reg_write_raw(ctx,
					    VC8000_H264_SWREG_FWD_PIC(reg_num++),
					  reg);
	}

	for (i = 0; i < HANTRO_H264_DPB_SIZE; i++)
		hantro_vc8000_write_addr(ctx, VC8000_H264_ADDR_REF_LSB(i),
					 VC8000_H264_ADDR_REF_MSB(i),
				       hantro_h264_get_ref_buf(ctx, i));
}

static int vc8000_h264_set_stream(struct hantro_ctx *ctx)
{
	struct vb2_v4l2_buffer *src_buf = hantro_get_src_buf(ctx);
	struct vb2_buffer *src = &src_buf->vb2_buf;
	u32 data_offset = src->planes[0].data_offset;
	u32 bytesused = vb2_get_plane_payload(src, 0);
	dma_addr_t stream_dma;
	u32 prefix, stream_len;

	if (data_offset >= bytesused || bytesused > vb2_plane_size(src, 0))
		return -EINVAL;

	stream_dma = vb2_dma_contig_plane_dma_addr(src, 0) + data_offset;
	prefix = stream_dma & 15;
	if (check_add_overflow(bytesused - data_offset, prefix, &stream_len))
		return -EINVAL;

	/*
	 * VC8000D h264StreamPosUpdate() @ 0x7c838: the 128-bit bus
	 * uses a 16-byte base and a bit offset within the first bus word.
	 * The normal H.264 branch writes the same length to ids 161 and
	 * 1360 and clears id 1361.  The product table maps the
	 * latter two ids to swreg258/259 (0x7cc5a, 0x7cc6e, 0x7cc88).
	 * V4L2 bytesused includes data_offset; only the payload is decoded.
	 */
	hantro_vc8000_write_addr(ctx, VC8000_H264_ADDR_STREAM_LSB,
				 VC8000_H264_ADDR_STREAM_MSB, stream_dma - prefix);
	hantro_vc8000_reg_write(ctx, &h264_strm_start_bit, prefix * 8);
	hantro_vc8000_reg_write(ctx, &h264_start_code_e, 1);
	hantro_vc8000_reg_write(ctx, &h264_stream_len, stream_len);
	hantro_vc8000_reg_write_raw(ctx, 258, stream_len);
	hantro_vc8000_reg_write_raw(ctx, 259, 0);

	return 0;
}

static void vc8000_h264_set_buffers(struct hantro_ctx *ctx)
{
	const struct hantro_h264_dec_ctrls *ctrls = &ctx->h264_dec.ctrls;
	struct vb2_v4l2_buffer *dst_buf = hantro_get_dst_buf(ctx);
	struct hantro_decoded_buffer *dst = hantro_vc8000_buf(dst_buf);
	struct hantro_aux_buf *native = hantro_vc8000_native(ctx, &dst->base.vb.vb2_buf);
	dma_addr_t dst_dma;
	size_t mv_offset = hantro_vc8000_h264_native_mv_offset(ctx);
	size_t chroma_offset = hantro_vc8000_h264_native_chroma_offset(ctx);
	size_t sync_offset;
	u32 stride;
	unsigned int i;

	dst_dma = native->dma;
	hantro_vc8000_write_addr(ctx, VC8000_H264_ADDR_DST_LSB,
				 VC8000_H264_ADDR_DST_MSB, dst_dma);

	if (ctrls->sps->profile_idc > 66 && ctrls->decode->nal_ref_idc) {
		hantro_vc8000_write_addr(ctx, VC8000_H264_ADDR_DIR_MV_LSB,
					 VC8000_H264_ADDR_DIR_MV_MSB,
				       dst_dma + mv_offset);
	}

	if (ctx->vc8000_dec->mode15) {
		hantro_vc8000_write_addr_pair(ctx, VC8000_HEVC_ADDR_OUT_CHROMA,
					      dst_dma + chroma_offset);
		for (i = 0; i < HANTRO_H264_DPB_SIZE; i++) {
			dma_addr_t ref = hantro_h264_get_ref_buf(ctx, i);

			hantro_vc8000_write_addr_pair(ctx, VC8000_HEVC_ADDR_REF_CHROMA(i),
						      ref + chroma_offset);
			hantro_vc8000_write_addr_pair(ctx, VC8000_HEVC_ADDR_REF_MV(i),
						      (ref & ~(dma_addr_t)3) + mv_offset);
		}
		sync_offset = mv_offset - 32;
	} else {
		sync_offset = mv_offset + 64 * MB_WIDTH(ctx->src_fmt.width) *
			      MB_HEIGHT(ctx->src_fmt.height);
	}
	/* h264bsdInitDpb initializes the 32-byte picture synchronization area. */
	memset((u8 *)native->cpu + sync_offset, 0xff, 32);

	stride = ALIGN(ctx->src_fmt.width * 4, 64);
	hantro_vc8000_reg_write(ctx, &h264_native_luma_stride,
				hantro_vc8000_luma_stride(ctx, stride));
	hantro_vc8000_reg_write(ctx, &h264_native_chroma_stride,
				hantro_vc8000_chroma_stride(ctx, stride));
	hantro_vc8000_set_postproc(ctx, ctx->src_fmt.width, ctx->src_fmt.height);

	hantro_vc8000_write_addr(ctx, VC8000_H264_ADDR_QTABLE_LSB,
				 VC8000_H264_ADDR_QTABLE_MSB,
			       ctx->h264_dec.priv.dma);
}

static int vc8000_h264_prepare_run(struct hantro_ctx *ctx)
{
	struct hantro_h264_dec_ctrls *ctrls = &ctx->h264_dec.ctrls;
	struct vb2_v4l2_buffer *dst_buf = hantro_get_dst_buf(ctx);
	struct hantro_aux_buf *native = hantro_vc8000_native(ctx, &dst_buf->vb2_buf);
	const struct v4l2_ctrl_h264_sps *sps;
	u32 width, height;
	size_t required_size;
	unsigned int i;
	int ret;

	hantro_start_prepare_run(ctx);
	ctrls->sps = hantro_get_ctrl(ctx, V4L2_CID_STATELESS_H264_SPS);
	ctrls->decode = hantro_get_ctrl(ctx, V4L2_CID_STATELESS_H264_DECODE_PARAMS);
	if (!ctrls->sps || !ctrls->decode)
		return -EINVAL;
	sps = ctrls->sps;
	/* This implementation supports progressive 8-bit 4:2:0 pictures. */
	if (sps->chroma_format_idc != 1 || sps->bit_depth_luma_minus8 ||
	    sps->bit_depth_chroma_minus8 ||
	    !(sps->flags & V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY) ||
	    (sps->flags & V4L2_H264_SPS_FLAG_MB_ADAPTIVE_FRAME_FIELD) ||
	    (ctrls->decode->flags & (V4L2_H264_DECODE_PARAM_FLAG_FIELD_PIC |
				     V4L2_H264_DECODE_PARAM_FLAG_BOTTOM_FIELD)))
		return -EINVAL;

	/*
	 * Baseline profile 66 uses mode 0. The supported Main and High profiles
	 * use mode 15. This engine selection is validated for 8-bit input and
	 * does not advertise High 10 profile support.
	 */
	ctx->vc8000_dec->mode15 = sps->profile_idc != 66;

	width = (sps->pic_width_in_mbs_minus1 + 1U) * MB_DIM;
	height = (sps->pic_height_in_map_units_minus1 + 1U) * MB_DIM;
	if (width != ALIGN(ctx->src_fmt.width, MB_DIM) ||
	    height != ALIGN(ctx->src_fmt.height, MB_DIM) ||
	    width != ALIGN(ctx->dst_fmt.width, MB_DIM) ||
	    height != ALIGN(ctx->dst_fmt.height, MB_DIM) ||
	    width != ctx->dst_fmt.plane_fmt[0].bytesperline)
		return -EINVAL;

	required_size = hantro_vc8000_h264_native_size(ctx);
	if (!native->cpu || native->size < required_size)
		return -EINVAL;

	ctx->h264_dec.scaling_list_offset = ctx->vc8000_dec->mode15 ? 8 : 0;
	ret = hantro_h264_dec_prepare(ctx);
	if (ret)
		return ret;
	for (i = 0; i < HANTRO_H264_DPB_SIZE; i++)
		if ((ctx->h264_dec.dpb[i].flags & V4L2_H264_DPB_ENTRY_FLAG_ACTIVE) &&
		    !hantro_get_ref(ctx, ctx->h264_dec.dpb[i].reference_ts))
			return -ENOENT;
	return 0;
}

int vc8000_h264_run(struct hantro_ctx *ctx)
{
	int ret;

	ret = vc8000_h264_prepare_run(ctx);
	if (ret)
		return ret;

	hantro_vc8000_set_common_config(ctx);
	ret = vc8000_h264_set_stream(ctx);
	if (ret)
		return ret;
	vc8000_h264_set_params(ctx);
	vc8000_h264_set_ref(ctx);
	vc8000_h264_set_buffers(ctx);

	hantro_end_prepare_run(ctx);
	hantro_vc8000_start(ctx);

	return 0;
}

int vc8000_h264_init(struct hantro_ctx *ctx)
{
	int ret = hantro_vc8000_ctx_init(ctx);

	if (ret)
		return ret;
	/* Mode 15 inserts eight bytes before the scaling lists. */
	ctx->h264_dec.scaling_list_offset = 8;
	ret = hantro_h264_dec_init(ctx);
	if (ret)
		hantro_vc8000_ctx_exit(ctx);
	return ret;
}

void vc8000_h264_exit(struct hantro_ctx *ctx)
{
	hantro_h264_dec_exit(ctx);
	hantro_vc8000_ctx_exit(ctx);
}
