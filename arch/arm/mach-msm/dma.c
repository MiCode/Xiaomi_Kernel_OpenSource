/* linux/arch/arm/mach-msm/dma.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2010, 2012 The Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/pm_runtime.h>
#include <mach/dma.h>

#define MODULE_NAME "msm_dmov"

#define MSM_DMOV_CHANNEL_COUNT 16
#define MSM_DMOV_CRCI_COUNT 16

enum {
	CLK_DIS,
	CLK_TO_BE_DIS,
	CLK_EN
};

struct msm_dmov_ci_conf {
	int start;
	int end;
	int burst;
};

struct msm_dmov_crci_conf {
	int sd;
	int blk_size;
};

struct msm_dmov_chan_conf {
	int sd;
	int block;
	int priority;
};

struct msm_dmov_conf {
	void *base;
	struct msm_dmov_crci_conf *crci_conf;
	struct msm_dmov_chan_conf *chan_conf;
	int channel_active;
	int sd;
	size_t sd_size;
	struct list_head staged_commands[MSM_DMOV_CHANNEL_COUNT];
	struct list_head ready_commands[MSM_DMOV_CHANNEL_COUNT];
	struct list_head active_commands[MSM_DMOV_CHANNEL_COUNT];
	struct mutex lock;
	spinlock_t list_lock;
	unsigned int irq;
	struct clk *clk;
	struct clk *pclk;
	struct clk *ebiclk;
	unsigned int clk_ctl;
	struct delayed_work work;
	struct workqueue_struct *cmd_wq;
};

static void msm_dmov_clock_work(struct work_struct *);

#ifdef CONFIG_ARCH_MSM8X60

#define DMOV_CHANNEL_DEFAULT_CONF { .sd = 1, .block = 0, .priority = 0 }
#define DMOV_CHANNEL_MODEM_CONF { .sd = 3, .block = 0, .priority = 0 }
#define DMOV_CHANNEL_CONF(secd, blk, pri) \
	{ .sd = secd, .block = blk, .priority = pri }

static struct msm_dmov_chan_conf adm0_chan_conf[] = {
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_MODEM_CONF,
	DMOV_CHANNEL_MODEM_CONF,
	DMOV_CHANNEL_MODEM_CONF,
	DMOV_CHANNEL_MODEM_CONF,
	DMOV_CHANNEL_MODEM_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
};

static struct msm_dmov_chan_conf adm1_chan_conf[] = {
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_DEFAULT_CONF,
	DMOV_CHANNEL_MODEM_CONF,
	DMOV_CHANNEL_MODEM_CONF,
	DMOV_CHANNEL_MODEM_CONF,
	DMOV_CHANNEL_MODEM_CONF,
	DMOV_CHANNEL_MODEM_CONF,
	DMOV_CHANNEL_MODEM_CONF,
};

#define DMOV_CRCI_DEFAULT_CONF { .sd = 1, .blk_size = 0 }
#define DMOV_CRCI_CONF(secd, blk) { .sd = secd, .blk_size = blk }

static struct msm_dmov_crci_conf adm0_crci_conf[] = {
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_CONF(1, 4),
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
};

static struct msm_dmov_crci_conf adm1_crci_conf[] = {
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_CONF(1, 1),
	DMOV_CRCI_CONF(1, 1),
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_CONF(1, 1),
	DMOV_CRCI_CONF(1, 1),
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_DEFAULT_CONF,
	DMOV_CRCI_CONF(1, 1),
	DMOV_CRCI_DEFAULT_CONF,
};

static struct msm_dmov_conf dmov_conf[] = {
	{
		.crci_conf = adm0_crci_conf,
		.chan_conf = adm0_chan_conf,
		.lock = __MUTEX_INITIALIZER(dmov_conf[0].lock),
		.list_lock = __SPIN_LOCK_UNLOCKED(dmov_list_lock),
		.clk_ctl = CLK_DIS,
		.work = __DELAYED_WORK_INITIALIZER(dmov_conf[0].work,
				msm_dmov_clock_work, 0),
	}, {
		.crci_conf = adm1_crci_conf,
		.chan_conf = adm1_chan_conf,
		.lock = __MUTEX_INITIALIZER(dmov_conf[1].lock),
		.list_lock = __SPIN_LOCK_UNLOCKED(dmov_list_lock),
		.clk_ctl = CLK_DIS,
		.work = __DELAYED_WORK_INITIALIZER(dmov_conf[1].work,
				msm_dmov_clock_work, 0),
	}
};
#else
static struct msm_dmov_conf dmov_conf[] = {
	{
		.crci_conf = NULL,
		.chan_conf = NULL,
		.lock = __MUTEX_INITIALIZER(dmov_conf[0].lock),
		.list_lock = __SPIN_LOCK_UNLOCKED(dmov_list_lock),
		.clk_ctl = CLK_DIS,
		.work = __DELAYED_WORK_INITIALIZER(dmov_conf[0].work,
				msm_dmov_clock_work, 0),
	}
};
#endif

#define MSM_DMOV_ID_COUNT (MSM_DMOV_CHANNEL_COUNT * ARRAY_SIZE(dmov_conf))
#define DMOV_REG(name, adm)    ((name) + (dmov_conf[adm].base) +\
	(dmov_conf[adm].sd * dmov_conf[adm].sd_size))
#define DMOV_ID_TO_ADM(id)   ((id) / MSM_DMOV_CHANNEL_COUNT)
#define DMOV_ID_TO_CHAN(id)   ((id) % MSM_DMOV_CHANNEL_COUNT)
#define DMOV_CHAN_ADM_TO_ID(ch, adm) ((ch) + (adm) * MSM_DMOV_CHANNEL_COUNT)

#ifdef CONFIG_MSM_ADM3
#define DMOV_IRQ_TO_ADM(irq)   \
({ \
	typeof(irq) _irq = irq; \
	((_irq == INT_ADM1_MASTER) || (_irq == INT_ADM1_AARM)); \
})
#else
#define DMOV_IRQ_TO_ADM(irq) 0
#endif

enum {
	MSM_DMOV_PRINT_ERRORS = 1,
	MSM_DMOV_PRINT_IO = 2,
	MSM_DMOV_PRINT_FLOW = 4
};

unsigned int msm_dmov_print_mask = MSM_DMOV_PRINT_ERRORS;

#define MSM_DMOV_DPRINTF(mask, format, args...) \
	do { \
		if ((mask) & msm_dmov_print_mask) \
			printk(KERN_ERR format, args); \
	} while (0)
#define PRINT_ERROR(format, args...) \
	MSM_DMOV_DPRINTF(MSM_DMOV_PRINT_ERRORS, format, args);
#define PRINT_IO(format, args...) \
	MSM_DMOV_DPRINTF(MSM_DMOV_PRINT_IO, format, args);
#define PRINT_FLOW(format, args...) \
	MSM_DMOV_DPRINTF(MSM_DMOV_PRINT_FLOW, format, args);

static int msm_dmov_clk_on(int adm)
{
	int ret;

	ret = clk_prepare_enable(dmov_conf[adm].clk);
	if (ret)
		return ret;
	if (dmov_conf[adm].pclk) {
		ret = clk_prepare_enable(dmov_conf[adm].pclk);
		if (ret) {
			clk_disable_unprepare(dmov_conf[adm].clk);
			return ret;
		}
	}
	if (dmov_conf[adm].ebiclk) {
		ret = clk_prepare_enable(dmov_conf[adm].ebiclk);
		if (ret) {
			if (dmov_conf[adm].pclk)
				clk_disable_unprepare(dmov_conf[adm].pclk);
			clk_disable_unprepare(dmov_conf[adm].clk);
		}
	}
	return ret;
}

static void msm_dmov_clk_off(int adm)
{
	if (dmov_conf[adm].ebiclk)
		clk_disable_unprepare(dmov_conf[adm].ebiclk);
	if (dmov_conf[adm].pclk)
		clk_disable_unprepare(dmov_conf[adm].pclk);
	clk_disable_unprepare(dmov_conf[adm].clk);
}

static void msm_dmov_clock_work(struct work_struct *work)
{
	struct msm_dmov_conf *conf =
		container_of(to_delayed_work(work), struct msm_dmov_conf, work);
	int adm = DMOV_IRQ_TO_ADM(conf->irq);
	mutex_lock(&conf->lock);
	if (conf->clk_ctl == CLK_TO_BE_DIS) {
		BUG_ON(conf->channel_active);
		msm_dmov_clk_off(adm);
		conf->clk_ctl = CLK_DIS;
	}
	mutex_unlock(&conf->lock);
}

enum {
	NOFLUSH = 0,
	GRACEFUL,
	NONGRACEFUL,
};

/* Caller must hold the list lock */
static struct msm_dmov_cmd *start_ready_cmd(unsigned ch, int adm)
{
	struct msm_dmov_cmd *cmd;

	if (list_empty(&dmov_conf[adm].ready_commands[ch]))
		return NULL;

	cmd = list_entry(dmov_conf[adm].ready_commands[ch].next, typeof(*cmd),
			 list);
	list_del(&cmd->list);
	if (cmd->exec_func)
		cmd->exec_func(cmd);
	list_add_tail(&cmd->list, &dmov_conf[adm].active_commands[ch]);
	if (!dmov_conf[adm].channel_active)
		enable_irq(dmov_conf[adm].irq);
	dmov_conf[adm].channel_active |= BIT(ch);
	PRINT_IO("msm dmov enqueue command, %x, ch %d\n", cmd->cmdptr, ch);
	writel_relaxed(cmd->cmdptr, DMOV_REG(DMOV_CMD_PTR(ch), adm));

	return cmd;
}

static void msm_dmov_enqueue_cmd_ext_work(struct work_struct *work)
{
	struct msm_dmov_cmd *cmd =
		container_of(work, struct msm_dmov_cmd, work);
	unsigned id = cmd->id;
	unsigned status;
	unsigned long flags;
	int adm = DMOV_ID_TO_ADM(id);
	int ch = DMOV_ID_TO_CHAN(id);

	mutex_lock(&dmov_conf[adm].lock);
	if (dmov_conf[adm].clk_ctl == CLK_DIS) {
		status = msm_dmov_clk_on(adm);
		if (status != 0)
			goto error;
	}
	dmov_conf[adm].clk_ctl = CLK_EN;

	spin_lock_irqsave(&dmov_conf[adm].list_lock, flags);

	cmd = list_entry(dmov_conf[adm].staged_commands[ch].next, typeof(*cmd),
			 list);
	list_del(&cmd->list);
	list_add_tail(&cmd->list, &dmov_conf[adm].ready_commands[ch]);
	status = readl_relaxed(DMOV_REG(DMOV_STATUS(ch), adm));
	if (status & DMOV_STATUS_CMD_PTR_RDY) {
		PRINT_IO("msm_dmov_enqueue_cmd(%d), start command, status %x\n",
			id, status);
		cmd = start_ready_cmd(ch, adm);
		/*
		 * We added something to the ready list, and still hold the
		 * list lock. Thus, no need to check for cmd == NULL
		 */
		if (cmd->toflush) {
			int flush = (cmd->toflush == GRACEFUL) ? 1 << 31 : 0;
			writel_relaxed(flush, DMOV_REG(DMOV_FLUSH0(ch), adm));
		}
	} else {
		cmd->toflush = 0;
		if (list_empty(&dmov_conf[adm].active_commands[ch]) &&
		    !list_empty(&dmov_conf[adm].ready_commands[ch]))
			PRINT_ERROR("msm_dmov_enqueue_cmd_ext(%d), stalled, "
				"status %x\n", id, status);
		PRINT_IO("msm_dmov_enqueue_cmd(%d), enqueue command, status "
		    "%x\n", id, status);
	}
	if (!dmov_conf[adm].channel_active) {
		dmov_conf[adm].clk_ctl = CLK_TO_BE_DIS;
		schedule_delayed_work(&dmov_conf[adm].work, (HZ/10));
	}
	spin_unlock_irqrestore(&dmov_conf[adm].list_lock, flags);
error:
	mutex_unlock(&dmov_conf[adm].lock);
}

static void __msm_dmov_enqueue_cmd_ext(unsigned id, struct msm_dmov_cmd *cmd)
{
	int adm = DMOV_ID_TO_ADM(id);
	int ch = DMOV_ID_TO_CHAN(id);
	unsigned long flags;
	cmd->id = id;
	cmd->toflush = 0;

	spin_lock_irqsave(&dmov_conf[adm].list_lock, flags);
	list_add_tail(&cmd->list, &dmov_conf[adm].staged_commands[ch]);
	queue_work(dmov_conf[adm].cmd_wq, &cmd->work);
	spin_unlock_irqrestore(&dmov_conf[adm].list_lock, flags);
}

void msm_dmov_enqueue_cmd_ext(unsigned id, struct msm_dmov_cmd *cmd)
{
	INIT_WORK(&cmd->work, msm_dmov_enqueue_cmd_ext_work);
	__msm_dmov_enqueue_cmd_ext(id, cmd);
}
EXPORT_SYMBOL(msm_dmov_enqueue_cmd_ext);

void msm_dmov_enqueue_cmd(unsigned id, struct msm_dmov_cmd *cmd)
{
	/* Disable callback function (for backwards compatibility) */
	cmd->exec_func = NULL;
	INIT_WORK(&cmd->work, msm_dmov_enqueue_cmd_ext_work);
	__msm_dmov_enqueue_cmd_ext(id, cmd);
}
EXPORT_SYMBOL(msm_dmov_enqueue_cmd);

void msm_dmov_flush(unsigned int id, int graceful)
{
	unsigned long irq_flags;
	int ch = DMOV_ID_TO_CHAN(id);
	int adm = DMOV_ID_TO_ADM(id);
	int flush = graceful ? DMOV_FLUSH_TYPE : 0;
	struct msm_dmov_cmd *cmd;

	spin_lock_irqsave(&dmov_conf[adm].list_lock, irq_flags);
	/* XXX not checking if flush cmd sent already */
	if (!list_empty(&dmov_conf[adm].active_commands[ch])) {
		PRINT_IO("msm_dmov_flush(%d), send flush cmd\n", id);
		writel_relaxed(flush, DMOV_REG(DMOV_FLUSH0(ch), adm));
	}
	list_for_each_entry(cmd, &dmov_conf[adm].staged_commands[ch], list)
		cmd->toflush = graceful ? GRACEFUL : NONGRACEFUL;
	/* spin_unlock_irqrestore has the necessary barrier */
	spin_unlock_irqrestore(&dmov_conf[adm].list_lock, irq_flags);
}
EXPORT_SYMBOL(msm_dmov_flush);

struct msm_dmov_exec_cmdptr_cmd {
	struct msm_dmov_cmd dmov_cmd;
	struct completion complete;
	unsigned id;
	unsigned int result;
	struct msm_dmov_errdata err;
};

static void
dmov_exec_cmdptr_complete_func(struct msm_dmov_cmd *_cmd,
			       unsigned int result,
			       struct msm_dmov_errdata *err)
{
	struct msm_dmov_exec_cmdptr_cmd *cmd = container_of(_cmd, struct msm_dmov_exec_cmdptr_cmd, dmov_cmd);
	cmd->result = result;
	if (result != 0x80000002 && err)
		memcpy(&cmd->err, err, sizeof(struct msm_dmov_errdata));

	complete(&cmd->complete);
}

int msm_dmov_exec_cmd(unsigned id, unsigned int cmdptr)
{
	struct msm_dmov_exec_cmdptr_cmd cmd;

	PRINT_FLOW("dmov_exec_cmdptr(%d, %x)\n", id, cmdptr);

	cmd.dmov_cmd.cmdptr = cmdptr;
	cmd.dmov_cmd.complete_func = dmov_exec_cmdptr_complete_func;
	cmd.dmov_cmd.exec_func = NULL;
	cmd.id = id;
	cmd.result = 0;
	INIT_WORK_ONSTACK(&cmd.dmov_cmd.work, msm_dmov_enqueue_cmd_ext_work);
	init_completion(&cmd.complete);

	__msm_dmov_enqueue_cmd_ext(id, &cmd.dmov_cmd);
	wait_for_completion_io(&cmd.complete);

	if (cmd.result != 0x80000002) {
		PRINT_ERROR("dmov_exec_cmdptr(%d): ERROR, result: %x\n", id, cmd.result);
		PRINT_ERROR("dmov_exec_cmdptr(%d):  flush: %x %x %x %x\n",
			id, cmd.err.flush[0], cmd.err.flush[1], cmd.err.flush[2], cmd.err.flush[3]);
		return -EIO;
	}
	PRINT_FLOW("dmov_exec_cmdptr(%d, %x) done\n", id, cmdptr);
	return 0;
}
EXPORT_SYMBOL(msm_dmov_exec_cmd);

static void fill_errdata(struct msm_dmov_errdata *errdata, int ch, int adm)
{
	errdata->flush[0] = readl_relaxed(DMOV_REG(DMOV_FLUSH0(ch), adm));
	errdata->flush[1] = readl_relaxed(DMOV_REG(DMOV_FLUSH1(ch), adm));
	errdata->flush[2] = 0;
	errdata->flush[3] = readl_relaxed(DMOV_REG(DMOV_FLUSH3(ch), adm));
	errdata->flush[4] = readl_relaxed(DMOV_REG(DMOV_FLUSH4(ch), adm));
	errdata->flush[5] = readl_relaxed(DMOV_REG(DMOV_FLUSH5(ch), adm));
}

static irqreturn_t msm_dmov_isr(int irq, void *dev_id)
{
	unsigned int int_status;
	unsigned int mask;
	unsigned int id;
	unsigned int ch;
	unsigned long irq_flags;
	unsigned int ch_status;
	unsigned int ch_result;
	unsigned int valid = 0;
	struct msm_dmov_cmd *cmd;
	int adm = DMOV_IRQ_TO_ADM(irq);

	mutex_lock(&dmov_conf[adm].lock);
	/* read and clear isr */
	int_status = readl_relaxed(DMOV_REG(DMOV_ISR, adm));
	PRINT_FLOW("msm_datamover_irq_handler: DMOV_ISR %x\n", int_status);

	spin_lock_irqsave(&dmov_conf[adm].list_lock, irq_flags);
	while (int_status) {
		mask = int_status & -int_status;
		ch = fls(mask) - 1;
		id = DMOV_CHAN_ADM_TO_ID(ch, adm);
		PRINT_FLOW("msm_datamover_irq_handler %08x %08x id %d\n", int_status, mask, id);
		int_status &= ~mask;
		ch_status = readl_relaxed(DMOV_REG(DMOV_STATUS(ch), adm));
		if (!(ch_status & DMOV_STATUS_RSLT_VALID)) {
			PRINT_FLOW("msm_datamover_irq_handler id %d, "
				"result not valid %x\n", id, ch_status);
			continue;
		}
		do {
			valid = 1;
			ch_result = readl_relaxed(DMOV_REG(DMOV_RSLT(ch), adm));
			if (list_empty(&dmov_conf[adm].active_commands[ch])) {
				PRINT_ERROR("msm_datamover_irq_handler id %d, got result "
					"with no active command, status %x, result %x\n",
					id, ch_status, ch_result);
				cmd = NULL;
			} else {
				cmd = list_entry(dmov_conf[adm].
					active_commands[ch].next, typeof(*cmd),
					list);
			}
			PRINT_FLOW("msm_datamover_irq_handler id %d, status %x, result %x\n", id, ch_status, ch_result);
			if (ch_result & DMOV_RSLT_DONE) {
				PRINT_FLOW("msm_datamover_irq_handler id %d, status %x\n",
					id, ch_status);
				PRINT_IO("msm_datamover_irq_handler id %d, got result "
					"for %p, result %x\n", id, cmd, ch_result);
				if (cmd) {
					list_del(&cmd->list);
					cmd->complete_func(cmd, ch_result, NULL);
				}
			}
			if (ch_result & DMOV_RSLT_FLUSH) {
				struct msm_dmov_errdata errdata;

				fill_errdata(&errdata, ch, adm);
				PRINT_FLOW("msm_datamover_irq_handler id %d, status %x\n", id, ch_status);
				PRINT_FLOW("msm_datamover_irq_handler id %d, flush, result %x, flush0 %x\n", id, ch_result, errdata.flush[0]);
				if (cmd) {
					list_del(&cmd->list);
					cmd->complete_func(cmd, ch_result, &errdata);
				}
			}
			if (ch_result & DMOV_RSLT_ERROR) {
				struct msm_dmov_errdata errdata;

				fill_errdata(&errdata, ch, adm);

				PRINT_ERROR("msm_datamover_irq_handler id %d, status %x\n", id, ch_status);
				PRINT_ERROR("msm_datamover_irq_handler id %d, error, result %x, flush0 %x\n", id, ch_result, errdata.flush[0]);
				if (cmd) {
					list_del(&cmd->list);
					cmd->complete_func(cmd, ch_result, &errdata);
				}
				/* this does not seem to work, once we get an error */
				/* the datamover will no longer accept commands */
				writel_relaxed(0, DMOV_REG(DMOV_FLUSH0(ch),
					       adm));
			}
			rmb();
			ch_status = readl_relaxed(DMOV_REG(DMOV_STATUS(ch),
						  adm));
			PRINT_FLOW("msm_datamover_irq_handler id %d, status %x\n", id, ch_status);
			if (ch_status & DMOV_STATUS_CMD_PTR_RDY)
				start_ready_cmd(ch, adm);
		} while (ch_status & DMOV_STATUS_RSLT_VALID);
		if (list_empty(&dmov_conf[adm].active_commands[ch]) &&
		    list_empty(&dmov_conf[adm].ready_commands[ch]))
			dmov_conf[adm].channel_active &= ~(1U << ch);
		PRINT_FLOW("msm_datamover_irq_handler id %d, status %x\n", id, ch_status);
	}
	spin_unlock_irqrestore(&dmov_conf[adm].list_lock, irq_flags);

	if (!dmov_conf[adm].channel_active && valid) {
		disable_irq_nosync(dmov_conf[adm].irq);
		dmov_conf[adm].clk_ctl = CLK_TO_BE_DIS;
		schedule_delayed_work(&dmov_conf[adm].work, (HZ/10));
	}

	mutex_unlock(&dmov_conf[adm].lock);
	return valid ? IRQ_HANDLED : IRQ_NONE;
}

static int msm_dmov_suspend_late(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int adm = (pdev->id >= 0) ? pdev->id : 0;
	mutex_lock(&dmov_conf[adm].lock);
	if (dmov_conf[adm].clk_ctl == CLK_TO_BE_DIS) {
		BUG_ON(dmov_conf[adm].channel_active);
		msm_dmov_clk_off(adm);
		dmov_conf[adm].clk_ctl = CLK_DIS;
	}
	mutex_unlock(&dmov_conf[adm].lock);
	return 0;
}

static int msm_dmov_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int msm_dmov_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static int msm_dmov_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: idling...\n");
	return 0;
}

static struct dev_pm_ops msm_dmov_dev_pm_ops = {
	.runtime_suspend = msm_dmov_runtime_suspend,
	.runtime_resume = msm_dmov_runtime_resume,
	.runtime_idle = msm_dmov_runtime_idle,
	.suspend = msm_dmov_suspend_late,
};

static int msm_dmov_init_clocks(struct platform_device *pdev)
{
	int adm = (pdev->id >= 0) ? pdev->id : 0;
	int ret;

	dmov_conf[adm].clk = clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(dmov_conf[adm].clk)) {
		printk(KERN_ERR "%s: Error getting adm_clk\n", __func__);
		dmov_conf[adm].clk = NULL;
		return -ENOENT;
	}

	dmov_conf[adm].pclk = clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(dmov_conf[adm].pclk)) {
		dmov_conf[adm].pclk = NULL;
		/* pclk not present on all SoCs, don't bail on failure */
	}

	dmov_conf[adm].ebiclk = clk_get(&pdev->dev, "mem_clk");
	if (IS_ERR(dmov_conf[adm].ebiclk)) {
		dmov_conf[adm].ebiclk = NULL;
		/* ebiclk not present on all SoCs, don't bail on failure */
	} else {
		ret = clk_set_rate(dmov_conf[adm].ebiclk, 27000000);
		if (ret)
			return -ENOENT;
	}

	return 0;
}

static void config_datamover(int adm)
{
#ifdef CONFIG_MSM_ADM3
	int i;
	for (i = 0; i < MSM_DMOV_CHANNEL_COUNT; i++) {
		struct msm_dmov_chan_conf *chan_conf =
			dmov_conf[adm].chan_conf;
		unsigned conf;
		/* Only configure scorpion channels */
		if (chan_conf[i].sd <= 1) {
			conf = readl_relaxed(DMOV_REG(DMOV_CONF(i), adm));
			conf &= ~DMOV_CONF_SD(7);
			conf |= DMOV_CONF_SD(chan_conf[i].sd);
			writel_relaxed(conf | DMOV_CONF_SHADOW_EN,
			       DMOV_REG(DMOV_CONF(i), adm));
		}
	}
	for (i = 0; i < MSM_DMOV_CRCI_COUNT; i++) {
		struct msm_dmov_crci_conf *crci_conf =
			dmov_conf[adm].crci_conf;

		writel_relaxed(DMOV_CRCI_CTL_BLK_SZ(crci_conf[i].blk_size),
		       DMOV_REG(DMOV_CRCI_CTL(i), adm));
	}
#endif
}

static int msm_dmov_probe(struct platform_device *pdev)
{
	int adm = (pdev->id >= 0) ? pdev->id : 0;
	int i;
	int ret;
	struct msm_dmov_pdata *pdata = pdev->dev.platform_data;
	struct resource *irqres =
		platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	struct resource *mres =
		platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (pdata) {
		dmov_conf[adm].sd = pdata->sd;
		dmov_conf[adm].sd_size = pdata->sd_size;
	}
	if (!dmov_conf[adm].sd_size)
		return -ENXIO;

	if (!irqres || !irqres->start)
		return -ENXIO;
	dmov_conf[adm].irq = irqres->start;

	if (!mres || !mres->start)
		return -ENXIO;
	dmov_conf[adm].base = ioremap_nocache(mres->start, resource_size(mres));
	if (!dmov_conf[adm].base)
		return -ENOMEM;

	dmov_conf[adm].cmd_wq = alloc_ordered_workqueue("dmov%d_wq", 0, adm);
	if (!dmov_conf[adm].cmd_wq) {
		PRINT_ERROR("Couldn't allocate ADM%d workqueue.\n", adm);
		ret = -ENOMEM;
		goto out_map;
	}

	ret = request_threaded_irq(dmov_conf[adm].irq, NULL, msm_dmov_isr,
				   IRQF_ONESHOT, "msmdatamover", NULL);
	if (ret) {
		PRINT_ERROR("Requesting ADM%d irq %d failed\n", adm,
			dmov_conf[adm].irq);
		goto out_wq;
	}
	disable_irq(dmov_conf[adm].irq);
	ret = msm_dmov_init_clocks(pdev);
	if (ret) {
		PRINT_ERROR("Requesting ADM%d clocks failed\n", adm);
		goto out_irq;
	}
	ret = msm_dmov_clk_on(adm);
	if (ret) {
		PRINT_ERROR("Enabling ADM%d clocks failed\n", adm);
		goto out_irq;
	}

	config_datamover(adm);
	for (i = 0; i < MSM_DMOV_CHANNEL_COUNT; i++) {
		INIT_LIST_HEAD(&dmov_conf[adm].staged_commands[i]);
		INIT_LIST_HEAD(&dmov_conf[adm].ready_commands[i]);
		INIT_LIST_HEAD(&dmov_conf[adm].active_commands[i]);

		writel_relaxed(DMOV_RSLT_CONF_IRQ_EN
		     | DMOV_RSLT_CONF_FORCE_FLUSH_RSLT,
		     DMOV_REG(DMOV_RSLT_CONF(i), adm));
	}
	wmb();
	msm_dmov_clk_off(adm);
	return ret;
out_irq:
	free_irq(dmov_conf[adm].irq, NULL);
out_wq:
	destroy_workqueue(dmov_conf[adm].cmd_wq);
out_map:
	iounmap(dmov_conf[adm].base);
	return ret;
}

static struct platform_driver msm_dmov_driver = {
	.probe = msm_dmov_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.pm = &msm_dmov_dev_pm_ops,
	},
};

/* static int __init */
static int __init msm_init_datamover(void)
{
	int ret;
	ret = platform_driver_register(&msm_dmov_driver);
	if (ret)
		return ret;
	return 0;
}
arch_initcall(msm_init_datamover);
