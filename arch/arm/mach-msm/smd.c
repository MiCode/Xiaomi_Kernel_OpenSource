/* arch/arm/mach-msm/smd.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/termios.h>
#include <linux/ctype.h>
#include <linux/remote_spinlock.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
#include <linux/wakelock.h>
#include <linux/notifier.h>
#include <linux/sort.h>
#include <mach/msm_smd.h>
#include <mach/msm_iomap.h>
#include <mach/system.h>
#include <mach/subsystem_notif.h>
#include <mach/socinfo.h>
#include <mach/proc_comm.h>
#include <asm/cacheflush.h>

#include "smd_private.h"
#include "modem_notifier.h"

#if defined(CONFIG_ARCH_QSD8X50) || defined(CONFIG_ARCH_MSM8X60) \
	|| defined(CONFIG_ARCH_MSM8960) || defined(CONFIG_ARCH_FSM9XXX) \
	|| defined(CONFIG_ARCH_MSM9615)	|| defined(CONFIG_ARCH_APQ8064)
#define CONFIG_QDSP6 1
#endif

#if defined(CONFIG_ARCH_MSM8X60) || defined(CONFIG_ARCH_MSM8960) \
	|| defined(CONFIG_ARCH_APQ8064)
#define CONFIG_DSPS 1
#endif

#if defined(CONFIG_ARCH_MSM8960) \
	|| defined(CONFIG_ARCH_APQ8064)
#define CONFIG_WCNSS 1
#define CONFIG_DSPS_SMSM 1
#endif

#define MODULE_NAME "msm_smd"
#define SMEM_VERSION 0x000B
#define SMD_VERSION 0x00020000
#define SMSM_SNAPSHOT_CNT 64
#define SMSM_SNAPSHOT_SIZE ((SMSM_NUM_ENTRIES + 1) * 4)

uint32_t SMSM_NUM_ENTRIES = 8;
uint32_t SMSM_NUM_HOSTS = 3;

/* Legacy SMSM interrupt notifications */
#define LEGACY_MODEM_SMSM_MASK (SMSM_RESET | SMSM_INIT | SMSM_SMDINIT \
			| SMSM_RUN | SMSM_SYSTEM_DOWNLOAD)

enum {
	MSM_SMD_DEBUG = 1U << 0,
	MSM_SMSM_DEBUG = 1U << 1,
	MSM_SMD_INFO = 1U << 2,
	MSM_SMSM_INFO = 1U << 3,
	MSM_SMx_POWER_INFO = 1U << 4,
};

struct smsm_shared_info {
	uint32_t *state;
	uint32_t *intr_mask;
	uint32_t *intr_mux;
};

static struct smsm_shared_info smsm_info;
static struct kfifo smsm_snapshot_fifo;
static struct wake_lock smsm_snapshot_wakelock;
static int smsm_snapshot_count;
static DEFINE_SPINLOCK(smsm_snapshot_count_lock);

struct smsm_size_info_type {
	uint32_t num_hosts;
	uint32_t num_entries;
	uint32_t reserved0;
	uint32_t reserved1;
};

struct smsm_state_cb_info {
	struct list_head cb_list;
	uint32_t mask;
	void *data;
	void (*notify)(void *data, uint32_t old_state, uint32_t new_state);
};

struct smsm_state_info {
	struct list_head callbacks;
	uint32_t last_value;
	uint32_t intr_mask_set;
	uint32_t intr_mask_clear;
};

struct interrupt_config_item {
	/* must be initialized */
	irqreturn_t (*irq_handler)(int req, void *data);
	/* outgoing interrupt config (set from platform data) */
	uint32_t out_bit_pos;
	void __iomem *out_base;
	uint32_t out_offset;
};

struct interrupt_config {
	struct interrupt_config_item smd;
	struct interrupt_config_item smsm;
};

static irqreturn_t smd_modem_irq_handler(int irq, void *data);
static irqreturn_t smsm_modem_irq_handler(int irq, void *data);
static irqreturn_t smd_dsp_irq_handler(int irq, void *data);
static irqreturn_t smsm_dsp_irq_handler(int irq, void *data);
static irqreturn_t smd_dsps_irq_handler(int irq, void *data);
static irqreturn_t smsm_dsps_irq_handler(int irq, void *data);
static irqreturn_t smd_wcnss_irq_handler(int irq, void *data);
static irqreturn_t smsm_wcnss_irq_handler(int irq, void *data);
static irqreturn_t smd_rpm_irq_handler(int irq, void *data);
static irqreturn_t smsm_irq_handler(int irq, void *data);

static struct interrupt_config private_intr_config[NUM_SMD_SUBSYSTEMS] = {
	[SMD_MODEM] = {
		.smd.irq_handler = smd_modem_irq_handler,
		.smsm.irq_handler = smsm_modem_irq_handler,
	},
	[SMD_Q6] = {
		.smd.irq_handler = smd_dsp_irq_handler,
		.smsm.irq_handler = smsm_dsp_irq_handler,
	},
	[SMD_DSPS] = {
		.smd.irq_handler = smd_dsps_irq_handler,
		.smsm.irq_handler = smsm_dsps_irq_handler,
	},
	[SMD_WCNSS] = {
		.smd.irq_handler = smd_wcnss_irq_handler,
		.smsm.irq_handler = smsm_wcnss_irq_handler,
	},
	[SMD_RPM] = {
		.smd.irq_handler = smd_rpm_irq_handler,
		.smsm.irq_handler = NULL, /* does not support smsm */
	},
};

struct smem_area {
	void *phys_addr;
	unsigned size;
	void __iomem *virt_addr;
};
static uint32_t num_smem_areas;
static struct smem_area *smem_areas;
static void *smem_range_check(void *base, unsigned offset);

struct interrupt_stat interrupt_stats[NUM_SMD_SUBSYSTEMS];

#define SMSM_STATE_ADDR(entry)           (smsm_info.state + entry)
#define SMSM_INTR_MASK_ADDR(entry, host) (smsm_info.intr_mask + \
					  entry * SMSM_NUM_HOSTS + host)
#define SMSM_INTR_MUX_ADDR(entry)        (smsm_info.intr_mux + entry)

/* Internal definitions which are not exported in some targets */
enum {
	SMSM_APPS_DEM_I = 3,
};

static int msm_smd_debug_mask;
module_param_named(debug_mask, msm_smd_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#if defined(CONFIG_MSM_SMD_DEBUG)
#define SMD_DBG(x...) do {				\
		if (msm_smd_debug_mask & MSM_SMD_DEBUG) \
			printk(KERN_DEBUG x);		\
	} while (0)

#define SMSM_DBG(x...) do {					\
		if (msm_smd_debug_mask & MSM_SMSM_DEBUG)	\
			printk(KERN_DEBUG x);			\
	} while (0)

#define SMD_INFO(x...) do {			 	\
		if (msm_smd_debug_mask & MSM_SMD_INFO)	\
			printk(KERN_INFO x);		\
	} while (0)

#define SMSM_INFO(x...) do {				\
		if (msm_smd_debug_mask & MSM_SMSM_INFO) \
			printk(KERN_INFO x);		\
	} while (0)
#define SMx_POWER_INFO(x...) do {				\
		if (msm_smd_debug_mask & MSM_SMx_POWER_INFO) \
			printk(KERN_INFO x);		\
	} while (0)
#else
#define SMD_DBG(x...) do { } while (0)
#define SMSM_DBG(x...) do { } while (0)
#define SMD_INFO(x...) do { } while (0)
#define SMSM_INFO(x...) do { } while (0)
#define SMx_POWER_INFO(x...) do { } while (0)
#endif

static unsigned last_heap_free = 0xffffffff;

static inline void smd_write_intr(unsigned int val,
				const void __iomem *addr);

#if defined(CONFIG_ARCH_MSM7X30)
#define MSM_TRIG_A2M_SMD_INT     \
			(smd_write_intr(1 << 0, MSM_APCS_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMD_INT    \
			(smd_write_intr(1 << 8, MSM_APCS_GCC_BASE + 0x8))
#define MSM_TRIG_A2M_SMSM_INT    \
			(smd_write_intr(1 << 5, MSM_APCS_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMSM_INT   \
			(smd_write_intr(1 << 8, MSM_APCS_GCC_BASE + 0x8))
#define MSM_TRIG_A2DSPS_SMD_INT
#define MSM_TRIG_A2DSPS_SMSM_INT
#define MSM_TRIG_A2WCNSS_SMD_INT
#define MSM_TRIG_A2WCNSS_SMSM_INT
#elif defined(CONFIG_ARCH_MSM8X60)
#define MSM_TRIG_A2M_SMD_INT     \
			(smd_write_intr(1 << 3, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMD_INT    \
			(smd_write_intr(1 << 15, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2M_SMSM_INT    \
			(smd_write_intr(1 << 4, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMSM_INT   \
			(smd_write_intr(1 << 14, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2DSPS_SMD_INT  \
			(smd_write_intr(1, MSM_SIC_NON_SECURE_BASE + 0x4080))
#define MSM_TRIG_A2DSPS_SMSM_INT
#define MSM_TRIG_A2WCNSS_SMD_INT
#define MSM_TRIG_A2WCNSS_SMSM_INT
#elif defined(CONFIG_ARCH_MSM9615)
#define MSM_TRIG_A2M_SMD_INT     \
			(smd_write_intr(1 << 3, MSM_APCS_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMD_INT    \
			(smd_write_intr(1 << 15, MSM_APCS_GCC_BASE + 0x8))
#define MSM_TRIG_A2M_SMSM_INT    \
			(smd_write_intr(1 << 4, MSM_APCS_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMSM_INT   \
			(smd_write_intr(1 << 14, MSM_APCS_GCC_BASE + 0x8))
#define MSM_TRIG_A2DSPS_SMD_INT
#define MSM_TRIG_A2DSPS_SMSM_INT
#define MSM_TRIG_A2WCNSS_SMD_INT
#define MSM_TRIG_A2WCNSS_SMSM_INT
#elif defined(CONFIG_ARCH_FSM9XXX)
#define MSM_TRIG_A2Q6_SMD_INT	\
			(smd_write_intr(1 << 10, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMSM_INT	\
			(smd_write_intr(1 << 10, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2M_SMD_INT	\
			(smd_write_intr(1 << 0, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2M_SMSM_INT	\
			(smd_write_intr(1 << 5, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2DSPS_SMD_INT
#define MSM_TRIG_A2DSPS_SMSM_INT
#define MSM_TRIG_A2WCNSS_SMD_INT
#define MSM_TRIG_A2WCNSS_SMSM_INT
#elif defined(CONFIG_ARCH_MSM7X01A) || defined(CONFIG_ARCH_MSM7x25)
#define MSM_TRIG_A2M_SMD_INT     \
			(smd_write_intr(1, MSM_CSR_BASE + 0x400 + (0) * 4))
#define MSM_TRIG_A2Q6_SMD_INT
#define MSM_TRIG_A2M_SMSM_INT    \
			(smd_write_intr(1, MSM_CSR_BASE + 0x400 + (5) * 4))
#define MSM_TRIG_A2Q6_SMSM_INT
#define MSM_TRIG_A2DSPS_SMD_INT
#define MSM_TRIG_A2DSPS_SMSM_INT
#define MSM_TRIG_A2WCNSS_SMD_INT
#define MSM_TRIG_A2WCNSS_SMSM_INT
#elif defined(CONFIG_ARCH_MSM7X27) || defined(CONFIG_ARCH_MSM7X27A)
#define MSM_TRIG_A2M_SMD_INT     \
			(smd_write_intr(1, MSM_CSR_BASE + 0x400 + (0) * 4))
#define MSM_TRIG_A2Q6_SMD_INT
#define MSM_TRIG_A2M_SMSM_INT    \
			(smd_write_intr(1, MSM_CSR_BASE + 0x400 + (5) * 4))
#define MSM_TRIG_A2Q6_SMSM_INT
#define MSM_TRIG_A2DSPS_SMD_INT
#define MSM_TRIG_A2DSPS_SMSM_INT
#define MSM_TRIG_A2WCNSS_SMD_INT
#define MSM_TRIG_A2WCNSS_SMSM_INT
#else /* use platform device / device tree configuration */
#define MSM_TRIG_A2M_SMD_INT
#define MSM_TRIG_A2Q6_SMD_INT
#define MSM_TRIG_A2M_SMSM_INT
#define MSM_TRIG_A2Q6_SMSM_INT
#define MSM_TRIG_A2DSPS_SMD_INT
#define MSM_TRIG_A2DSPS_SMSM_INT
#define MSM_TRIG_A2WCNSS_SMD_INT
#define MSM_TRIG_A2WCNSS_SMSM_INT
#endif

/*
 * stub out legacy macros if they are not being used so that the legacy
 * code compiles even though it is not used
 *
 * these definitions should not be used in active code and will cause
 * an early failure
 */
#ifndef INT_A9_M2A_0
#define INT_A9_M2A_0 -1
#endif
#ifndef INT_A9_M2A_5
#define INT_A9_M2A_5 -1
#endif
#ifndef INT_ADSP_A11
#define INT_ADSP_A11 -1
#endif
#ifndef INT_ADSP_A11_SMSM
#define INT_ADSP_A11_SMSM -1
#endif
#ifndef INT_DSPS_A11
#define INT_DSPS_A11 -1
#endif
#ifndef INT_DSPS_A11_SMSM
#define INT_DSPS_A11_SMSM -1
#endif
#ifndef INT_WCNSS_A11
#define INT_WCNSS_A11 -1
#endif
#ifndef INT_WCNSS_A11_SMSM
#define INT_WCNSS_A11_SMSM -1
#endif

#define SMD_LOOPBACK_CID 100

#define SMEM_SPINLOCK_SMEM_ALLOC       "S:3"
static remote_spinlock_t remote_spinlock;

static LIST_HEAD(smd_ch_list_loopback);
static void smd_fake_irq_handler(unsigned long arg);
static void smsm_cb_snapshot(uint32_t use_wakelock);

static struct workqueue_struct *smsm_cb_wq;
static void notify_smsm_cb_clients_worker(struct work_struct *work);
static DECLARE_WORK(smsm_cb_work, notify_smsm_cb_clients_worker);
static DEFINE_MUTEX(smsm_lock);
static struct smsm_state_info *smsm_states;
static int spinlocks_initialized;
static RAW_NOTIFIER_HEAD(smsm_driver_state_notifier_list);
static DEFINE_MUTEX(smsm_driver_state_notifier_lock);
static void smsm_driver_state_notify(uint32_t state, void *data);

static inline void smd_write_intr(unsigned int val,
				const void __iomem *addr)
{
	wmb();
	__raw_writel(val, addr);
}

#ifdef CONFIG_WCNSS
static inline void wakeup_v1_riva(void)
{
	/*
	 * workaround hack for RIVA v1 hardware bug
	 * trigger GPIO 40 to wake up RIVA from power collaspe
	 * not to be sent to customers
	 */
	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 1) {
		__raw_writel(0x0, MSM_TLMM_BASE + 0x1284);
		__raw_writel(0x2, MSM_TLMM_BASE + 0x1284);
	}
	/* end workaround */
}
#else
static inline void wakeup_v1_riva(void) {}
#endif

static inline void notify_modem_smd(void)
{
	static const struct interrupt_config_item *intr
	   = &private_intr_config[SMD_MODEM].smd;
	if (intr->out_base) {
		++interrupt_stats[SMD_MODEM].smd_out_config_count;
		smd_write_intr(intr->out_bit_pos,
		intr->out_base + intr->out_offset);
	} else {
		++interrupt_stats[SMD_MODEM].smd_out_hardcode_count;
		MSM_TRIG_A2M_SMD_INT;
	}
}

static inline void notify_dsp_smd(void)
{
	static const struct interrupt_config_item *intr
		= &private_intr_config[SMD_Q6].smd;
	if (intr->out_base) {
		++interrupt_stats[SMD_Q6].smd_out_config_count;
		smd_write_intr(intr->out_bit_pos,
		intr->out_base + intr->out_offset);
	} else {
		++interrupt_stats[SMD_Q6].smd_out_hardcode_count;
		MSM_TRIG_A2Q6_SMD_INT;
	}
}

static inline void notify_dsps_smd(void)
{
	static const struct interrupt_config_item *intr
		= &private_intr_config[SMD_DSPS].smd;
	if (intr->out_base) {
		++interrupt_stats[SMD_DSPS].smd_out_config_count;
		smd_write_intr(intr->out_bit_pos,
		intr->out_base + intr->out_offset);
	} else {
		++interrupt_stats[SMD_DSPS].smd_out_hardcode_count;
		MSM_TRIG_A2DSPS_SMD_INT;
	}
}

static inline void notify_wcnss_smd(void)
{
	static const struct interrupt_config_item *intr
		= &private_intr_config[SMD_WCNSS].smd;
	wakeup_v1_riva();

	if (intr->out_base) {
		++interrupt_stats[SMD_WCNSS].smd_out_config_count;
		smd_write_intr(intr->out_bit_pos,
		intr->out_base + intr->out_offset);
	} else {
		++interrupt_stats[SMD_WCNSS].smd_out_hardcode_count;
		MSM_TRIG_A2WCNSS_SMD_INT;
	}
}

static inline void notify_rpm_smd(void)
{
	static const struct interrupt_config_item *intr
		= &private_intr_config[SMD_RPM].smd;

	if (intr->out_base) {
		++interrupt_stats[SMD_RPM].smd_out_config_count;
		smd_write_intr(intr->out_bit_pos,
		intr->out_base + intr->out_offset);
	}
}

static inline void notify_modem_smsm(void)
{
	static const struct interrupt_config_item *intr
		= &private_intr_config[SMD_MODEM].smsm;
	if (intr->out_base) {
		++interrupt_stats[SMD_MODEM].smsm_out_config_count;
		smd_write_intr(intr->out_bit_pos,
		intr->out_base + intr->out_offset);
	} else {
		++interrupt_stats[SMD_MODEM].smsm_out_hardcode_count;
		MSM_TRIG_A2M_SMSM_INT;
	}
}

static inline void notify_dsp_smsm(void)
{
	static const struct interrupt_config_item *intr
		= &private_intr_config[SMD_Q6].smsm;
	if (intr->out_base) {
		++interrupt_stats[SMD_Q6].smsm_out_config_count;
		smd_write_intr(intr->out_bit_pos,
		intr->out_base + intr->out_offset);
	} else {
		++interrupt_stats[SMD_Q6].smsm_out_hardcode_count;
		MSM_TRIG_A2Q6_SMSM_INT;
	}
}

static inline void notify_dsps_smsm(void)
{
	static const struct interrupt_config_item *intr
		= &private_intr_config[SMD_DSPS].smsm;
	if (intr->out_base) {
		++interrupt_stats[SMD_DSPS].smsm_out_config_count;
		smd_write_intr(intr->out_bit_pos,
		intr->out_base + intr->out_offset);
	} else {
		++interrupt_stats[SMD_DSPS].smsm_out_hardcode_count;
		MSM_TRIG_A2DSPS_SMSM_INT;
	}
}

static inline void notify_wcnss_smsm(void)
{
	static const struct interrupt_config_item *intr
		= &private_intr_config[SMD_WCNSS].smsm;
	wakeup_v1_riva();

	if (intr->out_base) {
		++interrupt_stats[SMD_WCNSS].smsm_out_config_count;
		smd_write_intr(intr->out_bit_pos,
		intr->out_base + intr->out_offset);
	} else {
		++interrupt_stats[SMD_WCNSS].smsm_out_hardcode_count;
		MSM_TRIG_A2WCNSS_SMSM_INT;
	}
}

static void notify_other_smsm(uint32_t smsm_entry, uint32_t notify_mask)
{
	/* older protocol don't use smsm_intr_mask,
	   but still communicates with modem */
	if (!smsm_info.intr_mask ||
	    (__raw_readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_MODEM))
				& notify_mask))
		notify_modem_smsm();

	if (smsm_info.intr_mask &&
	    (__raw_readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_Q6))
				& notify_mask)) {
		uint32_t mux_val;

		if (cpu_is_qsd8x50() && smsm_info.intr_mux) {
			mux_val = __raw_readl(
					SMSM_INTR_MUX_ADDR(SMEM_APPS_Q6_SMSM));
			mux_val++;
			__raw_writel(mux_val,
					SMSM_INTR_MUX_ADDR(SMEM_APPS_Q6_SMSM));
		}
		notify_dsp_smsm();
	}

	if (smsm_info.intr_mask &&
	    (__raw_readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_WCNSS))
				& notify_mask)) {
		notify_wcnss_smsm();
	}

	if (smsm_info.intr_mask &&
	    (__raw_readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_DSPS))
				& notify_mask)) {
		notify_dsps_smsm();
	}

	/*
	 * Notify local SMSM callback clients without wakelock since this
	 * code is used by power management during power-down/-up sequencing
	 * on DEM-based targets.  Grabbing a wakelock in this case will
	 * abort the power-down sequencing.
	 */
	if (smsm_info.intr_mask &&
	    (__raw_readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_APPS))
				& notify_mask)) {
		smsm_cb_snapshot(0);
	}
}

void smd_diag(void)
{
	char *x;
	int size;

	x = smem_find(ID_DIAG_ERR_MSG, SZ_DIAG_ERR_MSG);
	if (x != 0) {
		x[SZ_DIAG_ERR_MSG - 1] = 0;
		SMD_INFO("smem: DIAG '%s'\n", x);
	}

	x = smem_get_entry(SMEM_ERR_CRASH_LOG, &size);
	if (x != 0) {
		x[size - 1] = 0;
		pr_err("smem: CRASH LOG\n'%s'\n", x);
	}
}


static void handle_modem_crash(void)
{
	pr_err("MODEM/AMSS has CRASHED\n");
	smd_diag();

	/* hard reboot if possible FIXME
	if (msm_reset_hook)
		msm_reset_hook();
	*/

	/* in this case the modem or watchdog should reboot us */
	for (;;)
		;
}

int smsm_check_for_modem_crash(void)
{
	/* if the modem's not ready yet, we have to hope for the best */
	if (!smsm_info.state)
		return 0;

	if (__raw_readl(SMSM_STATE_ADDR(SMSM_MODEM_STATE)) & SMSM_RESET) {
		handle_modem_crash();
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(smsm_check_for_modem_crash);

/* the spinlock is used to synchronize between the
 * irq handler and code that mutates the channel
 * list or fiddles with channel state
 */
static DEFINE_SPINLOCK(smd_lock);
DEFINE_SPINLOCK(smem_lock);

/* the mutex is used during open() and close()
 * operations to avoid races while creating or
 * destroying smd_channel structures
 */
static DEFINE_MUTEX(smd_creation_mutex);

static int smd_initialized;

struct smd_shared_v1 {
	struct smd_half_channel ch0;
	unsigned char data0[SMD_BUF_SIZE];
	struct smd_half_channel ch1;
	unsigned char data1[SMD_BUF_SIZE];
};

struct smd_shared_v2 {
	struct smd_half_channel ch0;
	struct smd_half_channel ch1;
};

struct smd_shared_v2_word_access {
	struct smd_half_channel_word_access ch0;
	struct smd_half_channel_word_access ch1;
};

struct smd_channel {
	volatile void *send; /* some variant of smd_half_channel */
	volatile void *recv; /* some variant of smd_half_channel */
	unsigned char *send_data;
	unsigned char *recv_data;
	unsigned fifo_size;
	unsigned fifo_mask;
	struct list_head ch_list;

	unsigned current_packet;
	unsigned n;
	void *priv;
	void (*notify)(void *priv, unsigned flags);

	int (*read)(smd_channel_t *ch, void *data, int len, int user_buf);
	int (*write)(smd_channel_t *ch, const void *data, int len,
			int user_buf);
	int (*read_avail)(smd_channel_t *ch);
	int (*write_avail)(smd_channel_t *ch);
	int (*read_from_cb)(smd_channel_t *ch, void *data, int len,
			int user_buf);

	void (*update_state)(smd_channel_t *ch);
	unsigned last_state;
	void (*notify_other_cpu)(void);

	char name[20];
	struct platform_device pdev;
	unsigned type;

	int pending_pkt_sz;

	char is_pkt_ch;

	/*
	 * private internal functions to access *send and *recv.
	 * never to be exported outside of smd
	 */
	struct smd_half_channel_access *half_ch;
};

struct edge_to_pid {
	uint32_t	local_pid;
	uint32_t	remote_pid;
	char		subsys_name[SMD_MAX_CH_NAME_LEN];
};

/**
 * Maps edge type to local and remote processor ID's.
 */
static struct edge_to_pid edge_to_pids[] = {
	[SMD_APPS_MODEM] = {SMD_APPS, SMD_MODEM, "modem"},
	[SMD_APPS_QDSP] = {SMD_APPS, SMD_Q6, "q6"},
	[SMD_MODEM_QDSP] = {SMD_MODEM, SMD_Q6},
	[SMD_APPS_DSPS] = {SMD_APPS, SMD_DSPS, "dsps"},
	[SMD_MODEM_DSPS] = {SMD_MODEM, SMD_DSPS},
	[SMD_QDSP_DSPS] = {SMD_Q6, SMD_DSPS},
	[SMD_APPS_WCNSS] = {SMD_APPS, SMD_WCNSS, "wcnss"},
	[SMD_MODEM_WCNSS] = {SMD_MODEM, SMD_WCNSS},
	[SMD_QDSP_WCNSS] = {SMD_Q6, SMD_WCNSS},
	[SMD_DSPS_WCNSS] = {SMD_DSPS, SMD_WCNSS},
	[SMD_APPS_Q6FW] = {SMD_APPS, SMD_MODEM_Q6_FW},
	[SMD_MODEM_Q6FW] = {SMD_MODEM, SMD_MODEM_Q6_FW},
	[SMD_QDSP_Q6FW] = {SMD_Q6, SMD_MODEM_Q6_FW},
	[SMD_DSPS_Q6FW] = {SMD_DSPS, SMD_MODEM_Q6_FW},
	[SMD_WCNSS_Q6FW] = {SMD_WCNSS, SMD_MODEM_Q6_FW},
	[SMD_APPS_RPM] = {SMD_APPS, SMD_RPM},
	[SMD_MODEM_RPM] = {SMD_MODEM, SMD_RPM},
	[SMD_QDSP_RPM] = {SMD_Q6, SMD_RPM},
	[SMD_WCNSS_RPM] = {SMD_WCNSS, SMD_RPM},
};

struct restart_notifier_block {
	unsigned processor;
	char *name;
	struct notifier_block nb;
};

static int disable_smsm_reset_handshake;
static struct platform_device loopback_tty_pdev = {.name = "LOOPBACK_TTY"};

static LIST_HEAD(smd_ch_closed_list);
static LIST_HEAD(smd_ch_closing_list);
static LIST_HEAD(smd_ch_to_close_list);
static LIST_HEAD(smd_ch_list_modem);
static LIST_HEAD(smd_ch_list_dsp);
static LIST_HEAD(smd_ch_list_dsps);
static LIST_HEAD(smd_ch_list_wcnss);
static LIST_HEAD(smd_ch_list_rpm);

static unsigned char smd_ch_allocated[64];
static struct work_struct probe_work;

static void finalize_channel_close_fn(struct work_struct *work);
static DECLARE_WORK(finalize_channel_close_work, finalize_channel_close_fn);
static struct workqueue_struct *channel_close_wq;

static int smd_alloc_channel(struct smd_alloc_elm *alloc_elm);

/* on smp systems, the probe might get called from multiple cores,
   hence use a lock */
static DEFINE_MUTEX(smd_probe_lock);

static void smd_channel_probe_worker(struct work_struct *work)
{
	struct smd_alloc_elm *shared;
	unsigned n;
	uint32_t type;

	shared = smem_find(ID_CH_ALLOC_TBL, sizeof(*shared) * 64);

	if (!shared) {
		pr_err("%s: allocation table not initialized\n", __func__);
		return;
	}

	mutex_lock(&smd_probe_lock);
	for (n = 0; n < 64; n++) {
		if (smd_ch_allocated[n])
			continue;

		/* channel should be allocated only if APPS
		   processor is involved */
		type = SMD_CHANNEL_TYPE(shared[n].type);
		if (type >= ARRAY_SIZE(edge_to_pids) ||
				edge_to_pids[type].local_pid != SMD_APPS)
			continue;
		if (!shared[n].ref_count)
			continue;
		if (!shared[n].name[0])
			continue;

		if (!smd_alloc_channel(&shared[n]))
			smd_ch_allocated[n] = 1;
		else
			SMD_INFO("Probe skipping ch %d, not allocated\n", n);
	}
	mutex_unlock(&smd_probe_lock);
}

/**
 * Lookup processor ID and determine if it belongs to the proved edge
 * type.
 *
 * @shared2:   Pointer to v2 shared channel structure
 * @type:      Edge type
 * @pid:       Processor ID of processor on edge
 * @local_ch:  Channel that belongs to processor @pid
 * @remote_ch: Other side of edge contained @pid
 *
 * Returns 0 for not on edge, 1 for found on edge
 */
static int pid_is_on_edge(struct smd_shared_v2 *shared2,
		uint32_t type, uint32_t pid,
		struct smd_half_channel **local_ch,
		struct smd_half_channel **remote_ch
		)
{
	int ret = 0;
	struct edge_to_pid *edge;

	*local_ch = 0;
	*remote_ch = 0;

	if (!shared2 || (type >= ARRAY_SIZE(edge_to_pids)))
		return 0;

	edge = &edge_to_pids[type];
	if (edge->local_pid != edge->remote_pid) {
		if (pid == edge->local_pid) {
			*local_ch = &shared2->ch0;
			*remote_ch = &shared2->ch1;
			ret = 1;
		} else if (pid == edge->remote_pid) {
			*local_ch = &shared2->ch1;
			*remote_ch = &shared2->ch0;
			ret = 1;
		}
	}

	return ret;
}

/*
 * Returns a pointer to the subsystem name or NULL if no
 * subsystem name is available.
 *
 * @type - Edge definition
 */
const char *smd_edge_to_subsystem(uint32_t type)
{
	const char *subsys = NULL;

	if (type < ARRAY_SIZE(edge_to_pids)) {
		subsys = edge_to_pids[type].subsys_name;
		if (subsys[0] == 0x0)
			subsys = NULL;
	}
	return subsys;
}
EXPORT_SYMBOL(smd_edge_to_subsystem);

/*
 * Returns a pointer to the subsystem name given the
 * remote processor ID.
 *
 * @pid     Remote processor ID
 * @returns Pointer to subsystem name or NULL if not found
 */
const char *smd_pid_to_subsystem(uint32_t pid)
{
	const char *subsys = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(edge_to_pids); ++i) {
		if (pid == edge_to_pids[i].remote_pid &&
			edge_to_pids[i].subsys_name[0] != 0x0
			) {
			subsys = edge_to_pids[i].subsys_name;
			break;
		}
	}

	return subsys;
}
EXPORT_SYMBOL(smd_pid_to_subsystem);

static void smd_reset_edge(struct smd_half_channel *ch, unsigned new_state)
{
	if (ch->state != SMD_SS_CLOSED) {
		ch->state = new_state;
		ch->fDSR = 0;
		ch->fCTS = 0;
		ch->fCD = 0;
		ch->fSTATE = 1;
	}
}

static void smd_channel_reset_state(struct smd_alloc_elm *shared,
		unsigned new_state, unsigned pid)
{
	unsigned n;
	struct smd_shared_v2 *shared2;
	uint32_t type;
	struct smd_half_channel *local_ch;
	struct smd_half_channel *remote_ch;

	for (n = 0; n < SMD_CHANNELS; n++) {
		if (!shared[n].ref_count)
			continue;
		if (!shared[n].name[0])
			continue;

		type = SMD_CHANNEL_TYPE(shared[n].type);
		shared2 = smem_alloc(SMEM_SMD_BASE_ID + n, sizeof(*shared2));
		if (!shared2)
			continue;

		if (pid_is_on_edge(shared2, type, pid, &local_ch, &remote_ch))
			smd_reset_edge(local_ch, new_state);

		/*
		 * ModemFW is in the same subsystem as ModemSW, but has
		 * separate SMD edges that need to be reset.
		 */
		if (pid == SMSM_MODEM &&
				pid_is_on_edge(shared2, type, SMD_MODEM_Q6_FW,
				 &local_ch, &remote_ch))
			smd_reset_edge(local_ch, new_state);
	}
}


void smd_channel_reset(uint32_t restart_pid)
{
	struct smd_alloc_elm *shared;
	unsigned long flags;

	SMD_DBG("%s: starting reset\n", __func__);
	shared = smem_find(ID_CH_ALLOC_TBL, sizeof(*shared) * 64);
	if (!shared) {
		pr_err("%s: allocation table not initialized\n", __func__);
		return;
	}

	/* release any held spinlocks */
	remote_spin_release(&remote_spinlock, restart_pid);
	remote_spin_release_all(restart_pid);

	/* reset SMSM entry */
	if (smsm_info.state) {
		writel_relaxed(0, SMSM_STATE_ADDR(restart_pid));

		/* restart SMSM init handshake */
		if (restart_pid == SMSM_MODEM) {
			smsm_change_state(SMSM_APPS_STATE,
				SMSM_INIT | SMSM_SMD_LOOPBACK | SMSM_RESET,
				0);
		}

		/* notify SMSM processors */
		smsm_irq_handler(0, 0);
		notify_modem_smsm();
		notify_dsp_smsm();
		notify_dsps_smsm();
		notify_wcnss_smsm();
	}

	/* change all remote states to CLOSING */
	mutex_lock(&smd_probe_lock);
	spin_lock_irqsave(&smd_lock, flags);
	smd_channel_reset_state(shared, SMD_SS_CLOSING, restart_pid);
	spin_unlock_irqrestore(&smd_lock, flags);
	mutex_unlock(&smd_probe_lock);

	/* notify SMD processors */
	mb();
	smd_fake_irq_handler(0);
	notify_modem_smd();
	notify_dsp_smd();
	notify_dsps_smd();
	notify_wcnss_smd();

	/* change all remote states to CLOSED */
	mutex_lock(&smd_probe_lock);
	spin_lock_irqsave(&smd_lock, flags);
	smd_channel_reset_state(shared, SMD_SS_CLOSED, restart_pid);
	spin_unlock_irqrestore(&smd_lock, flags);
	mutex_unlock(&smd_probe_lock);

	/* notify SMD processors */
	mb();
	smd_fake_irq_handler(0);
	notify_modem_smd();
	notify_dsp_smd();
	notify_dsps_smd();
	notify_wcnss_smd();

	SMD_DBG("%s: finished reset\n", __func__);
}

/* how many bytes are available for reading */
static int smd_stream_read_avail(struct smd_channel *ch)
{
	return (ch->half_ch->get_head(ch->recv) -
			ch->half_ch->get_tail(ch->recv)) & ch->fifo_mask;
}

/* how many bytes we are free to write */
static int smd_stream_write_avail(struct smd_channel *ch)
{
	return ch->fifo_mask - ((ch->half_ch->get_head(ch->send) -
			ch->half_ch->get_tail(ch->send)) & ch->fifo_mask);
}

static int smd_packet_read_avail(struct smd_channel *ch)
{
	if (ch->current_packet) {
		int n = smd_stream_read_avail(ch);
		if (n > ch->current_packet)
			n = ch->current_packet;
		return n;
	} else {
		return 0;
	}
}

static int smd_packet_write_avail(struct smd_channel *ch)
{
	int n = smd_stream_write_avail(ch);
	return n > SMD_HEADER_SIZE ? n - SMD_HEADER_SIZE : 0;
}

static int ch_is_open(struct smd_channel *ch)
{
	return (ch->half_ch->get_state(ch->recv) == SMD_SS_OPENED ||
		ch->half_ch->get_state(ch->recv) == SMD_SS_FLUSHING)
		&& (ch->half_ch->get_state(ch->send) == SMD_SS_OPENED);
}

/* provide a pointer and length to readable data in the fifo */
static unsigned ch_read_buffer(struct smd_channel *ch, void **ptr)
{
	unsigned head = ch->half_ch->get_head(ch->recv);
	unsigned tail = ch->half_ch->get_tail(ch->recv);
	*ptr = (void *) (ch->recv_data + tail);

	if (tail <= head)
		return head - tail;
	else
		return ch->fifo_size - tail;
}

static int read_intr_blocked(struct smd_channel *ch)
{
	return ch->half_ch->get_fBLOCKREADINTR(ch->recv);
}

/* advance the fifo read pointer after data from ch_read_buffer is consumed */
static void ch_read_done(struct smd_channel *ch, unsigned count)
{
	BUG_ON(count > smd_stream_read_avail(ch));
	ch->half_ch->set_tail(ch->recv,
		(ch->half_ch->get_tail(ch->recv) + count) & ch->fifo_mask);
	wmb();
	ch->half_ch->set_fTAIL(ch->send,  1);
}

/* basic read interface to ch_read_{buffer,done} used
 * by smd_*_read() and update_packet_state()
 * will read-and-discard if the _data pointer is null
 */
static int ch_read(struct smd_channel *ch, void *_data, int len, int user_buf)
{
	void *ptr;
	unsigned n;
	unsigned char *data = _data;
	int orig_len = len;
	int r = 0;

	while (len > 0) {
		n = ch_read_buffer(ch, &ptr);
		if (n == 0)
			break;

		if (n > len)
			n = len;
		if (_data) {
			if (user_buf) {
				r = copy_to_user(data, ptr, n);
				if (r > 0) {
					pr_err("%s: "
						"copy_to_user could not copy "
						"%i bytes.\n",
						__func__,
						r);
				}
			} else
				memcpy(data, ptr, n);
		}

		data += n;
		len -= n;
		ch_read_done(ch, n);
	}

	return orig_len - len;
}

static void update_stream_state(struct smd_channel *ch)
{
	/* streams have no special state requiring updating */
}

static void update_packet_state(struct smd_channel *ch)
{
	unsigned hdr[5];
	int r;

	/* can't do anything if we're in the middle of a packet */
	while (ch->current_packet == 0) {
		/* discard 0 length packets if any */

		/* don't bother unless we can get the full header */
		if (smd_stream_read_avail(ch) < SMD_HEADER_SIZE)
			return;

		r = ch_read(ch, hdr, SMD_HEADER_SIZE, 0);
		BUG_ON(r != SMD_HEADER_SIZE);

		ch->current_packet = hdr[0];
	}
}

/* provide a pointer and length to next free space in the fifo */
static unsigned ch_write_buffer(struct smd_channel *ch, void **ptr)
{
	unsigned head = ch->half_ch->get_head(ch->send);
	unsigned tail = ch->half_ch->get_tail(ch->send);
	*ptr = (void *) (ch->send_data + head);

	if (head < tail) {
		return tail - head - 1;
	} else {
		if (tail == 0)
			return ch->fifo_size - head - 1;
		else
			return ch->fifo_size - head;
	}
}

/* advace the fifo write pointer after freespace
 * from ch_write_buffer is filled
 */
static void ch_write_done(struct smd_channel *ch, unsigned count)
{
	BUG_ON(count > smd_stream_write_avail(ch));
	ch->half_ch->set_head(ch->send,
		(ch->half_ch->get_head(ch->send) + count) & ch->fifo_mask);
	wmb();
	ch->half_ch->set_fHEAD(ch->send, 1);
}

static void ch_set_state(struct smd_channel *ch, unsigned n)
{
	if (n == SMD_SS_OPENED) {
		ch->half_ch->set_fDSR(ch->send, 1);
		ch->half_ch->set_fCTS(ch->send, 1);
		ch->half_ch->set_fCD(ch->send, 1);
	} else {
		ch->half_ch->set_fDSR(ch->send, 0);
		ch->half_ch->set_fCTS(ch->send, 0);
		ch->half_ch->set_fCD(ch->send, 0);
	}
	ch->half_ch->set_state(ch->send, n);
	ch->half_ch->set_fSTATE(ch->send, 1);
	ch->notify_other_cpu();
}

static void do_smd_probe(void)
{
	struct smem_shared *shared = (void *) MSM_SHARED_RAM_BASE;
	if (shared->heap_info.free_offset != last_heap_free) {
		last_heap_free = shared->heap_info.free_offset;
		schedule_work(&probe_work);
	}
}

static void smd_state_change(struct smd_channel *ch,
			     unsigned last, unsigned next)
{
	ch->last_state = next;

	SMD_INFO("SMD: ch %d %d -> %d\n", ch->n, last, next);

	switch (next) {
	case SMD_SS_OPENING:
		if (ch->half_ch->get_state(ch->send) == SMD_SS_CLOSING ||
		    ch->half_ch->get_state(ch->send) == SMD_SS_CLOSED) {
			ch->half_ch->set_tail(ch->recv, 0);
			ch->half_ch->set_head(ch->send, 0);
			ch->half_ch->set_fBLOCKREADINTR(ch->send, 0);
			ch_set_state(ch, SMD_SS_OPENING);
		}
		break;
	case SMD_SS_OPENED:
		if (ch->half_ch->get_state(ch->send) == SMD_SS_OPENING) {
			ch_set_state(ch, SMD_SS_OPENED);
			ch->notify(ch->priv, SMD_EVENT_OPEN);
		}
		break;
	case SMD_SS_FLUSHING:
	case SMD_SS_RESET:
		/* we should force them to close? */
		break;
	case SMD_SS_CLOSED:
		if (ch->half_ch->get_state(ch->send) == SMD_SS_OPENED) {
			ch_set_state(ch, SMD_SS_CLOSING);
			ch->current_packet = 0;
			ch->pending_pkt_sz = 0;
			ch->notify(ch->priv, SMD_EVENT_CLOSE);
		}
		break;
	case SMD_SS_CLOSING:
		if (ch->half_ch->get_state(ch->send) == SMD_SS_CLOSED) {
			list_move(&ch->ch_list,
					&smd_ch_to_close_list);
			queue_work(channel_close_wq,
						&finalize_channel_close_work);
		}
		break;
	}
}

static void handle_smd_irq_closing_list(void)
{
	unsigned long flags;
	struct smd_channel *ch;
	struct smd_channel *index;
	unsigned tmp;

	spin_lock_irqsave(&smd_lock, flags);
	list_for_each_entry_safe(ch, index, &smd_ch_closing_list, ch_list) {
		if (ch->half_ch->get_fSTATE(ch->recv))
			ch->half_ch->set_fSTATE(ch->recv, 0);
		tmp = ch->half_ch->get_state(ch->recv);
		if (tmp != ch->last_state)
			smd_state_change(ch, ch->last_state, tmp);
	}
	spin_unlock_irqrestore(&smd_lock, flags);
}

static void handle_smd_irq(struct list_head *list, void (*notify)(void))
{
	unsigned long flags;
	struct smd_channel *ch;
	unsigned ch_flags;
	unsigned tmp;
	unsigned char state_change;

	spin_lock_irqsave(&smd_lock, flags);
	list_for_each_entry(ch, list, ch_list) {
		state_change = 0;
		ch_flags = 0;
		if (ch_is_open(ch)) {
			if (ch->half_ch->get_fHEAD(ch->recv)) {
				ch->half_ch->set_fHEAD(ch->recv, 0);
				ch_flags |= 1;
			}
			if (ch->half_ch->get_fTAIL(ch->recv)) {
				ch->half_ch->set_fTAIL(ch->recv, 0);
				ch_flags |= 2;
			}
			if (ch->half_ch->get_fSTATE(ch->recv)) {
				ch->half_ch->set_fSTATE(ch->recv, 0);
				ch_flags |= 4;
			}
		}
		tmp = ch->half_ch->get_state(ch->recv);
		if (tmp != ch->last_state) {
			SMx_POWER_INFO("SMD ch%d '%s' State change %d->%d\n",
					ch->n, ch->name, ch->last_state, tmp);
			smd_state_change(ch, ch->last_state, tmp);
			state_change = 1;
		}
		if (ch_flags & 0x3) {
			ch->update_state(ch);
			SMx_POWER_INFO("SMD ch%d '%s' Data event r%d/w%d\n",
					ch->n, ch->name,
					ch->read_avail(ch),
					ch->fifo_size - ch->write_avail(ch));
			ch->notify(ch->priv, SMD_EVENT_DATA);
		}
		if (ch_flags & 0x4 && !state_change) {
			SMx_POWER_INFO("SMD ch%d '%s' State update\n",
					ch->n, ch->name);
			ch->notify(ch->priv, SMD_EVENT_STATUS);
		}
	}
	spin_unlock_irqrestore(&smd_lock, flags);
	do_smd_probe();
}

static irqreturn_t smd_modem_irq_handler(int irq, void *data)
{
	SMx_POWER_INFO("SMD Int Modem->Apps\n");
	++interrupt_stats[SMD_MODEM].smd_in_count;
	handle_smd_irq(&smd_ch_list_modem, notify_modem_smd);
	handle_smd_irq_closing_list();
	return IRQ_HANDLED;
}

static irqreturn_t smd_dsp_irq_handler(int irq, void *data)
{
	SMx_POWER_INFO("SMD Int LPASS->Apps\n");
	++interrupt_stats[SMD_Q6].smd_in_count;
	handle_smd_irq(&smd_ch_list_dsp, notify_dsp_smd);
	handle_smd_irq_closing_list();
	return IRQ_HANDLED;
}

static irqreturn_t smd_dsps_irq_handler(int irq, void *data)
{
	SMx_POWER_INFO("SMD Int DSPS->Apps\n");
	++interrupt_stats[SMD_DSPS].smd_in_count;
	handle_smd_irq(&smd_ch_list_dsps, notify_dsps_smd);
	handle_smd_irq_closing_list();
	return IRQ_HANDLED;
}

static irqreturn_t smd_wcnss_irq_handler(int irq, void *data)
{
	SMx_POWER_INFO("SMD Int WCNSS->Apps\n");
	++interrupt_stats[SMD_WCNSS].smd_in_count;
	handle_smd_irq(&smd_ch_list_wcnss, notify_wcnss_smd);
	handle_smd_irq_closing_list();
	return IRQ_HANDLED;
}

static irqreturn_t smd_rpm_irq_handler(int irq, void *data)
{
	SMx_POWER_INFO("SMD Int RPM->Apps\n");
	++interrupt_stats[SMD_RPM].smd_in_count;
	handle_smd_irq(&smd_ch_list_rpm, notify_rpm_smd);
	handle_smd_irq_closing_list();
	return IRQ_HANDLED;
}

static void smd_fake_irq_handler(unsigned long arg)
{
	handle_smd_irq(&smd_ch_list_modem, notify_modem_smd);
	handle_smd_irq(&smd_ch_list_dsp, notify_dsp_smd);
	handle_smd_irq(&smd_ch_list_dsps, notify_dsps_smd);
	handle_smd_irq(&smd_ch_list_wcnss, notify_wcnss_smd);
	handle_smd_irq(&smd_ch_list_rpm, notify_rpm_smd);
	handle_smd_irq_closing_list();
}

static DECLARE_TASKLET(smd_fake_irq_tasklet, smd_fake_irq_handler, 0);

static inline int smd_need_int(struct smd_channel *ch)
{
	if (ch_is_open(ch)) {
		if (ch->half_ch->get_fHEAD(ch->recv) ||
				ch->half_ch->get_fTAIL(ch->recv) ||
				ch->half_ch->get_fSTATE(ch->recv))
			return 1;
		if (ch->half_ch->get_state(ch->recv) != ch->last_state)
			return 1;
	}
	return 0;
}

void smd_sleep_exit(void)
{
	unsigned long flags;
	struct smd_channel *ch;
	int need_int = 0;

	spin_lock_irqsave(&smd_lock, flags);
	list_for_each_entry(ch, &smd_ch_list_modem, ch_list) {
		if (smd_need_int(ch)) {
			need_int = 1;
			break;
		}
	}
	list_for_each_entry(ch, &smd_ch_list_dsp, ch_list) {
		if (smd_need_int(ch)) {
			need_int = 1;
			break;
		}
	}
	list_for_each_entry(ch, &smd_ch_list_dsps, ch_list) {
		if (smd_need_int(ch)) {
			need_int = 1;
			break;
		}
	}
	list_for_each_entry(ch, &smd_ch_list_wcnss, ch_list) {
		if (smd_need_int(ch)) {
			need_int = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&smd_lock, flags);
	do_smd_probe();

	if (need_int) {
		SMD_DBG("smd_sleep_exit need interrupt\n");
		tasklet_schedule(&smd_fake_irq_tasklet);
	}
}
EXPORT_SYMBOL(smd_sleep_exit);

static int smd_is_packet(struct smd_alloc_elm *alloc_elm)
{
	if (SMD_XFER_TYPE(alloc_elm->type) == 1)
		return 0;
	else if (SMD_XFER_TYPE(alloc_elm->type) == 2)
		return 1;

	/* for cases where xfer type is 0 */
	if (!strncmp(alloc_elm->name, "DAL", 3))
		return 0;

	/* for cases where xfer type is 0 */
	if (!strncmp(alloc_elm->name, "RPCCALL_QDSP", 12))
		return 0;

	if (alloc_elm->cid > 4 || alloc_elm->cid == 1)
		return 1;
	else
		return 0;
}

static int smd_stream_write(smd_channel_t *ch, const void *_data, int len,
				int user_buf)
{
	void *ptr;
	const unsigned char *buf = _data;
	unsigned xfer;
	int orig_len = len;
	int r = 0;

	SMD_DBG("smd_stream_write() %d -> ch%d\n", len, ch->n);
	if (len < 0)
		return -EINVAL;
	else if (len == 0)
		return 0;

	while ((xfer = ch_write_buffer(ch, &ptr)) != 0) {
		if (!ch_is_open(ch)) {
			len = orig_len;
			break;
		}
		if (xfer > len)
			xfer = len;
		if (user_buf) {
			r = copy_from_user(ptr, buf, xfer);
			if (r > 0) {
				pr_err("%s: "
					"copy_from_user could not copy %i "
					"bytes.\n",
					__func__,
					r);
			}
		} else
			memcpy(ptr, buf, xfer);
		ch_write_done(ch, xfer);
		len -= xfer;
		buf += xfer;
		if (len == 0)
			break;
	}

	if (orig_len - len)
		ch->notify_other_cpu();

	return orig_len - len;
}

static int smd_packet_write(smd_channel_t *ch, const void *_data, int len,
				int user_buf)
{
	int ret;
	unsigned hdr[5];

	SMD_DBG("smd_packet_write() %d -> ch%d\n", len, ch->n);
	if (len < 0)
		return -EINVAL;
	else if (len == 0)
		return 0;

	if (smd_stream_write_avail(ch) < (len + SMD_HEADER_SIZE))
		return -ENOMEM;

	hdr[0] = len;
	hdr[1] = hdr[2] = hdr[3] = hdr[4] = 0;


	ret = smd_stream_write(ch, hdr, sizeof(hdr), 0);
	if (ret < 0 || ret != sizeof(hdr)) {
		SMD_DBG("%s failed to write pkt header: "
			"%d returned\n", __func__, ret);
		return -1;
	}


	ret = smd_stream_write(ch, _data, len, user_buf);
	if (ret < 0 || ret != len) {
		SMD_DBG("%s failed to write pkt data: "
			"%d returned\n", __func__, ret);
		return ret;
	}

	return len;
}

static int smd_stream_read(smd_channel_t *ch, void *data, int len, int user_buf)
{
	int r;

	if (len < 0)
		return -EINVAL;

	r = ch_read(ch, data, len, user_buf);
	if (r > 0)
		if (!read_intr_blocked(ch))
			ch->notify_other_cpu();

	return r;
}

static int smd_packet_read(smd_channel_t *ch, void *data, int len, int user_buf)
{
	unsigned long flags;
	int r;

	if (len < 0)
		return -EINVAL;

	if (len > ch->current_packet)
		len = ch->current_packet;

	r = ch_read(ch, data, len, user_buf);
	if (r > 0)
		if (!read_intr_blocked(ch))
			ch->notify_other_cpu();

	spin_lock_irqsave(&smd_lock, flags);
	ch->current_packet -= r;
	update_packet_state(ch);
	spin_unlock_irqrestore(&smd_lock, flags);

	return r;
}

static int smd_packet_read_from_cb(smd_channel_t *ch, void *data, int len,
					int user_buf)
{
	int r;

	if (len < 0)
		return -EINVAL;

	if (len > ch->current_packet)
		len = ch->current_packet;

	r = ch_read(ch, data, len, user_buf);
	if (r > 0)
		if (!read_intr_blocked(ch))
			ch->notify_other_cpu();

	ch->current_packet -= r;
	update_packet_state(ch);

	return r;
}

#if (defined(CONFIG_MSM_SMD_PKG4) || defined(CONFIG_MSM_SMD_PKG3))
static int smd_alloc_v2(struct smd_channel *ch)
{
	void *buffer;
	unsigned buffer_sz;

	if (is_word_access_ch(ch->type)) {
		struct smd_shared_v2_word_access *shared2;
		shared2 = smem_alloc(SMEM_SMD_BASE_ID + ch->n,
						sizeof(*shared2));
		if (!shared2) {
			SMD_INFO("smem_alloc failed ch=%d\n", ch->n);
			return -EINVAL;
		}
		ch->send = &shared2->ch0;
		ch->recv = &shared2->ch1;
	} else {
		struct smd_shared_v2 *shared2;
		shared2 = smem_alloc(SMEM_SMD_BASE_ID + ch->n,
							sizeof(*shared2));
		if (!shared2) {
			SMD_INFO("smem_alloc failed ch=%d\n", ch->n);
			return -EINVAL;
		}
		ch->send = &shared2->ch0;
		ch->recv = &shared2->ch1;
	}
	ch->half_ch = get_half_ch_funcs(ch->type);

	buffer = smem_get_entry(SMEM_SMD_FIFO_BASE_ID + ch->n, &buffer_sz);
	if (!buffer) {
		SMD_INFO("smem_get_entry failed\n");
		return -EINVAL;
	}

	/* buffer must be a power-of-two size */
	if (buffer_sz & (buffer_sz - 1)) {
		SMD_INFO("Buffer size: %u not power of two\n", buffer_sz);
		return -EINVAL;
	}
	buffer_sz /= 2;
	ch->send_data = buffer;
	ch->recv_data = buffer + buffer_sz;
	ch->fifo_size = buffer_sz;

	return 0;
}

static int smd_alloc_v1(struct smd_channel *ch)
{
	return -EINVAL;
}

#else /* define v1 for older targets */
static int smd_alloc_v2(struct smd_channel *ch)
{
	return -EINVAL;
}

static int smd_alloc_v1(struct smd_channel *ch)
{
	struct smd_shared_v1 *shared1;
	shared1 = smem_alloc(ID_SMD_CHANNELS + ch->n, sizeof(*shared1));
	if (!shared1) {
		pr_err("smd_alloc_channel() cid %d does not exist\n", ch->n);
		return -EINVAL;
	}
	ch->send = &shared1->ch0;
	ch->recv = &shared1->ch1;
	ch->send_data = shared1->data0;
	ch->recv_data = shared1->data1;
	ch->fifo_size = SMD_BUF_SIZE;
	ch->half_ch = get_half_ch_funcs(ch->type);
	return 0;
}

#endif

static int smd_alloc_channel(struct smd_alloc_elm *alloc_elm)
{
	struct smd_channel *ch;

	ch = kzalloc(sizeof(struct smd_channel), GFP_KERNEL);
	if (ch == 0) {
		pr_err("smd_alloc_channel() out of memory\n");
		return -1;
	}
	ch->n = alloc_elm->cid;
	ch->type = SMD_CHANNEL_TYPE(alloc_elm->type);

	if (smd_alloc_v2(ch) && smd_alloc_v1(ch)) {
		kfree(ch);
		return -1;
	}

	ch->fifo_mask = ch->fifo_size - 1;

	/* probe_worker guarentees ch->type will be a valid type */
	if (ch->type == SMD_APPS_MODEM)
		ch->notify_other_cpu = notify_modem_smd;
	else if (ch->type == SMD_APPS_QDSP)
		ch->notify_other_cpu = notify_dsp_smd;
	else if (ch->type == SMD_APPS_DSPS)
		ch->notify_other_cpu = notify_dsps_smd;
	else if (ch->type == SMD_APPS_WCNSS)
		ch->notify_other_cpu = notify_wcnss_smd;
	else if (ch->type == SMD_APPS_RPM)
		ch->notify_other_cpu = notify_rpm_smd;

	if (smd_is_packet(alloc_elm)) {
		ch->read = smd_packet_read;
		ch->write = smd_packet_write;
		ch->read_avail = smd_packet_read_avail;
		ch->write_avail = smd_packet_write_avail;
		ch->update_state = update_packet_state;
		ch->read_from_cb = smd_packet_read_from_cb;
		ch->is_pkt_ch = 1;
	} else {
		ch->read = smd_stream_read;
		ch->write = smd_stream_write;
		ch->read_avail = smd_stream_read_avail;
		ch->write_avail = smd_stream_write_avail;
		ch->update_state = update_stream_state;
		ch->read_from_cb = smd_stream_read;
	}

	memcpy(ch->name, alloc_elm->name, SMD_MAX_CH_NAME_LEN);
	ch->name[SMD_MAX_CH_NAME_LEN-1] = 0;

	ch->pdev.name = ch->name;
	ch->pdev.id = ch->type;

	SMD_INFO("smd_alloc_channel() '%s' cid=%d\n",
		 ch->name, ch->n);

	mutex_lock(&smd_creation_mutex);
	list_add(&ch->ch_list, &smd_ch_closed_list);
	mutex_unlock(&smd_creation_mutex);

	platform_device_register(&ch->pdev);
	if (!strncmp(ch->name, "LOOPBACK", 8) && ch->type == SMD_APPS_MODEM) {
		/* create a platform driver to be used by smd_tty driver
		 * so that it can access the loopback port
		 */
		loopback_tty_pdev.id = ch->type;
		platform_device_register(&loopback_tty_pdev);
	}
	return 0;
}

static inline void notify_loopback_smd(void)
{
	unsigned long flags;
	struct smd_channel *ch;

	spin_lock_irqsave(&smd_lock, flags);
	list_for_each_entry(ch, &smd_ch_list_loopback, ch_list) {
		ch->notify(ch->priv, SMD_EVENT_DATA);
	}
	spin_unlock_irqrestore(&smd_lock, flags);
}

static int smd_alloc_loopback_channel(void)
{
	static struct smd_half_channel smd_loopback_ctl;
	static char smd_loopback_data[SMD_BUF_SIZE];
	struct smd_channel *ch;

	ch = kzalloc(sizeof(struct smd_channel), GFP_KERNEL);
	if (ch == 0) {
		pr_err("%s: out of memory\n", __func__);
		return -1;
	}
	ch->n = SMD_LOOPBACK_CID;

	ch->send = &smd_loopback_ctl;
	ch->recv = &smd_loopback_ctl;
	ch->send_data = smd_loopback_data;
	ch->recv_data = smd_loopback_data;
	ch->fifo_size = SMD_BUF_SIZE;

	ch->fifo_mask = ch->fifo_size - 1;
	ch->type = SMD_LOOPBACK_TYPE;
	ch->notify_other_cpu = notify_loopback_smd;

	ch->read = smd_stream_read;
	ch->write = smd_stream_write;
	ch->read_avail = smd_stream_read_avail;
	ch->write_avail = smd_stream_write_avail;
	ch->update_state = update_stream_state;
	ch->read_from_cb = smd_stream_read;

	memset(ch->name, 0, 20);
	memcpy(ch->name, "local_loopback", 14);

	ch->pdev.name = ch->name;
	ch->pdev.id = ch->type;

	SMD_INFO("%s: '%s' cid=%d\n", __func__, ch->name, ch->n);

	mutex_lock(&smd_creation_mutex);
	list_add(&ch->ch_list, &smd_ch_closed_list);
	mutex_unlock(&smd_creation_mutex);

	platform_device_register(&ch->pdev);
	return 0;
}

static void do_nothing_notify(void *priv, unsigned flags)
{
}

static void finalize_channel_close_fn(struct work_struct *work)
{
	unsigned long flags;
	struct smd_channel *ch;
	struct smd_channel *index;

	mutex_lock(&smd_creation_mutex);
	spin_lock_irqsave(&smd_lock, flags);
	list_for_each_entry_safe(ch, index,  &smd_ch_to_close_list, ch_list) {
		list_del(&ch->ch_list);
		list_add(&ch->ch_list, &smd_ch_closed_list);
		ch->notify(ch->priv, SMD_EVENT_REOPEN_READY);
		ch->notify = do_nothing_notify;
	}
	spin_unlock_irqrestore(&smd_lock, flags);
	mutex_unlock(&smd_creation_mutex);
}

struct smd_channel *smd_get_channel(const char *name, uint32_t type)
{
	struct smd_channel *ch;

	mutex_lock(&smd_creation_mutex);
	list_for_each_entry(ch, &smd_ch_closed_list, ch_list) {
		if (!strcmp(name, ch->name) &&
			(type == ch->type)) {
			list_del(&ch->ch_list);
			mutex_unlock(&smd_creation_mutex);
			return ch;
		}
	}
	mutex_unlock(&smd_creation_mutex);

	return NULL;
}

int smd_named_open_on_edge(const char *name, uint32_t edge,
			   smd_channel_t **_ch,
			   void *priv, void (*notify)(void *, unsigned))
{
	struct smd_channel *ch;
	unsigned long flags;

	if (smd_initialized == 0) {
		SMD_INFO("smd_open() before smd_init()\n");
		return -ENODEV;
	}

	SMD_DBG("smd_open('%s', %p, %p)\n", name, priv, notify);

	ch = smd_get_channel(name, edge);
	if (!ch) {
		/* check closing list for port */
		spin_lock_irqsave(&smd_lock, flags);
		list_for_each_entry(ch, &smd_ch_closing_list, ch_list) {
			if (!strncmp(name, ch->name, 20) &&
				(edge == ch->type)) {
				/* channel exists, but is being closed */
				spin_unlock_irqrestore(&smd_lock, flags);
				return -EAGAIN;
			}
		}

		/* check closing workqueue list for port */
		list_for_each_entry(ch, &smd_ch_to_close_list, ch_list) {
			if (!strncmp(name, ch->name, 20) &&
				(edge == ch->type)) {
				/* channel exists, but is being closed */
				spin_unlock_irqrestore(&smd_lock, flags);
				return -EAGAIN;
			}
		}
		spin_unlock_irqrestore(&smd_lock, flags);

		/* one final check to handle closing->closed race condition */
		ch = smd_get_channel(name, edge);
		if (!ch)
			return -ENODEV;
	}

	if (notify == 0)
		notify = do_nothing_notify;

	ch->notify = notify;
	ch->current_packet = 0;
	ch->last_state = SMD_SS_CLOSED;
	ch->priv = priv;

	if (edge == SMD_LOOPBACK_TYPE) {
		ch->last_state = SMD_SS_OPENED;
		ch->half_ch->set_state(ch->send, SMD_SS_OPENED);
		ch->half_ch->set_fDSR(ch->send, 1);
		ch->half_ch->set_fCTS(ch->send, 1);
		ch->half_ch->set_fCD(ch->send, 1);
	}

	*_ch = ch;

	SMD_DBG("smd_open: opening '%s'\n", ch->name);

	spin_lock_irqsave(&smd_lock, flags);
	if (SMD_CHANNEL_TYPE(ch->type) == SMD_APPS_MODEM)
		list_add(&ch->ch_list, &smd_ch_list_modem);
	else if (SMD_CHANNEL_TYPE(ch->type) == SMD_APPS_QDSP)
		list_add(&ch->ch_list, &smd_ch_list_dsp);
	else if (SMD_CHANNEL_TYPE(ch->type) == SMD_APPS_DSPS)
		list_add(&ch->ch_list, &smd_ch_list_dsps);
	else if (SMD_CHANNEL_TYPE(ch->type) == SMD_APPS_WCNSS)
		list_add(&ch->ch_list, &smd_ch_list_wcnss);
	else if (SMD_CHANNEL_TYPE(ch->type) == SMD_APPS_RPM)
		list_add(&ch->ch_list, &smd_ch_list_rpm);
	else
		list_add(&ch->ch_list, &smd_ch_list_loopback);

	SMD_DBG("%s: opening ch %d\n", __func__, ch->n);

	if (edge != SMD_LOOPBACK_TYPE)
		smd_state_change(ch, ch->last_state, SMD_SS_OPENING);

	spin_unlock_irqrestore(&smd_lock, flags);

	return 0;
}
EXPORT_SYMBOL(smd_named_open_on_edge);


int smd_open(const char *name, smd_channel_t **_ch,
	     void *priv, void (*notify)(void *, unsigned))
{
	return smd_named_open_on_edge(name, SMD_APPS_MODEM, _ch, priv,
				      notify);
}
EXPORT_SYMBOL(smd_open);

int smd_close(smd_channel_t *ch)
{
	unsigned long flags;

	if (ch == 0)
		return -1;

	SMD_INFO("smd_close(%s)\n", ch->name);

	spin_lock_irqsave(&smd_lock, flags);
	list_del(&ch->ch_list);
	if (ch->n == SMD_LOOPBACK_CID) {
		ch->half_ch->set_fDSR(ch->send, 0);
		ch->half_ch->set_fCTS(ch->send, 0);
		ch->half_ch->set_fCD(ch->send, 0);
		ch->half_ch->set_state(ch->send, SMD_SS_CLOSED);
	} else
		ch_set_state(ch, SMD_SS_CLOSED);

	if (ch->half_ch->get_state(ch->recv) == SMD_SS_OPENED) {
		list_add(&ch->ch_list, &smd_ch_closing_list);
		spin_unlock_irqrestore(&smd_lock, flags);
	} else {
		spin_unlock_irqrestore(&smd_lock, flags);
		ch->notify = do_nothing_notify;
		mutex_lock(&smd_creation_mutex);
		list_add(&ch->ch_list, &smd_ch_closed_list);
		mutex_unlock(&smd_creation_mutex);
	}

	return 0;
}
EXPORT_SYMBOL(smd_close);

int smd_write_start(smd_channel_t *ch, int len)
{
	int ret;
	unsigned hdr[5];

	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}
	if (!ch->is_pkt_ch) {
		pr_err("%s: non-packet channel specified\n", __func__);
		return -EACCES;
	}
	if (len < 1) {
		pr_err("%s: invalid length: %d\n", __func__, len);
		return -EINVAL;
	}

	if (ch->pending_pkt_sz) {
		pr_err("%s: packet of size: %d in progress\n", __func__,
			ch->pending_pkt_sz);
		return -EBUSY;
	}
	ch->pending_pkt_sz = len;

	if (smd_stream_write_avail(ch) < (SMD_HEADER_SIZE)) {
		ch->pending_pkt_sz = 0;
		SMD_DBG("%s: no space to write packet header\n", __func__);
		return -EAGAIN;
	}

	hdr[0] = len;
	hdr[1] = hdr[2] = hdr[3] = hdr[4] = 0;


	ret = smd_stream_write(ch, hdr, sizeof(hdr), 0);
	if (ret < 0 || ret != sizeof(hdr)) {
		ch->pending_pkt_sz = 0;
		pr_err("%s: packet header failed to write\n", __func__);
		return -EPERM;
	}
	return 0;
}
EXPORT_SYMBOL(smd_write_start);

int smd_write_segment(smd_channel_t *ch, void *data, int len, int user_buf)
{
	int bytes_written;

	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}
	if (len < 1) {
		pr_err("%s: invalid length: %d\n", __func__, len);
		return -EINVAL;
	}

	if (!ch->pending_pkt_sz) {
		pr_err("%s: no transaction in progress\n", __func__);
		return -ENOEXEC;
	}
	if (ch->pending_pkt_sz - len < 0) {
		pr_err("%s: segment of size: %d will make packet go over "
			"length\n", __func__, len);
		return -EINVAL;
	}

	bytes_written = smd_stream_write(ch, data, len, user_buf);

	ch->pending_pkt_sz -= bytes_written;

	return bytes_written;
}
EXPORT_SYMBOL(smd_write_segment);

int smd_write_end(smd_channel_t *ch)
{

	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}
	if (ch->pending_pkt_sz) {
		pr_err("%s: current packet not completely written\n", __func__);
		return -E2BIG;
	}

	return 0;
}
EXPORT_SYMBOL(smd_write_end);

int smd_read(smd_channel_t *ch, void *data, int len)
{
	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}

	return ch->read(ch, data, len, 0);
}
EXPORT_SYMBOL(smd_read);

int smd_read_user_buffer(smd_channel_t *ch, void *data, int len)
{
	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}

	return ch->read(ch, data, len, 1);
}
EXPORT_SYMBOL(smd_read_user_buffer);

int smd_read_from_cb(smd_channel_t *ch, void *data, int len)
{
	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}

	return ch->read_from_cb(ch, data, len, 0);
}
EXPORT_SYMBOL(smd_read_from_cb);

int smd_write(smd_channel_t *ch, const void *data, int len)
{
	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}

	return ch->pending_pkt_sz ? -EBUSY : ch->write(ch, data, len, 0);
}
EXPORT_SYMBOL(smd_write);

int smd_write_user_buffer(smd_channel_t *ch, const void *data, int len)
{
	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}

	return ch->pending_pkt_sz ? -EBUSY : ch->write(ch, data, len, 1);
}
EXPORT_SYMBOL(smd_write_user_buffer);

int smd_read_avail(smd_channel_t *ch)
{
	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}

	return ch->read_avail(ch);
}
EXPORT_SYMBOL(smd_read_avail);

int smd_write_avail(smd_channel_t *ch)
{
	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}

	return ch->write_avail(ch);
}
EXPORT_SYMBOL(smd_write_avail);

void smd_enable_read_intr(smd_channel_t *ch)
{
	if (ch)
		ch->half_ch->set_fBLOCKREADINTR(ch->send, 0);
}
EXPORT_SYMBOL(smd_enable_read_intr);

void smd_disable_read_intr(smd_channel_t *ch)
{
	if (ch)
		ch->half_ch->set_fBLOCKREADINTR(ch->send, 1);
}
EXPORT_SYMBOL(smd_disable_read_intr);

int smd_wait_until_readable(smd_channel_t *ch, int bytes)
{
	return -1;
}

int smd_wait_until_writable(smd_channel_t *ch, int bytes)
{
	return -1;
}

int smd_cur_packet_size(smd_channel_t *ch)
{
	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}

	return ch->current_packet;
}
EXPORT_SYMBOL(smd_cur_packet_size);

int smd_tiocmget(smd_channel_t *ch)
{
	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}

	return  (ch->half_ch->get_fDSR(ch->recv) ? TIOCM_DSR : 0) |
		(ch->half_ch->get_fCTS(ch->recv) ? TIOCM_CTS : 0) |
		(ch->half_ch->get_fCD(ch->recv) ? TIOCM_CD : 0) |
		(ch->half_ch->get_fRI(ch->recv) ? TIOCM_RI : 0) |
		(ch->half_ch->get_fCTS(ch->send) ? TIOCM_RTS : 0) |
		(ch->half_ch->get_fDSR(ch->send) ? TIOCM_DTR : 0);
}
EXPORT_SYMBOL(smd_tiocmget);

/* this api will be called while holding smd_lock */
int
smd_tiocmset_from_cb(smd_channel_t *ch, unsigned int set, unsigned int clear)
{
	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}

	if (set & TIOCM_DTR)
		ch->half_ch->set_fDSR(ch->send, 1);

	if (set & TIOCM_RTS)
		ch->half_ch->set_fCTS(ch->send, 1);

	if (clear & TIOCM_DTR)
		ch->half_ch->set_fDSR(ch->send, 0);

	if (clear & TIOCM_RTS)
		ch->half_ch->set_fCTS(ch->send, 0);

	ch->half_ch->set_fSTATE(ch->send, 1);
	barrier();
	ch->notify_other_cpu();

	return 0;
}
EXPORT_SYMBOL(smd_tiocmset_from_cb);

int smd_tiocmset(smd_channel_t *ch, unsigned int set, unsigned int clear)
{
	unsigned long flags;

	if (!ch) {
		pr_err("%s: Invalid channel specified\n", __func__);
		return -ENODEV;
	}

	spin_lock_irqsave(&smd_lock, flags);
	smd_tiocmset_from_cb(ch, set, clear);
	spin_unlock_irqrestore(&smd_lock, flags);

	return 0;
}
EXPORT_SYMBOL(smd_tiocmset);

int smd_is_pkt_avail(smd_channel_t *ch)
{
	if (!ch || !ch->is_pkt_ch)
		return -EINVAL;

	if (ch->current_packet)
		return 1;

	update_packet_state(ch);

	return ch->current_packet ? 1 : 0;
}
EXPORT_SYMBOL(smd_is_pkt_avail);


/* -------------------------------------------------------------------------- */

/*
 * Shared Memory Range Check
 *
 * Takes a physical address and an offset and checks if the resulting physical
 * address would fit into one of the aux smem regions.  If so, returns the
 * corresponding virtual address.  Otherwise returns NULL.  Expects the array
 * of smem regions to be in ascending physical address order.
 *
 * @base: physical base address to check
 * @offset: offset from the base to get the final address
 */
static void *smem_range_check(void *base, unsigned offset)
{
	int i;
	void *phys_addr;
	unsigned size;

	for (i = 0; i < num_smem_areas; ++i) {
		phys_addr = smem_areas[i].phys_addr;
		size = smem_areas[i].size;
		if (base < phys_addr)
			return NULL;
		if (base > phys_addr + size)
			continue;
		if (base >= phys_addr && base + offset < phys_addr + size)
			return smem_areas[i].virt_addr + offset;
	}

	return NULL;
}

/* smem_alloc returns the pointer to smem item if it is already allocated.
 * Otherwise, it returns NULL.
 */
void *smem_alloc(unsigned id, unsigned size)
{
	return smem_find(id, size);
}
EXPORT_SYMBOL(smem_alloc);

/* smem_alloc2 returns the pointer to smem item.  If it is not allocated,
 * it allocates it and then returns the pointer to it.
 */
void *smem_alloc2(unsigned id, unsigned size_in)
{
	struct smem_shared *shared = (void *) MSM_SHARED_RAM_BASE;
	struct smem_heap_entry *toc = shared->heap_toc;
	unsigned long flags;
	void *ret = NULL;

	if (!shared->heap_info.initialized) {
		pr_err("%s: smem heap info not initialized\n", __func__);
		return NULL;
	}

	if (id >= SMEM_NUM_ITEMS)
		return NULL;

	size_in = ALIGN(size_in, 8);
	remote_spin_lock_irqsave(&remote_spinlock, flags);
	if (toc[id].allocated) {
		SMD_DBG("%s: %u already allocated\n", __func__, id);
		if (size_in != toc[id].size)
			pr_err("%s: wrong size %u (expected %u)\n",
			       __func__, toc[id].size, size_in);
		else
			ret = (void *)(MSM_SHARED_RAM_BASE + toc[id].offset);
	} else if (id > SMEM_FIXED_ITEM_LAST) {
		SMD_DBG("%s: allocating %u\n", __func__, id);
		if (shared->heap_info.heap_remaining >= size_in) {
			toc[id].offset = shared->heap_info.free_offset;
			toc[id].size = size_in;
			wmb();
			toc[id].allocated = 1;

			shared->heap_info.free_offset += size_in;
			shared->heap_info.heap_remaining -= size_in;
			ret = (void *)(MSM_SHARED_RAM_BASE + toc[id].offset);
		} else
			pr_err("%s: not enough memory %u (required %u)\n",
			       __func__, shared->heap_info.heap_remaining,
			       size_in);
	}
	wmb();
	remote_spin_unlock_irqrestore(&remote_spinlock, flags);
	return ret;
}
EXPORT_SYMBOL(smem_alloc2);

void *smem_get_entry(unsigned id, unsigned *size)
{
	struct smem_shared *shared = (void *) MSM_SHARED_RAM_BASE;
	struct smem_heap_entry *toc = shared->heap_toc;
	int use_spinlocks = spinlocks_initialized;
	void *ret = 0;
	unsigned long flags = 0;

	if (id >= SMEM_NUM_ITEMS)
		return ret;

	if (use_spinlocks)
		remote_spin_lock_irqsave(&remote_spinlock, flags);
	/* toc is in device memory and cannot be speculatively accessed */
	if (toc[id].allocated) {
		*size = toc[id].size;
		barrier();
		if (!(toc[id].reserved & BASE_ADDR_MASK))
			ret = (void *) (MSM_SHARED_RAM_BASE + toc[id].offset);
		else
			ret = smem_range_check(
				(void *)(toc[id].reserved & BASE_ADDR_MASK),
				toc[id].offset);
	} else {
		*size = 0;
	}
	if (use_spinlocks)
		remote_spin_unlock_irqrestore(&remote_spinlock, flags);

	return ret;
}
EXPORT_SYMBOL(smem_get_entry);

void *smem_find(unsigned id, unsigned size_in)
{
	unsigned size;
	void *ptr;

	ptr = smem_get_entry(id, &size);
	if (!ptr)
		return 0;

	size_in = ALIGN(size_in, 8);
	if (size_in != size) {
		pr_err("smem_find(%d, %d): wrong size %d\n",
		       id, size_in, size);
		return 0;
	}

	return ptr;
}
EXPORT_SYMBOL(smem_find);

static int smsm_cb_init(void)
{
	struct smsm_state_info *state_info;
	int n;
	int ret = 0;

	smsm_states = kmalloc(sizeof(struct smsm_state_info)*SMSM_NUM_ENTRIES,
		   GFP_KERNEL);

	if (!smsm_states) {
		pr_err("%s: SMSM init failed\n", __func__);
		return -ENOMEM;
	}

	smsm_cb_wq = create_singlethread_workqueue("smsm_cb_wq");
	if (!smsm_cb_wq) {
		pr_err("%s: smsm_cb_wq creation failed\n", __func__);
		kfree(smsm_states);
		return -EFAULT;
	}

	mutex_lock(&smsm_lock);
	for (n = 0; n < SMSM_NUM_ENTRIES; n++) {
		state_info = &smsm_states[n];
		state_info->last_value = __raw_readl(SMSM_STATE_ADDR(n));
		state_info->intr_mask_set = 0x0;
		state_info->intr_mask_clear = 0x0;
		INIT_LIST_HEAD(&state_info->callbacks);
	}
	mutex_unlock(&smsm_lock);

	return ret;
}

static int smsm_init(void)
{
	struct smem_shared *shared = (void *) MSM_SHARED_RAM_BASE;
	int i;
	struct smsm_size_info_type *smsm_size_info;

	i = remote_spin_lock_init(&remote_spinlock, SMEM_SPINLOCK_SMEM_ALLOC);
	if (i) {
		pr_err("%s: remote spinlock init failed %d\n", __func__, i);
		return i;
	}
	spinlocks_initialized = 1;

	smsm_size_info = smem_alloc(SMEM_SMSM_SIZE_INFO,
				sizeof(struct smsm_size_info_type));
	if (smsm_size_info) {
		SMSM_NUM_ENTRIES = smsm_size_info->num_entries;
		SMSM_NUM_HOSTS = smsm_size_info->num_hosts;
	}

	i = kfifo_alloc(&smsm_snapshot_fifo,
			sizeof(uint32_t) * SMSM_NUM_ENTRIES * SMSM_SNAPSHOT_CNT,
			GFP_KERNEL);
	if (i) {
		pr_err("%s: SMSM state fifo alloc failed %d\n", __func__, i);
		return i;
	}
	wake_lock_init(&smsm_snapshot_wakelock, WAKE_LOCK_SUSPEND,
			"smsm_snapshot");

	if (!smsm_info.state) {
		smsm_info.state = smem_alloc2(ID_SHARED_STATE,
					      SMSM_NUM_ENTRIES *
					      sizeof(uint32_t));

		if (smsm_info.state) {
			__raw_writel(0, SMSM_STATE_ADDR(SMSM_APPS_STATE));
			if ((shared->version[VERSION_MODEM] >> 16) >= 0xB)
				__raw_writel(0, \
					SMSM_STATE_ADDR(SMSM_APPS_DEM_I));
		}
	}

	if (!smsm_info.intr_mask) {
		smsm_info.intr_mask = smem_alloc2(SMEM_SMSM_CPU_INTR_MASK,
						  SMSM_NUM_ENTRIES *
						  SMSM_NUM_HOSTS *
						  sizeof(uint32_t));

		if (smsm_info.intr_mask) {
			for (i = 0; i < SMSM_NUM_ENTRIES; i++)
				__raw_writel(0x0,
					SMSM_INTR_MASK_ADDR(i, SMSM_APPS));

			/* Configure legacy modem bits */
			__raw_writel(LEGACY_MODEM_SMSM_MASK,
				SMSM_INTR_MASK_ADDR(SMSM_MODEM_STATE,
					SMSM_APPS));
		}
	}

	if (!smsm_info.intr_mux)
		smsm_info.intr_mux = smem_alloc2(SMEM_SMD_SMSM_INTR_MUX,
						 SMSM_NUM_INTR_MUX *
						 sizeof(uint32_t));

	i = smsm_cb_init();
	if (i)
		return i;

	wmb();
	smsm_driver_state_notify(SMSM_INIT, NULL);
	return 0;
}

void smsm_reset_modem(unsigned mode)
{
	if (mode == SMSM_SYSTEM_DOWNLOAD) {
		mode = SMSM_RESET | SMSM_SYSTEM_DOWNLOAD;
	} else if (mode == SMSM_MODEM_WAIT) {
		mode = SMSM_RESET | SMSM_MODEM_WAIT;
	} else { /* reset_mode is SMSM_RESET or default */
		mode = SMSM_RESET;
	}

	smsm_change_state(SMSM_APPS_STATE, mode, mode);
}
EXPORT_SYMBOL(smsm_reset_modem);

void smsm_reset_modem_cont(void)
{
	unsigned long flags;
	uint32_t state;

	if (!smsm_info.state)
		return;

	spin_lock_irqsave(&smem_lock, flags);
	state = __raw_readl(SMSM_STATE_ADDR(SMSM_APPS_STATE)) \
						& ~SMSM_MODEM_WAIT;
	__raw_writel(state, SMSM_STATE_ADDR(SMSM_APPS_STATE));
	wmb();
	spin_unlock_irqrestore(&smem_lock, flags);
}
EXPORT_SYMBOL(smsm_reset_modem_cont);

static void smsm_cb_snapshot(uint32_t use_wakelock)
{
	int n;
	uint32_t new_state;
	unsigned long flags;
	int ret;

	ret = kfifo_avail(&smsm_snapshot_fifo);
	if (ret < SMSM_SNAPSHOT_SIZE) {
		pr_err("%s: SMSM snapshot full %d\n", __func__, ret);
		return;
	}

	/*
	 * To avoid a race condition with notify_smsm_cb_clients_worker, the
	 * following sequence must be followed:
	 *   1) increment snapshot count
	 *   2) insert data into FIFO
	 *
	 *   Potentially in parallel, the worker:
	 *   a) verifies >= 1 snapshots are in FIFO
	 *   b) processes snapshot
	 *   c) decrements reference count
	 *
	 *   This order ensures that 1 will always occur before abc.
	 */
	if (use_wakelock) {
		spin_lock_irqsave(&smsm_snapshot_count_lock, flags);
		if (smsm_snapshot_count == 0) {
			SMx_POWER_INFO("SMSM snapshot wake lock\n");
			wake_lock(&smsm_snapshot_wakelock);
		}
		++smsm_snapshot_count;
		spin_unlock_irqrestore(&smsm_snapshot_count_lock, flags);
	}

	/* queue state entries */
	for (n = 0; n < SMSM_NUM_ENTRIES; n++) {
		new_state = __raw_readl(SMSM_STATE_ADDR(n));

		ret = kfifo_in(&smsm_snapshot_fifo,
				&new_state, sizeof(new_state));
		if (ret != sizeof(new_state)) {
			pr_err("%s: SMSM snapshot failure %d\n", __func__, ret);
			goto restore_snapshot_count;
		}
	}

	/* queue wakelock usage flag */
	ret = kfifo_in(&smsm_snapshot_fifo,
			&use_wakelock, sizeof(use_wakelock));
	if (ret != sizeof(use_wakelock)) {
		pr_err("%s: SMSM snapshot failure %d\n", __func__, ret);
		goto restore_snapshot_count;
	}

	queue_work(smsm_cb_wq, &smsm_cb_work);
	return;

restore_snapshot_count:
	if (use_wakelock) {
		spin_lock_irqsave(&smsm_snapshot_count_lock, flags);
		if (smsm_snapshot_count) {
			--smsm_snapshot_count;
			if (smsm_snapshot_count == 0) {
				SMx_POWER_INFO("SMSM snapshot wake unlock\n");
				wake_unlock(&smsm_snapshot_wakelock);
			}
		} else {
			pr_err("%s: invalid snapshot count\n", __func__);
		}
		spin_unlock_irqrestore(&smsm_snapshot_count_lock, flags);
	}
}

static irqreturn_t smsm_irq_handler(int irq, void *data)
{
	unsigned long flags;

	if (irq == INT_ADSP_A11_SMSM) {
		uint32_t mux_val;
		static uint32_t prev_smem_q6_apps_smsm;

		if (smsm_info.intr_mux && cpu_is_qsd8x50()) {
			mux_val = __raw_readl(
					SMSM_INTR_MUX_ADDR(SMEM_Q6_APPS_SMSM));
			if (mux_val != prev_smem_q6_apps_smsm)
				prev_smem_q6_apps_smsm = mux_val;
		}

		spin_lock_irqsave(&smem_lock, flags);
		smsm_cb_snapshot(1);
		spin_unlock_irqrestore(&smem_lock, flags);
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&smem_lock, flags);
	if (!smsm_info.state) {
		SMSM_INFO("<SM NO STATE>\n");
	} else {
		unsigned old_apps, apps;
		unsigned modm = __raw_readl(SMSM_STATE_ADDR(SMSM_MODEM_STATE));

		old_apps = apps = __raw_readl(SMSM_STATE_ADDR(SMSM_APPS_STATE));

		SMSM_DBG("<SM %08x %08x>\n", apps, modm);
		if (apps & SMSM_RESET) {
			/* If we get an interrupt and the apps SMSM_RESET
			   bit is already set, the modem is acking the
			   app's reset ack. */
			if (!disable_smsm_reset_handshake)
				apps &= ~SMSM_RESET;
			/* Issue a fake irq to handle any
			 * smd state changes during reset
			 */
			smd_fake_irq_handler(0);

			/* queue modem restart notify chain */
			modem_queue_start_reset_notify();

		} else if (modm & SMSM_RESET) {
			pr_err("\nSMSM: Modem SMSM state changed to SMSM_RESET.");
			if (!disable_smsm_reset_handshake) {
				apps |= SMSM_RESET;
				flush_cache_all();
				outer_flush_all();
			}
			modem_queue_start_reset_notify();

		} else if (modm & SMSM_INIT) {
			if (!(apps & SMSM_INIT)) {
				apps |= SMSM_INIT;
				modem_queue_smsm_init_notify();
			}

			if (modm & SMSM_SMDINIT)
				apps |= SMSM_SMDINIT;
			if ((apps & (SMSM_INIT | SMSM_SMDINIT | SMSM_RPCINIT)) ==
				(SMSM_INIT | SMSM_SMDINIT | SMSM_RPCINIT))
				apps |= SMSM_RUN;
		} else if (modm & SMSM_SYSTEM_DOWNLOAD) {
			pr_err("\nSMSM: Modem SMSM state changed to SMSM_SYSTEM_DOWNLOAD.");
			modem_queue_start_reset_notify();
		}

		if (old_apps != apps) {
			SMSM_DBG("<SM %08x NOTIFY>\n", apps);
			__raw_writel(apps, SMSM_STATE_ADDR(SMSM_APPS_STATE));
			do_smd_probe();
			notify_other_smsm(SMSM_APPS_STATE, (old_apps ^ apps));
		}

		smsm_cb_snapshot(1);
	}
	spin_unlock_irqrestore(&smem_lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t smsm_modem_irq_handler(int irq, void *data)
{
	SMx_POWER_INFO("SMSM Int Modem->Apps\n");
	++interrupt_stats[SMD_MODEM].smsm_in_count;
	return smsm_irq_handler(irq, data);
}

static irqreturn_t smsm_dsp_irq_handler(int irq, void *data)
{
	SMx_POWER_INFO("SMSM Int LPASS->Apps\n");
	++interrupt_stats[SMD_Q6].smsm_in_count;
	return smsm_irq_handler(irq, data);
}

static irqreturn_t smsm_dsps_irq_handler(int irq, void *data)
{
	SMx_POWER_INFO("SMSM Int DSPS->Apps\n");
	++interrupt_stats[SMD_DSPS].smsm_in_count;
	return smsm_irq_handler(irq, data);
}

static irqreturn_t smsm_wcnss_irq_handler(int irq, void *data)
{
	SMx_POWER_INFO("SMSM Int WCNSS->Apps\n");
	++interrupt_stats[SMD_WCNSS].smsm_in_count;
	return smsm_irq_handler(irq, data);
}

/*
 * Changes the global interrupt mask.  The set and clear masks are re-applied
 * every time the global interrupt mask is updated for callback registration
 * and de-registration.
 *
 * The clear mask is applied first, so if a bit is set to 1 in both the clear
 * mask and the set mask, the result will be that the interrupt is set.
 *
 * @smsm_entry  SMSM entry to change
 * @clear_mask  1 = clear bit, 0 = no-op
 * @set_mask    1 = set bit, 0 = no-op
 *
 * @returns 0 for success, < 0 for error
 */
int smsm_change_intr_mask(uint32_t smsm_entry,
			  uint32_t clear_mask, uint32_t set_mask)
{
	uint32_t  old_mask, new_mask;
	unsigned long flags;

	if (smsm_entry >= SMSM_NUM_ENTRIES) {
		pr_err("smsm_change_state: Invalid entry %d\n",
		       smsm_entry);
		return -EINVAL;
	}

	if (!smsm_info.intr_mask) {
		pr_err("smsm_change_intr_mask <SM NO STATE>\n");
		return -EIO;
	}

	spin_lock_irqsave(&smem_lock, flags);
	smsm_states[smsm_entry].intr_mask_clear = clear_mask;
	smsm_states[smsm_entry].intr_mask_set = set_mask;

	old_mask = __raw_readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_APPS));
	new_mask = (old_mask & ~clear_mask) | set_mask;
	__raw_writel(new_mask, SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_APPS));

	wmb();
	spin_unlock_irqrestore(&smem_lock, flags);

	return 0;
}
EXPORT_SYMBOL(smsm_change_intr_mask);

int smsm_get_intr_mask(uint32_t smsm_entry, uint32_t *intr_mask)
{
	if (smsm_entry >= SMSM_NUM_ENTRIES) {
		pr_err("smsm_change_state: Invalid entry %d\n",
		       smsm_entry);
		return -EINVAL;
	}

	if (!smsm_info.intr_mask) {
		pr_err("smsm_change_intr_mask <SM NO STATE>\n");
		return -EIO;
	}

	*intr_mask = __raw_readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_APPS));
	return 0;
}
EXPORT_SYMBOL(smsm_get_intr_mask);

int smsm_change_state(uint32_t smsm_entry,
		      uint32_t clear_mask, uint32_t set_mask)
{
	unsigned long flags;
	uint32_t  old_state, new_state;

	if (smsm_entry >= SMSM_NUM_ENTRIES) {
		pr_err("smsm_change_state: Invalid entry %d",
		       smsm_entry);
		return -EINVAL;
	}

	if (!smsm_info.state) {
		pr_err("smsm_change_state <SM NO STATE>\n");
		return -EIO;
	}
	spin_lock_irqsave(&smem_lock, flags);

	old_state = __raw_readl(SMSM_STATE_ADDR(smsm_entry));
	new_state = (old_state & ~clear_mask) | set_mask;
	__raw_writel(new_state, SMSM_STATE_ADDR(smsm_entry));
	SMSM_DBG("smsm_change_state %x\n", new_state);
	notify_other_smsm(SMSM_APPS_STATE, (old_state ^ new_state));

	spin_unlock_irqrestore(&smem_lock, flags);

	return 0;
}
EXPORT_SYMBOL(smsm_change_state);

uint32_t smsm_get_state(uint32_t smsm_entry)
{
	uint32_t rv = 0;

	/* needs interface change to return error code */
	if (smsm_entry >= SMSM_NUM_ENTRIES) {
		pr_err("smsm_change_state: Invalid entry %d",
		       smsm_entry);
		return 0;
	}

	if (!smsm_info.state) {
		pr_err("smsm_get_state <SM NO STATE>\n");
	} else {
		rv = __raw_readl(SMSM_STATE_ADDR(smsm_entry));
	}

	return rv;
}
EXPORT_SYMBOL(smsm_get_state);

/**
 * Performs SMSM callback client notifiction.
 */
void notify_smsm_cb_clients_worker(struct work_struct *work)
{
	struct smsm_state_cb_info *cb_info;
	struct smsm_state_info *state_info;
	int n;
	uint32_t new_state;
	uint32_t state_changes;
	uint32_t use_wakelock;
	int ret;
	unsigned long flags;

	if (!smd_initialized)
		return;

	while (kfifo_len(&smsm_snapshot_fifo) >= SMSM_SNAPSHOT_SIZE) {
		mutex_lock(&smsm_lock);
		for (n = 0; n < SMSM_NUM_ENTRIES; n++) {
			state_info = &smsm_states[n];

			ret = kfifo_out(&smsm_snapshot_fifo, &new_state,
					sizeof(new_state));
			if (ret != sizeof(new_state)) {
				pr_err("%s: snapshot underflow %d\n",
					__func__, ret);
				mutex_unlock(&smsm_lock);
				return;
			}

			state_changes = state_info->last_value ^ new_state;
			if (state_changes) {
				SMx_POWER_INFO("SMSM Change %d: %08x->%08x\n",
						n, state_info->last_value,
						new_state);
				list_for_each_entry(cb_info,
					&state_info->callbacks, cb_list) {

					if (cb_info->mask & state_changes)
						cb_info->notify(cb_info->data,
							state_info->last_value,
							new_state);
				}
				state_info->last_value = new_state;
			}
		}

		/* read wakelock flag */
		ret = kfifo_out(&smsm_snapshot_fifo, &use_wakelock,
				sizeof(use_wakelock));
		if (ret != sizeof(use_wakelock)) {
			pr_err("%s: snapshot underflow %d\n",
				__func__, ret);
			mutex_unlock(&smsm_lock);
			return;
		}
		mutex_unlock(&smsm_lock);

		if (use_wakelock) {
			spin_lock_irqsave(&smsm_snapshot_count_lock, flags);
			if (smsm_snapshot_count) {
				--smsm_snapshot_count;
				if (smsm_snapshot_count == 0) {
					SMx_POWER_INFO("SMSM snapshot"
						   " wake unlock\n");
					wake_unlock(&smsm_snapshot_wakelock);
				}
			} else {
				pr_err("%s: invalid snapshot count\n",
						__func__);
			}
			spin_unlock_irqrestore(&smsm_snapshot_count_lock,
					flags);
		}
	}
}


/**
 * Registers callback for SMSM state notifications when the specified
 * bits change.
 *
 * @smsm_entry  Processor entry to deregister
 * @mask        Bits to deregister (if result is 0, callback is removed)
 * @notify      Notification function to deregister
 * @data        Opaque data passed in to callback
 *
 * @returns Status code
 *  <0 error code
 *  0  inserted new entry
 *  1  updated mask of existing entry
 */
int smsm_state_cb_register(uint32_t smsm_entry, uint32_t mask,
		void (*notify)(void *, uint32_t, uint32_t), void *data)
{
	struct smsm_state_info *state;
	struct smsm_state_cb_info *cb_info;
	struct smsm_state_cb_info *cb_found = 0;
	uint32_t new_mask = 0;
	int ret = 0;

	if (smsm_entry >= SMSM_NUM_ENTRIES)
		return -EINVAL;

	mutex_lock(&smsm_lock);

	if (!smsm_states) {
		/* smsm not yet initialized */
		ret = -ENODEV;
		goto cleanup;
	}

	state = &smsm_states[smsm_entry];
	list_for_each_entry(cb_info,
			&state->callbacks, cb_list) {
		if (!ret && (cb_info->notify == notify) &&
				(cb_info->data == data)) {
			cb_info->mask |= mask;
			cb_found = cb_info;
			ret = 1;
		}
		new_mask |= cb_info->mask;
	}

	if (!cb_found) {
		cb_info = kmalloc(sizeof(struct smsm_state_cb_info),
			GFP_ATOMIC);
		if (!cb_info) {
			ret = -ENOMEM;
			goto cleanup;
		}

		cb_info->mask = mask;
		cb_info->notify = notify;
		cb_info->data = data;
		INIT_LIST_HEAD(&cb_info->cb_list);
		list_add_tail(&cb_info->cb_list,
			&state->callbacks);
		new_mask |= mask;
	}

	/* update interrupt notification mask */
	if (smsm_entry == SMSM_MODEM_STATE)
		new_mask |= LEGACY_MODEM_SMSM_MASK;

	if (smsm_info.intr_mask) {
		unsigned long flags;

		spin_lock_irqsave(&smem_lock, flags);
		new_mask = (new_mask & ~state->intr_mask_clear)
				| state->intr_mask_set;
		__raw_writel(new_mask,
				SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_APPS));
		wmb();
		spin_unlock_irqrestore(&smem_lock, flags);
	}

cleanup:
	mutex_unlock(&smsm_lock);
	return ret;
}
EXPORT_SYMBOL(smsm_state_cb_register);


/**
 * Deregisters for SMSM state notifications for the specified bits.
 *
 * @smsm_entry  Processor entry to deregister
 * @mask        Bits to deregister (if result is 0, callback is removed)
 * @notify      Notification function to deregister
 * @data        Opaque data passed in to callback
 *
 * @returns Status code
 *  <0 error code
 *  0  not found
 *  1  updated mask
 *  2  removed callback
 */
int smsm_state_cb_deregister(uint32_t smsm_entry, uint32_t mask,
		void (*notify)(void *, uint32_t, uint32_t), void *data)
{
	struct smsm_state_cb_info *cb_info;
	struct smsm_state_cb_info *cb_tmp;
	struct smsm_state_info *state;
	uint32_t new_mask = 0;
	int ret = 0;

	if (smsm_entry >= SMSM_NUM_ENTRIES)
		return -EINVAL;

	mutex_lock(&smsm_lock);

	if (!smsm_states) {
		/* smsm not yet initialized */
		mutex_unlock(&smsm_lock);
		return -ENODEV;
	}

	state = &smsm_states[smsm_entry];
	list_for_each_entry_safe(cb_info, cb_tmp,
		&state->callbacks, cb_list) {
		if (!ret && (cb_info->notify == notify) &&
			(cb_info->data == data)) {
			cb_info->mask &= ~mask;
			ret = 1;
			if (!cb_info->mask) {
				/* no mask bits set, remove callback */
				list_del(&cb_info->cb_list);
				kfree(cb_info);
				ret = 2;
				continue;
			}
		}
		new_mask |= cb_info->mask;
	}

	/* update interrupt notification mask */
	if (smsm_entry == SMSM_MODEM_STATE)
		new_mask |= LEGACY_MODEM_SMSM_MASK;

	if (smsm_info.intr_mask) {
		unsigned long flags;

		spin_lock_irqsave(&smem_lock, flags);
		new_mask = (new_mask & ~state->intr_mask_clear)
				| state->intr_mask_set;
		__raw_writel(new_mask,
				SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_APPS));
		wmb();
		spin_unlock_irqrestore(&smem_lock, flags);
	}

	mutex_unlock(&smsm_lock);
	return ret;
}
EXPORT_SYMBOL(smsm_state_cb_deregister);

int smsm_driver_state_notifier_register(struct notifier_block *nb)
{
	int ret;
	if (!nb)
		return -EINVAL;
	mutex_lock(&smsm_driver_state_notifier_lock);
	ret = raw_notifier_chain_register(&smsm_driver_state_notifier_list, nb);
	mutex_unlock(&smsm_driver_state_notifier_lock);
	return ret;
}
EXPORT_SYMBOL(smsm_driver_state_notifier_register);

int smsm_driver_state_notifier_unregister(struct notifier_block *nb)
{
	int ret;
	if (!nb)
		return -EINVAL;
	mutex_lock(&smsm_driver_state_notifier_lock);
	ret = raw_notifier_chain_unregister(&smsm_driver_state_notifier_list,
					    nb);
	mutex_unlock(&smsm_driver_state_notifier_lock);
	return ret;
}
EXPORT_SYMBOL(smsm_driver_state_notifier_unregister);

static void smsm_driver_state_notify(uint32_t state, void *data)
{
	mutex_lock(&smsm_driver_state_notifier_lock);
	raw_notifier_call_chain(&smsm_driver_state_notifier_list,
				state, data);
	mutex_unlock(&smsm_driver_state_notifier_lock);
}

int smd_core_init(void)
{
	int r;
	unsigned long flags = IRQF_TRIGGER_RISING;
	SMD_INFO("smd_core_init()\n");

	r = request_irq(INT_A9_M2A_0, smd_modem_irq_handler,
			flags, "smd_dev", 0);
	if (r < 0)
		return r;
	r = enable_irq_wake(INT_A9_M2A_0);
	if (r < 0)
		pr_err("smd_core_init: "
		       "enable_irq_wake failed for INT_A9_M2A_0\n");

	r = request_irq(INT_A9_M2A_5, smsm_modem_irq_handler,
			flags, "smsm_dev", 0);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		return r;
	}
	r = enable_irq_wake(INT_A9_M2A_5);
	if (r < 0)
		pr_err("smd_core_init: "
		       "enable_irq_wake failed for INT_A9_M2A_5\n");

#if defined(CONFIG_QDSP6)
#if (INT_ADSP_A11 == INT_ADSP_A11_SMSM)
		flags |= IRQF_SHARED;
#endif
	r = request_irq(INT_ADSP_A11, smd_dsp_irq_handler,
			flags, "smd_dev", smd_dsp_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		return r;
	}

	r = request_irq(INT_ADSP_A11_SMSM, smsm_dsp_irq_handler,
			flags, "smsm_dev", smsm_dsp_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		return r;
	}

	r = enable_irq_wake(INT_ADSP_A11);
	if (r < 0)
		pr_err("smd_core_init: "
		       "enable_irq_wake failed for INT_ADSP_A11\n");

#if (INT_ADSP_A11 != INT_ADSP_A11_SMSM)
	r = enable_irq_wake(INT_ADSP_A11_SMSM);
	if (r < 0)
		pr_err("smd_core_init: enable_irq_wake "
		       "failed for INT_ADSP_A11_SMSM\n");
#endif
	flags &= ~IRQF_SHARED;
#endif

#if defined(CONFIG_DSPS)
	r = request_irq(INT_DSPS_A11, smd_dsps_irq_handler,
			flags, "smd_dev", smd_dsps_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		free_irq(INT_ADSP_A11_SMSM, smsm_dsp_irq_handler);
		return r;
	}

	r = enable_irq_wake(INT_DSPS_A11);
	if (r < 0)
		pr_err("smd_core_init: "
		       "enable_irq_wake failed for INT_ADSP_A11\n");
#endif

#if defined(CONFIG_WCNSS)
	r = request_irq(INT_WCNSS_A11, smd_wcnss_irq_handler,
			flags, "smd_dev", smd_wcnss_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		free_irq(INT_ADSP_A11_SMSM, smsm_dsp_irq_handler);
		free_irq(INT_DSPS_A11, smd_dsps_irq_handler);
		return r;
	}

	r = enable_irq_wake(INT_WCNSS_A11);
	if (r < 0)
		pr_err("smd_core_init: "
		       "enable_irq_wake failed for INT_WCNSS_A11\n");

	r = request_irq(INT_WCNSS_A11_SMSM, smsm_wcnss_irq_handler,
			flags, "smsm_dev", smsm_wcnss_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		free_irq(INT_ADSP_A11_SMSM, smsm_dsp_irq_handler);
		free_irq(INT_DSPS_A11, smd_dsps_irq_handler);
		free_irq(INT_WCNSS_A11, smd_wcnss_irq_handler);
		return r;
	}

	r = enable_irq_wake(INT_WCNSS_A11_SMSM);
	if (r < 0)
		pr_err("smd_core_init: "
		       "enable_irq_wake failed for INT_WCNSS_A11_SMSM\n");
#endif

#if defined(CONFIG_DSPS_SMSM)
	r = request_irq(INT_DSPS_A11_SMSM, smsm_dsps_irq_handler,
			flags, "smsm_dev", smsm_dsps_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		free_irq(INT_ADSP_A11_SMSM, smsm_dsp_irq_handler);
		free_irq(INT_DSPS_A11, smd_dsps_irq_handler);
		free_irq(INT_WCNSS_A11, smd_wcnss_irq_handler);
		free_irq(INT_WCNSS_A11_SMSM, smsm_wcnss_irq_handler);
		return r;
	}

	r = enable_irq_wake(INT_DSPS_A11_SMSM);
	if (r < 0)
		pr_err("smd_core_init: "
		       "enable_irq_wake failed for INT_DSPS_A11_SMSM\n");
#endif
	SMD_INFO("smd_core_init() done\n");

	return 0;
}

static int intr_init(struct interrupt_config_item *private_irq,
			struct smd_irq_config *platform_irq,
			struct platform_device *pdev
			)
{
	int irq_id;
	int ret;
	int ret_wake;

	private_irq->out_bit_pos = platform_irq->out_bit_pos;
	private_irq->out_offset = platform_irq->out_offset;
	private_irq->out_base = platform_irq->out_base;

	irq_id = platform_get_irq_byname(
					pdev,
					platform_irq->irq_name
				);
	SMD_DBG("smd: %s: register irq: %s id: %d\n", __func__,
				platform_irq->irq_name, irq_id);
	ret = request_irq(irq_id,
				private_irq->irq_handler,
				platform_irq->flags,
				platform_irq->device_name,
				(void *)platform_irq->dev_id
			);
	if (ret < 0) {
		platform_irq->irq_id = ret;
	} else {
		platform_irq->irq_id = irq_id;
		ret_wake = enable_irq_wake(irq_id);
		if (ret_wake < 0) {
			pr_err("smd: enable_irq_wake failed on %s",
					platform_irq->irq_name);
		}
	}

	return ret;
}

int sort_cmp_func(const void *a, const void *b)
{
	struct smem_area *left = (struct smem_area *)(a);
	struct smem_area *right = (struct smem_area *)(b);

	return left->phys_addr - right->phys_addr;
}

int smd_core_platform_init(struct platform_device *pdev)
{
	int i;
	int ret;
	uint32_t num_ss;
	struct smd_platform *smd_platform_data;
	struct smd_subsystem_config *smd_ss_config_list;
	struct smd_subsystem_config *cfg;
	int err_ret = 0;
	struct smd_smem_regions *smd_smem_areas;
	int smem_idx = 0;

	smd_platform_data = pdev->dev.platform_data;
	num_ss = smd_platform_data->num_ss_configs;
	smd_ss_config_list = smd_platform_data->smd_ss_configs;

	if (smd_platform_data->smd_ssr_config)
		disable_smsm_reset_handshake = smd_platform_data->
			   smd_ssr_config->disable_smsm_reset_handshake;

	smd_smem_areas = smd_platform_data->smd_smem_areas;
	if (smd_smem_areas) {
		num_smem_areas = smd_platform_data->num_smem_areas;
		smem_areas = kmalloc(sizeof(struct smem_area) * num_smem_areas,
						GFP_KERNEL);
		if (!smem_areas) {
			pr_err("%s: smem_areas kmalloc failed\n", __func__);
			err_ret = -ENOMEM;
			goto smem_areas_alloc_fail;
		}

		for (smem_idx = 0; smem_idx < num_smem_areas; ++smem_idx) {
			smem_areas[smem_idx].phys_addr =
					smd_smem_areas[smem_idx].phys_addr;
			smem_areas[smem_idx].size =
					smd_smem_areas[smem_idx].size;
			smem_areas[smem_idx].virt_addr = ioremap_nocache(
				(unsigned long)(smem_areas[smem_idx].phys_addr),
				smem_areas[smem_idx].size);
			if (!smem_areas[smem_idx].virt_addr) {
				pr_err("%s: ioremap_nocache() of addr:%p"
					" size: %x\n", __func__,
					smem_areas[smem_idx].phys_addr,
					smem_areas[smem_idx].size);
				err_ret = -ENOMEM;
				++smem_idx;
				goto smem_failed;
			}
		}
		sort(smem_areas, num_smem_areas,
				sizeof(struct smem_area),
				sort_cmp_func, NULL);
	}

	for (i = 0; i < num_ss; i++) {
		cfg = &smd_ss_config_list[i];

		ret = intr_init(
			&private_intr_config[cfg->irq_config_id].smd,
			&cfg->smd_int,
			pdev
			);

		if (ret < 0) {
			err_ret = ret;
			pr_err("smd: register irq failed on %s\n",
				cfg->smd_int.irq_name);
			goto intr_failed;
		}

		/* only init smsm structs if this edge supports smsm */
		if (cfg->smsm_int.irq_id)
			ret = intr_init(
				&private_intr_config[cfg->irq_config_id].smsm,
				&cfg->smsm_int,
				pdev
				);

		if (ret < 0) {
			err_ret = ret;
			pr_err("smd: register irq failed on %s\n",
				cfg->smsm_int.irq_name);
			goto intr_failed;
		}

		if (cfg->subsys_name)
			strlcpy(edge_to_pids[cfg->edge].subsys_name,
				cfg->subsys_name, SMD_MAX_CH_NAME_LEN);
	}


	SMD_INFO("smd_core_platform_init() done\n");
	return 0;

intr_failed:
	pr_err("smd: deregistering IRQs\n");
	for (i = 0; i < num_ss; ++i) {
		cfg = &smd_ss_config_list[i];

		if (cfg->smd_int.irq_id >= 0)
			free_irq(cfg->smd_int.irq_id,
				(void *)cfg->smd_int.dev_id
				);
		if (cfg->smsm_int.irq_id >= 0)
			free_irq(cfg->smsm_int.irq_id,
				(void *)cfg->smsm_int.dev_id
				);
	}
smem_failed:
	for (smem_idx = smem_idx - 1; smem_idx >= 0; --smem_idx)
		iounmap(smem_areas[smem_idx].virt_addr);
	kfree(smem_areas);
smem_areas_alloc_fail:
	return err_ret;
}

static int __devinit msm_smd_probe(struct platform_device *pdev)
{
	int ret;

	SMD_INFO("smd probe\n");
	INIT_WORK(&probe_work, smd_channel_probe_worker);

	channel_close_wq = create_singlethread_workqueue("smd_channel_close");
	if (IS_ERR(channel_close_wq)) {
		pr_err("%s: create_singlethread_workqueue ENOMEM\n", __func__);
		return -ENOMEM;
	}

	if (smsm_init()) {
		pr_err("smsm_init() failed\n");
		return -1;
	}

	if (pdev) {
		if (pdev->dev.of_node) {
			pr_err("SMD: Device tree not currently supported\n");
			return -ENODEV;
		} else if (pdev->dev.platform_data) {
			ret = smd_core_platform_init(pdev);
			if (ret) {
				pr_err(
				"SMD: smd_core_platform_init() failed\n");
				return -ENODEV;
			}
		} else {
			ret = smd_core_init();
			if (ret) {
				pr_err("smd_core_init() failed\n");
				return -ENODEV;
			}
		}
	} else {
		pr_err("SMD: PDEV not found\n");
		return -ENODEV;
	}

	smd_initialized = 1;

	smd_alloc_loopback_channel();
	smsm_irq_handler(0, 0);
	tasklet_schedule(&smd_fake_irq_tasklet);

	return 0;
}

static int restart_notifier_cb(struct notifier_block *this,
				  unsigned long code,
				  void *data);

static struct restart_notifier_block restart_notifiers[] = {
	{SMD_MODEM, "modem", .nb.notifier_call = restart_notifier_cb},
	{SMD_Q6, "lpass", .nb.notifier_call = restart_notifier_cb},
	{SMD_WCNSS, "riva", .nb.notifier_call = restart_notifier_cb},
	{SMD_DSPS, "dsps", .nb.notifier_call = restart_notifier_cb},
	{SMD_MODEM, "gss", .nb.notifier_call = restart_notifier_cb},
};

static int restart_notifier_cb(struct notifier_block *this,
				  unsigned long code,
				  void *data)
{
	if (code == SUBSYS_AFTER_SHUTDOWN) {
		struct restart_notifier_block *notifier;

		notifier = container_of(this,
				struct restart_notifier_block, nb);
		SMD_INFO("%s: ssrestart for processor %d ('%s')\n",
				__func__, notifier->processor,
				notifier->name);

		smd_channel_reset(notifier->processor);
	}

	return NOTIFY_DONE;
}

static __init int modem_restart_late_init(void)
{
	int i;
	void *handle;
	struct restart_notifier_block *nb;

	for (i = 0; i < ARRAY_SIZE(restart_notifiers); i++) {
		nb = &restart_notifiers[i];
		handle = subsys_notif_register_notifier(nb->name, &nb->nb);
		SMD_DBG("%s: registering notif for '%s', handle=%p\n",
				__func__, nb->name, handle);
	}
	return 0;
}
late_initcall(modem_restart_late_init);

static struct platform_driver msm_smd_driver = {
	.probe = msm_smd_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
	},
};

int __init msm_smd_init(void)
{
	static bool registered;

	if (registered)
		return 0;

	registered = true;
	return platform_driver_register(&msm_smd_driver);
}

module_init(msm_smd_init);

MODULE_DESCRIPTION("MSM Shared Memory Core");
MODULE_AUTHOR("Brian Swetland <swetland@google.com>");
MODULE_LICENSE("GPL");
