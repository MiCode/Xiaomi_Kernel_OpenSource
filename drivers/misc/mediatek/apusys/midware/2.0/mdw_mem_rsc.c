// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/mutex.h>
#include <linux/errno.h>


#include "mdw_cmn.h"
#include "mdw_mem_rsc.h"

#define MDW_MEM_DOMAIN_MAX (2)

struct mdw_mem_rsc_mgr {
	struct apusys_memory mdw_mem[MDW_MEM_DOMAIN_MAX];
	struct mutex mtx;
};

static struct mdw_mem_rsc_mgr rsc_mem_mgr;

int mdw_mem_rsc_init(void)
{
	memset(&rsc_mem_mgr, 0, sizeof(rsc_mem_mgr));
	mutex_init(&rsc_mem_mgr.mtx);

	pr_info("%s done\n", __func__);

	return 0;

}
void mdw_mem_rsc_deinit(void)
{

}
struct device *mdw_mem_rsc_get_dev(int type)
{
	switch (type) {
	case APUSYS_MEMORY_CODE:
	case APUSYS_MEMORY_DATA:
		break;
	default:
		pr_info("Unsupported Type %d\n", type);
		goto err;
	}

	return rsc_mem_mgr.mdw_mem[type - 1].dev;

err:
	return NULL;
}


int mdw_mem_rsc_register(struct device *dev, int type)
{
	int ret = 0;

	if (!dev)
		return -EINVAL;

	mutex_lock(&rsc_mem_mgr.mtx);
	switch (type) {
	case APUSYS_MEMORY_CODE:
	case APUSYS_MEMORY_DATA:
		rsc_mem_mgr.mdw_mem[type - 1].mem_type = type;
		if (rsc_mem_mgr.mdw_mem[type - 1].dev == NULL)
			rsc_mem_mgr.mdw_mem[type - 1].dev = dev;
		else {
			ret = -EINVAL;
			pr_info("Mulit-Register %d\n", type);
		}
		break;
	default:
		ret = -EINVAL;
		pr_info("Unsupported Type %d\n", type);
		break;
	}
	mutex_unlock(&rsc_mem_mgr.mtx);

	pr_info("Register type[%d] Done\n", type);

	return ret;
}

int mdw_mem_rsc_unregister(int type)
{
	int ret = 0;

	mutex_lock(&rsc_mem_mgr.mtx);
	switch (type) {
	case APUSYS_MEMORY_CODE:
	case APUSYS_MEMORY_DATA:
		rsc_mem_mgr.mdw_mem[type - 1].mem_type = APUSYS_MEMORY_NONE;
		rsc_mem_mgr.mdw_mem[type - 1].dev = NULL;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&rsc_mem_mgr.mtx);

	return ret;
}
