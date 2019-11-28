/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#ifdef CONFIG_OF
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#endif
#include "mdla.h"
#include "mdla_hw_reg.h"
#include "mdla_ion.h"
#include "mdla_trace.h"
#include "mdla_debug.h"
#include "mdla_util.h"
#include "mdla_power_ctrl.h"
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
#include "apusys_power.h"
#endif


static struct platform_device *mdlactlPlatformDevice;

#if 0
static u32 reg_read(void *base, u32 offset)
{
	return ioread32(base + offset);
}

static void reg_write(void *base, u32 offset, u32 value)
{
	iowrite32(value, base + offset);
}

static void reg_set(void *base, u32 offset, u32 value)
{
	reg_write(base, offset, reg_read(base, offset) | value);
}

static void reg_clr(void *base, u32 offset, u32 value)
{
	reg_write(base, offset, reg_read(base, offset) & (~value));
}
#endif

static void mdla_cfg_write_with_mdlaid(u32 mdlaid, u32 value, u32 offset)
{
	iowrite32(value, mdla_reg_control[mdlaid].apu_mdla_config_top + offset);
}


static void mdla_reg_write_with_mdlaid(u32 mdlaid, u32 value, u32 offset)
{
	iowrite32(value,
		mdla_reg_control[mdlaid].apu_mdla_cmde_mreg_top + offset);
}

#define mdla_cfg_set_with_mdlaid(mdlaid, mask, offset) \
	mdla_cfg_write_with_mdlaid(mdlaid,\
	mdla_cfg_read_with_mdlaid(mdlaid, offset) | (mask), (offset))


int mdla_dts_map(struct platform_device *pdev)
{
	struct resource *apu_mdla_command; /* IO mem resources */
	struct resource *apu_mdla_config; /* IO mem resources */
	struct resource *apu_mdla_biu; /* IO mem resources */
	struct resource *apu_mdla_gsm; /* IO mem resources */
	struct resource *apu_conn; /* IO mem resources */
	//struct resource *infracfg_ao; /* IO mem resources */
	struct device *dev = &pdev->dev;
	struct device_node *node;

	int rc = 0;
	int i;

	mdlactlPlatformDevice = pdev;

	dev_info(dev, "Device Tree Probing\n");

	for (i = 0; i < mdla_max_num_core; i++) {
		/* Get iospace for MDLA Config */
		apu_mdla_config = platform_get_resource(pdev,
			IORESOURCE_MEM, i+(i*2));
		if (!apu_mdla_config) {
			mdla_drv_debug("invalid address\n");
			return -ENODEV;
		}

		/* Get iospace for MDLA Command */
		apu_mdla_command = platform_get_resource(pdev,
			IORESOURCE_MEM, i+1+(i*2));
		if (!apu_mdla_command) {
			dev_info(dev, "invalid address\n");
			return -ENODEV;
		}

		/* Get iospace for MDAL PMU */
		apu_mdla_biu = platform_get_resource(pdev,
			IORESOURCE_MEM, i+2+(i*2));
		if (!apu_mdla_biu) {
			dev_info(dev, "apu_mdla_biu address\n");
			return -ENODEV;
		}

		mdla_reg_control[i].apu_mdla_config_top = ioremap_nocache(
				apu_mdla_config->start,
				apu_mdla_config->end -
				apu_mdla_config->start + 1);
		if (!mdla_reg_control[i].apu_mdla_config_top) {
			dev_info(dev, "mtk_mdla: Could not allocate iomem\n");
			rc = -EIO;
			return rc;
		}

		mdla_reg_control[i].apu_mdla_cmde_mreg_top = ioremap_nocache(
				apu_mdla_command->start,
				apu_mdla_command->end -
				apu_mdla_command->start + 1);
		if (!mdla_reg_control[i].apu_mdla_cmde_mreg_top) {
			dev_info(dev, "mtk_mdla: Could not allocate iomem\n");
			rc = -EIO;
			return rc;
		}

		mdla_reg_control[i].apu_mdla_biu_top = ioremap_nocache(
				apu_mdla_biu->start,
				apu_mdla_biu->end - apu_mdla_biu->start + 1);
		if (!mdla_reg_control[i].apu_mdla_biu_top) {
			dev_info(dev, "mtk_mdla: Could not allocate iomem\n");
			rc = -EIO;
			return rc;
		}

		dev_info(dev, "mdla_config_top at 0x%08lx mapped to 0x%08lx\n",
				(unsigned long __force)apu_mdla_config->start,
				(unsigned long __force)apu_mdla_config->end);

		dev_info(dev, "mdla_command at 0x%08lx mapped to 0x%08lx\n",
				(unsigned long __force)apu_mdla_command->start,
				(unsigned long __force)apu_mdla_command->end);

		dev_info(dev, "mdla_biu_top at 0x%08lx mapped to 0x%08lx\n",
				(unsigned long __force)apu_mdla_biu->start,
				(unsigned long __force)apu_mdla_biu->end);

	}


	/* Get iospace GSM */
	apu_mdla_gsm = platform_get_resource(pdev,
		IORESOURCE_MEM,
		6);
	if (!apu_mdla_gsm) {
		dev_info(dev, "apu_gsm address\n");
		return -ENODEV;
	}

	/* Get iospace APU CONN */
	apu_conn = platform_get_resource(pdev,
		IORESOURCE_MEM,
		7);
	if (!apu_conn) {
		mdla_drv_debug("apu_conn address\n");
		return -ENODEV;
	}

#if 0
	/* Get INFRA CFG */
	infracfg_ao = platform_get_resource(pdev, IORESOURCE_MEM, 5);
	if (!infracfg_ao) {
		mdla_drv_debug("infracfg_ao address\n");
		return -ENODEV;
	}
#endif

	apu_mdla_gsm_top = ioremap_nocache(apu_mdla_gsm->start,
			apu_mdla_gsm->end - apu_mdla_gsm->start + 1);
	if (!apu_mdla_gsm_top) {
		dev_info(dev, "mtk_mdla: Could not allocate iomem\n");
		rc = -EIO;
		return rc;
	}
	apu_mdla_gsm_base = (void *) apu_mdla_gsm->start;
	pr_info("%s: apu_mdla_gsm_top: %p, apu_mdla_gsm_base: %p\n",
		__func__, apu_mdla_gsm_top, apu_mdla_gsm_base);

	apu_conn_top = ioremap_nocache(apu_conn->start,
			apu_conn->end - apu_conn->start + 1);
	if (!apu_conn_top) {
		mdla_drv_debug("mtk_mdla: Could not allocate apu_conn_top\n");
		rc = -EIO;
		return rc;
	}

	dev_info(dev, "apu_mdla_gsm at 0x%08lx mapped to 0x%08lx\n",
			(unsigned long __force)apu_mdla_gsm->start,
			(unsigned long __force)apu_mdla_gsm->end);

	dev_info(dev, "apu_conn_top at 0x%08lx mapped to 0x%08lx\n",
			(unsigned long __force)apu_conn->start,
			(unsigned long __force)apu_conn->end);

	node = pdev->dev.of_node;
	if (!node) {
		dev_info(dev, "get mdla device node err\n");
		return rc;
	}

	for (i = 0; i < mdla_max_num_core; i++) {
		mdla_irqdesc[i].irq  = irq_of_parse_and_map(node, i);
		if (!mdla_irqdesc[i].irq) {
			dev_info(dev, "get mdla irq: %d failed\n", i);
			return rc;
		}
		rc = request_irq(mdla_irqdesc[i].irq, mdla_irqdesc[i].handler,
				IRQF_TRIGGER_HIGH, DRIVER_NAME, dev);

		if (rc) {

			dev_info(dev, "mtk_mdla[%d]: Could not allocate interrupt %d.\n",
					i, mdla_irqdesc[i].irq);
#if 0
			/*IRQF_TRIGGER_HIGH for Simulator workaroud only*/
			rc = request_irq(mdla_irqdesc[i].irq,
			mdla_irqdesc[i].handler,
			IRQF_TRIGGER_HIGH,
			DRIVER_NAME, dev);
			if (rc) {
				dev_info(dev, "mtk_mdla[%d]: Could not allocate interrupt %d.\n",
						i, mdla_irqdesc[i].irq);
				return rc;
			}
#endif
		}
		dev_info(dev, "request_irq %d done\n", mdla_irqdesc[i].irq);
	}

	return 0;
}

void mdla_reset(int core, int res)
{
	const char *str = mdla_get_reason_str(res);
	unsigned long flags;

	/*use power down==>power on apis insted bus protect init*/
	mdla_drv_debug("%s: MDLA RESET: %s(%d)\n", __func__,
		str, res);

	spin_lock_irqsave(&mdla_devices[core].hw_lock, flags);
	mdla_cfg_write_with_mdlaid(core, 0xffffffff, MDLA_CG_CLR);

	mdla_reg_write_with_mdlaid(core, MDLA_IRQ_MASK & ~(MDLA_IRQ_SWCMD_DONE),
		MREG_TOP_G_INTP2);

	/* for DCM and CG */
	mdla_reg_write_with_mdlaid(core, cfg_eng0, MREG_TOP_ENG0);
	mdla_reg_write_with_mdlaid(core, cfg_eng1, MREG_TOP_ENG1);
	mdla_reg_write_with_mdlaid(core, cfg_eng2, MREG_TOP_ENG2);
	/*TODO, 0x0 after verification*/
	mdla_reg_write_with_mdlaid(core, cfg_eng11, MREG_TOP_ENG11);

#ifdef CONFIG_MTK_MDLA_ION
	mdla_cfg_set_with_mdlaid(core, MDLA_AXI_CTRL_MASK, MDLA_AXI_CTRL);
	mdla_cfg_set_with_mdlaid(core, MDLA_AXI_CTRL_MASK, MDLA_AXI1_CTRL);
#endif
	spin_unlock_irqrestore(&mdla_devices[core].hw_lock, flags);

	mdla_profile_reset(core, str);//TODO, to confirm multi mdla settings

}


irqreturn_t mdla_interrupt(u32 mdlaid)
{
	//FIXME: h/w reg need support MDLA 1.5 & dual core
	u32 status_int = mdla_reg_read_with_mdlaid(mdlaid, MREG_TOP_G_INTP0);
	unsigned long flags;
	u32 id;

	spin_lock_irqsave(&mdla_devices[mdlaid].hw_lock, flags);
	/*Toggle for Latch Fin1 Tile ID*/
	mdla_reg_read_with_mdlaid(mdlaid, MREG_TOP_G_FIN0);
	id = mdla_reg_read_with_mdlaid(mdlaid, MREG_TOP_G_FIN3);
	pmu_reg_save(mdlaid);//pmu need refine for multi core

	mdla_devices[mdlaid].max_cmd_id = id;

	if (status_int & MDLA_IRQ_PMU_INTE)
		mdla_reg_write_with_mdlaid(
		mdlaid, MDLA_IRQ_PMU_INTE, MREG_TOP_G_INTP0);

	spin_unlock_irqrestore(&mdla_devices[mdlaid].hw_lock, flags);

	complete(&mdla_devices[mdlaid].command_done);

	return IRQ_HANDLED;
}

unsigned int mdla_multi_core_is_swcmd_cnt(unsigned int core_id)
{
	unsigned int is_swcmd_done = 0;

	if (mdla_reg_read_with_mdlaid(core_id, MREG_TOP_SWCMD_DONE_CNT)|
		(0x00f0<<(core_id * 4)))
		is_swcmd_done |= 1<<core_id;

	return is_swcmd_done;
}

void mdla_multi_core_sw_rst(unsigned int core_id)
{
	mdla_cfg_write_with_mdlaid(core_id, 0xff, MDLA_SW_RST);
	mdla_cfg_write_with_mdlaid(core_id, 0x0, MDLA_SW_RST);
}

void mdla_multi_core_sync_rst_done(void)
{
	int i = 0;

	for (i = 0; i < mdla_max_num_core; i++)
		while (mdla_multi_core_is_swcmd_cnt(i))
			udelay(10);
}

int mdla_zero_skip_detect(int core_id)
{
	u32 dde_debug_if_0, dde_debug_if_2, dde_it_front_c_invalid;

	dde_debug_if_0 =
		mdla_reg_read_with_mdlaid(core_id, MREG_DDE_DEBUG_IF_0);
	dde_debug_if_2 =
		mdla_reg_read_with_mdlaid(core_id, MREG_DDE_DEBUG_IF_2);
	dde_it_front_c_invalid =
		mdla_reg_read_with_mdlaid(core_id, MREG_DDE_IT_FRONT_C_INVALID);

	if (dde_debug_if_0 == 0x6) {
		if ((dde_debug_if_2 == dde_it_front_c_invalid) ||
			(dde_debug_if_2 == (dde_it_front_c_invalid/2))) {
			mdla_timeout_debug("core:%d, %s: match zero skip issue\n",
				core_id,
				__func__);
			mdla_devices[core_id].mdla_dde_zero_skip_count++;
			return -1;
		}
	}
	return 0;
}

#ifndef __APUSYS_PREEMPTION__
int mdla_process_command(int core_id, struct command_entry *ce)
{
	dma_addr_t addr;
	u32 count;
	int ret = 0;
	unsigned long flags;

	if (ce == NULL) {
		mdla_cmd_debug("%s: invalid command entry: ce=%p\n",
			__func__, ce);
		return 0;
	}

	addr = ce->mva;
	count = ce->count;

	mdla_drv_debug("%s: count: %d, addr: %lx\n",
		__func__, ce->count,
		(unsigned long)addr);

	if (ret)
		return ret;

#if 0//TODO, pending for multi mdla cmd
	mdla_multi_core_sync_rst_done();
#endif

	//TODO fix it for multicore
	/* Issue command */
	spin_lock_irqsave(&mdla_devices[core_id].hw_lock, flags);
	ce->state = CE_RUN;
	mdla_reg_write_with_mdlaid(core_id, addr, MREG_TOP_G_CDMA1);
	mdla_reg_write_with_mdlaid(core_id, count, MREG_TOP_G_CDMA2);
	mdla_reg_write_with_mdlaid(core_id, count, MREG_TOP_G_CDMA3);
	spin_unlock_irqrestore(&mdla_devices[core_id].hw_lock, flags);

//	list_add_tail(&ce->list, &cmd_fin_list);

	return ret;
}
#endif


int mdla_run_command_codebuf_check(struct command_entry *ce)
{
	int i;
	__u32 *cmd_addr;

	if (ce->kva != NULL) {
		for (i = 0; i < ce->count; i++) {
			cmd_addr = (u32 *)(((char *)ce->kva) +
				(i * MREG_CMD_SIZE));
			if ((cmd_addr[36]&0xffff) == 0) {
				mdla_cmd_debug("%s: c_tile_shape_k=%08x, count: %u\n",
						__func__,
						(cmd_addr[36]&0xffff),
						i);
				return -1;
			}
		}
		cmd_addr = (u32 *)(((char *)ce->kva) +
			((ce->count-1) * MREG_CMD_SIZE));
		if ((cmd_addr[85] & 0x1000000) == 0) {
			mdla_cmd_debug("%s: mreg_cmd_tile_cnt_int=%08x, count: %u\n",
					__func__,
					cmd_addr[85],
					ce->count);
			return -1;
		}
	}

	return 0;
}


#ifdef __APUSYS_PREEMPTION__
static inline struct mdla_scheduler *mdla_get_scheduler(unsigned int core_id)
{
	return (core_id > MTK_MDLA_MAX_NUM) ?
		NULL : mdla_devices[core_id].scheduler;
}

/*
 * Enqueue one CE and start scheduler
 *
 * NOTE: scheduler->lock should be acquired by caller
 */
void mdla_enqueue_ce(unsigned int core_id, struct command_entry *ce)
{
	struct mdla_scheduler *scheduler = mdla_get_scheduler(core_id);

#ifdef DBG_MDLA_SCHEDULER_LOG
	mdla_cmd_debug("%s: ce = 0x%p\n", __func__, ce);
#endif
	if (!scheduler || !ce)
		return;

	init_completion(&ce->done);
	ce->queue_t = sched_clock();
	list_add_tail(&ce->node, &scheduler->ce_queue);

	/* there are CEs under processing */
	if (scheduler->processing_ce)
		return;

	/* there is no CE under processing: dequeue and trigger engine */
	scheduler->dequeue_ce(core_id);
	scheduler->issue_ce(core_id);
}

/*
 * Check the engine halt successfully or not,
 *
 * Return value:
 * 1. false if the halt_en is not set
 * 2. true if engine halt on command or tile
 * 3. false for other cases
 */
static bool mdla_check_halt_success(u32 fin_cid, u32 fin_tid, u32 stream0,
				    u32 stream1, struct command_entry *ce)
{
	if ((stream1 & MSK_MREG_TOP_G_STREAM1_HALT_EN) == 0)
		return false;

	/* command-based scheduling */
	if (((fin_cid + 1) == (stream0 & MSK_MREG_TOP_G_STREAM0_PROD_CMD_ID))
		&& (fin_tid >= (stream1 & MSK_MREG_TOP_G_STREAM1_PROD_TILE_ID)))
		return true;

	return false;
}

/*
 * Consume the processing_ce:
 * 1. get the finished command id and tile id from HW RGs
 * 2. check whether this batch completed
 *
 * Return value:
 * 1. return CE_DONE if all the batches in the CE completed.
 * 2. return CE_RUN if a batch completed, and we can go on to issue the next.
 * 3. return CE_NONE if the batch is still under processing.
 *
 * NOTE: scheduler->lock should be acquired by caller
 */
unsigned int mdla_process_ce(unsigned int core_id)
{
	unsigned long flags;
	struct command_entry *ce;
	struct mdla_scheduler *scheduler = mdla_get_scheduler(core_id);
	u32 fin_cid, fin_tid, stream0, stream1, irq_status;

	if (!scheduler)
		return CE_NONE;

	spin_lock_irqsave(&mdla_devices[core_id].hw_lock, flags);

	irq_status = mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_INTP0);
	fin_cid = mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_FIN0);
	fin_tid = mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_FIN1);
	stream0 = mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_STREAM0);
	stream1 = mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_STREAM1);

	/* handle PMU */
	pmu_reg_save();
	if (irq_status & MDLA_IRQ_PMU_INTE)
		mdla_reg_write_with_mdlaid(core_id, MDLA_IRQ_PMU_INTE,
					   MREG_TOP_G_INTP0);

	spin_unlock_irqrestore(&mdla_devices[core_id].hw_lock, flags);

#ifdef DBG_MDLA_SCHEDULER_LOG
	mdla_cmd_debug("%s: irq_status = 0x%x\n", __func__, irq_status);
	mdla_cmd_debug("%s: fin_cid = %d, fin_tid = %d\n",
		       __func__, fin_cid, fin_tid);
	mdla_cmd_debug("%s: stream0 = 0x%x, stream1 = 0x%x\n",
		       __func__, stream0, stream1);
#endif
	ce = scheduler->processing_ce;
	if (!ce)
		return CE_NONE;

#ifdef DBG_MDLA_SCHEDULER_LOG
	mdla_cmd_debug("%s: ce = 0x%p, ce->fin_cid = %d\n",
		       __func__, ce, ce->fin_cid);
#endif

	/* all command done for command-based scheduling */
	if (fin_cid == ce->count) {
		ce->fin_cid = fin_cid;
		return CE_DONE;
	}

	if (mdla_check_halt_success(fin_cid, fin_tid, stream0, stream1, ce)) {
		ce->fin_cid = fin_cid;
		return CE_RUN;
	}

	return CE_NONE;
}

#if 0
static inline u32 mdla_get_tilecnt(void *base_kva, u32 cid)
{
	return (*(u32 *)(base_kva + cid * MREG_CMD_SIZE +
		MREG_CMD_TILE_CNT_S) & MSK_MREG_TOP_G_STREAM1_PROD_TILE_ID);
}
#endif

/*
 * Issue the processing_ce to HW engine
 * NOTE: scheduler->lock should be acquired by caller
 */
void mdla_issue_ce(unsigned int core_id)
{
	dma_addr_t addr;
	u32 nr_cmd_to_issue, halt_cid, halt_tid, cmd_batch_size;
	struct mdla_scheduler *scheduler = mdla_get_scheduler(core_id);
	struct command_entry *ce;
	unsigned long flags;

	if (!scheduler)
		return;

	ce = scheduler->processing_ce;
	if (!ce)
		return;

	/* TODO: separate power on, dvfs, qos logic? */
	mdla_power_on(ce);

	if (ce->poweron_t == 0) {
		ce->poweron_t = sched_clock();
		ce->req_start_t = ce->poweron_t;
	}

	addr = ce->mva + ce->fin_cid * MREG_CMD_SIZE;
	nr_cmd_to_issue = ce->count - ce->fin_cid;
	cmd_batch_size = min(ce->cmd_batch_size, nr_cmd_to_issue);

	/* command-based scheduling */
	halt_cid = ce->fin_cid + cmd_batch_size + 1;
	halt_tid = 1;
#ifdef DBG_MDLA_SCHEDULER_LOG
	mdla_cmd_debug("%s: nr_cmd_to_issue = %d, addr = 0x%lx\n",
		       __func__, nr_cmd_to_issue, (unsigned long)addr);
	mdla_cmd_debug("%s: fin_cid = %d, halt_cid = %d\n",
		       __func__, ce->fin_cid, halt_cid);
#endif
	spin_lock_irqsave(&mdla_devices[core_id].hw_lock, flags);

	if (ce->preempted) {
		ce->preempted = false;
		/* set command address */
		mdla_reg_write_with_mdlaid(core_id,
			addr, MREG_TOP_G_CDMA1);
		/* set command number */
		mdla_reg_write_with_mdlaid(core_id,
			nr_cmd_to_issue, MREG_TOP_G_CDMA2);

		/* halt if this is not the last batch */
		if (halt_cid <= ce->count) {
			/* set command halt id */
			mdla_reg_write_with_mdlaid(core_id,
				halt_cid, MREG_TOP_G_STREAM0);
			/* set tile halt id */
			mdla_reg_write_with_mdlaid(core_id,
				halt_tid | MSK_MREG_TOP_G_STREAM1_HALT_EN,
						   MREG_TOP_G_STREAM1);
		} else {
			/* the last batch: clear halt_en */
			mdla_reg_write_with_mdlaid(core_id,
				0x0, MREG_TOP_G_STREAM0);
			mdla_reg_write_with_mdlaid(core_id,
					0x0, MREG_TOP_G_STREAM1);
		}

		/* trigger engine */
		mdla_reg_write_with_mdlaid(core_id,
			1, MREG_TOP_G_CDMA3);
	} else {
		if (halt_cid <= ce->count) {
			/* update command/tile halt id to resume engine */
			mdla_reg_write_with_mdlaid(core_id,
				halt_cid, MREG_TOP_G_STREAM0);
			mdla_reg_write_with_mdlaid(core_id,
				halt_tid | MSK_MREG_TOP_G_STREAM1_HALT_EN,
				MREG_TOP_G_STREAM1);
		} else {
			/* the last batch: resume engine by clearing halt_en */
			mdla_reg_write_with_mdlaid(core_id,
				0x0, MREG_TOP_G_STREAM0);
			mdla_reg_write_with_mdlaid(core_id,
				0x0, MREG_TOP_G_STREAM1);
		}
	}

	spin_unlock_irqrestore(&mdla_devices[core_id].hw_lock, flags);
}

/*
 * Move the processing_ce to completed_ce_queue
 * NOTE: scheduler->lock should be acquired by caller
 */
void mdla_complete_ce(unsigned int core_id)
{
	struct mdla_scheduler *scheduler = mdla_get_scheduler(core_id);

#ifdef DBG_MDLA_SCHEDULER_LOG
	mdla_cmd_debug("%s: processing_ce = 0x%p\n", __func__,
		       scheduler->processing_ce);
#endif
	if (!scheduler)
		return;

	scheduler->processing_ce->req_end_t = sched_clock();
	complete(&scheduler->processing_ce->done);

	list_add_tail(&scheduler->processing_ce->node,
		      &scheduler->completed_ce_queue);
	scheduler->processing_ce = NULL;
}

/*
 * Procedure on preempting CE.
 * Because MDLA v1.0 does not support clearing CMDE on halting,
 * we reset the engine instead.
 *
 */
void mdla_preempt_ce(unsigned int core_id)
{
	struct mdla_scheduler *scheduler = mdla_get_scheduler(core_id);
	unsigned long flags;

#ifdef DBG_MDLA_SCHEDULER_LOG
	u64 t1 = sched_clock(), t2;
#endif
	if (!scheduler)
		return;

	/* reset the mdla engine on preemption */
	spin_lock_irqsave(&mdla_devices[core_id].hw_lock, flags);
	mdla_reset(core_id, REASON_PREEMPTION);
	spin_unlock_irqrestore(&mdla_devices[core_id].hw_lock, flags);

#ifdef DBG_MDLA_SCHEDULER_LOG
	t2 = sched_clock();
	mdla_cmd_debug("%s: reset mdla takes %ld ns\n", __func__, t2 - t1);
#endif
}

/*
 * Dequeue a prioritized CE from queues, handle the context switch of
 * the original processing_ce, and set the prioritized CE as processing_ce.
 *
 * NOTE: scheduler->lock should be acquired by caller
 */
void mdla_dequeue_ce(unsigned int core_id)
{
	struct mdla_scheduler *scheduler = mdla_get_scheduler(core_id);
	struct command_entry *prioritized_ce = NULL;

	if (!scheduler)
		return;

	/* get one CE from the CE queue */
	prioritized_ce = list_first_entry_or_null(&scheduler->ce_queue,
						  struct command_entry, node);

	/* return if we don't need to update the processing_ce */
	if (!prioritized_ce)
		return;

#ifdef DBG_MDLA_SCHEDULER_LOG
	mdla_cmd_debug("%s: prioritized_ce = 0x%p\n", __func__, prioritized_ce);
#endif

	/* remove prioritized CE from its queue */
	list_del(&prioritized_ce->node);

	if (scheduler->processing_ce) {
		scheduler->processing_ce->preempted = true;
		if (scheduler->preempt_ce)
			scheduler->preempt_ce(core_id);
		/* move the current CE to the head of CE queue */
		list_add(&scheduler->processing_ce->node, &scheduler->ce_queue);
	}

	scheduler->processing_ce = prioritized_ce;
}

/*
 * Scheduler to process the current CE, and pick the next one to issue
 * NOTE: mdla_scheduler would be invoked in ISR
 */
irqreturn_t mdla_scheduler(unsigned int core_id)
{
	struct mdla_scheduler *scheduler = mdla_get_scheduler(core_id);
	unsigned long flags;
	unsigned int status;

	if (!scheduler)
		return IRQ_NONE;

	spin_lock_irqsave(&scheduler->lock, flags);

	/* process the current CE */
	status = scheduler->process_ce(core_id);
	if (status == CE_DONE) {
		scheduler->complete_ce(core_id);
	} else if (status == CE_NONE) {
		/* nothing to do but wait for the engine completed */
		spin_unlock_irqrestore(&scheduler->lock, flags);
		return IRQ_HANDLED;
	}

	/* get the next CE to be processed */
	scheduler->dequeue_ce(core_id);

	if (scheduler->processing_ce)
		scheduler->issue_ce(core_id);
	else
		// FIXME: scheduler->all_ce_done(core_id);
		scheduler->all_ce_done();

	spin_unlock_irqrestore(&scheduler->lock, flags);

	return IRQ_HANDLED;
}
#endif

void dump_timeout_debug_info(int core_id)
{
	u32 mreg_top_g_idle, c2r_exe_st, ste_debug_if_1;
	int i;

	mreg_top_g_idle = mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_IDLE);
	c2r_exe_st = mdla_reg_read_with_mdlaid(core_id, 0x0000);
	ste_debug_if_1 =
		mdla_reg_read_with_mdlaid(core_id, MREG_STE_DEBUG_IF_1);
	if (((ste_debug_if_1&0x1C0) != 0x0 && (ste_debug_if_1&0x3) == 0x3)) {
		mdla_timeout_debug(
				"Matched, %s, mdla_timeout:%d, mreg_top_g_idle: %08x, c2r_exe_st: %08x, ste_debug_if_1: %08x\n",
				__func__, mdla_timeout,
				mreg_top_g_idle, c2r_exe_st, ste_debug_if_1);
	} else {
		mdla_timeout_debug(
				"Not match, %s, mdla_timeout:%d, mreg_top_g_idle: %08x, c2r_exe_st: %08x, ste_debug_if_1: %08x\n",
				__func__, mdla_timeout,
				mreg_top_g_idle, c2r_exe_st, ste_debug_if_1);
	}

	for (i = 0x0000; i < 0x1000; i += 4)
		mdla_timeout_all_debug("apu_mdla_config_top+%04X: %08X\n",
				i, mdla_cfg_read_with_mdlaid(core_id, i));
	for (i = 0x0000; i < 0x1000; i += 4)
		mdla_timeout_debug("apu_mdla_cmde_mreg_top+%04X: %08X\n",
				i, mdla_reg_read_with_mdlaid(core_id, i));

}

void mdla_dump_reg(int core_id)
{
	mdla_timeout_debug("mdla_timeout\n");
	// TODO: too many registers, dump only debug required ones.
	dump_reg_cfg(core_id, MDLA_CG_CON);
	dump_reg_cfg(core_id, MDLA_SW_RST);
	dump_reg_cfg(core_id, MDLA_MBIST_MODE0);
	dump_reg_cfg(core_id, MDLA_MBIST_MODE1);
	dump_reg_cfg(core_id, MDLA_MBIST_CTL);
	dump_reg_cfg(core_id, MDLA_MBIST_DEFAULT_DELSEL);
	dump_reg_cfg(core_id, MDLA_RP_RST);
	dump_reg_cfg(core_id, MDLA_RP_CON);
	dump_reg_cfg(core_id, MDLA_AXI_CTRL);
	dump_reg_cfg(core_id, MDLA_AXI1_CTRL);

	dump_reg_top(core_id, MREG_TOP_G_REV);
	dump_reg_top(core_id, MREG_TOP_G_INTP0);
	dump_reg_top(core_id, MREG_TOP_G_INTP1);
	dump_reg_top(core_id, MREG_TOP_G_INTP2);
	dump_reg_top(core_id, MREG_TOP_G_CDMA0);
	dump_reg_top(core_id, MREG_TOP_G_CDMA1);
	dump_reg_top(core_id, MREG_TOP_G_CDMA2);
	dump_reg_top(core_id, MREG_TOP_G_CDMA3);
	dump_reg_top(core_id, MREG_TOP_G_CDMA4);
	dump_reg_top(core_id, MREG_TOP_G_CDMA5);
	dump_reg_top(core_id, MREG_TOP_G_CDMA6);
	dump_reg_top(core_id, MREG_TOP_G_CUR0);
	dump_reg_top(core_id, MREG_TOP_G_CUR1);
	dump_reg_top(core_id, MREG_TOP_G_FIN0);
	dump_reg_top(core_id, MREG_TOP_G_FIN1);
	dump_reg_top(core_id, MREG_TOP_G_FIN3);
	dump_reg_top(core_id, MREG_TOP_G_IDLE);

	/* for DCM and CG */
	dump_reg_top(core_id, MREG_TOP_ENG0);
	dump_reg_top(core_id, MREG_TOP_ENG1);
	dump_reg_top(core_id, MREG_TOP_ENG2);
	dump_reg_top(core_id, MREG_TOP_ENG11);
	dump_timeout_debug_info(core_id);
}

