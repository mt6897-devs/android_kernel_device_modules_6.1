# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_DRM_PANEL_NOVATEK_NT35695B_VDO) += panel-nt35695b-auo-vdo.o
obj-$(CONFIG_DRM_PANEL_NOVATEK_NT36672A_VDO) += panel-nt36672a-rt4801-vdo.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_ANA6705_CMD) += panel-samsung-ana6705-cmd.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_ANA6705_CMD_FHDP) += panel-samsung-ana6705-cmd-fhdp.o
obj-$(CONFIG_DRM_PANEL_HX_NT37701_DPHY_CMD) += panel-hx-nt37701-dphy-cmd.o
obj-$(CONFIG_DRM_PANEL_HX_NT37701_DPHY_CMD_120HZ) += panel-hx-nt37701-dphy-cmd-120hz.o
obj-$(CONFIG_DRM_PANEL_TRULY_TD4330_VDO) += panel-truly-td4330-vdo.o
obj-$(CONFIG_DRM_PANEL_TRULY_TD4330_CMD) += panel-truly-td4330-cmd.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_OP_CMD) += panel-samsung-op-cmd.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_OP_CMD_MSYNC2) += panel-samsung-op-cmd-msync2.o
obj-$(CONFIG_DRM_PANEL_TRULY_FT8756_VDO) += panel-truly-ft8756-vdo.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672C_CPHY_VDO) += panel-alpha-jdi-nt36672c-cphy-vdo.o
obj-$(CONFIG_DRM_PANEL_SC_NT36672C_VDO_120HZ) += panel-sc-nt36672c-vdo-120hz.o
obj-$(CONFIG_DRM_PANEL_TIANMA_R66451_CMD_120HZ) += panel-tianma-r66451-cmd-120hz.o
obj-$(CONFIG_DRM_PANEL_TIANMA_R66451_CMD_120HZ_WA) += panel-tianma-r66451-cmd-120hz-wa.o
obj-$(CONFIG_DRM_PANEL_TIANMA_NT36672E_VDO_120HZ_HFP) += panel-tianma-nt36672e-vdo-120hz-hfp.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_CPHY_VDO) += panel-alpha-jdi-nt36672e-cphy-vdo.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_120HZ) += panel-alpha-jdi-nt36672e-vdo-120hz.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_144HZ) += panel-alpha-jdi-nt36672e-vdo-144hz.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_60HZ) += panel-alpha-jdi-nt36672e-vdo-60hz.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_120HZ_HFP) += panel-alpha-jdi-nt36672e-vdo-120hz-hfp.o
obj-$(CONFIG_DRM_PANEL_ALPHA_JDI_NT36672E_VDO_144HZ_HFP) += panel-alpha-jdi-nt36672e-vdo-144hz-hfp.o
obj-$(CONFIG_DRM_PANEL_TRULY_NT35595_CMD) += panel-truly-nt35595-cmd.o
obj-$(CONFIG_DRM_PANEL_NT37801_MSYNC2_RTE) += panel-nt37801-msync2-rte.o
obj-$(CONFIG_DRM_PANEL_SAMSUNG_S68FC01_VDO_AOD) += panel-samsung-s68fc01-vdo-aod.o
obj-$(CONFIG_DRM_PANEL_ALPHA_SAMSUNG_S6E3HAE_CMD_120HZ) += panel-alpha-samsung-s6e3hae-fhd-dphy-cmd-120hz-r4831a.o
obj-$(CONFIG_DRM_PANEL_HIMAX_HX83121A_VDO) += panel-himax-hx83121a-vdo.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_LTPO) += panel-nt37801-cmd-ltpo.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_120HZ) += panel-nt37801-cmd-120hz.o
obj-$(CONFIG_DRM_PANEL_N11A_42_02_0A_DSC_VDO) += panel-n11a-42-02-0a-dsc-vdo.o
obj-$(CONFIG_DRM_PANEL_N11A_41_02_0B_DSC_VDO) += panel-n11a-41-02-0b-dsc-vdo.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_FHD) += panel-nt37801-cmd-fhd.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_SPR) += panel-nt37801-cmd-spr.o
obj-$(CONFIG_DRM_PANEL_NT37801_CMD_FHD_PLUS) += panel-nt37801-cmd-fhd-plus.o
obj-$(CONFIG_DRM_PANEL_ALPHA_NT37706_VDO_120HZ) += panel-alpha-nt37706-vdo-120hz.o
ifneq ($(wildcard $(srctree)/$(src)/k6985v1_64),)
obj-$(CONFIG_DRM_TD2204_WQHD_AMB678ZY01_S6E3HC3_CMD) += k6985v1_64/td2204-wqhd-amb678zy01-s6e3hc3-cmd.o
endif
ifneq ($(wildcard $(srctree)/$(src)/k6985v1_64_alpha),)
obj-$(CONFIG_DRM_PANEL_NT37705_ALPHA_CMD) += k6985v1_64_alpha/panel-nt37705-alpha-cmd.o
endif
ifneq ($(wildcard $(srctree)/$(src)/k6989v1_64_alpha),)
obj-$(CONFIG_DRM_PANEL_ILI7838E_ALPHA_CMD) += k6989v1_64_alpha/panel-ili7838e-alpha-cmd.o
obj-$(CONFIG_DRM_PANEL_ILI7838E_DV2_ALPHA_CMD) += k6989v1_64_alpha/panel-ili7838e-dv2-alpha-cmd.o
endif

mediatek-drm-gateic-y := mtk_drm_panel_i2c.o \
		  mtk_drm_gateic_rt4801h.o \
		  mtk_drm_gateic_rt4831a.o \
		  mtk_drm_gateic.o

mediatek-drm-panel-drv-y :=  \
		  mtk_round_corner/mtk_drm_rc.o \
		  mtk_drm_panel_helper.o \
		  mtk_drm_panel_helper_dbi.o \
		  mtk_drm_panel_helper_dpi.o \
		  mtk_drm_panel_helper_dsi.o \
		  mtk_drm_panel_drv_dbi.o \
		  mtk_drm_panel_drv_dpi.o \
		  mtk_drm_panel_drv_dsi.o \
		  mtk_drm_panel_drv.o

mediatek-cust-panel-sample-y :=  \
		  cust_panel/mtk_cust_panel_sample/mtk_drm_panel_cust.o \
		  cust_panel/cust_lcm_hal.o


obj-$(CONFIG_DRM_MEDIATEK_PANEL) += mediatek-drm-gateic.o
obj-$(CONFIG_DRM_MEDIATEK_PANEL) += mediatek-drm-panel-drv.o
obj-$(CONFIG_DRM_MEDIATEK_PANEL) += mediatek-cust-panel-sample.o

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)	\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/			\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp/			\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs/			\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/			\
             -I$(DEVICE_MODULES_PATH)/drivers/iommu/				\
             -I$(DEVICE_MODULES_PATH)/drivers/devfreq/				\
             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/				\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/leds/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/leds/$(MTK_PLATFORM) \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/clkbuf/v1/inc/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include/ \
             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/mi_disp/ \
             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/ \

ccflags-$(CONFIG_DRM_MEDIATEK_PANEL) += -I$(DEVICE_MODULES_PATH)/drivers/leds/
