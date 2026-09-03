/******************************************************************************
 * Copyright 2024 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ******************************************************************************/

#if IS_ENABLED(CONFIG_NXP_NFC_VBAT_MONITOR)
#ifndef _NFC_VBAT_MONITOR_H_
#define _NFC_VBAT_MONITOR_H_

#include <linux/cdev.h>

#define NXP_NFC_VBAT_MONITOR
#define NFC_VBAT_MONITOR_MAX_RETRY_COUNT (3)
#define NFC_RESET_RETRY_DELAY_MS (500)
#define NFC_RST_CMD_READ_DELAY_MS (50)
#define NFC_VBAT_MONITOR_WORKQUEUE_NAME	"nfc_vbat_monitor_workq"
#define DTS_NFC_VBAT_MONITOR_STR		"nxp,sn-vbat"

/* vbat monitoring specific parameters*/
struct nfc_vbat_monitor {
	spinlock_t nfc_vbat_monitor_enabled_lock;
	struct workqueue_struct *wq;
	struct work_struct work;
	int irq_num;
	bool vbat_monitor_status;
};

/* vbat monitoring handling and workqueue functions*/
irqreturn_t nfc_vbat_monitor_irq_handler(int irq, void *dev_id);
int nfc_vbat_monitor_init_workqueue(struct nfc_vbat_monitor *nfc_vbat_monitor);

#endif /* _NFC_VBAT_MONITOR_H_ */
#endif /* CONFIG_NXP_NFC_VBAT_MONITOR */
