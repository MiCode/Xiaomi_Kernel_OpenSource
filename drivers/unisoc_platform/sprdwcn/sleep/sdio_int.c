// SPDX-License-Identifier: GPL-2.0-only
/*
 * sdio_int.c - Unisoc platform driver
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

#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <misc/marlin_platform.h>
#include <linux/module.h>
#include <linux/spinlock.h>

#include <misc/wcn_bus.h>

#include "sdio_int.h"
#include "slp_mgr.h"
#include "slp_sdio.h"
#include "wcn_glb.h"
#include "../sdio/sdiohal.h"
#include "slp_dbg.h"

struct sdio_int_t sdio_int = {0};

static atomic_t flag_pub_int_done;
static bool sdio_power_notify;

static inline int sdio_pub_int_clr0(unsigned char int_sts0)
{
	return sprdwcn_bus_aon_writeb(sdio_int.pub_int_clr0,
			int_sts0);
}

bool sdio_get_power_notify(void)
{
	return sdio_power_notify;
}

void sdio_record_power_notify(bool notify_cb_sts)
{
	sdio_power_notify = notify_cb_sts;
}

void sdio_wait_pub_int_done(void)
{
	struct slp_mgr_t *slp_mgr;
	long ret = 0;

	slp_mgr = slp_get_info();

	if (sdio_power_notify) {
		/* enter suspend, means no tx data to cp2, so set sleep */
		mutex_lock(&(slp_mgr->drv_slp_lock));
		if (atomic_read(&(slp_mgr->cp2_state)) == STAY_AWAKING) {
			WCN_INFO("allow sleep1\n");
			slp_allow_sleep();
			atomic_set(&(slp_mgr->cp2_state), STAY_SLPING);
		}
		mutex_unlock(&(slp_mgr->drv_slp_lock));

		/* wait pub_int handle finish */
		if (unlikely(atomic_read(&flag_pub_int_done) == 0))
			WCN_INFO("wait pub_int_done\n");
		ret = wait_event_killable_timeout(sdio_int.pub_int_done,
			atomic_read(&flag_pub_int_done), usecs_to_jiffies(3000 * 10));

		WCN_INFO("flag_pub_int_done(%s)-%d\n", ret == 0 ? "timeout" : "success",
			atomic_read(&flag_pub_int_done));
	} else
		WCN_INFO("sdio power_notify is NULL\n");
}
EXPORT_SYMBOL(sdio_wait_pub_int_done);

static int pub_int_handle_thread(void *data)
{
	union PUB_INT_STS0_REG pub_int_sts0 = {0};
	int bit_num, ret;

	set_user_nice(current, -20);
	while (!kthread_should_stop()) {
		wait_for_completion(&(sdio_int.pub_int_completion));

		ret = sprdwcn_bus_aon_readb(sdio_int.pub_int_sts0,
			&(pub_int_sts0.reg));
		/* sdio cmd52 fail, it should be chip power off */
		if (ret < 0)
			WCN_INFO("sdio cmd52 fail, ret-%d\n", ret);
		else {
			WCN_INFO("PUB_INT_STS0-0x%x\n", pub_int_sts0.reg);
			sdio_pub_int_clr0(pub_int_sts0.reg);

		bit_num = 0;
		do {
			if ((pub_int_sts0.reg & BIT(bit_num)) &&
				sdio_int.pub_int_cb[bit_num]) {
				sdio_int.pub_int_cb[bit_num]();
			}
			bit_num++;
		} while (bit_num < PUB_INT_MAX);
		}

		if (sdio_power_notify) {
			atomic_set(&flag_pub_int_done, 1);
			wake_up_all(&sdio_int.pub_int_done);
		} else
			__pm_relax(sdio_int.pub_int_wakelock);

		enable_irq(sdio_int.pub_int_num);
	}

	return 0;
}

static int irq_cnt;
static irqreturn_t pub_int_isr(int irq, void *para)
{
	disable_irq_nosync(irq);
	/*
	 * for wifi powersave special handle, when wifi driver send
	 * power save cmd to cp2, then pub int can't call wakelock,
	 * or ap can't enter deep sleep.
	 */
	if (sdio_power_notify)
		atomic_set(&flag_pub_int_done, 0);
	else
		__pm_stay_awake(sdio_int.pub_int_wakelock);

	irq_cnt++;
	WCN_INFO("irq_cnt%d!!\n", irq_cnt);

	complete(&(sdio_int.pub_int_completion));

	return IRQ_HANDLED;
}

static struct task_struct *pub_int_handle_task;
static int sdio_isr_handle_init(void)
{
	if (!pub_int_handle_task)
		pub_int_handle_task = kthread_create(pub_int_handle_thread,
			NULL, "pub_int_handle_thread");
	if (pub_int_handle_task) {
		wake_up_process(pub_int_handle_task);
		return 0;
	}

	WCN_INFO("%s ok!\n", __func__);

	return -1;
}

static int sdio_pub_int_register(int irq)
{
	int ret;

	WCN_INFO("public_int, gpio-%d\n", irq);

	ret = gpio_direction_input(irq);
	if (ret < 0) {
		WCN_ERR("public_int, gpio-%d input set fail!!!\n", irq);
		return ret;
	}

	sdio_int.pub_int_num = gpio_to_irq(irq);

	ret = request_irq(sdio_int.pub_int_num,
			pub_int_isr,
			IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND,
			"pub_int_isr",
			NULL);
	if (ret != 0) {
		WCN_ERR("req irq-%d err!!!\n", sdio_int.pub_int_num);
		return ret;
	}

	/* enable interrupt when chip power on */
	disable_irq(sdio_int.pub_int_num);

	return ret;
}

int sdio_ap_int_cp0(enum AP_INT_CP_BIT bit)
{
	union AP_INT_CP0_REG reg_int_cp0 = {0};

	switch (bit) {
	case ALLOW_CP_SLP:
		reg_int_cp0.bit.allow_cp_slp = 1;
		break;
	case WIFI_BIN_DOWNLOAD:
		reg_int_cp0.bit.wifi_bin_download = 1;
		break;
	case BT_BIN_DOWNLOAD:
		reg_int_cp0.bit.bt_bin_download = 1;
		break;
	case SAVE_CP_MEM:
		reg_int_cp0.bit.save_cp_mem = 1;
		break;
	case TEST_DEL_THREAD:
		reg_int_cp0.bit.test_delet_thread = 1;
		break;
	default:
		WCN_INFO("ap_int_cp bit error\n");
		break;
	}

	return sprdwcn_bus_aon_writeb(sdio_int.ap_int_cp0,
			reg_int_cp0.reg);
}
EXPORT_SYMBOL(sdio_ap_int_cp0);

int sdio_pub_int_regcb(enum PUB_INT_BIT bit,
		PUB_INT_ISR isr_handler)
{
	if (isr_handler == NULL) {
		WCN_ERR("pub_int_RegCb error !!\n");
		return -1;
	}

	sdio_int.pub_int_cb[bit] = isr_handler;

	WCN_INFO("0X%x pub_int_RegCb\n", bit);

	return 0;
}
EXPORT_SYMBOL(sdio_pub_int_regcb);

int sdio_pub_int_btwf_en0(void)
{
	union PUB_INT_EN0_REG reg_int_en = {0};

	sprdwcn_bus_aon_readb(sdio_int.pub_int_en0, &(reg_int_en.reg));

	reg_int_en.bit.req_slp = 1;
	reg_int_en.bit.mem_save_bin = 1;
	reg_int_en.bit.wifi_open = 1;
	reg_int_en.bit.bt_open = 1;
	reg_int_en.bit.wifi_close = 1;
	reg_int_en.bit.bt_close = 1;
	sprdwcn_bus_aon_writeb(sdio_int.pub_int_en0, reg_int_en.reg);

	WCN_INFO("%s ok!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(sdio_pub_int_btwf_en0);

int sdio_pub_int_gnss_en0(void)
{
	union PUB_INT_EN0_REG reg_int_en = {0};

	sprdwcn_bus_aon_readb(sdio_int.pub_int_en0, &(reg_int_en.reg));

	reg_int_en.bit.gnss_cali_done = 1;

	sprdwcn_bus_aon_writeb(sdio_int.pub_int_en0, reg_int_en.reg);

	WCN_INFO("%s ok!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(sdio_pub_int_gnss_en0);

void sdio_pub_int_poweron(bool state)
{
	atomic_set(&(sdio_int.chip_power_on), state);

	if (state)
		enable_irq(sdio_int.pub_int_num);
	else
		disable_irq(sdio_int.pub_int_num);
}
EXPORT_SYMBOL(sdio_pub_int_poweron);

int sdio_pub_int_init(int irq)
{
	sdio_int.cp_slp_ctl = get_cp_slp_ctl_reg();
	sdio_int.ap_int_cp0 = REG_AP_INT_CP0;
	sdio_int.pub_int_en0 = REG_PUB_INT_EN0;
	sdio_int.pub_int_clr0 = REG_PUB_INT_CLR0;
	sdio_int.pub_int_sts0 = REG_PUB_INT_STS0;

	sdio_int.pub_int_wakelock =
		kmalloc(sizeof(struct wakeup_source), GFP_KERNEL);
	if (!(sdio_int.pub_int_wakelock))
		return -ENOMEM;

	atomic_set(&flag_pub_int_done, 1);
	sdio_int.pub_int_wakelock = wakeup_source_create("pub_int_wakelock");
	wakeup_source_add(sdio_int.pub_int_wakelock);
	init_completion(&(sdio_int.pub_int_completion));
	init_waitqueue_head(&(sdio_int.pub_int_done));

	sdio_pub_int_register(irq);

	sdio_isr_handle_init();

	WCN_INFO("%s ok!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(sdio_pub_int_init);

int sdio_pub_int_deinit(void)
{
	atomic_set(&flag_pub_int_done, 1);
	if (pub_int_handle_task) {
		disable_irq(sdio_int.pub_int_num);
		complete(&(sdio_int.pub_int_completion));
		kthread_stop(pub_int_handle_task);
		pub_int_handle_task = NULL;
	}

	sdio_power_notify = false;
	disable_irq(sdio_int.pub_int_num);
	free_irq(sdio_int.pub_int_num, NULL);
	wakeup_source_remove(sdio_int.pub_int_wakelock);
	wakeup_source_destroy(sdio_int.pub_int_wakelock);
	kfree(sdio_int.pub_int_wakelock);

	WCN_INFO("%s ok!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(sdio_pub_int_deinit);

