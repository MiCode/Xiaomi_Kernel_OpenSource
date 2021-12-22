// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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

void mdla_reset(unsigned int core, int res)
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

#ifndef CONFIG_MTK_MDLA_IOMMU_DISABLE
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
	pmu_reg_save(mdlaid, 0);//pmu need refine for multi core

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

int mdla_zero_skip_detect(unsigned int core_id)
{
	u32 debug_if_0, debug_if_2, it_front_c_invalid;

	debug_if_0 =
		mdla_reg_read_with_mdlaid(core_id, MREG_DEBUG_IF_0);
	debug_if_2 =
		mdla_reg_read_with_mdlaid(core_id, MREG_DEBUG_IF_2);
	it_front_c_invalid =
		mdla_reg_read_with_mdlaid(core_id, MREG_IT_FRONT_C_INVALID);

	if (debug_if_0 == 0x6) {
		if ((debug_if_2 == it_front_c_invalid) ||
			(debug_if_2 == (it_front_c_invalid/2))) {
			mdla_timeout_debug("core:%d, %s: match zero skip issue\n",
				core_id,
				__func__);
			mdla_devices[core_id].mdla_zero_skip_count++;
			return -1;
		}
	}
	return 0;
}

#if 0
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
	/* reset pmu */
	pmu_reset(core_id);
	/* set pmu register */
	pmu_event_write_all(core_id, (u16)ce->cmd_batch_en);
	ce->state |= (1 << CE_RUN);
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

static inline u32 mdla_get_swcmd(void *base_kva, u32 offset)
{
	return (*(u32 *)(base_kva + offset));
}

static inline void mdla_set_swcmd(void *base_kva, u32 offset, u32 val)
{
	(*(u32 *)(base_kva + offset)) = val;
}

//cid, which cmd id layer end bit do you want to get
static inline bool mdla_is_layer_end(void *base_kva, u32 cid)
{
	return (mdla_get_swcmd(base_kva + (cid - 1) * MREG_CMD_SIZE,
			       MREG_CMD_GENERAL_CTRL_0)
			       & MSK_MREG_CMD_LAYER_END);
}

//cid, which cmd id wait bit do you want to clear
static inline void mdla_clear_swcmd_wait_bit(void *base_kva, u32 cid)
{
	void *cmd_kva = base_kva + (cid - 1) * MREG_CMD_SIZE;

	mdla_set_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1,
		       mdla_get_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1)
			   & ~MSK_MREG_CMD_SWCMD_WAIT_SWCMDDONE);
}

//cid, which cmd id issue bit do you want to clear
static inline void mdla_clear_swcmd_int_bit(void *base_kva, u32 cid)
{
	void *cmd_kva = base_kva + (cid - 1) * MREG_CMD_SIZE;

	mdla_set_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1,
		       mdla_get_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1)
			   & ~MSK_MREG_CMD_SWCMD_INT_SWCMDDONE);
}

static inline void mdla_set_swcmd_done_int(void *base_kva, u32 cid)
{
	void *cmd_kva = base_kva + (cid - 1) * MREG_CMD_SIZE;

	mdla_set_swcmd(cmd_kva, MREG_CMD_TILE_CNT_INT,
		       mdla_get_swcmd(cmd_kva, MREG_CMD_TILE_CNT_INT)
		       | MSK_MREG_CMD_SWCMD_FINISH_INT_EN);
}

void mdla_del_free_command_batch(struct command_entry *ce)
{
	struct command_batch *cb;
	struct list_head *tmp, *next;

	if (unlikely(ce->batch_list_head == NULL))
		return;
	if (unlikely(ce->cmd_batch_size >= ce->count))
		return;
	list_for_each_safe(tmp, next, ce->batch_list_head) {
		cb = list_entry(tmp, struct command_batch, node);
		list_del(&cb->node);
		kfree(cb);
	}
	kfree(ce->batch_list_head);
}

void mdla_restore_cmd_batch(struct command_entry *ce)
{
	uint32_t i = 0;
	void *cmd_kva = NULL;

	if (ce->cmd_int_backup == NULL || ce->cmd_ctrl_1_backup == NULL)
		return;
	if (unlikely(ce->cmdbuf == NULL))
		return;
	apusys_mem_invalidate(ce->cmdbuf);
	for (i = 1; i <= ce->count; i++) {
		cmd_kva = ce->kva + (i - 1) * MREG_CMD_SIZE;
		mdla_set_swcmd(cmd_kva,
			MREG_CMD_TILE_CNT_INT, ce->cmd_int_backup[i - 1]);
		mdla_set_swcmd(cmd_kva,
			MREG_CMD_GENERAL_CTRL_1, ce->cmd_ctrl_1_backup[i - 1]);
	}
	apusys_mem_flush(ce->cmdbuf);
}

void mdla_split_command_batch(struct command_entry *ce)
{
	size_t i, j;
	u32 cur_batch_len = 0;
	u32 batch_tail_id = 0;
	u32 batch_size = ce->cmd_batch_size;
	u32 cmd_count = ce->count;
	struct command_batch *cb;
	struct list_head *tmp, *next;

	// if the command list is resumed, do nothing
	if (ce->batch_list_head == NULL)
		return;
	if (!list_empty(ce->batch_list_head))
		return;
	if (ce->cmd_batch_size >= ce->count)
		return;
	if (unlikely(ce->cmdbuf == NULL))
		return;
	if (ce->cmd_int_backup == NULL || ce->cmd_ctrl_1_backup == NULL)
		return;
	apusys_mem_invalidate(ce->cmdbuf);
	/* backup cmd buffer value */
	for (i = 1; i <= cmd_count; i++) {
		void *cmd_kva = ce->kva + (i - 1) * MREG_CMD_SIZE;

		ce->cmd_int_backup[i - 1] =
			mdla_get_swcmd(cmd_kva, MREG_CMD_TILE_CNT_INT);
		ce->cmd_ctrl_1_backup[i - 1] =
			mdla_get_swcmd(cmd_kva, MREG_CMD_GENERAL_CTRL_1);
	}
	// TODO: add default policy when batch_size is zero
	for (i = 1; i <= cmd_count; cur_batch_len = 0) {
		for (j = i; j <= cmd_count; ++j) {
			cur_batch_len++;
			if (cur_batch_len >= batch_size &&
			    mdla_is_layer_end(ce->kva, j))
				break;
		}

		// allocate one command batch
		cb = kzalloc(sizeof(*cb), GFP_KERNEL);
		if (!cb)
			return;

		cb->index = i;// first cmd id
		cb->size = cur_batch_len;
		batch_tail_id = i + cur_batch_len - 1;// Last cmd id
		list_add_tail(&cb->node, ce->batch_list_head);
		// encode SWCMD DONE
		mdla_set_swcmd_done_int(ce->kva, batch_tail_id);

		// handle first cmd wait bit and check fuse cmd
		mdla_clear_swcmd_wait_bit(ce->kva, cb->index);
		if (batch_tail_id > cb->index &&
			!mdla_is_layer_end(ce->kva, cb->index)) {
			mdla_clear_swcmd_wait_bit(ce->kva, cb->index + 1);
		}
		// encode the previous layer if that is fused with tail
		mdla_clear_swcmd_int_bit(ce->kva, batch_tail_id);
		if (batch_tail_id > 1 &&
			!mdla_is_layer_end(ce->kva, batch_tail_id - 1)) {
			mdla_set_swcmd_done_int(ce->kva, batch_tail_id - 1);
			mdla_clear_swcmd_int_bit(ce->kva, batch_tail_id - 1);
		}
		i += cur_batch_len;
	}

	// buffer sync here
	if (likely(ce->cmdbuf != NULL))
		apusys_mem_flush(ce->cmdbuf);

	if (likely(mdla_preemption_debug == 0))
		return;
	list_for_each_safe(tmp, next, ce->batch_list_head) {
		cb = list_entry(tmp, struct command_batch, node);
		mdla_cmd_debug("%s: id = %d, size = %d, tid = %d\n",
			__func__, cb->index,
			cb->size, cb->index + cb->size - 1);
	}
}

static inline struct mdla_scheduler *mdla_get_scheduler(unsigned int core_id)
{
	return (core_id >= MTK_MDLA_MAX_NUM) ?
		NULL : mdla_devices[core_id].sched;
}
#if 1
static void mdla_issue_ce_cdma_error(
	uint32_t core_id,
	struct command_entry *ce,
	uint32_t cmda1_golden,
	uint32_t cmda2_golden)
{
	uint32_t cdma1, cdma2;

	cdma1 = mdla_reg_read_with_mdlaid(
			core_id, MREG_TOP_G_CDMA1);

	cdma2 = mdla_reg_read_with_mdlaid(
			core_id, MREG_TOP_G_CDMA2);

	if (cdma1 != cmda1_golden) {
		ce->state |= (1 << CE_ISSUE_ERROR1);
		ce_func_trace(ce, F_ISSUE|1);
	}
	if (cdma2 != cmda2_golden) {
		ce->state |= (1 << CE_ISSUE_ERROR2);
		ce_func_trace(ce, F_ISSUE|2);
	}
}

static void mdla_update_dual_deadline(
	uint16_t priority,
	uint64_t deadline)
{
	unsigned long flags;

	spin_lock_irqsave(&g_smp_deadline[priority].lock, flags);
	g_smp_deadline[priority].deadline = deadline;
	spin_unlock_irqrestore(&g_smp_deadline[priority].lock, flags);
}

/*
 * Enqueue one CE and start scheduler
 * resume = 0: add tail because first start
 * resume = 1: add begin because resume
 * resume = 2: add begin because check dual normal cmd
 */
void mdla_enqueue_ce_2_1(
	unsigned int core_id,
	struct command_entry *ce,
	uint32_t resume)
{
	struct mdla_scheduler *sched = mdla_get_scheduler(core_id);

	/* ce == NULL will occur when mdla_normal_dual_cmd_issue call it */
	if (unlikely(sched == NULL || ce == NULL))
		return;

	if (resume == 0) {
		list_add_tail(&ce->node, &(sched->ce_list[ce->priority]));
		ce->state |= (1 << CE_QUEUE);
	} else if (resume > 0) {
		list_add(&ce->node, &(sched->ce_list[ce->priority]));
		ce->state |= (1 << CE_QUEUE_RESUME);
	}
	ce_func_trace(ce, (F_ENQUEUE|resume));
}

/*
 * Dequeue a prioritized CE from active CE queue, handle the context switch of
 * the original processing_ce, and set the prioritized CE as processing_ce.
 *
 * NOTE: sched->lock should be acquired by caller
 */
struct command_entry *mdla_dequeue_ce_2_1(unsigned int core_id)
{
	struct mdla_scheduler *sched = mdla_get_scheduler(core_id);
	struct command_entry *new_ce = NULL;
	int16_t i = 0;

	if (unlikely(sched == NULL))
		return NULL;

	for (i = PRIORITY_LEVEL - 1; i >= 0; i--) {
		/* get one CE from the active CE queue */
		new_ce = list_first_entry_or_null(
					&(sched->ce_list[i]),
					struct command_entry,
					node);
		if (new_ce != NULL) {
			/* remove prioritized CE from active CE queue */
			list_del(&new_ce->node);
			new_ce->state |= (1 << CE_DEQUE);
			ce_func_trace(new_ce, F_DEQUEUE);
			return new_ce;
		}
	}

	return new_ce;
}

void mdla_preempt_ce_2_1(unsigned int core_id, struct command_entry *high_ce)
{
	struct mdla_scheduler *sched = mdla_get_scheduler(core_id);
	struct command_entry *low_ce;
	uint64_t deadline =
		get_jiffies_64() + msecs_to_jiffies(mdla_timeout);

	if (unlikely(sched == NULL))
		return;
	low_ce = sched->pro_ce;

	if (likely(sched->pro_ce != NULL)) {
		sched->pro_ce->req_end_t = sched_clock();
		sched->pro_ce->state |= (1 << CE_PREEMPTED);
		mdla_preemption_times++;
		sched->enqueue_ce(core_id, low_ce, 1);
		low_ce->deadline_t = deadline;
	}

	if (likely(high_ce != NULL)) {
		high_ce->state |= (1 << CE_PREEMPTING);
		sched->pro_ce = high_ce;
	}
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
 * NOTE: sched->lock should be acquired by caller
 */
unsigned int mdla_process_ce_2_1(unsigned int core_id)
{
	unsigned long flags;
	unsigned int ret = CE_NONE;
	struct command_entry *ce;
	struct command_batch *cb;
	struct mdla_scheduler *sched = mdla_get_scheduler(core_id);
	u32 fin_cid;

	if (unlikely(sched == NULL))
		return ret;

	spin_lock_irqsave(&mdla_devices[core_id].hw_lock, flags);
	fin_cid = mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_FIN3);
	spin_unlock_irqrestore(&mdla_devices[core_id].hw_lock, flags);

	ce = sched->pro_ce;
	if (unlikely(ce == NULL))
		return ret;

	ce->fin_cid = fin_cid;
	if (unlikely(fin_cid < ce->wish_fin_cid)) {
		ce->irq_state |= IRQ_TWICE;
		ce_func_trace(ce, F_CMDDONE_CE_FIN3ERROR);
		return ret;
	}

	/* clear event id after this event is done */
	spin_lock_irqsave(&mdla_devices[core_id].hw_lock, flags);
#ifdef __APUSYS_MDLA_PMU_SUPPORT__
	/* handle PMU */
	pmu_reg_save(core_id, (u16)sched->pro_ce->priority);
#endif
	mdla_reg_write_with_mdlaid(core_id, 1, MREG_TOP_G_CDMA4);
	spin_unlock_irqrestore(&mdla_devices[core_id].hw_lock, flags);

	if (ce->batch_list_head != NULL) {
		if (likely(!list_empty(ce->batch_list_head))) {
			cb = list_first_entry(ce->batch_list_head,
						struct command_batch, node);
			list_del(&cb->node);
			kfree(cb);
		}
	}

	/* all command done for command-based scheduling */
	if (fin_cid >= ce->count) {
		ret = CE_DONE;
		ce->state |= (1 << CE_DONE);
	} else {
		ret = CE_SCHED;
		ce->state |= (1 << CE_SCHED);
	}
	return ret;
}

/*
 * Issue the processing_ce to HW engine
 * NOTE: sched->lock should be acquired by caller
 */
void mdla_issue_ce_2_1(unsigned int core_id)
{
	dma_addr_t addr;
	u32 nr_cmd_to_issue;
	struct mdla_scheduler *sched = mdla_get_scheduler(core_id);
	struct command_entry *ce;
	struct command_batch *cb;
	unsigned long flags;
	u32 irq_status;

	if (unlikely(sched == NULL))
		return;

	ce = sched->pro_ce;
	if (unlikely(ce == NULL))
		return;

	if (ce->poweron_t == 0) {
		ce->poweron_t = sched_clock();
		ce->req_start_t = ce->poweron_t;
	}

	addr = ce->mva + ((dma_addr_t)ce->fin_cid) * MREG_CMD_SIZE;
	nr_cmd_to_issue = ce->count - ce->fin_cid;
	if (ce->batch_list_head != NULL) {
		if (!list_empty(ce->batch_list_head)) {
			cb = list_first_entry(ce->batch_list_head,
						struct command_batch, node);
			nr_cmd_to_issue = cb->size;
		}
	}
	ce->wish_fin_cid = ce->fin_cid + nr_cmd_to_issue;

	spin_lock_irqsave(&mdla_devices[core_id].hw_lock, flags);

	irq_status =
		mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_INTP0);
	if (likely(irq_status&MDLA_IRQ_CDMA_FIFO_EMPTY)) {
		uint64_t deadline =
			get_jiffies_64() + msecs_to_jiffies(mdla_timeout);

#ifdef __APUSYS_MDLA_PMU_SUPPORT__
		/* reset pmu and set register */
		pmu_set_reg(core_id, (u16)ce->priority);
#endif
		ce->state |= (1 << CE_RUN);
		/* update deadline value */
		if (ce->multicore_total == 2) {
			mdla_update_dual_deadline(
				ce->priority,
				deadline);
		} else
			ce->deadline_t = deadline;

		if (likely(ce->context_callback != NULL))
			ce->context_callback(
				APUSYS_DEVICE_MDLA,
				core_id, ce->ctx_id);

		/* set command address */
		mdla_reg_write_with_mdlaid(core_id,
			addr, MREG_TOP_G_CDMA1);

		/* set command number */
		mdla_reg_write_with_mdlaid(core_id,
			nr_cmd_to_issue, MREG_TOP_G_CDMA2);

		/* trigger engine */
		mdla_reg_write_with_mdlaid(
			core_id, ce->csn, MREG_TOP_G_CDMA3);

		if (unlikely(mdla_timeout_dbg)) {
			mdla_issue_ce_cdma_error(
				core_id,
				ce, addr,
				nr_cmd_to_issue);
		} else
			ce_func_trace(ce, F_ISSUE);
	} else {
		if ((ce->irq_state & IRQ_N_EMPTY_IN_SCHED) == 0)
			ce->irq_state |= IRQ_NE_ISSUE_FIRST;
		ce->irq_state |= IRQ_N_EMPTY_IN_ISSUE;
		ce->state |= (1 << CE_SKIP);
		if (in_interrupt()) {
			ce->irq_state |= IRQ_IN_IRQ;
			ce_func_trace(ce, F_ISSUE|3);
		} else {
			ce->irq_state |= IRQ_NOT_IN_IRQ;
			ce_func_trace(ce, F_ISSUE|4);
		}
	}
	spin_unlock_irqrestore(&mdla_devices[core_id].hw_lock, flags);
}

/*
 * Set the status of completed CE as CE_FIN
 * NOTE: sched->lock should be acquired by caller
 */
void mdla_complete_ce_2_1(unsigned int core_id)
{
	struct mdla_scheduler *sched = mdla_get_scheduler(core_id);

	if (unlikely(sched == NULL))
		return;

	sched->pro_ce->req_end_t = sched_clock();
	sched->pro_ce->state |= (1 << CE_FIN);

	ce_func_trace(sched->pro_ce, F_COMPLETE);

	complete(&sched->pro_ce->swcmd_done_wait);
	sched->pro_ce = NULL;
}

void mdla_normal_dual_cmd_issue(
	uint32_t core_id,
	uint64_t dual_cmd_id)
{
	struct mdla_scheduler *sched[MTK_MDLA_MAX_NUM];
	int16_t i = 0;
	unsigned long flags[MTK_MDLA_MAX_NUM];
	struct command_entry *ce[MTK_MDLA_MAX_NUM];

	/* Initial local variables */
	for (i = 0; i < MTK_MDLA_MAX_NUM; i++) {
		sched[i] = mdla_get_scheduler(i);
		if (sched[i] == NULL)
			return;
	}
	/* Get all cores spin lock and check hw free */
	for (i = 0; i < MTK_MDLA_MAX_NUM; i++) {
		spin_lock_irqsave(&sched[i]->lock, flags[i]);
		if (sched[i]->pro_ce != NULL) {
			/* core i is not free */
			for (; i >= 0; i--)
				spin_unlock_irqrestore(
					&sched[i]->lock,
					flags[i]);
			return;
		}
	}
	/* check list status */
	for (i = 0; i < MTK_MDLA_MAX_NUM; i++) {
		ce[i] = sched[i]->dequeue_ce(i);
		if (ce[i] == NULL) {
			for (; i >= 0; i--)
				sched[i]->enqueue_ce(i, ce[i], 2);
			goto unlock;
		} else if (ce[i]->cmd_id != dual_cmd_id) {
			for (; i >= 0; i--)
				sched[i]->enqueue_ce(i, ce[i], 2);
			goto unlock;
		} else if (ce[i]->priority != MDLA_LOW_PRIORITY) {
			for (; i >= 0; i--)
				sched[i]->enqueue_ce(i, ce[i], 2);
			goto unlock;
		}
	}
	/* issue cmd */
	for (i = 0; i < MTK_MDLA_MAX_NUM; i++) {
		sched[i]->pro_ce = ce[i];
		sched[i]->issue_ce(i);
	}
unlock:
	/* free all cores spin lock */
	for (i = MTK_MDLA_MAX_NUM - 1; i >= 0; i--)
		spin_unlock_irqrestore(&sched[i]->lock, flags[i]);
}

/*
 * Scheduler to process the current CE, and pick the next one to issue
 * NOTE: mdla_scheduler would be invoked in ISR
 */
irqreturn_t mdla_scheduler_2_1(unsigned int core_id)
{
	struct mdla_scheduler *sched = mdla_get_scheduler(core_id);
	unsigned long flags;
	unsigned int status;
	struct mdla_dev *mdla_info = &mdla_devices[core_id];
	u32 irq_status = 0;
	u32 cdma4 = 0;
	struct command_entry *new_ce;

	/* clear intp0 to avoid irq fire twice */
	spin_lock_irqsave(&mdla_info->hw_lock, flags);
	irq_status =
		mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_INTP0);
	mdla_reg_write_with_mdlaid(
		core_id,
		MDLA_IRQ_SWCMD_DONE | MDLA_IRQ_PMU_INTE,
		MREG_TOP_G_INTP0);
	cdma4 = mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_CDMA4);
	spin_unlock_irqrestore(&mdla_info->hw_lock, flags);

	/* interrupt error check start */
	if (unlikely(sched == NULL)) {
		mdla_info->error_bit |= IRQ_NO_SCHEDULER;
		goto end;
	}

	spin_lock_irqsave(&sched->lock, flags);
	if (unlikely(sched->pro_ce == NULL)) {
		mdla_info->error_bit |= IRQ_NO_PROCESSING_CE;
		goto unlock;
	}

	if (unlikely(sched->pro_ce->state & (1 << CE_FAIL))) {
		mdla_info->error_bit |= IRQ_TIMEOUT;
		ce_func_trace(sched->pro_ce, F_TIMEOUT|1);
		goto unlock;
	}

	if (unlikely(time_after64(
		get_jiffies_64(),
		sched->pro_ce->deadline_t)
		)) {
		sched->pro_ce->state |= (1 << CE_TIMEOUT);
		ce_func_trace(sched->pro_ce, F_TIMEOUT);
		goto unlock;
	}
	if (unlikely((irq_status&MDLA_IRQ_SWCMD_DONE) == 0)) {
		ce_func_trace(sched->pro_ce, F_INIRQ_ERROR);
		goto unlock;
	}

	if (unlikely(cdma4 != (sched->pro_ce->csn))) {
		sched->pro_ce->irq_state |= IRQ_RECORD_ERROR;
		ce_func_trace(sched->pro_ce, F_INIRQ_CDMA4ERROR);
		goto unlock;
	}
	/* interrupt error check end */

	sched->pro_ce->state |= (1 << CE_SCHED);

	/* process the current CE */
	status = sched->process_ce(core_id);

	if (status == CE_DONE) {
		sched->complete_ce(core_id);
	} else if (status == CE_NONE) {
		/* nothing to do but wait for the engine completed */
		goto unlock;
	}
	/* get the next CE to be processed */
	new_ce = sched->dequeue_ce(core_id);

	if (new_ce != NULL) {
		uint32_t dual_cmd = new_ce->multicore_total;
		uint16_t priority = new_ce->priority;

		if (sched->pro_ce != NULL) {
			sched->preempt_ce(core_id, new_ce);
			sched->issue_ce(core_id);
		} else if (dual_cmd == 2 && priority == MDLA_LOW_PRIORITY) {
			sched->enqueue_ce(core_id, new_ce, 1);
			spin_unlock_irqrestore(&sched->lock, flags);
			sched->issue_dual_lowce(core_id, new_ce->cmd_id);
			goto end;
		} else {
			sched->pro_ce = new_ce;
			sched->issue_ce(core_id);
		}
	} else {
		if (sched->pro_ce != NULL) {
			/* Issue next cmd batch */
			sched->issue_ce(core_id);
		}
	}

unlock:
	spin_unlock_irqrestore(&sched->lock, flags);
end:
	return IRQ_HANDLED;
}
#else
/*
 * Enqueue one CE and start scheduler
 *
 *
 */
void mdla_enqueue_ce(unsigned int core_id, struct command_entry *ce)
{
	struct mdla_scheduler *sched = mdla_get_scheduler(core_id);
	unsigned long flags;

	if (!sched || !ce)
		return;

	spin_lock_irqsave(&sched->lock, flags);
	list_add_tail(&ce->node, &sched->active_ce_queue);
	ce->state |= (1 << CE_QUEUE);
	/* there are CEs under processing */
	if (sched->pro_ce != NULL) {
		spin_unlock_irqrestore(&sched->lock, flags);
		return;
	}

	/* there is no CE under processing: dequeue and trigger engine */
	sched->dequeue_ce(core_id);
	ce->deadline_t = get_jiffies_64() + msecs_to_jiffies(mdla_timeout);
	sched->issue_ce(core_id);
	spin_unlock_irqrestore(&sched->lock, flags);
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
 * NOTE: sched->lock should be acquired by caller
 */
unsigned int mdla_process_ce(unsigned int core_id)
{
	unsigned long flags;
	unsigned int ret = CE_NONE;
	struct command_entry *ce;
	struct command_batch *cb;
	struct mdla_scheduler *sched = mdla_get_scheduler(core_id);
	u32 cmda4;
	u32 fin_cid, irq_status;

	if (!sched)
		return ret;

	spin_lock_irqsave(&mdla_devices[core_id].hw_lock, flags);

	irq_status = mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_INTP0);

	cmda4 = mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_CDMA4);

	fin_cid = mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_FIN3);

#ifdef __APUSYS_MDLA_PMU_SUPPORT__
	/* handle PMU */
	pmu_reg_save(core_id, (u16)sched->pro_ce->cmd_batch_en);
#endif

	if (likely(irq_status & MDLA_IRQ_PMU_INTE)) {
		mdla_reg_write_with_mdlaid(core_id, MDLA_IRQ_PMU_INTE,
					   MREG_TOP_G_INTP0);
	}
	spin_unlock_irqrestore(&mdla_devices[core_id].hw_lock, flags);

	ce = sched->pro_ce;
	if (!ce)
		return ret;

	if (cmda4 != (ce->cmd_batch_en + 1)) {
		ce->irq_state |= IRQ_RECORD_ERROR;
		return ret;
	}

	if (fin_cid < ce->wish_fin_cid) {
		ce->irq_state |= IRQ_TWICE;
		ce->fin_cid = fin_cid;
		return ret;
	}
	/* read after write make sure it will write back to register now */
	spin_lock_irqsave(&mdla_devices[core_id].hw_lock, flags);
	mdla_reg_write_with_mdlaid(core_id, 1, MREG_TOP_G_CDMA4);
	mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_CDMA4);
	spin_unlock_irqrestore(&mdla_devices[core_id].hw_lock, flags);

	ce->fin_cid = fin_cid;
	if (ce->batch_list_head != NULL) {
		if (likely(!list_empty(ce->batch_list_head))) {
			cb = list_first_entry(ce->batch_list_head,
						struct command_batch, node);
			list_del(&cb->node);
			kfree(cb);
		}
	}

	/* all command done for command-based scheduling */
	if (fin_cid >= ce->count) {
		ret = CE_DONE;
		ce->state |= (1 << CE_DONE);
	} else {
		ret = CE_SCHED;
		ce->state |= (1 << CE_SCHED);
	}
	return ret;
}

/*
 * Issue the processing_ce to HW engine
 * NOTE: sched->lock should be acquired by caller
 */
void mdla_issue_ce(unsigned int core_id)
{
	dma_addr_t addr;
	u32 nr_cmd_to_issue;
	struct mdla_scheduler *sched = mdla_get_scheduler(core_id);
	struct command_entry *ce;
	struct command_batch *cb;
	unsigned long flags;
	u32 irq_status;

	if (!sched)
		return;

	ce = sched->pro_ce;
	if (!ce)
		return;

	if (ce->poweron_t == 0) {
		ce->poweron_t = sched_clock();
		ce->req_start_t = ce->poweron_t;
	}

	addr = ce->mva + ((dma_addr_t)ce->fin_cid) * MREG_CMD_SIZE;
	nr_cmd_to_issue = ce->count - ce->fin_cid;
	if (ce->batch_list_head != NULL) {
		if (!list_empty(ce->batch_list_head)) {
			cb = list_first_entry(ce->batch_list_head,
						struct command_batch, node);
			nr_cmd_to_issue = cb->size;
		}
	}
	ce->wish_fin_cid = ce->fin_cid + nr_cmd_to_issue;

	spin_lock_irqsave(&mdla_devices[core_id].hw_lock, flags);

	irq_status =
		mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_INTP0);
	if (likely(irq_status&MDLA_IRQ_CDMA_FIFO_EMPTY)) {
		u32 cdma1 = 0;
		u32 cdma2 = 0;

#ifdef __APUSYS_MDLA_PMU_SUPPORT__
		/* reset pmu and set register */
		pmu_set_reg(core_id, (u16)ce->cmd_batch_en);
#endif
		ce->state |= (1 << CE_RUN);

		if (likely(ce->context_callback != NULL))
			ce->context_callback(
				APUSYS_DEVICE_MDLA,
				core_id, ce->ctx_id);

		/* set command address */
		mdla_reg_write_with_mdlaid(core_id,
			addr, MREG_TOP_G_CDMA1);
		if (unlikely(mdla_timeout_dbg)) {
			cdma1 =
				mdla_reg_read_with_mdlaid(
					core_id, MREG_TOP_G_CDMA1);
			if (cdma1 != addr) {
				ce->state |= (1 << CE_ISSUE_ERROR1);
				//pr_info("cdma1 = %x\n", cdma1);
			}
		}

		/* set command number */
		mdla_reg_write_with_mdlaid(core_id,
			nr_cmd_to_issue, MREG_TOP_G_CDMA2);
		if (unlikely(mdla_timeout_dbg)) {
			cdma2 =
				mdla_reg_read_with_mdlaid(
					core_id, MREG_TOP_G_CDMA2);
			if (cdma2 != nr_cmd_to_issue) {
				ce->state |= (1 << CE_ISSUE_ERROR2);
				//pr_info("cdma2 = %x\n", cdma2);
			}
		}

		/* trigger engine */
		mdla_reg_write_with_mdlaid(
			core_id,
			(ce->cmd_batch_en + 1),
			MREG_TOP_G_CDMA3);

	} else {
		if ((ce->irq_state & IRQ_N_EMPTY_IN_SCHED) == 0)
			ce->irq_state |= IRQ_NE_ISSUE_FIRST;
		ce->irq_state |= IRQ_N_EMPTY_IN_ISSUE;
		ce->state |= (1 << CE_SKIP);
		if (in_interrupt())
			ce->irq_state |= IRQ_IN_IRQ;
		else
			ce->irq_state |= IRQ_NOT_IN_IRQ;
	}
	spin_unlock_irqrestore(&mdla_devices[core_id].hw_lock, flags);
}

/*
 * Set the status of completed CE as CE_FIN
 * NOTE: sched->lock should be acquired by caller
 */
void mdla_complete_ce(unsigned int core_id)
{
	struct mdla_scheduler *sched = mdla_get_scheduler(core_id);

	if (!sched)
		return;

	sched->pro_ce->req_end_t = sched_clock();
	sched->pro_ce->state |= (1 << CE_FIN);

	complete(&sched->pro_ce->swcmd_done_wait);
	sched->pro_ce = NULL;
}

/*
 * Dequeue a prioritized CE from active CE queue, handle the context switch of
 * the original processing_ce, and set the prioritized CE as processing_ce.
 *
 * NOTE: sched->lock should be acquired by caller
 */
unsigned int mdla_dequeue_ce(unsigned int core_id)
{
	struct mdla_scheduler *sched = mdla_get_scheduler(core_id);
	struct command_entry *prioritized_ce = NULL;
	unsigned int ret = REASON_QUEUE_NORMALEXE;

	if (!sched)
		return REASON_QUEUE_NULLSCHEDULER;

	/* get one CE from the active CE queue */
	prioritized_ce =
		list_first_entry_or_null(
			&sched->active_ce_queue,
			struct command_entry,
			node);

	/* return if we don't need to update the processing_ce */
	if (prioritized_ce == NULL)
		return REASON_QUEUE_NOCHANGE;

	/* remove prioritized CE from active CE queue */
	list_del(&prioritized_ce->node);
	prioritized_ce->state |= (1 << CE_DEQUE);
	if (sched->pro_ce != NULL) {
		sched->pro_ce->req_end_t = sched_clock();
		sched->pro_ce->state |= (1 << CE_PREEMPTED);
		mdla_preemption_times++;
		list_add_tail(
			&sched->pro_ce->node,
			&sched->active_ce_queue);
		ret = REASON_QUEUE_PREEMPTION;
		prioritized_ce->state |= (1 << CE_PREEMPTING);
	}
	prioritized_ce->deadline_t =
		get_jiffies_64() + msecs_to_jiffies(mdla_timeout);
	sched->pro_ce = prioritized_ce;
	return ret;
}

/*
 * Scheduler to process the current CE, and pick the next one to issue
 * NOTE: mdla_scheduler would be invoked in ISR
 */
irqreturn_t mdla_scheduler(unsigned int core_id)
{
	struct mdla_scheduler *sched = mdla_get_scheduler(core_id);
	unsigned long flags;
	unsigned int status;
	struct mdla_dev *mdla_info = &mdla_devices[core_id];
	u32 irq_status = 0;

	/* clear intp0 to avoid irq fire twice */
	spin_lock_irqsave(&mdla_info->hw_lock, flags);
	irq_status =
		mdla_reg_read_with_mdlaid(core_id, MREG_TOP_G_INTP0);
	mdla_reg_write_with_mdlaid(
		core_id,
		MDLA_IRQ_SWCMD_DONE,
		MREG_TOP_G_INTP0);
	spin_unlock_irqrestore(&mdla_info->hw_lock, flags);

	if (unlikely(sched == NULL)) {
		mdla_info->error_bit |= IRQ_NO_SCHEDULER;
		goto end;
	}

	spin_lock_irqsave(&sched->lock, flags);

	if (unlikely(sched->pro_ce == NULL)) {
		mdla_info->error_bit |= IRQ_NO_PROCESSING_CE;
		goto unlock;
	}

	if (unlikely(sched->pro_ce->state & (1 << CE_FAIL))) {
		mdla_info->error_bit |= IRQ_TIMEOUT;
		goto unlock;
	}

	if (unlikely(time_after64(
		get_jiffies_64(),
		sched->pro_ce->deadline_t)
		)) {
		sched->pro_ce->state |= (1 << CE_TIMEOUT);
		goto unlock;
	}

	sched->pro_ce->state |= (1 << CE_SCHED);

	/* process the current CE */
	status = sched->process_ce(core_id);

	if (status == CE_DONE) {
		sched->complete_ce(core_id);
	} else if (status == CE_NONE) {
		/* nothing to do but wait for the engine completed */
		goto unlock;
	}

	if (sched->pro_ce != NULL) {
		if ((irq_status & MDLA_IRQ_CDMA_FIFO_EMPTY) == 0) {
			if ((sched->pro_ce->irq_state & IRQ_N_EMPTY_IN_ISSUE))
				sched->pro_ce->irq_state |= IRQ_NE_SCHED_FIRST;
			sched->pro_ce->irq_state |= IRQ_N_EMPTY_IN_SCHED;
		}
	}

	/* get the next CE to be processed */
	status = sched->dequeue_ce(core_id);

	if (likely(sched->pro_ce != NULL))
		sched->issue_ce(core_id);

unlock:
	spin_unlock_irqrestore(&sched->lock, flags);
end:
	return IRQ_HANDLED;
}
#endif

void dump_timeout_debug_info(int core_id)
{
	int i;

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

