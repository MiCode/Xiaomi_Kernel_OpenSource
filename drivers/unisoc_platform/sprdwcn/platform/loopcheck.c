// SPDX-License-Identifier: GPL-2.0-only
/*
 * loopcheck.c - Unisoc platform driver
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/rtc.h>
#include <linux/timekeeping.h>

#include "wcn_glb.h"
#include "wcn_misc.h"
#include "wcn_procfs.h"
#include "pcie.h"

#define LOOPCHECK_TIMER_INTERVAL      5
#define WCN_LOOPCHECK_INIT	1
#define WCN_LOOPCHECK_OPEN	2
#define WCN_LOOPCHECK_FAIL	3
#define LOOPCHECK_START_WAIT	2

struct wcn_loopcheck {
	unsigned long status;
	struct completion completion;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
};

static struct wcn_loopcheck loopcheck;

#ifdef BUILD_WCN_PCIE
static int loopcheck_send_pcie(char *cmd, unsigned int len)
{
	struct mbuf_t *head = NULL;
	struct mbuf_t *tail = NULL;
	int num = 1;
	int ret;
	/* dma_buf for dma */
	static struct dma_buf dma_buf;
	static int at_buf_flag;
	struct wcn_pcie_info *pcie_dev;

	pcie_dev = get_wcn_device_info();
	if (!pcie_dev) {
		WCN_ERR("%s:PCIE device link error\n", __func__);
		return -1;
	}
	if (atomic_read(&pcie_dev->is_suspending)) {
		WCN_ERR("%s:PCIE dev is suspending\n", __func__);
		return -1;
	}
	if (pcie_dev->pci_status == WCN_BUS_DOWN) {
		WCN_ERR("%s:PCIE wcn bus down\n", __func__);
		return -1;
	}

	ret = sprdwcn_bus_list_alloc(0, &head, &tail, &num);
	if (ret || head == NULL || tail == NULL) {
		WCN_ERR("%s:%d mbuf_link_alloc fail\n", __func__, __LINE__);
		return -1;
	}

	if (at_buf_flag == 0) {
		ret = dmalloc(pcie_dev, &dma_buf, 128);
		if (ret != 0) {
			return -1;
		}
		at_buf_flag = 1;
	}
	head->buf = (unsigned char *)(dma_buf.vir);
	head->phy = (unsigned long)(dma_buf.phy);
	head->len = len;
	memset(head->buf, 0x0, dma_buf.size);
	memcpy(head->buf, cmd, len);
	head->next = NULL;
	ret = sprdwcn_bus_push_list(0, head, tail, num);
	if (ret)
		WCN_INFO("sprdwcn_bus_push_list error=%d\n", ret);

	WCN_INFO("tx:%s in %s\n", cmd, __func__);

	return len;
}
#endif

static int loopcheck_send(char *buf, unsigned int len)
{
	unsigned char *send_buf = NULL;
	struct mbuf_t *head = NULL;
	struct mbuf_t *tail = NULL;
	int ret = 0, num = 1;
	struct mchn_ops_t *p_mdbg_proc_ops = get_mdbg_proc_op() + MDBG_AT_TX_OPS;
	struct wcn_match_data *g_match_config = get_wcn_match_config();
	unsigned int pub_head_rsv;

	if (g_match_config && g_match_config->unisoc_wcn_pcie) {
#ifdef BUILD_WCN_PCIE
		ret = loopcheck_send_pcie(buf, len);
#endif
		return ret;
	}

	if (g_match_config && g_match_config->unisoc_wcn_sdio)
		pub_head_rsv = SDIOHAL_PUB_HEAD_RSV;
	else
		pub_head_rsv = PUB_HEAD_RSV;

	WCN_INFO("%s", __wcn_get_sw_ver());
	WCN_INFO("tx:%s\n", buf);
	if (unlikely(!marlin_get_module_status())) {
		WCN_ERR("WCN module have not open\n");
		return -EIO;
	}

	send_buf = kzalloc(len + pub_head_rsv + 1, GFP_KERNEL);
	if (!send_buf)
		return -ENOMEM;
	memcpy(send_buf + pub_head_rsv, buf, len);

	if (!sprdwcn_bus_list_alloc(p_mdbg_proc_ops->channel, &head, &tail, &num)) {
		head->buf = send_buf;
		head->len = len;
		head->next = NULL;

		if (g_match_config && g_match_config->unisoc_wcn_sdio) {
			ret = sprdwcn_bus_push_list_direct(p_mdbg_proc_ops->channel,
							   head, tail, num);
			if (p_mdbg_proc_ops->pop_link)
				p_mdbg_proc_ops->pop_link(p_mdbg_proc_ops->channel,
							  head, tail, num);
			else
				sprdwcn_bus_list_free(p_mdbg_proc_ops->channel,
						      head, tail, num);
		} else {
			ret = sprdwcn_bus_push_list(p_mdbg_proc_ops->channel,
						    head, tail, num);
		}

		if (ret != 0)
			WCN_ERR("loopcheck send fail!\n");
	} else {
		WCN_ERR("%s alloc buf fail!\n", __func__);
		kfree(send_buf);
		return -ENOMEM;
	}
	return ret;
}

static void loopcheck_work_queue(struct work_struct *work)
{
	int ret;
	char a[64];
	unsigned long timeleft;
	unsigned long long sprdwcn_rx_cnt_a = 0, sprdwcn_rx_cnt_b = 0;
	unsigned long long loopcheck_tx_ns, marlin_boot_t;
	struct timespec64 ts;
	struct rtc_time tm;
	static unsigned int loopcheck_cnt;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	loopcheck_tx_ns = local_clock();
	marlin_boot_t = marlin_bootup_time_get();
	MARLIN_64B_NS_TO_32B_MS(loopcheck_tx_ns);
	MARLIN_64B_NS_TO_32B_MS(marlin_boot_t);
	snprintf(a, (size_t)sizeof(a), "at+loopcheck=%llu,%llu\r\n",
		 loopcheck_tx_ns, marlin_boot_t);

	if ((unlikely(!marlin_get_module_status())) ||
	    (!test_bit(WCN_LOOPCHECK_OPEN, &loopcheck.status))) {
		WCN_ERR("WCN module have not open\n");
		return;
	}

	sprdwcn_rx_cnt_a = sprdwcn_bus_get_rx_total_cnt();
	usleep_range_state(4000, 6000, TASK_UNINTERRUPTIBLE);
	sprdwcn_rx_cnt_b = sprdwcn_bus_get_rx_total_cnt();

	if (sprdwcn_rx_cnt_a == sprdwcn_rx_cnt_b) {
		if (sprdwcn_bus_is_suspended(false)) {
			WCN_INFO("BUS suspended, pause loopcheck\n");
			ret = queue_delayed_work(loopcheck.workqueue, &loopcheck.work,
				 msecs_to_jiffies(1500));
			return;
		}
		wcn_send_atcmd_lock();
		ret = loopcheck_send(a, strlen(a));
		if ((g_match_config && g_match_config->unisoc_wcn_pcie) && (ret == -1)) {
			WCN_ERR("pcie is not ok, need to wait!\n");
			wcn_send_atcmd_unlock();
		} else {
			timeleft = wait_for_completion_timeout(&loopcheck.completion, (4 * HZ));
			wcn_send_atcmd_unlock();
			if (!test_bit(WCN_LOOPCHECK_OPEN, &loopcheck.status))
				return;
			if (loopcheck_cnt++ % 25 == 0) {
				ktime_get_real_ts64(&ts);
				ts.tv_sec -= sys_tz.tz_minuteswest * 60;
				rtc_time64_to_tm(ts.tv_sec, &tm);
				WCN_INFO("loopcheck(%u) %04d-%02d-%02d_%02d:%02d:%02d.%ld",
					loopcheck_cnt,
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
					tm.tm_min, tm.tm_sec, ts.tv_nsec);
			}
			if (!timeleft) {
				set_bit(WCN_LOOPCHECK_FAIL, &loopcheck.status);
				WCN_ERR("didn't get loopcheck ack, printk=%d\n", console_loglevel);
				mdbg_assert_interface("WCN loopcheck erro!");
				clear_bit(WCN_LOOPCHECK_FAIL, &loopcheck.status);
				return;
			}
		}
	}
	ret = queue_delayed_work(loopcheck.workqueue, &loopcheck.work,
				 LOOPCHECK_TIMER_INTERVAL * HZ);
}

void start_loopcheck(void)
{
	if (!test_bit(WCN_LOOPCHECK_INIT, &loopcheck.status) ||
	    test_and_set_bit(WCN_LOOPCHECK_OPEN, &loopcheck.status))
		return;
	WCN_INFO("%s\n", __func__);
	reinit_completion(&loopcheck.completion);
	queue_delayed_work(loopcheck.workqueue, &loopcheck.work,
				LOOPCHECK_START_WAIT * HZ);
}

void stop_loopcheck(void)
{
	if (!test_bit(WCN_LOOPCHECK_INIT, &loopcheck.status) ||
	    !test_and_clear_bit(WCN_LOOPCHECK_OPEN, &loopcheck.status) ||
	    test_bit(WCN_LOOPCHECK_FAIL, &loopcheck.status))
		return;
	WCN_INFO("%s\n", __func__);
	complete_all(&loopcheck.completion);
	cancel_delayed_work_sync(&loopcheck.work);
}

void complete_kernel_loopcheck(void)
{
	complete(&loopcheck.completion);
}

int loopcheck_status(void)
{
	return loopcheck.status;
}

int loopcheck_init(void)
{
	loopcheck.status = 0;
	init_completion(&loopcheck.completion);
	loopcheck.workqueue =
		create_singlethread_workqueue("WCN_LOOPCHECK_QUEUE");
	if (!loopcheck.workqueue) {
		WCN_ERR("WCN_LOOPCHECK_QUEUE create failed");
		return -ENOMEM;
	}
	set_bit(WCN_LOOPCHECK_INIT, &loopcheck.status);
	INIT_DELAYED_WORK(&loopcheck.work, loopcheck_work_queue);

	return 0;
}

int loopcheck_deinit(void)
{
	stop_loopcheck();
	destroy_workqueue(loopcheck.workqueue);
	loopcheck.status = 0;

	return 0;
}
