/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/diagchar.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <asm/current.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <mach/usbdiag.h>
#endif
#include "diagchar_hdlc.h"
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include "diag_dci.h"

unsigned int dci_max_reg = 100;
unsigned int dci_max_clients = 10;

static void diag_smd_dci_send_req(int proc_num)
{
	void *buf = NULL;
	smd_channel_t *smd_ch = NULL;
	int i, r, found = 1;
	int cmd_code_len = 1;

	if (driver->in_busy_dci)
		return;

	if (proc_num == MODEM_PROC) {
		buf = driver->buf_in_dci;
		smd_ch = driver->ch_dci;
	}

	if (!smd_ch || !buf)
		return;

	r = smd_read_avail(smd_ch);
	if (r > IN_BUF_SIZE) {
		if (r < MAX_IN_BUF_SIZE) {
			pr_err("diag: SMD DCI sending pkt upto %d bytes", r);
			buf = krealloc(buf, r, GFP_KERNEL);
		} else {
			pr_err("diag: DCI pkt > %d bytes", MAX_IN_BUF_SIZE);
			return;
		}
	}
	if (buf && r > 0) {
		smd_read(smd_ch, buf, r);
		pr_debug("diag: data received ---\n");
		for (i = 0; i < r; i++)
			pr_debug("\t %x \t", *(((unsigned char *)buf)+i));

		if (*(uint8_t *)(buf+4) != DCI_CMD_CODE)
			cmd_code_len = 4; /* delayed response */
		driver->write_ptr_dci->length =
			 (int)(*(uint16_t *)(buf+2)) - (4+cmd_code_len);
		pr_debug("diag: len = %d\n", (int)(*(uint16_t *)(buf+2))
							 - (4+cmd_code_len));
		/* look up DCI client with tag */
		for (i = 0; i < dci_max_reg; i++) {
			if (driver->dci_tbl[i].tag ==
			    *(int *)(buf+(4+cmd_code_len))) {
				found = 0;
				break;
			}
		}
		if (found)
			pr_alert("diag: No matching PID for DCI data\n");
		pr_debug("\n diag PID = %d", driver->dci_tbl[i].pid);
		if (driver->dci_tbl[i].pid == 0)
			pr_alert("diag: Receiving DCI process deleted\n");
		*(int *)(buf+4+cmd_code_len) = driver->dci_tbl[i].uid;
		/* update len after adding UID */
		driver->write_ptr_dci->length =
			driver->write_ptr_dci->length + 4;
		pr_debug("diag: data receivd, wake process\n");
		driver->in_busy_dci = 1;
		diag_update_sleeping_process(driver->dci_tbl[i].pid,
							DCI_DATA_TYPE);
		/* delete immediate response entry */
		if (driver->buf_in_dci[8+cmd_code_len] != 0x80)
			driver->dci_tbl[i].pid = 0;
		for (i = 0; i < dci_max_reg; i++)
			if (driver->dci_tbl[i].pid != 0)
				pr_debug("diag: PID = %d, UID = %d, tag = %d\n",
				driver->dci_tbl[i].pid, driver->dci_tbl[i].uid,
				 driver->dci_tbl[i].tag);
		pr_debug("diag: completed clearing table\n");
	}
}

void diag_read_smd_dci_work_fn(struct work_struct *work)
{
	diag_smd_dci_send_req(MODEM_PROC);
}

static void diag_smd_dci_notify(void *ctxt, unsigned event)
{
	queue_work(driver->diag_wq, &(driver->diag_read_smd_dci_work));
}

void diag_dci_notify_client(int peripheral_mask)
{
	int i, stat;

	/* Notify the DCI process that the peripheral DCI Channel is up */
	for (i = 0; i < MAX_DCI_CLIENT; i++) {
		if (driver->dci_notify_tbl[i].list & peripheral_mask) {
			pr_debug("diag: sending signal now\n");
			stat = send_sig(driver->dci_notify_tbl[i].signal_type,
					 driver->dci_notify_tbl[i].client, 0);
			if (stat)
				pr_err("diag: Err send sig stat: %d\n", stat);
			break;
		}
	} /* end of loop for all DCI clients */
}

static int diag_dci_probe(struct platform_device *pdev)
{
	int err = 0;

	if (pdev->id == SMD_APPS_MODEM) {
		err = smd_open("DIAG_2", &driver->ch_dci, driver,
					    diag_smd_dci_notify);
		if (err)
			pr_err("diag: cannot open DCI port, Id = %d, err ="
				" %d\n", pdev->id, err);
		else
			diag_dci_notify_client(DIAG_CON_MPSS);
	}
	return err;
}


int diag_send_dci_pkt(struct diag_master_table entry, unsigned char *buf,
					 int len, int index)
{
	int i;

	/* remove UID from user space pkt before sending to peripheral */
	buf = buf + 4;
	len = len - 4;
	mutex_lock(&driver->dci_mutex);
	/* prepare DCI packet */
	driver->apps_dci_buf[0] = CONTROL_CHAR; /* start */
	driver->apps_dci_buf[1] = 1; /* version */
	*(uint16_t *)(driver->apps_dci_buf + 2) = len + 4 + 1; /* length */
	driver->apps_dci_buf[4] = DCI_CMD_CODE; /* DCI ID */
	*(int *)(driver->apps_dci_buf + 5) = driver->dci_tbl[index].tag;
	for (i = 0; i < len; i++)
		driver->apps_dci_buf[i+9] = *(buf+i);
	driver->apps_dci_buf[9+len] = CONTROL_CHAR; /* end */

	if (entry.client_id == MODEM_PROC && driver->ch_dci) {
		smd_write(driver->ch_dci, driver->apps_dci_buf, len + 10);
		i = DIAG_DCI_NO_ERROR;
	} else {
		pr_alert("diag: check DCI channel\n");
		i = DIAG_DCI_SEND_DATA_FAIL;
	}
	mutex_unlock(&driver->dci_mutex);
	return i;
}

int diag_register_dci_transaction(int uid)
{
	int i, new_dci_client = 1, ret = -1;

	for (i = 0; i < dci_max_reg; i++) {
		if (driver->dci_tbl[i].pid == current->tgid) {
			new_dci_client = 0;
			break;
		}
	}
	mutex_lock(&driver->dci_mutex);
	if (new_dci_client)
		driver->num_dci_client++;
	if (driver->num_dci_client > MAX_DCI_CLIENT) {
		pr_info("diag: Max DCI Client limit reached\n");
		driver->num_dci_client--;
		mutex_unlock(&driver->dci_mutex);
		return ret;
	}
	/* Make an entry in kernel DCI table */
	driver->dci_tag++;
	for (i = 0; i < dci_max_reg; i++) {
		if (driver->dci_tbl[i].pid == 0) {
			driver->dci_tbl[i].pid = current->tgid;
			driver->dci_tbl[i].uid = uid;
			driver->dci_tbl[i].tag = driver->dci_tag;
			ret = i;
			break;
		}
	}
	mutex_unlock(&driver->dci_mutex);
	return ret;
}

int diag_process_dci_client(unsigned char *buf, int len)
{
	unsigned char *temp = buf;
	uint16_t subsys_cmd_code;
	int subsys_id, cmd_code, i, ret = -1, index = -1;
	struct diag_master_table entry;

	/* enter this UID into kernel table and return index */
	index = diag_register_dci_transaction(*(int *)temp);
	if (index < 0) {
		pr_alert("diag: registering new DCI transaction failed\n");
		return DIAG_DCI_NO_REG;
	}
	temp += 4;
	/* Check for registered peripheral and fwd pkt to apropriate proc */
	cmd_code = (int)(*(char *)buf);
	temp++;
	subsys_id = (int)(*(char *)temp);
	temp++;
	subsys_cmd_code = *(uint16_t *)temp;
	temp += 2;
	pr_debug("diag: %d %d %d", cmd_code, subsys_id, subsys_cmd_code);
	for (i = 0; i < diag_max_reg; i++) {
		entry = driver->table[i];
		if (entry.process_id != NO_PROCESS) {
			if (entry.cmd_code == cmd_code && entry.subsys_id ==
				 subsys_id && entry.cmd_code_lo <=
							 subsys_cmd_code &&
				  entry.cmd_code_hi >= subsys_cmd_code) {
				ret = diag_send_dci_pkt(entry, buf, len, index);
			} else if (entry.cmd_code == 255
				  && cmd_code == 75) {
				if (entry.subsys_id ==
					subsys_id &&
				   entry.cmd_code_lo <=
					subsys_cmd_code &&
					 entry.cmd_code_hi >=
					subsys_cmd_code) {
					ret = diag_send_dci_pkt(entry, buf, len,
								 index);
				}
			} else if (entry.cmd_code == 255 &&
				  entry.subsys_id == 255) {
				if (entry.cmd_code_lo <=
						 cmd_code &&
						 entry.
						cmd_code_hi >= cmd_code) {
					ret = diag_send_dci_pkt(entry, buf, len,
								 index);
				}
			}
		}
	}
	return ret;
}

static int diag_dci_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int diag_dci_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops diag_dci_dev_pm_ops = {
	.runtime_suspend = diag_dci_runtime_suspend,
	.runtime_resume = diag_dci_runtime_resume,
};

struct platform_driver msm_diag_dci_driver = {
	.probe = diag_dci_probe,
	.driver = {
		   .name = "DIAG_2",
		   .owner = THIS_MODULE,
		   .pm   = &diag_dci_dev_pm_ops,
		   },
};

int diag_dci_init(void)
{
	int success = 0;

	driver->dci_tag = 0;
	driver->dci_client_id = 0;
	driver->num_dci_client = 0;
	driver->in_busy_dci = 0;
	mutex_init(&driver->dci_mutex);
	if (driver->buf_in_dci == NULL) {
		driver->buf_in_dci = kzalloc(IN_BUF_SIZE, GFP_KERNEL);
		if (driver->buf_in_dci == NULL)
			goto err;
	}
	if (driver->write_ptr_dci == NULL) {
		driver->write_ptr_dci = kzalloc(
			sizeof(struct diag_write_device), GFP_KERNEL);
		if (driver->write_ptr_dci == NULL)
			goto err;
	}
	if (driver->dci_tbl == NULL) {
		driver->dci_tbl = kzalloc(dci_max_reg *
			sizeof(struct diag_dci_tbl), GFP_KERNEL);
		if (driver->dci_tbl == NULL)
			goto err;
	}
	if (driver->dci_notify_tbl == NULL) {
		driver->dci_notify_tbl = kzalloc(MAX_DCI_CLIENT *
			sizeof(struct dci_notification_tbl), GFP_KERNEL);
		if (driver->dci_notify_tbl == NULL)
			goto err;
	}
	if (driver->apps_dci_buf == NULL) {
		driver->apps_dci_buf = kzalloc(APPS_BUF_SIZE, GFP_KERNEL);
		if (driver->apps_dci_buf == NULL)
			goto err;
	}
	success = platform_driver_register(&msm_diag_dci_driver);
	if (success) {
		pr_err("diag: Could not register DCI driver\n");
		goto err;
	}
	return DIAG_DCI_NO_ERROR;
err:
	pr_err("diag: Could not initialize diag DCI buffers");
	kfree(driver->dci_tbl);
	kfree(driver->dci_notify_tbl);
	kfree(driver->apps_dci_buf);
	kfree(driver->buf_in_dci);
	kfree(driver->write_ptr_dci);
	return DIAG_DCI_NO_REG;
}

void diag_dci_exit(void)
{
	smd_close(driver->ch_dci);
	driver->ch_dci = 0;
	platform_driver_unregister(&msm_diag_dci_driver);
	kfree(driver->dci_tbl);
	kfree(driver->dci_notify_tbl);
	kfree(driver->apps_dci_buf);
	kfree(driver->buf_in_dci);
	kfree(driver->write_ptr_dci);
}

