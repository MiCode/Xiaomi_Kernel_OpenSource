
#ifndef __OEM_MODEM_EDP_H
#define __OEM_MODEM_EDP_H

#include <linux/device.h>
#include <linux/edp.h>

struct oem_modem_edp_data {
	char edp_mgr_name[EDP_NAME_LEN];
	struct edp_client *edp_client;
};

struct oem_modem_edp {
	struct device *pdev;
	char edp_mgr_name[EDP_NAME_LEN];
	int active_estate;
	struct edp_client *edp_client;
	int edp_initialized;
	struct workqueue_struct *edp_wq;
	struct work_struct edp_work;
	struct mutex edp_mutex;
};

#define DEBUG

#ifdef DEBUG
#define EDP_DBG(stuff...)	pr_info("oem_modem_edp: " stuff)
#else
#define EDP_DBG(stuff...)	do {} while (0)
#endif

#endif /* __OEM_MODEM_EDP_H */
