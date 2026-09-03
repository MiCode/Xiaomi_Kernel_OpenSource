/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : slp_mgr.c
 * Abstract : This file is a implementation for  sleep manager
 *
 * Authors	: sam.sun
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <misc/wcn_bus.h>
#include "../sdio/sdiohal.h"
#include "slp_mgr.h"
#include "slp_sdio.h"
#include "wcn_glb.h"
#include "slp_dbg.h"

static struct slp_mgr_t slp_mgr;

struct slp_mgr_t *slp_get_info(void)
{
	return &slp_mgr;
}

void slp_mgr_drv_sleep(enum slp_subsys subsys, bool enable)
{
	mutex_lock(&(slp_mgr.drv_slp_lock));
	if (enable)
		slp_mgr.active_module &= ~(BIT(subsys));
	else
		slp_mgr.active_module |= (BIT(subsys));
	if ((slp_mgr.active_module == 0) &&
		(subsys > PACKER_DT_RX)) {
		slp_allow_sleep();
		atomic_set(&(slp_mgr.cp2_state), STAY_SLPING);
	}
	mutex_unlock(&(slp_mgr.drv_slp_lock));
}

int slp_mgr_wakeup(enum slp_subsys subsys)
{
	unsigned char slp_sts = 0;
	int ret;
	int do_dump = 0;
	ktime_t time_end;
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (STAY_DEATH == (atomic_read(&slp_mgr.cp2_state))) {
		WCN_ERR("CP2 has been shutdown, ignoring this wakeup\n");
		return -1;
	}

	mutex_lock(&(slp_mgr.wakeup_lock));
	if (STAY_SLPING == (atomic_read(&(slp_mgr.cp2_state)))) {
		ap_wakeup_cp();
		time_end = ktime_add_ms(ktime_get(), 5);
		while (1) {
			ret = sprdwcn_bus_aon_readb(get_btwf_slp_sts_reg(), &slp_sts);
			if (ret < 0) {
				WCN_ERR("read slp sts err:%d\n", ret);
				usleep_range_state(40, 80, TASK_UNINTERRUPTIBLE);
				goto try_timeout;
			}
			slp_sts &= 0xF0;

			if (g_match_config && g_match_config->unisoc_wcn_m3lite) {
				if ((slp_sts != BTWF_IN_DEEPSLEEP) &&
				   (slp_sts != M3L_BTWF_PLL_PWR_WAIT) &&
				   (slp_sts != M3L_BTWF_XLT_WAIT) &&
				   (slp_sts != M3L_BTWF_XLTBUF_WAIT) &&
				   (slp_sts != get_btwf_in_deepslp_xtl_on()))
					break;
			} else {
				if ((slp_sts != BTWF_IN_DEEPSLEEP) &&
				   (slp_sts != get_btwf_in_deepslp_xtl_on()))
					break;
			}

try_timeout:
			if (do_dump) {
				atomic_set(&(slp_mgr.cp2_state), STAY_AWAKING);
				WCN_INFO("wakeup fail, slp_sts-0x%x\n",
					 slp_sts);
				sdiohal_dump_aon_reg();
				mutex_unlock(&(slp_mgr.wakeup_lock));
				return -1;
			}
			if (ktime_after(ktime_get(), time_end))
				do_dump = 1;
		}

		atomic_set(&(slp_mgr.cp2_state), STAY_AWAKING);
	}

	mutex_unlock(&(slp_mgr.wakeup_lock));

	return 0;
}

/* called after chip power on, and reset sleep status */
void slp_mgr_reset(void)
{
	atomic_set(&(slp_mgr.cp2_state), STAY_AWAKING);
	reinit_completion(&(slp_mgr.wakeup_ack_completion));
}

int slp_mgr_init(void)
{
	WCN_INFO("%s enter\n", __func__);

	atomic_set(&(slp_mgr.cp2_state), STAY_AWAKING);
	mutex_init(&(slp_mgr.drv_slp_lock));
	mutex_init(&(slp_mgr.wakeup_lock));
	init_completion(&(slp_mgr.wakeup_ack_completion));
	slp_pub_int_regcb();
#ifdef SLP_MGR_TEST
	slp_test_init();
#endif

	WCN_INFO("%s ok!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(slp_mgr_init);

int slp_mgr_deinit(void)
{
	WCN_INFO("%s enter\n", __func__);
	atomic_set(&(slp_mgr.cp2_state), STAY_SLPING);
	slp_mgr.active_module = 0;
	mutex_destroy(&(slp_mgr.drv_slp_lock));
	mutex_destroy(&(slp_mgr.wakeup_lock));
	WCN_INFO("%s ok!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(slp_mgr_deinit);

int slp_mgr_death(void)
{
	atomic_set(&(slp_mgr.cp2_state), STAY_DEATH);
	return 0;
}
EXPORT_SYMBOL(slp_mgr_death);
