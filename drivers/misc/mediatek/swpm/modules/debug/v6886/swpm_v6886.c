// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/perf_event.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
#include <mtk_qos_ipi.h>
#endif

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#endif

#if IS_ENABLED(CONFIG_MTK_THERMAL)
#include <thermal_interface.h>
#endif

#define CREATE_TRACE_POINTS
#include <swpm_tracker_trace.h>
/* EXPORT_TRACEPOINT_SYMBOL(swpm_power); */
/* EXPORT_TRACEPOINT_SYMBOL(swpm_power_idx); */

#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_v6886.h>
#include <swpm_v6886_ext.h>

/****************************************************************************
 *  Global Variables
 ****************************************************************************/
struct power_rail_data swpm_power_rail[NR_POWER_RAIL] = {
	[VPROC2] = {0, "VPROC2"},
	[VPROC1] = {0, "VPROC1"},
	[VGPU] = {0, "VGPU"},
	[VCORE] = {0, "VCORE"},
	[VDRAM] = {0, "VDRAM"},
};
struct share_wrap *wrap_d;

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
static unsigned int swpm_init_state;
static void swpm_init_retry(struct work_struct *work);
static struct workqueue_struct *swpm_init_retry_work_queue;
DECLARE_WORK(swpm_init_retry_work, swpm_init_retry);

static unsigned int swpm_log_mask = DEFAULT_LOG_MASK;
static struct swpm_rec_data *swpm_info_ref;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
/* share dram for subsys related table communication */
static phys_addr_t rec_phys_addr, rec_virt_addr;
static unsigned long long rec_size;
#endif

/****************************************************************************
 *  Static Function
 ****************************************************************************/
static unsigned int swpm_get_avg_power(enum power_rail type)
{
	if (type >= NR_POWER_RAIL)
		pr_notice("Invalid SWPM type = %d\n", type);

	/* Remove the calculation of the power rail power and return 0 directly) */
	return 0;
}

static void swpm_send_enable_ipi(unsigned int type, unsigned int enable)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && \
	IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
	struct qos_ipi_data qos_d;

	qos_d.cmd = QOS_IPI_SWPM_ENABLE;
	qos_d.u.swpm_enable.type = type;
	qos_d.u.swpm_enable.enable = enable;
	qos_ipi_to_sspm_scmi_command(qos_d.cmd, type, enable, 0,
				     QOS_IPI_SCMI_SET);
#endif
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
static void swpm_send_init_ipi(unsigned int addr, unsigned int size,
			      unsigned int ch_num)
{
#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
	unsigned int offset;

	offset = swpm_set_and_get_cmd(addr, size, 0, SWPM_COMMON_INIT);

	if (offset == -1) {
		pr_notice("qos ipi not ready init fail\n");
		goto error;
	} else if (offset == 0) {
		pr_notice("swpm share sram init fail\n");
		goto error;
	} else {
		pr_notice("swpm init offset = 0x%x\n", offset);
	}

	/* get wrapped sram address */
	wrap_d = (struct share_wrap *)
		sspm_sbuf_get(offset);

	/* exception control for illegal sbuf request */
	if (!wrap_d) {
		pr_notice("swpm share sram offset fail\n");
		goto error;
	}

#if SWPM_TEST
	pr_notice("wrap_d = 0x%p\n", wrap_d);
#endif

	swpm_init_state = 1;

	if (swpm_init_state)
		return;

error:
#endif
	swpm_init_state = 0;
}

static inline void swpm_pass_to_sspm(void)
{
	swpm_send_init_ipi((unsigned int)(rec_phys_addr & 0xFFFFFFFF),
		(unsigned int)(rec_size & 0xFFFFFFFF), 2);
}

static void swpm_init_retry(struct work_struct *work)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	if (!swpm_init_state)
		swpm_pass_to_sspm();
#endif
}

static char pwr_buf[POWER_CHAR_SIZE] = { 0 };
#if SWPM_TEST
static char idx_buf[POWER_INDEX_CHAR_SIZE] = { 0 };
#endif
static void swpm_log_loop(struct timer_list *t)
{
	char *ptr = pwr_buf;
#if SWPM_TEST
	char *idx_ptr = idx_buf;
#endif
	int i;

	/* initialization retry */
	if (swpm_init_retry_work_queue && !swpm_init_state)
		queue_work(swpm_init_retry_work_queue, &swpm_init_retry_work);

	for (i = 0; i < NR_POWER_RAIL; i++) {
		if ((1 << i) & swpm_log_mask) {
			ptr += scnprintf(ptr, 256, "%s/",
					swpm_power_rail[i].name);
		}
	}
	ptr--;
	ptr += sprintf(ptr, " = ");

	for (i = 0; i < NR_POWER_RAIL; i++) {
		if ((1 << i) & swpm_log_mask) {
			swpm_power_rail[i].avg_power =
				swpm_get_avg_power((enum power_rail)i);
			ptr += scnprintf(ptr, 256, "%d/",
					swpm_power_rail[i].avg_power);
		}
	}
	ptr--;
	ptr += sprintf(ptr, " uA");

#if SWPM_TEST
	/* for LTR */
	if (share_idx_ref && share_idx_ctrl) {
		memset(idx_buf, 0, sizeof(char) * POWER_INDEX_CHAR_SIZE);
		/* exclude window_cnt */
		for (i = 0; i < idx_output_size; i++) {
			idx_ptr += scnprintf(idx_ptr, POWER_INDEX_CHAR_SIZE,
					    "%d,", *(idx_ref_uint_ptr+i));
		}
		idx_ptr--;
		idx_ptr += scnprintf(idx_ptr, POWER_INDEX_CHAR_SIZE,
				    " window_cnt = %d",
				    share_idx_ref->window_cnt);

		/* set share sram clear flag and release lock */
		share_idx_ctrl->clear_flag = 1;

		/* put power index data to ftrace */
		trace_swpm_power_idx(idx_buf);
	}
	/* put power data to ftrace */
	trace_swpm_power(pwr_buf);
#endif

	swpm_call_event_notifier(SWPM_LOG_DATA_NOTIFY, NULL);

	mod_timer(t, jiffies + msecs_to_jiffies(swpm_log_interval_ms));
}

/* critical section function */
static void swpm_timer_init(void)
{
	swpm_lock(&swpm_mutex);

	swpm_timer.function = swpm_log_loop;
	timer_setup(&swpm_timer, swpm_log_loop, TIMER_DEFERRABLE);

	swpm_unlock(&swpm_mutex);
}

#endif /* #if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) : 684 */

/***************************************************************************
 *  API
 ***************************************************************************/
void swpm_set_enable(unsigned int type, unsigned int enable)
{
	if (!swpm_init_state
	    || (type != ALL_SWPM_TYPE && type >= NR_SWPM_TYPE))
		return;

	if (type == ALL_SWPM_TYPE) {
		int i;

		for_each_pwr_mtr(i) {
			if (enable) {
				if (swpm_get_status(i))
					continue;
				swpm_set_status(i);
			} else {
				if (!swpm_get_status(i))
					continue;
				swpm_clr_status(i);
			}
		}
		swpm_send_enable_ipi(type, enable);
	} else if (type < NR_SWPM_TYPE) {
		if (enable && !swpm_get_status(type)) {
			swpm_set_status(type);
		} else if (!enable && swpm_get_status(type)) {
			swpm_clr_status(type);
		}
		swpm_send_enable_ipi(type, enable);
	}
}

int swpm_v6886_init(void)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	swpm_get_rec_addr(&rec_phys_addr,
			  &rec_virt_addr,
			  &rec_size);
	/* CONFIG_PHYS_ADDR_T_64BIT */
	swpm_info_ref = (struct swpm_rec_data *)rec_virt_addr;
	pr_info("rec_virt_addr = 0x%llx, swpm_info_ref = 0x%llx\n",
		  rec_virt_addr, swpm_info_ref);
#endif

	if (!swpm_info_ref) {
		pr_notice("get sspm dram addr failed\n");
		ret = -1;
		goto end;
	}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	if (!swpm_init_state)
		swpm_pass_to_sspm();
#endif

	if (!swpm_init_retry_work_queue) {
		swpm_init_retry_work_queue =
			create_workqueue("swpm_init_retry");
		if (!swpm_init_retry_work_queue)
			pr_debug("swpm_init_retry workqueue create failed\n");
	}

	/* Only setup timer function */
	swpm_timer_init();
end:
	return ret;
}

void swpm_v6886_exit(void)
{
	swpm_lock(&swpm_mutex);

	del_timer_sync(&swpm_timer);
	swpm_set_enable(ALL_SWPM_TYPE, 0);

	swpm_unlock(&swpm_mutex);
}
