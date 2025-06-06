// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_rect.h"
#include "mtk_drm_drv.h"
#include "mtk_disp_rsz.h"
#include "platform/mtk_drm_platform.h"

#define DISP_REG_RSZ_ENABLE (0x000)
#define FLD_RSZ_RST REG_FLD_MSB_LSB(16, 16)
#define FLD_RSZ_EN REG_FLD_MSB_LSB(0, 0)
#define DISP_REG_RSZ_CONTROL_1 (0x004)
#define FLD_RSZ_INT_WCLR_EN REG_FLD_MSB_LSB(31, 31)
#define FLD_RSZ_INTEN REG_FLD_MSB_LSB(30, 28)
#define FLD_RSZ_DCM_DIS REG_FLD_MSB_LSB(27, 27)
#define FLD_RSZ_VERTICAL_TABLE_SELECT REG_FLD_MSB_LSB(25, 21)
#define FLD_RSZ_HORIZONTAL_TABLE_SELECT REG_FLD_MSB_LSB(20, 16)
#define FLD_RSZ_VERTICAL_EN REG_FLD_MSB_LSB(1, 1)
#define FLD_RSZ_HORIZONTAL_EN REG_FLD_MSB_LSB(0, 0)
#define DISP_REG_RSZ_CONTROL_2 (0x008)
#define FLD_RSZ_RGB_BIT_MODE REG_FLD_MSB_LSB(28, 28)
#define FLD_RSZ_POWER_SAVING REG_FLD_MSB_LSB(9, 9)
#define DISP_REG_RSZ_INT_FLAG (0x00c)
#define FLD_RSZ_SOF_RESET REG_FLD_MSB_LSB(5, 5)
#define FLD_RSZ_SIZE_ERR REG_FLD_MSB_LSB(4, 4)
#define FLD_RSZ_FRAME_END REG_FLD_MSB_LSB(1, 1)
#define FLD_RSZ_FRAME_START REG_FLD_MSB_LSB(0, 0)
#define DISP_REG_RSZ_INPUT_IMAGE (0x010)
#define FLD_RSZ_INPUT_IMAGE_H REG_FLD_MSB_LSB(31, 16)
#define FLD_RSZ_INPUT_IMAGE_W REG_FLD_MSB_LSB(15, 0)
#define DISP_REG_RSZ_OUTPUT_IMAGE (0x014)
#define FLD_RSZ_OUTPUT_IMAGE_H REG_FLD_MSB_LSB(31, 16)
#define FLD_RSZ_OUTPUT_IMAGE_W REG_FLD_MSB_LSB(15, 0)
#define DISP_REG_RSZ_HORIZONTAL_COEFF_STEP (0x018)
#define DISP_REG_RSZ_VERTICAL_COEFF_STEP (0x01c)
#define DISP_REG_RSZ_LUMA_HORIZONTAL_INTEGER_OFFSET (0x020)
#define DISP_REG_RSZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET (0x024)
#define DISP_REG_RSZ_LUMA_VERTICAL_INTEGER_OFFSET (0x028)
#define DISP_REG_RSZ_LUMA_VERTICAL_SUBPIXEL_OFFSET (0x02c)
#define DISP_REG_RSZ_DEBUG_SEL (0x044)
#define DISP_REG_RSZ_DEBUG (0x048)
#define DISP_REG_RSZ_SHADOW_CTRL (0x0f0)
#define FLD_RSZ_READ_WRK_REG REG_FLD_MSB_LSB(2, 2)
#define FLD_RSZ_FORCE_COMMIT REG_FLD_MSB_LSB(1, 1)
#define FLD_RSZ_BYPASS_SHADOW REG_FLD_MSB_LSB(0, 0)
#define RSZ_READ_WRK_REG	BIT(2)
#define RSZ_FORCE_COMMIT	BIT(1)
#define RSZ_BYPASS_SHADOW	BIT(0)


#define UNIT 32768
#define OUT_TILE_LOSS 0

enum mtk_rsz_color_format {
	ARGB2101010,
	RGB999,
	RGB888,
	UNKNOWN_RSZ_CFMT,
};

struct rsz_tile_params {
	u32 step;
	u32 int_offset;
	u32 sub_offset;
	u32 in_len;
	u32 out_len;
};

struct mtk_rsz_config_struct {
	struct rsz_tile_params tw[2];
	struct rsz_tile_params th[1];
	enum mtk_rsz_color_format fmt;
	u32 frm_in_w;
	u32 frm_in_h;
	u32 frm_out_w;
	u32 frm_out_h;
};

/**
 * struct mtk_disp_rsz - DISP_RSZ driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report irq events to
 */
struct mtk_disp_rsz {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_rsz_data *data;
};

struct mtk_disp_rsz_tile_overhead {
	unsigned int left_in_width;
	unsigned int left_overhead;
	unsigned int left_comp_overhead;
	unsigned int left_out_tile_loss;
	unsigned int right_in_width;
	unsigned int right_overhead;
	unsigned int right_comp_overhead;
	unsigned int right_out_tile_loss;
	bool is_support;

	/* store rsz tile_overhead calc */
	struct rsz_tile_params tw[2];
};

struct mtk_disp_rsz_tile_overhead rsz_tile_overhead = { 0 };

static inline struct mtk_disp_rsz *comp_to_rsz(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_rsz, ddp_comp);
}

static void mtk_rsz_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	if (priv->data->mmsys_id == MMSYS_MT6989 && comp->id == DDP_COMPONENT_RSZ1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_ENABLE, 0, ~0);
		return;
	}
	if ((!comp->mtk_crtc->scaling_ctx.scaling_en)
		&& mtk_crtc_check_is_scaling_comp(comp->mtk_crtc, comp->id)) {
		DDPDBG("%s: scaling-up disable, no need to start %s\n", __func__,
			mtk_dump_comp_str(comp));
		return;
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_ENABLE, 0x1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_DEBUG_SEL, 0x3, ~0);
}

static void mtk_rsz_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_ENABLE, 0x0, ~0);
}

int mtk_rsz_calc_tile_params(u32 frm_in_len, u32 frm_out_len, bool tile_mode,
			     struct rsz_tile_params t[])
{
	u32 out_tile_loss[2] = {OUT_TILE_LOSS, OUT_TILE_LOSS};
	u32 in_tile_loss[2] = {out_tile_loss[0] + 4, out_tile_loss[1] + 4};
	u32 step = 0;
	s32 init_phase = 0;
	s32 offset[2] = {0};
	s32 int_offset[2] = {0};
	s32 sub_offset[2] = {0};
	u32 tile_in_len[2] = {0};
	u32 tile_out_len[2] = {0};

	if (frm_out_len > 1)
		step = (UNIT * (frm_in_len - 1) + (frm_out_len - 2)) /
			(frm_out_len - 1);
	else {
		DRM_ERROR("%s:%d Division by zero\n", __func__, __LINE__);
		return -1;
	}


	/* left half */
	offset[0] = (step * (frm_out_len - 1) - UNIT * (frm_in_len - 1)) / 2;
	init_phase = UNIT - offset[0];
	sub_offset[0] = -offset[0];
	if (sub_offset[0] < 0) {
		int_offset[0]--;
		sub_offset[0] = UNIT + sub_offset[0];
	}
	if (sub_offset[0] >= UNIT) {
		int_offset[0]++;
		sub_offset[0] = sub_offset[0] - UNIT;
	}
	if (tile_mode) {
		out_tile_loss[0] =
			(rsz_tile_overhead.is_support ? rsz_tile_overhead.left_out_tile_loss : 0);
		in_tile_loss[0] = out_tile_loss[0] + 4;
		DDPINFO("%s :out_tile_loss[0]:%d, in_tile_loss[0]:%d\n", __func__,
			out_tile_loss[0], in_tile_loss[0]);

		tile_in_len[0] = (((frm_out_len / 2) * frm_in_len * 10) /
			frm_out_len + 5) / 10;
		if (tile_in_len[0] + in_tile_loss[0] >= frm_in_len)
			in_tile_loss[0] = frm_in_len - tile_in_len[0];

		tile_out_len[0] = frm_out_len / 2 + out_tile_loss[0];
		if (tile_in_len[0] + in_tile_loss[0] > tile_out_len[0])
			in_tile_loss[0] = tile_out_len[0] - tile_in_len[0];
		tile_in_len[0] += in_tile_loss[0];
	} else {
		tile_in_len[0] = frm_in_len;
		tile_out_len[0] = frm_out_len;
	}

	t[0].step = step;
	t[0].int_offset = (u32)(int_offset[0] & 0xffff);
	t[0].sub_offset = (u32)(sub_offset[0] & 0x1fffff);
	t[0].in_len = tile_in_len[0];
	t[0].out_len = tile_out_len[0];

	DDPDBG("%s:%s:step:%u,offset:%u.%u,len:%u->%u\n", __func__,
	       tile_mode ? "dual" : "single", t[0].step, t[0].int_offset,
	       t[0].sub_offset, t[0].in_len, t[0].out_len);

	if (int_offset[0] < -1)
		DDPINFO("%s :pipe0_scale_err\n", __func__);
	if (!tile_mode)
		return 0;

	/* right half */
	out_tile_loss[1] =
		(rsz_tile_overhead.is_support ? rsz_tile_overhead.right_out_tile_loss : 0);
	in_tile_loss[1] = out_tile_loss[1] + 4;
	DDPINFO("%s :out_tile_loss[1]:%d, in_tile_loss[1]:%d\n", __func__,
		out_tile_loss[1], in_tile_loss[1]);

	tile_out_len[1] = frm_out_len - (tile_out_len[0] - out_tile_loss[0]) + out_tile_loss[1];
	tile_in_len[1] = (((tile_out_len[1] - out_tile_loss[1]) * frm_in_len * 10) /
		frm_out_len + 5) / 10;
	if (tile_in_len[1] + in_tile_loss[1] >= frm_in_len)
		in_tile_loss[1] = frm_in_len - tile_in_len[1];

	if (tile_in_len[1] + in_tile_loss[1] > tile_out_len[1])
		in_tile_loss[1] = tile_out_len[1] - tile_in_len[1];
	tile_in_len[1] += in_tile_loss[1];

	offset[1] = (-offset[0]) + ((tile_out_len[0] - out_tile_loss[0] - out_tile_loss[1]) *
		step) - (frm_in_len - tile_in_len[1]) * UNIT;

	int_offset[1] = offset[1] / UNIT;
	if (offset[1] >= 0)
		sub_offset[1] = offset[1] - UNIT * int_offset[1];
	else
		sub_offset[1] = UNIT * int_offset[1] - offset[1];

	t[1].step = step;
	t[1].int_offset = (u32)(int_offset[1] & 0xffff);
	t[1].sub_offset = (u32)(sub_offset[1] & 0x1fffff);
	t[1].in_len = tile_in_len[1];
	t[1].out_len = tile_out_len[1];

	DDPDBG("%s:%s:step:%u,offset:%u.%u,len:%u->%u\n", __func__,
	       tile_mode ? "dual" : "single", t[1].step, t[1].int_offset,
	       t[1].sub_offset, t[1].in_len, t[1].out_len);

	if (int_offset[1] < -1 || tile_out_len[1] >= frm_out_len)
		DDPINFO("%s :pipe1_scale_err\n", __func__);
	return 0;
}

static int mtk_rsz_set_color_format(enum mtk_rsz_color_format fmt)
{
	u32 reg_val = 0;

	switch (fmt) {
	case ARGB2101010:
		reg_val = REG_FLD_VAL(FLD_RSZ_POWER_SAVING, 0x0);
		reg_val |= REG_FLD_VAL(FLD_RSZ_RGB_BIT_MODE, 0x0);
		break;
	case RGB999:
		reg_val = REG_FLD_VAL(FLD_RSZ_POWER_SAVING, 0x1);
		reg_val |= REG_FLD_VAL(FLD_RSZ_RGB_BIT_MODE, 0x0);
		break;
	case RGB888:
		reg_val = REG_FLD_VAL(FLD_RSZ_POWER_SAVING, 0x1);
		reg_val |= REG_FLD_VAL(FLD_RSZ_RGB_BIT_MODE, 0x1);
		break;
	default:
		DDPMSG("unknown resize color format\n");
		break;
	}
	return reg_val;
}

static int mtk_rsz_check_params(struct mtk_rsz_config_struct *rsz_config,
				unsigned int tile_length)
{
	if ((rsz_config->frm_in_w != rsz_config->frm_out_w ||
	     rsz_config->frm_in_h != rsz_config->frm_out_h) &&
	    rsz_config->frm_in_w > tile_length) {
		DDPPR_ERR("%s:need rsz but input width(%u) > limit(%u)\n",
			  __func__, rsz_config->frm_in_w, tile_length);
		return -EINVAL;
	}

	return 0;
}

static void mtk_disp_rsz_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	bool tile_mode = true;
	u32 tile_idx = 0;
	u32 frm_in_len, frm_out_len;
	u32 in_w = 0, out_w = 0;
	u32 left_in_w = 0, right_in_w = 0;

	DDPINFO("%s comp:%s, scaling_en:%d, cfg:(%ux%u)->(%ux%u)\n", __func__,
		mtk_dump_comp_str(comp), comp->mtk_crtc->scaling_ctx.scaling_en,
		cfg->rsz_src_w, cfg->rsz_src_h, cfg->w, cfg->h);

	if (!comp->mtk_crtc->scaling_ctx.scaling_en)
		return;

	if (cfg->tile_overhead.is_support) {

		frm_in_len = cfg->rsz_src_w;
		frm_out_len = cfg->w;

		left_in_w = cfg->rsz_src_w / 2;
		right_in_w =  cfg->rsz_src_w / 2;

		/*set component overhead*/
		if (comp->id == DDP_COMPONENT_RSZ0) {

			/* copy from post-accumulation */
			rsz_tile_overhead.left_out_tile_loss = cfg->tile_overhead.left_overhead;
			rsz_tile_overhead.is_support = cfg->tile_overhead.is_support;

			mtk_rsz_calc_tile_params(frm_in_len, frm_out_len,
				tile_mode, rsz_tile_overhead.tw);

			tile_idx = 0;
			in_w = rsz_tile_overhead.tw[tile_idx].in_len;
			out_w = rsz_tile_overhead.tw[tile_idx].out_len;

			rsz_tile_overhead.left_comp_overhead = in_w - left_in_w;

			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead =
				rsz_tile_overhead.left_comp_overhead;
			cfg->tile_overhead.left_in_width =
				rsz_tile_overhead.left_comp_overhead + left_in_w;
			cfg->tile_overhead.left_overhead_scaling =
				rsz_tile_overhead.left_out_tile_loss;

			/*copy from total overhead info*/
			rsz_tile_overhead.left_in_width = cfg->tile_overhead.left_in_width;
			rsz_tile_overhead.left_overhead = cfg->tile_overhead.left_overhead;
		}
		if (comp->id == DDP_COMPONENT_RSZ2) {

			/* copy from post-accumulation */
			rsz_tile_overhead.right_out_tile_loss = cfg->tile_overhead.right_overhead;
			rsz_tile_overhead.is_support = cfg->tile_overhead.is_support;

			mtk_rsz_calc_tile_params(frm_in_len, frm_out_len,
				tile_mode, rsz_tile_overhead.tw);

			tile_idx = 1;
			in_w = rsz_tile_overhead.tw[tile_idx].in_len;
			out_w = rsz_tile_overhead.tw[tile_idx].out_len;

			rsz_tile_overhead.right_comp_overhead = in_w - right_in_w;

			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead =
				rsz_tile_overhead.right_comp_overhead;
			cfg->tile_overhead.right_in_width =
				rsz_tile_overhead.right_comp_overhead + right_in_w;
			cfg->tile_overhead.right_overhead_scaling =
				rsz_tile_overhead.right_out_tile_loss;

			/*copy from total overhead info*/
			rsz_tile_overhead.right_in_width = cfg->tile_overhead.right_in_width;
			rsz_tile_overhead.right_overhead = cfg->tile_overhead.right_overhead;
		}
	}
}

static void mtk_rsz_config(struct mtk_ddp_comp *comp,
			   struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_rsz_config_struct *rsz_config = NULL;
	struct mtk_disp_rsz *rsz = comp_to_rsz(comp);
	enum mtk_rsz_color_format fmt = ARGB2101010;
	bool tile_mode = false;
	u32 reg_val = 0;
	u32 tile_idx = 0;
	u32 in_w = 0, in_h = 0, out_w = 0, out_h = 0;

	if (!mtk_crtc_check_is_scaling_comp(comp->mtk_crtc, comp->id)) {
		DDPINFO("%s only for res switch on ap, return\n", __func__);
		return;
	}

	DDPINFO("%s comp:%s, scaling_en:%d, cfg:(%ux%u)->(%ux%u)\n", __func__,
		mtk_dump_comp_str(comp), comp->mtk_crtc->scaling_ctx.scaling_en,
		cfg->rsz_src_w, cfg->rsz_src_h, cfg->w, cfg->h);

	if (!comp->mtk_crtc->scaling_ctx.scaling_en) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_REG_RSZ_ENABLE, 0x0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_REG_RSZ_INPUT_IMAGE, 0x0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_REG_RSZ_OUTPUT_IMAGE, 0x0, ~0);
		return;
	}

	rsz_config = kzalloc(sizeof(struct mtk_rsz_config_struct), GFP_KERNEL);
	if (!rsz_config) {
		DDPPR_ERR("fail to create rsz_config!\n");
		return;
	}

	if (comp->mtk_crtc->is_dual_pipe) {
		rsz_config->frm_in_w = cfg->rsz_src_w / 2;
		rsz_config->frm_out_w = cfg->w / 2;
	} else {
		rsz_config->frm_in_w = cfg->rsz_src_w;
		rsz_config->frm_out_w = cfg->w;
	}

	rsz_config->frm_in_h = cfg->rsz_src_h;
	rsz_config->frm_out_h = cfg->h;

	if (mtk_rsz_check_params(rsz_config, rsz->data->tile_length)) {
		kfree(rsz_config);
		return;
	}

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		if (comp->id == DDP_COMPONENT_RSZ0)
			tile_idx = 0;
		else if (comp->id == DDP_COMPONENT_RSZ2)
			tile_idx = 1;

		rsz_config->tw[tile_idx].in_len =
			rsz_tile_overhead.tw[tile_idx].in_len;
		rsz_config->tw[tile_idx].out_len =
			rsz_tile_overhead.tw[tile_idx].out_len;
		rsz_config->tw[tile_idx].step =
			rsz_tile_overhead.tw[tile_idx].step;
		rsz_config->tw[tile_idx].int_offset =
			rsz_tile_overhead.tw[tile_idx].int_offset;
		rsz_config->tw[tile_idx].sub_offset =
			rsz_tile_overhead.tw[tile_idx].sub_offset;
	} else {
		/* dual pipe without tile_overhead or single pipe */
		mtk_rsz_calc_tile_params(rsz_config->frm_in_w, rsz_config->frm_out_w,
					 tile_mode, rsz_config->tw);
	}

	mtk_rsz_calc_tile_params(rsz_config->frm_in_h, rsz_config->frm_out_h,
				 tile_mode, rsz_config->th);

	in_w = rsz_config->tw[tile_idx].in_len;
	in_h = rsz_config->th[0].in_len;
	out_w = rsz_config->tw[tile_idx].out_len;
	out_h = rsz_config->th[0].out_len;

	if (in_w > out_w || in_h > out_h) {
		DDPPR_ERR("DISP_RSZ only supports scale-up,(%ux%u)->(%ux%u)\n",
			  in_w, in_h, out_w, out_h);
		kfree(rsz_config);
		return;
	}

	reg_val = 0;
	reg_val |= REG_FLD_VAL(FLD_RSZ_HORIZONTAL_EN, 1);
	reg_val |= REG_FLD_VAL(FLD_RSZ_VERTICAL_EN, 1);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_CONTROL_1, reg_val, ~0);
	DDPDBG("%s:CONTROL_1:0x%x\n", __func__, reg_val);

	reg_val = mtk_rsz_set_color_format(fmt);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_CONTROL_2, reg_val, ~0);
	DDPDBG("%s:CONTROL_2:0x%x\n", __func__, reg_val);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_INPUT_IMAGE,
		       in_h << 16 | in_w, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_OUTPUT_IMAGE,
		       out_h << 16 | out_w, ~0);
	DDPDBG("%s:%s:(%ux%u)->(%ux%u)\n", __func__, mtk_dump_comp_str(comp),
	       in_w, in_h, out_w, out_h);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_HORIZONTAL_COEFF_STEP,
		       rsz_config->tw[tile_idx].step, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_VERTICAL_COEFF_STEP,
		       rsz_config->th[0].step, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa +
			       DISP_REG_RSZ_LUMA_HORIZONTAL_INTEGER_OFFSET,
		       rsz_config->tw[tile_idx].int_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa +
			       DISP_REG_RSZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET,
		       rsz_config->tw[tile_idx].sub_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa +
			       DISP_REG_RSZ_LUMA_VERTICAL_INTEGER_OFFSET,
		       rsz_config->th[0].int_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa +
			       DISP_REG_RSZ_LUMA_VERTICAL_SUBPIXEL_OFFSET,
		       rsz_config->th[0].sub_offset, ~0);

	kfree(rsz_config);
}

static void mtk_rsz_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	struct mtk_addon_rsz_config config = addon_config->addon_rsz_config;
	struct mtk_rsz_config_struct *rsz_config = NULL;
	struct mtk_disp_rsz *rsz = comp_to_rsz(comp);
	enum mtk_rsz_color_format fmt = ARGB2101010;
	bool tile_mode = false;
	u32 reg_val = 0;
	u32 tile_idx = 0;
	u32 in_w = 0, in_h = 0, out_w = 0, out_h = 0;

	rsz_config = kzalloc(sizeof(struct mtk_rsz_config_struct), GFP_KERNEL);
	if (!rsz_config) {
		DDPPR_ERR("fail to create rsz_config!\n");
		return;
	}

	rsz_config->frm_in_w = config.rsz_src_roi.width;
	rsz_config->frm_in_h = config.rsz_src_roi.height;
	rsz_config->frm_out_w = config.rsz_dst_roi.width;
	rsz_config->frm_out_h = config.rsz_dst_roi.height;

	if (mtk_rsz_check_params(rsz_config, rsz->data->tile_length)) {
		kfree(rsz_config);
		return;
	}

	if (comp->mtk_crtc->is_dual_pipe) {
		rsz_config->tw[tile_idx].in_len =
			addon_config->addon_rsz_config.rsz_param.in_len;
		rsz_config->tw[tile_idx].out_len =
			addon_config->addon_rsz_config.rsz_param.out_len;
		rsz_config->tw[tile_idx].step =
			addon_config->addon_rsz_config.rsz_param.step;
		rsz_config->tw[tile_idx].int_offset =
			addon_config->addon_rsz_config.rsz_param.int_offset;
		rsz_config->tw[tile_idx].sub_offset =
			addon_config->addon_rsz_config.rsz_param.sub_offset;
	} else {
		mtk_rsz_calc_tile_params(rsz_config->frm_in_w, rsz_config->frm_out_w,
					 tile_mode, rsz_config->tw);
	}
	mtk_rsz_calc_tile_params(rsz_config->frm_in_h, rsz_config->frm_out_h,
				 tile_mode, rsz_config->th);

	in_w = rsz_config->tw[tile_idx].in_len;
	in_h = rsz_config->th[0].in_len;
	out_w = rsz_config->tw[tile_idx].out_len;
	out_h = rsz_config->th[0].out_len;

	if (in_w > out_w || in_h > out_h) {
		DDPPR_ERR("DISP_RSZ only supports scale-up,(%ux%u)->(%ux%u)\n",
			  in_w, in_h, out_w, out_h);
		kfree(rsz_config);
		return;
	}

	reg_val = 0;
	reg_val |= REG_FLD_VAL(FLD_RSZ_HORIZONTAL_EN, 1);
	reg_val |= REG_FLD_VAL(FLD_RSZ_VERTICAL_EN, 1);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_CONTROL_1, reg_val, ~0);
	DDPDBG("%s:CONTROL_1:0x%x\n", __func__, reg_val);

	reg_val = mtk_rsz_set_color_format(fmt);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_CONTROL_2, reg_val, ~0);
	DDPDBG("%s:CONTROL_2:0x%x\n", __func__, reg_val);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_INPUT_IMAGE,
		       in_h << 16 | in_w, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_OUTPUT_IMAGE,
		       out_h << 16 | out_w, ~0);
	DDPDBG("%s:%s:(%ux%u)->(%ux%u)\n", __func__, mtk_dump_comp_str(comp),
	       in_w, in_h, out_w, out_h);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_HORIZONTAL_COEFF_STEP,
		       rsz_config->tw[tile_idx].step, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RSZ_VERTICAL_COEFF_STEP,
		       rsz_config->th[0].step, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa +
			       DISP_REG_RSZ_LUMA_HORIZONTAL_INTEGER_OFFSET,
		       rsz_config->tw[tile_idx].int_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa +
			       DISP_REG_RSZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET,
		       rsz_config->tw[tile_idx].sub_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa +
			       DISP_REG_RSZ_LUMA_VERTICAL_INTEGER_OFFSET,
		       rsz_config->th[0].int_offset, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa +
			       DISP_REG_RSZ_LUMA_VERTICAL_SUBPIXEL_OFFSET,
		       rsz_config->th[0].sub_offset, ~0);

	kfree(rsz_config);
}

int mtk_rsz_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i = 0;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	DDPDUMP("== DISP %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	for (i = 0; i < 3; i++) {
		DDPDUMP("0x%03X: 0x%08x 0x%08x 0x%08x 0x%08x\n", i * 0x10,
			readl(baddr + i * 0x10), readl(baddr + i * 0x10 + 0x4),
			readl(baddr + i * 0x10 + 0x8),
			readl(baddr + i * 0x10 + 0xC));
	}
	DDPDUMP("0x044: 0x%08x 0x%08x; 0x0F0: 0x%08x\n", readl(baddr + 0x44),
		readl(baddr + 0x48), readl(baddr + 0xF0));

	return 0;
}

int mtk_rsz_analysis(struct mtk_ddp_comp *comp)
{
#define LEN 100
	void __iomem *baddr = comp->regs;
	u32 enable = 0;
	u32 con1 = 0;
	u32 con2 = 0;
	u32 int_flag = 0;
	u32 in_size = 0;
	u32 out_size = 0;
	u32 in_pos = 0;
	u32 shadow = 0;
	char msg[LEN];
	int n = 0;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	enable = readl(baddr + DISP_REG_RSZ_ENABLE);
	con1 = readl(baddr + DISP_REG_RSZ_CONTROL_1);
	con2 = readl(baddr + DISP_REG_RSZ_CONTROL_2);
	int_flag = readl(baddr + DISP_REG_RSZ_INT_FLAG);
	in_size = readl(baddr + DISP_REG_RSZ_INPUT_IMAGE);
	out_size = readl(baddr + DISP_REG_RSZ_OUTPUT_IMAGE);
	in_pos = readl(baddr + DISP_REG_RSZ_DEBUG);
	shadow = readl(baddr + DISP_REG_RSZ_SHADOW_CTRL);

	DDPDUMP("== DISP %s ANALYSIS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);

	writel(0x3, baddr + DISP_REG_RSZ_DEBUG_SEL);
	n = snprintf(msg, LEN,
		     "en:%d,rst:%d,h_en:%d,v_en:%d,h_table:%d,v_table:%d,",
		     REG_FLD_VAL_GET(FLD_RSZ_EN, enable),
		     REG_FLD_VAL_GET(FLD_RSZ_RST, enable),
		     REG_FLD_VAL_GET(FLD_RSZ_HORIZONTAL_EN, con1),
		     REG_FLD_VAL_GET(FLD_RSZ_VERTICAL_EN, con1),
		     REG_FLD_VAL_GET(FLD_RSZ_HORIZONTAL_TABLE_SELECT, con1),
		     REG_FLD_VAL_GET(FLD_RSZ_VERTICAL_TABLE_SELECT, con1));
	n += snprintf(msg + n, LEN - n, "dcm_dis:%d,int_en:%d,wclr_en:%d\n",
		      REG_FLD_VAL_GET(FLD_RSZ_DCM_DIS, con1),
		      REG_FLD_VAL_GET(FLD_RSZ_INTEN, con1),
		      REG_FLD_VAL_GET(FLD_RSZ_INT_WCLR_EN, con1));
	DDPDUMP("%s", msg);

	n = snprintf(msg, LEN,
		     "power_saving:%d,rgb_bit_mode:%d,frm_start:%d,frm_end:%d,",
		     REG_FLD_VAL_GET(FLD_RSZ_POWER_SAVING, con2),
		     REG_FLD_VAL_GET(FLD_RSZ_RGB_BIT_MODE, con2),
		     REG_FLD_VAL_GET(FLD_RSZ_FRAME_START, int_flag),
		     REG_FLD_VAL_GET(FLD_RSZ_FRAME_END, int_flag));
	n += snprintf(msg + n, LEN - n, "size_err:%d,sof_rst:%d\n",
		      REG_FLD_VAL_GET(FLD_RSZ_SIZE_ERR, int_flag),
		      REG_FLD_VAL_GET(FLD_RSZ_SOF_RESET, int_flag));
	DDPDUMP("%s", msg);

	n = snprintf(msg, LEN, "in(%ux%u),out(%ux%u),h_step:%d,v_step:%d\n",
		     REG_FLD_VAL_GET(FLD_RSZ_INPUT_IMAGE_W, in_size),
		     REG_FLD_VAL_GET(FLD_RSZ_INPUT_IMAGE_H, in_size),
		     REG_FLD_VAL_GET(FLD_RSZ_OUTPUT_IMAGE_W, out_size),
		     REG_FLD_VAL_GET(FLD_RSZ_OUTPUT_IMAGE_H, out_size),
		     readl(baddr + DISP_REG_RSZ_HORIZONTAL_COEFF_STEP),
		     readl(baddr + DISP_REG_RSZ_VERTICAL_COEFF_STEP));
	DDPDUMP("%s", msg);

	n = snprintf(
		msg, LEN, "luma_h:%d.%d,luma_v:%d.%d\n",
		readl(baddr + DISP_REG_RSZ_LUMA_HORIZONTAL_INTEGER_OFFSET),
		readl(baddr + DISP_REG_RSZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET),
		readl(baddr + DISP_REG_RSZ_LUMA_VERTICAL_INTEGER_OFFSET),
		readl(baddr + DISP_REG_RSZ_LUMA_VERTICAL_SUBPIXEL_OFFSET));
	DDPDUMP("%s", msg);

	n = snprintf(msg, LEN,
		     "dbg_sel:%d, in(%u,%u);shadow_ctrl:bypass:%d,force:%d,",
		     readl(baddr + DISP_REG_RSZ_DEBUG_SEL), in_pos & 0xFFFF,
		     (in_pos >> 16) & 0xFFFF,
		     REG_FLD_VAL_GET(FLD_RSZ_BYPASS_SHADOW, shadow),
		     REG_FLD_VAL_GET(FLD_RSZ_FORCE_COMMIT, shadow));
	n += snprintf(msg + n, LEN - n, "read_working:%d\n",
		      REG_FLD_VAL_GET(FLD_RSZ_READ_WRK_REG, shadow));
	DDPDUMP("%s", msg);

	writel(0x7, baddr + DISP_REG_RSZ_DEBUG_SEL);
	n = snprintf(msg, LEN - n, "in_total_data_cnt:%d",
		      readl(baddr + DISP_REG_RSZ_DEBUG));

	writel(0xB, baddr + DISP_REG_RSZ_DEBUG_SEL);
	n += snprintf(msg + n, LEN - n, ",out_total_data_cnt:%d\n",
		      readl(baddr + DISP_REG_RSZ_DEBUG));
	DDPDUMP("%s", msg);

	return 0;
}

static void mtk_rsz_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_rsz *rsz = comp_to_rsz(comp);

	mtk_ddp_comp_clk_prepare(comp);

	/* Bypass shadow register and read shadow register */
	if (rsz->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, RSZ_BYPASS_SHADOW,
			DISP_REG_RSZ_SHADOW_CTRL, RSZ_BYPASS_SHADOW);
	else
		mtk_ddp_write_mask_cpu(comp, 0,
			DISP_REG_RSZ_SHADOW_CTRL, RSZ_BYPASS_SHADOW);
}

static void mtk_rsz_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static int mtk_rsz_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd io_cmd, void *params)
{
	int ret = 0;
	struct mtk_disp_rsz *rsz = comp_to_rsz(comp);

	switch (io_cmd) {
	case RSZ_GET_TILE_LENGTH: {
		unsigned int *val = (unsigned int *)params;

		if (rsz && rsz->data) {
			*val = rsz->data->tile_length;
			DDPINFO("%s, tile_length[%u]\n", __func__, *val);
		} else
			ret = -1;
		break;
	}
	case RSZ_GET_IN_MAX_HEIGHT: {
		unsigned int *val = (unsigned int *)params;

		if (rsz && rsz->data) {
			*val = rsz->data->in_max_height;
			DDPINFO("%s, in_max_height[%u]\n", __func__, *val);
		} else
			ret = -1;
		break;
	}
	default:
		break;
	}

	return ret;
}

static const struct mtk_ddp_comp_funcs mtk_disp_rsz_funcs = {
	.start = mtk_rsz_start,
	.stop = mtk_rsz_stop,
	.addon_config = mtk_rsz_addon_config,
	.config = mtk_rsz_config,
	.config_overhead = mtk_disp_rsz_config_overhead,
	.prepare = mtk_rsz_prepare,
	.unprepare = mtk_rsz_unprepare,
	.io_cmd = mtk_rsz_io_cmd,
};

static int mtk_disp_rsz_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_rsz *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *private = drm_dev->dev_private;
	int ret;

	DDPFUNC();
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	private->rsz_in_max[0] = priv->data->tile_length;
	private->rsz_in_max[1] = priv->data->in_max_height;

	return 0;
}

static void mtk_disp_rsz_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_rsz *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_rsz_component_ops = {
	.bind = mtk_disp_rsz_bind, .unbind = mtk_disp_rsz_unbind,
};

static int mtk_disp_rsz_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_rsz *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_RSZ);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_rsz_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_rsz_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_rsz_remove(struct platform_device *pdev)
{
	struct mtk_disp_rsz *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_rsz_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_disp_rsz_data mt6779_rsz_driver_data = {
	.tile_length = 1088, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_rsz_data mt6885_rsz_driver_data = {
	.tile_length = 1440, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_rsz_data mt6873_rsz_driver_data = {
	.tile_length = 1440, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_rsz_data mt6853_rsz_driver_data = {
	.tile_length = 1088, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_rsz_data mt6833_rsz_driver_data = {
	.tile_length = 1088, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_rsz_data mt6879_rsz_driver_data = {
	.tile_length = 1440, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_rsz_data mt6855_rsz_driver_data = {
	.tile_length = 1668, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_rsz_data mt6983_rsz_driver_data = {
	.tile_length = 1440, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_rsz_data mt6895_rsz_driver_data = {
	.tile_length = 1952, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_rsz_data mt6886_rsz_driver_data = {
	.tile_length = 1300, .in_max_height = 4000,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_rsz_data mt6985_rsz_driver_data = {
	.tile_length = 1660, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_rsz_data mt6989_rsz_driver_data = {
	.tile_length = 1660, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_rsz_data mt6897_rsz_driver_data = {
	.tile_length = 1660, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_rsz_driver_dt_match[] = {
	{.compatible = "mediatek,mt6779-disp-rsz",
	 .data = &mt6779_rsz_driver_data},
	{.compatible = "mediatek,mt6885-disp-rsz",
	 .data = &mt6885_rsz_driver_data},
	{.compatible = "mediatek,mt6873-disp-rsz",
	 .data = &mt6873_rsz_driver_data},
	{.compatible = "mediatek,mt6853-disp-rsz",
	 .data = &mt6853_rsz_driver_data},
	{.compatible = "mediatek,mt6833-disp-rsz",
	 .data = &mt6833_rsz_driver_data},
	{.compatible = "mediatek,mt6879-disp-rsz",
	 .data = &mt6879_rsz_driver_data},
	{.compatible = "mediatek,mt6983-disp-rsz",
	 .data = &mt6983_rsz_driver_data},
	{.compatible = "mediatek,mt6895-disp-rsz",
	 .data = &mt6895_rsz_driver_data},
	{.compatible = "mediatek,mt6886-disp-rsz",
	 .data = &mt6886_rsz_driver_data},
	{.compatible = "mediatek,mt6855-disp-rsz",
	 .data = &mt6855_rsz_driver_data},
	{.compatible = "mediatek,mt6985-disp-rsz",
	 .data = &mt6985_rsz_driver_data},
	{.compatible = "mediatek,mt6989-disp-rsz",
	 .data = &mt6989_rsz_driver_data},
	{.compatible = "mediatek,mt6897-disp-rsz",
	 .data = &mt6897_rsz_driver_data},
	{.compatible = "mediatek,mt6835-disp-rsz",
	 .data = &mt6835_rsz_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_rsz_driver_dt_match);

struct platform_driver mtk_disp_rsz_driver = {
	.probe = mtk_disp_rsz_probe,
	.remove = mtk_disp_rsz_remove,
	.driver = {

			.name = "mediatek-disp-rsz",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_rsz_driver_dt_match,
		},
};
