/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef _MTK_MMDVFS_V3_MEMORY_H_
#define _MTK_MMDVFS_V3_MEMORY_H_

#if IS_ENABLED(CONFIG_MTK_MMDVFS)
void *mmdvfs_get_vcp_base(phys_addr_t *pa);
bool mmdvfs_is_init_done(void);
#else
static inline void *mmdvfs_get_vcp_base(phys_addr_t *pa)
{
	if (pa)
		*pa = 0;
	return NULL;
}
static inline bool mmdvfs_is_init_done(void) { return false; }
#endif

#define MEM_BASE		mmdvfs_get_vcp_base(NULL)
#define MEM_LOG_FLAG		(MEM_BASE + 0x0)
#define MEM_FREERUN		(MEM_BASE + 0x4)
#define MEM_VSRAM_VOL		(MEM_BASE + 0x8)
#define MEM_IPI_SYNC_FUNC	(MEM_BASE + 0xC)
#define MEM_IPI_SYNC_DATA	(MEM_BASE + 0x10)
#define MEM_VMRC_LOG_FLAG	(MEM_BASE + 0x14)

#define MEM_GENPD_ENABLE_USR(x)	(MEM_BASE + 0x18 + 0x4 * (x)) // CAM, VDE
#define MEM_AGING_CNT_USR(x)	(MEM_BASE + 0x20 + 0x4 * (x)) // CAM, IMG
#define MEM_FRESH_CNT_USR(x)	(MEM_BASE + 0x28 + 0x4 * (x)) // CAM, IMG
#define MEM_FORCE_OPP_PWR(x)	(MEM_BASE + 0x30 + 0x4 * (x)) // POWER_NUM
#define MEM_VOTE_OPP_PWR(x)	(MEM_BASE + 0x40 + 0x4 * (x)) // POWER_NUM
#define MEM_VOTE_OPP_USR(x)	(MEM_BASE + 0x50 + 0x4 * (x)) // USER_NUM(14)

#define MEM_MMDVFS_LP_MODE	(MEM_BASE + 0x88)
#define MEM_VMM_CEIL_ENABLE	(MEM_BASE + 0x8C)

#define MEM_CLKMUX_ENABLE	(MEM_BASE + 0x90)
#define MEM_CLKMUX_ENABLE_DONE	(MEM_BASE + 0x94)
#define MEM_VMM_EFUSE_LOW	(MEM_BASE + 0x98)
#define MEM_VMM_EFUSE_HIGH	(MEM_BASE + 0x9C)

#define MEM_VMM_OPP_VOLT(x)	(MEM_BASE + 0xA0 + 0x4 * (x)) // VMM_OPP_NUM(8)
#define MEM_VDISP_AVS_STEP(x)	(MEM_BASE + 0xC0 + 0x4 * (x)) // OPP_LEVEL(8)
#define MEM_PROFILE_TIMES	(MEM_BASE + 0xE0)
/* reserved: 0xE4/0xE8/0xEC */

#define MEM_USR_OPP_SEC(x)	(MEM_BASE + 0xF0 + 0x4 * (x))  // USER_NUM(32)
#define MEM_USR_OPP_USEC(x)	(MEM_BASE + 0x170 + 0x4 * (x)) // USER_NUM(32)
#define MEM_USR_OPP(x)		(MEM_BASE + 0x1F0 + 0x4 * (x)) // USER_NUM(32)
#define MEM_USR_FREQ(x)		(MEM_BASE + 0x270 + 0x4 * (x)) // USER_NUM(32)

#define MEM_MUX_OPP_SEC(x)	(MEM_BASE + 0x2F0 + 0x4 * (x)) // MUX_NUM(16)
#define MEM_MUX_OPP_USEC(x)	(MEM_BASE + 0x330 + 0x4 * (x)) // MUX_NUM(16)
#define MEM_MUX_OPP(x)		(MEM_BASE + 0x370 + 0x4 * (x)) // MUX_NUM(16)
#define MEM_MUX_MIN(x)		(MEM_BASE + 0x3B0 + 0x4 * (x)) // MUX_NUM(16)
#define MEM_FORCE_CLK(x)	(MEM_BASE + 0x3F0 + 0x4 * (x)) // MUX_NUM(16)

#define MEM_PWR_OPP_SEC(x)	(MEM_BASE + 0x430 + 0x4 * (x)) // POWER_NUM(4)
#define MEM_PWR_OPP_USEC(x)	(MEM_BASE + 0x440 + 0x4 * (x)) // POWER_NUM(4)
#define MEM_PWR_OPP(x)		(MEM_BASE + 0x450 + 0x4 * (x)) // POWER_NUM(4)
#define MEM_PWR_CUR_GEAR(x)	(MEM_BASE + 0x460 + 0x4 * (x)) // POWER_NUM(4)
#define MEM_FORCE_VOL(x)	(MEM_BASE + 0x470 + 0x4 * (x)) // POWER_NUM(4)
/* next start: 0x480 */

#define MEM_REC_PWR_OBJ		4
#define MEM_REC_USR_OBJ		5
#define MEM_REC_MUX_OBJ		3
#define MEM_REC_VMM_DBG_OBJ	5
#define MEM_REC_CNT_MAX		16

#define MEM_REC_MUX_CNT		(MEM_BASE + 0xBB0)
#define MEM_REC_MUX_SEC(x)	(MEM_BASE + 0xBB4 + MEM_REC_MUX_OBJ * 0x4 * (x))
#define MEM_REC_MUX_USEC(x)	(MEM_BASE + 0xBB8 + MEM_REC_MUX_OBJ * 0x4 * (x))
/* mux_id/opp/min/level */
#define MEM_REC_MUX_VAL(x)	(MEM_BASE + 0xBBC + MEM_REC_MUX_OBJ * 0x4 * (x))

#define MEM_REC_VMM_DBG_CNT	(MEM_BASE + 0xC74)
#define MEM_REC_VMM_SEC(x)	(MEM_BASE + 0xC78 + MEM_REC_VMM_DBG_OBJ * 0x4 * (x))
#define MEM_REC_VMM_NSEC(x)	(MEM_BASE + 0xC7C + MEM_REC_VMM_DBG_OBJ * 0x4 * (x))
#define MEM_REC_VMM_VOLT(x)	(MEM_BASE + 0xC80 + MEM_REC_VMM_DBG_OBJ * 0x4 * (x))
#define MEM_REC_VMM_TEMP(x)	(MEM_BASE + 0xC84 + MEM_REC_VMM_DBG_OBJ * 0x4 * (x))
#define MEM_REC_VMM_AVS(x)	(MEM_BASE + 0xC88 + MEM_REC_VMM_DBG_OBJ * 0x4 * (x))

#define MEM_REC_PWR_CNT		(MEM_BASE + 0xDB8)
#define MEM_REC_PWR_SEC(x)	(MEM_BASE + 0xDBC + MEM_REC_PWR_OBJ * 0x4 * (x))
#define MEM_REC_PWR_NSEC(x)	(MEM_BASE + 0xDC0 + MEM_REC_PWR_OBJ * 0x4 * (x))
#define MEM_REC_PWR_ID(x)	(MEM_BASE + 0xDC4 + MEM_REC_PWR_OBJ * 0x4 * (x))
#define MEM_REC_PWR_OPP(x)	(MEM_BASE + 0xDC8 + MEM_REC_PWR_OBJ * 0x4 * (x))

#define MEM_REC_USR_CNT		(MEM_BASE + 0xEBC)
#define MEM_REC_USR_SEC(x)	(MEM_BASE + 0xEC0 + MEM_REC_USR_OBJ * 0x4 * (x))
#define MEM_REC_USR_NSEC(x)	(MEM_BASE + 0xEC4 + MEM_REC_USR_OBJ * 0x4 * (x))
#define MEM_REC_USR_PWR(x)	(MEM_BASE + 0xEC8 + MEM_REC_USR_OBJ * 0x4 * (x))
#define MEM_REC_USR_ID(x)	(MEM_BASE + 0xECC + MEM_REC_USR_OBJ * 0x4 * (x))
#define MEM_REC_USR_OPP(x)	(MEM_BASE + 0xED0 + MEM_REC_USR_OBJ * 0x4 * (x))

#endif

