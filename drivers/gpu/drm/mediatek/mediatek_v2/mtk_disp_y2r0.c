// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_rect.h"
#include "mtk_drm_drv.h"
#include "../mml/mtk-mml-color.h"

#define MT6989_DISP_REG_DISP_Y2R0_EN   0x000
	#define MT6989_Y2R0_EN BIT(0)
#define MT6989_DISP_REG_DISP_Y2R0_RST  0x004
#define MT6989_DISP_REG_DISP_Y2R0_CFG  0x018
	#define MT6989_DISP_REG_DISP_Y2R0_CLAMP BIT(4)
	#define MT6989_DISP_REG_DISP_Y2R0_RELAY_MODE BIT(5)
	//#define MT6989_DISP_REG_DISP_Y2R0_LUT_EN BIT(6)
	//#define MT6989_DISP_REG_DISP_Y2R0_STALL_CG_ON BIT(7)
	//#define MT6989_DISP_REG_DISP_Y2R0_OP_8BIT_MODE BIT(8)
	#define MT6989_DISP_REG_DISP_Y2R0_CLAMP_U10 BIT(9)
#define MT6989_DISP_REG_DISP_Y2R0_SIZE 0x01C
#define MT6989_DISP_REG_DISP_Y2R0_1TNP 0x020
	#define MT6989_DISP_1T2P BIT(0)
#define MT6989_DISP_Y2R0_MATRIX_SEL_FULL_RANGE_BT601_RGB 0x4
#define MT6989_DISP_Y2R0_MATRIX_SEL_FULL_RANGE_BT709_RGB 0x5
#define MT6989_DISP_Y2R0_MATRIX_SEL_LIMIT_RANGE_BT601_RGB 0x6
#define MT6989_DISP_Y2R0_MATRIX_SEL_LIMIT_RANGE_BT709_RGB 0x7
#define MT6989_DISP_Y2R0_MATRIX_SEL_FULL_RANGE_BT2020_RGB 0x8
#define MT6989_DISP_Y2R0_MATRIX_SEL_LIMIT_RANGE_BT2020_RGB 0x9


#define DISP_REG_DISP_Y2R0_EN 0x250
#define DISP_REG_DISP_Y2R0_RST 0x254
#define DISP_REG_DISP_Y2R0_CON0 0x258
	#define DISP_REG_DISP_Y2R0_MATRIX_SEL_JPEG_RGB 0x4 //Bit 2 always on
	#define DISP_REG_DISP_Y2R0_MATRIX_SEL_FULL_RANGE_BT709_RGB 0x5
	#define DISP_REG_DISP_Y2R0_MATRIX_SEL_LIMIT_RANGE_BT601_RGB 0x6
	#define DISP_REG_DISP_Y2R0_MATRIX_SEL_LIMIT_RANGE_BT709_RGB 0x7
	#define DISP_REG_DISP_Y2R0_CLAMP BIT(4)
	#define DISP_REG_DISP_Y2R0_BYPASS_DATA_PROCESS_NOBYPASS BIT(5)

/**
 * struct mtk_disp_y2r - DISP_RSZ driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_y2r {
	struct mtk_ddp_comp ddp_comp;
};

void mtk_mt6989_y2r_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== DISP %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	DDPDUMP("0x000: 0x%08x 0x%08x 0x%08x 0x%08x\n", readl(baddr + 0x000),
		readl(baddr + 0x004), readl(baddr + 0x008), readl(baddr + 0x00C));
	DDPDUMP("0x010: 0x%08x 0x%08x 0x%08x 0x%08x\n", readl(baddr + 0x010),
		readl(baddr + 0x014), readl(baddr + 0x018), readl(baddr + 0x01C));
	DDPDUMP("0x020: 0x%08x 0x%08x 0x%08x 0x%08x\n", readl(baddr + 0x020),
		readl(baddr + 0x024), readl(baddr + 0x028), readl(baddr + 0x02C));
	DDPDUMP("0x030: 0x%08x\n", readl(baddr + 0x030));
}

static inline struct mtk_disp_y2r *comp_to_y2r(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_y2r, ddp_comp);
}

static void mtk_y2r_mt6989_config(struct mtk_drm_crtc *mtk_crtc,
				 struct mtk_ddp_comp *comp,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	unsigned int disp_y2r_cfg = 0;
	unsigned int mml_profile = 0;
	bool y2r_en = addon_config->addon_mml_config.y2r_en | g_y2r_en;

	if (mtk_crtc->mml_cfg) {
		int hsize = 0, vsize = 0;
		int profile = 5;
		mml_profile = mtk_crtc->mml_cfg->info.dest[0].data.profile;

		hsize = mtk_crtc->mml_cfg->dl_out[0].width & 0x1FFF;
		vsize = mtk_crtc->mml_cfg->dl_out[0].height & 0x1FFF;

		cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
				comp->regs_pa + MT6989_DISP_REG_DISP_Y2R0_SIZE,
				((hsize << 16) | vsize), ~0);
		DDPINFO("%s: p:%d,e:%d,h:%d,v:%d\n", __func__, mml_profile, y2r_en, hsize, vsize);

		/*follow mml output mml_ycbcr_profile*/
		switch (mml_profile) {
		case MML_YCBCR_PROFILE_FULL_BT601:
			profile = MT6989_DISP_Y2R0_MATRIX_SEL_FULL_RANGE_BT601_RGB;
			break;
		case MML_YCBCR_PROFILE_FULL_BT709:
			profile = MT6989_DISP_Y2R0_MATRIX_SEL_FULL_RANGE_BT709_RGB;
			break;
		case MML_YCBCR_PROFILE_BT601:
			profile = MT6989_DISP_Y2R0_MATRIX_SEL_LIMIT_RANGE_BT601_RGB;
			break;
		case MML_YCBCR_PROFILE_BT709:
			profile = MT6989_DISP_Y2R0_MATRIX_SEL_LIMIT_RANGE_BT709_RGB;
			break;
		case MML_YCBCR_PROFILE_FULL_BT2020:
			profile = MT6989_DISP_Y2R0_MATRIX_SEL_FULL_RANGE_BT2020_RGB;
			break;
		case MML_YCBCR_PROFILE_BT2020:
			profile = MT6989_DISP_Y2R0_MATRIX_SEL_LIMIT_RANGE_BT2020_RGB;
			break;
		default:
			profile = MT6989_DISP_Y2R0_MATRIX_SEL_FULL_RANGE_BT709_RGB;
			DDPMSG("unknown p:%d\n", mml_profile);
			break;
		}
		profile |= MT6989_DISP_REG_DISP_Y2R0_CLAMP;
		profile |= MT6989_DISP_REG_DISP_Y2R0_CLAMP_U10;
		disp_y2r_cfg = profile;

	} else {
		disp_y2r_cfg = MT6989_DISP_Y2R0_MATRIX_SEL_FULL_RANGE_BT709_RGB & 0xF;
	}

	if (!y2r_en) {	// relay
		cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
			comp->regs_pa + MT6989_DISP_REG_DISP_Y2R0_CFG,
			MT6989_DISP_REG_DISP_Y2R0_RELAY_MODE, ~0);
	} else {
		disp_y2r_cfg |= ~MT6989_DISP_REG_DISP_Y2R0_RELAY_MODE;
		DDPINFO("disp_y2r_cfg: 0x%x\n",disp_y2r_cfg);
		cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
			comp->regs_pa + MT6989_DISP_REG_DISP_Y2R0_CFG, disp_y2r_cfg, ~0);
	}

	cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
			comp->regs_pa + MT6989_DISP_REG_DISP_Y2R0_1TNP,
			MT6989_DISP_1T2P, MT6989_DISP_1T2P);
	cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
			comp->regs_pa + MT6989_DISP_REG_DISP_Y2R0_EN,
			MT6989_Y2R0_EN, MT6989_Y2R0_EN);

	DDPINFO("%s -\n", __func__);
}

static void mtk_y2r_config(struct mtk_drm_crtc *mtk_crtc,
				 struct mtk_ddp_comp *comp,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	DDPINFO("%s +\n", __func__);

	if (mtk_crtc->mml_cfg) {
		/*follow mml output prefile*/
		int profile = 5;

		switch (mtk_crtc->mml_cfg->info.dest[0].data.profile) {
		case MML_YCBCR_PROFILE_BT601:
			profile = DISP_REG_DISP_Y2R0_MATRIX_SEL_LIMIT_RANGE_BT601_RGB;
			break;
		case MML_YCBCR_PROFILE_BT709:
			profile = DISP_REG_DISP_Y2R0_MATRIX_SEL_LIMIT_RANGE_BT709_RGB;
			break;
		case MML_YCBCR_PROFILE_JPEG:
			profile = DISP_REG_DISP_Y2R0_MATRIX_SEL_JPEG_RGB;
			break;
		default:
			break;
		}
		profile |= DISP_REG_DISP_Y2R0_CLAMP;
		cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
				comp->regs_pa + DISP_REG_DISP_Y2R0_CON0, profile, ~0);
	} else
		cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
				comp->regs_pa + DISP_REG_DISP_Y2R0_CON0,
				DISP_REG_DISP_Y2R0_MATRIX_SEL_FULL_RANGE_BT709_RGB, ~0);

	cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
			comp->regs_pa + DISP_REG_DISP_Y2R0_EN,
			addon_config->addon_mml_config.is_yuv, ~0);

	DDPINFO("%s -\n", __func__);
}
static void mtk_y2r_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv = NULL;

	if (!mtk_crtc) {
		DDPINFO("%s mtk_crtc is not assigned\n", __func__);
		return;
	}

	priv = mtk_crtc->base.dev->dev_private;

	if (!mtk_crtc->is_force_mml_scen)
		return;

	if (addon_config->config_type.type == ADDON_DISCONNECT)
		return;
	if (priv->data->mmsys_id == MMSYS_MT6989)
		mtk_y2r_mt6989_config(mtk_crtc, comp,addon_config, handle);
	else
		mtk_y2r_config(mtk_crtc, comp,addon_config, handle);
}

void mtk_y2r_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv = NULL;

	priv = mtk_crtc->base.dev->dev_private;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}
	if (priv->data->mmsys_id == MMSYS_MT6989)
		mtk_mt6989_y2r_dump(comp);
	else {
		DDPDUMP("== DISP %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
		DDPDUMP("0x250: 0x%08x 0x%08x 0x%08x\n", readl(baddr + 0x250),
			readl(baddr + 0x254), readl(baddr + 0x258));
	}
}

int mtk_y2r_analysis(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	return 0;
}

static void mtk_y2r_discrete_config(struct mtk_ddp_comp *comp,
		unsigned int idx, struct mtk_plane_state *state, struct cmdq_pkt *handle)
{
	struct mtk_plane_pending_state *pending = &state->pending;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	unsigned int width = pending->width;
	unsigned int height = pending->height;
	struct mtk_drm_private *priv = NULL;

	priv = mtk_crtc->base.dev->dev_private;

	if (priv->data->mmsys_id == MMSYS_MT6989) {
		if (idx == 0)
			//plane 0 config, need to clear prev atomic commit frame done event
			mtk_crtc_clr_comp_done(mtk_crtc, handle, comp, 0);
		else
			//plane n need to wait prev plane n-1 frame done event
			mtk_crtc_wait_comp_done(mtk_crtc, handle, comp, 0);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + MT6989_DISP_REG_DISP_Y2R0_SIZE,
			((width << 16) | height), ~0);
	}
}

static void mtk_y2r_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv = NULL;

	priv = mtk_crtc->base.dev->dev_private;
	DDPDBG("%s, comp->regs_pa=0x%llx\n", __func__, comp->regs_pa);
	if (priv->data->mmsys_id == MMSYS_MT6989) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + MT6989_DISP_REG_DISP_Y2R0_EN,
				MT6989_Y2R0_EN, MT6989_Y2R0_EN);
	}
}

static void mtk_y2r_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv = NULL;

	priv = mtk_crtc->base.dev->dev_private;
	DDPDBG("%s, comp->regs_pa=0x%llx\n", __func__, comp->regs_pa);
	if (priv->data->mmsys_id == MMSYS_MT6989)
		cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + MT6989_DISP_REG_DISP_Y2R0_EN,
				0, MT6989_Y2R0_EN);
}

static void mtk_y2r_prepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s, comp->regs_pa=0x%llx\n", __func__, comp->regs_pa);
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_y2r_unprepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s, comp->regs_pa=0x%llx\n", __func__, comp->regs_pa);
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_y2r_funcs = {
	.start = mtk_y2r_start,
	.stop = mtk_y2r_stop,
	.addon_config = mtk_y2r_addon_config,
	.discrete_config = mtk_y2r_discrete_config,
	.prepare = mtk_y2r_prepare,
	.unprepare = mtk_y2r_unprepare,
};

static int mtk_disp_y2r_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_y2r *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s &priv->ddp_comp:0x%lx\n", __func__, (unsigned long)&priv->ddp_comp);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_y2r_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_y2r *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_y2r_component_ops = {
	.bind = mtk_disp_y2r_bind, .unbind = mtk_disp_y2r_unbind,
};

static int mtk_disp_y2r_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_y2r *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_Y2R);
	DDPINFO("comp_id:%d", comp_id);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_y2r_funcs);

	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	//priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	DDPINFO("&priv->ddp_comp:0x%lx", (unsigned long)&priv->ddp_comp);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_y2r_component_ops);

	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_y2r_remove(struct platform_device *pdev)
{
	struct mtk_disp_y2r *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_y2r_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct of_device_id mtk_disp_y2r_driver_dt_match[] = {
	{.compatible = "mediatek,mt6983-disp-y2r",},
	{.compatible = "mediatek,mt6895-disp-y2r",},
	{.compatible = "mediatek,mt6985-disp-y2r",},
	{.compatible = "mediatek,mt6989-disp-y2r",},
	{.compatible = "mediatek,mt6886-disp-y2r",},
	{.compatible = "mediatek,mt6897-disp-y2r",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_y2r_driver_dt_match);

struct platform_driver mtk_disp_y2r_driver = {
	.probe = mtk_disp_y2r_probe,
	.remove = mtk_disp_y2r_remove,
	.driver = {
		.name = "mediatek-disp-y2r",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_y2r_driver_dt_match,
	},
};
