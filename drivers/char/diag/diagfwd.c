/* Copyright (c) 2008-2013, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/diagchar.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/kmemleak.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <mach/usbdiag.h>
#endif
#include <mach/msm_smd.h>
#include <mach/socinfo.h>
#include <mach/restart.h>
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_cntl.h"
#include "diagfwd_hsic.h"
#include "diagchar_hdlc.h"
#ifdef CONFIG_DIAG_SDIO_PIPE
#include "diagfwd_sdio.h"
#endif
#include "diag_dci.h"
#include "diag_masks.h"
#include "diagfwd_bridge.h"

#define MODE_CMD		41
#define RESET_ID		2

int diag_debug_buf_idx;
unsigned char diag_debug_buf[1024];
static unsigned int buf_tbl_size = 8; /*Number of entries in table of buffers */
struct diag_master_table entry;
struct diag_send_desc_type send = { NULL, NULL, DIAG_STATE_START, 0 };
struct diag_hdlc_dest_type enc = { NULL, NULL, 0 };
int wrap_enabled;
uint16_t wrap_count;

void encode_rsp_and_send(int buf_length)
{
	struct diag_smd_info *data = &(driver->smd_data[MODEM_DATA]);
	send.state = DIAG_STATE_START;
	send.pkt = driver->apps_rsp_buf;
	send.last = (void *)(driver->apps_rsp_buf + buf_length);
	send.terminate = 1;
	if (!data->in_busy_1) {
		enc.dest = data->buf_in_1;
		enc.dest_last = (void *)(data->buf_in_1 + APPS_BUF_SIZE - 1);
		diag_hdlc_encode(&send, &enc);
		data->write_ptr_1->buf = data->buf_in_1;
		data->write_ptr_1->length = (int)(enc.dest -
						(void *)(data->buf_in_1));
		data->in_busy_1 = 1;
		diag_device_write(data->buf_in_1, data->peripheral,
					data->write_ptr_1);
		memset(driver->apps_rsp_buf, '\0', APPS_BUF_SIZE);
	}
}

/* Determine if this device uses a device tree */
#ifdef CONFIG_OF
static int has_device_tree(void)
{
	struct device_node *node;

	node = of_find_node_by_path("/");
	if (node) {
		of_node_put(node);
		return 1;
	}
	return 0;
}
#else
static int has_device_tree(void)
{
	return 0;
}
#endif

int chk_config_get_id(void)
{
	/* For all Fusion targets,  Modem will always be present */
	if (machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa())
		return 0;

	if (driver->use_device_tree) {
		if (machine_is_msm8974())
			return MSM8974_TOOLS_ID;
		else
			return 0;
	} else {
		switch (socinfo_get_msm_cpu()) {
		case MSM_CPU_8X60:
			return APQ8060_TOOLS_ID;
		case MSM_CPU_8960:
		case MSM_CPU_8960AB:
			return AO8960_TOOLS_ID;
		case MSM_CPU_8064:
		case MSM_CPU_8064AB:
		case MSM_CPU_8064AA:
			return APQ8064_TOOLS_ID;
		case MSM_CPU_8930:
		case MSM_CPU_8930AA:
		case MSM_CPU_8930AB:
			return MSM8930_TOOLS_ID;
		case MSM_CPU_8974:
			return MSM8974_TOOLS_ID;
		case MSM_CPU_8625:
			return MSM8625_TOOLS_ID;
		default:
			return 0;
		}
	}
}

/*
 * This will return TRUE for targets which support apps only mode and hence SSR.
 * This applies to 8960 and newer targets.
 */
int chk_apps_only(void)
{
	if (driver->use_device_tree)
		return 1;

	switch (socinfo_get_msm_cpu()) {
	case MSM_CPU_8960:
	case MSM_CPU_8960AB:
	case MSM_CPU_8064:
	case MSM_CPU_8064AB:
	case MSM_CPU_8064AA:
	case MSM_CPU_8930:
	case MSM_CPU_8930AA:
	case MSM_CPU_8930AB:
	case MSM_CPU_8627:
	case MSM_CPU_9615:
	case MSM_CPU_8974:
		return 1;
	default:
		return 0;
	}
}

/*
 * This will return TRUE for targets which support apps as master.
 * Thus, SW DLOAD and Mode Reset are supported on apps processor.
 * This applies to 8960 and newer targets.
 */
int chk_apps_master(void)
{
	if (driver->use_device_tree)
		return 1;
	else if (soc_class_is_msm8960() || soc_class_is_msm8930() ||
		 soc_class_is_apq8064() || cpu_is_msm9615())
		return 1;
	else
		return 0;
}

int chk_polling_response(void)
{
	if (!(driver->polling_reg_flag) && chk_apps_master())
		/*
		 * If the apps processor is master and no other processor
		 * has registered to respond for polling
		 */
		return 1;
	else if (!(driver->smd_data[MODEM_DATA].ch) &&
					!(chk_apps_master()))
		/*
		 * If the apps processor is not the master and the modem
		 * is not up
		 */
		return 1;
	else
		return 0;
}

/*
 * This function should be called if you feel that the logging process may
 * need to be woken up. For instance, if the logging mode is MEMORY_DEVICE MODE
 * and while trying to read data from a SMD data channel there are no buffers
 * available to read the data into, then this function should be called to
 * determine if the logging process needs to be woken up.
 */
void chk_logging_wakeup(void)
{
	int i;

	/* Find the index of the logging process */
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i].pid ==
			driver->logging_process_id)
			break;

	if (i < driver->num_clients) {
		/* At very high logging rates a race condition can
		 * occur where the buffers containing the data from
		 * an smd channel are all in use, but the data_ready
		 * flag is cleared. In this case, the buffers never
		 * have their data read/logged.  Detect and remedy this
		 * situation.
		 */
		if ((driver->data_ready[i] & USER_SPACE_DATA_TYPE) == 0) {
			driver->data_ready[i] |= USER_SPACE_DATA_TYPE;
			pr_debug("diag: Force wakeup of logging process\n");
			wake_up_interruptible(&driver->wait_q);
		}
	}
}

void process_lock_enabling(struct diag_nrt_wake_lock *lock, int real_time)
{
	unsigned long read_lock_flags;

	spin_lock_irqsave(&lock->read_spinlock, read_lock_flags);
	if (real_time)
		lock->enabled = 0;
	else
		lock->enabled = 1;
	lock->ref_count = 0;
	lock->copy_count = 0;
	wake_unlock(&lock->read_lock);
	spin_unlock_irqrestore(&lock->read_spinlock, read_lock_flags);
}

void process_lock_on_notify(struct diag_nrt_wake_lock *lock)
{
	unsigned long read_lock_flags;

	spin_lock_irqsave(&lock->read_spinlock, read_lock_flags);
	/*
	 * Do not work with ref_count here in case
	 * of spurious interrupt
	 */
	if (lock->enabled)
		wake_lock(&lock->read_lock);
	spin_unlock_irqrestore(&lock->read_spinlock, read_lock_flags);
}

void process_lock_on_read(struct diag_nrt_wake_lock *lock, int pkt_len)
{
	unsigned long read_lock_flags;

	spin_lock_irqsave(&lock->read_spinlock, read_lock_flags);
	if (lock->enabled) {
		if (pkt_len > 0) {
			/*
			 * We have an data that is read that
			 * needs to be processed, make sure the
			 * processor does not go to sleep
			 */
			lock->ref_count++;
			if (!wake_lock_active(&lock->read_lock))
				wake_lock(&lock->read_lock);
		} else {
			/*
			 * There was no data associated with the
			 * read from the smd, unlock the wake lock
			 * if it is not needed.
			 */
			if (lock->ref_count < 1) {
				if (wake_lock_active(&lock->read_lock))
					wake_unlock(&lock->read_lock);
				lock->ref_count = 0;
				lock->copy_count = 0;
			}
		}
	}
	spin_unlock_irqrestore(&lock->read_spinlock, read_lock_flags);
}

void process_lock_on_copy(struct diag_nrt_wake_lock *lock)
{
	unsigned long read_lock_flags;

	spin_lock_irqsave(&lock->read_spinlock, read_lock_flags);
	if (lock->enabled)
		lock->copy_count++;
	spin_unlock_irqrestore(&lock->read_spinlock, read_lock_flags);
}

void process_lock_on_copy_complete(struct diag_nrt_wake_lock *lock)
{
	unsigned long read_lock_flags;

	spin_lock_irqsave(&lock->read_spinlock, read_lock_flags);
	if (lock->enabled) {
		lock->ref_count -= lock->copy_count;
		if (lock->ref_count < 1) {
			wake_unlock(&lock->read_lock);
			lock->ref_count = 0;
		}
		lock->copy_count = 0;
	}
	spin_unlock_irqrestore(&lock->read_spinlock, read_lock_flags);
}

/* Process the data read from the smd data channel */
int diag_process_smd_read_data(struct diag_smd_info *smd_info, void *buf,
								int total_recd)
{
	struct diag_request *write_ptr_modem = NULL;
	int *in_busy_ptr = 0;

	if (smd_info->buf_in_1 == buf) {
		write_ptr_modem = smd_info->write_ptr_1;
		in_busy_ptr = &smd_info->in_busy_1;
	} else if (smd_info->buf_in_2 == buf) {
		write_ptr_modem = smd_info->write_ptr_2;
		in_busy_ptr = &smd_info->in_busy_2;
	} else {
		pr_err("diag: In %s, no match for in_busy_1\n", __func__);
	}

	if (write_ptr_modem) {
		write_ptr_modem->length = total_recd;
		*in_busy_ptr = 1;
		diag_device_write(buf, smd_info->peripheral, write_ptr_modem);
	}

	return 0;
}

void diag_smd_send_req(struct diag_smd_info *smd_info)
{
	void *buf = NULL, *temp_buf = NULL;
	int total_recd = 0, r = 0, pkt_len;
	int loop_count = 0;
	int notify = 0;

	if (!smd_info) {
		pr_err("diag: In %s, no smd info. Not able to read.\n",
			__func__);
		return;
	}

	if (!smd_info->in_busy_1)
		buf = smd_info->buf_in_1;
	else if ((smd_info->type == SMD_DATA_TYPE) && !smd_info->in_busy_2)
		buf = smd_info->buf_in_2;

	if (smd_info->ch && buf) {
		temp_buf = buf;
		pkt_len = smd_cur_packet_size(smd_info->ch);

		while (pkt_len && (pkt_len != total_recd)) {
			loop_count++;
			r = smd_read_avail(smd_info->ch);
			pr_debug("diag: In %s, received pkt %d %d\n",
				__func__, r, total_recd);
			if (!r) {
				/* Nothing to read from SMD */
				wait_event(driver->smd_wait_q,
					((smd_info->ch == 0) ||
					smd_read_avail(smd_info->ch)));
				/* If the smd channel is open */
				if (smd_info->ch) {
					pr_debug("diag: In %s, return from wait_event\n",
						__func__);
					continue;
				} else {
					pr_debug("diag: In %s, return from wait_event ch closed\n",
						__func__);
					return;
				}
			}
			total_recd += r;
			if (total_recd > IN_BUF_SIZE) {
				if (total_recd < MAX_IN_BUF_SIZE) {
					pr_err("diag: In %s, SMD sending in packets up to %d bytes\n",
						__func__, total_recd);
					buf = krealloc(buf, total_recd,
						GFP_KERNEL);
				} else {
					pr_err("diag: In %s, SMD sending in packets more than %d bytes\n",
						__func__, MAX_IN_BUF_SIZE);
					return;
				}
			}
			if (pkt_len < r) {
				pr_err("diag: In %s, SMD sending incorrect pkt\n",
					__func__);
				return;
			}
			if (pkt_len > r) {
				pr_err("diag: In %s, SMD sending partial pkt %d %d %d %d %d %d\n",
				__func__, pkt_len, r, total_recd, loop_count,
				smd_info->peripheral, smd_info->type);
			}

			/* keep reading for complete packet */
			smd_read(smd_info->ch, temp_buf, r);
			temp_buf += r;
		}
		if (!driver->real_time_mode && smd_info->type == SMD_DATA_TYPE)
			process_lock_on_read(&smd_info->nrt_lock, pkt_len);

		if (total_recd > 0) {
			if (!buf) {
				pr_err("diag: Out of diagmem for Modem\n");
			} else if (smd_info->process_smd_read_data) {
				notify = smd_info->process_smd_read_data(
						smd_info, buf, total_recd);
				/* Poll SMD channels to check for data */
				if (notify)
					diag_smd_notify(smd_info,
							SMD_EVENT_DATA);
			}
		}
	} else if (smd_info->ch && !buf &&
		(driver->logging_mode == MEMORY_DEVICE_MODE)) {
			chk_logging_wakeup();
	}
}

void diag_read_smd_work_fn(struct work_struct *work)
{
	struct diag_smd_info *smd_info = container_of(work,
							struct diag_smd_info,
							diag_read_smd_work);
	diag_smd_send_req(smd_info);
}

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
static void diag_mem_dev_mode_ready_update(int index, int hsic_updated)
{
	if (hsic_updated) {
		unsigned long flags;
		spin_lock_irqsave(&driver->hsic_ready_spinlock, flags);
		driver->data_ready[index] |= USER_SPACE_DATA_TYPE;
		spin_unlock_irqrestore(&driver->hsic_ready_spinlock, flags);
	} else {
		driver->data_ready[index] |= USER_SPACE_DATA_TYPE;
	}
}
#else
static void diag_mem_dev_mode_ready_update(int index, int hsic_updated)
{
	(void) hsic_updated;
	driver->data_ready[index] |= USER_SPACE_DATA_TYPE;
}
#endif
int diag_device_write(void *buf, int data_type, struct diag_request *write_ptr)
{
	int i, err = 0, index;
	index = 0;

	if (driver->logging_mode == MEMORY_DEVICE_MODE) {
		int hsic_updated = 0;
		if (data_type == APPS_DATA) {
			for (i = 0; i < driver->poolsize_write_struct; i++)
				if (driver->buf_tbl[i].length == 0) {
					driver->buf_tbl[i].buf = buf;
					driver->buf_tbl[i].length =
								 driver->used;
#ifdef DIAG_DEBUG
					pr_debug("diag: ENQUEUE buf ptr"
						   " and length is %x , %d\n",
						   (unsigned int)(driver->buf_
				tbl[i].buf), driver->buf_tbl[i].length);
#endif
					break;
				}
		}

#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
		else if (data_type == HSIC_DATA || data_type == HSIC_2_DATA) {
			unsigned long flags;
			int foundIndex = -1;
			hsic_updated = 1;
			index = data_type - HSIC_DATA;
			spin_lock_irqsave(&diag_hsic[index].hsic_spinlock,
									flags);
			for (i = 0; i < diag_hsic[index].poolsize_hsic_write;
									i++) {
				if (diag_hsic[index].hsic_buf_tbl[i].length
									== 0) {
					diag_hsic[index].hsic_buf_tbl[i].buf
									= buf;
					diag_hsic[index].hsic_buf_tbl[i].length
						= diag_bridge[index].write_len;
					diag_hsic[index].
						num_hsic_buf_tbl_entries++;
					foundIndex = i;
					break;
				}
			}
			spin_unlock_irqrestore(&diag_hsic[index].hsic_spinlock,
									flags);
			if (foundIndex == -1)
				err = -1;
			else
				pr_debug("diag: ENQUEUE HSIC buf ptr and length is %x , %d, ch %d\n",
					(unsigned int)buf,
					 diag_bridge[index].write_len, index);
		}
#endif
		for (i = 0; i < driver->num_clients; i++)
			if (driver->client_map[i].pid ==
						 driver->logging_process_id)
				break;
		if (i < driver->num_clients) {
			diag_mem_dev_mode_ready_update(i, hsic_updated);
			pr_debug("diag: wake up logging process\n");
			wake_up_interruptible(&driver->wait_q);
		} else
			return -EINVAL;
	} else if (driver->logging_mode == NO_LOGGING_MODE) {
		if ((data_type >= 0) && (data_type < NUM_SMD_DATA_CHANNELS)) {
			driver->smd_data[data_type].in_busy_1 = 0;
			driver->smd_data[data_type].in_busy_2 = 0;
			queue_work(driver->diag_wq,
				&(driver->smd_data[data_type].
							diag_read_smd_work));
		}
#ifdef CONFIG_DIAG_SDIO_PIPE
		else if (data_type == SDIO_DATA) {
			driver->in_busy_sdio = 0;
			queue_work(driver->diag_sdio_wq,
				&(driver->diag_read_sdio_work));
		}
#endif
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
		else if (data_type == HSIC_DATA || data_type == HSIC_2_DATA) {
			index = data_type - HSIC_DATA;
			if (diag_hsic[index].hsic_ch)
				queue_work(diag_bridge[index].wq,
					   &(diag_hsic[index].
					     diag_read_hsic_work));
		}
#endif
		err = -1;
	}
#ifdef CONFIG_DIAG_OVER_USB
	else if (driver->logging_mode == USB_MODE) {
		if (data_type == APPS_DATA) {
			driver->write_ptr_svc = (struct diag_request *)
			(diagmem_alloc(driver, sizeof(struct diag_request),
				 POOL_TYPE_WRITE_STRUCT));
			if (driver->write_ptr_svc) {
				driver->write_ptr_svc->length = driver->used;
				driver->write_ptr_svc->buf = buf;
				err = usb_diag_write(driver->legacy_ch,
						driver->write_ptr_svc);
			} else
				err = -1;
		} else if ((data_type >= 0) &&
				(data_type < NUM_SMD_DATA_CHANNELS)) {
			write_ptr->buf = buf;
#ifdef DIAG_DEBUG
			printk(KERN_INFO "writing data to USB,"
				"pkt length %d\n", write_ptr->length);
			print_hex_dump(KERN_DEBUG, "Written Packet Data to"
					   " USB: ", 16, 1, DUMP_PREFIX_ADDRESS,
					    buf, write_ptr->length, 1);
#endif /* DIAG DEBUG */
			err = usb_diag_write(driver->legacy_ch, write_ptr);
		}
#ifdef CONFIG_DIAG_SDIO_PIPE
		else if (data_type == SDIO_DATA) {
			if (machine_is_msm8x60_fusion() ||
					 machine_is_msm8x60_fusn_ffa()) {
				write_ptr->buf = buf;
				err = usb_diag_write(driver->mdm_ch, write_ptr);
			} else
				pr_err("diag: Incorrect sdio data "
						"while USB write\n");
		}
#endif
#ifdef CONFIG_DIAGFWD_BRIDGE_CODE
		else if (data_type == HSIC_DATA || data_type == HSIC_2_DATA) {
			index = data_type - HSIC_DATA;
			if (diag_hsic[index].hsic_device_enabled) {
				struct diag_request *write_ptr_mdm;
				write_ptr_mdm = (struct diag_request *)
						diagmem_alloc(driver,
						sizeof(struct diag_request),
							index +
							POOL_TYPE_HSIC_WRITE);
				if (write_ptr_mdm) {
					write_ptr_mdm->buf = buf;
					write_ptr_mdm->length =
					   diag_bridge[index].write_len;
					write_ptr_mdm->context = (void *)index;
					err = usb_diag_write(
					diag_bridge[index].ch, write_ptr_mdm);
					/* Return to the pool immediately */
					if (err) {
						diagmem_free(driver,
							write_ptr_mdm,
							index +
							POOL_TYPE_HSIC_WRITE);
						pr_err_ratelimited("diag: HSIC write failure, err: %d, ch %d\n",
							err, index);
					}
				} else {
					pr_err("diag: allocate write fail\n");
					err = -1;
				}
			} else {
				pr_err("diag: Incorrect HSIC data "
						"while USB write\n");
				err = -1;
			}
		} else if (data_type == SMUX_DATA) {
				write_ptr->buf = buf;
				write_ptr->context = (void *)SMUX;
				pr_debug("diag: writing SMUX data\n");
				err = usb_diag_write(diag_bridge[SMUX].ch,
								 write_ptr);
		}
#endif
		APPEND_DEBUG('d');
	}
#endif /* DIAG OVER USB */
    return err;
}

static void diag_update_pkt_buffer(unsigned char *buf)
{
	unsigned char *ptr = driver->pkt_buf;
	unsigned char *temp = buf;

	mutex_lock(&driver->diagchar_mutex);
	if (CHK_OVERFLOW(ptr, ptr, ptr + PKT_SIZE, driver->pkt_length)) {
		memcpy(ptr, temp , driver->pkt_length);
		driver->in_busy_pktdata = 1;
	} else
		printk(KERN_CRIT " Not enough buffer space for PKT_RESP\n");
	mutex_unlock(&driver->diagchar_mutex);
}

void diag_update_userspace_clients(unsigned int type)
{
	int i;

	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i].pid != 0)
			driver->data_ready[i] |= type;
	wake_up_interruptible(&driver->wait_q);
	mutex_unlock(&driver->diagchar_mutex);
}

void diag_update_sleeping_process(int process_id, int data_type)
{
	int i;

	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i].pid == process_id) {
			driver->data_ready[i] |= data_type;
			break;
		}
	wake_up_interruptible(&driver->wait_q);
	mutex_unlock(&driver->diagchar_mutex);
}

static int diag_check_mode_reset(unsigned char *buf)
{
	int is_mode_reset = 0;
	if (chk_apps_master() && (int)(*(char *)buf) == MODE_CMD)
		if ((int)(*(char *)(buf+1)) == RESET_ID)
			is_mode_reset = 1;
	return is_mode_reset;
}

void diag_send_data(struct diag_master_table entry, unsigned char *buf,
					 int len, int type)
{
	driver->pkt_length = len;
	if (entry.process_id != NON_APPS_PROC && type != MODEM_DATA) {
		diag_update_pkt_buffer(buf);
		diag_update_sleeping_process(entry.process_id, PKT_TYPE);
	} else {
		if (len > 0) {
			if ((entry.client_id >= 0) &&
				(entry.client_id < NUM_SMD_DATA_CHANNELS)) {
				int index = entry.client_id;
				if (driver->smd_data[index].ch) {
					if ((index == MODEM_DATA) &&
						diag_check_mode_reset(buf)) {
						return;
					}
					mutex_lock(&driver->smd_data[index].
								smd_ch_mutex);
					smd_write(driver->smd_data[index].ch,
							buf, len);
					mutex_unlock(&driver->smd_data[index].
								smd_ch_mutex);
				} else {
					pr_err("diag: In %s, smd channel %d not open\n",
						__func__, index);
				}
			} else {
				pr_alert("diag: In %s, incorrect channel: %d",
					__func__, entry.client_id);
			}
		}
	}
}

int diag_process_apps_pkt(unsigned char *buf, int len)
{
	uint16_t subsys_cmd_code;
	int subsys_id, ssid_first, ssid_last, ssid_range;
	int packet_type = 1, i, cmd_code;
	unsigned char *temp = buf;
	int data_type;
	int mask_ret;
#if defined(CONFIG_DIAG_OVER_USB)
	unsigned char *ptr;
#endif

	/* Check if the command is a supported mask command */
	mask_ret = diag_process_apps_masks(buf, len);
	if (mask_ret <= 0)
		return mask_ret;

	/* Check for registered clients and forward packet to apropriate proc */
	cmd_code = (int)(*(char *)buf);
	temp++;
	subsys_id = (int)(*(char *)temp);
	temp++;
	subsys_cmd_code = *(uint16_t *)temp;
	temp += 2;
	data_type = APPS_DATA;
	/* Dont send any command other than mode reset */
	if (chk_apps_master() && cmd_code == MODE_CMD) {
		if (subsys_id != RESET_ID)
			data_type = MODEM_DATA;
	}

	pr_debug("diag: %d %d %d", cmd_code, subsys_id, subsys_cmd_code);
	for (i = 0; i < diag_max_reg; i++) {
		entry = driver->table[i];
		if (entry.process_id != NO_PROCESS) {
			if (entry.cmd_code == cmd_code && entry.subsys_id ==
				 subsys_id && entry.cmd_code_lo <=
							 subsys_cmd_code &&
				  entry.cmd_code_hi >= subsys_cmd_code) {
				diag_send_data(entry, buf, len, data_type);
				packet_type = 0;
			} else if (entry.cmd_code == 255
				  && cmd_code == 75) {
				if (entry.subsys_id ==
					subsys_id &&
				   entry.cmd_code_lo <=
					subsys_cmd_code &&
					 entry.cmd_code_hi >=
					subsys_cmd_code) {
					diag_send_data(entry, buf, len,
								 data_type);
					packet_type = 0;
				}
			} else if (entry.cmd_code == 255 &&
				  entry.subsys_id == 255) {
				if (entry.cmd_code_lo <=
						 cmd_code &&
						 entry.
						cmd_code_hi >= cmd_code) {
					diag_send_data(entry, buf, len,
								 data_type);
					packet_type = 0;
				}
			}
		}
	}
#if defined(CONFIG_DIAG_OVER_USB)
	/* Check for the command/respond msg for the maximum packet length */
	if ((*buf == 0x4b) && (*(buf+1) == 0x12) &&
		(*(uint16_t *)(buf+2) == 0x0055)) {
		for (i = 0; i < 4; i++)
			*(driver->apps_rsp_buf+i) = *(buf+i);
		*(uint32_t *)(driver->apps_rsp_buf+4) = PKT_SIZE;
		encode_rsp_and_send(7);
		return 0;
	}
	/* Check for Apps Only & get event mask request */
	else if (!(driver->smd_data[MODEM_DATA].ch) && chk_apps_only() &&
								*buf == 0x81) {
		driver->apps_rsp_buf[0] = 0x81;
		driver->apps_rsp_buf[1] = 0x0;
		*(uint16_t *)(driver->apps_rsp_buf + 2) = 0x0;
		*(uint16_t *)(driver->apps_rsp_buf + 4) = EVENT_LAST_ID + 1;
		for (i = 0; i < EVENT_LAST_ID/8 + 1; i++)
			*(unsigned char *)(driver->apps_rsp_buf + 6 + i) = 0x0;
		encode_rsp_and_send(6 + EVENT_LAST_ID/8);
		return 0;
	}
	/* Get log ID range & Check for Apps Only */
	else if (!(driver->smd_data[MODEM_DATA].ch) && chk_apps_only()
			  && (*buf == 0x73) && *(int *)(buf+4) == 1) {
		driver->apps_rsp_buf[0] = 0x73;
		*(int *)(driver->apps_rsp_buf + 4) = 0x1; /* operation ID */
		*(int *)(driver->apps_rsp_buf + 8) = 0x0; /* success code */
		*(int *)(driver->apps_rsp_buf + 12) = LOG_GET_ITEM_NUM(LOG_0);
		*(int *)(driver->apps_rsp_buf + 16) = LOG_GET_ITEM_NUM(LOG_1);
		*(int *)(driver->apps_rsp_buf + 20) = LOG_GET_ITEM_NUM(LOG_2);
		*(int *)(driver->apps_rsp_buf + 24) = LOG_GET_ITEM_NUM(LOG_3);
		*(int *)(driver->apps_rsp_buf + 28) = LOG_GET_ITEM_NUM(LOG_4);
		*(int *)(driver->apps_rsp_buf + 32) = LOG_GET_ITEM_NUM(LOG_5);
		*(int *)(driver->apps_rsp_buf + 36) = LOG_GET_ITEM_NUM(LOG_6);
		*(int *)(driver->apps_rsp_buf + 40) = LOG_GET_ITEM_NUM(LOG_7);
		*(int *)(driver->apps_rsp_buf + 44) = LOG_GET_ITEM_NUM(LOG_8);
		*(int *)(driver->apps_rsp_buf + 48) = LOG_GET_ITEM_NUM(LOG_9);
		*(int *)(driver->apps_rsp_buf + 52) = LOG_GET_ITEM_NUM(LOG_10);
		*(int *)(driver->apps_rsp_buf + 56) = LOG_GET_ITEM_NUM(LOG_11);
		*(int *)(driver->apps_rsp_buf + 60) = LOG_GET_ITEM_NUM(LOG_12);
		*(int *)(driver->apps_rsp_buf + 64) = LOG_GET_ITEM_NUM(LOG_13);
		*(int *)(driver->apps_rsp_buf + 68) = LOG_GET_ITEM_NUM(LOG_14);
		*(int *)(driver->apps_rsp_buf + 72) = LOG_GET_ITEM_NUM(LOG_15);
		encode_rsp_and_send(75);
		return 0;
	}
	/* Respond to Get SSID Range request message */
	else if (!(driver->smd_data[MODEM_DATA].ch) && chk_apps_only()
			 && (*buf == 0x7d) && (*(buf+1) == 0x1)) {
		driver->apps_rsp_buf[0] = 0x7d;
		driver->apps_rsp_buf[1] = 0x1;
		driver->apps_rsp_buf[2] = 0x1;
		driver->apps_rsp_buf[3] = 0x0;
		/* -1 to un-account for OEM SSID range */
		*(int *)(driver->apps_rsp_buf + 4) = MSG_MASK_TBL_CNT - 1;
		*(uint16_t *)(driver->apps_rsp_buf + 8) = MSG_SSID_0;
		*(uint16_t *)(driver->apps_rsp_buf + 10) = MSG_SSID_0_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 12) = MSG_SSID_1;
		*(uint16_t *)(driver->apps_rsp_buf + 14) = MSG_SSID_1_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 16) = MSG_SSID_2;
		*(uint16_t *)(driver->apps_rsp_buf + 18) = MSG_SSID_2_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 20) = MSG_SSID_3;
		*(uint16_t *)(driver->apps_rsp_buf + 22) = MSG_SSID_3_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 24) = MSG_SSID_4;
		*(uint16_t *)(driver->apps_rsp_buf + 26) = MSG_SSID_4_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 28) = MSG_SSID_5;
		*(uint16_t *)(driver->apps_rsp_buf + 30) = MSG_SSID_5_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 32) = MSG_SSID_6;
		*(uint16_t *)(driver->apps_rsp_buf + 34) = MSG_SSID_6_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 36) = MSG_SSID_7;
		*(uint16_t *)(driver->apps_rsp_buf + 38) = MSG_SSID_7_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 40) = MSG_SSID_8;
		*(uint16_t *)(driver->apps_rsp_buf + 42) = MSG_SSID_8_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 44) = MSG_SSID_9;
		*(uint16_t *)(driver->apps_rsp_buf + 46) = MSG_SSID_9_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 48) = MSG_SSID_10;
		*(uint16_t *)(driver->apps_rsp_buf + 50) = MSG_SSID_10_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 52) = MSG_SSID_11;
		*(uint16_t *)(driver->apps_rsp_buf + 54) = MSG_SSID_11_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 56) = MSG_SSID_12;
		*(uint16_t *)(driver->apps_rsp_buf + 58) = MSG_SSID_12_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 60) = MSG_SSID_13;
		*(uint16_t *)(driver->apps_rsp_buf + 62) = MSG_SSID_13_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 64) = MSG_SSID_14;
		*(uint16_t *)(driver->apps_rsp_buf + 66) = MSG_SSID_14_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 68) = MSG_SSID_15;
		*(uint16_t *)(driver->apps_rsp_buf + 70) = MSG_SSID_15_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 72) = MSG_SSID_16;
		*(uint16_t *)(driver->apps_rsp_buf + 74) = MSG_SSID_16_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 76) = MSG_SSID_17;
		*(uint16_t *)(driver->apps_rsp_buf + 78) = MSG_SSID_17_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 80) = MSG_SSID_18;
		*(uint16_t *)(driver->apps_rsp_buf + 82) = MSG_SSID_18_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 84) = MSG_SSID_19;
		*(uint16_t *)(driver->apps_rsp_buf + 86) = MSG_SSID_19_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 88) = MSG_SSID_20;
		*(uint16_t *)(driver->apps_rsp_buf + 90) = MSG_SSID_20_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 92) = MSG_SSID_21;
		*(uint16_t *)(driver->apps_rsp_buf + 94) = MSG_SSID_21_LAST;
		*(uint16_t *)(driver->apps_rsp_buf + 96) = MSG_SSID_22;
		*(uint16_t *)(driver->apps_rsp_buf + 98) = MSG_SSID_22_LAST;
		encode_rsp_and_send(99);
		return 0;
	}
	/* Check for Apps Only Respond to Get Subsys Build mask */
	else if (!(driver->smd_data[MODEM_DATA].ch) && chk_apps_only()
			 && (*buf == 0x7d) && (*(buf+1) == 0x2)) {
		ssid_first = *(uint16_t *)(buf + 2);
		ssid_last = *(uint16_t *)(buf + 4);
		ssid_range = 4 * (ssid_last - ssid_first + 1);
		/* frame response */
		driver->apps_rsp_buf[0] = 0x7d;
		driver->apps_rsp_buf[1] = 0x2;
		*(uint16_t *)(driver->apps_rsp_buf + 2) = ssid_first;
		*(uint16_t *)(driver->apps_rsp_buf + 4) = ssid_last;
		driver->apps_rsp_buf[6] = 0x1;
		driver->apps_rsp_buf[7] = 0x0;
		ptr = driver->apps_rsp_buf + 8;
		/* bld time masks */
		switch (ssid_first) {
		case MSG_SSID_0:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_0[i/4];
			break;
		case MSG_SSID_1:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_1[i/4];
			break;
		case MSG_SSID_2:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_2[i/4];
			break;
		case MSG_SSID_3:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_3[i/4];
			break;
		case MSG_SSID_4:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_4[i/4];
			break;
		case MSG_SSID_5:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_5[i/4];
			break;
		case MSG_SSID_6:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_6[i/4];
			break;
		case MSG_SSID_7:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_7[i/4];
			break;
		case MSG_SSID_8:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_8[i/4];
			break;
		case MSG_SSID_9:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_9[i/4];
			break;
		case MSG_SSID_10:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_10[i/4];
			break;
		case MSG_SSID_11:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_11[i/4];
			break;
		case MSG_SSID_12:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_12[i/4];
			break;
		case MSG_SSID_13:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_13[i/4];
			break;
		case MSG_SSID_14:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_14[i/4];
			break;
		case MSG_SSID_15:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_15[i/4];
			break;
		case MSG_SSID_16:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_16[i/4];
			break;
		case MSG_SSID_17:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_17[i/4];
			break;
		case MSG_SSID_18:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_18[i/4];
			break;
		case MSG_SSID_19:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_19[i/4];
			break;
		case MSG_SSID_20:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_20[i/4];
			break;
		case MSG_SSID_21:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_21[i/4];
			break;
		case MSG_SSID_22:
			for (i = 0; i < ssid_range; i += 4)
				*(int *)(ptr + i) = msg_bld_masks_22[i/4];
			break;
		}
		encode_rsp_and_send(8 + ssid_range - 1);
		return 0;
	}
	/* Check for download command */
	else if ((cpu_is_msm8x60() || chk_apps_master()) && (*buf == 0x3A)) {
		/* send response back */
		driver->apps_rsp_buf[0] = *buf;
		encode_rsp_and_send(0);
		msleep(5000);
		/* call download API */
		msm_set_restart_mode(RESTART_DLOAD);
		printk(KERN_CRIT "diag: download mode set, Rebooting SoC..\n");
		kernel_restart(NULL);
		/* Not required, represents that command isnt sent to modem */
		return 0;
	}
	/* Check for polling for Apps only DIAG */
	else if ((*buf == 0x4b) && (*(buf+1) == 0x32) &&
		(*(buf+2) == 0x03)) {
		/* If no one has registered for polling */
		if (chk_polling_response()) {
			/* Respond to polling for Apps only DIAG */
			for (i = 0; i < 3; i++)
				driver->apps_rsp_buf[i] = *(buf+i);
			for (i = 0; i < 13; i++)
				driver->apps_rsp_buf[i+3] = 0;

			encode_rsp_and_send(15);
			return 0;
		}
	}
	/* Return the Delayed Response Wrap Status */
	else if ((*buf == 0x4b) && (*(buf+1) == 0x32) &&
		(*(buf+2) == 0x04) && (*(buf+3) == 0x0)) {
		memcpy(driver->apps_rsp_buf, buf, 4);
		driver->apps_rsp_buf[4] = wrap_enabled;
		encode_rsp_and_send(4);
		return 0;
	}
	/* Wrap the Delayed Rsp ID */
	else if ((*buf == 0x4b) && (*(buf+1) == 0x32) &&
		(*(buf+2) == 0x05) && (*(buf+3) == 0x0)) {
		wrap_enabled = true;
		memcpy(driver->apps_rsp_buf, buf, 4);
		driver->apps_rsp_buf[4] = wrap_count;
		encode_rsp_and_send(5);
		return 0;
	}
	 /* Check for ID for NO MODEM present */
	else if (chk_polling_response()) {
		/* respond to 0x0 command */
		if (*buf == 0x00) {
			for (i = 0; i < 55; i++)
				driver->apps_rsp_buf[i] = 0;

			encode_rsp_and_send(54);
			return 0;
		}
		/* respond to 0x7c command */
		else if (*buf == 0x7c) {
			driver->apps_rsp_buf[0] = 0x7c;
			for (i = 1; i < 8; i++)
				driver->apps_rsp_buf[i] = 0;
			/* Tools ID for APQ 8060 */
			*(int *)(driver->apps_rsp_buf + 8) =
							 chk_config_get_id();
			*(unsigned char *)(driver->apps_rsp_buf + 12) = '\0';
			*(unsigned char *)(driver->apps_rsp_buf + 13) = '\0';
			encode_rsp_and_send(13);
			return 0;
		}
	}
#endif
	return packet_type;
}

#ifdef CONFIG_DIAG_OVER_USB
void diag_send_error_rsp(int index)
{
	int i;

	if (index > 490) {
		pr_err("diag: error response too huge, aborting\n");
		return;
	}
	driver->apps_rsp_buf[0] = 0x13; /* error code 13 */
	for (i = 0; i < index; i++)
		driver->apps_rsp_buf[i+1] = *(driver->hdlc_buf+i);
	encode_rsp_and_send(index - 3);
}
#else
static inline void diag_send_error_rsp(int index) {}
#endif

void diag_process_hdlc(void *data, unsigned len)
{
	struct diag_hdlc_decode_type hdlc;
	int ret, type = 0;

	mutex_lock(&driver->diag_hdlc_mutex);

	pr_debug("diag: HDLC decode fn, len of data  %d\n", len);
	hdlc.dest_ptr = driver->hdlc_buf;
	hdlc.dest_size = USB_MAX_OUT_BUF;
	hdlc.src_ptr = data;
	hdlc.src_size = len;
	hdlc.src_idx = 0;
	hdlc.dest_idx = 0;
	hdlc.escaping = 0;

	ret = diag_hdlc_decode(&hdlc);

	/*
	 * If the message is 3 bytes or less in length then the message is
	 * too short. A message will need 4 bytes minimum, since there are
	 * 2 bytes for the CRC and 1 byte for the ending 0x7e for the hdlc
	 * encoding
	 */
	if (hdlc.dest_idx < 4) {
		pr_err_ratelimited("diag: In %s, message is too short, len: %d, dest len: %d\n",
			__func__, len, hdlc.dest_idx);
		mutex_unlock(&driver->diag_hdlc_mutex);
		return;
	}

	if (ret) {
		type = diag_process_apps_pkt(driver->hdlc_buf,
							  hdlc.dest_idx - 3);
		if (type < 0) {
			mutex_unlock(&driver->diag_hdlc_mutex);
			return;
		}
	} else if (driver->debug_flag) {
		printk(KERN_ERR "Packet dropped due to bad HDLC coding/CRC"
				" errors or partial packet received, packet"
				" length = %d\n", len);
		print_hex_dump(KERN_DEBUG, "Dropped Packet Data: ", 16, 1,
					   DUMP_PREFIX_ADDRESS, data, len, 1);
		driver->debug_flag = 0;
	}
	/* send error responses from APPS for Central Routing */
	if (type == 1 && chk_apps_only()) {
		diag_send_error_rsp(hdlc.dest_idx);
		type = 0;
	}
	/* implies this packet is NOT meant for apps */
	if (!(driver->smd_data[MODEM_DATA].ch) && type == 1) {
		if (chk_apps_only()) {
			diag_send_error_rsp(hdlc.dest_idx);
		} else { /* APQ 8060, Let Q6 respond */
			if (driver->smd_data[LPASS_DATA].ch) {
				mutex_lock(&driver->smd_data[LPASS_DATA].
								smd_ch_mutex);
				smd_write(driver->smd_data[LPASS_DATA].ch,
						driver->hdlc_buf,
						hdlc.dest_idx - 3);
				mutex_unlock(&driver->smd_data[LPASS_DATA].
								smd_ch_mutex);
			}
		}
		type = 0;
	}

#ifdef DIAG_DEBUG
	pr_debug("diag: hdlc.dest_idx = %d", hdlc.dest_idx);
	for (i = 0; i < hdlc.dest_idx; i++)
		printk(KERN_DEBUG "\t%x", *(((unsigned char *)
							driver->hdlc_buf)+i));
#endif /* DIAG DEBUG */
	/* ignore 2 bytes for CRC, one for 7E and send */
	if ((driver->smd_data[MODEM_DATA].ch) && (ret) && (type) &&
						(hdlc.dest_idx > 3)) {
		APPEND_DEBUG('g');
		mutex_lock(&driver->smd_data[MODEM_DATA].smd_ch_mutex);
		smd_write(driver->smd_data[MODEM_DATA].ch,
					driver->hdlc_buf, hdlc.dest_idx - 3);
		mutex_unlock(&driver->smd_data[MODEM_DATA].smd_ch_mutex);
		APPEND_DEBUG('h');
#ifdef DIAG_DEBUG
		printk(KERN_INFO "writing data to SMD, pkt length %d\n", len);
		print_hex_dump(KERN_DEBUG, "Written Packet Data to SMD: ", 16,
			       1, DUMP_PREFIX_ADDRESS, data, len, 1);
#endif /* DIAG DEBUG */
	}
	mutex_unlock(&driver->diag_hdlc_mutex);
}

#ifdef CONFIG_DIAG_OVER_USB
/* 2+1 for modem ; 2 for LPASS ; 1 for WCNSS */
#define N_LEGACY_WRITE	(driver->poolsize + 6)
#define N_LEGACY_READ	1

int diagfwd_connect(void)
{
	int err;
	int i;

	printk(KERN_DEBUG "diag: USB connected\n");
	err = usb_diag_alloc_req(driver->legacy_ch, N_LEGACY_WRITE,
			N_LEGACY_READ);
	if (err)
		printk(KERN_ERR "diag: unable to alloc USB req on legacy ch");

	driver->usb_connected = 1;
	for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {
		driver->smd_data[i].in_busy_1 = 0;
		driver->smd_data[i].in_busy_2 = 0;
		/* Poll SMD data channels to check for data */
		queue_work(driver->diag_wq,
			&(driver->smd_data[i].diag_read_smd_work));
		/* Poll SMD CNTL channels to check for data */
		diag_smd_notify(&(driver->smd_cntl[i]), SMD_EVENT_DATA);
	}

	/* Poll USB channel to check for data*/
	queue_work(driver->diag_wq, &(driver->diag_read_work));
#ifdef CONFIG_DIAG_SDIO_PIPE
	if (machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa()) {
		if (driver->mdm_ch && !IS_ERR(driver->mdm_ch))
			diagfwd_connect_sdio();
		else
			printk(KERN_INFO "diag: No USB MDM ch");
	}
#endif
	return 0;
}

int diagfwd_disconnect(void)
{
	int i;

	printk(KERN_DEBUG "diag: USB disconnected\n");
	driver->usb_connected = 0;
	driver->debug_flag = 1;
	usb_diag_free_req(driver->legacy_ch);
	if (driver->logging_mode == USB_MODE) {
		for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {
			driver->smd_data[i].in_busy_1 = 1;
			driver->smd_data[i].in_busy_2 = 1;
		}
	}
#ifdef CONFIG_DIAG_SDIO_PIPE
	if (machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa())
		if (driver->mdm_ch && !IS_ERR(driver->mdm_ch))
			diagfwd_disconnect_sdio();
#endif
	/* TBD - notify and flow control SMD */
	return 0;
}

int diagfwd_write_complete(struct diag_request *diag_write_ptr)
{
	unsigned char *buf = diag_write_ptr->buf;
	int found_it = 0;
	int i;

	/* Determine if the write complete is for data from modem/apps/q6 */
	/* Need a context variable here instead */
	for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++) {
		struct diag_smd_info *data = &(driver->smd_data[i]);
		if (buf == (void *)data->buf_in_1) {
			data->in_busy_1 = 0;
			queue_work(driver->diag_wq,
				&(data->diag_read_smd_work));
			found_it = 1;
			break;
		} else if (buf == (void *)data->buf_in_2) {
			data->in_busy_2 = 0;
			queue_work(driver->diag_wq,
				&(data->diag_read_smd_work));
			found_it = 1;
			break;
		}
	}
#ifdef CONFIG_DIAG_SDIO_PIPE
	if (!found_it) {
		if (buf == (void *)driver->buf_in_sdio) {
			if (machine_is_msm8x60_fusion() ||
				 machine_is_msm8x60_fusn_ffa())
				diagfwd_write_complete_sdio();
			else
				pr_err("diag: Incorrect buffer pointer while WRITE");
			found_it = 1;
		}
	}
#endif
	if (!found_it) {
		diagmem_free(driver, (unsigned char *)buf,
						POOL_TYPE_HDLC);
		diagmem_free(driver, (unsigned char *)diag_write_ptr,
						POOL_TYPE_WRITE_STRUCT);
	}
	return 0;
}

int diagfwd_read_complete(struct diag_request *diag_read_ptr)
{
	int status = diag_read_ptr->status;
	unsigned char *buf = diag_read_ptr->buf;

	/* Determine if the read complete is for data on legacy/mdm ch */
	if (buf == (void *)driver->usb_buf_out) {
		driver->read_len_legacy = diag_read_ptr->actual;
		APPEND_DEBUG('s');
#ifdef DIAG_DEBUG
		printk(KERN_INFO "read data from USB, pkt length %d",
		    diag_read_ptr->actual);
		print_hex_dump(KERN_DEBUG, "Read Packet Data from USB: ", 16, 1,
		       DUMP_PREFIX_ADDRESS, diag_read_ptr->buf,
		       diag_read_ptr->actual, 1);
#endif /* DIAG DEBUG */
		if (driver->logging_mode == USB_MODE) {
			if (status != -ECONNRESET && status != -ESHUTDOWN)
				queue_work(driver->diag_wq,
					&(driver->diag_proc_hdlc_work));
			else
				queue_work(driver->diag_wq,
						 &(driver->diag_read_work));
		}
	}
#ifdef CONFIG_DIAG_SDIO_PIPE
	else if (buf == (void *)driver->usb_buf_mdm_out) {
		if (machine_is_msm8x60_fusion() ||
				 machine_is_msm8x60_fusn_ffa()) {
			driver->read_len_mdm = diag_read_ptr->actual;
			diagfwd_read_complete_sdio();
		} else
			pr_err("diag: Incorrect buffer pointer while READ");
	}
#endif
	else
		printk(KERN_ERR "diag: Unknown buffer ptr from USB");

	return 0;
}

void diag_read_work_fn(struct work_struct *work)
{
	APPEND_DEBUG('d');
	driver->usb_read_ptr->buf = driver->usb_buf_out;
	driver->usb_read_ptr->length = USB_MAX_OUT_BUF;
	usb_diag_read(driver->legacy_ch, driver->usb_read_ptr);
	APPEND_DEBUG('e');
}

void diag_process_hdlc_fn(struct work_struct *work)
{
	APPEND_DEBUG('D');
	diag_process_hdlc(driver->usb_buf_out, driver->read_len_legacy);
	diag_read_work_fn(work);
	APPEND_DEBUG('E');
}

void diag_usb_legacy_notifier(void *priv, unsigned event,
			struct diag_request *d_req)
{
	switch (event) {
	case USB_DIAG_CONNECT:
		diagfwd_connect();
		break;
	case USB_DIAG_DISCONNECT:
		diagfwd_disconnect();
		break;
	case USB_DIAG_READ_DONE:
		diagfwd_read_complete(d_req);
		break;
	case USB_DIAG_WRITE_DONE:
		diagfwd_write_complete(d_req);
		break;
	default:
		printk(KERN_ERR "Unknown event from USB diag\n");
		break;
	}
}

#endif /* DIAG OVER USB */

void diag_smd_notify(void *ctxt, unsigned event)
{
	struct diag_smd_info *smd_info = (struct diag_smd_info *)ctxt;
	if (!smd_info)
		return;

	if (event == SMD_EVENT_CLOSE) {
		smd_info->ch = 0;
		wake_up(&driver->smd_wait_q);
		if (smd_info->type == SMD_DATA_TYPE) {
			smd_info->notify_context = event;
			queue_work(driver->diag_cntl_wq,
				 &(smd_info->diag_notify_update_smd_work));
		} else if (smd_info->type == SMD_DCI_TYPE) {
			/* Notify the clients of the close */
			diag_dci_notify_client(smd_info->peripheral_mask,
							DIAG_STATUS_CLOSED);
		}
		return;
	} else if (event == SMD_EVENT_OPEN) {
		if (smd_info->ch_save)
			smd_info->ch = smd_info->ch_save;

		if (smd_info->type == SMD_CNTL_TYPE) {
			smd_info->notify_context = event;
			queue_work(driver->diag_cntl_wq,
				&(smd_info->diag_notify_update_smd_work));
		} else if (smd_info->type == SMD_DCI_TYPE) {
			smd_info->notify_context = event;
			queue_work(driver->diag_dci_wq,
				&(smd_info->diag_notify_update_smd_work));
			/* Notify the clients of the open */
			diag_dci_notify_client(smd_info->peripheral_mask,
							DIAG_STATUS_OPEN);
		}
	} else if (event == SMD_EVENT_DATA && !driver->real_time_mode &&
					smd_info->type == SMD_DATA_TYPE) {
		process_lock_on_notify(&smd_info->nrt_lock);
	}

	wake_up(&driver->smd_wait_q);

	if (smd_info->type == SMD_DCI_TYPE)
		queue_work(driver->diag_dci_wq,
				&(smd_info->diag_read_smd_work));
	else
		queue_work(driver->diag_wq, &(smd_info->diag_read_smd_work));
}

static int diag_smd_probe(struct platform_device *pdev)
{
	int r = 0;
	int index = -1;

	if (pdev->id == SMD_APPS_MODEM) {
		index = MODEM_DATA;
		r = smd_open("DIAG", &driver->smd_data[index].ch,
					&driver->smd_data[index],
					diag_smd_notify);
		driver->smd_data[index].ch_save =
					driver->smd_data[index].ch;
	}
#if defined(CONFIG_MSM_N_WAY_SMD)
	if (pdev->id == SMD_APPS_QDSP) {
		index = LPASS_DATA;
		r = smd_named_open_on_edge("DIAG", SMD_APPS_QDSP,
					&driver->smd_data[index].ch,
					&driver->smd_data[index],
					diag_smd_notify);
		driver->smd_data[index].ch_save =
					driver->smd_data[index].ch;
	}
#endif
	if (pdev->id == SMD_APPS_WCNSS) {
		index = WCNSS_DATA;
		r = smd_named_open_on_edge("APPS_RIVA_DATA",
					SMD_APPS_WCNSS,
					&driver->smd_data[index].ch,
					&driver->smd_data[index],
					diag_smd_notify);
		driver->smd_data[index].ch_save =
					driver->smd_data[index].ch;
	}

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pr_debug("diag: open SMD port, Id = %d, r = %d\n", pdev->id, r);

	return 0;
}

static int diag_smd_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int diag_smd_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops diag_smd_dev_pm_ops = {
	.runtime_suspend = diag_smd_runtime_suspend,
	.runtime_resume = diag_smd_runtime_resume,
};

static struct platform_driver msm_smd_ch1_driver = {

	.probe = diag_smd_probe,
	.driver = {
		.name = "DIAG",
		.owner = THIS_MODULE,
		.pm   = &diag_smd_dev_pm_ops,
	},
};

static struct platform_driver diag_smd_lite_driver = {

	.probe = diag_smd_probe,
	.driver = {
		.name = "APPS_RIVA_DATA",
		.owner = THIS_MODULE,
		.pm   = &diag_smd_dev_pm_ops,
	},
};

void diag_smd_destructor(struct diag_smd_info *smd_info)
{
	if (smd_info->type == SMD_DATA_TYPE)
		wake_lock_destroy(&smd_info->nrt_lock.read_lock);

	if (smd_info->ch)
		smd_close(smd_info->ch);

	smd_info->ch = 0;
	smd_info->ch_save = 0;
	kfree(smd_info->buf_in_1);
	kfree(smd_info->buf_in_2);
	kfree(smd_info->write_ptr_1);
	kfree(smd_info->write_ptr_2);
}

int diag_smd_constructor(struct diag_smd_info *smd_info, int peripheral,
			  int type)
{
	smd_info->peripheral = peripheral;
	smd_info->type = type;
	mutex_init(&smd_info->smd_ch_mutex);

	switch (peripheral) {
	case MODEM_DATA:
		smd_info->peripheral_mask = DIAG_CON_MPSS;
		break;
	case LPASS_DATA:
		smd_info->peripheral_mask = DIAG_CON_LPASS;
		break;
	case WCNSS_DATA:
		smd_info->peripheral_mask = DIAG_CON_WCNSS;
		break;
	default:
		pr_err("diag: In %s, unknown peripheral, peripheral: %d\n",
			__func__, peripheral);
		goto err;
	}

	smd_info->ch = 0;
	smd_info->ch_save = 0;

	if (smd_info->buf_in_1 == NULL) {
		smd_info->buf_in_1 = kzalloc(IN_BUF_SIZE, GFP_KERNEL);
		if (smd_info->buf_in_1 == NULL)
			goto err;
		kmemleak_not_leak(smd_info->buf_in_1);
	}

	if (smd_info->write_ptr_1 == NULL) {
		smd_info->write_ptr_1 = kzalloc(sizeof(struct diag_request),
								GFP_KERNEL);
		if (smd_info->write_ptr_1 == NULL)
			goto err;
		kmemleak_not_leak(smd_info->write_ptr_1);
	}

	/* The smd data type needs two buffers */
	if (smd_info->type == SMD_DATA_TYPE) {
		if (smd_info->buf_in_2 == NULL) {
			smd_info->buf_in_2 = kzalloc(IN_BUF_SIZE, GFP_KERNEL);
			if (smd_info->buf_in_2 == NULL)
				goto err;
			kmemleak_not_leak(smd_info->buf_in_2);
		}
		if (smd_info->write_ptr_2 == NULL) {
			smd_info->write_ptr_2 =
				kzalloc(sizeof(struct diag_request),
				GFP_KERNEL);
			if (smd_info->write_ptr_2 == NULL)
				goto err;
			kmemleak_not_leak(smd_info->write_ptr_2);
		}
	}

	INIT_WORK(&(smd_info->diag_read_smd_work), diag_read_smd_work_fn);

	/*
	 * The update function assigned to the diag_notify_update_smd_work
	 * work_struct is meant to be used for updating that is not to
	 * be done in the context of the smd notify function. The
	 * notify_context variable can be used for passing additional
	 * information to the update function.
	 */
	smd_info->notify_context = 0;
	if (type == SMD_DATA_TYPE)
		INIT_WORK(&(smd_info->diag_notify_update_smd_work),
							diag_clean_reg_fn);
	else if (type == SMD_CNTL_TYPE)
		INIT_WORK(&(smd_info->diag_notify_update_smd_work),
							diag_mask_update_fn);
	else if (type == SMD_DCI_TYPE)
		INIT_WORK(&(smd_info->diag_notify_update_smd_work),
						diag_update_smd_dci_work_fn);
	else {
		pr_err("diag: In %s, unknown type, type: %d\n", __func__, type);
		goto err;
	}

	/*
	 * Set function ptr for function to call to process the data that
	 * was just read from the smd channel
	 */
	if (type == SMD_DATA_TYPE)
		smd_info->process_smd_read_data = diag_process_smd_read_data;
	else if (type == SMD_CNTL_TYPE)
		smd_info->process_smd_read_data =
						diag_process_smd_cntl_read_data;
	else if (type == SMD_DCI_TYPE)
		smd_info->process_smd_read_data =
						diag_process_smd_dci_read_data;
	else {
		pr_err("diag: In %s, unknown type, type: %d\n", __func__, type);
		goto err;
	}

	smd_info->nrt_lock.enabled = 0;
	smd_info->nrt_lock.ref_count = 0;
	smd_info->nrt_lock.copy_count = 0;
	if (type == SMD_DATA_TYPE) {
		spin_lock_init(&smd_info->nrt_lock.read_spinlock);

		switch (peripheral) {
		case MODEM_DATA:
			wake_lock_init(&smd_info->nrt_lock.read_lock,
				WAKE_LOCK_SUSPEND, "diag_nrt_modem_read");
			break;
		case LPASS_DATA:
			wake_lock_init(&smd_info->nrt_lock.read_lock,
				WAKE_LOCK_SUSPEND, "diag_nrt_lpass_read");
			break;
		case WCNSS_DATA:
			wake_lock_init(&smd_info->nrt_lock.read_lock,
				WAKE_LOCK_SUSPEND, "diag_nrt_wcnss_read");
			break;
		default:
			break;
		}
	}

	return 1;
err:
	kfree(smd_info->buf_in_1);
	kfree(smd_info->buf_in_2);
	kfree(smd_info->write_ptr_1);
	kfree(smd_info->write_ptr_2);

	return 0;
}

void diagfwd_init(void)
{
	int success;
	int i;

	wrap_enabled = 0;
	wrap_count = 0;
	diag_debug_buf_idx = 0;
	driver->read_len_legacy = 0;
	driver->use_device_tree = has_device_tree();
	driver->real_time_mode = 1;
	mutex_init(&driver->diag_hdlc_mutex);
	mutex_init(&driver->diag_cntl_mutex);

	success = diag_smd_constructor(&driver->smd_data[MODEM_DATA],
					MODEM_DATA, SMD_DATA_TYPE);
	if (!success)
		goto err;

	success = diag_smd_constructor(&driver->smd_data[LPASS_DATA],
					LPASS_DATA, SMD_DATA_TYPE);
	if (!success)
		goto err;

	success = diag_smd_constructor(&driver->smd_data[WCNSS_DATA],
					WCNSS_DATA, SMD_DATA_TYPE);
	if (!success)
		goto err;

	if (driver->usb_buf_out  == NULL &&
	     (driver->usb_buf_out = kzalloc(USB_MAX_OUT_BUF,
					 GFP_KERNEL)) == NULL)
		goto err;
	kmemleak_not_leak(driver->usb_buf_out);
	if (driver->hdlc_buf == NULL
	    && (driver->hdlc_buf = kzalloc(HDLC_MAX, GFP_KERNEL)) == NULL)
		goto err;
	kmemleak_not_leak(driver->hdlc_buf);
	if (driver->user_space_data == NULL)
		driver->user_space_data = kzalloc(USER_SPACE_DATA, GFP_KERNEL);
		if (driver->user_space_data == NULL)
			goto err;
	kmemleak_not_leak(driver->user_space_data);
	if (driver->client_map == NULL &&
	    (driver->client_map = kzalloc
	     ((driver->num_clients) * sizeof(struct diag_client_map),
		   GFP_KERNEL)) == NULL)
		goto err;
	kmemleak_not_leak(driver->client_map);
	if (driver->buf_tbl == NULL)
			driver->buf_tbl = kzalloc(buf_tbl_size *
			  sizeof(struct diag_write_device), GFP_KERNEL);
	if (driver->buf_tbl == NULL)
		goto err;
	kmemleak_not_leak(driver->buf_tbl);
	if (driver->data_ready == NULL &&
	     (driver->data_ready = kzalloc(driver->num_clients * sizeof(int)
							, GFP_KERNEL)) == NULL)
		goto err;
	kmemleak_not_leak(driver->data_ready);
	if (driver->table == NULL &&
	     (driver->table = kzalloc(diag_max_reg*
		      sizeof(struct diag_master_table),
		       GFP_KERNEL)) == NULL)
		goto err;
	kmemleak_not_leak(driver->table);

	if (driver->usb_read_ptr == NULL) {
		driver->usb_read_ptr = kzalloc(
			sizeof(struct diag_request), GFP_KERNEL);
		if (driver->usb_read_ptr == NULL)
			goto err;
		kmemleak_not_leak(driver->usb_read_ptr);
	}
	if (driver->pkt_buf == NULL &&
	     (driver->pkt_buf = kzalloc(PKT_SIZE,
			 GFP_KERNEL)) == NULL)
		goto err;
	kmemleak_not_leak(driver->pkt_buf);
	if (driver->apps_rsp_buf == NULL) {
		driver->apps_rsp_buf = kzalloc(APPS_BUF_SIZE, GFP_KERNEL);
		if (driver->apps_rsp_buf == NULL)
			goto err;
		kmemleak_not_leak(driver->apps_rsp_buf);
	}
	driver->diag_wq = create_singlethread_workqueue("diag_wq");
#ifdef CONFIG_DIAG_OVER_USB
	INIT_WORK(&(driver->diag_proc_hdlc_work), diag_process_hdlc_fn);
	INIT_WORK(&(driver->diag_read_work), diag_read_work_fn);
	driver->legacy_ch = usb_diag_open(DIAG_LEGACY, driver,
			diag_usb_legacy_notifier);
	if (IS_ERR(driver->legacy_ch)) {
		printk(KERN_ERR "Unable to open USB diag legacy channel\n");
		goto err;
	}
#endif
	platform_driver_register(&msm_smd_ch1_driver);
	platform_driver_register(&diag_smd_lite_driver);
	return;
err:
	pr_err("diag: Could not initialize diag buffers");

	for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++)
		diag_smd_destructor(&driver->smd_data[i]);

	kfree(driver->buf_msg_mask_update);
	kfree(driver->buf_log_mask_update);
	kfree(driver->buf_event_mask_update);
	kfree(driver->usb_buf_out);
	kfree(driver->hdlc_buf);
	kfree(driver->client_map);
	kfree(driver->buf_tbl);
	kfree(driver->data_ready);
	kfree(driver->table);
	kfree(driver->pkt_buf);
	kfree(driver->usb_read_ptr);
	kfree(driver->apps_rsp_buf);
	kfree(driver->user_space_data);
	if (driver->diag_wq)
		destroy_workqueue(driver->diag_wq);
}

void diagfwd_exit(void)
{
	int i;

	for (i = 0; i < NUM_SMD_DATA_CHANNELS; i++)
		diag_smd_destructor(&driver->smd_data[i]);

#ifdef CONFIG_DIAG_OVER_USB
	if (driver->usb_connected)
		usb_diag_free_req(driver->legacy_ch);
	usb_diag_close(driver->legacy_ch);
#endif
	platform_driver_unregister(&msm_smd_ch1_driver);
	platform_driver_unregister(&msm_diag_dci_driver);
	platform_driver_unregister(&diag_smd_lite_driver);

	kfree(driver->buf_msg_mask_update);
	kfree(driver->buf_log_mask_update);
	kfree(driver->buf_event_mask_update);
	kfree(driver->usb_buf_out);
	kfree(driver->hdlc_buf);
	kfree(driver->client_map);
	kfree(driver->buf_tbl);
	kfree(driver->data_ready);
	kfree(driver->table);
	kfree(driver->pkt_buf);
	kfree(driver->usb_read_ptr);
	kfree(driver->apps_rsp_buf);
	kfree(driver->user_space_data);
	destroy_workqueue(driver->diag_wq);
}
