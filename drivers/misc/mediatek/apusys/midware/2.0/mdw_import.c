// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_cmn.h"
#include "mdw_import.h"
#include "mdw_trace.h"
#include "reviser_export.h"
#include "reviser_mem_def.h"
#include "apummu_export.h"
#include "apummu_mem_def.h"
#include "mnoc_api.h"
#include "apusys_power.h"

bool mdw_pwr_check(void)
{
	return apusys_power_check();
}

int mdw_rvs_set_ctx(int type, int idx, uint8_t ctx)
{
	int ret = 0;

	mdw_trace_begin("apumdw:ctx_set|type:%d idx:%d ctx:%u",
		type, idx, ctx);
	ret = reviser_set_context(type, idx, ctx);
	mdw_trace_end();

	return ret;
}

int mdw_rvs_free_vlm(uint32_t ctx)
{
	int ret = 0;

	mdw_trace_begin("apumdw:ctx_free|ctx:%u", ctx);
	ret =  reviser_free_vlm(ctx);
	mdw_trace_end();

	return ret;
}

int mdw_rvs_get_vlm(uint32_t req_size, bool force,
		uint32_t *id, uint32_t *tcm_size)
{
	int ret = 0;

	mdw_trace_begin("apumdw:ctx_alloc|size:%u", req_size);
	ret = reviser_get_vlm(req_size, force, (unsigned long *)id, tcm_size);
	mdw_trace_end();

	return ret;
}

int mdw_rvs_get_vlm_property(uint64_t *start, uint32_t *size)
{
	return reviser_get_resource_vlm((unsigned int *)start,
		(unsigned int *)size);
}

static int mdw_rvs_type_convert(uint32_t type, uint32_t *out)
{
	switch (type) {
	case MDW_MEM_TYPE_VLM:
		*out = APUMMU_MEM_TYPE_VLM;
		break;
	case MDW_MEM_TYPE_LOCAL:
		*out = APUMMU_MEM_TYPE_RSV_T;
		break;
	case MDW_MEM_TYPE_SYSTEM_ISP:
		*out = APUMMU_MEM_TYPE_EXT;
		break;
	case MDW_MEM_TYPE_SYSTEM_APU:
		*out = APUMMU_MEM_TYPE_RSV_S;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

int mdw_rvs_mem_alloc(uint32_t type, uint32_t size,
	uint64_t *addr, uint32_t *sid)
{
	uint32_t mapped_type = 0;
	int ret = 0;

	if (mdw_rvs_type_convert(type, &mapped_type))
		return -EINVAL;

	ret = apummu_alloc_mem(mapped_type, size, addr, sid);
	mdw_flw_debug("type(%u->%u)size(%u)addr(0x%llx)sid(%u)\n",
		type, mapped_type, size, *addr, *sid);

	return ret;
}

int mdw_rvs_mem_free(uint32_t sid)
{
	mdw_flw_debug("sid(%u)\n", sid);
	return apummu_free_mem(sid);
}

int mdw_rvs_mem_import(uint64_t session, uint32_t sid)
{
	mdw_flw_debug("s(0x%llx)sid(%u)\n", (uint64_t)session, sid);
	return apummu_import_mem(session, sid);
}

int mdw_rvs_mem_unimport(uint64_t session, uint32_t sid)
{
	mdw_flw_debug("s(0x%llx)sid(%u)\n", (uint64_t)session, sid);
	return apummu_unimport_mem(session, sid);
}

int mdw_rvs_mem_map(uint64_t session, uint32_t sid, uint64_t *vaddr)
{
	int ret = 0;

	ret = apummu_map_mem(session, sid, vaddr);
	mdw_flw_debug("s(0x%llx)sid(%u)vaddr(0x%llx)\n",
		session, sid, *vaddr);

	return ret;
}

int mdw_rvs_mem_unmap(uint64_t session, uint32_t sid)
{
	mdw_flw_debug("s(0x%llx)sid(%u)\n", (uint64_t)session, sid);
	return apummu_unmap_mem(session, sid);
}

int mdw_ammu_eva2iova(uint64_t eva, uint64_t *iova)
{
	return apummu_eva2iova(eva, iova);
}

int mdw_qos_cmd_start(uint64_t cmd_id, uint64_t sc_id,
		int type, int core, uint32_t boost)
{
	return apu_cmd_qos_start(cmd_id, sc_id, type, core, boost);
}

int mdw_qos_cmd_end(uint64_t cmd_id, uint64_t sc_id,
		int type, int core)
{
	return apu_cmd_qos_end(cmd_id, sc_id, type, core);
}
