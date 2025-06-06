// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kconfig.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <slbc.h>
#include <slbc_ops.h>
#include <slbc_ipi.h>
#include <slbc_trace.h>
#include <mtk_slbc_sram.h>
#include <mtk_heap.h>

/* #define CREATE_TRACE_POINTS */
/* #include <slbc_events.h> */
#define trace_slbc_api(f, id)
#define trace_slbc_data(f, data)

#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <linux/cpuidle.h>

static struct pm_qos_request slbc_qos_request;

#define SLBC_WAY_SIZE			0x80000

#if IS_ENABLED(CONFIG_MTK_L3C_PART)
#include <l3c_part.h>
#endif /* CONFIG_MTK_L3C_PART */

/* #define SLBC_TRACE */
#define ENABLE_SLBC

#define SLBC_WAY_A_BASE			0x0f000000
#define SLBC_WAY_B_BASE			0x680000000
#define SLBC_PADDR_MASK			0x00ffffff


#define GID_MAX					64
#define GID_REQ					(-1)
#define GID_MD					5
#define GID_GPU					6
#define GID_GPU_OVL				7
#define GID_VDEC_FRAME				8
#define GID_VDEC_UBE				9

#define BUF_ID_NOT_CARE			0x00000000
#define BUF_ID_GPU			0x0000000f
#define BUF_ID_OVL			0x000000f0
#define BUF_ID_VDEC			0x00000f00


static struct mtk_slbc *slbc;

static int slb_disable;
static int slc_disable;
static int slbc_sram_enable;
static u32 slbc_force;
static int buffer_ref;
static u32 acp_ref;
static u32 slbc_ref;
static u32 slbc_sta;
static u32 slbc_ack_c;
static u32 slbc_ack_g;
static u32 cpuqos_mode;
static u32 slbc_sram_con;
static u32 slbc_cache_used;
static u32 slbc_pmu_0;
static u32 slbc_pmu_1;
static u32 slbc_pmu_2;
static u32 slbc_pmu_3;
static u32 slbc_pmu_4;
static u32 slbc_pmu_5;
static u32 slbc_pmu_6;
static int debug_level;
static int uid_ref[UID_MAX];
static int slbc_mic_num = 3;
static int slbc_inner = 5;
static int slbc_outer = 5;
static int gid_ref[GID_MAX];
static int gid_vld_cnt[GID_MAX];

static u64 req_val_count;
static u64 rel_val_count;
static u64 req_val_min;
static u64 req_val_max;
static u64 req_val_total;
static u64 rel_val_min;
static u64 rel_val_max;
static u64 rel_val_total;

static struct slbc_data test_d;

static LIST_HEAD(slbc_ops_list);
static DEFINE_MUTEX(slbc_ops_lock);

/* 1 in bit is from request done to relase done */
static unsigned long slbc_uid_used;
/* 1 in bit is for mask */
static unsigned long slbc_sid_mask;
/* 1 in bit is under request flow */
static unsigned long slbc_sid_req_q;
/* 1 int bit is under release flow */
static unsigned long slbc_sid_rel_q;
/* 1 int bit is for slot ussage */
static unsigned long slbc_slot_used;

static struct slbc_config p_config[] = {
	/* SLBC_ENTRY(id, sid, max, fix, p, extra, res, cache) */
	SLBC_ENTRY(UID_MM_VENC, 0, 0, 0, 0, 0x0, 0x7c0, 0),
	SLBC_ENTRY(UID_SH_P2, 1, 0, 0, 0, 0x0, 0x03f, 0),
	SLBC_ENTRY(UID_SH_APU, 2, 0, 0, 0, 0x0, 0x03f, 0),
	SLBC_ENTRY(UID_MML, 3, 0, 0, 0, 0x0, 0x030, 0),
	SLBC_ENTRY(UID_AINR, 4, 0, 0, 0, 0x0, 0x800, 0),
	SLBC_ENTRY(UID_DISP, 5, 0, 0, 0, 0x0, 0x030, 0),
	SLBC_ENTRY(UID_AOV_DC, 6, 0, 0, 0, 0x0, 0x007, 0),
	SLBC_ENTRY(UID_AOV_APU, 7, 0, 0, 0, 0x0, 0x038, 0),
	SLBC_ENTRY(UID_AISR_APU, 8, 0, 0, 0, 0x0, 0x00c, 0),
	SLBC_ENTRY(UID_AISR_MML, 9, 0, 0, 0, 0x0, 0x00c, 0),
	SLBC_ENTRY(UID_SH_P1, 10, 0, 0, 0, 0x0, 0x03f, 0),
	SLBC_ENTRY(UID_SMT, 11, 0, 0, 0, 0x0, 0x03f, 0),
	SLBC_ENTRY(UID_APU, 12, 0, 0, 0, 0x0, 0x03f, 0),
	SLBC_ENTRY(UID_AOD, 13, 0, 0, 0, 0x0, 0x7c0, 0),
	SLBC_ENTRY(UID_BIF, 14, 0, 0, 0, 0x0, 0x03f, 0),
};

u32 slbc_sram_read(u32 offset)
{
	if (!slbc_sram_enable)
		return 0;

	if (!slbc->sram_vaddr || offset >= slbc->regsize)
		return 0;

	/* pr_info("#@# %s(%d) regs 0x%x 0%x\n", __func__, __LINE__, slbc->regs, offset); */

	return readl(slbc->sram_vaddr + offset);
}

void slbc_sram_write(u32 offset, u32 val)
{
	if (!slbc_sram_enable)
		return;

	if (!slbc->sram_vaddr || offset >= slbc->regsize)
		return;

	/* pr_info("#@# %s(%d) regs 0x%x 0%x\n", __func__, __LINE__, slbc->regs, offset); */

	writel(val, slbc->sram_vaddr + offset);
}

void slbc_sram_init(struct mtk_slbc *slbc)
{
	int i;

	SLBC_TRACE_REC(LVL_QOS, TYPE_N, 0, 0,
			"slbc_sram addr:0x%lx len:%d", (unsigned long)slbc->regs, slbc->regsize);

	/* print_hex_dump(KERN_INFO, "SLBC: ", DUMP_PREFIX_OFFSET, */
	/* 16, 4, slbc->sram_vaddr, slbc->regsize, 1); */

	for (i = 0; i < slbc->regsize; i += 4)
		writel(0x0, slbc->sram_vaddr + i);
}

static void slbc_set_sram_data(struct slbc_data *d)
{
	SLBC_TRACE_REC(LVL_NORM, TYPE_B, d->uid, 0,
			"set pa:%lx va:%lx", (unsigned long)d->paddr, (unsigned long)d->vaddr);
}

static void slbc_clr_sram_data(struct slbc_data *d)
{
	SLBC_TRACE_REC(LVL_NORM, TYPE_B, d->uid, 0,
			"clr pa:%lx va:%lx", (unsigned long)d->paddr, (unsigned long)d->vaddr);
}

static void slbc_debug_log(const char *fmt, ...)
{
#ifdef SLBC_DEBUG
	static char buf[1024];
	va_list va;
	int len;

	va_start(va, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	if (len)
		pr_info("#@# %s\n", buf);
#endif /* SLBC_DEBUG */
}

static int slbc_get_sid_by_uid(enum slbc_uid uid)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(p_config); i++) {
		if (p_config[i].uid == uid)
			return p_config[i].slot_id;
	}

	return SID_NOT_FOUND;
}

static u32 slbc_read_debug_sram(int sid)
{
	if (sid < 0 || sid >= ARRAY_SIZE(p_config) || sid >= 16)
		return SID_NOT_FOUND;

	if (sid < 8)
		return slbc_sram_read(SLBC_DEBUG_0 + sid * 4);
	else
		return slbc_sram_read(SLBC_DEBUG_8 + (sid - 8) * 4);
}

void slbc_force_cmd(unsigned int force)
{
	if ((force & 0xffff) < FR_MAX) {
		slbc_force = force;
		slbc_force_scmi_cmd(force);
	}
}

static int slbc_request_cache(struct slbc_data *d)
{
	int ret = 0;

	/* slbc_debug_log("%s: TP_CACHE\n", __func__); */

	ret = _slbc_request_cache_scmi(d);

	return ret;
}

static int slbc_request_status(struct slbc_data *d)
{
	int ret = 0;
	int uid = d->uid;
	int sid;
	struct slbc_config *config;

	/* slbc_debug_log("%s: TP_BUFFER\n", __func__); */

	if (uid <= UID_ZERO || uid > UID_MAX)
		d->config = NULL;
	else {
		sid = slbc_get_sid_by_uid((enum slbc_uid)uid);
		if (sid != SID_NOT_FOUND) {
			d->sid = sid;
			d->config = &p_config[sid];
			config = (struct slbc_config *)d->config;
			ret = _slbc_buffer_status_scmi(d);
		}
	}

	return ret;
}

static int slbc_request_buffer(struct slbc_data *d)
{
	int ret = 0;
	int uid = d->uid;
	int sid;
	struct slbc_config *config;

	/* slbc_debug_log("%s: TP_BUFFER\n", __func__); */

	if (uid <= UID_ZERO || uid > UID_MAX)
		d->config = NULL;
	else {
		sid = slbc_get_sid_by_uid((enum slbc_uid)uid);
		if (sid != SID_NOT_FOUND) {
			d->sid = sid;
			d->config = &p_config[sid];
			config = (struct slbc_config *)d->config;
		}
	}

	ret = _slbc_request_buffer_scmi(d);

	if (!ret) {
		slbc_set_sram_data(d);

#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
		buffer_ref = slbc_sram_read(SLBC_BUFFER_REF);
#else
		buffer_ref++;
#endif /* CONFIG_MTK_SLBC_IPI */
	}

	return ret;
}

static int slbc_request_acp(void *ptr)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_L3C_PART)
	struct slbc_data *d = ptr;

	/* slbc_debug_log("%s: TP_ACP\n", __func__); */

	if (!acp_ref) {
#if IS_ENABLED(CONFIG_PM_SLEEP)
		__pm_stay_awake(slbc->ws);
#endif /* CONFIG_PM_SLEEP */

		cpu_latency_qos_update_request(&slbc_qos_request,
				slbc->slbc_qos_latency);
	}

	if (acp_ref++ == 0) {
		const unsigned long ratio_bits = d->flag & FG_ACP_BITS;
		int ratio = find_first_bit(&ratio_bits, 32)
			- ACP_ONLY_BIT;

		slbc_debug_log("%s: before %s:0x%x, %s:0x%x\n",
				__func__,
				"MTK_L3C_PART_MCU",
				mtk_l3c_get_part(MTK_L3C_PART_MCU),
				"MTK_L3C_PART_ACP",
				mtk_l3c_get_part(MTK_L3C_PART_ACP));
		if (d->flag & FG_ACP_ONLY)
			mtk_l3c_set_mcu_part(4 - ratio);
		else
			mtk_l3c_set_mcu_part(4);
		mtk_l3c_set_acp_part(ratio);
		slbc_debug_log("%s: after %s:0x%x, %s:0x%x\n",
				__func__,
				"MTK_L3C_PART_MCU",
				mtk_l3c_get_part(MTK_L3C_PART_MCU),
				"MTK_L3C_PART_ACP",
				mtk_l3c_get_part(MTK_L3C_PART_ACP));
	}
#endif /* CONFIG_MTK_L3C_PART */

	return ret;
}

int slbc_status(struct slbc_data *d)
{
	int ret = 0;

	if ((d->type) == TP_BUFFER)
		ret = slbc_request_status(d);

	SLBC_TRACE_REC(LVL_QOS, TYPE_B, d->uid, ret,
			"uid 0x%x ret %d", d->uid, ret);

	return ret;
}

int slbc_request(struct slbc_data *d)
{
	int ret = 0;
	int sid;
	u64 begin, val;

	if (slbc_enable == 0)
		return -EDISABLED;

	begin = ktime_get_ns();

	if ((d->type) == TP_BUFFER) {
		ret = slbc_request_buffer(d);

		sid = slbc_get_sid_by_uid((enum slbc_uid)d->uid);
		if (sid != SID_NOT_FOUND)
			d->slot_used = p_config[sid].res_slot;

		if (!d->paddr)
			ret = -1;
		else
			d->emi_paddr = (void __iomem *)((((unsigned long)d->paddr)
					 & SLBC_PADDR_MASK) | SLBC_WAY_B_BASE);
	} else if ((d->type) == TP_CACHE)
		ret = slbc_request_cache(d);
	else if ((d->type) == TP_ACP)
		ret = slbc_request_acp(d);

	SLBC_TRACE_REC(LVL_QOS, TYPE_B, d->uid, ret,
			"uid 0x%x ret %d d->ret %d pa 0x%lx emipa 0x%lx size 0x%lx",
			d->uid, ret, d->ret,
			(unsigned long)d->paddr, (unsigned long)d->emi_paddr, d->size);

	if (!ret) {
#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
		slbc_ref = slbc_sram_read(SLBC_REF);
#else
		slbc_ref++;
#endif /* CONFIG_MTK_SLBC_IPI */
	}

	val = (ktime_get_ns() - begin) / 1000000;
	req_val_count++;
	req_val_total += val;
	req_val_max = max(val, req_val_max);
	if (!req_val_min)
		req_val_min = val;
	else
		req_val_min = min(val, req_val_min);

	return ret;
}

static int slbc_release_cache(struct slbc_data *d)
{
	int ret = 0;

	/* slbc_debug_log("%s: TP_CACHE\n", __func__); */

	ret = _slbc_release_cache_scmi(d);

	return ret;
}

static int slbc_release_buffer(struct slbc_data *d)
{
	int ret = 0;

	/* slbc_debug_log("%s: TP_BUFFER\n", __func__); */

	ret = _slbc_release_buffer_scmi(d);

	if (!ret) {
		slbc_clr_sram_data(d);

#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
		buffer_ref = slbc_sram_read(SLBC_BUFFER_REF);
#else
		buffer_ref--;
#endif /* CONFIG_MTK_SLBC_IPI */
		WARN_ON(buffer_ref < 0);
	}

	return ret;
}

static int slbc_release_acp(void *ptr)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_L3C_PART)
	struct slbc_data *d = ptr;

	/* slbc_debug_log("%s: TP_ACP\n", __func__); */

	if (--acp_ref == 0) {
		slbc_debug_log("%s: before %s:0x%x, %s:0x%x\n",
				__func__,
				"MTK_L3C_PART_MCU",
				mtk_l3c_get_part(MTK_L3C_PART_MCU),
				"MTK_L3C_PART_ACP",
				mtk_l3c_get_part(MTK_L3C_PART_ACP));
		mtk_l3c_set_mcu_part(4);
		mtk_l3c_set_acp_part(1);
		slbc_debug_log("%s: after %s:0x%x, %s:0x%x\n",
				__func__,
				"MTK_L3C_PART_MCU",
				mtk_l3c_get_part(MTK_L3C_PART_MCU),
				"MTK_L3C_PART_ACP",
				mtk_l3c_get_part(MTK_L3C_PART_ACP));
	}
	WARN_ON(acp_ref < 0);

	if (!acp_ref) {
#if IS_ENABLED(CONFIG_PM_SLEEP)
		__pm_relax(slbc->ws);
#endif /* CONFIG_PM_SLEEP */

		cpu_latency_qos_update_request(&slbc_qos_request,
				PM_QOS_DEFAULT_VALUE);
	}
#endif /* CONFIG_MTK_L3C_PART */

	return ret;
}

int slbc_release(struct slbc_data *d)
{
	int ret = 0;
	u64 begin, val;

	begin = ktime_get_ns();

	if ((d->type) == TP_BUFFER) {
		ret = slbc_release_buffer(d);
		d->size = 0;
	} else if ((d->type) == TP_CACHE)
		ret = slbc_release_cache(d);
	else if ((d->type) == TP_ACP)
		ret = slbc_release_acp(d);

	SLBC_TRACE_REC(LVL_QOS, TYPE_B, d->uid, ret,
			"uid 0x%x ret %d d->ret %d pa 0x%lx size 0x%lx",
			d->uid, ret, d->ret,
			(unsigned long)d->paddr, d->size);

	if (!ret) {
#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
		slbc_ref = slbc_sram_read(SLBC_REF);
#else
		slbc_ref--;
#endif /* CONFIG_MTK_SLBC_IPI */
	}

	val = (ktime_get_ns() - begin) / 1000000;
	rel_val_count++;
	rel_val_total += val;
	rel_val_max = max(val, rel_val_max);
	if (!rel_val_min)
		rel_val_min = val;
	else
		rel_val_min = min(val, rel_val_min);

	return ret;
}

static void slbc_sync_mb(void *task)
{
	dsb(sy);
	isb();
	/* Pairs with smp_wmb() */
	smp_rmb();
}

static void slbc_mem_barrier(void)
{
	/*
	 * Ensure all data update before kicking the CPUs.
	 * Pairs with smp_rmb() in slbc_sync_mb().
	 */
	smp_wmb();
	dsb(sy);
	isb();

	smp_call_function(slbc_sync_mb, NULL, 1);
}

int slbc_power_on(struct slbc_data *d)
{
	unsigned int uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (!d)
		return -EINVAL;

	uid = d->uid;
	if (uid <= UID_ZERO || uid >= UID_MAX)
		return -EINVAL;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	/* slbc_debug_log("%s: %s flag %x", __func__, */
	/* slbc_uid_str[uid], d->flag); */

	return 0;
}

int slbc_power_off(struct slbc_data *d)
{
	unsigned int uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (!d)
		return -EINVAL;

	uid = d->uid;
	if (uid <= UID_ZERO || uid >= UID_MAX)
		return -EINVAL;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	/* slbc_debug_log("%s: %s flag %x", __func__, */
	/* slbc_uid_str[uid], d->flag); */

	return 0;
}

int slbc_secure_on(struct slbc_data *d)
{
	unsigned int uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (!d)
		return -EINVAL;

	uid = d->uid;
	if (uid <= UID_ZERO || uid >= UID_MAX)
		return -EINVAL;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s flag %x", __func__, slbc_uid_str[uid], d->flag);

	return 0;
}

int slbc_secure_off(struct slbc_data *d)
{
	unsigned int uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (!d)
		return -EINVAL;

	uid = d->uid;
	if (uid <= UID_ZERO || uid >= UID_MAX)
		return -EINVAL;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s flag %x", __func__, slbc_uid_str[uid], d->flag);

	return 0;
}

void slbc_update_mm_bw(unsigned int bw)
{
	slbc_sram_write(SLBC_MM_EST_BW, bw);
}

void slbc_update_mic_num(unsigned int num)
{
	int i;

	slbc_mic_num = num;
	slbc_mic_num_cmd(num);

	if (!uid_ref[UID_HIFI3]) {
		for (i = 0; i < ARRAY_SIZE(p_config); i++) {
			if (p_config[i].uid == UID_HIFI3) {
				if (num == 3)
					p_config[i].res_slot = 0xe00;
				else
					p_config[i].res_slot = 0x200;
			}
		}
	}
}

void slbc_update_inner(unsigned int inner)
{
	slbc_inner = inner;
	slbc_inner_cmd(inner);
}

void slbc_update_outer(unsigned int outer)
{
	slbc_outer = outer;
	slbc_outer_cmd(outer);
}

int slbc_gid_request(enum slc_ach_uid uid, int *gid, struct slbc_gid_data *data)
{
	if (*gid >= GID_MAX)
		return 0;

	if (data->sign != 0x51ca11ca)
		return 0;

	SLBC_TRACE_REC(LVL_QOS, TYPE_C, uid, 0, "gid:%d", *gid);

	switch (uid) {
	case ID_MD:
		if (*gid == GID_REQ)
			*gid = GID_MD;
		else if (*gid != GID_MD)
			return 0;
		break;
	case ID_VDEC_FRAME:
		if (*gid != GID_VDEC_FRAME)
			return 0;
		break;
	case ID_VDEC_UBE:
		if (*gid == GID_REQ)
			*gid = GID_VDEC_UBE;
		else if (*gid != GID_VDEC_UBE)
			return 0;
		break;
	case ID_GPU:
		if (*gid == GID_REQ)
			*gid = GID_GPU;
		else if (*gid != GID_GPU)
			return 0;
		break;
	case ID_GPU_W:
	case ID_OVL_R:
		if (*gid != GID_GPU_OVL)
			return 0;
		break;
	default:
		SLBC_TRACE_REC(LVL_ERR, TYPE_C, uid, 0, "unrecognized uid");
		return 0;
	}

	gid_ref[*gid]++;

	/* Set M/G and G/P tables */
	_slbc_ach_scmi(IPI_SLBC_GID_REQUEST_FROM_AP, uid, *gid, data);

	return 0;
}

int slbc_gid_release(enum slc_ach_uid uid, int gid)
{
	SLBC_TRACE_REC(LVL_QOS, TYPE_C, uid, 0, "gid:%d", gid);
	if (gid >= GID_MAX)
		return -EINVAL;

	switch (uid) {
	case ID_MD:
		if (gid != GID_MD)
			return -EINVAL;
		break;
	case ID_VDEC_FRAME:
		if (gid != GID_VDEC_FRAME)
			return -EINVAL;
		break;
	case ID_VDEC_UBE:
		if (gid != GID_VDEC_UBE)
			return -EINVAL;
		break;
	case ID_GPU:
		if (gid != GID_GPU)
			return -EINVAL;
		break;
	case ID_GPU_W:
	case ID_OVL_R:
		if (gid != GID_GPU_OVL)
			return -EINVAL;
		break;
	default:
		SLBC_TRACE_REC(LVL_ERR, TYPE_C, uid, 0, "unrecognized uid");
		return -EINVAL;
	}

	gid_ref[gid]--;

	/* Clear M/G and G/P tables */
	_slbc_ach_scmi(IPI_SLBC_GID_RELEASE_FROM_AP, uid, gid, NULL);

	return 0;
}

int slbc_roi_update(enum slc_ach_uid uid, int gid, struct slbc_gid_data *data)
{
	if (gid >= GID_MAX)
		return -EINVAL;

	_slbc_ach_scmi(IPI_SLBC_ROI_UPDATE_FROM_AP, uid, gid, data);

	return 0;
}

int slbc_validate(enum slc_ach_uid uid, int gid)
{
	SLBC_TRACE_REC(LVL_NORM, TYPE_C, uid, 0, "gid:%d", gid);
	if (gid >= GID_MAX)
		return -EINVAL;

	gid_vld_cnt[gid]++;
	_slbc_ach_scmi(IPI_SLBC_GID_VALID_FROM_AP, uid, gid, NULL);

	return 0;
}

int slbc_invalidate(enum slc_ach_uid uid, int gid)
{
	SLBC_TRACE_REC(LVL_NORM, TYPE_C, uid, 0, "gid:%d", gid);
	if (gid >= GID_MAX)
		return -EINVAL;

	gid_vld_cnt[gid]--;
	_slbc_ach_scmi(IPI_SLBC_GID_INVALID_FROM_AP, uid, gid, NULL);

	return 0;
}

int slbc_read_invalidate(enum slc_ach_uid uid, int gid, int enable)
{
	struct slbc_gid_data data;

	SLBC_TRACE_REC(LVL_NORM, TYPE_C, uid, 0, "gid:%d", gid);
	if (gid >= GID_MAX)
		return -EINVAL;

	data.bw = enable;
	_slbc_ach_scmi(IPI_SLBC_GID_READ_INVALID_FROM_AP, uid, gid, &data);

	return 0;
}

static void slbc_dump_data(struct seq_file *m, struct slbc_data *d)
{
	unsigned int uid = d->uid;

	seq_printf(m, "ID %s\t", slbc_uid_str[uid]);

	if (test_bit(uid, &slbc_uid_used))
		seq_puts(m, " activate\n");
	else
		seq_puts(m, " deactivate\n");

	seq_printf(m, "uid: %d\n", uid);
	seq_printf(m, "type: 0x%x\n", d->type);
	seq_printf(m, "size: %ld\n", d->size);
	seq_printf(m, "paddr: %lx\n", (unsigned long)d->paddr);
	seq_printf(m, "vaddr: %lx\n", (unsigned long)d->vaddr);
	seq_printf(m, "sid: %d\n", d->sid);
	seq_printf(m, "slot_used: 0x%x\n", d->slot_used);
	seq_printf(m, "config: %p\n", d->config);
	seq_printf(m, "ref: %d\n", d->ref);
	seq_printf(m, "pwr_ref: %d\n", d->pwr_ref);
}

static int dbg_slbc_proc_show(struct seq_file *m, void *v)
{
	struct slbc_ops *ops;
	int i;
	int sid;

	slbc_sspm_sram_update();

#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
	slbc_uid_used = slbc_sram_read(SLBC_UID_USED);
	slbc_sid_mask = slbc_sram_read(SLBC_SID_MASK);
	slbc_sid_req_q = slbc_sram_read(SLBC_SID_REQ_Q);
	slbc_sid_rel_q = slbc_sram_read(SLBC_SID_REL_Q);
	slbc_slot_used = slbc_sram_read(SLBC_SLOT_USED);
	slbc_force = slbc_sram_read(SLBC_FORCE);
	buffer_ref = slbc_sram_read(SLBC_BUFFER_REF);
	slbc_ref = slbc_sram_read(SLBC_REF);
	slbc_sta = slbc_sram_read(SLBC_STA);
	slbc_ack_c = slbc_sram_read(SLBC_ACK_C);
	slbc_ack_g = slbc_sram_read(SLBC_ACK_G);
	cpuqos_mode = slbc_sram_read(CPUQOS_MODE);
	slbc_sram_con = slbc_sram_read(SLBC_SRAM_CON);
	slbc_cache_used = slbc_sram_read(SLBC_CACHE_USED);
	slbc_pmu_0 = slbc_sram_read(SLBC_PMU_0);
	slbc_pmu_1 = slbc_sram_read(SLBC_PMU_1);
	slbc_pmu_2 = slbc_sram_read(SLBC_PMU_2);
	slbc_pmu_3 = slbc_sram_read(SLBC_PMU_3);
	slbc_pmu_4 = slbc_sram_read(SLBC_PMU_4);
	slbc_pmu_5 = slbc_sram_read(SLBC_PMU_5);
	slbc_pmu_6 = slbc_sram_read(SLBC_PMU_6);

	for (i = 0; i < UID_MAX; i++) {
		sid = slbc_get_sid_by_uid(i);
		if (sid != SID_NOT_FOUND)
			uid_ref[i] = slbc_read_debug_sram(sid);
	}
#endif /* CONFIG_MTK_SLBC_IPI */

	seq_printf(m, "slbc_enable %x\n", slbc_enable);
	seq_printf(m, "slb_disable %x\n", slb_disable);
	seq_printf(m, "slc_disable %x\n", slc_disable);
	seq_printf(m, "slbc_sram_enable %x\n", slbc_sram_enable);
	seq_printf(m, "slbc_uid_used 0x%lx\n", slbc_uid_used);
	seq_printf(m, "slbc_sid_mask 0x%lx\n", slbc_sid_mask);
	seq_printf(m, "slbc_sid_req_q 0x%lx\n", slbc_sid_req_q);
	seq_printf(m, "slbc_sid_rel_q 0x%lx\n", slbc_sid_rel_q);
	seq_printf(m, "slbc_slot_used 0x%lx\n", slbc_slot_used);
	seq_printf(m, "slbc_force 0x%x\n", slbc_force);
	seq_printf(m, "buffer_ref %x\n", buffer_ref);
	seq_printf(m, "acp_ref %x\n", acp_ref);
	seq_printf(m, "slbc_ref %x\n", slbc_ref);
	seq_printf(m, "debug_level %x\n", debug_level);
	seq_printf(m, "slbc_sta %x\n", slbc_sta);
	seq_printf(m, "slbc_ack_c %x\n", slbc_ack_c);
	seq_printf(m, "slbc_ack_g %x\n", slbc_ack_g);
	seq_printf(m, "cpuqos_mode %x\n", cpuqos_mode);
	seq_printf(m, "slbc_sram_con %x\n", slbc_sram_con);
	seq_printf(m, "slbc_cache_used %x\n", slbc_cache_used);
	seq_printf(m, "slbc_pmu_0 %x\n", slbc_pmu_0);
	seq_printf(m, "slbc_pmu_1 %x\n", slbc_pmu_1);
	seq_printf(m, "slbc_pmu_2 %x\n", slbc_pmu_2);
	seq_printf(m, "slbc_pmu_3 %x\n", slbc_pmu_3);
	seq_printf(m, "slbc_pmu_4 %x\n", slbc_pmu_4);
	seq_printf(m, "slbc_pmu_5 %x\n", slbc_pmu_5);
	seq_printf(m, "slbc_pmu_6 %x\n", slbc_pmu_6);
	seq_printf(m, "mic_num %x\n", slbc_mic_num);
	seq_printf(m, "inner %x\n", slbc_inner);
	seq_printf(m, "outer %x\n", slbc_outer);
	if (slbc_all_cache_mode) {
		seq_puts(m, "gid         ");
		for (i = 0; i < UID_MAX; i++)
			seq_printf(m, "%2d", i);
		seq_puts(m, "\n");
		seq_puts(m, "gid_ref     ");
		for (i = 0; i < UID_MAX; i++)
			seq_printf(m, "%2x", gid_ref[i]);
		seq_puts(m, "\n");
		seq_puts(m, "gid_vld_cnt ");
		for (i = 0; i < UID_MAX; i++)
			seq_printf(m, "%2x", gid_vld_cnt[i]);
		seq_puts(m, "\n");
	}

	for (i = 0; i < UID_MAX; i++) {
		sid = slbc_get_sid_by_uid(i);
		if (sid != SID_NOT_FOUND)
			seq_printf(m, "uid_ref %s %x\n",
					slbc_uid_str[i], uid_ref[i]);
	}

	mutex_lock(&slbc_ops_lock);
	list_for_each_entry(ops, &slbc_ops_list, node) {
		struct slbc_data *d = ops->data;

		slbc_dump_data(m, d);
	}
	mutex_unlock(&slbc_ops_lock);

	if (req_val_count) {
		seq_printf(m, "stat req count:%lld min:%lld avg:%lld max:%lld\n",
				req_val_count, req_val_min,
				req_val_total / req_val_count, req_val_max);
		seq_printf(m, "stat rel count:%lld min:%lld avg:%lld max:%lld\n",
				rel_val_count, rel_val_min,
				rel_val_total / rel_val_count, rel_val_max);
	}

	seq_puts(m, "\n");

	return 0;
}

static ssize_t dbg_slbc_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	int ret = 0;
	char *buf = (char *) __get_free_page(GFP_USER);
	char cmd[64];
	unsigned long val_1;
	unsigned long val_2;

	if (!buf)
		return -ENOMEM;

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	ret = sscanf(buf, "%63s %lx %lx", cmd, &val_1, &val_2);
	if (ret < 1) {
		ret = -EPERM;
		goto out;
	}

	if (!strcmp(cmd, "slbc_enable")) {
		if (slbc_enable == !!val_1) {
			pr_info("slbc: slbc_enable config has already %d\n", slbc_enable);
			goto out;
		}

		slbc_enable = !!val_1;
		pr_info("slbc enable %d\n", slbc_enable);

		/* ask slb user to release slb */
		if (slbc_enable == 0) {
			struct slbc_ops *ops;

			mutex_lock(&slbc_ops_lock);
			list_for_each_entry(ops, &slbc_ops_list, node) {
				struct slbc_data *d = ops->data;
				unsigned int uid = d->uid;

				if (test_bit(uid, &slbc_uid_used))
					ops->deactivate(d);
			}
			mutex_unlock(&slbc_ops_lock);
		}

		/* enable/disable slc */
		slbc_sspm_enable(slbc_enable);
	} else if (!strcmp(cmd, "slb_disable")) {
		pr_info("slb disable %ld\n", val_1);
		slb_disable = val_1;
		slbc_sspm_slb_disable((int)!!val_1);
	} else if (!strcmp(cmd, "slc_disable")) {
		pr_info("slc disable %ld\n", val_1);
		slc_disable = val_1;
		slbc_sspm_slc_disable((int)!!val_1);
	} else if (!strcmp(cmd, "slbc_uid_used")) {
		slbc_uid_used = val_1;
		slbc_sram_write(SLBC_UID_USED, slbc_uid_used);
	} else if (!strcmp(cmd, "slbc_sid_mask")) {
		slbc_sid_mask = val_1;
		slbc_sram_write(SLBC_SID_MASK, slbc_sid_mask);
	} else if (!strcmp(cmd, "slbc_sid_req_q")) {
		slbc_sid_req_q = val_1;
		slbc_sram_write(SLBC_SID_REQ_Q, slbc_sid_req_q);
	} else if (!strcmp(cmd, "slbc_sid_rel_q")) {
		slbc_sid_rel_q = val_1;
		slbc_sram_write(SLBC_SID_REL_Q, slbc_sid_rel_q);
	} else if (!strcmp(cmd, "slbc_slot_used")) {
		slbc_slot_used = val_1;
		slbc_sram_write(SLBC_SLOT_USED, slbc_slot_used);
	} else if (!strcmp(cmd, "test_acp_request")) {
		test_d.uid = UID_TEST_ACP;
		test_d.type  = TP_ACP;
		test_d.flag = val_1;
		ret = slbc_request(&test_d);
	} else if (!strcmp(cmd, "test_acp_release")) {
		test_d.uid = UID_TEST_ACP;
		test_d.type  = TP_ACP;
		test_d.flag = val_1;
		ret = slbc_release(&test_d);
	} else if (!strcmp(cmd, "test_slb_request")) {
		test_d.uid = val_1;
		test_d.type  = TP_BUFFER;
		ret = slbc_request(&test_d);
	} else if (!strcmp(cmd, "test_slb_release")) {
		test_d.uid = val_1;
		test_d.type  = TP_BUFFER;
		ret = slbc_release(&test_d);
	} else if (!strcmp(cmd, "slbc_force")) {
		slbc_force = val_1;
		slbc_force_cmd(slbc_force);
	} else if (!strcmp(cmd, "mic_num")) {
		slbc_update_mic_num(val_1);
	} else if (!strcmp(cmd, "inner")) {
		slbc_update_inner(val_1);
	} else if (!strcmp(cmd, "outer")) {
		slbc_update_outer(val_1);
	} else if (!strcmp(cmd, "debug_level")) {
		debug_level = val_1;
	} else if (!strcmp(cmd, "sram")) {
		pr_info("#@# %s(%d) slbc->regs 0x%lx slbc->regsize 0x%x\n",
				__func__, __LINE__,
				(unsigned long)slbc->regs, slbc->regsize);

		print_hex_dump(KERN_INFO, "SLBC: ", DUMP_PREFIX_OFFSET,
				16, 4, slbc->sram_vaddr, slbc->regsize, 1);
	} else {
		SLBC_TRACE_REC(LVL_WARN, TYPE_N, 0, 0,
				"wrong cmd %s val %ld", cmd, val_1);
	}

out:
	free_page((unsigned long)buf);

	if (ret < 0)
		return ret;

	return count;
}

PROC_FOPS_RW(dbg_slbc);

static int trace_slbc_proc_show(struct seq_file *m, void *v)
{
	slbc_trace_dump(m);
	return 0;
}

static ssize_t trace_slbc_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	int ret = 0;
	char *buf = (char *) __get_free_page(GFP_USER);
	char cmd[64];
	unsigned long val_1;
	unsigned long val_2;

	if (!buf)
		return -ENOMEM;

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	ret = sscanf(buf, "%63s %lx %lx", cmd, &val_1, &val_2);
	if (ret < 1) {
		ret = -EPERM;
		goto out;
	}

	if (!strcmp(cmd, "enable")) {
		slbc_trace->enable = (val_1) ? 1 : 0;
	} else if (!strcmp(cmd, "log_enable")) {
		slbc_trace->log_enable = (val_1) ? 1 : 0;
	} else if (!strcmp(cmd, "rec_enable")) {
		slbc_trace->rec_enable = (val_1) ? 1 : 0;
	} else if (!strcmp(cmd, "log_mask")) {
		slbc_trace->log_mask = val_1;
	} else if (!strcmp(cmd, "rec_mask")) {
		slbc_trace->rec_mask = val_1;
	} else if (!strcmp(cmd, "dump_mask")) {
		slbc_trace->dump_mask = val_1;
	} else if (!strcmp(cmd, "focus_b")) {
		slbc_trace->focus = 1;
		slbc_trace->focus_type |= 1UL << TYPE_B;
		slbc_trace->focus_id = 0;
	} else if (!strcmp(cmd, "focus_b_id")) {
		if (!(slbc_trace->focus_type & 1UL << TYPE_B)) {
			slbc_trace->focus_type |= 1UL << TYPE_B;
			slbc_trace->focus_id = 0;
		}
			slbc_trace->focus = 1;
			slbc_trace->focus_id |= 1UL << val_1;
	} else if (!strcmp(cmd, "focus_c")) {
		slbc_trace->focus = 1;
		slbc_trace->focus_type |= 1UL << TYPE_C;
		slbc_trace->focus_id = 0;
	} else if (!strcmp(cmd, "focus_c_id")) {
		if (!(slbc_trace->focus_type & 1UL << TYPE_C)) {
			slbc_trace->focus_type |= 1UL << TYPE_C;
			slbc_trace->focus_id = 0;
		}
			slbc_trace->focus = 1;
			slbc_trace->focus_id |= 1UL << val_1;
	} else if (!strcmp(cmd, "focus_reset")) {
		slbc_trace->focus_type = 0;
		slbc_trace->focus_id = 0;
		slbc_trace->focus = 0;
	} else if (!strcmp(cmd, "dump_num")) {
		slbc_trace->dump_num = (u32)val_1;
	} else {
		SLBC_TRACE_REC(LVL_WARN, TYPE_N, 0, 0,
				"wrong cmd %s val %ld", cmd, val_1);
	}

out:
	free_page((unsigned long)buf);

	if (ret < 0)
		return ret;

	return count;
}

PROC_FOPS_RW(trace_slbc);

static int slbc_create_debug_fs(void)
{
	int i;
	struct proc_dir_entry *dir = NULL;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
		void *data;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(dbg_slbc),
		PROC_ENTRY(trace_slbc),
	};

	/* create /proc/slbc */
	dir = proc_mkdir("slbc", NULL);
	if (!dir) {
		SLBC_TRACE_REC(LVL_ERR, TYPE_N, 0, -ENOMEM,
				"fail to create /proc/slbc");

		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create_data(entries[i].name, 0660,
					dir, entries[i].fops, entries[i].data))
			SLBC_TRACE_REC(LVL_ERR, TYPE_N, 0, 0,
					"create /proc/slbc/%s failed", entries[i].name);
	}

	return 0;
}

static struct slbc_common_ops common_ops = {
	.slbc_status = slbc_status,
	.slbc_request = slbc_request,
	.slbc_release = slbc_release,
	.slbc_power_on = slbc_power_on,
	.slbc_power_off = slbc_power_off,
	.slbc_secure_on = slbc_secure_on,
	.slbc_secure_off = slbc_secure_off,
	.slbc_sram_read = slbc_sram_read,
	.slbc_sram_write = slbc_sram_write,
	.slbc_update_mm_bw = slbc_update_mm_bw,
	.slbc_update_mic_num = slbc_update_mic_num,
	.slbc_gid_request = slbc_gid_request,
	.slbc_gid_release = slbc_gid_release,
	.slbc_roi_update = slbc_roi_update,
	.slbc_validate = slbc_validate,
	.slbc_invalidate = slbc_invalidate,
	.slbc_read_invalidate = slbc_read_invalidate,
};

static struct slbc_ipi_ops ipi_ops = {
	.slbc_request_acp = slbc_request_acp,
	.slbc_release_acp = slbc_release_acp,
	.slbc_mem_barrier = slbc_mem_barrier,
};

void slbc_get_gid_for_dma(struct dma_buf *dmabuf_2)
{
	int gid = GID_REQ;
	struct slbc_gid_data *gid_data;
	unsigned int buffer_fd, producer, consumer;
	struct iosys_map map;
	struct dma_buf *dmabuf_1;
	int ret;

	memset(&map, 0, sizeof(map));
	ret = dma_buf_vmap(dmabuf_2, &map);
	if (ret) {
		SLBC_TRACE_REC(LVL_ERR, TYPE_C, 0, ret, "dma_buf_vmap failed");
		return;
	}

	gid_data = (struct slbc_gid_data *)map.vaddr;
	buffer_fd = gid_data->buffer_fd;
	producer = gid_data->producer;
	consumer = gid_data->consumer;
	SLBC_TRACE_REC(LVL_QOS, TYPE_C, 0, ret, "buffer_fd:%d  producer:%d  consumer:%d",
			buffer_fd, producer, consumer);


	/* mapping producre/consumer to GID */
	switch (producer) {
	case BUF_ID_GPU | BUF_ID_OVL:
	case BUF_ID_GPU:
		if (consumer & BUF_ID_OVL)	/* GPU to OVL */
			gid = GID_GPU_OVL;
		else                        /* GPU only */
			gid = GID_GPU;
		break;
	case BUF_ID_VDEC:
		gid = GID_VDEC_FRAME;
		break;
	default:
		if (consumer & BUF_ID_GPU)  /* GPU only */
			gid = GID_GPU;
	}
	SLBC_TRACE_REC(LVL_QOS, TYPE_C, 0, 0, "gid:%d", gid);


	dmabuf_1 = dma_buf_get(buffer_fd);
	if (IS_ERR(dmabuf_1)) {
		SLBC_TRACE_REC(LVL_ERR, TYPE_C, 0, 0, "dma_buf_get failed");
		return;
	}
	ret = dma_buf_set_gid(dmabuf_1, gid);
	if (ret)
		SLBC_TRACE_REC(LVL_ERR, TYPE_C, 0, ret, "dma_buf_set_gid failed");

	dma_buf_put(dmabuf_1);
	dma_buf_vunmap(dmabuf_2, &map);
}

static int slbc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int ret = 0;
	struct cpuidle_driver *drv = cpuidle_get_driver();
	/* struct resource *res; */
	uint32_t reg[4] = {0, 0, 0, 0};

	ret = slbc_trace_init(pdev);
	if (ret)
		pr_info("SLBC FAILED TO CREATE TRACE MEMORY (%d)\n", ret);

#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
	ret = slbc_scmi_init();
	if (ret < 0)
		return ret;
#endif /* CONFIG_MTK_SLBC_IPI */

	slbc = devm_kzalloc(dev, sizeof(struct mtk_slbc), GFP_KERNEL);
	if (!slbc)
		return -ENOMEM;

	slbc->dev = dev;

#ifdef ENABLE_SLBC
	if (node) {
		ret = of_property_read_u32(node,
				"slbc-enable", &slbc_enable);
		if (ret)
			SLBC_TRACE_REC(LVL_ERR, TYPE_N, 0, ret,
					"failed to get slbc_enable from dts");
		else
			SLBC_TRACE_REC(LVL_QOS, TYPE_N, 0, ret,
					"slbc_enable %d", slbc_enable);

		ret = of_property_read_u32(node,
				"slbc-all-cache-enable", &slbc_all_cache_mode);
		if (ret)
			SLBC_TRACE_REC(LVL_ERR, TYPE_N, 0, ret,
					"failed to get slbc_all_cache_mode from dts");
		else
			SLBC_TRACE_REC(LVL_QOS, TYPE_N, 0, ret,
					"slbc_all_cache_mode %d", slbc_all_cache_mode);
	} else
		SLBC_TRACE_REC(LVL_ERR, TYPE_N, 0, -1,
				"find slbc node failed");
#else
	slbc_enable = 0;

	SLBC_TRACE_REC(LVL_QOS, TYPE_N, 0, 0,
			"slbc_enable %d", slbc_enable);
#endif /* ENABLE_SLBC */

	ret = of_property_read_u32_array(node, "reg", reg, 4);
	if (ret < 0) {
		slbc_sram_enable = 0;

		SLBC_TRACE_REC(LVL_ERR, TYPE_N, 0, ret,
				"slbc of_property_read_u32_array ERR : %d", ret);
	} else {

		slbc->regs = (void *)(long)reg[1];
		slbc->regsize = reg[3];
		slbc->sram_vaddr = (void __iomem *) devm_memremap(dev,
				(resource_size_t)reg[1], slbc->regsize,
				MEMREMAP_WT);
		if (IS_ERR(slbc->sram_vaddr)) {
			slbc_sram_enable = 0;

			dev_notice(dev, "slbc could not ioremap resource for memory\n");
		} else {
			slbc_sram_enable = 1;

			SLBC_TRACE_REC(LVL_QOS, TYPE_N, 0, 0,
					"slbc->regs 0x%lx slbc->regsize 0x%x",
					(unsigned long)slbc->regs, slbc->regsize);
			slbc_sram_init(slbc);
		}
	}

#if IS_ENABLED(CONFIG_PM_SLEEP)
	slbc->ws = wakeup_source_register(NULL, "slbc");
	if (!slbc->ws)
		SLBC_TRACE_REC(LVL_ERR, TYPE_N, 0, 0,
				"slbc wakelock register fail!");
#endif /* CONFIG_PM_SLEEP */

	ret = slbc_create_debug_fs();
	if (ret) {
		SLBC_TRACE_REC(LVL_ERR, TYPE_N, 0, ret,
				"SLBC FAILED TO CREATE DEBUG FILESYSTEM");

		return ret;
	}

	if (drv)
		slbc->slbc_qos_latency = drv->states[2].exit_latency - 10;
	else
		slbc->slbc_qos_latency = 300;
	SLBC_TRACE_REC(LVL_QOS, TYPE_N, 0, 0,
			"slbc_qos_latency %dus", slbc->slbc_qos_latency);
	cpu_latency_qos_add_request(&slbc_qos_request,
			PM_QOS_DEFAULT_VALUE);

	slbc_register_ipi_ops(&ipi_ops);
	slbc_register_common_ops(&common_ops);
	if (slbc_all_cache_mode) {
		ret = mtk_dmaheap_register_slc_callback(slbc_get_gid_for_dma);
		SLBC_TRACE_REC(LVL_NORM, TYPE_C, 0, ret, "mtk_dmaheap_register_slc_callback done");
	}

	if (slbc_enable)
		slbc_sspm_enable(slbc_enable);

	return 0;
}

static int slbc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	slbc_trace_exit();
	slbc_unregister_ipi_ops(&ipi_ops);
	slbc_unregister_common_ops(&common_ops);
	if (slbc_all_cache_mode)
		mtk_dmaheap_unregister_slc_callback();
	devm_kfree(dev, slbc);

	return 0;
}

static int slbc_suspend(struct device *dev)
{
#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
	slbc_suspend_resume_notify(1);
#endif /* CONFIG_MTK_SLBC_IPI */
	return 0;
}

static int slbc_resume(struct device *dev)
{
#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
	slbc_suspend_resume_notify(0);
#endif /* CONFIG_MTK_SLBC_IPI */
	return 0;
}

static int slbc_suspend_cb(struct device *dev)
{
	int i;
	int sid;

	for (i = 0; i < UID_MAX; i++) {
		sid = slbc_get_sid_by_uid(i);
		if (sid != SID_NOT_FOUND && uid_ref[i]) {
			if (sid == UID_HIFI3) {
				SLBC_TRACE_REC(LVL_QOS, TYPE_N, 0, 0,
						"uid_ref %s %x",
						slbc_uid_str[i], uid_ref[i]);
			} else if (sid == UID_AOV |
					sid == UID_AOV_DC |
					sid == UID_AOV_APU) {
				SLBC_TRACE_REC(LVL_QOS, TYPE_N, 0, 0,
						"uid_ref %s %x, screen off user",
						slbc_uid_str[i], uid_ref[i]);
			} else {
				SLBC_TRACE_REC(LVL_QOS, TYPE_N, 0, 0,
						"uid_ref %s %x, screen on user",
						slbc_uid_str[i], uid_ref[i]);
				WARN_ON(uid_ref[i]);
			}
		}
	}

	return 0;
}

static int slbc_resume_cb(struct device *dev)
{
	int i;
	int sid;

	for (i = 0; i < UID_MAX; i++) {
		sid = slbc_get_sid_by_uid(i);
		if (sid != SID_NOT_FOUND && uid_ref[i]) {
			if (sid == UID_HIFI3) {
				SLBC_TRACE_REC(LVL_QOS, TYPE_N, 0, 0,
						"uid_ref %s %x",
						slbc_uid_str[i], uid_ref[i]);
			} else if (sid == UID_AOV |
					sid == UID_AOV_DC |
					sid == UID_AOV_APU) {
				SLBC_TRACE_REC(LVL_QOS, TYPE_N, 0, 0,
						"uid_ref %s %x, screen off user",
						slbc_uid_str[i], uid_ref[i]);
				WARN_ON(uid_ref[i]);
			} else {
				SLBC_TRACE_REC(LVL_QOS, TYPE_N, 0, 0,
						"uid_ref %s %x, screen on user",
						slbc_uid_str[i], uid_ref[i]);
			}
		}
	}

	return 0;
}

static const struct dev_pm_ops slbc_pm_ops = {
	.suspend = slbc_suspend,
	.resume = slbc_resume,
	.suspend_late = slbc_suspend_cb,
	.resume_early = slbc_resume_cb,
};

static const struct of_device_id slbc_of_match[] = {
	{ .compatible = "mediatek,mtk-slbc", },
	{}
};

static struct platform_driver slbc_pdrv = {
	.probe = slbc_probe,
	.remove = slbc_remove,
	.driver = {
		.name = "slbc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(slbc_of_match),
		.pm = &slbc_pm_ops,
	},
};

static int __init slbc_module_init(void)
{
	return platform_driver_register(&slbc_pdrv);
}
module_init(slbc_module_init);

static void __exit slbc_module_exit(void)
{
	platform_driver_unregister(&slbc_pdrv);
}
module_exit(slbc_module_exit);

MODULE_SOFTDEP("pre: tinysys-scmi.ko");
MODULE_DESCRIPTION("SLBC Driver mt6985 v0.1");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL");
