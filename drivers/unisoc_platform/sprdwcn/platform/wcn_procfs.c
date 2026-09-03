/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * File:		wcn_procfs.c
 * Description:	Marlin Debug System main file. Module,device &
 * driver related defination.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the	1
 * GNU General Public License for more details.
 */

#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/wait.h>
#include <misc/marlin_platform.h>
#include <misc/wcn_bus.h>

#include "edma_engine.h"
#include "pcie.h"
#include "wcn_dump.h"
#include "wcn_misc.h"
#include "wcn_glb.h"
#include "wcn_log.h"
#include "wcn_misc.h"
#include "wcn_procfs.h"
#include "wcn_txrx.h"
#include "mdbg_type.h"
#include "../include/wcn_dbg.h"

u32 wcn_print_level = WCN_DEBUG_OFF;

static u32 g_dumpmem_switch =  1;
static u32 g_loopcheck_switch;
struct wcn_pcie_info *pcie_dev;

struct mdbg_proc_entry {
	char *name;
	struct proc_dir_entry *entry;
	struct completion completed;
	wait_queue_head_t	rxwait;
	unsigned int	rcv_len;
	void *buf;
};

struct async_assert_t {
	struct work_struct assert_wq;
	/*device_suspend/device_resume device_lock()*/
	atomic_t dev_lock;
	atomic_t flag;
	enum wcn_source_type async_assert_type;
	char async_assert_str[128];
};

struct mdbg_proc_t {
	char *dir_name;
	struct proc_dir_entry		*procdir;
	struct mdbg_proc_entry		assert;
	struct mdbg_proc_entry		loopcheck;
	struct mdbg_proc_entry		at_cmd;
	struct mdbg_proc_entry		snap_shoot;
	struct mutex		mutex;
	char write_buf[MDBG_WRITE_SIZE];
	int fail_count;
	int assert_notify_flag;
	bool loopcheck_flag;

	/*see device_lock*/
	struct async_assert_t async_assert;
};

static struct mdbg_proc_t *mdbg_proc;
static int dump_cnt;

void mdbg_device_lock_notify(void)
{
	WARN_ON(atomic_read(&mdbg_proc->async_assert.dev_lock));
	atomic_set(&mdbg_proc->async_assert.dev_lock, 1);
}
EXPORT_SYMBOL_GPL(mdbg_device_lock_notify);

void mdbg_device_unlock_notify(void)
{
	WARN_ON(!atomic_read(&mdbg_proc->async_assert.dev_lock));
	atomic_set(&mdbg_proc->async_assert.dev_lock, 0);
}
EXPORT_SYMBOL_GPL(mdbg_device_unlock_notify);

void wcn_reset_process(void)
{
	WCN_INFO("%s reset begin\n", __func__);
	wcn_reset_cp2();
	mdbg_proc->assert_notify_flag = 0;
	dump_cnt = 0;
	sprdwcn_bus_set_carddump_status(false);
	/*notify slogmodem to restore the state of saving dump*/
	wcn_notify_fw_error(WCN_SOURCE_CP2_ALIVE, "cp2 alive");
	WCN_INFO("%s reset end\n", __func__);
}

void wcn_dump_process(enum wcn_source_type type)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (dump_cnt) {
		WCN_ERR("dump_cnt: %d, not dump again!\n", dump_cnt);
		return;
	}

	WCN_INFO("%s dumpmem begin\n", __func__);
	sprdwcn_bus_set_carddump_status(true);

	if (g_match_config && g_match_config->unisoc_wcn_pcie) {
		edma_hw_pause();
		dump_arm_reg();
	}

	dump_cnt++;

	if (g_match_config && g_match_config->unisoc_wcn_integrated)
		mdbg_dump_mem_integ(type);
	else
		mdbg_dump_mem();

	WCN_INFO("%s dumpmem end\n", __func__);
}

bool wcn_is_assert(void)
{
	return mutex_is_locked(&mdbg_proc->mutex);
}
EXPORT_SYMBOL_GPL(wcn_is_assert);

void __wcn_assert_interface(enum wcn_source_type type, char *str)
{
	int reset_prop = wcn_sysfs_get_reset_prop();

	WCN_INFO("wcn_assert_interface %d\n", reset_prop);
	WCN_ERR("wcn_source_type:%d\n", type);
	WCN_ERR("fw assert:%s\n", str);
	if (g_dumpmem_switch == 0) {
		WCN_ERR("dump disable!\n");
		return;
	}
	if (!mutex_trylock(&mdbg_proc->mutex)) {
		WCN_ERR("fw assert hanppend already\n");
		return;
	}
	if (!marlin_get_power()) {
		WCN_INFO("no modules open\n");
		goto out;
	}

	if (!mdbg_proc->assert_notify_flag) {
		wcn_notify_fw_error(type, str);
		mdbg_proc->assert_notify_flag = 1;
	}

	sprdwcn_bus_debug_point_show();
	/*wcn reset or dump process*/
	stop_loopcheck();
	wcnlog_clear_log();

	if (reset_prop == WCN_ASSERT_ONLY_DUMP) { /*userdebug version*/
		wcn_dump_process(type);
		goto out;
	} else if (reset_prop == WCN_ASSERT_ONLY_RESET) { /*user version*/
		wcn_reset_process();
		goto out;
	} else if (reset_prop == WCN_ASSERT_BOTH_RESET_DUMP) {
		/*need to do, please reference androidr_trunk_u01*/
		wcn_dump_process(type);
		msleep(2000);
		wcn_reset_process();
		sprdwcn_bus_set_carddump_status(false);
		/*notify slogmodem to restore the state of saving dump*/
		wcn_notify_fw_error(WCN_SOURCE_CP2_ALIVE, "save dump");
		goto out;
	}
out:
	mutex_unlock(&mdbg_proc->mutex);
}

char wcn_assert_str[128];
void wcn_assert_interface(enum wcn_source_type type, char *str)
{
	strscpy(wcn_assert_str, str, sizeof(wcn_assert_str));

	if (in_interrupt() || unlikely(atomic_read(&mdbg_proc->async_assert.flag)) ||
		unlikely(atomic_read(&mdbg_proc->async_assert.dev_lock))) {
		/* dump: dev_lock(resume), cause deadlock in system resume. see mdbg_dump_data() */
		WCN_INFO("Async assert! %ld,%d,%d\n", in_interrupt(),
			atomic_read(&mdbg_proc->async_assert.flag),
			atomic_read(&mdbg_proc->async_assert.dev_lock));
		mdbg_proc->async_assert.async_assert_type = type;
		strscpy(mdbg_proc->async_assert.async_assert_str, str,
				sizeof(mdbg_proc->async_assert.async_assert_str));
		schedule_work(&mdbg_proc->async_assert.assert_wq);
	} else
		__wcn_assert_interface(type, str);
}
EXPORT_SYMBOL_GPL(wcn_assert_interface);

void wcn_assert_interface_async(enum wcn_source_type type, char *str)
{
	atomic_set(&mdbg_proc->async_assert.flag, 1);
	wcn_assert_interface(type, str);
	atomic_set(&mdbg_proc->async_assert.flag, 0);
}
EXPORT_SYMBOL_GPL(wcn_assert_interface_async);

static void wcn_assert_wq(struct work_struct *work)
{
	struct async_assert_t *as = container_of(work, struct async_assert_t, assert_wq);

	if (IS_ERR_OR_NULL(as))
		return;

	__wcn_assert_interface(as->async_assert_type, as->async_assert_str);
}

void mdbg_assert_interface(char *str)
{
	wcn_assert_interface(WCN_SOURCE_BTWF, str);
}
EXPORT_SYMBOL_GPL(mdbg_assert_interface);

int mdbg_assert_read(int channel, struct mbuf_t *head,
		     struct mbuf_t *tail, int num)
{
	struct bus_puh_t *puh = NULL;
	struct wcn_match_data *g_match_config = get_wcn_match_config();
	unsigned int pub_head_rsv;

	if (g_match_config && g_match_config->unisoc_wcn_sdio)
		pub_head_rsv = SDIOHAL_PUB_HEAD_RSV;
	else
		pub_head_rsv = PUB_HEAD_RSV;

	if (g_match_config && (!g_match_config->unisoc_wcn_pcie) &&
		(!g_match_config->unisoc_wcn_sipc)) {
		puh = (struct bus_puh_t *)head->buf;
		if (puh->len > MDBG_ASSERT_SIZE) {
			WCN_ERR("assert data len:%d,beyond max read:%d",
				puh->len, MDBG_ASSERT_SIZE);
			sprdwcn_bus_push_list(channel, head, tail, num);
			return -1;
		}

		wcn_assert_interface(WCN_SOURCE_BTWF, head->buf + pub_head_rsv);
	} else {
		wcn_assert_interface(WCN_SOURCE_BTWF, head->buf);
	}
	sprdwcn_bus_push_list(channel, head, tail, num);

	return 0;
}
EXPORT_SYMBOL_GPL(mdbg_assert_read);

int mdbg_loopcheck_read(int channel, struct mbuf_t *head,
			struct mbuf_t *tail, int num)
{
	struct bus_puh_t *puh = NULL;
	struct wcn_match_data *g_match_config = get_wcn_match_config();
	unsigned int pub_head_rsv;

	if (g_match_config && g_match_config->unisoc_wcn_sdio)
		pub_head_rsv = SDIOHAL_PUB_HEAD_RSV;
	else
		pub_head_rsv = PUB_HEAD_RSV;

	if (g_match_config && (!g_match_config->unisoc_wcn_pcie) &&
		(!g_match_config->unisoc_wcn_sipc)) {
		puh = (struct bus_puh_t *)head->buf;
		if (puh->len > MDBG_LOOPCHECK_SIZE) {
			WCN_ERR("The loopcheck data len:%d,beyond max read:%d",
				puh->len, MDBG_LOOPCHECK_SIZE);
			sprdwcn_bus_push_list(channel, head, tail, num);
			return -1;
		}

		memset(mdbg_proc->loopcheck.buf, 0, MDBG_LOOPCHECK_SIZE);
		memcpy(mdbg_proc->loopcheck.buf, head->buf + pub_head_rsv, puh->len);
		mdbg_proc->loopcheck.rcv_len = puh->len;
	} else {
		memset(mdbg_proc->loopcheck.buf, 0, MDBG_LOOPCHECK_SIZE);
		memcpy(mdbg_proc->loopcheck.buf, head->buf, head->len);
		mdbg_proc->loopcheck.rcv_len = head->len;
	}
	WCN_INFO("rx:%s\n", (char *)(mdbg_proc->loopcheck.buf));
	mdbg_proc->fail_count = 0;
	complete(&mdbg_proc->loopcheck.completed);
	complete_kernel_loopcheck();
	sprdwcn_bus_push_list(channel, head, tail, num);

	return 0;
}
EXPORT_SYMBOL_GPL(mdbg_loopcheck_read);

int mdbg_at_cmd_read(int channel, struct mbuf_t *head,
		     struct mbuf_t *tail, int num)
{
	struct bus_puh_t *puh = NULL;
	struct wcn_match_data *g_match_config = get_wcn_match_config();
	unsigned int pub_head_rsv;

	if (g_match_config && g_match_config->unisoc_wcn_sdio)
		pub_head_rsv = SDIOHAL_PUB_HEAD_RSV;
	else
		pub_head_rsv = PUB_HEAD_RSV;

	if (g_match_config && (!g_match_config->unisoc_wcn_pcie) &&
		(!g_match_config->unisoc_wcn_sipc)) {
		puh = (struct bus_puh_t *)head->buf;
		if (puh->len > MDBG_AT_CMD_SIZE) {
			WCN_ERR("The at cmd data len:%d,beyond max read:%d",
				puh->len, MDBG_AT_CMD_SIZE);
			sprdwcn_bus_push_list(channel, head, tail, num);
			return -1;
		}

		memset(mdbg_proc->at_cmd.buf, 0, MDBG_AT_CMD_SIZE);
		memcpy(mdbg_proc->at_cmd.buf, head->buf + pub_head_rsv, puh->len);
		mdbg_proc->at_cmd.rcv_len = puh->len;
		WCN_INFO("at cmd read:%s\n",
			(char *)(mdbg_proc->at_cmd.buf));
		complete(&mdbg_proc->at_cmd.completed);
		notify_at_cmd_finish(mdbg_proc->at_cmd.buf, mdbg_proc->at_cmd.rcv_len);
		sprdwcn_bus_push_list(channel, head, tail, num);
	} else {
		memset(mdbg_proc->at_cmd.buf, 0, MDBG_AT_CMD_SIZE);
		memcpy(mdbg_proc->at_cmd.buf, head->buf, head->len);
		mdbg_proc->at_cmd.rcv_len = head->len;
		WCN_INFO("AT cmd read:%s\n",
			 (char *)(mdbg_proc->at_cmd.buf));
		complete(&mdbg_proc->at_cmd.completed);
		notify_at_cmd_finish(mdbg_proc->at_cmd.buf, mdbg_proc->at_cmd.rcv_len);
		sprdwcn_bus_push_list(channel, head, tail, num);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mdbg_at_cmd_read);

static int mdbg_tx_comptele_cb(int chn, int timeout)
{
	WCN_DBG("%s: chn=%d, timeout=%d\n", __func__, chn, timeout);

	return 0;
}

static int free_prepare_buf(struct dma_buf *dm)
{
	pcie_dev = get_wcn_device_info();
	if (!pcie_dev) {
		WCN_ERR("%s:PCIE device link error\n", __func__);
		return -1;
	}

	if (dm->vir && dm->phy)
		dmfree(pcie_dev, dm);

	return 0;
}

static int prepare_free_buf(struct dma_buf *dm, int chn, int size, int num)
{
	int ret, i;
	struct mbuf_t *mbuf, *head, *tail;

	pcie_dev = get_wcn_device_info();
	if (!pcie_dev) {
		WCN_ERR("%s:PCIE device link error\n", __func__);
		return -1;
	}
	ret = sprdwcn_bus_list_alloc(chn, &head, &tail, &num);
	if (ret != 0)
		return -1;
	for (i = 0, mbuf = head; i < num; i++) {
		ret = dmalloc(pcie_dev, dm, size);
		if (ret != 0)
			return -1;
		mbuf->buf = (unsigned char *)(dm->vir);
		mbuf->phy = (unsigned long)(dm->phy);
		mbuf->len = dm->size;
		memset(mbuf->buf, 0x0, mbuf->len);
		mbuf = mbuf->next;
	}

	ret = sprdwcn_bus_push_list(chn, head, tail, num);

	return ret;
}

static int loopcheck_prepare_buf(int chn, struct mbuf_t **head,
				 struct mbuf_t **tail, int *num)
{
	int ret;

	WCN_INFO("%s: chn=%d, num=%d\n", __func__, chn, *num);
	ret = sprdwcn_bus_list_alloc(chn, head, tail, num);

	return ret;
}

static int at_cmd_prepare_buf(int chn, struct mbuf_t **head,
			      struct mbuf_t **tail, int *num)
{
	int ret;

	WCN_INFO("%s: chn=%d, num=%d\n", __func__, chn, *num);
	ret = sprdwcn_bus_list_alloc(chn, head, tail, num);

	return ret;
}

static int assert_prepare_buf(int chn, struct mbuf_t **head,
			      struct mbuf_t **tail, int *num)
{
	int ret;

	WCN_INFO("%s: chn=%d, num=%d\n", __func__, chn, *num);
	ret = sprdwcn_bus_list_alloc(chn, head, tail, num);

	return ret;
}

static ssize_t mdbg_snap_shoot_seq_write(struct file *file,
						const char __user *buffer,
						size_t count, loff_t *ppos)
{
	/* nothing to do */
	return count;
}

static void *mdbg_snap_shoot_seq_start(struct seq_file *m, loff_t *pos)
{
	u8 *pdata;
	u8 *buf;
	s32 ret = 0;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (!*(u32 *)pos) {
		buf = mdbg_proc->snap_shoot.buf;
		memset(buf, 0, MDBG_SNAP_SHOOT_SIZE);
		if (g_match_config && g_match_config->unisoc_wcn_integrated) {
			ret = mdbg_snap_shoot_iram(buf);
			if (ret < 0) {
				seq_puts(m, "==== IRAM DATA SNAP SHOOT FAIL ====\n");
				return NULL;
			}
			seq_puts(m, "==== IRAM DATA SNAP SHOOT START ====\n");
		} else {
			WCN_ERR("not support iram snap shoot! ret %d\n", ret);
			seq_puts(m, "==== IRAM DATA SNAP SHOOT NOT SUPPORT ====\n");
			return NULL;
		}
	}

	pdata = mdbg_proc->snap_shoot.buf + *(u32 *)pos * 16;
	(*(u32 *)pos)++;

	if (*(u32 *)pos > 2048) {
		seq_puts(m, "==== IRAM DATA SNAP SHOOT END    ====\n");
		return NULL;
	} else
		return pdata;
}

static void *mdbg_snap_shoot_seq_next(struct seq_file *m, void *p, loff_t *pos)

{
	return mdbg_snap_shoot_seq_start(m, pos);
}

static void mdbg_snap_shoot_seq_stop(struct seq_file *m, void *p)
{
	/* nothing to do */
}

static int mdbg_snap_shoot_seq_show(struct seq_file *m, void *p)
{
	u8 *pdata;
	u32 loop;

	if (p) {
		for (loop = 0; loop < 2; loop++) {
			pdata = p + 8*loop;
			seq_printf(m, "0x%02x%02x%02x%02x 0x%02x%02x%02x%02x ",
					pdata[3], pdata[2], pdata[1], pdata[0],
					pdata[7], pdata[6], pdata[5], pdata[4]);
		}
		seq_puts(m, "\n");
	}

	return 0;
}

static const struct seq_operations mdbg_snap_shoot_seq_ops = {
	.start = mdbg_snap_shoot_seq_start,
	.next = mdbg_snap_shoot_seq_next,
	.stop = mdbg_snap_shoot_seq_stop,
	.show = mdbg_snap_shoot_seq_show
};

static int mdbg_snap_shoot_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &mdbg_snap_shoot_seq_ops);
}

static const struct proc_ops mdbg_snap_shoot_seq_fops = {
	.proc_open = mdbg_snap_shoot_seq_open,
	.proc_read = seq_read,
	.proc_write = mdbg_snap_shoot_seq_write,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release
};

static int mdbg_proc_open(struct inode *inode, struct file *filp)
{
	struct mdbg_proc_entry *entry =
		(struct mdbg_proc_entry *)PDE_DATA(inode);
	filp->private_data = entry;

	return 0;
}

static int mdbg_proc_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t mdbg_proc_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct mdbg_proc_entry *entry =
		(struct mdbg_proc_entry *)filp->private_data;
	char *type = entry->name;
	int timeout = -1;
	int len = 0;
	int ret;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (filp->f_flags & O_NONBLOCK)
		timeout = 0;

	if (strcmp(type, "assert") == 0) {
		if (timeout < 0) {
			while (1) {
				ret = wait_for_completion_timeout(
						&mdbg_proc->assert.completed,
						msecs_to_jiffies(1000));
				if (ret != -ERESTARTSYS)
					break;
			}
		}

		if (copy_to_user((void __user *)buf,
				mdbg_proc->assert.buf,
				min(count, (size_t)MDBG_ASSERT_SIZE)))
			WCN_ERR("Read assert info error\n");
		len = mdbg_proc->assert.rcv_len;
		mdbg_proc->assert.rcv_len = 0;
		memset(mdbg_proc->assert.buf, 0, MDBG_ASSERT_SIZE);
	}

	if (strcmp(type, "loopcheck") == 0) {
		if (unlikely(g_loopcheck_switch != 0)) {
			if (marlin_get_module_status() == 1) {
				WCN_INFO("fake loopcheck\n");
				if (copy_to_user((void __user *)buf,
							"loopcheck_ack", 13))
					WCN_ERR("fake loopcheck reply error\n");
				len = 13;
			} else {
				if (copy_to_user((void __user *)buf,
							"poweroff", 8))
					WCN_ERR("read loopcheck error\n");
				len = 8;
				WCN_INFO("loopcheck poweroff\n");
			}
			return len;
		}

		if (timeout < 0) {
			while (1) {
				ret = wait_for_completion_timeout(
						&mdbg_proc->loopcheck.completed,
						msecs_to_jiffies(1000));
				if (ret != -ERESTARTSYS)
					break;
			}
		}
		if (marlin_get_module_status() == 1) {
			/* tell wcnd str"loopcheck_ack" to start loopcheck */
			if (mdbg_proc->loopcheck_flag) {
				if (copy_to_user((void __user *)buf,
					"loopcheck_ack", 13))
					return -EFAULT;
				loopcheck_ready_clear();
				WCN_INFO("CP power on first time\n");
				len = 13;
			} else if (mdbg_rx_count_change()) {
			/* fix the error(ack slow),use rx count to verify CP */
				WCN_DBG("CP run well with rx_cnt change\n");
				if (copy_to_user((void __user *)buf,
							"loopcheck_ack", 13))
					return -EFAULT;
				len = 13;
			} else {
				if (copy_to_user((void __user *)buf,
					mdbg_proc->loopcheck.buf, min(count,
						(size_t)MDBG_LOOPCHECK_SIZE)))
					return -EFAULT;
				len = mdbg_proc->loopcheck.rcv_len;
				if (strncmp(mdbg_proc->loopcheck.buf,
					"loopcheck_ack", 13) != 0)
					mdbg_proc->fail_count++;
				WCN_INFO("loopcheck status:%d\n",
					mdbg_proc->fail_count);
			}

			if (g_match_config && g_match_config->unisoc_wcn_pcie) {
				pcie_dev = get_wcn_device_info();
				if (!(atomic_dec_and_test(&pcie_dev->xmit_cnt)))
					atomic_set(&pcie_dev->xmit_cnt, 0x0);
			}
		} else {
			if (copy_to_user((void __user *)buf, "poweroff", 8))
				return -EFAULT;
			len = 8;
			WCN_INFO("mdbg loopcheck poweroff\n");
		}
		memset(mdbg_proc->loopcheck.buf, 0, MDBG_LOOPCHECK_SIZE);
		mdbg_proc->loopcheck.rcv_len = 0;
	}

	if (strcmp(type, "at_cmd") == 0) {
		if (timeout < 0) {
			while (1) {
				ret = wait_for_completion_timeout(
						&mdbg_proc->at_cmd.completed,
						msecs_to_jiffies(1000));
				if (ret != -ERESTARTSYS)
					break;
			}
		}

		if (copy_to_user((void __user *)buf,
					mdbg_proc->at_cmd.buf,
					min(count, (size_t)MDBG_AT_CMD_SIZE)))
			WCN_ERR("Read at cmd ack info error\n");

		len = mdbg_proc->at_cmd.rcv_len;
		mdbg_proc->at_cmd.rcv_len = 0;
		memset(mdbg_proc->at_cmd.buf, 0, MDBG_AT_CMD_SIZE);
	}
	return len;
}
/**************************************************
 * marlin2 crash
 *   |-user:       rebootmarlin
 *   `-userdebug : dumpmem for btwifi
 *
 * GNSS2 crash
 *   |-user:       rebootwcn
 *   `-userdebug : no action(libgps and gnss dbg will do)
 *
 * marlin3 crash
 *   |-user:       rebootmarlin
 *   `-userdebug : dumpmem for btwifi
 *
 * GNSS3 crash
 *   |-user:       rebootwcn
 *   `-userdebug : dumpmem for gnss
 *
 *  rebootmarlin: reset gpio enable
 *  rebootwcn :   chip_en gpio and reset gpio enable
 *****************************************************/
static ssize_t mdbg_proc_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct mbuf_t *head = NULL, *tail = NULL, *mbuf = NULL;
	int num = 1;
	static struct dma_buf at_dm;
	int ret = 0;
	static int at_buf_flag;
	char x;
#ifdef MDBG_PROC_CMD_DEBUG
	char *tempbuf = NULL;
	int ret = -1, i;
#endif
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (count < 1)
		return -EINVAL;
	if (copy_from_user(&x, buf, 1))
		return -EFAULT;
#ifdef MDBG_PROC_CMD_DEBUG
/* for test boot */

	if (g_match_config && g_match_config->unisoc_wcn_integrated) {
		if (x == '0')
			dump_arm_reg_integ();
	} else {
		if (x == '0')
			dump_arm_reg();
	}

	if (x == 'B') {
		WCN_INFO("wsh proc write =%c\n", x);
		tempbuf = kzalloc(10, GFP_KERNEL);
		memset(tempbuf, 0, 10);
		ret = sprdwcn_bus_direct_read(get_cp_start_addr(), tempbuf, 10);
		if (ret < 0)
			WCN_ERR("wsh debug get_cp_start_addr error:%d\n", ret);
		WCN_INFO("\nwsh debug get_cp_start_addr(10) :\n");
		for (i = 0; i < 10; i++)
			WCN_INFO("0x%x\n", tempbuf[i]);

		memset(tempbuf, 0, 10);
		ret = sprdwcn_bus_reg_read(get_cp_reset_reg(), tempbuf, 4);
		if (ret < 0)
			WCN_ERR("wsh debug get_cp_reset_reg error:%d\n", ret);
		WCN_INFO("\nwsh debug get_cp_reset_reg(4) :\n");
		for (i = 0; i < 4; i++)
			WCN_INFO(":0x%x\n", tempbuf[i]);

		memset(tempbuf, 0, 10);
		ret = sprdwcn_bus_direct_read(get_gnss_cp_start_addr(), tempbuf, 10);
		if (ret < 0)
			WCN_ERR("wsh debug get_gnss_cp_start_addr error:%d\n", ret);
		WCN_INFO("\nwsh debug get_gnss_cp_start_addr(10) :\n");
		for (i = 0; i < 10; i++)
			WCN_INFO(":0x%x\n", tempbuf[i]);

		memset(tempbuf, 0, 10);
		ret = sprdwcn_bus_reg_read(get_gnss_cp_reset_reg(), tempbuf, 4);
		if (ret < 0)
			WCN_ERR("wsh debug get_gnss_cp_reset_reg error:%d\n", ret);
		WCN_INFO("\nwsh debug get_gnss_cp_reset_reg(4) :\n");
		for (i = 0; i < 4; i++)
			WCN_INFO(":0x%x\n", tempbuf[i]);

		kfree(tempbuf);
	}

#ifdef MDBG_PROC_CMD_DEBUG
/* for test cdev */
	if (x == '1') {
		WCN_INFO("wsh proc write =%c\n", x);
		WCN_INFO("start char release test\n");
		log_cdev_exit();
	}
	if (x == '2') {
		WCN_INFO("wsh proc write =%c\n", x);
		WCN_INFO("start char register test\n");
		log_cdev_init();
	}
#endif
/* for test power on/off frequently */
#ifdef MDBG_PROC_CMD_DEBUG
	if (x == '0')
		start_marlin(MARLIN_BLUETOOTH);
	if (x == '1')
		start_marlin(MARLIN_FM);
	if (x == '2')
		start_marlin(MARLIN_WIFI);
	if (x == '3')
		start_marlin(MARLIN_MDBG);
	if (x == '4')
		start_marlin(MARLIN_GNSS);

	if (x == '5')
		stop_marlin(MARLIN_BLUETOOTH);
	if (x == '6')
		stop_marlin(MARLIN_FM);
	if (x == '7')
		stop_marlin(MARLIN_WIFI);
	if (x == '8')
		stop_marlin(MARLIN_MDBG);
	if (x == '9')
		stop_marlin(MARLIN_GNSS);
	if (x == 'x')
		open_power_ctl();
#endif
	if (x == 'Z')
		slp_mgr_drv_sleep(MARLIN_GNSS, FALSE);
	if (x == 'Y')
		slp_mgr_wakeup(MARLIN_GNSS);
	if (x == 'W')
		slp_mgr_drv_sleep(MARLIN_GNSS, TRUE);
	if (x == 'V')
		inform_cp_wifi_download();
	if (x == 'U')
		sprdwcn_bus_aon_writeb(0X1B0, 0X10);
	if (x == 'T')
		mem_pd_mgr(MARLIN_WIFI, 0X1);
	if (x == 'Q')
		mem_pd_save_bin();
	if (x == 'N')
		start_marlin(MARLIN_WIFI);
	if (x == 'R')
		stop_marlin(MARLIN_WIFI);

#endif
	if (count > MDBG_WRITE_SIZE) {
		WCN_ERR("%s count > MDBG_WRITE_SIZE\n", __func__);
		return -ENOMEM;
	}
	memset(mdbg_proc->write_buf, 0, MDBG_WRITE_SIZE);

	if (copy_from_user(mdbg_proc->write_buf, buf, count))
		return -EFAULT;

	WCN_INFO("mdbg_proc->write_buf:%s\n", mdbg_proc->write_buf);

	if (strncmp(mdbg_proc->write_buf, "startwcn", 8) == 0) {
		if (start_marlin(MARLIN_MDBG)) {
			WCN_ERR("%s power on failed\n", __func__);
			return -EIO;
		}
		return count;
	}

	if (strncmp(mdbg_proc->write_buf, "stopwcn", 7) == 0) {
		if (stop_marlin(MARLIN_MDBG)) {
			WCN_ERR("%s power off failed\n", __func__);
			return -EIO;
		}
		return count;
	}
	if (strncmp(mdbg_proc->write_buf, "startgnss", 9) == 0) {
		if (start_marlin(MARLIN_GNSS)) {
			WCN_ERR("%s power on failed\n", __func__);
			return -EIO;
		}
		return count;
	}

	if (strncmp(mdbg_proc->write_buf, "stopgnss", 8) == 0) {
		if (stop_marlin(MARLIN_GNSS)) {
			WCN_ERR("%s power off failed\n", __func__);
			return -EIO;
		}
		return count;
	}

	if (strncmp(mdbg_proc->write_buf, "disabledumpmem",
		strlen("disabledumpmem")) == 0) {
		g_dumpmem_switch = 0;
		WCN_INFO("hold mdbg dumpmem function:switch(%d)\n",
				g_dumpmem_switch);
		return count;
	}
	if (strncmp(mdbg_proc->write_buf, "enabledumpmem",
		strlen("enabledumpmem")) == 0) {
		g_dumpmem_switch = 1;
		WCN_INFO("release mdbg dumpmem function:switch(%d)\n",
				g_dumpmem_switch);
		return count;
	}
	if (strncmp(mdbg_proc->write_buf, "loopcheckoff",
		strlen("loopcheckoff")) == 0) {
		stop_loopcheck();
		g_loopcheck_switch = 1;
		WCN_INFO("loopcheck debug:switch(%d)\n",
				g_loopcheck_switch);
		return count;
	}
	if (strncmp(mdbg_proc->write_buf, "loopcheckon",
		strlen("loopcheckon")) == 0) {
		g_loopcheck_switch = 0;
		start_loopcheck();
		WCN_INFO("loopcheck debug:switch(%d)\n",
				g_loopcheck_switch);
		return count;
	}

	if (g_match_config && g_match_config->unisoc_wcn_integrated) {
		if (strncmp(mdbg_proc->write_buf, "dumpmem", 7) == 0) {
			mutex_lock(&mdbg_proc->mutex);
			if (g_dumpmem_switch == 0) {
				mutex_unlock(&mdbg_proc->mutex);
				return count;
			}
			WCN_INFO("start mdbg dumpmem");
			sprdwcn_bus_set_carddump_status(true);
			mdbg_dump_mem_integ(WCN_SOURCE_BTWF);
			mutex_unlock(&mdbg_proc->mutex);
			return count;
		}
		if (strncmp(mdbg_proc->write_buf, "holdcp2cpu",
			strlen("holdcp2cpu")) == 0) {
			mdbg_hold_cpu();
			WCN_INFO("hold cp cpu\n");
			return count;
		}
		if (strncmp(mdbg_proc->write_buf, "rebootwcn", 9) == 0 ||
			strncmp(mdbg_proc->write_buf, "rebootmarlin", 12) == 0) {
			WCN_INFO("marlin gnss need reset\n");
			WCN_INFO("fail_count is value %d\n", mdbg_proc->fail_count);
			mdbg_proc->fail_count = 0;
			sprdwcn_bus_set_carddump_status(false);
			wcn_device_poweroff();
			WCN_INFO("marlin gnss  reset finish!\n");
			return count;
		}
	} else {
		if (strncmp(mdbg_proc->write_buf, "dumpmem", 7) == 0) {
			sprdwcn_bus_set_carddump_status(true);

			mutex_lock(&mdbg_proc->mutex);
			marlin_set_sleep(MARLIN_MDBG, FALSE);
			marlin_set_wakeup(MARLIN_MDBG);
			mdbg_dump_mem();
			marlin_set_sleep(MARLIN_MDBG, TRUE);
			mutex_unlock(&mdbg_proc->mutex);
			return count;
		}

		if (strncmp(mdbg_proc->write_buf, "poweroff_wcn", 12) == 0) {
			marlin_power_off(MARLIN_ALL);
			return count;
		}

		if (strncmp(mdbg_proc->write_buf, "rebootmarlin", 12) == 0) {
			flag_reset = 1;
			WCN_INFO("marlin need reset\n");
			WCN_INFO("fail_count is value %d\n", mdbg_proc->fail_count);
			WCN_INFO("fail_reset is value %d\n", flag_reset);
			mdbg_proc->fail_count = 0;
			marlin_set_download_status(0);
			sprdwcn_bus_set_carddump_status(false);
			marlin_chip_en(false, true);
			if (marlin_reset_func != NULL)
				marlin_reset_func(marlin_callback_para);
			return count;
		}
		if (strncmp(mdbg_proc->write_buf, "rebootwcn", 9) == 0) {
			flag_reset = 1;
			WCN_INFO("marlin gnss need reset\n");
			WCN_INFO("fail_count is value %d\n", mdbg_proc->fail_count);
			mdbg_proc->fail_count = 0;
			marlin_set_download_status(0);
			sprdwcn_bus_set_carddump_status(false);
			marlin_chip_en(false, true);
			wcn_reset_cp2();
			return count;
		}
		if (strncmp(mdbg_proc->write_buf, "at+getchipversion", 17) == 0) {
			struct device_node *np_marlin2 = NULL;

			WCN_INFO("marlin get chip version\n");
			np_marlin2 = of_find_node_by_name(NULL, "sprd-marlin2");
			if (np_marlin2) {
				if (of_get_property(np_marlin2,
					"common_chip_en", NULL)) {
					WCN_INFO("marlin common_chip_en\n");
					memcpy(mdbg_proc->at_cmd.buf,
						"2342B", strlen("2342B"));
					mdbg_proc->at_cmd.rcv_len = strlen("2342B");
				}
			}
			return count;
		}
	}

	/*
	 * One AP Code used for many different CP2.
	 * But some CP2 already producted, it can't
	 * change code any more, so use the macro
	 * to disable SharkLE-Marlin2/SharkL3-Marlin2
	 * Pike2-Marlin2.
	 */
	if (g_match_config && (g_match_config->unisoc_wcn_integrated == false)) {
		/* loopcheck add kernel time ms/1000 */
		if (strncmp(mdbg_proc->write_buf, "at+loopcheck", 12) == 0) {
			/* struct timespec now; */
			unsigned long long loopcheck_tx_ns = local_clock();
			unsigned long long marlin_boot_t = marlin_bootup_time_get();

			MARLIN_64B_NS_TO_32B_MS(loopcheck_tx_ns);
			MARLIN_64B_NS_TO_32B_MS(marlin_boot_t);

			sprintf(mdbg_proc->write_buf, "at+loopcheck=%llu,%llu\r",
				loopcheck_tx_ns, marlin_boot_t);
			/* Be care the count value changed here before send to CP2 */
			count = strlen(mdbg_proc->write_buf);
			WCN_INFO("%s, count = %d", mdbg_proc->write_buf, (int)count);
		}
	}

	if (g_match_config && g_match_config->unisoc_wcn_pcie) {
		pcie_dev = get_wcn_device_info();
		if (!pcie_dev) {
			WCN_ERR("%s:PCIE device link error\n", __func__);
			return -1;
		}
		/* make sure don't send at cmd to pcie when chip has power off */
		if ((strncmp(mdbg_proc->write_buf, "at+loopcheck", 12) == 0)) {
			if (atomic_inc_return(&pcie_dev->xmit_cnt) >=
				BUS_REMOVE_CARD_VAL) {
				atomic_dec(&pcie_dev->xmit_cnt);
				WCN_INFO("ignore AT, chip has powroff\n");
				return count;
			}
		}

		ret = sprdwcn_bus_list_alloc(0, &head, &tail, &num);
		if (ret || head == NULL || tail == NULL) {
			WCN_ERR("%s:%d mbuf_link_alloc fail\n", __func__, __LINE__);
			return -1;
		}

		if (at_buf_flag == 0) {
			ret = dmalloc(pcie_dev, &at_dm, MDBG_WRITE_SIZE);
			if (ret != 0)
				return -1;
			at_buf_flag = 1;
		}
		mbuf = head;
		mbuf->buf = (unsigned char *)(at_dm.vir);
		mbuf->phy = (unsigned long)(at_dm.phy);
		mbuf->len = at_dm.size;
		memset(mbuf->buf, 0x0, mbuf->len);
		memcpy(mbuf->buf, mdbg_proc->write_buf, count);
		mbuf->next = NULL;
		WCN_DBG("mbuf->buf:%s\n", mbuf->buf);

		ret = sprdwcn_bus_push_list(0, head, tail, num);
		if (ret)
			WCN_INFO("sprdwcn_bus_push_list error=%d\n", ret);
	} else {
		if (g_match_config && g_match_config->unisoc_wcn_integrated)
			mdbg_send_atcmd(mdbg_proc->write_buf, count, WCN_ATCMD_WCND);
		else
			mdbg_send(mdbg_proc->write_buf, count, MDBG_SUBTYPE_AT);
	}

	return count;
}

static unsigned int mdbg_proc_poll(struct file *filp, poll_table *wait)
{
	struct mdbg_proc_entry *entry =
		(struct mdbg_proc_entry *)filp->private_data;
	char *type = entry->name;
	unsigned int mask = 0;

	if (strcmp(type, "assert") == 0) {
		poll_wait(filp, &mdbg_proc->assert.rxwait, wait);
		if (mdbg_proc->assert.rcv_len > 0)
			mask |= POLLIN | POLLRDNORM;
	}

	if (strcmp(type, "loopcheck") == 0) {
		poll_wait(filp, &mdbg_proc->loopcheck.rxwait, wait);
		WCN_DBG("loopcheck:power_state_changed:%d\n",
			wcn_get_module_status_changed());
		if (wcn_get_module_status_changed()) {
			wcn_set_module_status_changed(false);
			mask |= POLLIN | POLLRDNORM;
		}
	}

	return mask;
}

static const struct proc_ops mdbg_proc_fops = {
	.proc_open		= mdbg_proc_open,
	.proc_release	= mdbg_proc_release,
	.proc_read		= mdbg_proc_read,
	.proc_write		= mdbg_proc_write,
	.proc_poll		= mdbg_proc_poll,
};

int mdbg_memory_alloc(void)
{
	mdbg_proc->assert.buf =  kzalloc(MDBG_ASSERT_SIZE, GFP_KERNEL);
	if (!mdbg_proc->assert.buf)
		return -ENOMEM;

	mdbg_proc->loopcheck.buf =  kzalloc(MDBG_LOOPCHECK_SIZE, GFP_KERNEL);
	if (!mdbg_proc->loopcheck.buf) {
		kfree(mdbg_proc->assert.buf);
		return -ENOMEM;
	}
	mdbg_proc->at_cmd.buf =  kzalloc(MDBG_AT_CMD_SIZE, GFP_KERNEL);
	if (!mdbg_proc->at_cmd.buf) {
		kfree(mdbg_proc->assert.buf);
		kfree(mdbg_proc->loopcheck.buf);
		return -ENOMEM;
	}

	mdbg_proc->snap_shoot.buf =  kzalloc(MDBG_SNAP_SHOOT_SIZE, GFP_KERNEL);
	if (!mdbg_proc->snap_shoot.buf) {
		kfree(mdbg_proc->assert.buf);
		kfree(mdbg_proc->loopcheck.buf);
		kfree(mdbg_proc->at_cmd.buf);
		return -ENOMEM;
	}

	return 0;
}

/*
 * TX: pop_link(tx_cb), tx_complete(all_node_finish_tx_cb)
 * Rx: pop_link(rx_cb), push_link(prepare free buf)
 */
struct mchn_ops_t mdbg_proc_ops_pcie[MDBG_ASSERT_RX_OPS + 1] = {
	{
		.channel = WCN_AT_TX,
		.inout = WCNBUS_TX,
		.pool_size = 5,
		.hif_type = 1,
		.cb_in_irq = 1,
		.pop_link = mdbg_tx_cb,
		.tx_complete = mdbg_tx_comptele_cb,
	},
	{
		.channel = WCN_LOOPCHECK_RX,
		.inout = WCNBUS_RX,
		.pool_size = 5,
		.hif_type = 1,
		.cb_in_irq = 1,
		.pop_link = mdbg_loopcheck_read,
		.push_link = loopcheck_prepare_buf,
	},
	{
		.channel = WCN_AT_RX,
		.inout = WCNBUS_RX,
		.pool_size = 5,
		.hif_type = 1,
		.cb_in_irq = 1,
		.pop_link = mdbg_at_cmd_read,
		.push_link = at_cmd_prepare_buf,
	},
	{
		.channel = WCN_ASSERT_RX,
		.inout = WCNBUS_RX,
		.pool_size = 5,
		.hif_type = 1,
		.cb_in_irq = 1,
		.pop_link = mdbg_assert_read,
		.push_link = assert_prepare_buf,
	},
};

struct mchn_ops_t mdbg_proc_ops_sipc[MDBG_ASSERT_RX_OPS + 1] = {
	{
		.channel = WCN_SIPC_AT_TX,
		.inout = WCNBUS_TX,
		.pool_size = 5,
		.pop_link = mdbg_tx_cb,
		.chn_config.sipc_ch = WCN_INIT_SIPC_SBUF(
				WCN_SIPC_DST, SIPC_CHN_ATCMD, 0,
				"sbuf_atcmd_tx", 5, 128, 0,
				0x2400, 0x2400)
	},
	{
		.channel = WCN_SIPC_LOOPCHECK_RX,
		.inout = WCNBUS_RX,
		.pool_size = 1,
		.pop_link = mdbg_loopcheck_read,
		.chn_config.sipc_ch = WCN_INIT_SIPC_SBUF(
				WCN_SIPC_DST, SIPC_CHN_LOOPCHECK,
				WCN_CHN_CREATE | WCN_CHN_CALLBACK,
				"sbuf_loopcheck", 0, 128, 1,
				0x400, 0x400),
	},
	{
		.channel = WCN_SIPC_AT_RX,
		.inout = WCNBUS_RX,
		.pool_size = 1,
		.pop_link = mdbg_at_cmd_read,
		.chn_config.sipc_ch = WCN_INIT_SIPC_SBUF(
				WCN_SIPC_DST, SIPC_CHN_ATCMD, WCN_CHN_CALLBACK,
				"sbuf_atcmd_rx", 5, 128, 0,
				0x2400, 0x2400),
	},
	{
		.channel = WCN_SIPC_ASSERT_RX,
		.inout = WCNBUS_RX,
		.pool_size = 1,
		.pop_link = mdbg_assert_read,
		.chn_config.sipc_ch = WCN_INIT_SIPC_SBUF(
				WCN_SIPC_DST, SIPC_CHN_ASSERT,
				WCN_CHN_CREATE | WCN_CHN_CALLBACK,
				"sbuf_assert", 0, 1024, 1,
				0x400, 0x400),
	},
};

struct mchn_ops_t mdbg_proc_ops[MDBG_ASSERT_RX_OPS + 1] = {
	{
		.channel = WCN_AT_TX,
		.inout = WCNBUS_TX,
		.pool_size = 5,
		.pop_link = mdbg_tx_cb,
	},
	{
		.channel = WCN_LOOPCHECK_RX,
		.inout = WCNBUS_RX,
		.pool_size = 1,
		.pop_link = mdbg_loopcheck_read,
	},
	{
		.channel = WCN_AT_RX,
		.inout = WCNBUS_RX,
		.pool_size = 1,
		.pop_link = mdbg_at_cmd_read,
	},
	{
		.channel = WCN_ASSERT_RX,
		.inout = WCNBUS_RX,
		.pool_size = 1,
		.pop_link = mdbg_assert_read,
	},
};

struct mchn_ops_t *get_mdbg_proc_op(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_pcie)
		return mdbg_proc_ops_pcie;
	else if (g_match_config && g_match_config->unisoc_wcn_sipc)
		return mdbg_proc_ops_sipc;
	else
		return mdbg_proc_ops;
}

static struct dma_buf at_buf[3];
void mdbg_fs_channel_destroy(void)
{
	int i;
	struct mchn_ops_t *p_mdbg_proc_ops = get_mdbg_proc_op();
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	for (i = 0; i <= MDBG_ASSERT_RX_OPS; i++)
		sprdwcn_bus_chn_deinit(p_mdbg_proc_ops + i);

	if (g_match_config && g_match_config->unisoc_wcn_pcie) {
		free_prepare_buf(&at_buf[0]);
		free_prepare_buf(&at_buf[1]);
		free_prepare_buf(&at_buf[2]);
	}
}

void mdbg_fs_channel_init(void)
{
	int i;
	struct mchn_ops_t *p_mdbg_proc_ops = get_mdbg_proc_op();
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	for (i = 0; i <= MDBG_ASSERT_RX_OPS; i++)
		sprdwcn_bus_chn_init(p_mdbg_proc_ops + i);

	if (g_match_config && g_match_config->unisoc_wcn_pcie) {
		/* PCIe: malloc for rx buf */
		prepare_free_buf(&at_buf[0], 12, 256, 1);
		prepare_free_buf(&at_buf[1], 13, 256, 1);
		prepare_free_buf(&at_buf[2], 14, 256, 1);
	}
}

static  void mdbg_memory_free(void)
{
	kfree(mdbg_proc->snap_shoot.buf);
	mdbg_proc->snap_shoot.buf = NULL;

	kfree(mdbg_proc->assert.buf);
	mdbg_proc->assert.buf = NULL;

	kfree(mdbg_proc->loopcheck.buf);
	mdbg_proc->loopcheck.buf = NULL;

	kfree(mdbg_proc->at_cmd.buf);
	mdbg_proc->at_cmd.buf = NULL;
}

int proc_fs_init(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	mdbg_proc = kzalloc(sizeof(struct mdbg_proc_t), GFP_KERNEL);
	if (!mdbg_proc)
		return -ENOMEM;

	mdbg_proc->dir_name = "mdbg";
	mdbg_proc->procdir = proc_mkdir(mdbg_proc->dir_name, NULL);

	mdbg_proc->assert.name = "assert";
	mdbg_proc->assert.entry = proc_create_data(mdbg_proc->assert.name,
						0600,
						mdbg_proc->procdir,
						&mdbg_proc_fops,
						&(mdbg_proc->assert));

	mdbg_proc->loopcheck.name = "loopcheck";
	mdbg_proc->loopcheck.entry = proc_create_data(mdbg_proc->loopcheck.name,
						0600,
						mdbg_proc->procdir,
						&mdbg_proc_fops,
						&(mdbg_proc->loopcheck));

	mdbg_proc->at_cmd.name = "at_cmd";
	mdbg_proc->at_cmd.entry = proc_create_data(mdbg_proc->at_cmd.name,
						0600,
						mdbg_proc->procdir,
						&mdbg_proc_fops,
						&(mdbg_proc->at_cmd));

	mdbg_proc->snap_shoot.name = "snap_shoot";
	mdbg_proc->snap_shoot.entry = proc_create_data(
						mdbg_proc->snap_shoot.name,
						0600,
						mdbg_proc->procdir,
						&mdbg_snap_shoot_seq_fops,
						&(mdbg_proc->snap_shoot));

	if (g_match_config && !g_match_config->unisoc_wcn_pcie)
		mdbg_fs_channel_init();

	init_completion(&mdbg_proc->assert.completed);
	init_completion(&mdbg_proc->loopcheck.completed);
	init_completion(&mdbg_proc->at_cmd.completed);
	init_waitqueue_head(&mdbg_proc->assert.rxwait);
	init_waitqueue_head(&mdbg_proc->loopcheck.rxwait);
	mutex_init(&mdbg_proc->mutex);
	INIT_WORK(&mdbg_proc->async_assert.assert_wq, wcn_assert_wq);

	if (mdbg_memory_alloc() < 0) {
		kfree(mdbg_proc);
		return -ENOMEM;
	}

	return 0;
}

void proc_fs_exit(void)
{
	mdbg_memory_free();
	mutex_destroy(&mdbg_proc->mutex);
	mdbg_fs_channel_destroy();
	remove_proc_entry(mdbg_proc->snap_shoot.name, mdbg_proc->procdir);
	remove_proc_entry(mdbg_proc->assert.name, mdbg_proc->procdir);
	remove_proc_entry(mdbg_proc->loopcheck.name, mdbg_proc->procdir);
	remove_proc_entry(mdbg_proc->at_cmd.name, mdbg_proc->procdir);
	remove_proc_entry(mdbg_proc->dir_name, NULL);

	kfree(mdbg_proc);
	mdbg_proc = NULL;
}

int get_loopcheck_status(void)
{
	return mdbg_proc->fail_count;
}

void wakeup_loopcheck_int(void)
{
	wake_up_interruptible(&mdbg_proc->loopcheck.rxwait);
}

void loopcheck_ready_clear(void)
{
	mdbg_proc->loopcheck_flag = false;
}

void loopcheck_ready_set(void)
{
	mdbg_proc->loopcheck_flag = true;
}
