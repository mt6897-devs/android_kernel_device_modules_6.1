// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Benjamin Chao <benjamin.chao@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6897-clk.h>

/* bringup config */
#define MT_CCF_BRINGUP		1
#define MT_CCF_PLL_DISABLE	0
#define MT_CCF_MUX_DISABLE	0

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1

/* TOPCK MUX SEL REG */
#define CLK_CFG_UPDATE				0x0004
#define CLK_CFG_UPDATE1				0x0008
#define CLK_CFG_UPDATE2				0x000c
#define VLP_CLK_CFG_UPDATE			0x0004
#define CLK_CFG_0				0x0010
#define CLK_CFG_0_SET				0x0014
#define CLK_CFG_0_CLR				0x0018
#define CLK_CFG_1				0x0020
#define CLK_CFG_1_SET				0x0024
#define CLK_CFG_1_CLR				0x0028
#define CLK_CFG_2				0x0030
#define CLK_CFG_2_SET				0x0034
#define CLK_CFG_2_CLR				0x0038
#define CLK_CFG_3				0x0040
#define CLK_CFG_3_SET				0x0044
#define CLK_CFG_3_CLR				0x0048
#define CLK_CFG_4				0x0050
#define CLK_CFG_4_SET				0x0054
#define CLK_CFG_4_CLR				0x0058
#define CLK_CFG_5				0x0060
#define CLK_CFG_5_SET				0x0064
#define CLK_CFG_5_CLR				0x0068
#define CLK_CFG_6				0x0070
#define CLK_CFG_6_SET				0x0074
#define CLK_CFG_6_CLR				0x0078
#define CLK_CFG_7				0x0080
#define CLK_CFG_7_SET				0x0084
#define CLK_CFG_7_CLR				0x0088
#define CLK_CFG_8				0x0090
#define CLK_CFG_8_SET				0x0094
#define CLK_CFG_8_CLR				0x0098
#define CLK_CFG_9				0x00A0
#define CLK_CFG_9_SET				0x00A4
#define CLK_CFG_9_CLR				0x00A8
#define CLK_CFG_10				0x00B0
#define CLK_CFG_10_SET				0x00B4
#define CLK_CFG_10_CLR				0x00B8
#define CLK_CFG_11				0x00C0
#define CLK_CFG_11_SET				0x00C4
#define CLK_CFG_11_CLR				0x00C8
#define CLK_CFG_12				0x00D0
#define CLK_CFG_12_SET				0x00D4
#define CLK_CFG_12_CLR				0x00D8
#define CLK_CFG_13				0x00E0
#define CLK_CFG_13_SET				0x00E4
#define CLK_CFG_13_CLR				0x00E8
#define CLK_CFG_14				0x00F0
#define CLK_CFG_14_SET				0x00F4
#define CLK_CFG_14_CLR				0x00F8
#define CLK_CFG_15				0x0100
#define CLK_CFG_15_SET				0x0104
#define CLK_CFG_15_CLR				0x0108
#define CLK_CFG_16				0x0110
#define CLK_CFG_16_SET				0x0114
#define CLK_CFG_16_CLR				0x0118
#define CLK_CFG_17				0x0120
#define CLK_CFG_17_SET				0x0124
#define CLK_CFG_17_CLR				0x0128
#define CLK_CFG_18				0x0130
#define CLK_CFG_18_SET				0x0134
#define CLK_CFG_18_CLR				0x0138
#define CLK_CFG_19				0x0140
#define CLK_CFG_19_SET				0x0144
#define CLK_CFG_19_CLR				0x0148
#define CLK_CFG_20				0x0150
#define CLK_CFG_20_SET				0x0154
#define CLK_CFG_20_CLR				0x0158
#define CLK_CFG_21				0x0160
#define CLK_CFG_21_SET				0x0164
#define CLK_CFG_21_CLR				0x0168
#define CLK_CFG_30				0x01f0
#define CLK_CFG_30_SET				0x01f4
#define CLK_CFG_30_CLR				0x01f8
#define CLK_AUDDIV_0				0x0320
#define VLP_CLK_CFG_0				0x0010
#define VLP_CLK_CFG_0_SET				0x0014
#define VLP_CLK_CFG_0_CLR				0x0018
#define VLP_CLK_CFG_1				0x0020
#define VLP_CLK_CFG_1_SET				0x0024
#define VLP_CLK_CFG_1_CLR				0x0028
#define VLP_CLK_CFG_2				0x0030
#define VLP_CLK_CFG_2_SET				0x0034
#define VLP_CLK_CFG_2_CLR				0x0038
#define VLP_CLK_CFG_3				0x0040
#define VLP_CLK_CFG_3_SET				0x0044
#define VLP_CLK_CFG_3_CLR				0x0048
#define VLP_CLK_CFG_4				0x0050
#define VLP_CLK_CFG_4_SET				0x0054
#define VLP_CLK_CFG_4_CLR				0x0058
#define VLP_CLK_CFG_5				0x0060
#define VLP_CLK_CFG_5_SET				0x0064
#define VLP_CLK_CFG_5_CLR				0x0068

/* TOPCK MUX SHIFT */
#define TOP_MUX_AXI_SHIFT			0
#define TOP_MUX_PERI_FAXI_SHIFT			1
#define TOP_MUX_UFS_FAXI_SHIFT			2
#define TOP_MUX_BUS_AXIMEM_SHIFT		3
#define TOP_MUX_DISP0_SHIFT			4
#define TOP_MUX_DISP1_SHIFT			5
#define TOP_MUX_OVL0_SHIFT			6
#define TOP_MUX_OVL1_SHIFT			7
#define TOP_MUX_MDP0_SHIFT			8
#define TOP_MUX_MDP1_SHIFT			9
#define TOP_MUX_MMINFRA_SHIFT			10
#define TOP_MUX_DSP_SHIFT			11
#define TOP_MUX_MFG_REF_SHIFT			12
#define TOP_MUX_MFGSC_REF_SHIFT			13
#define TOP_MUX_CAMTG2_SHIFT			14
#define TOP_MUX_CAMTG3_SHIFT			15
#define TOP_MUX_CAMTG4_SHIFT			16
#define TOP_MUX_CAMTG5_SHIFT			17
#define TOP_MUX_CAMTG6_SHIFT			18
#define TOP_MUX_CAMTG7_SHIFT			19
#define TOP_MUX_CAMTG8_SHIFT			20
#define TOP_MUX_UART_SHIFT			21
#define TOP_MUX_MSDC_MACRO_1P_SHIFT		22
#define TOP_MUX_MSDC_MACRO_2P_SHIFT		23
#define TOP_MUX_MSDC30_1_SHIFT			24
#define TOP_MUX_MSDC30_2_SHIFT			25
#define TOP_MUX_AUD_INTBUS_SHIFT		26
#define TOP_MUX_ATB_SHIFT			27
#define TOP_MUX_DP_SHIFT			28
#define TOP_MUX_DISP_PWM_SHIFT			29
#define TOP_MUX_USB_TOP_SHIFT			30
#define TOP_MUX_SSUSB_XHCI_SHIFT		0
#define TOP_MUX_I2C_SHIFT			1
#define TOP_MUX_SENINF_SHIFT			2
#define TOP_MUX_SENINF1_SHIFT			3
#define TOP_MUX_SENINF2_SHIFT			4
#define TOP_MUX_SENINF3_SHIFT			5
#define TOP_MUX_SENINF4_SHIFT			6
#define TOP_MUX_SENINF5_SHIFT			7
#define TOP_MUX_AUD_ENGEN1_SHIFT		9
#define TOP_MUX_AUD_ENGEN2_SHIFT		10
#define TOP_MUX_AES_UFSFDE_SHIFT		11
#define TOP_MUX_UFS_SHIFT			12
#define TOP_MUX_PEXTP_MBIST_SHIFT		14
#define TOP_MUX_AUD_1_SHIFT			15
#define TOP_MUX_AUD_2_SHIFT			16
#define TOP_MUX_ADSP_SHIFT			17
#define TOP_MUX_AUDIO_LOCAL_BUS_SHIFT		18
#define TOP_MUX_DPMAIF_MAIN_SHIFT		19
#define TOP_MUX_VENC_SHIFT			20
#define TOP_MUX_VDEC_SHIFT			21
#define TOP_MUX_PWM_SHIFT			22
#define TOP_MUX_AUDIO_H_SHIFT			23
#define TOP_MUX_MCUPM_SHIFT			24
#define TOP_MUX_MEM_SUB_SHIFT			25
#define TOP_MUX_PERI_FMEM_SUB_SHIFT		26
#define TOP_MUX_UFS_FMEM_SUB_SHIFT		27
#define TOP_MUX_EMI_N_SHIFT			28
#define TOP_MUX_EMI_S_SHIFT			29
#define TOP_MUX_CCU_AHB_SHIFT			30
#define TOP_MUX_AP2CONN_HOST_SHIFT		0
#define TOP_MUX_IMG1_SHIFT			1
#define TOP_MUX_IPE_SHIFT			2
#define TOP_MUX_MCU_ACP_SHIFT			3
#define TOP_MUX_MCU_L3GIC_SHIFT			4
#define TOP_MUX_MCU_INFRA_SHIFT			5
#define TOP_MUX_TL_SHIFT			7
#define TOP_MUX_PEXTP_FAXI_SHIFT		8
#define TOP_MUX_PEXTP_FMEM_SUB_SHIFT		9
#define TOP_MUX_EMI_INTERFACE_546_SHIFT		10
#define TOP_MUX_SPI0_SHIFT			11
#define TOP_MUX_SPI1_SHIFT			12
#define TOP_MUX_SPI2_SHIFT			13
#define TOP_MUX_SPI3_SHIFT			14
#define TOP_MUX_SPI4_SHIFT			15
#define TOP_MUX_SPI5_SHIFT			16
#define TOP_MUX_SPI6_SHIFT			17
#define TOP_MUX_SPI7_SHIFT			18
#define TOP_MUX_MMUP_SHIFT			19
#define TOP_MUX_DBGAO_26M_SHIFT			20
#define TOP_MUX_CAM_SHIFT			22
#define TOP_MUX_CAMTM_SHIFT			23
#define TOP_MUX_DPE_SHIFT			24
#define TOP_MUX_SCP_SHIFT			0
#define TOP_MUX_SCP_SPI_SHIFT			1
#define TOP_MUX_SCP_IIC_SHIFT			2
#define TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT		3
#define TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT		4
#define TOP_MUX_PWRAP_ULPOSC_SHIFT		5
#define TOP_MUX_SPMI_M_MST_SHIFT		7
#define TOP_MUX_SPMI_P_MST_SHIFT		8
#define TOP_MUX_DVFSRC_SHIFT			9
#define TOP_MUX_PWM_VLP_SHIFT			10
#define TOP_MUX_AXI_VLP_SHIFT			11
#define TOP_MUX_SYSTIMER_26M_SHIFT		12
#define TOP_MUX_SSPM_SHIFT			13
#define TOP_MUX_SRCK_SHIFT			14
#define TOP_MUX_SRAMRC_SHIFT			15
#define TOP_MUX_CAMTG_SHIFT			16
#define TOP_MUX_IPS_SHIFT			17
#define TOP_MUX_ULPOSC_SSPM_SHIFT		19
#define TOP_MUX_CCUSYS_SHIFT			20
#define TOP_MUX_CCUTM_SHIFT			21

/* TOPCK DIVIDER REG */
#define CLK_AUDDIV_2				0x0328
#define CLK_AUDDIV_3				0x0334
#define CLK_AUDDIV_4				0x0338
#define CLK_AUDDIV_5				0x033C

/* APMIXED PLL REG */
#define AP_PLL_CON3				0x00C
#define APLL1_TUNER_CON0			0x040
#define APLL2_TUNER_CON0			0x044
#define MAINPLL_CON0				0x350
#define MAINPLL_CON1				0x354
#define MAINPLL_CON2				0x358
#define MAINPLL_CON3				0x35C
#define UNIVPLL_CON0				0x308
#define UNIVPLL_CON1				0x30C
#define UNIVPLL_CON2				0x310
#define UNIVPLL_CON3				0x314
#define MSDCPLL_CON0				0x360
#define MSDCPLL_CON1				0x364
#define MSDCPLL_CON2				0x368
#define MSDCPLL_CON3				0x36C
#define MMPLL_CON0				0x3A0
#define MMPLL_CON1				0x3A4
#define MMPLL_CON2				0x3A8
#define MMPLL_CON3				0x3AC
#define ADSPPLL_CON0				0x380
#define ADSPPLL_CON1				0x384
#define ADSPPLL_CON2				0x388
#define ADSPPLL_CON3				0x38C
#define TVDPLL_CON0				0x248
#define TVDPLL_CON1				0x24C
#define TVDPLL_CON2				0x250
#define TVDPLL_CON3				0x254
#define APLL1_CON0				0x328
#define APLL1_CON1				0x32C
#define APLL1_CON2				0x330
#define APLL1_CON3				0x334
#define APLL1_CON4				0x338
#define APLL2_CON0				0x33C
#define APLL2_CON1				0x340
#define APLL2_CON2				0x344
#define APLL2_CON3				0x348
#define APLL2_CON4				0x34C
#define MPLL_CON0				0x390
#define MPLL_CON1				0x394
#define MPLL_CON2				0x398
#define MPLL_CON3				0x39C
#define EMIPLL_CON0				0x3B0
#define EMIPLL_CON1				0x3B4
#define EMIPLL_CON2				0x3B8
#define EMIPLL_CON3				0x3BC
#define IMGPLL_CON0				0x370
#define IMGPLL_CON1				0x374
#define IMGPLL_CON2				0x378
#define IMGPLL_CON3				0x37C
#define MFGPLL_CON0				0x008
#define MFGPLL_CON1				0x00C
#define MFGPLL_CON2				0x010
#define MFGPLL_CON3				0x014
#define MFGSCPLL_CON0				0x008
#define MFGSCPLL_CON1				0x00C
#define MFGSCPLL_CON2				0x010
#define MFGSCPLL_CON3				0x014
#define CCIPLL_CON0				0x008
#define CCIPLL_CON1				0x00C
#define CCIPLL_CON2				0x010
#define CCIPLL_CON3				0x014
#define ARMPLL_LL_CON0				0x008
#define ARMPLL_LL_CON1				0x00C
#define ARMPLL_LL_CON2				0x010
#define ARMPLL_LL_CON3				0x014
#define ARMPLL_BL_CON0				0x008
#define ARMPLL_BL_CON1				0x00C
#define ARMPLL_BL_CON2				0x010
#define ARMPLL_BL_CON3				0x014
#define ARMPLL_B_CON0				0x008
#define ARMPLL_B_CON1				0x00C
#define ARMPLL_B_CON2				0x010
#define ARMPLL_B_CON3				0x014
#define PTPPLL_CON0				0x008
#define PTPPLL_CON1				0x00C
#define PTPPLL_CON2				0x010
#define PTPPLL_CON3				0x014

/* HW Voter REG */
#define HW_CCF_CG_0_SET				0x0000
#define HW_CCF_CG_0_CLR				0x0004
#define HW_CCF_CG_0_DONE			0x1C00
#define HW_CCF_CG_1_SET				0x0008
#define HW_CCF_CG_1_CLR				0x000C
#define HW_CCF_CG_1_DONE			0x1C04
#define HW_CCF_CG_2_SET				0x0010
#define HW_CCF_CG_2_CLR				0x0014
#define HW_CCF_CG_2_DONE			0x1C08
#define HW_CCF_CG_3_SET				0x0018
#define HW_CCF_CG_3_CLR				0x001C
#define HW_CCF_CG_3_DONE			0x1C0C
#define HW_CCF_CG_4_SET				0x0020
#define HW_CCF_CG_4_CLR				0x0024
#define HW_CCF_CG_4_DONE			0x1C10
#define HW_CCF_CG_5_SET				0x0028
#define HW_CCF_CG_5_CLR				0x002C
#define HW_CCF_CG_5_DONE			0x1C14
#define HW_CCF_CG_6_SET				0x0030
#define HW_CCF_CG_6_CLR				0x0034
#define HW_CCF_CG_6_DONE			0x1C18
#define HW_CCF_CG_7_SET				0x0038
#define HW_CCF_CG_7_CLR				0x003C
#define HW_CCF_CG_7_DONE			0x1C1C
#define HW_CCF_CG_8_SET				0x0040
#define HW_CCF_CG_8_CLR				0x0044
#define HW_CCF_CG_8_DONE			0x1C20
#define HW_CCF_CG_9_SET				0x0048
#define HW_CCF_CG_9_CLR				0x004C
#define HW_CCF_CG_9_DONE			0x1C24
#define HW_CCF_CG_10_SET			0x0050
#define HW_CCF_CG_10_CLR			0x0054
#define HW_CCF_CG_10_DONE			0x1C28
#define HW_CCF_CG_11_SET			0x0058
#define HW_CCF_CG_11_CLR			0x005C
#define HW_CCF_CG_11_DONE			0x1C2C
#define HW_CCF_CG_12_SET			0x0060
#define HW_CCF_CG_12_CLR			0x0064
#define HW_CCF_CG_12_DONE			0x1C30
#define HW_CCF_CG_13_SET			0x0068
#define HW_CCF_CG_13_CLR			0x006C
#define HW_CCF_CG_13_DONE			0x1C34
#define HW_CCF_CG_14_SET			0x0070
#define HW_CCF_CG_14_CLR			0x0074
#define HW_CCF_CG_14_DONE			0x1C38
#define HW_CCF_CG_15_SET			0x0078
#define HW_CCF_CG_15_CLR			0x007C
#define HW_CCF_CG_15_DONE			0x1C3C
#define HW_CCF_CG_16_SET			0x0080
#define HW_CCF_CG_16_CLR			0x0084
#define HW_CCF_CG_16_DONE			0x1C40
#define HW_CCF_CG_17_SET			0x0088
#define HW_CCF_CG_17_CLR			0x008C
#define HW_CCF_CG_17_DONE			0x1C44
#define HW_CCF_CG_18_SET			0x0090
#define HW_CCF_CG_18_CLR			0x0094
#define HW_CCF_CG_18_DONE			0x1C48
#define HW_CCF_CG_19_SET			0x0098
#define HW_CCF_CG_19_CLR			0x009C
#define HW_CCF_CG_19_DONE			0x1C4C
#define HW_CCF_CG_20_SET			0x00A0
#define HW_CCF_CG_20_CLR			0x00A4
#define HW_CCF_CG_20_DONE			0x1C50
#define HW_CCF_CG_21_SET			0x00A8
#define HW_CCF_CG_21_CLR			0x00AC
#define HW_CCF_CG_21_DONE			0x1C54
#define HW_CCF_CG_22_SET			0x00B0
#define HW_CCF_CG_22_CLR			0x00B4
#define HW_CCF_CG_22_DONE			0x1C58
#define HW_CCF_CG_23_SET			0x00B8
#define HW_CCF_CG_23_CLR			0x00BC
#define HW_CCF_CG_23_DONE			0x1C5C
#define HW_CCF_CG_24_SET			0x00C0
#define HW_CCF_CG_24_CLR			0x00C4
#define HW_CCF_CG_24_DONE			0x1C60
#define HW_CCF_CG_25_SET			0x00C8
#define HW_CCF_CG_25_CLR			0x00CC
#define HW_CCF_CG_25_DONE			0x1C64
#define HW_CCF_CG_26_SET			0x00D0
#define HW_CCF_CG_26_CLR			0x00D4
#define HW_CCF_CG_26_DONE			0x1C68
#define HWV_PLL_SET				0x0190
#define HWV_PLL_CLR				0x0194
#define HWV_PLL_SET_STA			0x1464
#define HWV_PLL_CLR_STA			0x1468
#define HWV_PLL_DONE				0x140C

static DEFINE_SPINLOCK(mt6897_clk_lock);

static const struct mtk_fixed_factor vlp_ck_divs[] = {
	FACTOR(CLK_VLP_CK_SCP, "vlp_scp_ck",
			"vlp_scp_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SCP_SPI, "vlp_scp_spi_ck",
			"vlp_scp_spi_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SCP_IIC, "vlp_scp_iic_ck",
			"vlp_scp_iic_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SCP_SPI_HIGH_SPD, "vlp_scp_spi_hspd_ck",
			"vlp_scp_spi_hspd_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SCP_IIC_HIGH_SPD, "vlp_scp_iic_hspd_ck",
			"vlp_scp_iic_hspd_sel", 1, 1),
	FACTOR(CLK_VLP_CK_PWRAP_ULPOSC, "vlp_pwrap_ulposc_ck",
			"vlp_pwrap_ulposc_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SPMI_M_MST, "vlp_spmi_m_ck",
			"vlp_spmi_m_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SPMI_P_MST, "vlp_spmi_p_ck",
			"vlp_spmi_p_sel", 1, 1),
	FACTOR(CLK_VLP_CK_DVFSRC, "vlp_dvfsrc_ck",
			"vlp_dvfsrc_sel", 1, 1),
	FACTOR(CLK_VLP_CK_PWM_VLP, "vlp_pwm_vlp_ck",
			"vlp_pwm_vlp_sel", 1, 1),
	FACTOR(CLK_VLP_CK_AXI_VLP, "vlp_axi_vlp_ck",
			"vlp_axi_vlp_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SYSTIMER_26M, "vlp_systimer_26m_ck",
			"vlp_systimer_26m_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SSPM, "vlp_sspm_ck",
			"vlp_sspm_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SRCK, "vlp_srck_ck",
			"vlp_srck_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SRAMRC, "vlp_sramrc_ck",
			"vlp_sramrc_sel", 1, 1),
	FACTOR(CLK_VLP_CK_CAMTG, "vlp_camtg_ck",
			"vlp_camtg_sel", 1, 1),
	FACTOR(CLK_VLP_CK_IPS, "vlp_ips_ck",
			"vlp_ips_sel", 1, 1),
	FACTOR(CLK_VLP_CK_F26M_SSPM, "vlp_f26m_sspm_ck",
			"vlp_26m_sspm_sel", 1, 1),
	FACTOR(CLK_VLP_CK_ULPOSC_SSPM, "vlp_ulposc_sspm_ck",
			"vlp_ulposc_sspm_sel", 1, 1),
	FACTOR(CLK_VLP_CK_CCUSYS, "vlp_ccusys_ck",
			"vlp_ccusys_sel", 1, 1),
	FACTOR(CLK_VLP_CK_CCUTM, "vlp_ccutm_ck",
			"vlp_ccutm_sel", 1, 1),
	FACTOR(CLK_VLP_CK_APUSYS_26M, "vlp_apusys_26m_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_VLP_CK_INFRA_26M, "vlp_infra_26m_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_VLP_CK_F32K_SSPM, "vlp_f32k_sspm_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_SYSTIMER_32K, "vlp_systimer_32k_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_SRC_32K_TEST, "vlp_src_32k_test_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_SPMI_M_MST_32K, "vlp_spmi_m_32k_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_SPMI_P_MST_32K, "vlp_spmi_p_32k_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_KP_32K, "vlp_kp_32k_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_APXGPT_32K, "vlp_apxgpt_32k_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_APEINT_32K, "vlp_apeint_32k_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_MD_OSC_26M_VLP, "vlp_md_26m_vlp_ck",
			"osc_d20", 1, 1),
	FACTOR(CLK_VLP_CK_VLP_32K_COM, "vlp_vlp_32k_com_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_VLP_26M_COM, "vlp_vlp_26m_com_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_VLP_CK_SPM, "vlp_spm_ck",
			"mainpll_d7_d4", 1, 1),
	FACTOR(CLK_VLP_CK_RTC32K_CK_NO_SCAN, "vlp_rtc32k_no_scan",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_SYS26M_CK_NO_SCAN, "vlp_sys26m_no_scan",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_VLP_CK_OSC26M_CK_NO_SCAN, "vlp_osc26m_no_scan",
			"osc_d20", 1, 1),
	FACTOR(CLK_VLP_CK_PCIE_LP_REF, "vlp_pcie_lp_ref_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_VLP_CK_SCP_32K, "vlp_scp_32k_ck",
			"vlp_vlp_32k_com_ck", 1, 1),
	FACTOR(CLK_VLP_CK_SCP_26M, "vlp_scp_26m_ck",
			"vlp_vlp_26m_com_ck", 1, 1),
	FACTOR(CLK_VLP_CK_DPMSRCK, "vlp_dpmsrck_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_VLP_CK_DPMSRULP, "vlp_dpmsrulp_ck",
			"osc_d20", 1, 1),
	FACTOR(CLK_VLP_CK_DPMSRRTC, "vlp_dpmsrrtc_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_SYS_26M, "vlp_sys_26m_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_VLP_CK_U_SAP_CFG, "vlp_u_cfg_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_VLP_CK_U_TICK1US, "vlp_u_tick1us_ck",
			"clk26m", 1, 1),
};

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_MFGPLL, "mfgpll_ck",
			"mfg-ao-mfgpll", 1, 1),
	FACTOR(CLK_TOP_MFGSCPLL, "mfgscpll_ck",
			"mfgsc-ao-mfgscpll", 1, 1),
	FACTOR(CLK_TOP_MAINPLL_D3, "mainpll_d3",
			"mainpll", 1, 3),
	FACTOR(CLK_TOP_MAINPLL_D4, "mainpll_d4",
			"mainpll", 1, 4),
	FACTOR(CLK_TOP_MAINPLL_D4_D2, "mainpll_d4_d2",
			"mainpll", 1, 8),
	FACTOR(CLK_TOP_MAINPLL_D4_D4, "mainpll_d4_d4",
			"mainpll", 1, 16),
	FACTOR(CLK_TOP_MAINPLL_D4_D8, "mainpll_d4_d8",
			"mainpll", 43, 1375),
	FACTOR(CLK_TOP_MAINPLL_D5, "mainpll_d5",
			"mainpll", 1, 5),
	FACTOR(CLK_TOP_MAINPLL_D5_D2, "mainpll_d5_d2",
			"mainpll", 1, 10),
	FACTOR(CLK_TOP_MAINPLL_D5_D4, "mainpll_d5_d4",
			"mainpll", 1, 20),
	FACTOR(CLK_TOP_MAINPLL_D6, "mainpll_d6",
			"mainpll", 1, 6),
	FACTOR(CLK_TOP_MAINPLL_D6_D2, "mainpll_d6_d2",
			"mainpll", 1, 12),
	FACTOR(CLK_TOP_MAINPLL_D7, "mainpll_d7",
			"mainpll", 1, 7),
	FACTOR(CLK_TOP_MAINPLL_D7_D2, "mainpll_d7_d2",
			"mainpll", 1, 14),
	FACTOR(CLK_TOP_MAINPLL_D7_D4, "mainpll_d7_d4",
			"mainpll", 1, 28),
	FACTOR(CLK_TOP_MAINPLL_D7_D8, "mainpll_d7_d8",
			"mainpll", 1, 56),
	FACTOR(CLK_TOP_MAINPLL_D9, "mainpll_d9",
			"mainpll", 1, 9),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3",
			"univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL_D4, "univpll_d4",
			"univpll", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D4_D2, "univpll_d4_d2",
			"univpll", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_D4_D4, "univpll_d4_d4",
			"univpll", 1, 16),
	FACTOR(CLK_TOP_UNIVPLL_D4_D8, "univpll_d4_d8",
			"univpll", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5",
			"univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL_D5_D2, "univpll_d5_d2",
			"univpll", 1, 10),
	FACTOR(CLK_TOP_UNIVPLL_D5_D4, "univpll_d5_d4",
			"univpll", 1, 20),
	FACTOR(CLK_TOP_UNIVPLL_D6, "univpll_d6",
			"univpll", 1, 6),
	FACTOR(CLK_TOP_UNIVPLL_D6_D2, "univpll_d6_d2",
			"univpll", 1, 12),
	FACTOR(CLK_TOP_UNIVPLL_D6_D4, "univpll_d6_d4",
			"univpll", 1, 24),
	FACTOR(CLK_TOP_UNIVPLL_D6_D8, "univpll_d6_d8",
			"univpll", 1, 48),
	FACTOR(CLK_TOP_UNIVPLL_D6_D16, "univpll_d6_d16",
			"univpll", 1, 96),
	FACTOR(CLK_TOP_UNIVPLL_D7, "univpll_d7",
			"univpll", 1, 7),
	FACTOR(CLK_TOP_UNIVPLL_D7_D2, "univpll_d7_d2",
			"univpll", 1, 14),
	FACTOR(CLK_TOP_UNIVPLL_192M_D4, "univpll_192m_d4",
			"univpll", 1, 52),
	FACTOR(CLK_TOP_UNIVPLL_192M_D8, "univpll_192m_d8",
			"univpll", 1, 104),
	FACTOR(CLK_TOP_UNIVPLL_192M_D10, "univpll_192m_d10",
			"univpll", 1, 130),
	FACTOR(CLK_TOP_UNIVPLL_192M_D16, "univpll_192m_d16",
			"univpll", 1, 208),
	FACTOR(CLK_TOP_UNIVPLL_192M_D32, "univpll_192m_d32",
			"univpll", 1, 416),
	FACTOR(CLK_TOP_UNIVPLL_192M, "univpll_192m_ck",
			"univpll", 1, 13),
	FACTOR(CLK_TOP_USB20_PLL_D4, "usb20_pll_d4",
			"univpll", 1, 52),
	FACTOR(CLK_TOP_MPLL_208M, "mpll_208m_ck",
			"mpll", 1, 1),
	FACTOR(CLK_TOP_APLL1, "apll1_ck",
			"apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D2, "apll1_d2",
			"apll1", 1, 2),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4",
			"apll1", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "apll1_d8",
			"apll1", 1, 8),
	FACTOR(CLK_TOP_APLL2, "apll2_ck",
			"apll2", 1, 1),
	FACTOR(CLK_TOP_APLL2_D2, "apll2_d2",
			"apll2", 1, 2),
	FACTOR(CLK_TOP_APLL2_D4, "apll2_d4",
			"apll2", 1, 4),
	FACTOR(CLK_TOP_APLL2_D8, "apll2_d8",
			"apll2", 1, 8),
	FACTOR(CLK_TOP_CLK26M_BYP, "clk26m_byp",
			"apll2", 57, 431),
	FACTOR(CLK_TOP_ADSPPLL, "adsppll_ck",
			"adsppll", 1, 1),
	FACTOR(CLK_TOP_EMIPLL, "emipll_ck",
			"emipll", 1, 1),
	FACTOR(CLK_TOP_IMGPLL_D2, "imgpll_d2",
			"imgpll", 1, 2),
	FACTOR(CLK_TOP_IMGPLL_D5, "imgpll_d5",
			"imgpll", 1, 5),
	FACTOR(CLK_TOP_MMPLL_D3, "mmpll_d3",
			"mmpll", 1, 3),
	FACTOR(CLK_TOP_MMPLL_D4, "mmpll_d4",
			"mmpll", 1, 4),
	FACTOR(CLK_TOP_MMPLL_D4_D2, "mmpll_d4_d2",
			"mmpll", 1, 8),
	FACTOR(CLK_TOP_MMPLL_D5, "mmpll_d5",
			"mmpll", 1, 5),
	FACTOR(CLK_TOP_MMPLL_D5_D2, "mmpll_d5_d2",
			"mmpll", 1, 10),
	FACTOR(CLK_TOP_MMPLL_D6, "mmpll_d6",
			"mmpll", 1, 6),
	FACTOR(CLK_TOP_MMPLL_D6_D2, "mmpll_d6_d2",
			"mmpll", 1, 12),
	FACTOR(CLK_TOP_MMPLL_D7, "mmpll_d7",
			"mmpll", 1, 7),
	FACTOR(CLK_TOP_MMPLL_D9, "mmpll_d9",
			"mmpll", 1, 9),
	FACTOR(CLK_TOP_TVDPLL, "tvdpll_ck",
			"tvdpll", 1, 1),
	FACTOR(CLK_TOP_TVDPLL_D2, "tvdpll_d2",
			"tvdpll", 1, 2),
	FACTOR(CLK_TOP_TVDPLL_D4, "tvdpll_d4",
			"tvdpll", 1, 4),
	FACTOR(CLK_TOP_TVDPLL_D8, "tvdpll_d8",
			"tvdpll", 1, 8),
	FACTOR(CLK_TOP_TVDPLL_D16, "tvdpll_d16",
			"tvdpll", 92, 1473),
	FACTOR(CLK_TOP_MSDCPLL, "msdcpll_ck",
			"msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2",
			"msdcpll", 1, 2),
	FACTOR(CLK_TOP_CLKRTC, "clkrtc",
			"clk32k", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX8, "tck_26m_mx8_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX9, "tck_26m_mx9_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX10, "tck_26m_mx10_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX11, "tck_26m_mx11_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX12, "tck_26m_mx12_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_F26M_CK_D2, "f26m_d2",
			"clk26m", 1, 2),
	FACTOR(CLK_TOP_OSC, "osc_ck",
			"ulposc", 1, 1),
	FACTOR(CLK_TOP_OSC_D2, "osc_d2",
			"ulposc", 1, 2),
	FACTOR(CLK_TOP_OSC_D3, "osc_d3",
			"ulposc", 1, 3),
	FACTOR(CLK_TOP_OSC_D4, "osc_d4",
			"ulposc", 1, 4),
	FACTOR(CLK_TOP_OSC_D5, "osc_d5",
			"ulposc", 1, 5),
	FACTOR(CLK_TOP_OSC_D6, "osc_d6",
			"ulposc", 1, 6),
	FACTOR(CLK_TOP_OSC_D7, "osc_d7",
			"ulposc", 1, 7),
	FACTOR(CLK_TOP_OSC_D8, "osc_d8",
			"ulposc", 1, 8),
	FACTOR(CLK_TOP_OSC_D10, "osc_d10",
			"ulposc", 1, 10),
	FACTOR(CLK_TOP_OSC_D14, "osc_d14",
			"ulposc", 1, 14),
	FACTOR(CLK_TOP_OSC_D16, "osc_d16",
			"ulposc", 46, 735),
	FACTOR(CLK_TOP_OSC_D20, "osc_d20",
			"ulposc", 1, 20),
	FACTOR(CLK_TOP_OSC_D32, "osc_d32",
			"ulposc", 23, 735),
	FACTOR(CLK_TOP_OSC_D40, "osc_d40",
			"ulposc", 1, 40),
	FACTOR(CLK_TOP_OSC_CK_2, "osc_2",
			"ulposc", 1, 1),
	FACTOR(CLK_TOP_OSC2_D2, "osc2_d2",
			"ulposc", 42, 55),
	FACTOR(CLK_TOP_OSC2_D3, "osc2_d3",
			"ulposc", 28, 55),
	FACTOR(CLK_TOP_ULPOSC, "ulposc_ck",
			"None", 1, 1),
	FACTOR(CLK_TOP_F26M, "f26m_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_AXI, "axi_ck",
			"axi_sel", 1, 1),
	FACTOR(CLK_TOPP_FAXI, "peri_faxi_ck",
			"peri_faxi_sel", 1, 1),
	FACTOR(CLK_TOP_U_FAXI, "ufs_faxi_ck",
			"ufs_faxi_sel", 1, 1),
	FACTOR(CLK_TOP_BUS_AXIMEM, "bus_aximem_ck",
			"bus_aximem_sel", 1, 1),
	FACTOR(CLK_TOP_DISP0, "disp0_ck",
			"disp0_sel", 1, 1),
	FACTOR(CLK_TOP_DISP1, "disp1_ck",
			"disp1_sel", 1, 1),
	FACTOR(CLK_TOP_OVL0, "ovl0_ck",
			"ovl0_sel", 1, 1),
	FACTOR(CLK_TOP_OVL1, "ovl1_ck",
			"ovl1_sel", 1, 1),
	FACTOR(CLK_TOP_MDP0, "mdp0_ck",
			"mdp0_sel", 1, 1),
	FACTOR(CLK_TOP_MDP1, "mdp1_ck",
			"mdp1_sel", 1, 1),
	FACTOR(CLK_TOP_MMINFRA, "mminfra_ck",
			"mminfra_sel", 1, 1),
	FACTOR(CLK_TOP_DSP, "dsp_ck",
			"dsp_sel", 1, 1),
	FACTOR(CLK_TOP_MFG_REF, "mfg_ref_ck",
			"mfg_ref_sel", 1, 1),
	FACTOR(CLK_TOP_MFGSC_REF, "mfgsc_ref_ck",
			"mfgsc_ref_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTG2, "camtg2_ck",
			"camtg2_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTG3, "camtg3_ck",
			"camtg3_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTG4, "camtg4_ck",
			"camtg4_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTG5, "camtg5_ck",
			"camtg5_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTG6, "camtg6_ck",
			"camtg6_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTG7, "camtg7_ck",
			"camtg7_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTG8, "camtg8_ck",
			"camtg8_sel", 1, 1),
	FACTOR(CLK_TOP_UART, "uart_ck",
			"uart_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC_MACRO_1P, "msdc_macro_1p_ck",
			"msdc_macro_1p_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC_MACRO_2P, "msdc_macro_2p_ck",
			"msdc_macro_2p_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_1, "msdc30_1_ck",
			"msdc30_1_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_2, "msdc30_2_ck",
			"msdc30_2_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_INTBUS, "aud_intbus_ck",
			"aud_intbus_sel", 1, 1),
	FACTOR(CLK_TOP_ATB, "atb_ck",
			"atb_sel", 1, 1),
	FACTOR(CLK_TOP_DP, "dp_ck",
			"dp_sel", 1, 1),
	FACTOR(CLK_TOP_DISP_PWM, "disp_pwm_ck",
			"disp_pwm_sel", 1, 1),
	FACTOR(CLK_TOP_USB_TOP, "usb_ck",
			"usb_sel", 1, 1),
	FACTOR(CLK_TOP_USB_XHCI, "ssusb_xhci_ck",
			"ssusb_xhci_sel", 1, 1),
	FACTOR(CLK_TOP_I2C, "i2c_ck",
			"i2c_sel", 1, 1),
	FACTOR(CLK_TOP_SENINF, "seninf_ck",
			"seninf_sel", 1, 1),
	FACTOR(CLK_TOP_SENINF1, "seninf1_ck",
			"seninf1_sel", 1, 1),
	FACTOR(CLK_TOP_SENINF2, "seninf2_ck",
			"seninf2_sel", 1, 1),
	FACTOR(CLK_TOP_SENINF3, "seninf3_ck",
			"seninf3_sel", 1, 1),
	FACTOR(CLK_TOP_SENINF4, "seninf4_ck",
			"seninf4_sel", 1, 1),
	FACTOR(CLK_TOP_SENINF5, "seninf5_ck",
			"seninf5_sel", 1, 1),
	FACTOR(CLK_TOP_DXCC, "dxcc_ck",
			"dxcc_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN1, "aud_engen1_ck",
			"aud_engen1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN2, "aud_engen2_ck",
			"aud_engen2_sel", 1, 1),
	FACTOR(CLK_TOP_AES_UFSFDE, "aes_ufsfde_ck",
			"aes_ufsfde_sel", 1, 1),
	FACTOR(CLK_TOP_UFS, "ufs_ck",
			"ufs_sel", 1, 1),
	FACTOR(CLK_TOP_U_MBIST, "ufs_mbist_ck",
			"ufs_mbist_sel", 1, 1),
	FACTOR(CLK_TOP_PEXTP_MBIST, "pextp_mbist_ck",
			"pextp_mbist_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_1, "aud_1_ck",
			"aud_1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_2, "aud_2_ck",
			"aud_2_sel", 1, 1),
	FACTOR(CLK_TOP_ADSP, "adsp_ck",
			"adsp_sel", 1, 1),
	FACTOR(CLK_TOP_AUDIO_LOCAL_BUS, "audio_local_b",
			"audio_local_bus_sel", 1, 1),
	FACTOR(CLK_TOP_DPMAIF_MAIN, "dpmaif_main_ck",
			"dpmaif_main_sel", 1, 1),
	FACTOR(CLK_TOP_VENC, "venc_ck",
			"venc_sel", 1, 1),
	FACTOR(CLK_TOP_VDEC, "vdec_ck",
			"vdec_sel", 1, 1),
	FACTOR(CLK_TOP_PWM, "pwm_ck",
			"pwm_sel", 1, 1),
	FACTOR(CLK_TOP_AUDIO_H, "audio_h_ck",
			"audio_h_sel", 1, 1),
	FACTOR(CLK_TOP_MCUPM, "mcupm_ck",
			"mcupm_sel", 1, 1),
	FACTOR(CLK_TOP_MEM_SUB, "mem_sub_ck",
			"mem_sub_sel", 1, 1),
	FACTOR(CLK_TOPP_FMEM_SUB, "peri_fmem_sub_ck",
			"peri_fmem_sub_sel", 1, 1),
	FACTOR(CLK_TOP_U_FMEM_SUB, "ufs_fmem_sub_ck",
			"ufs_fmem_sub_sel", 1, 1),
	FACTOR(CLK_TOP_EMI_N, "emi_n_ck",
			"emi_n_sel", 1, 1),
	FACTOR(CLK_TOP_EMI_S, "emi_s_ck",
			"emi_s_sel", 1, 1),
	FACTOR(CLK_TOP_CCU_AHB, "ccu_ahb_ck",
			"ccu_ahb_sel", 1, 1),
	FACTOR(CLK_TOP_AP2CONN_HOST, "ap2conn_host_ck",
			"ap2conn_host_sel", 1, 1),
	FACTOR(CLK_TOP_IMG1, "img1_ck",
			"img1_sel", 1, 1),
	FACTOR(CLK_TOP_IPE, "ipe_ck",
			"ipe_sel", 1, 1),
	FACTOR(CLK_TOP_MCU_ACP, "mcu_acp_ck",
			"mcu_acp_sel", 1, 1),
	FACTOR(CLK_TOP_MCU_L3GIC, "mcu_l3gic_ck",
			"mcu_l3gic_sel", 1, 1),
	FACTOR(CLK_TOP_MCU_INFRA, "mcu_infra_ck",
			"mcu_infra_sel", 1, 1),
	FACTOR(CLK_TOP_IPS_CORE, "ips_core_ck",
			"ips_core_sel", 1, 1),
	FACTOR(CLK_TOP_TL, "tl_ck",
			"tl_sel", 1, 1),
	FACTOR(CLK_TOP_PEXTP_FAXI, "pextp_faxi_ck",
			"pextp_faxi_sel", 1, 1),
	FACTOR(CLK_TOP_PEXTP_FMEM_SUB, "pextp_fmem_sub_ck",
			"pextp_fmem_sub_sel", 1, 1),
	FACTOR(CLK_TOP_EMI_INTERFACE_546, "emi_if_546_ck",
			"emi_if_546_sel", 1, 1),
	FACTOR(CLK_TOP_SPI0, "spi0_ck",
			"spi0_sel", 1, 1),
	FACTOR(CLK_TOP_SPI1, "spi1_ck",
			"spi1_sel", 1, 1),
	FACTOR(CLK_TOP_SPI2, "spi2_ck",
			"spi2_sel", 1, 1),
	FACTOR(CLK_TOP_SPI3, "spi3_ck",
			"spi3_sel", 1, 1),
	FACTOR(CLK_TOP_SPI4, "spi4_ck",
			"spi4_sel", 1, 1),
	FACTOR(CLK_TOP_SPI5, "spi5_ck",
			"spi5_sel", 1, 1),
	FACTOR(CLK_TOP_SPI6, "spi6_ck",
			"spi6_sel", 1, 1),
	FACTOR(CLK_TOP_SPI7, "spi7_ck",
			"spi7_sel", 1, 1),
	FACTOR(CLK_TOP_MMUP, "mmup_ck",
			"mmup_sel", 1, 1),
	FACTOR(CLK_TOP_DBGAO_26M, "dbgao_26m_ck",
			"dbgao_26m_sel", 1, 1),
	FACTOR(CLK_TOP_CAM, "cam_ck",
			"cam_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTM, "camtm_ck",
			"camtm_sel", 1, 1),
	FACTOR(CLK_TOP_DPE, "dpe_ck",
			"dpe_sel", 1, 1),
	FACTOR(CLK_TOP_UARTHUB_BCLK, "uarthub_bclk_ck",
			"uarthub_bclk_sel", 1, 1),
	FACTOR(CLK_TOP_MFG_INT0, "mfg_int0_ck",
			"mfg_int0_sel", 1, 1),
	FACTOR(CLK_TOP_MFG1_INT1, "mfg1_int1_ck",
			"mfg1_int1_sel", 1, 1),
	FACTOR(CLK_TOP_MIPI_CSI_ULPOSC26M, "mipi_26m_ck",
			"osc_d20", 1, 1),
	FACTOR(CLK_TOP_USB_FMCNT, "ssusb_fmcnt_ck",
			"univpll_192m_d4", 1, 1),
	FACTOR(CLK_TOP_ULPOSC_CORE, "ulposc_core_ck",
			"osc_2", 1, 1),
	FACTOR(CLK_TOP_EINT_E_MCLK, "eint_e_mclk",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_EINT_S_MCLK, "eint_s_mclk",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_EINT_N_MCLK, "eint_n_mclk",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_EINT_W_MCLK, "eint_w_mclk",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_AP2CONN_OSC, "ap2conn_osc_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_SSR_273, "ssr_273_ck",
			"univpll_d6_d4", 1, 1),
	FACTOR(CLK_TOP_SSR_136, "ssr_136_ck",
			"mainpll_d4_d2", 1, 1),
	FACTOR(CLK_TOP_SSR_312, "ssr_312_ck",
			"mainpll_d5_d2", 1, 1),
	FACTOR(CLK_TOP_SSR_436, "ssr_436_ck",
			"mainpll_d7", 1, 1),
	FACTOR(CLK_TOP_ARMPLL_DIVIDER_PLL1, "armpll_div_pll1_ck",
			"mainpll_d5", 1, 1),
	FACTOR(CLK_TOP_U_728M_OCC, "ufs_728m_occ_ck",
			"tvdpll_ck", 1, 1),
	FACTOR(CLK_TOP_USB_240M_OCC, "usb_240m_occ_ck",
			"mainpll_d3", 1, 1),
	FACTOR(CLK_TOP_USB_312P5M_OCC, "usb_312p5m_occ_ck",
			"mainpll_d9", 1, 1),
	FACTOR(CLK_TOP_USB_500M_OCC, "usb_500m_occ_ck",
			"univpll_d4_d2", 1, 1),
	FACTOR(CLK_TOP_USB_625M_OCC, "usb_625m_occ_ck",
			"univpll_d5", 1, 1),
	FACTOR(CLK_TOP_DPTX_50M_OCC, "dptx_50m_occ_ck",
			"univpll_d4", 1, 1),
	FACTOR(CLK_TOP_DPTX_148M_OCC, "dptx_148m_occ_ck",
			"univpll_d6_d8", 1, 1),
	FACTOR(CLK_TOP_DPTX_156M_OCC, "dptx_156m_occ_ck",
			"tvdpll_d4", 1, 1),
	FACTOR(CLK_TOP_DPTX_540M_OCC, "dptx_540m_occ_ck",
			"univpll_d4_d4", 1, 1),
	FACTOR(CLK_TOP_DPTX_202M_OCC, "dptx_202m_occ_ck",
			"mainpll_d4", 1, 1),
	FACTOR(CLK_TOP_MFG_50M_OCC, "mfg_50m_occ_ck",
			"univpll_d6_d2", 1, 1),
	FACTOR(CLK_TOP_AFE_480M_OCC, "afe_480m_occ_ck",
			"univpll_d6_d8", 1, 1),
	FACTOR(CLK_TOP_DPM, "dpm_ck",
			"mmpll_d6", 1, 1),
	FACTOR(CLK_TOP_IPSEAST, "ipseast_ck",
			"mpll_208m_ck", 1, 1),
	FACTOR(CLK_TOP_IMGAVS, "imgavs_ck",
			"osc_d20", 1, 1),
	FACTOR(CLK_TOP_SYS_26M, "sys_26m_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_U_SAP_CFG, "ufs_cfg_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_U_TICK1US, "f_u_tick1us_ck",
			"clk26m", 1, 1),
};

static const char * const vlp_scp_parents[] = {
	"tck_26m_mx9_ck",
	"mmpll_d3",
	"univpll_d3",
	"mainpll_d3",
	"univpll_d6",
	"apll1_ck",
	"mainpll_d4",
	"tvdpll_ck",
	"mainpll_d6",
	"osc_d20"
};

static const char * const vlp_scp_spi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2",
	"mainpll_d5_d4",
	"osc_d20"
};

static const char * const vlp_scp_iic_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2",
	"mainpll_d5_d4",
	"osc_d20"
};

static const char * const vlp_scp_spi_hspd_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7",
	"mainpll_d7_d2",
	"mainpll_d5_d4",
	"osc_d20"
};

static const char * const vlp_scp_iic_hspd_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7",
	"mainpll_d7_d2",
	"mainpll_d5_d4",
	"osc_d20"
};

static const char * const vlp_pwrap_ulposc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"osc_d14",
	"osc_d16",
	"osc_d32",
	"mainpll_d7_d8"
};

static const char * const vlp_spmi_m_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"osc_d16",
	"osc_d20",
	"osc_d32",
	"osc_d14",
	"clkrtc",
	"mainpll_d7_d8"
};

static const char * const vlp_spmi_p_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"osc_d16",
	"osc_d20",
	"osc_d32",
	"osc_d14",
	"clkrtc",
	"mainpll_d7_d8"
};

static const char * const vlp_dvfsrc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20"
};

static const char * const vlp_pwm_vlp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d8",
	"clkrtc",
	"osc_d20",
	"mainpll_d4_d8"
};

static const char * const vlp_axi_vlp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"osc_d4",
	"mainpll_d7_d4",
	"mainpll_d7_d2"
};

static const char * const vlp_systimer_26m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20"
};

static const char * const vlp_sspm_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"mainpll_d5_d2",
	"osc_d2",
	"mainpll_d6"
};

static const char * const vlp_srck_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20"
};

static const char * const vlp_sramrc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20"
};

static const char * const vlp_camtg_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"univpll_192m_d32",
	"univpll_192m_d16",
	"univpll_192m_d10",
	"univpll_192m_d8",
	"univpll_d6_d16",
	"univpll_d6_d8",
	"tvdpll_d16",
	"osc_d40",
	"osc_d32",
	"osc_d20"
};

static const char * const vlp_ips_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d4"
};

static const char * const vlp_26m_sspm_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20"
};

static const char * const vlp_ulposc_sspm_parents[] = {
	"osc_d2",
	"univpll_d5_d2"
};

static const char * const vlp_ccusys_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d3",
	"mainpll_d3",
	"imgpll_d2",
	"mainpll_d4",
	"univpll_d5",
	"mmpll_d6",
	"univpll_d6",
	"mmpll_d7",
	"univpll_d4_d2",
	"mmpll_d5_d2",
	"osc_d3",
	"osc_d4"
};

static const char * const vlp_ccutm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_d6_d4",
	"osc_d3",
	"osc_d4"
};

static const struct mtk_mux vlp_ck_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* VLP_CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SPI_HIGH_SPD_SEL/* dts */, "vlp_scp_spi_hspd_sel",
		vlp_scp_spi_hspd_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hspd_sel",
		vlp_scp_iic_hspd_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_P_MST_SEL/* dts */, "vlp_spmi_p_sel",
		vlp_spmi_p_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_P_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWM_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SRAMRC_SEL/* dts */, "vlp_sramrc_sel",
		vlp_sramrc_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRAMRC_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_CAMTG_SEL/* dts */, "vlp_camtg_sel",
		vlp_camtg_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_IPS_SEL/* dts */, "vlp_ips_sel",
		vlp_ips_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IPS_SHIFT/* upd shift */),
	MUX_CLR_SET(CLK_VLP_CK_26M_SSPM_SEL/* dts */, "vlp_26m_sspm_sel",
		vlp_26m_sspm_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 1/* width */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_ULPOSC_SSPM_SEL/* dts */, "vlp_ulposc_sspm_sel",
		vlp_ulposc_sspm_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ULPOSC_SSPM_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_CCUSYS_SEL/* dts */, "vlp_ccusys_sel",
		vlp_ccusys_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CCUSYS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_CCUTM_SEL/* dts */, "vlp_ccutm_sel",
		vlp_ccutm_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CCUTM_SHIFT/* upd shift */),
#else
	/* VLP_CLK_CFG_0 */
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SPI_HIGH_SPD_SEL/* dts */, "vlp_scp_spi_hspd_sel",
		vlp_scp_spi_hspd_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hspd_sel",
		vlp_scp_iic_hspd_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_P_MST_SEL/* dts */, "vlp_spmi_p_sel",
		vlp_spmi_p_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_P_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWM_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SRAMRC_SEL/* dts */, "vlp_sramrc_sel",
		vlp_sramrc_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRAMRC_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_CAMTG_SEL/* dts */, "vlp_camtg_sel",
		vlp_camtg_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_IPS_SEL/* dts */, "vlp_ips_sel",
		vlp_ips_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IPS_SHIFT/* upd shift */),
	MUX_CLR_SET(CLK_VLP_CK_26M_SSPM_SEL/* dts */, "vlp_26m_sspm_sel",
		vlp_26m_sspm_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 1/* width */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_ULPOSC_SSPM_SEL/* dts */, "vlp_ulposc_sspm_sel",
		vlp_ulposc_sspm_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ULPOSC_SSPM_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_CCUSYS_SEL/* dts */, "vlp_ccusys_sel",
		vlp_ccusys_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CCUSYS_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_CCUTM_SEL/* dts */, "vlp_ccutm_sel",
		vlp_ccutm_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CCUTM_SHIFT/* upd shift */),
#endif
};

static const char * const axi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d2",
	"osc_d4",
	"osc_d8",
	"osc_d20"
};

static const char * const peri_faxi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d2",
	"osc_d8"
};

static const char * const ufs_faxi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4",
	"osc_d8"
};

static const char * const bus_aximem_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"osc_d4",
	"mainpll_d7_d2",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6"
};

static const char * const disp0_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d9",
	"univpll_d5_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"univpll_d6",
	"mainpll_d5",
	"mmpll_d6",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d4"
};

static const char * const disp1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d9",
	"univpll_d5_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"univpll_d6",
	"mainpll_d5",
	"mmpll_d6",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d4"
};

static const char * const ovl0_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d9",
	"univpll_d5_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"univpll_d6",
	"mainpll_d5",
	"mmpll_d6",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d4"
};

static const char * const ovl1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d9",
	"univpll_d5_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"univpll_d6",
	"mainpll_d5",
	"mmpll_d6",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d4"
};

static const char * const mdp0_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d9",
	"univpll_d5_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mmpll_d4_d2",
	"univpll_d7",
	"mainpll_d6",
	"univpll_d6",
	"mainpll_d5",
	"mmpll_d6",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d4"
};

static const char * const mdp1_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d9",
	"univpll_d5_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mmpll_d4_d2",
	"univpll_d7",
	"mainpll_d6",
	"univpll_d6",
	"mainpll_d5",
	"mmpll_d6",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d4"
};

static const char * const mminfra_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d4",
	"univpll_d6_d2",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d9",
	"mainpll_d4_d2",
	"mmpll_d4_d2",
	"univpll_d6",
	"mainpll_d5",
	"mmpll_d6",
	"univpll_d5",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d4"
};

static const char * const dsp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d5",
	"osc_d4",
	"osc_d3",
	"univpll_d6_d2",
	"osc_d2",
	"univpll_d4_d2",
	"osc_ck"
};

static const char * const mfg_ref_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6",
	"mainpll_d5_d2"
};

static const char * const mfgsc_ref_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6",
	"mainpll_d5_d2"
};

static const char * const camtg2_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"univpll_192m_d32",
	"univpll_192m_d16",
	"univpll_192m_d10",
	"univpll_192m_d8",
	"univpll_d6_d16",
	"univpll_d6_d8",
	"tvdpll_d16",
	"osc_d40",
	"osc_d32",
	"osc_d20"
};

static const char * const camtg3_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"univpll_192m_d32",
	"univpll_192m_d16",
	"univpll_192m_d10",
	"univpll_192m_d8",
	"univpll_d6_d16",
	"univpll_d6_d8",
	"tvdpll_d16",
	"osc_d40",
	"osc_d32",
	"osc_d20"
};

static const char * const camtg4_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"univpll_192m_d32",
	"univpll_192m_d16",
	"univpll_192m_d10",
	"univpll_192m_d8",
	"univpll_d6_d16",
	"univpll_d6_d8",
	"tvdpll_d16",
	"osc_d40",
	"osc_d32",
	"osc_d20"
};

static const char * const camtg5_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"univpll_192m_d32",
	"univpll_192m_d16",
	"univpll_192m_d10",
	"univpll_192m_d8",
	"univpll_d6_d16",
	"univpll_d6_d8",
	"tvdpll_d16",
	"osc_d40",
	"osc_d32",
	"osc_d20"
};

static const char * const camtg6_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"univpll_192m_d32",
	"univpll_192m_d16",
	"univpll_192m_d10",
	"univpll_192m_d8",
	"univpll_d6_d16",
	"univpll_d6_d8",
	"tvdpll_d16",
	"osc_d40",
	"osc_d32",
	"osc_d20"
};

static const char * const camtg7_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"univpll_192m_d32",
	"univpll_192m_d16",
	"univpll_192m_d10",
	"univpll_192m_d8",
	"univpll_d6_d16",
	"univpll_d6_d8",
	"tvdpll_d16",
	"osc_d40",
	"osc_d32",
	"osc_d20"
};

static const char * const camtg8_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"univpll_192m_d32",
	"univpll_192m_d16",
	"univpll_192m_d10",
	"univpll_192m_d8",
	"univpll_d6_d16",
	"univpll_d6_d8",
	"tvdpll_d16",
	"osc_d40",
	"osc_d32",
	"osc_d20"
};

static const char * const uart_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d8",
	"univpll_d6_d4"
};

static const char * const msdc_macro_1p_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6",
	"mainpll_d6",
	"univpll_d6_d2",
	"msdcpll_ck"
};

static const char * const msdc_macro_2p_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6",
	"mainpll_d6",
	"univpll_d6_d2",
	"msdcpll_ck"
};

static const char * const msdc30_1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"univpll_d6_d4",
	"msdcpll_d2"
};

static const char * const msdc30_2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"univpll_d6_d4",
	"msdcpll_d2"
};

static const char * const aud_intbus_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d4"
};

static const char * const atb_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d5_d2"
};

static const char * const dp_parents[] = {
	"tck_26m_mx9_ck",
	"tvdpll_d4",
	"tvdpll_d8",
	"tvdpll_d16"
};

static const char * const disp_pwm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"osc_d4",
	"osc_d8",
	"osc_d32",
	"univpll_d5_d4",
	"mainpll_d4_d4"
};

static const char * const usb_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const ssusb_xhci_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const i2c_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d8",
	"univpll_d5_d4",
	"mainpll_d4_d4"
};

static const char * const seninf_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"univpll_d6",
	"univpll_d5",
	"osc_d10",
	"osc_d8",
	"osc_d5",
	"osc_d4",
	"osc_d2"
};

static const char * const seninf1_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"univpll_d6",
	"univpll_d5",
	"osc_d10",
	"osc_d8",
	"osc_d5",
	"osc_d4",
	"osc_d2"
};

static const char * const seninf2_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"univpll_d6",
	"univpll_d5",
	"osc_d10",
	"osc_d8",
	"osc_d5",
	"osc_d4",
	"osc_d2"
};

static const char * const seninf3_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"univpll_d6",
	"univpll_d5",
	"osc_d10",
	"osc_d8",
	"osc_d5",
	"osc_d4",
	"osc_d2"
};

static const char * const seninf4_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"univpll_d6",
	"univpll_d5",
	"osc_d10",
	"osc_d8",
	"osc_d5",
	"osc_d4",
	"osc_d2"
};

static const char * const seninf5_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"univpll_d6",
	"univpll_d5",
	"osc_d10",
	"osc_d8",
	"osc_d5",
	"osc_d4",
	"osc_d2"
};

static const char * const aud_engen1_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_d4",
	"apll1_d8"
};

static const char * const aud_engen2_parents[] = {
	"tck_26m_mx9_ck",
	"apll2_d4",
	"apll2_d8"
};

static const char * const aes_ufsfde_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d4_d4",
	"univpll_d4_d2",
	"univpll_d6"
};

static const char * const ufs_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d4_d4",
	"univpll_d4_d2",
	"mainpll_d6",
	"mainpll_d5",
	"univpll_d6",
	"mmpll_d6"
};

static const char * const pextp_mbist_parents[] = {
	"tck_26m_mx9_ck",
	"mmpll_d6_d2",
	"mainpll_d9",
	"univpll_d5_d2"
};

static const char * const aud_1_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_ck"
};

static const char * const aud_2_parents[] = {
	"tck_26m_mx9_ck",
	"apll2_ck"
};

static const char * const adsp_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6",
	"mainpll_d5_d2",
	"univpll_d4_d4",
	"univpll_d4",
	"univpll_d6",
	"osc_d2",
	"adsppll_ck"
};

static const char * const audio_local_bus_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"univpll_d4",
	"univpll_d7_d2",
	"univpll_d6",
	"univpll_d5",
	"osc_d2"
};

static const char * const dpmaif_main_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5",
	"mainpll_d5",
	"univpll_d6",
	"mainpll_d6",
	"mainpll_d4_d2",
	"univpll_d5_d2",
	"univpll_d4_d2"
};

static const char * const venc_parents[] = {
	"tck_26m_mx9_ck",
	"mmpll_d4_d2",
	"mainpll_d6",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"univpll_d6",
	"mmpll_d6",
	"mainpll_d5_d2",
	"mainpll_d3",
	"mmpll_d9",
	"mmpll_d4",
	"mainpll_d4",
	"univpll_d4",
	"univpll_d5",
	"univpll_d5_d2",
	"mainpll_d5"
};

static const char * const vdec_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2",
	"mainpll_d6_d2",
	"mainpll_d4",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d5_d2",
	"mainpll_d7",
	"univpll_d7",
	"mmpll_d7",
	"univpll_d6",
	"mainpll_d5",
	"tvdpll_ck",
	"imgpll_d2"
};

static const char * const pwm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d8"
};

static const char * const audio_h_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_d2",
	"apll2_d2"
};

static const char * const mcupm_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"univpll_d6_d2",
	"mainpll_d6_d2"
};

static const char * const mem_sub_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"univpll_d4_d4",
	"osc_d4",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"mainpll_d5",
	"mmpll_d6",
	"univpll_d5",
	"mainpll_d4",
	"univpll_d4",
	"emipll_ck",
	"mainpll_d4",
	"mainpll_d3"
};

static const char * const peri_fmem_sub_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"osc_d4",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"mainpll_d5",
	"mmpll_d6",
	"univpll_d5",
	"mainpll_d4",
	"univpll_d4"
};

static const char * const ufs_fmem_sub_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"osc_d4",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"mainpll_d5",
	"mmpll_d6",
	"univpll_d5",
	"mainpll_d4"
};

static const char * const emi_n_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6_d2",
	"osc_d4",
	"emipll_ck"
};

static const char * const emi_s_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6_d2",
	"osc_d4",
	"emipll_ck"
};

static const char * const ccu_ahb_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d3",
	"tvdpll_ck",
	"mainpll_d4",
	"univpll_d5",
	"univpll_d6",
	"univpll_d4_d2",
	"mmpll_d5_d2"
};

static const char * const ap2conn_host_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4"
};

static const char * const img1_parents[] = {
	"tck_26m_mx9_ck",
	"imgpll_d2",
	"univpll_d4",
	"mainpll_d4",
	"univpll_d5",
	"mmpll_d6",
	"univpll_d6",
	"mmpll_d7",
	"mmpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"imgpll_d5",
	"osc_d4",
	"osc_d3",
	"mmpll_d6_d2"
};

static const char * const ipe_parents[] = {
	"tck_26m_mx9_ck",
	"imgpll_d2",
	"mainpll_d4",
	"univpll_d6",
	"mainpll_d6",
	"mmpll_d4_d2",
	"univpll_d4_d2",
	"mmpll_d5_d2",
	"osc_d4",
	"osc_d3",
	"mmpll_d6_d2"
};

static const char * const mcu_acp_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d4",
	"univpll_d4"
};

static const char * const mcu_l3gic_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2"
};

static const char * const mcu_infra_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6"
};

static const char * const tl_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"mainpll_d4_d4"
};

static const char * const pextp_faxi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d2",
	"osc_d8"
};

static const char * const pextp_fmem_sub_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"osc_d4",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"mainpll_d5",
	"mmpll_d6",
	"univpll_d5",
	"mainpll_d4"
};

static const char * const emi_if_546_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4"
};

static const char * const spi0_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_192m_ck",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4"
};

static const char * const spi1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"mainpll_d7_d4"
};

static const char * const spi2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"mainpll_d7_d4"
};

static const char * const spi3_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"mainpll_d7_d4"
};

static const char * const spi4_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"mainpll_d7_d4"
};

static const char * const spi5_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"mainpll_d7_d4"
};

static const char * const spi6_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"mainpll_d7_d4"
};

static const char * const spi7_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"mainpll_d7_d4"
};

static const char * const mmup_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"mmpll_d4_d2",
	"univpll_d4",
	"mmpll_d4",
	"mainpll_d3"
};

static const char * const dbgao_26m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20"
};

static const char * const cam_parents[] = {
	"tck_26m_mx9_ck",
	"mmpll_d4",
	"univpll_d4",
	"mainpll_d4",
	"univpll_d5",
	"mmpll_d6",
	"univpll_d6",
	"mmpll_d7",
	"univpll_d7",
	"univpll_d4_d2",
	"mmpll_d5_d2",
	"osc_d3",
	"osc_d4",
	"osc_d6",
	"osc_d8",
	"osc_d10"
};

static const char * const camtm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_d6_d4",
	"osc_d3",
	"osc_d4"
};

static const char * const dpe_parents[] = {
	"tck_26m_mx9_ck",
	"imgpll_d2",
	"mmpll_d5",
	"univpll_d5",
	"mmpll_d6",
	"univpll_d6",
	"mmpll_d7",
	"univpll_d4_d2",
	"mmpll_d5_d2"
};

static const char * const mfg_int0_parents[] = {
	"mfg_ref_ck",
	"mfgpll_ck"
};

static const char * const mfg1_int1_parents[] = {
	"mfgsc_ref_ck",
	"mfgscpll_ck"
};

static const char * const apll_i2sin0_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin1_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin2_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin3_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin4_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin6_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout0_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout1_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout2_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout3_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout4_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout6_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_fmi2s_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_tdmout_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const struct mtk_mux top_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_TOP_AXI_SEL/* dts */, "axi_sel",
		axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOPP_FAXI_SEL/* dts */, "peri_faxi_sel",
		peri_faxi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_FAXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_U_FAXI_SEL/* dts */, "ufs_faxi_sel",
		ufs_faxi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_FAXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "bus_aximem_sel",
		bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_TOP_DISP0_SEL/* dts */, "disp0_sel",
		disp0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DISP1_SEL/* dts */, "disp1_sel",
		disp1_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_OVL0_SEL/* dts */, "ovl0_sel",
		ovl0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_OVL0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_OVL1_SEL/* dts */, "ovl1_sel",
		ovl1_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_OVL1_SHIFT/* upd shift */),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_TOP_MDP0_SEL/* dts */, "mdp0_sel",
		mdp0_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MDP0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MDP1_SEL/* dts */, "mdp1_sel",
		mdp1_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MDP1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MMINFRA_SEL/* dts */, "mminfra_sel",
		mminfra_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DSP_SEL/* dts */, "dsp_sel",
		dsp_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DSP_SHIFT/* upd shift */),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_TOP_MFG_REF_SEL/* dts */, "mfg_ref_sel",
		mfg_ref_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MFGSC_REF_SEL/* dts */, "mfgsc_ref_sel",
		mfgsc_ref_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFGSC_REF_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL/* dts */, "camtg2_sel",
		camtg2_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG3_SEL/* dts */, "camtg3_sel",
		camtg3_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG3_SHIFT/* upd shift */),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG4_SEL/* dts */, "camtg4_sel",
		camtg4_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG4_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG5_SEL/* dts */, "camtg5_sel",
		camtg5_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG5_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG6_SEL/* dts */, "camtg6_sel",
		camtg6_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG6_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG7_SEL/* dts */, "camtg7_sel",
		camtg7_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG7_SHIFT/* upd shift */),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG8_SEL/* dts */, "camtg8_sel",
		camtg8_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG8_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_UART_SEL/* dts */, "uart_sel",
		uart_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UART_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC_MACRO_1P_SEL/* dts */, "msdc_macro_1p_sel",
		msdc_macro_1p_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC_MACRO_2P_SEL/* dts */, "msdc_macro_2p_sel",
		msdc_macro_2p_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC_MACRO_2P_SHIFT/* upd shift */),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL/* dts */, "msdc30_1_sel",
		msdc30_1_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC30_1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC30_2_SEL/* dts */, "msdc30_2_sel",
		msdc30_2_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC30_2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL/* dts */, "aud_intbus_sel",
		aud_intbus_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_ATB_SEL/* dts */, "atb_sel",
		atb_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD(CLK_TOP_DP_SEL/* dts */, "dp_sel",
		dp_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL/* dts */, "disp_pwm_sel",
		disp_pwm_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP_PWM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL/* dts */, "usb_sel",
		usb_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_USB_TOP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USB_XHCI_SEL/* dts */, "ssusb_xhci_sel",
		ssusb_xhci_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */),
	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD(CLK_TOP_I2C_SEL/* dts */, "i2c_sel",
		i2c_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_I2C_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF_SEL/* dts */, "seninf_sel",
		seninf_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SENINF_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF1_SEL/* dts */, "seninf1_sel",
		seninf1_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SENINF1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF2_SEL/* dts */, "seninf2_sel",
		seninf2_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SENINF2_SHIFT/* upd shift */),
	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD(CLK_TOP_SENINF3_SEL/* dts */, "seninf3_sel",
		seninf3_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SENINF3_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF4_SEL/* dts */, "seninf4_sel",
		seninf4_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SENINF4_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF5_SEL/* dts */, "seninf5_sel",
		seninf5_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SENINF5_SHIFT/* upd shift */),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "aud_engen1_sel",
		aud_engen1_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "aud_engen2_sel",
		aud_engen2_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AES_UFSFDE_SEL/* dts */, "aes_ufsfde_sel",
		aes_ufsfde_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_U_SEL/* dts */, "ufs_sel",
		ufs_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_SHIFT/* upd shift */),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD(CLK_TOP_PEXTP_MBIST_SEL/* dts */, "pextp_mbist_sel",
		pextp_mbist_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PEXTP_MBIST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_1_SEL/* dts */, "aud_1_sel",
		aud_1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_2_SEL/* dts */, "aud_2_sel",
		aud_2_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_2_SHIFT/* upd shift */),
	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD(CLK_TOP_ADSP_SEL/* dts */, "adsp_sel",
		adsp_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_ADSP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUDIO_LOCAL_BUS_SEL/* dts */, "audio_local_bus_sel",
		audio_local_bus_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUDIO_LOCAL_BUS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DPMAIF_MAIN_SEL/* dts */, "dpmaif_main_sel",
		dpmaif_main_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_VENC_SEL/* dts */, "venc_sel",
		venc_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_VENC_SHIFT/* upd shift */),
	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD(CLK_TOP_VDEC_SEL/* dts */, "vdec_sel",
		vdec_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_VDEC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_PWM_SEL/* dts */, "pwm_sel",
		pwm_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PWM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUDIO_H_SEL/* dts */, "audio_h_sel",
		audio_h_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUDIO_H_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCUPM_SEL/* dts */, "mcupm_sel",
		mcupm_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */),
	/* CLK_CFG_14 */
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_SEL/* dts */, "mem_sub_sel",
		mem_sub_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOPP_FMEM_SUB_SEL/* dts */, "peri_fmem_sub_sel",
		peri_fmem_sub_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PERI_FMEM_SUB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_U_FMEM_SUB_SEL/* dts */, "ufs_fmem_sub_sel",
		ufs_fmem_sub_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_FMEM_SUB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EMI_N_SEL/* dts */, "emi_n_sel",
		emi_n_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */),
	/* CLK_CFG_15 */
	MUX_CLR_SET_UPD(CLK_TOP_EMI_S_SEL/* dts */, "emi_s_sel",
		emi_s_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_S_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CCU_AHB_SEL/* dts */, "ccu_ahb_sel",
		ccu_ahb_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_CCU_AHB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "ap2conn_host_sel",
		ap2conn_host_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_IMG1_SEL/* dts */, "img1_sel",
		img1_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_IMG1_SHIFT/* upd shift */),
	/* CLK_CFG_16 */
	MUX_CLR_SET_UPD(CLK_TOP_IPE_SEL/* dts */, "ipe_sel",
		ipe_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_IPE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCU_ACP_SEL/* dts */, "mcu_acp_sel",
		mcu_acp_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MCU_ACP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCU_L3GIC_SEL/* dts */, "mcu_l3gic_sel",
		mcu_l3gic_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MCU_L3GIC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCU_INFRA_SEL/* dts */, "mcu_infra_sel",
		mcu_infra_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MCU_INFRA_SHIFT/* upd shift */),
	/* CLK_CFG_17 */
	MUX_CLR_SET_UPD(CLK_TOP_TL_SEL/* dts */, "tl_sel",
		tl_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_TL_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_PEXTP_FAXI_SEL/* dts */, "pextp_faxi_sel",
		pextp_faxi_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_PEXTP_FAXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_PEXTP_FMEM_SUB_SEL/* dts */, "pextp_fmem_sub_sel",
		pextp_fmem_sub_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_PEXTP_FMEM_SUB_SHIFT/* upd shift */),
	/* CLK_CFG_18 */
	MUX_CLR_SET_UPD(CLK_TOP_EMI_INTERFACE_546_SEL/* dts */, "emi_if_546_sel",
		emi_if_546_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI0_SEL/* dts */, "spi0_sel",
		spi0_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPI0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI1_SEL/* dts */, "spi1_sel",
		spi1_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPI1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI2_SEL/* dts */, "spi2_sel",
		spi2_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPI2_SHIFT/* upd shift */),
	/* CLK_CFG_19 */
	MUX_CLR_SET_UPD(CLK_TOP_SPI3_SEL/* dts */, "spi3_sel",
		spi3_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPI3_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI4_SEL/* dts */, "spi4_sel",
		spi4_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPI4_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI5_SEL/* dts */, "spi5_sel",
		spi5_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPI5_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI6_SEL/* dts */, "spi6_sel",
		spi6_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPI6_SHIFT/* upd shift */),
	/* CLK_CFG_20 */
	MUX_CLR_SET_UPD(CLK_TOP_SPI7_SEL/* dts */, "spi7_sel",
		spi7_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SPI7_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MMUP_SEL/* dts */, "mmup_sel",
		mmup_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MMUP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DBGAO_26M_SEL/* dts */, "dbgao_26m_sel",
		dbgao_26m_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DBGAO_26M_SHIFT/* upd shift */),
	/* CLK_CFG_21 */
	MUX_CLR_SET_UPD(CLK_TOP_CAM_SEL/* dts */, "cam_sel",
		cam_parents/* parent */, CLK_CFG_21, CLK_CFG_21_SET,
		CLK_CFG_21_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_CAM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTM_SEL/* dts */, "camtm_sel",
		camtm_parents/* parent */, CLK_CFG_21, CLK_CFG_21_SET,
		CLK_CFG_21_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_CAMTM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DPE_SEL/* dts */, "dpe_sel",
		dpe_parents/* parent */, CLK_CFG_21, CLK_CFG_21_SET,
		CLK_CFG_21_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DPE_SHIFT/* upd shift */),
	/* CLK_CFG_30 */
	MUX_CLR_SET(CLK_TOP_MFG_INT0_SEL/* dts */, "mfg_int0_sel",
		mfg_int0_parents/* parent */, CLK_CFG_30, CLK_CFG_30_SET,
		CLK_CFG_30_CLR/* set parent */, 16/* lsb */, 1/* width */),
	MUX_CLR_SET(CLK_TOP_MFG1_INT1_SEL/* dts */, "mfg1_int1_sel",
		mfg1_int1_parents/* parent */, CLK_CFG_30, CLK_CFG_30_SET,
		CLK_CFG_30_CLR/* set parent */, 17/* lsb */, 1/* width */),
#else
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_TOP_AXI_SEL/* dts */, "axi_sel",
		axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOPP_FAXI_SEL/* dts */, "peri_faxi_sel",
		peri_faxi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_FAXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_U_FAXI_SEL/* dts */, "ufs_faxi_sel",
		ufs_faxi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_FAXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "bus_aximem_sel",
		bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */),
	/* CLK_CFG_1 */
	MUX_HWV_FLAGS(CLK_TOP_DISP0_SEL/* dts */, "disp0_sel", disp0_parents/* parent */,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR/* set parent */,
		HW_CCF_CG_18_DONE, HW_CCF_CG_18_SET, HW_CCF_CG_18_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP0_SHIFT/* upd shift */, CLK_ENABLE_MERGE_CONTROL),
	MUX_HWV_FLAGS(CLK_TOP_DISP1_SEL/* dts */, "disp1_sel", disp1_parents/* parent */,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR/* set parent */,
		HW_CCF_CG_18_DONE, HW_CCF_CG_18_SET, HW_CCF_CG_18_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP1_SHIFT/* upd shift */, CLK_ENABLE_MERGE_CONTROL),
	MUX_HWV_FLAGS(CLK_TOP_OVL0_SEL/* dts */, "ovl0_sel", ovl0_parents/* parent */,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR/* set parent */,
		HW_CCF_CG_18_DONE, HW_CCF_CG_18_SET, HW_CCF_CG_18_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_OVL0_SHIFT/* upd shift */, CLK_ENABLE_MERGE_CONTROL),
	MUX_HWV_FLAGS(CLK_TOP_OVL1_SEL/* dts */, "ovl1_sel", ovl1_parents/* parent */,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR/* set parent */,
		HW_CCF_CG_18_DONE, HW_CCF_CG_18_SET, HW_CCF_CG_18_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_OVL1_SHIFT/* upd shift */, CLK_ENABLE_MERGE_CONTROL),
	/* CLK_CFG_2 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MDP0_SEL/* dts */, "mdp0_sel",
		mdp0_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MDP0_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MDP1_SEL/* dts */, "mdp1_sel",
		mdp1_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MDP1_SHIFT/* upd shift */),
	MUX_HWV(CLK_TOP_MMINFRA_SEL/* dts */, "mminfra_sel", mminfra_parents/* parent */,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR/* set parent */,
		HW_CCF_CG_19_DONE, HW_CCF_CG_19_SET, HW_CCF_CG_19_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DSP_SEL/* dts */, "dsp_sel",
		dsp_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DSP_SHIFT/* upd shift */),
	/* CLK_CFG_3 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFG_REF_SEL/* dts */, "mfg_ref_sel",
		mfg_ref_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MFG_REF_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFGSC_REF_SEL/* dts */, "mfgsc_ref_sel",
		mfgsc_ref_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MFGSC_REF_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL/* dts */, "camtg2_sel",
		camtg2_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG3_SEL/* dts */, "camtg3_sel",
		camtg3_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG3_SHIFT/* upd shift */),
	/* CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG4_SEL/* dts */, "camtg4_sel",
		camtg4_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG4_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG5_SEL/* dts */, "camtg5_sel",
		camtg5_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG5_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG6_SEL/* dts */, "camtg6_sel",
		camtg6_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG6_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG7_SEL/* dts */, "camtg7_sel",
		camtg7_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG7_SHIFT/* upd shift */),
	/* CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG8_SEL/* dts */, "camtg8_sel",
		camtg8_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG8_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UART_SEL/* dts */, "uart_sel",
		uart_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_UART_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC_MACRO_1P_SEL/* dts */, "msdc_macro_1p_sel",
		msdc_macro_1p_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC_MACRO_2P_SEL/* dts */, "msdc_macro_2p_sel",
		msdc_macro_2p_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC_MACRO_2P_SHIFT/* upd shift */),
	/* CLK_CFG_6 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL/* dts */, "msdc30_1_sel",
		msdc30_1_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_2_SEL/* dts */, "msdc30_2_sel",
		msdc30_2_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL/* dts */, "aud_intbus_sel",
		aud_intbus_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_ATB_SEL/* dts */, "atb_sel",
		atb_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */),
	/* CLK_CFG_7 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DP_SEL/* dts */, "dp_sel",
		dp_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DP_SHIFT/* upd shift */),
	MUX_HWV(CLK_TOP_DISP_PWM_SEL/* dts */, "disp_pwm_sel", disp_pwm_parents/* parent */,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR/* set parent */,
		HW_CCF_CG_20_DONE, HW_CCF_CG_20_SET, HW_CCF_CG_20_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP_PWM_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL/* dts */, "usb_sel",
		usb_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_USB_TOP_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_XHCI_SEL/* dts */, "ssusb_xhci_sel",
		ssusb_xhci_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */),
	/* CLK_CFG_8 */
	MUX_HWV(CLK_TOP_I2C_SEL/* dts */, "i2c_sel", i2c_parents/* parent */,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR/* set parent */,
		HW_CCF_CG_21_DONE, HW_CCF_CG_21_SET, HW_CCF_CG_21_CLR, /* hwv */
		0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF_SEL/* dts */, "seninf_sel",
		seninf_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF1_SEL/* dts */, "seninf1_sel",
		seninf1_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF2_SEL/* dts */, "seninf2_sel",
		seninf2_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF2_SHIFT/* upd shift */),
	/* CLK_CFG_9 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF3_SEL/* dts */, "seninf3_sel",
		seninf3_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF3_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF4_SEL/* dts */, "seninf4_sel",
		seninf4_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF4_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF5_SEL/* dts */, "seninf5_sel",
		seninf5_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF5_SHIFT/* upd shift */),
	/* CLK_CFG_10 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "aud_engen1_sel",
		aud_engen1_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "aud_engen2_sel",
		aud_engen2_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_UFSFDE_SEL/* dts */, "aes_ufsfde_sel",
		aes_ufsfde_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD_FLAGS_2(CLK_TOP_U_SEL/* dts */, "ufs_sel",
		ufs_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_SHIFT/* upd shift */, MUX_ROUND_CLOSEST),
	/* CLK_CFG_11 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PEXTP_MBIST_SEL/* dts */, "pextp_mbist_sel",
		pextp_mbist_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PEXTP_MBIST_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_1_SEL/* dts */, "aud_1_sel",
		aud_1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_2_SEL/* dts */, "aud_2_sel",
		aud_2_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_2_SHIFT/* upd shift */),
	/* CLK_CFG_12 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ADSP_SEL/* dts */, "adsp_sel",
		adsp_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_ADSP_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_LOCAL_BUS_SEL/* dts */, "audio_local_bus_sel",
		audio_local_bus_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUDIO_LOCAL_BUS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DPMAIF_MAIN_SEL/* dts */, "dpmaif_main_sel",
		dpmaif_main_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */),
	MUX_HWV(CLK_TOP_VENC_SEL/* dts */, "venc_sel", venc_parents/* parent */,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR/* set parent */,
		HW_CCF_CG_22_DONE, HW_CCF_CG_22_SET, HW_CCF_CG_22_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VENC_SHIFT/* upd shift */),
	/* CLK_CFG_13 */
	MUX_IPI(CLK_TOP_VDEC_SEL/* dts */, "vdec_sel", vdec_parents/* parent */,
		CLK_CFG_13, CLK_CFG_13_SET, CLK_CFG_13_CLR/* set parent */,
		HW_CCF_CG_23_DONE, HW_CCF_CG_23_SET, HW_CCF_CG_23_CLR, /* hwv */
		0/* ipi */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VDEC_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWM_SEL/* dts */, "pwm_sel",
		pwm_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PWM_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_H_SEL/* dts */, "audio_h_sel",
		audio_h_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUDIO_H_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCUPM_SEL/* dts */, "mcupm_sel",
		mcupm_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */),
	/* CLK_CFG_14 */
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_SEL/* dts */, "mem_sub_sel",
		mem_sub_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOPP_FMEM_SUB_SEL/* dts */, "peri_fmem_sub_sel",
		peri_fmem_sub_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PERI_FMEM_SUB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_U_FMEM_SUB_SEL/* dts */, "ufs_fmem_sub_sel",
		ufs_fmem_sub_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_FMEM_SUB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EMI_N_SEL/* dts */, "emi_n_sel",
		emi_n_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */),
	/* CLK_CFG_15 */
	MUX_CLR_SET_UPD(CLK_TOP_EMI_S_SEL/* dts */, "emi_s_sel",
		emi_s_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_S_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CCU_AHB_SEL/* dts */, "ccu_ahb_sel",
		ccu_ahb_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_CCU_AHB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "ap2conn_host_sel",
		ap2conn_host_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */),
	MUX_IPI(CLK_TOP_IMG1_SEL/* dts */, "img1_sel", img1_parents/* parent */,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR/* set parent */,
		HW_CCF_CG_24_DONE, HW_CCF_CG_24_SET, HW_CCF_CG_24_CLR, /* hwv */
		1/* ipi */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_IMG1_SHIFT/* upd shift */),
	/* CLK_CFG_16 */
	MUX_IPI(CLK_TOP_IPE_SEL/* dts */, "ipe_sel", ipe_parents/* parent */,
		CLK_CFG_16, CLK_CFG_16_SET, CLK_CFG_16_CLR/* set parent */,
		HW_CCF_CG_25_DONE, HW_CCF_CG_25_SET, HW_CCF_CG_25_CLR, /* hwv */
		2/* ipi */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_IPE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCU_ACP_SEL/* dts */, "mcu_acp_sel",
		mcu_acp_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MCU_ACP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCU_L3GIC_SEL/* dts */, "mcu_l3gic_sel",
		mcu_l3gic_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MCU_L3GIC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCU_INFRA_SEL/* dts */, "mcu_infra_sel",
		mcu_infra_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_MCU_INFRA_SHIFT/* upd shift */),
	/* CLK_CFG_17 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_TL_SEL/* dts */, "tl_sel",
		tl_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_TL_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_PEXTP_FAXI_SEL/* dts */, "pextp_faxi_sel",
		pextp_faxi_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_PEXTP_FAXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_PEXTP_FMEM_SUB_SEL/* dts */, "pextp_fmem_sub_sel",
		pextp_fmem_sub_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_PEXTP_FMEM_SUB_SHIFT/* upd shift */),
	/* CLK_CFG_18 */
	MUX_CLR_SET_UPD(CLK_TOP_EMI_INTERFACE_546_SEL/* dts */, "emi_if_546_sel",
		emi_if_546_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI0_SEL/* dts */, "spi0_sel",
		spi0_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SPI0_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI1_SEL/* dts */, "spi1_sel",
		spi1_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SPI1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI2_SEL/* dts */, "spi2_sel",
		spi2_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SPI2_SHIFT/* upd shift */),
	/* CLK_CFG_19 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI3_SEL/* dts */, "spi3_sel",
		spi3_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SPI3_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI4_SEL/* dts */, "spi4_sel",
		spi4_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SPI4_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI5_SEL/* dts */, "spi5_sel",
		spi5_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SPI5_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI6_SEL/* dts */, "spi6_sel",
		spi6_parents/* parent */, CLK_CFG_19, CLK_CFG_19_SET,
		CLK_CFG_19_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SPI6_SHIFT/* upd shift */),
	/* CLK_CFG_20 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI7_SEL/* dts */, "spi7_sel",
		spi7_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SPI7_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MMUP_SEL/* dts */, "mmup_sel",
		mmup_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MMUP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DBGAO_26M_SEL/* dts */, "dbgao_26m_sel",
		dbgao_26m_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DBGAO_26M_SHIFT/* upd shift */),
	/* CLK_CFG_21 */
	MUX_IPI(CLK_TOP_CAM_SEL/* dts */, "cam_sel", cam_parents/* parent */,
		CLK_CFG_21, CLK_CFG_21_SET, CLK_CFG_21_CLR/* set parent */,
		HW_CCF_CG_26_DONE, HW_CCF_CG_26_SET, HW_CCF_CG_26_CLR, /* hwv */
		3/* ipi */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_CAM_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTM_SEL/* dts */, "camtm_sel",
		camtm_parents/* parent */, CLK_CFG_21, CLK_CFG_21_SET,
		CLK_CFG_21_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_CAMTM_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DPE_SEL/* dts */, "dpe_sel",
		dpe_parents/* parent */, CLK_CFG_21, CLK_CFG_21_SET,
		CLK_CFG_21_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_DPE_SHIFT/* upd shift */),
	/* CLK_CFG_30 */
	MUX_CLR_SET(CLK_TOP_MFG_INT0_SEL/* dts */, "mfg_int0_sel",
		mfg_int0_parents/* parent */, CLK_CFG_30, CLK_CFG_30_SET,
		CLK_CFG_30_CLR/* set parent */, 16/* lsb */, 1/* width */),
	MUX_CLR_SET(CLK_TOP_MFG1_INT1_SEL/* dts */, "mfg1_int1_sel",
		mfg1_int1_parents/* parent */, CLK_CFG_30, CLK_CFG_30_SET,
		CLK_CFG_30_CLR/* set parent */, 17/* lsb */, 1/* width */),
#endif
};

static const struct mtk_composite top_composites[] = {
	/* CLK_AUDDIV_0 */
	MUX(CLK_TOP_APLL_I2SIN0_MCK_SEL/* dts */, "apll_i2sin0_mck_sel",
		apll_i2sin0_mck_parents/* parent */, 0x0320/* ofs */,
		16/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN1_MCK_SEL/* dts */, "apll_i2sin1_mck_sel",
		apll_i2sin1_mck_parents/* parent */, 0x0320/* ofs */,
		17/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN2_MCK_SEL/* dts */, "apll_i2sin2_mck_sel",
		apll_i2sin2_mck_parents/* parent */, 0x0320/* ofs */,
		18/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN3_MCK_SEL/* dts */, "apll_i2sin3_mck_sel",
		apll_i2sin3_mck_parents/* parent */, 0x0320/* ofs */,
		19/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN4_MCK_SEL/* dts */, "apll_i2sin4_mck_sel",
		apll_i2sin4_mck_parents/* parent */, 0x0320/* ofs */,
		20/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN6_MCK_SEL/* dts */, "apll_i2sin6_mck_sel",
		apll_i2sin6_mck_parents/* parent */, 0x0320/* ofs */,
		21/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT0_MCK_SEL/* dts */, "apll_i2sout0_mck_sel",
		apll_i2sout0_mck_parents/* parent */, 0x0320/* ofs */,
		22/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT1_MCK_SEL/* dts */, "apll_i2sout1_mck_sel",
		apll_i2sout1_mck_parents/* parent */, 0x0320/* ofs */,
		23/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT2_MCK_SEL/* dts */, "apll_i2sout2_mck_sel",
		apll_i2sout2_mck_parents/* parent */, 0x0320/* ofs */,
		24/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT3_MCK_SEL/* dts */, "apll_i2sout3_mck_sel",
		apll_i2sout3_mck_parents/* parent */, 0x0320/* ofs */,
		25/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT4_MCK_SEL/* dts */, "apll_i2sout4_mck_sel",
		apll_i2sout4_mck_parents/* parent */, 0x0320/* ofs */,
		26/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT6_MCK_SEL/* dts */, "apll_i2sout6_mck_sel",
		apll_i2sout6_mck_parents/* parent */, 0x0320/* ofs */,
		27/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_FMI2S_MCK_SEL/* dts */, "apll_fmi2s_mck_sel",
		apll_fmi2s_mck_parents/* parent */, 0x0320/* ofs */,
		28/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_TDMOUT_MCK_SEL/* dts */, "apll_tdmout_mck_sel",
		apll_tdmout_mck_parents/* parent */, 0x0320/* ofs */,
		29/* lsb */, 1/* width */),
	/* CLK_AUDDIV_2 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN0/* dts */, "apll12_div_in0"/* ccf */,
		"apll_i2sin0_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		0/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN1/* dts */, "apll12_div_in1"/* ccf */,
		"apll_i2sin1_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		1/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN2/* dts */, "apll12_div_in2"/* ccf */,
		"apll_i2sin2_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		2/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN3/* dts */, "apll12_div_in3"/* ccf */,
		"apll_i2sin3_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		3/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_3 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN4/* dts */, "apll12_div_in4"/* ccf */,
		"apll_i2sin4_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		4/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN6/* dts */, "apll12_div_in6"/* ccf */,
		"apll_i2sin6_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		5/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT0/* dts */, "apll12_div_i2sout0"/* ccf */,
		"apll_i2sout0_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		6/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT1/* dts */, "apll12_div_i2sout1"/* ccf */,
		"apll_i2sout1_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		7/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_4 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT2/* dts */, "apll12_div_i2sout2"/* ccf */,
		"apll_i2sout2_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		8/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT3/* dts */, "apll12_div_i2sout3"/* ccf */,
		"apll_i2sout3_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		9/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT4/* dts */, "apll12_div_i2sout4"/* ccf */,
		"apll_i2sout4_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		10/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT6/* dts */, "apll12_div_i2sout6"/* ccf */,
		"apll_i2sout6_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		11/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_5 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_FMI2S/* dts */, "apll12_div_f2s"/* ccf */,
		"apll_fmi2s_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		12/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_TDMOUT_M/* dts */, "apll12_div_tdm"/* ccf */,
		"apll_tdmout_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		13/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_TDMOUT_B/* dts */, "apll12_div_tdb"/* ccf */,
		"apll_tdmout_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		14/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		16/* lsb */),
};


enum subsys_id {
	CCIPLL_PLL_CTRL = 0,
	APMIXEDSYS = 1,
	MFGPLL_PLL_CTRL = 2,
	PTPPLL_PLL_CTRL = 3,
	ARMPLL_BL_PLL_CTRL = 4,
	ARMPLL_B_PLL_CTRL = 5,
	ARMPLL_LL_PLL_CTRL = 6,
	MFGSCPLL_PLL_CTRL = 7,
	PLL_SYS_NUM,
};

static const struct mtk_pll_data *plls_data[PLL_SYS_NUM];
static void __iomem *plls_base[PLL_SYS_NUM];

#define MT6897_PLL_FMAX		(3800UL * MHZ)
#define MT6897_PLL_FMIN		(1500UL * MHZ)
#define MT6897_INTEGER_BITS	8

#if MT_CCF_PLL_DISABLE
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

#define PLL(_id, _name, _reg, _en_reg, _en_mask, _pll_en_bit,		\
			_pwr_reg, _flags, _rst_bar_mask,		\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit,			\
			_pcw_reg, _pcw_shift, _pcwbits) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.en_reg = _en_reg,					\
		.en_mask = _en_mask,					\
		.pll_en_bit = _pll_en_bit,				\
		.pwr_reg = _pwr_reg,					\
		.flags = (_flags | PLL_CFLAGS),				\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT6897_PLL_FMAX,				\
		.fmin = MT6897_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,			\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6897_INTEGER_BITS,			\
	}

#define PLL_HWV(_id, _name, _reg, _en_reg, _div_en_msk, _pll_en_bit,	\
			_pwr_reg, _flags, _rst_bar_mask,		\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit,			\
			_pcw_reg, _pcw_shift, _pcwbits,	\
			_hwv_set_ofs, _hwv_clr_ofs, _hwv_done_ofs,	\
			_hwv_set_sta_ofs, _hwv_clr_sta_ofs, _hwv_shift) {	\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.en_reg = _en_reg,					\
		.en_mask = _div_en_msk,					\
		.pll_en_bit = _pll_en_bit,				\
		.pwr_reg = _pwr_reg,					\
		.flags = (_flags | PLL_CFLAGS | CLK_USE_HW_VOTER | HWV_CHK_FULL_STA),	\
		.rst_bar_mask = _rst_bar_mask,			\
		.fmax = MT6897_PLL_FMAX,				\
		.fmin = MT6897_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6897_INTEGER_BITS,			\
		.hwv_set_ofs = _hwv_set_ofs,				\
		.hwv_clr_ofs = _hwv_clr_ofs,				\
		.hwv_done_ofs = _hwv_done_ofs,				\
		.hwv_set_sta_ofs = _hwv_set_sta_ofs,			\
		.hwv_clr_sta_ofs = _hwv_clr_sta_ofs,			\
		.hwv_shift = _hwv_shift,				\
	}

static const struct mtk_pll_data ccipll_pll_ctrl_plls[] = {
	PLL(CLK_CCIPLL_PLL_CTRL_CCIPLL, "ccipll-pll-ctpll", CCIPLL_CON0/*base*/,
		CCIPLL_CON0, 0, 0/*en*/,
		CCIPLL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		CCIPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		CCIPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data apmixed_plls[] = {
	PLL_HWV(CLK_APMIXED_MAINPLL, "mainpll", MAINPLL_CON0/*base*/,
		MAINPLL_CON0, 0xff000000, 0/*en*/,
		MAINPLL_CON3/*pwr*/, HAVE_RST_BAR | PLL_AO, BIT(23)/*rstb*/,
		MAINPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MAINPLL_CON1, 0, 22/*pcw*/,
		HWV_PLL_SET, HWV_PLL_CLR, HWV_PLL_DONE,
		HWV_PLL_SET_STA, HWV_PLL_CLR_STA, 0),
	PLL_HWV(CLK_APMIXED_UNIVPLL, "univpll", UNIVPLL_CON0/*base*/,
		UNIVPLL_CON0, 0xff000000, 0/*en*/,
		UNIVPLL_CON3/*pwr*/, HAVE_RST_BAR, BIT(23)/*rstb*/,
		UNIVPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		UNIVPLL_CON1, 0, 22/*pcw*/,
		HWV_PLL_SET, HWV_PLL_CLR, HWV_PLL_DONE,
		HWV_PLL_SET_STA, HWV_PLL_CLR_STA, 1),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", MSDCPLL_CON0/*base*/,
		MSDCPLL_CON0, 0, 0/*en*/,
		MSDCPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		MSDCPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MSDCPLL_CON1, 0, 22/*pcw*/),
	PLL_HWV(CLK_APMIXED_MMPLL, "mmpll", MMPLL_CON0/*base*/,
		MMPLL_CON0, 0xff000000, 0/*en*/,
		MMPLL_CON3/*pwr*/, HAVE_RST_BAR, BIT(23)/*rstb*/,
		MMPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MMPLL_CON1, 0, 22/*pcw*/,
		HWV_PLL_SET, HWV_PLL_CLR, HWV_PLL_DONE,
		HWV_PLL_SET_STA, HWV_PLL_CLR_STA, 2),
	PLL(CLK_APMIXED_ADSPPLL, "adsppll", ADSPPLL_CON0/*base*/,
		ADSPPLL_CON0, 0, 0/*en*/,
		ADSPPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		ADSPPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ADSPPLL_CON1, 0, 22/*pcw*/),
	PLL_HWV(CLK_APMIXED_TVDPLL, "tvdpll", TVDPLL_CON0/*base*/,
		TVDPLL_CON0, 0, 0/*en*/,
		TVDPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		TVDPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		TVDPLL_CON1, 0, 22/*pcw*/,
		HWV_PLL_SET, HWV_PLL_CLR, HWV_PLL_DONE,
		HWV_PLL_SET_STA, HWV_PLL_CLR_STA, 3),
	PLL(CLK_APMIXED_APLL1, "apll1", APLL1_CON0/*base*/,
		APLL1_CON0, 0, 0/*en*/,
		APLL1_CON4/*pwr*/, 0, BIT(0)/*rstb*/,
		APLL1_CON1, 24/*pd*/,
		APLL1_TUNER_CON0, AP_PLL_CON3, 0/*tuner*/,
		APLL1_CON2, 0, 32/*pcw*/),
	PLL(CLK_APMIXED_APLL2, "apll2", APLL2_CON0/*base*/,
		APLL2_CON0, 0, 0/*en*/,
		APLL2_CON4/*pwr*/, 0, BIT(0)/*rstb*/,
		APLL2_CON1, 24/*pd*/,
		APLL2_TUNER_CON0, AP_PLL_CON3, 1/*tuner*/,
		APLL2_CON2, 0, 32/*pcw*/),
	PLL(CLK_APMIXED_MPLL, "mpll", MPLL_CON0/*base*/,
		MPLL_CON0, 0, 0/*en*/,
		MPLL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		MPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_EMIPLL, "emipll", EMIPLL_CON0/*base*/,
		EMIPLL_CON0, 0, 0/*en*/,
		EMIPLL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		EMIPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		EMIPLL_CON1, 0, 22/*pcw*/),
	PLL_HWV(CLK_APMIXED_IMGPLL, "imgpll", IMGPLL_CON0/*base*/,
		IMGPLL_CON0, 0xff000000, 0/*en*/,
		IMGPLL_CON3/*pwr*/, HAVE_RST_BAR, BIT(23)/*rstb*/,
		IMGPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		IMGPLL_CON1, 0, 22/*pcw*/,
		HWV_PLL_SET, HWV_PLL_CLR, HWV_PLL_DONE,
		HWV_PLL_SET_STA, HWV_PLL_CLR_STA, 4),
};

static const struct mtk_pll_data mfg_ao_plls[] = {
	PLL(CLK_MFG_AO_MFGPLL, "mfg-ao-mfgpll", MFGPLL_CON0/*base*/,
		MFGPLL_CON0, 0, 0/*en*/,
		MFGPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		MFGPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data ptppll_pll_ctrl_plls[] = {
	PLL(CLK_PTPPLL_PLL_CTRL_PTPPLL, "ptppll-pll-ctpll", PTPPLL_CON0/*base*/,
		PTPPLL_CON0, 0, 0/*en*/,
		PTPPLL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		PTPPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		PTPPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data armpll_bl_pll_ctrl_plls[] = {
	PLL(CLK_ARMPLL_BL_PLL_CTRL_ARMPLL_BL, "armpll-bl-pll-ct-bl", ARMPLL_BL_CON0/*base*/,
		ARMPLL_BL_CON0, 0, 0/*en*/,
		ARMPLL_BL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_BL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_BL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data armpll_b_pll_ctrl_plls[] = {
	PLL(CLK_ARMPLL_B_PLL_CTRL_ARMPLL_B, "armpll-b-pll-ct-b", ARMPLL_B_CON0/*base*/,
		ARMPLL_B_CON0, 0, 0/*en*/,
		ARMPLL_B_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_B_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_B_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data armpll_ll_pll_ctrl_plls[] = {
	PLL(CLK_ARMPLL_LL_PLL_CTRL_ARMPLL_LL, "armpll-ll-pll-ct-ll", ARMPLL_LL_CON0/*base*/,
		ARMPLL_LL_CON0, 0, 0/*en*/,
		ARMPLL_LL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_LL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_LL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data mfgsc_ao_plls[] = {
	PLL(CLK_MFGSC_AO_MFGSCPLL, "mfgsc-ao-mfgscpll", MFGSCPLL_CON0/*base*/,
		MFGSCPLL_CON0, 0, 0/*en*/,
		MFGSCPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		MFGSCPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGSCPLL_CON1, 0, 22/*pcw*/),
};

static int clk_mt6897_pll_registration(enum subsys_id id,
		const struct mtk_pll_data *plls,
		struct platform_device *pdev,
		int num_plls)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	if (id >= PLL_SYS_NUM) {
		pr_notice("%s init invalid id(%d)\n", __func__, id);
		return 0;
	}

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(num_plls);

	mtk_clk_register_plls(node, plls, num_plls,
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	plls_data[id] = plls;
	plls_base[id] = base;

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6897_apmixed_probe(struct platform_device *pdev)
{
	return clk_mt6897_pll_registration(APMIXEDSYS, apmixed_plls,
			pdev, ARRAY_SIZE(apmixed_plls));
}

static int clk_mt6897_armpll_bl_pll_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6897_pll_registration(ARMPLL_BL_PLL_CTRL, armpll_bl_pll_ctrl_plls,
			pdev, ARRAY_SIZE(armpll_bl_pll_ctrl_plls));
}

static int clk_mt6897_armpll_b_pll_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6897_pll_registration(ARMPLL_B_PLL_CTRL, armpll_b_pll_ctrl_plls,
			pdev, ARRAY_SIZE(armpll_b_pll_ctrl_plls));
}

static int clk_mt6897_armpll_ll_pll_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6897_pll_registration(ARMPLL_LL_PLL_CTRL, armpll_ll_pll_ctrl_plls,
			pdev, ARRAY_SIZE(armpll_ll_pll_ctrl_plls));
}

static int clk_mt6897_ccipll_pll_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6897_pll_registration(CCIPLL_PLL_CTRL, ccipll_pll_ctrl_plls,
			pdev, ARRAY_SIZE(ccipll_pll_ctrl_plls));
}

static int clk_mt6897_mfg_ao_probe(struct platform_device *pdev)
{
	return clk_mt6897_pll_registration(MFGPLL_PLL_CTRL, mfg_ao_plls,
			pdev, ARRAY_SIZE(mfg_ao_plls));
}

static int clk_mt6897_mfgsc_ao_probe(struct platform_device *pdev)
{
	return clk_mt6897_pll_registration(MFGSCPLL_PLL_CTRL, mfgsc_ao_plls,
			pdev, ARRAY_SIZE(mfgsc_ao_plls));
}

static int clk_mt6897_ptppll_pll_ctrl_probe(struct platform_device *pdev)
{
	return clk_mt6897_pll_registration(PTPPLL_PLL_CTRL, ptppll_pll_ctrl_plls,
			pdev, ARRAY_SIZE(ptppll_pll_ctrl_plls));
}

static int sp_clk_control(struct mtk_clk_mux *mux, u8 index, u32 mask)
{
	u32 mask_new = 0, index_new = 0;

	mask_new = (mask | (mask << 8) | (mask << 16) | (mask << 24));
	index_new = (index | (index << 8) | (index << 16) | (index << 24));

	regmap_write(mux->regmap, mux->data->clr_ofs, mask_new);
	regmap_write(mux->regmap, mux->data->set_ofs, index_new);

	regmap_write(mux->regmap, mux->data->upd_ofs, 0xf0);

	return 0;
}

static struct sp_clk_ops sp_clk_mt6897_ops = {
	.sp_clk_control = sp_clk_control,
};

static int clk_mt6897_top_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);

	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs),
			clk_data);

	mtk_clk_register_muxes(top_muxes, ARRAY_SIZE(top_muxes), node,
			&mt6897_clk_lock, clk_data);

	mtk_clk_register_composites(top_composites, ARRAY_SIZE(top_composites),
			base, &mt6897_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	set_sp_clk_ops(&sp_clk_mt6897_ops);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6897_vlp_ck_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_VLP_CK_NR_CLK);

	mtk_clk_register_factors(vlp_ck_divs, ARRAY_SIZE(vlp_ck_divs),
			clk_data);

	mtk_clk_register_muxes(vlp_ck_muxes, ARRAY_SIZE(vlp_ck_muxes), node,
			&mt6897_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

/* for suspend LDVT only */
static void pll_force_off_internal(const struct mtk_pll_data *plls,
		void __iomem *base)
{
	void __iomem *rst_reg, *en_reg, *pwr_reg;

	for (; plls->name; plls++) {
		/* do not pwrdn the AO PLLs */
		if ((plls->flags & PLL_AO) == PLL_AO)
			continue;

		if ((plls->flags & HAVE_RST_BAR) == HAVE_RST_BAR) {
			rst_reg = base + plls->en_reg;
			writel(readl(rst_reg) & ~plls->rst_bar_mask,
				rst_reg);
		}

		en_reg = base + plls->en_reg;

		pwr_reg = base + plls->pwr_reg;

		writel(readl(en_reg) & ~plls->en_mask,
				en_reg);
		writel(readl(pwr_reg) | (0x2),
				pwr_reg);
		writel(readl(pwr_reg) & ~(0x1),
				pwr_reg);
	}
}

void mt6897_pll_force_off(void)
{
	int i;

	for (i = 0; i < PLL_SYS_NUM; i++)
		pll_force_off_internal(plls_data[i], plls_base[i]);
}
EXPORT_SYMBOL_GPL(mt6897_pll_force_off);

static const struct of_device_id of_match_clk_mt6897[] = {
	{
		.compatible = "mediatek,mt6897-apmixedsys",
		.data = clk_mt6897_apmixed_probe,
	}, {
		.compatible = "mediatek,mt6897-armpll_bl_pll_ctrl",
		.data = clk_mt6897_armpll_bl_pll_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6897-armpll_b_pll_ctrl",
		.data = clk_mt6897_armpll_b_pll_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6897-armpll_ll_pll_ctrl",
		.data = clk_mt6897_armpll_ll_pll_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6897-ccipll_pll_ctrl",
		.data = clk_mt6897_ccipll_pll_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6897-mfgpll_pll_ctrl",
		.data = clk_mt6897_mfg_ao_probe,
	}, {
		.compatible = "mediatek,mt6897-mfgscpll_pll_ctrl",
		.data = clk_mt6897_mfgsc_ao_probe,
	}, {
		.compatible = "mediatek,mt6897-ptppll_pll_ctrl",
		.data = clk_mt6897_ptppll_pll_ctrl_probe,
	}, {
		.compatible = "mediatek,mt6897-topckgen",
		.data = clk_mt6897_top_probe,
	}, {
		.compatible = "mediatek,mt6897-vlp_cksys",
		.data = clk_mt6897_vlp_ck_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6897_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pd);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt6897_drv = {
	.probe = clk_mt6897_probe,
	.driver = {
		.name = "clk-mt6897",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6897,
	},
};

module_platform_driver(clk_mt6897_drv);
MODULE_LICENSE("GPL");

