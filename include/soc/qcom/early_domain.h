/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* Early domain services invoked in bootloaders run in parallel after
 * kernel takes over. One page in memory is reserved to pass information
 * between bootloader and kernel. This page has a header to capture status,
 * request and cpumask described in structure early_domain_header. Early
 * domain core driver in kernel reads this header to decide the status of
 * services and takes necessary action. The rest of the page is intended to
 * pass service specific information. Offset for service specific area are
 * defined in macros, and its the service specific driver's responsiblity
 * to operate in their defined areas to pass service specific information.
 * *
 * *
 * *
 * *****************************************
 * *              Header                   *
 * *                                       *
 * *****************************************
 * *                                       *
 * *              Early display            *
 * *                                       *
 * *                                       *
 * *****************************************
 * *                                       *
 * *              Early camera             *
 * *                                       *
 * *                                       *
 * *****************************************
 * *                                       *
 * *              Early audio              *
 * *                                       *
 * *                                       *
 * *****************************************
 * *
 */

enum service_id {
	EARLY_DOMAIN_CORE = 0,
	EARLY_DISPLAY,
	EARLY_CAMERA,
	EARLY_AUDIO,
};

#ifdef CONFIG_QCOM_EARLY_DOMAIN
#include <linux/workqueue.h>
#include <linux/cpumask.h>
#include <linux/pm_qos.h>
#include <linux/notifier.h>

#define EARLY_DOMAIN_MAGIC	"ERLYDOM"
#define MAGIC_SIZE		8
#define NUM_SERVICES		3
#define SERVICE_SHARED_MEM_SIZE		((PAGE_SIZE)/(NUM_SERVICES))

struct early_domain_header {
	char magic[MAGIC_SIZE];
	unsigned long cpumask;
	unsigned long early_domain_status;
	unsigned long early_domain_request;
};

struct early_domain_core {
	struct platform_device *pdev;
	struct early_domain_header *pdata;
	struct work_struct early_domain_work;
	phys_addr_t lk_pool_paddr;
	size_t lk_pool_size;
	phys_addr_t early_domain_shm;
	size_t early_domain_shm_size;
	cpumask_t cpumask;
	struct notifier_block ed_notifier;
	struct pm_qos_request ed_qos_request;
	struct wakeup_source ed_wake_lock;
};

void request_early_service_shutdown(enum service_id);
bool get_early_service_status(enum service_id sid);
void *get_service_shared_mem_start(enum service_id sid);

#else

static inline void request_early_service_shutdown(enum service_id sid) {}
static inline bool get_early_service_status(enum service_id sid)
						{ return false; }
static inline void *get_service_shared_mem_start(enum service_id sid)
						{ return NULL; }

#endif
