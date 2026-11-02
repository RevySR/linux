/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * TH1520 VC8000D register definitions for ASIC 0x80018000, build 0x1f88.
 *
 * Copyright (C) 2026 th1520-v4l2 contributors
 *
 * Copyright 2018 Google LLC.
 * Copyright (c) 2021, Collabora
 *
 * The TH1520 register coordinates are hardware-interface facts validated
 * for this build. They are not interchangeable with other Hantro cores.
 * Each register index denotes a 32-bit word at decoder-base + 4 * index.
 */

#ifndef VC8000_VDEC_REGS_H_
#define VC8000_VDEC_REGS_H_

#include <linux/bits.h>
#include <linux/types.h>

/*
 * The per-job shadow covers all 512 decoder and post-processor words.
 */
#define VC8000_VDEC_REG_COUNT		512

#define VC8000_VDEC_REG_OFF(n)		((n) * 4)

/*
 * Register 0 is the read-only ASIC identifier; the build is in word 309.
 */

#define VC8000_VDEC_SWREG_IRQ		1

/*
 * Bit-field descriptor: register index, least-significant bit and unshifted mask.
 */
#define VC8000_REG(_swreg, _shift, _mask) \
	((const struct hantro_reg) { \
		.base = VC8000_VDEC_REG_OFF(_swreg), .shift = (_shift), .mask = (_mask), \
	})

/*
 * Register 1: interrupt status and decoder control.
 */
#define VC8000_IRQ_DEC_TIMEOUT		BIT(18)
#define VC8000_IRQ_DEC_ERROR_INT	BIT(16)
#define VC8000_IRQ_DEC_ASO_INT		BIT(15)
#define VC8000_IRQ_DEC_BUFFER_INT	BIT(14)
#define VC8000_IRQ_DEC_BUS_INT		BIT(13)
#define VC8000_IRQ_DEC_RDY_INT		BIT(12)
#define VC8000_IRQ_DEC_ABORT_INT	BIT(11)
#define VC8000_IRQ_DEC_IRQ		BIT(8)
#define VC8000_IRQ_DEC_ABORT_E		BIT(5)
#define VC8000_IRQ_DEC_IRQ_DIS		BIT(4)
#define VC8000_IRQ_DEC_STRM_CORRUPTED	BIT(1)
#define VC8000_IRQ_DEC_E		BIT(0)

/* SDK product-table field 2127 covers all fifteen status bits. */
#define VC8000_IRQ_STAT_MASK		GENMASK(25, 11)

#define VC8000_IRQ_ERROR_MASK \
	(VC8000_IRQ_DEC_TIMEOUT | VC8000_IRQ_DEC_ERROR_INT | \
	 VC8000_IRQ_DEC_BUS_INT | VC8000_IRQ_DEC_BUFFER_INT)

#define vc8000_dec_tile_int_e		VC8000_REG(1, 7, 0x1)
#define vc8000_dec_irq_dis		VC8000_REG(1, 4, 0x1)
#define vc8000_dec_e			VC8000_REG(1, 0, 0x1)

/*
 * Register 3 bits 31:27 select the hardware decoder engine.
 */
#define VC8000_DEC_MODE_H264		0
#define VC8000_DEC_MODE_HEVC		12
#define VC8000_DEC_MODE_VP9		13
#define VC8000_DEC_MODE_H264_HIGH10	15

#define vc8000_dec_mode			VC8000_REG(3, 27, 0x1f)

#define VC8000_BUS_WIDTH_128		2

/*
 * H.264 controls. The build-specific coding-block dimensions are in the backend.
 */
#define h264_table_byte_order		VC8000_REG(2, 12, 0xf)
#define h264_input_exhausted_irq_enable		VC8000_REG(3, 2, 0x1)
#define h264_input_block_mode		VC8000_REG(3, 1, 0x1)

#define h264_rlc_mode_e			VC8000_REG(3, 24, 0x1)
#define h264_pic_interlace_e		VC8000_REG(3, 23, 0x1)
#define h264_pic_fieldmode_e		VC8000_REG(3, 22, 0x1)
#define h264_pic_topfield_e		VC8000_REG(3, 19, 0x1)
#define h264_dec_out_dis		VC8000_REG(3, 15, 0x1)
#define h264_filtering_dis		VC8000_REG(3, 14, 0x1)
#define h264_mvc_e			VC8000_REG(3, 13, 0x1)
#define h264_write_mvs_e		VC8000_REG(3, 12, 0x1)
#define h264_seq_mbaff_e		VC8000_REG(3, 10, 0x1)
#define h264_picord_count_e		VC8000_REG(3, 9, 0x1)

#define h264_max_burst_sw58		VC8000_REG(58, 0, 0xff)
#define h264_buswidth_sw58		VC8000_REG(58, 8, 0x7)
#define h264_axi_rd_id_e_sw58		VC8000_REG(58, 14, 0x1)
#define h264_axi_wd_id_e_sw58		VC8000_REG(58, 13, 0x1)

#define h264_ref_frames			VC8000_REG(4, 0, 0x1f)

#define h264_strm_start_bit		VC8000_REG(5, 25, 0x7f)
#define h264_type1_quant_e		VC8000_REG(5, 24, 0x1)
#define h264_ch_qp_offset		VC8000_REG(5, 19, 0x1f)
#define h264_ch_qp_offset2		VC8000_REG(5, 14, 0x1f)
#define h264_fieldpic_flag_e		VC8000_REG(5, 0, 0x1)

/*
 * Register 6 contains the full 32-bit stream length.
 */
#define h264_stream_len			VC8000_REG(6, 0, 0xffffffff)

#define h264_cabac_e			VC8000_REG(7, 31, 0x1)
#define h264_blackwhite_e		VC8000_REG(7, 30, 0x1)
#define h264_dir_8x8_infer_e		VC8000_REG(7, 29, 0x1)
#define h264_weight_pred_e		VC8000_REG(7, 28, 0x1)
#define h264_weight_bipr_idc		VC8000_REG(7, 26, 0x3)
#define h264_framenum_len		VC8000_REG(7, 16, 0x1f)
#define h264_framenum			VC8000_REG(7, 0, 0xffff)

#define h264_const_intra_e		VC8000_REG(8, 31, 0x1)
#define h264_filt_ctrl_pres		VC8000_REG(8, 30, 0x1)
#define h264_rdpic_cnt_pres		VC8000_REG(8, 29, 0x1)
#define h264_8x8trans_flag_e		VC8000_REG(8, 28, 0x1)
#define h264_refpic_mk_len		VC8000_REG(8, 17, 0x7ff)
#define h264_idr_pic_e			VC8000_REG(8, 16, 0x1)
#define h264_idr_pic_id			VC8000_REG(8, 0, 0xffff)

#define h264_pps_id			VC8000_REG(9, 24, 0xff)
#define h264_refidx1_active		VC8000_REG(9, 19, 0x1f)
#define h264_refidx0_active		VC8000_REG(9, 14, 0x1f)
#define h264_poc_length			VC8000_REG(9, 0, 0xff)

/*
 * Register 13 contains start-code parsing and initial QP controls.
 */
#define h264_start_code_e		VC8000_REG(13, 31, 0x1)
#define h264_init_qp			VC8000_REG(13, 24, 0x7f)

#define VC8000_H264_SWREG_ERR_CONC	48

#define h264_pred_bc_tap_0_0		VC8000_REG(49, 22, 0x3ff)
#define h264_pred_bc_tap_0_1		VC8000_REG(49, 12, 0x3ff)
#define h264_pred_bc_tap_0_2		VC8000_REG(49, 2, 0x3ff)

#define h264_apf_threshold		VC8000_REG(55, 0, 0xffff)

#define h264_error_control_bit31	VC8000_REG(266, 31, 0x1)

#define h264_native_luma_stride		VC8000_REG(314, 16, 0xffff)
#define h264_native_chroma_stride		VC8000_REG(314, 0, 0xffff)

/*
 * Long-term and valid-reference bitmaps use complete 32-bit words.
 */
#define VC8000_H264_SWREG_LT_REF	38
#define VC8000_H264_SWREG_VALID_REF	39

/*
 * Address pairs are ordered MSB then LSB. Native references use these addresses.
 */
#define VC8000_H264_ADDR_STREAM_LSB	169
#define VC8000_H264_ADDR_STREAM_MSB	168
#define VC8000_H264_ADDR_DST_LSB	65
#define VC8000_H264_ADDR_DST_MSB	64
#define VC8000_H264_ADDR_REF_LSB(i)	(67 + (i) * 2)
#define VC8000_H264_ADDR_REF_MSB(i)	(66 + (i) * 2)
#define VC8000_H264_ADDR_QTABLE_LSB	175
#define VC8000_H264_ADDR_QTABLE_MSB	174
#define VC8000_H264_ADDR_DIR_MV_LSB	133
#define VC8000_H264_ADDR_DIR_MV_MSB	132

/*
 * Each reference-picture word packs two 16-bit frame numbers.
 */
#define VC8000_H264_SWREG_REF_PIC(i)	(30 + (i))
#define VC8000_H264_REF_NBR_EVEN(x)	(((x) & 0xffff) << 0)
#define VC8000_H264_REF_NBR_ODD(x)	(((x) & 0xffff) << 16)

/*
 * Mode 0 reference lists pack six 5-bit entries per word.
 */
#define VC8000_H264_SWREG_BD_P_REF_PIC	47
#define VC8000_H264_SWREG_FWD_PIC(i)	(10 + (i))

/*
 * HEVC controls for hardware mode 12.
 */

#define hevc_out_ec_bypass		VC8000_REG(3, 8, 0x1)
#define hevc_control_bit7		VC8000_REG(3, 7, 0x1)
#define hevc_skip_reference_reads		VC8000_REG(3, 6, 0x1)
#define hevc_control_bit5		VC8000_REG(3, 5, 0x1)
#define hevc_input_exhausted_irq_enable		VC8000_REG(3, 2, 0x1)
#define hevc_input_block_mode	VC8000_REG(3, 1, 0x1)
#define hevc_final_input_buffer		VC8000_REG(3, 0, 0x1)

#define hevc_out_dis			VC8000_REG(3, 15, 0x1)
#define hevc_filtering_dis		VC8000_REG(3, 14, 0x1)
#define hevc_write_mvs_e		VC8000_REG(3, 12, 0x1)

#define hevc_pic_width_in_cbs		VC8000_REG(4, 19, 0x1fff)
#define hevc_pic_height_in_cbs		VC8000_REG(4, 6, 0x1fff)
#define hevc_num_ref_frames		VC8000_REG(4, 0, 0x1f)

#define hevc_strm_start_bit		VC8000_REG(5, 25, 0x7f)
#define hevc_scaling_list_e		VC8000_REG(5, 24, 0x1)
#define hevc_cb_qp_offset		VC8000_REG(5, 19, 0x1f)
#define hevc_cr_qp_offset		VC8000_REG(5, 14, 0x1f)
#define hevc_sign_data_hide		VC8000_REG(5, 12, 0x1)
#define hevc_tempor_mvp_e		VC8000_REG(5, 11, 0x1)
#define hevc_max_cu_qpd_depth		VC8000_REG(5, 5, 0x3f)
#define hevc_cu_qpd_e			VC8000_REG(5, 4, 0x1)

#define hevc_stream_len			VC8000_REG(6, 0, 0xffffffff)

#define hevc_cabac_init_present		VC8000_REG(7, 31, 0x1)
#define hevc_blackwhite_e		VC8000_REG(7, 30, 0x1)
#define hevc_weight_pred_e		VC8000_REG(7, 28, 0x1)
#define hevc_weight_bipr_idc		VC8000_REG(7, 26, 0x3)
#define hevc_loop_filter_across_slices		VC8000_REG(7, 25, 0x1)
#define hevc_loop_filter_across_tiles		VC8000_REG(7, 24, 0x1)
#define hevc_asym_pred_e		VC8000_REG(7, 23, 0x1)
#define hevc_sao_e			VC8000_REG(7, 22, 0x1)
#define hevc_pcm_loop_filter_disabled		VC8000_REG(7, 21, 0x1)
#define hevc_slice_chroma_qp_offsets_present		VC8000_REG(7, 20, 0x1)
#define hevc_dependent_segments_enabled		VC8000_REG(7, 19, 0x1)
#define hevc_deblocking_override_enabled		VC8000_REG(7, 18, 0x1)
#define hevc_strong_smooth_e		VC8000_REG(7, 17, 0x1)
#define hevc_filt_offset_beta		VC8000_REG(7, 12, 0x1f)
#define hevc_filt_offset_tc		VC8000_REG(7, 7, 0x1f)
#define hevc_slice_hdr_ext_e		VC8000_REG(7, 6, 0x1)
#define hevc_slice_header_extra_bits		VC8000_REG(7, 3, 0x7)

#define hevc_const_intra_e		VC8000_REG(8, 31, 0x1)
#define hevc_filt_ctrl_pres		VC8000_REG(8, 30, 0x1)
#define hevc_idr_pic_e			VC8000_REG(8, 16, 0x1)
#define hevc_pcm_luma_sample_depth		VC8000_REG(8, 12, 0xf)
#define hevc_pcm_chroma_sample_depth		VC8000_REG(8, 8, 0xf)
#define hevc_bit_depth_y_minus8		VC8000_REG(8, 6, 0x3)
#define hevc_bit_depth_c_minus8		VC8000_REG(8, 4, 0x3)
#define hevc_output_8_bits		VC8000_REG(8, 3, 0x1)
#define hevc_output_format		VC8000_REG(8, 0, 0x7)

#define hevc_pps_id			VC8000_REG(9, 24, 0xff)
#define hevc_refidx1_active		VC8000_REG(9, 19, 0x1f)
#define hevc_refidx0_active		VC8000_REG(9, 14, 0x1f)
#define hevc_hdr_skip_length		VC8000_REG(9, 0, 0x3fff)

/*
 * Register 10 stores the tile-column and tile-row counts.
 */
#define hevc_num_tile_cols_8k		VC8000_REG(10, 17, 0x7f)
#define hevc_num_tile_rows_8k		VC8000_REG(10, 12, 0x1f)
#define hevc_tile_enable		VC8000_REG(10, 1, 0x1)
#define hevc_entropy_row_sync_enabled		VC8000_REG(10, 0, 0x1)

/*
 * The HEVC long-term-reference bitmap shares register 38 with H.264.
 */
#define hevc_refer_lterm_e		VC8000_REG(38, 0, 0xffffffff)
#define hevc_min_cb_size		VC8000_REG(12, 13, 0x7)
#define hevc_max_cb_size		VC8000_REG(12, 10, 0x7)
#define hevc_min_pcm_size		VC8000_REG(12, 7, 0x7)
#define hevc_max_pcm_size		VC8000_REG(12, 4, 0x7)
#define hevc_pcm_e			VC8000_REG(12, 3, 0x1)
#define hevc_transform_skip_enabled		VC8000_REG(12, 2, 0x1)
#define hevc_transquant_bypass_enabled		VC8000_REG(12, 1, 0x1)
#define hevc_reference_list_modification_enabled		VC8000_REG(12, 0, 0x1)

#define hevc_start_code_e		VC8000_REG(13, 31, 0x1)
#define hevc_init_qp			VC8000_REG(13, 24, 0x7f)
#define hevc_min_trb_size		VC8000_REG(13, 13, 0x7)
#define hevc_max_trb_size		VC8000_REG(13, 10, 0x7)
#define hevc_max_intra_hierdepth	VC8000_REG(13, 7, 0x7)
#define hevc_max_inter_hierdepth	VC8000_REG(13, 4, 0x7)
#define hevc_parallel_merge		VC8000_REG(13, 0, 0xf)

/*
 * Each reference-list word packs three pairs of 5-bit L0 and L1 indices.
 */
#define VC8000_HEVC_SWREG_RLIST(i)	(14 + (i))

#define hevc_partial_ctb_x		VC8000_REG(20, 31, 0x1)
#define hevc_partial_ctb_y		VC8000_REG(20, 30, 0x1)
#define hevc_pic_width_4x4		VC8000_REG(20, 16, 0xfff)
#define hevc_pic_height_4x4		VC8000_REG(20, 0, 0xfff)

/*
 * Each POC word packs four signed 8-bit differences; saturate before encoding.
 */
#define VC8000_HEVC_SWREG_CUR_POC(i)	(46 + (i))

#define hevc_apf_disable		VC8000_REG(55, 31, 0x1)
#define hevc_apf_threshold		VC8000_REG(55, 0, 0xffff)

#define hevc_refer_doublebuffer_e	VC8000_REG(58, 15, 0x1)
#define hevc_axi_rd_id_e		VC8000_REG(58, 14, 0x1)
#define hevc_axi_wd_id_e		VC8000_REG(58, 13, 0x1)
#define hevc_buswidth			VC8000_REG(58, 8, 0x7)
#define hevc_max_burst			VC8000_REG(58, 0, 0xff)

#define hevc_axi_wr_id			VC8000_REG(60, 16, 0xffff)
#define hevc_axi_rd_id			VC8000_REG(60, 0, 0xffff)

/*
 * Register 265: master bit stays clear; fields 27:18 and 17:8 are set to 64.
 * The individual field meanings remain unverified.
 */

#define hevc_down_scale_e		VC8000_REG(184, 7, 0x1)

#define hevc_strm_buffer_len		VC8000_REG(258, 0, 0xffffffff)
#define hevc_strm_start_offset		VC8000_REG(259, 0, 0xffffffff)

#define hevc_native_luma_stride		VC8000_REG(314, 16, 0xffff)
#define hevc_native_chroma_stride		VC8000_REG(314, 0, 0xffff)

/*
 * HEVC address pairs: the listed index is the MSB word; the LSB follows.
 */
#define VC8000_HEVC_ADDR_OUT_LUMA	64
#define VC8000_HEVC_ADDR_REF_LUMA(i)	(66 + (i) * 2)
#define VC8000_HEVC_ADDR_OUT_CHROMA	98
#define VC8000_HEVC_ADDR_REF_CHROMA(i)	(100 + (i) * 2)
#define VC8000_HEVC_ADDR_OUT_MV		132
#define VC8000_HEVC_ADDR_REF_MV(i)	(134 + (i) * 2)
#define VC8000_HEVC_ADDR_TILE_SIZES	166
#define VC8000_HEVC_ADDR_STREAM		168
#define VC8000_HEVC_ADDR_SCALING_LIST	170
#define VC8000_HEVC_ADDR_TILE_FILTER	178
#define VC8000_HEVC_ADDR_TILE_SAO	180
#define VC8000_HEVC_ADDR_TILE_BSD	182

/*
 * VC9000D shares the decoder registers above but moves the post-processor.
 * Register 353 holds its input format and the mask of enabled output units,
 * and the first output unit starts at register 384. Scale mode 1 leaves the
 * picture size unchanged.
 */
#define VC9000D_PP_IN_FORMAT_DEC	1
#define VC9000D_PP_UNIT0		BIT(0)
#define VC9000D_PP_SCALE_NONE		1

#define vc9000d_pp_in_format		VC8000_REG(353, 27, 0x1f)
#define vc9000d_pp_units		VC8000_REG(353, 0, 0xff)
#define vc9000d_pp_out_e		VC8000_REG(384, 0, 0x1)
#define vc9000d_pp_out_y_stride		VC8000_REG(391, 16, 0xffff)
#define vc9000d_pp_out_c_stride		VC8000_REG(391, 0, 0xffff)
#define vc9000d_pp_in_width		VC8000_REG(393, 16, 0xffff)
#define vc9000d_pp_in_height		VC8000_REG(393, 0, 0xffff)
#define vc9000d_pp_hor_scale_mode	VC8000_REG(394, 8, 0xff)
#define vc9000d_pp_ver_scale_mode	VC8000_REG(394, 0, 0xff)
#define vc9000d_pp_out_width		VC8000_REG(395, 16, 0xffff)
#define vc9000d_pp_out_height		VC8000_REG(395, 0, 0xffff)

/*
 * VC9000D address pairs, MSB word first like above. The SDK programs the
 * tile edge and post-processor column buffers into two pairs each.
 */
#define VC9000D_ADDR_TILE_EDGE(i)	(271 + (i) * 2)
#define VC9000D_ADDR_PP_COLUMN(i)	(362 + (i) * 2)
#define VC9000D_ADDR_PP_OUT_LUMA	385
#define VC9000D_ADDR_PP_OUT_CHROMA	387
#define VC9000D_ADDR_PP_SCALER0(i)	(403 + (i) * 2)
#define VC9000D_ADDR_PP_SCALER1(i)	(407 + (i) * 2)

#endif
