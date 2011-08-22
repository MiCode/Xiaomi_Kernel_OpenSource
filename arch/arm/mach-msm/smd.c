/* arch/arm/mach-msm/smd.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
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
#include <mach/msm_smd.h>
#include <mach/msm_iomap.h>
#include <mach/system.h>
#include <mach/subsystem_notif.h>

#include "smd_private.h"
#include "proc_comm.h"
#include "modem_notifier.h"

#if defined(CONFIG_ARCH_QSD8X50) || defined(CONFIG_ARCH_MSM8X60) \
	|| defined(CONFIG_ARCH_MSM8960) || defined(CONFIG_ARCH_FSM9XXX)
#define CONFIG_QDSP6 1
#endif

#if defined(CONFIG_ARCH_MSM8X60) || defined(CONFIG_ARCH_MSM8960)
#define CONFIG_DSPS 1
#endif

#if defined(CONFIG_ARCH_MSM8960)
#define CONFIG_WCNSS 1
#define CONFIG_DSPS_SMSM 1
#endif

#define MODULE_NAME "msm_smd"
#define SMEM_VERSION 0x000B
#define SMD_VERSION 0x00020000

uint32_t SMSM_NUM_ENTRIES = 8;
uint32_t SMSM_NUM_HOSTS = 3;

enum {
	MSM_SMD_DEBUG = 1U << 0,
	MSM_SMSM_DEBUG = 1U << 1,
	MSM_SMD_INFO = 1U << 2,
	MSM_SMSM_INFO = 1U << 3,
};

struct smsm_shared_info {
	uint32_t *state;
	uint32_t *intr_mask;
	uint32_t *intr_mux;
};

static struct smsm_shared_info smsm_info;

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
};

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
#else
#define SMD_DBG(x...) do { } while (0)
#define SMSM_DBG(x...) do { } while (0)
#define SMD_INFO(x...) do { } while (0)
#define SMSM_INFO(x...) do { } while (0)
#endif

static unsigned last_heap_free = 0xffffffff;

static inline void smd_write_intr(unsigned int val,
				const void __iomem *addr);

#if defined(CONFIG_ARCH_MSM7X30)
#define MSM_TRIG_A2M_SMD_INT     \
			(smd_write_intr(1 << 0, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMD_INT    \
			(smd_write_intr(1 << 8, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2M_SMSM_INT    \
			(smd_write_intr(1 << 5, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMSM_INT   \
			(smd_write_intr(1 << 8, MSM_GCC_BASE + 0x8))
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
#elif defined(CONFIG_ARCH_MSM8960)
#define MSM_TRIG_A2M_SMD_INT     \
			(smd_write_intr(1 << 3, MSM_APCS_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMD_INT    \
			(smd_write_intr(1 << 15, MSM_APCS_GCC_BASE + 0x8))
#define MSM_TRIG_A2M_SMSM_INT    \
			(smd_write_intr(1 << 4, MSM_APCS_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMSM_INT   \
			(smd_write_intr(1 << 14, MSM_APCS_GCC_BASE + 0x8))
#define MSM_TRIG_A2DSPS_SMD_INT  \
			(smd_write_intr(1, MSM_SIC_NON_SECURE_BASE + 0x4080))
#define MSM_TRIG_A2DSPS_SMSM_INT \
			(smd_write_intr(1, MSM_SIC_NON_SECURE_BASE + 0x4094))
#define MSM_TRIG_A2WCNSS_SMD_INT  \
			(smd_write_intr(1 << 25, MSM_APCS_GCC_BASE + 0x8))
#define MSM_TRIG_A2WCNSS_SMSM_INT  \
			(smd_write_intr(1 << 23, MSM_APCS_GCC_BASE + 0x8))
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
#else
#define MSM_TRIG_A2M_SMD_INT     \
			(smd_write_intr(1, MSM_CSR_BASE + 0x400 + (0) * 4))
#define MSM_TRIG_A2Q6_SMD_INT    \
			(smd_write_intr(1, MSM_CSR_BASE + 0x400 + (8) * 4))
#define MSM_TRIG_A2M_SMSM_INT    \
			(smd_write_intr(1, MSM_CSR_BASE + 0x400 + (5) * 4))
#define MSM_TRIG_A2Q6_SMSM_INT   \
			(smd_write_intr(1, MSM_CSR_BASE + 0x400 + (8) * 4))
#define MSM_TRIG_A2DSPS_SMD_INT
#define MSM_TRIG_A2DSPS_SMSM_INT
#define MSM_TRIG_A2WCNSS_SMD_INT
#define MSM_TRIG_A2WCNSS_SMSM_INT
#endif

#define SMD_LOOPBACK_CID 100

static LIST_HEAD(smd_ch_list_loopback);
static irqreturn_t smsm_irq_handler(int irq, void *data);
static void smd_fake_irq_handler(unsigned long arg);

static void notify_smsm_cb_clients_worker(struct work_struct *work);
static DECLARE_WORK(smsm_cb_work, notify_smsm_cb_clients_worker);
static DEFINE_SPINLOCK(smsm_lock);
static struct smsm_state_info *smsm_states;
static int spinlocks_initialized;

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
	__raw_writel(0x0, MSM_TLMM_BASE + 0x1284);
	__raw_writel(0x2, MSM_TLMM_BASE + 0x1284);
	/* end workaround */
}
#else
static inline void wakeup_v1_riva(void) {}
#endif

static void notify_other_smsm(uint32_t smsm_entry, uint32_t notify_mask)
{
	/* older protocol don't use smsm_intr_mask,
	   but still communicates with modem */
	if (!smsm_info.intr_mask ||
	    (__raw_readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_MODEM))
				& notify_mask))
		MSM_TRIG_A2M_SMSM_INT;

	if (smsm_info.intr_mask &&
	    (__raw_readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_Q6))
				& notify_mask)) {
#if !defined(CONFIG_ARCH_MSM8X60) && !defined(MCONFIG_ARCH_MSM8960)
		uint32_t mux_val;

		if (smsm_info.intr_mux) {
			mux_val = __raw_readl(
					SMSM_INTR_MUX_ADDR(SMEM_APPS_Q6_SMSM));
			mux_val++;
			__raw_writel(mux_val,
					SMSM_INTR_MUX_ADDR(SMEM_APPS_Q6_SMSM));
		}
#endif
		MSM_TRIG_A2Q6_SMSM_INT;
	}

	if (smsm_info.intr_mask &&
	    (__raw_readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_WCNSS))
				& notify_mask)) {
		wakeup_v1_riva();
		MSM_TRIG_A2WCNSS_SMSM_INT;
	}

	if (smsm_info.intr_mask &&
	    (__raw_readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_DSPS))
				& notify_mask)) {
		MSM_TRIG_A2DSPS_SMSM_INT;
	}

	schedule_work(&smsm_cb_work);
}

static inline void notify_modem_smd(void)
{
	MSM_TRIG_A2M_SMD_INT;
}

static inline void notify_dsp_smd(void)
{
	MSM_TRIG_A2Q6_SMD_INT;
}

static inline void notify_dsps_smd(void)
{
	MSM_TRIG_A2DSPS_SMD_INT;
}

static inline void notify_wcnss_smd(void)
{
	wakeup_v1_riva();
	MSM_TRIG_A2WCNSS_SMD_INT;
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

struct smd_channel {
	volatile struct smd_half_channel *send;
	volatile struct smd_half_channel *recv;
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
};

struct edge_to_pid {
	uint32_t	local_pid;
	uint32_t	remote_pid;
};

/**
 * Maps edge type to local and remote processor ID's.
 */
static struct edge_to_pid edge_to_pids[] = {
	[SMD_APPS_MODEM] = {SMSM_APPS, SMSM_MODEM},
	[SMD_APPS_QDSP] = {SMSM_APPS, SMSM_Q6},
	[SMD_MODEM_QDSP] = {SMSM_MODEM, SMSM_Q6},
	[SMD_APPS_DSPS] = {SMSM_APPS, SMSM_DSPS},
	[SMD_MODEM_DSPS] = {SMSM_MODEM, SMSM_DSPS},
	[SMD_QDSP_DSPS] = {SMSM_Q6, SMSM_DSPS},
	[SMD_APPS_WCNSS] = {SMSM_APPS, SMSM_WCNSS},
	[SMD_MODEM_WCNSS] = {SMSM_MODEM, SMSM_WCNSS},
	[SMD_QDSP_WCNSS] = {SMSM_Q6, SMSM_WCNSS},
	[SMD_DSPS_WCNSS] = {SMSM_DSPS, SMSM_WCNSS},
};

struct restart_notifier_block {
	unsigned processor;
	char *name;
	struct notifier_block nb;
};

static struct platform_device loopback_tty_pdev = {.name = "LOOPBACK_TTY"};

static LIST_HEAD(smd_ch_closed_list);
static LIST_HEAD(smd_ch_closing_list);
static LIST_HEAD(smd_ch_to_close_list);
static LIST_HEAD(smd_ch_list_modem);
static LIST_HEAD(smd_ch_list_dsp);
static LIST_HEAD(smd_ch_list_dsps);
static LIST_HEAD(smd_ch_list_wcnss);

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
		if ((type != SMD_APPS_MODEM) && (type != SMD_APPS_QDSP) &&
		    (type != SMD_APPS_DSPS) && (type != SMD_APPS_WCNSS))
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

		if (pid_is_on_edge(shared2, type, pid, &local_ch, &remote_ch)) {

			/* force remote state for processor being restarted */
			if (local_ch->state != SMD_SS_CLOSED) {
				local_ch->state = new_state;
				local_ch->fDSR = 0;
				local_ch->fCTS = 0;
				local_ch->fCD = 0;
				local_ch->fSTATE = 1;
			}
		}
	}
}


void smd_channel_reset(uint32_t restart_pid)
{
	struct smd_alloc_elm *shared;
	unsigned n;
	uint32_t *smem_lock;
	unsigned long flags;

	SMD_DBG("%s: starting reset\n", __func__);
	shared = smem_find(ID_CH_ALLOC_TBL, sizeof(*shared) * 64);
	if (!shared) {
		pr_err("%s: allocation table not initialized\n", __func__);
		return;
	}

	smem_lock = smem_alloc(SMEM_SPINLOCK_ARRAY, 8 * sizeof(uint32_t));
	if (smem_lock) {
		SMD_DBG("%s: releasing locks\n", __func__);
		for (n = 0; n < 8; n++) {
			uint32_t pid = readl_relaxed(smem_lock);
			if (pid == (restart_pid + 1))
				writel_relaxed(0, smem_lock);
			smem_lock++;
		}
	}

	/* reset SMSM entry */
	if (smsm_info.state) {
		writel_relaxed(0, SMSM_STATE_ADDR(restart_pid));

		/* clear apps SMSM to restart SMSM init handshake */
		if (restart_pid == SMSM_MODEM)
			writel_relaxed(0, SMSM_STATE_ADDR(SMSM_APPS));

		/* notify SMSM processors */
		smsm_irq_handler(0, 0);
		MSM_TRIG_A2M_SMSM_INT;
		MSM_TRIG_A2Q6_SMSM_INT;
		MSM_TRIG_A2DSPS_SMSM_INT;
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
	return (ch->recv->head - ch->recv->tail) & ch->fifo_mask;
}

/* how many bytes we are free to write */
static int smd_stream_write_avail(struct smd_channel *ch)
{
	return ch->fifo_mask -
		((ch->send->head - ch->send->tail) & ch->fifo_mask);
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
	return (ch->recv->state == SMD_SS_OPENED ||
		ch->recv->state == SMD_SS_FLUSHING)
		&& (ch->send->state == SMD_SS_OPENED);
}

/* provide a pointer and length to readable data in the fifo */
static unsigned ch_read_buffer(struct smd_channel *ch, void **ptr)
{
	unsigned head = ch->recv->head;
	unsigned tail = ch->recv->tail;
	*ptr = (void *) (ch->recv_data + tail);

	if (tail <= head)
		return head - tail;
	else
		return ch->fifo_size - tail;
}

static int read_intr_blocked(struct smd_channel *ch)
{
	return ch->recv->fBLOCKREADINTR;
}

/* advance the fifo read pointer after data from ch_read_buffer is consumed */
static void ch_read_done(struct smd_channel *ch, unsigned count)
{
	BUG_ON(count > smd_stream_read_avail(ch));
	ch->recv->tail = (ch->recv->tail + count) & ch->fifo_mask;
	wmb();
	ch->send->fTAIL = 1;
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
	unsigned head = ch->send->head;
	unsigned tail = ch->send->tail;
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
	ch->send->head = (ch->send->head + count) & ch->fifo_mask;
	wmb();
	ch->send->fHEAD = 1;
}

static void ch_set_state(struct smd_channel *ch, unsigned n)
{
	if (n == SMD_SS_OPENED) {
		ch->send->fDSR = 1;
		ch->send->fCTS = 1;
		ch->send->fCD = 1;
	} else {
		ch->send->fDSR = 0;
		ch->send->fCTS = 0;
		ch->send->fCD = 0;
	}
	ch->send->state = n;
	ch->send->fSTATE = 1;
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
		if (ch->send->state == SMD_SS_CLOSING ||
		    ch->send->state == SMD_SS_CLOSED) {
			ch->recv->tail = 0;
			ch->send->head = 0;
			ch->send->fBLOCKREADINTR = 0;
			ch_set_state(ch, SMD_SS_OPENING);
		}
		break;
	case SMD_SS_OPENED:
		if (ch->send->state == SMD_SS_OPENING) {
			ch_set_state(ch, SMD_SS_OPENED);
			ch->notify(ch->priv, SMD_EVENT_OPEN);
		}
		break;
	case SMD_SS_FLUSHING:
	case SMD_SS_RESET:
		/* we should force them to close? */
		break;
	case SMD_SS_CLOSED:
		if (ch->send->state == SMD_SS_OPENED) {
			ch_set_state(ch, SMD_SS_CLOSING);
			ch->current_packet = 0;
			ch->notify(ch->priv, SMD_EVENT_CLOSE);
		}
		break;
	case SMD_SS_CLOSING:
		if (ch->send->state == SMD_SS_CLOSED) {
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
		if (ch->recv->fSTATE)
			ch->recv->fSTATE = 0;
		tmp = ch->recv->state;
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
			if (ch->recv->fHEAD) {
				ch->recv->fHEAD = 0;
				ch_flags |= 1;
			}
			if (ch->recv->fTAIL) {
				ch->recv->fTAIL = 0;
				ch_flags |= 2;
			}
			if (ch->recv->fSTATE) {
				ch->recv->fSTATE = 0;
				ch_flags |= 4;
			}
		}
		tmp = ch->recv->state;
		if (tmp != ch->last_state) {
			smd_state_change(ch, ch->last_state, tmp);
			state_change = 1;
		}
		if (ch_flags) {
			ch->update_state(ch);
			ch->notify(ch->priv, SMD_EVENT_DATA);
		}
		if (ch_flags & 0x4 && !state_change)
			ch->notify(ch->priv, SMD_EVENT_STATUS);
	}
	spin_unlock_irqrestore(&smd_lock, flags);
	do_smd_probe();
}

static irqreturn_t smd_modem_irq_handler(int irq, void *data)
{
	handle_smd_irq(&smd_ch_list_modem, notify_modem_smd);
	handle_smd_irq_closing_list();
	return IRQ_HANDLED;
}

#if defined(CONFIG_QDSP6)
static irqreturn_t smd_dsp_irq_handler(int irq, void *data)
{
	handle_smd_irq(&smd_ch_list_dsp, notify_dsp_smd);
	handle_smd_irq_closing_list();
	return IRQ_HANDLED;
}
#endif

#if defined(CONFIG_DSPS)
static irqreturn_t smd_dsps_irq_handler(int irq, void *data)
{
	handle_smd_irq(&smd_ch_list_dsps, notify_dsps_smd);
	handle_smd_irq_closing_list();
	return IRQ_HANDLED;
}
#endif

#if defined(CONFIG_WCNSS)
static irqreturn_t smd_wcnss_irq_handler(int irq, void *data)
{
	handle_smd_irq(&smd_ch_list_wcnss, notify_wcnss_smd);
	handle_smd_irq_closing_list();
	return IRQ_HANDLED;
}
#endif

static void smd_fake_irq_handler(unsigned long arg)
{
	handle_smd_irq(&smd_ch_list_modem, notify_modem_smd);
	handle_smd_irq(&smd_ch_list_dsp, notify_dsp_smd);
	handle_smd_irq(&smd_ch_list_dsps, notify_dsps_smd);
	handle_smd_irq(&smd_ch_list_wcnss, notify_wcnss_smd);
	handle_smd_irq_closing_list();
}

static DECLARE_TASKLET(smd_fake_irq_tasklet, smd_fake_irq_handler, 0);

static inline int smd_need_int(struct smd_channel *ch)
{
	if (ch_is_open(ch)) {
		if (ch->recv->fHEAD || ch->recv->fTAIL || ch->recv->fSTATE)
			return 1;
		if (ch->recv->state != ch->last_state)
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
		if (!ch_is_open(ch))
			break;
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

static int smd_alloc_v2(struct smd_channel *ch)
{
	struct smd_shared_v2 *shared2;
	void *buffer;
	unsigned buffer_sz;

	shared2 = smem_alloc(SMEM_SMD_BASE_ID + ch->n, sizeof(*shared2));
	if (!shared2) {
		SMD_INFO("smem_alloc failed ch=%d\n", ch->n);
		return -1;
	}
	buffer = smem_get_entry(SMEM_SMD_FIFO_BASE_ID + ch->n, &buffer_sz);
	if (!buffer) {
		SMD_INFO("smem_get_entry failed \n");
		return -1;
	}

	/* buffer must be a power-of-two size */
	if (buffer_sz & (buffer_sz - 1))
		return -1;

	buffer_sz /= 2;
	ch->send = &shared2->ch0;
	ch->recv = &shared2->ch1;
	ch->send_data = buffer;
	ch->recv_data = buffer + buffer_sz;
	ch->fifo_size = buffer_sz;
	return 0;
}

static int smd_alloc_v1(struct smd_channel *ch)
{
	struct smd_shared_v1 *shared1;
	shared1 = smem_alloc(ID_SMD_CHANNELS + ch->n, sizeof(*shared1));
	if (!shared1) {
		pr_err("smd_alloc_channel() cid %d does not exist\n", ch->n);
		return -1;
	}
	ch->send = &shared1->ch0;
	ch->recv = &shared1->ch1;
	ch->send_data = shared1->data0;
	ch->recv_data = shared1->data1;
	ch->fifo_size = SMD_BUF_SIZE;
	return 0;
}

static int smd_alloc_channel(struct smd_alloc_elm *alloc_elm)
{
	struct smd_channel *ch;

	ch = kzalloc(sizeof(struct smd_channel), GFP_KERNEL);
	if (ch == 0) {
		pr_err("smd_alloc_channel() out of memory\n");
		return -1;
	}
	ch->n = alloc_elm->cid;

	if (smd_alloc_v2(ch) && smd_alloc_v1(ch)) {
		kfree(ch);
		return -1;
	}

	ch->fifo_mask = ch->fifo_size - 1;
	ch->type = SMD_CHANNEL_TYPE(alloc_elm->type);

	if (ch->type == SMD_APPS_MODEM)
		ch->notify_other_cpu = notify_modem_smd;
	else if (ch->type == SMD_APPS_QDSP)
		ch->notify_other_cpu = notify_dsp_smd;
	else if (ch->type == SMD_APPS_DSPS)
		ch->notify_other_cpu = notify_dsps_smd;
	else
		ch->notify_other_cpu = notify_wcnss_smd;

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

	spin_lock_irqsave(&smd_lock, flags);
	list_for_each_entry_safe(ch, index,  &smd_ch_to_close_list, ch_list) {
		list_del(&ch->ch_list);
		spin_unlock_irqrestore(&smd_lock, flags);
		mutex_lock(&smd_creation_mutex);
		list_add(&ch->ch_list, &smd_ch_closed_list);
		mutex_unlock(&smd_creation_mutex);
		ch->notify(ch->priv, SMD_EVENT_REOPEN_READY);
		ch->notify = do_nothing_notify;
		spin_lock_irqsave(&smd_lock, flags);
	}
	spin_unlock_irqrestore(&smd_lock, flags);
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
	if (!ch)
		return -ENODEV;

	if (notify == 0)
		notify = do_nothing_notify;

	ch->notify = notify;
	ch->current_packet = 0;
	ch->last_state = SMD_SS_CLOSED;
	ch->priv = priv;

	if (edge == SMD_LOOPBACK_TYPE) {
		ch->last_state = SMD_SS_OPENED;
		ch->send->state = SMD_SS_OPENED;
		ch->send->fDSR = 1;
		ch->send->fCTS = 1;
		ch->send->fCD = 1;
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
		ch->send->fDSR = 0;
		ch->send->fCTS = 0;
		ch->send->fCD = 0;
		ch->send->state = SMD_SS_CLOSED;
	} else
		ch_set_state(ch, SMD_SS_CLOSED);

	if (ch->recv->state == SMD_SS_OPENED) {
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
	return ch->read(ch, data, len, 0);
}
EXPORT_SYMBOL(smd_read);

int smd_read_user_buffer(smd_channel_t *ch, void *data, int len)
{
	return ch->read(ch, data, len, 1);
}
EXPORT_SYMBOL(smd_read_user_buffer);

int smd_read_from_cb(smd_channel_t *ch, void *data, int len)
{
	return ch->read_from_cb(ch, data, len, 0);
}
EXPORT_SYMBOL(smd_read_from_cb);

int smd_write(smd_channel_t *ch, const void *data, int len)
{
	return ch->pending_pkt_sz ? -EBUSY : ch->write(ch, data, len, 0);
}
EXPORT_SYMBOL(smd_write);

int smd_write_user_buffer(smd_channel_t *ch, const void *data, int len)
{
	return ch->pending_pkt_sz ? -EBUSY : ch->write(ch, data, len, 1);
}
EXPORT_SYMBOL(smd_write_user_buffer);

int smd_read_avail(smd_channel_t *ch)
{
	return ch->read_avail(ch);
}
EXPORT_SYMBOL(smd_read_avail);

int smd_write_avail(smd_channel_t *ch)
{
	return ch->write_avail(ch);
}
EXPORT_SYMBOL(smd_write_avail);

void smd_enable_read_intr(smd_channel_t *ch)
{
	if (ch)
		ch->send->fBLOCKREADINTR = 0;
}
EXPORT_SYMBOL(smd_enable_read_intr);

void smd_disable_read_intr(smd_channel_t *ch)
{
	if (ch)
		ch->send->fBLOCKREADINTR = 1;
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
	return ch->current_packet;
}
EXPORT_SYMBOL(smd_cur_packet_size);

int smd_tiocmget(smd_channel_t *ch)
{
	return  (ch->recv->fDSR ? TIOCM_DSR : 0) |
		(ch->recv->fCTS ? TIOCM_CTS : 0) |
		(ch->recv->fCD ? TIOCM_CD : 0) |
		(ch->recv->fRI ? TIOCM_RI : 0) |
		(ch->send->fCTS ? TIOCM_RTS : 0) |
		(ch->send->fDSR ? TIOCM_DTR : 0);
}
EXPORT_SYMBOL(smd_tiocmget);

/* this api will be called while holding smd_lock */
int
smd_tiocmset_from_cb(smd_channel_t *ch, unsigned int set, unsigned int clear)
{
	if (set & TIOCM_DTR)
		ch->send->fDSR = 1;

	if (set & TIOCM_RTS)
		ch->send->fCTS = 1;

	if (clear & TIOCM_DTR)
		ch->send->fDSR = 0;

	if (clear & TIOCM_RTS)
		ch->send->fCTS = 0;

	ch->send->fSTATE = 1;
	barrier();
	ch->notify_other_cpu();

	return 0;
}
EXPORT_SYMBOL(smd_tiocmset_from_cb);

int smd_tiocmset(smd_channel_t *ch, unsigned int set, unsigned int clear)
{
	unsigned long flags;

	spin_lock_irqsave(&smd_lock, flags);
	smd_tiocmset_from_cb(ch, set, clear);
	spin_unlock_irqrestore(&smd_lock, flags);

	return 0;
}
EXPORT_SYMBOL(smd_tiocmset);


/* -------------------------------------------------------------------------- */

/* smem_alloc returns the pointer to smem item if it is already allocated.
 * Otherwise, it returns NULL.
 */
void *smem_alloc(unsigned id, unsigned size)
{
	return smem_find(id, size);
}
EXPORT_SYMBOL(smem_alloc);

#define SMEM_SPINLOCK_SMEM_ALLOC       "S:3"
static remote_spinlock_t remote_spinlock;

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
		ret = (void *) (MSM_SHARED_RAM_BASE + toc[id].offset);
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
	unsigned long flags;
	struct smsm_state_info *state_info;
	int n;
	int ret = 0;

	smsm_states = kmalloc(sizeof(struct smsm_state_info)*SMSM_NUM_ENTRIES,
		   GFP_KERNEL);

	if (!smsm_states) {
		pr_err("%s: SMSM init failed\n", __func__);
		return -ENOMEM;
	}

	spin_lock_irqsave(&smsm_lock, flags);
	for (n = 0; n < SMSM_NUM_ENTRIES; n++) {
		state_info = &smsm_states[n];
		state_info->last_value = __raw_readl(SMSM_STATE_ADDR(n));
		INIT_LIST_HEAD(&state_info->callbacks);
	}
	spin_unlock_irqrestore(&smsm_lock, flags);

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

		if (smsm_info.intr_mask)
			for (i = 0; i < SMSM_NUM_ENTRIES; i++)
				__raw_writel(0xffffffff,
				       SMSM_INTR_MASK_ADDR(i, SMSM_APPS));
	}

	if (!smsm_info.intr_mux)
		smsm_info.intr_mux = smem_alloc2(SMEM_SMD_SMSM_INTR_MUX,
						 SMSM_NUM_INTR_MUX *
						 sizeof(uint32_t));

	i = smsm_cb_init();
	if (i)
		return i;

	wmb();
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

static irqreturn_t smsm_irq_handler(int irq, void *data)
{
	unsigned long flags;

#if !defined(CONFIG_ARCH_MSM8X60)
	uint32_t mux_val;
	static uint32_t prev_smem_q6_apps_smsm;

	if (irq == INT_ADSP_A11_SMSM) {
		if (!smsm_info.intr_mux)
			return IRQ_HANDLED;
		mux_val = __raw_readl(SMSM_INTR_MUX_ADDR(SMEM_Q6_APPS_SMSM));
		if (mux_val != prev_smem_q6_apps_smsm)
			prev_smem_q6_apps_smsm = mux_val;
		return IRQ_HANDLED;
	}
#else
	if (irq == INT_ADSP_A11_SMSM)
		return IRQ_HANDLED;
#endif


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
			apps &= ~SMSM_RESET;

			/* Issue a fake irq to handle any
			 * smd state changes during reset
			 */
			smd_fake_irq_handler(0);

			/* queue modem restart notify chain */
			modem_queue_start_reset_notify();

		} else if (modm & SMSM_RESET) {
			apps |= SMSM_RESET;

			pr_err("\nSMSM: Modem SMSM state changed to SMSM_RESET.");
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

		schedule_work(&smsm_cb_work);
	}
	spin_unlock_irqrestore(&smem_lock, flags);
	return IRQ_HANDLED;
}

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
	unsigned long flags;
	struct smsm_state_cb_info *cb_info;
	struct smsm_state_info *state_info;
	int n;
	uint32_t new_state;
	uint32_t state_changes;

	spin_lock_irqsave(&smsm_lock, flags);

	if (!smsm_states) {
		/* smsm not yet initialized */
		spin_unlock_irqrestore(&smsm_lock, flags);
		return;
	}

	for (n = 0; n < SMSM_NUM_ENTRIES; n++) {
		state_info = &smsm_states[n];
		new_state = __raw_readl(SMSM_STATE_ADDR(n));

		if (new_state != state_info->last_value) {
			state_changes = state_info->last_value ^ new_state;

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

	spin_unlock_irqrestore(&smsm_lock, flags);
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
	unsigned long flags;
	struct smsm_state_cb_info *cb_info;
	struct smsm_state_cb_info *cb_found = 0;
	int ret = 0;

	if (smsm_entry >= SMSM_NUM_ENTRIES)
		return -EINVAL;

	spin_lock_irqsave(&smsm_lock, flags);

	if (!smsm_states) {
		/* smsm not yet initialized */
		ret = -ENODEV;
		goto cleanup;
	}

	list_for_each_entry(cb_info,
			&smsm_states[smsm_entry].callbacks, cb_list) {
		if ((cb_info->notify == notify) &&
				(cb_info->data == data)) {
			cb_info->mask |= mask;
			cb_found = cb_info;
			ret = 1;
			break;
		}
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
			&smsm_states[smsm_entry].callbacks);
	}

cleanup:
	spin_unlock_irqrestore(&smsm_lock, flags);
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
	unsigned long flags;
	struct smsm_state_cb_info *cb_info;
	int ret = 0;

	if (smsm_entry >= SMSM_NUM_ENTRIES)
		return -EINVAL;

	spin_lock_irqsave(&smsm_lock, flags);

	if (!smsm_states) {
		/* smsm not yet initialized */
		spin_unlock_irqrestore(&smsm_lock, flags);
		return -ENODEV;
	}

	list_for_each_entry(cb_info,
		&smsm_states[smsm_entry].callbacks, cb_list) {
		if ((cb_info->notify == notify) &&
			(cb_info->data == data)) {
			cb_info->mask &= ~mask;
			ret = 1;
			if (!cb_info->mask) {
				/* no mask bits set, remove callback */
				list_del(&cb_info->cb_list);
				kfree(cb_info);
				ret = 2;
			}
			break;
		}
	}

	spin_unlock_irqrestore(&smsm_lock, flags);
	return ret;
}
EXPORT_SYMBOL(smsm_state_cb_deregister);


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

	r = request_irq(INT_A9_M2A_5, smsm_irq_handler,
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

	r = request_irq(INT_ADSP_A11_SMSM, smsm_irq_handler,
			flags, "smsm_dev", smsm_irq_handler);
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
		free_irq(INT_ADSP_A11_SMSM, smsm_irq_handler);
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
		free_irq(INT_ADSP_A11_SMSM, smsm_irq_handler);
		free_irq(INT_DSPS_A11, smd_dsps_irq_handler);
		return r;
	}

	r = enable_irq_wake(INT_WCNSS_A11);
	if (r < 0)
		pr_err("smd_core_init: "
		       "enable_irq_wake failed for INT_WCNSS_A11\n");

	r = request_irq(INT_WCNSS_A11_SMSM, smsm_irq_handler,
			flags, "smsm_dev", smsm_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		free_irq(INT_ADSP_A11_SMSM, smsm_irq_handler);
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
	r = request_irq(INT_DSPS_A11_SMSM, smsm_irq_handler,
			flags, "smsm_dev", smsm_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		free_irq(INT_ADSP_A11_SMSM, smsm_irq_handler);
		free_irq(INT_DSPS_A11, smd_dsps_irq_handler);
		free_irq(INT_WCNSS_A11, smd_wcnss_irq_handler);
		free_irq(INT_WCNSS_A11_SMSM, smsm_irq_handler);
		return r;
	}

	r = enable_irq_wake(INT_DSPS_A11_SMSM);
	if (r < 0)
		pr_err("smd_core_init: "
		       "enable_irq_wake failed for INT_DSPS_A11_SMSM\n");
#endif

	/* we may have missed a signal while booting -- fake
	 * an interrupt to make sure we process any existing
	 * state
	 */
	smsm_irq_handler(0, 0);

	SMD_INFO("smd_core_init() done\n");

	return 0;
}

static int __devinit msm_smd_probe(struct platform_device *pdev)
{
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

	if (smd_core_init()) {
		pr_err("smd_core_init() failed\n");
		return -1;
	}

	smd_initialized = 1;

	smd_alloc_loopback_channel();

	return 0;
}

static int restart_notifier_cb(struct notifier_block *this,
				  unsigned long code,
				  void *data);

static struct restart_notifier_block restart_notifiers[] = {
	{SMSM_MODEM, "modem", .nb.notifier_call = restart_notifier_cb},
	{SMSM_Q6, "lpass", .nb.notifier_call = restart_notifier_cb},
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

static int __init msm_smd_init(void)
{
	return platform_driver_register(&msm_smd_driver);
}

module_init(msm_smd_init);

MODULE_DESCRIPTION("MSM Shared Memory Core");
MODULE_AUTHOR("Brian Swetland <swetland@google.com>");
MODULE_LICENSE("GPL");
