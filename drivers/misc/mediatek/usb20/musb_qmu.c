// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/usb/composite.h>

#include "musb_core.h"
#include "musb_host.h"
#include "musbhsdma.h"
#include "mtk_musb.h"
#include "musb_qmu.h"

static unsigned int host_qmu_tx_max_active_isoc_gpd[MAX_QMU_EP + 1];
static unsigned int host_qmu_tx_max_number_of_pkts[MAX_QMU_EP + 1];
static unsigned int host_qmu_rx_max_active_isoc_gpd[MAX_QMU_EP + 1];
static unsigned int host_qmu_rx_max_number_of_pkts[MAX_QMU_EP + 1];
static u8 mtk_host_active_dev_table[128];
#ifdef CONFIG_MTK_UAC_POWER_SAVING
static struct workqueue_struct *low_power_timer_test_wq;
static struct work_struct low_power_timer_test_work;
static int low_power_timer_activate;
static unsigned long low_power_timer_total_sleep;
static unsigned long low_power_timer_total_wake;
static int low_power_timer_request_time;
static unsigned int low_power_timer_trigger_cnt;
static unsigned int low_power_timer_wake_cnt;
static ktime_t ktime_wake, ktime_sleep, ktime_begin, ktime_end;
static int low_power_timer_irq_ctrl;
#define LOW_POWER_TIMER_MIN_PERIOD 10 /* ms */
#define LOW_POWER_TIMER_MAX_PERIOD 50 /* ms */
static struct delayed_work low_power_timer_montior_work;

enum {
	IDLE_STAGE,
	RUNNING_STAGE
};
#define MONITOR_FREQ 6000 /* ms */
static void do_low_power_timer_monitor_work(struct work_struct *work)
{
	static int state = IDLE_STAGE;
	static unsigned int last_trigger_cnt;

	DBG(1, "state:%s, last:%d, balanced<%d,%d>\n",
			state?"RUNNING_STAGE":"IDLE_STAGE",
			last_trigger_cnt,
			low_power_timer_total_trigger_cnt,
			low_power_timer_total_wake_cnt);

	if (state == IDLE_STAGE) {
		if (last_trigger_cnt != low_power_timer_total_trigger_cnt)
			state = RUNNING_STAGE;
	} else if (state == RUNNING_STAGE) {
		if (last_trigger_cnt == low_power_timer_total_trigger_cnt
				&& low_power_timer_total_trigger_cnt ==
				low_power_timer_total_wake_cnt)
			state = IDLE_STAGE;
		else if (last_trigger_cnt ==
				low_power_timer_total_trigger_cnt
				&& low_power_timer_total_trigger_cnt !=
				low_power_timer_total_wake_cnt)
			musb_bug();
	}

	last_trigger_cnt = low_power_timer_total_trigger_cnt;
	schedule_delayed_work(&low_power_timer_montior_work,
		msecs_to_jiffies(MONITOR_FREQ));
}

static void low_power_timer_wakeup_func(unsigned long data);
static DEFINE_TIMER(low_power_timer, low_power_timer_wakeup_func, 0, 0);

void low_power_timer_resource_reset(void)
{
	low_power_timer_total_sleep = low_power_timer_total_wake = 0;
	low_power_timer_trigger_cnt = low_power_timer_wake_cnt = 0;
}

static void low_power_timer_wakeup_func(unsigned long data)
{
#ifdef TIMER_LOCK
	unsigned long flags;

	spin_lock_irqsave(&mtk_musb->lock, flags);
#endif
	if (unlikely(low_power_timer_irq_ctrl))
		enable_irq(mtk_musb->nIrq);

	low_power_timer_wake_cnt++;
	low_power_timer_total_wake_cnt++;
	ktime_wake = ktime_get();
	low_power_timer_total_sleep +=
		ktime_to_us(ktime_sub(ktime_wake, ktime_sleep));

	/* deep idle forbidd here */
	if (low_power_timer_mode == 1)
		usb_hal_dpidle_request(USB_DPIDLE_FORBIDDEN);
	/* make sure all global status sync to memory */
	mb();

	if (low_power_timer_trigger_cnt >= 2) {
		static DEFINE_RATELIMIT_STATE(ratelimit, HZ, 1);

		if (__ratelimit(&ratelimit))
			pr_debug("<ratelimit> avg sleep(%lu/%d, %lu)us,
			avg wake(%lu/%d, %lu)us, mode<%d,%d>,
			local balanced<%d,%d>\n"
				, low_power_timer_total_sleep,
				low_power_timer_trigger_cnt,
				low_power_timer_total_sleep/
				low_power_timer_trigger_cnt,
				low_power_timer_total_wake,
				(low_power_timer_trigger_cnt - 1),
				low_power_timer_total_wake/
				(low_power_timer_trigger_cnt - 1),
				low_power_timer_mode,
				low_power_timer_mode2_option,
				low_power_timer_trigger_cnt,
				low_power_timer_wake_cnt);
		}

	low_power_timer_activate = 0;
	DBG(1, "sleep forbidden <%d,%d>\n",
			low_power_timer_total_trigger_cnt,
			low_power_timer_total_wake_cnt);
#ifdef TIMER_FUNC_LOCK
	spin_unlock_irqrestore(&mtk_musb->lock, flags);
#endif

}

void try_trigger_low_power_timer(signed int sleep_ms)
{

	DBG(1, "sleep_ms:%d\n", sleep_ms);
	if (sleep_ms <= LOW_POWER_TIMER_MIN_PERIOD ||
		sleep_ms >= LOW_POWER_TIMER_MAX_PERIOD) {
		low_power_timer_activate = 0;
		return;
	}

	/* dynamic allocation for timer , result in memory leak */
#ifdef TIMER_DYNAMIC
	{
		struct timer_list *timer;

		timer = kzalloc(sizeof(struct timer_list), GFP_ATOMIC);
		if (!timer) {
			low_power_timer_activate = 0;
			DBG(0, "alloc timer fail\n");
			return;
		}

		init_timer(timer);
		timer->function = low_power_timer_wakeup_func;
		timer->data = (unsigned long)timer;
		timer->expires = jiffies + msecs_to_jiffies(sleep_ms);
		add_timer(timer);
	}
#else
	mod_timer(&low_power_timer, jiffies + msecs_to_jiffies(sleep_ms));
#endif

	DBG(1, "sleep allowed <%d,%d>, %d ms\n",
			low_power_timer_total_trigger_cnt,
			low_power_timer_total_wake_cnt, sleep_ms);

	ktime_sleep = ktime_get();
	low_power_timer_trigger_cnt++;
	low_power_timer_total_trigger_cnt++;

	if (unlikely(low_power_timer_irq_ctrl))
		disable_irq_nosync(mtk_musb->nIrq);

	if (low_power_timer_trigger_cnt >= 2)
		low_power_timer_total_wake +=
			ktime_to_us(ktime_sub(ktime_sleep, ktime_wake));

	/* deep idle allow here */
	if (low_power_timer_mode == 1)
		usb_hal_dpidle_request(USB_DPIDLE_SRAM);
}

void do_low_power_timer_test_work(struct work_struct *work)
{
	unsigned long flags;
	signed int set_time;
	unsigned long diff_time_ns;
	int gpd_free_count_last, gpd_free_count;
	int done = 0;

	spin_lock_irqsave(&mtk_musb->lock, flags);
	gpd_free_count = qmu_free_gpd_count(0, ISOC_EP_START_IDX);
	gpd_free_count_last = gpd_free_count;
	spin_unlock_irqrestore(&mtk_musb->lock, flags);

	while (!done) {

		udelay(300);

		spin_lock_irqsave(&mtk_musb->lock, flags);
		gpd_free_count = qmu_free_gpd_count(0, ISOC_EP_START_IDX);

		if (gpd_free_count == gpd_free_count_last) {
			ktime_end = ktime_get();
			diff_time_ns =
				ktime_to_us(ktime_sub(ktime_end, ktime_begin));
			set_time = (low_power_timer_request_time -
				(diff_time_ns/1000));
			try_trigger_low_power_timer(set_time);
			done = 1;
		}

		gpd_free_count_last = gpd_free_count;
		spin_unlock_irqrestore(&mtk_musb->lock, flags);
	}
}

void lower_power_timer_test_init(void)
{
	INIT_WORK(&low_power_timer_test_work, do_low_power_timer_test_work);
	low_power_timer_test_wq =
		create_singlethread_workqueue("usb20_low_power_timer_test_wq");

	/* TIMER_DEFERRABLE for not interfering with deep idle */
	INIT_DEFERRABLE_WORK(&low_power_timer_montior_work,
		do_low_power_timer_monitor_work);
	schedule_delayed_work(&low_power_timer_montior_work, 0);
}

/*  mode 0 : no timer mechanism
 *  mode 1 : real case for idle task, no-usb-irq control
 *  mode 2 + option 0: simulate SCREEN ON  mode 1 case
 *  mode 2 + option 1: simulate SCREEN OFF mode 1 perfect case
 *  mode 2 + option 2: simulate SCREEN OFF mode 1 real case
 */

void low_power_timer_sleep(unsigned int sleep_ms)
{
	DBG(1, "sleep(%d) ms\n", sleep_ms);

	low_power_timer_activate = 1;
	if (low_power_timer_mode == 2 && low_power_timer_mode2_option != 0)
		low_power_timer_irq_ctrl = 1;
	else
		low_power_timer_irq_ctrl = 0;

	if ((low_power_timer_mode == 2) &&
		(low_power_timer_mode2_option == 2)) {
		low_power_timer_request_time = sleep_ms;
		ktime_begin = ktime_get();
		queue_work(low_power_timer_test_wq, &low_power_timer_test_work);
	} else
		try_trigger_low_power_timer(sleep_ms);

}
#endif

void mtk_host_active_dev_resource_reset(void)
{
	memset(mtk_host_active_dev_table, 0, sizeof(mtk_host_active_dev_table));
	mtk_host_active_dev_cnt = 0;
}

void musb_host_active_dev_add(int addr)
{
	DBG(1, "devnum:%d\n", addr);
	if (!mtk_host_active_dev_table[addr]) {
		mtk_host_active_dev_table[addr] = 1;
		mtk_host_active_dev_cnt++;
	}
}


void __iomem *qmu_base;
/* debug variable to check qmu_base issue */
void __iomem *qmu_base_2;

int musb_qmu_init(struct musb *musb)
{
	/* set DMA channel 0 burst mode to boost QMU speed */
	musb_writel(musb->mregs, 0x204, musb_readl(musb->mregs, 0x204) | 0x600);
#ifdef CONFIG_MTK_MUSB_DRV_36BIT
	/* eanble DMA channel 0 36-BIT support */
	musb_writel(musb->mregs, 0x204,
		musb_readl(musb->mregs, 0x204) | 0x4000);
#endif

	/* make IOC field in GPD workable */
	musb_writel((musb->mregs + MUSB_QISAR), 0x30, 0);

	qmu_base = (void __iomem *)(mtk_musb->mregs + MUSB_QMUBASE);
	/* debug variable to check qmu_base issue */
	qmu_base_2 = (void __iomem *)(mtk_musb->mregs + MUSB_QMUBASE);

	/* finish all hw op before init qmu */
	mb();

	if (qmu_init_gpd_pool(musb->controller)) {
		QMU_ERR("[QMU]qmu_init_gpd_pool fail\n");
		return -1;
	}
#ifdef CONFIG_MTK_UAC_POWER_SAVING
	lower_power_timer_test_init();
#endif

	return 0;
}

void musb_qmu_exit(struct musb *musb)
{
	qmu_destroy_gpd_pool(musb->controller);
}

void musb_disable_q_all(struct musb *musb)
{
	u32 ep_num;

	QMU_WARN("disable_q_all\n");
	mtk_host_active_dev_resource_reset();
#ifdef CONFIG_MTK_UAC_POWER_SAVING
	low_power_timer_resource_reset();
#endif

	for (ep_num = 1; ep_num <= RXQ_NUM; ep_num++) {
		if (mtk_is_qmu_enabled(ep_num, RXQ))
			mtk_disable_q(musb, ep_num, 1);

		/* reset statistics */
		host_qmu_rx_max_active_isoc_gpd[ep_num] = 0;
		host_qmu_rx_max_number_of_pkts[ep_num] = 0;
	}
	for (ep_num = 1; ep_num <= TXQ_NUM; ep_num++) {
		if (mtk_is_qmu_enabled(ep_num, TXQ))
			mtk_disable_q(musb, ep_num, 0);

		/* reset statistics */
		host_qmu_tx_max_active_isoc_gpd[ep_num] = 0;
		host_qmu_tx_max_number_of_pkts[ep_num] = 0;
	}
}

void musb_kick_D_CmdQ(struct musb *musb, struct musb_request *request)
{
	int isRx;

	isRx = request->tx ? 0 : 1;

	/* enable qmu at musb_gadget_eanble */
#ifdef NEVER
	if (!mtk_is_qmu_enabled(request->epnum, isRx)) {
		/* enable qmu */
		mtk_qmu_enable(musb, request->epnum, isRx);
	}
#endif

	/* note tx needed additional zlp field */
	mtk_qmu_insert_task(request->epnum,
			    isRx,
			    request->request.dma,
			    request->request.length,
			    ((request->request.zero == 1) ? 1 : 0), 1);

	mtk_qmu_resume(request->epnum, isRx);
}

irqreturn_t musb_q_irq(struct musb *musb)
{

	irqreturn_t retval = IRQ_NONE;
	u32 wQmuVal = musb->int_queue;
	int i;

	QMU_INFO("wQmuVal:%d\n", wQmuVal);
	for (i = 1; i <= MAX_QMU_EP; i++) {
		if (wQmuVal & DQMU_M_RX_DONE(i)) {
			if (!musb->is_host)
				qmu_done_rx(musb, i);
			else
				h_qmu_done_rx(musb, i);
		}
		if (wQmuVal & DQMU_M_TX_DONE(i)) {
			if (!musb->is_host)
				qmu_done_tx(musb, i);
			else
				h_qmu_done_tx(musb, i);
		}
	}

	mtk_qmu_irq_err(musb, wQmuVal);

	return retval;
}

void musb_flush_qmu(u32 ep_num, u8 isRx)
{
	QMU_DBG("flush %s(%d)\n", isRx ? "RQ" : "TQ", ep_num);
	mtk_qmu_stop(ep_num, isRx);
	qmu_reset_gpd_pool(ep_num, isRx);
}

void musb_restart_qmu(struct musb *musb, u32 ep_num, u8 isRx)
{
	QMU_DBG("restart %s(%d)\n", isRx ? "RQ" : "TQ", ep_num);
	flush_ep_csr(musb, ep_num, isRx);
	mtk_qmu_enable(musb, ep_num, isRx);
}

bool musb_is_qmu_stop(u32 ep_num, u8 isRx)
{
	void __iomem *base = qmu_base;

	/* debug variable to check qmu_base issue */
	if (qmu_base != qmu_base_2)
		QMU_WARN("qmu_base != qmu_base_2, qmu_base = %p, qmu_base_2=%p"
				, qmu_base, qmu_base_2);

	if (!isRx) {
		if (MGC_ReadQMU16(base, MGC_O_QMU_TQCSR(ep_num))
				& DQMU_QUE_ACTIVE)
			return false;
		else
			return true;
	} else {
		if (MGC_ReadQMU16(base, MGC_O_QMU_RQCSR(ep_num))
				& DQMU_QUE_ACTIVE)
			return false;
		else
			return true;
	}
}

#ifndef CONFIG_MTK_MUSB_QMU_PURE_ZLP_SUPPORT
void musb_tx_zlp_qmu(struct musb *musb, u32 ep_num)
{
	/* sent ZLP through PIO */
	void __iomem *epio = musb->endpoints[ep_num].regs;
	void __iomem *mbase = musb->mregs;
	int cnt = 50; /* 50*200us, total 10 ms */
	int is_timeout = 1;
	u16 csr;

	QMU_WARN("TX ZLP direct sent\n");
	musb_ep_select(mbase, ep_num);

	/* disable dma for pio */
	csr = musb_readw(epio, MUSB_TXCSR);
	csr &= ~MUSB_TXCSR_DMAENAB;
	musb_writew(epio, MUSB_TXCSR, csr);

	/* TXPKTRDY */
	csr = musb_readw(epio, MUSB_TXCSR);
	csr |= MUSB_TXCSR_TXPKTRDY;
	musb_writew(epio, MUSB_TXCSR, csr);

	/* wait ZLP sent */
	while (cnt--) {
		csr = musb_readw(epio, MUSB_TXCSR);
		if (!(csr & MUSB_TXCSR_TXPKTRDY)) {
			is_timeout = 0;
			break;
		}
		udelay(200);
	}

	/* re-enable dma for qmu */
	csr = musb_readw(epio, MUSB_TXCSR);
	csr |= MUSB_TXCSR_DMAENAB;
	musb_writew(epio, MUSB_TXCSR, csr);

	if (is_timeout)
		QMU_ERR("TX ZLP sent fail???\n");
	QMU_WARN("TX ZLP sent done\n");
}
#endif

int mtk_kick_CmdQ(struct musb *musb,
	int isRx, struct musb_qh *qh, struct urb *urb)
{
	void __iomem        *mbase = musb->mregs;
	u16 intr_e = 0;
	struct musb_hw_ep	*hw_ep = qh->hw_ep;
	void __iomem		*epio = hw_ep->regs;
	unsigned int offset = 0;
	u8 bIsIoc;
	dma_addr_t pBuffer;
	u32 dwLength;
	u16 i;
	u32 gpd_free_count = 0;

	if (!urb) {
		QMU_WARN("!urb\n");
		return -1; /*KOBE : should we return a value */
	}

	if (!mtk_is_qmu_enabled(hw_ep->epnum, isRx)) {
		DBG(1, "! mtk_is_qmu_enabled<%d,%s>\n",
			hw_ep->epnum, isRx?"RXQ":"TXQ");

		musb_ep_select(mbase, hw_ep->epnum);
		flush_ep_csr(musb, hw_ep->epnum,  isRx);

		if (isRx) {
			DBG(4, "isRX = 1\n");
			if (qh->type == USB_ENDPOINT_XFER_ISOC) {
				DBG(4, "USB_ENDPOINT_XFER_ISOC\n");
				if (qh->hb_mult == 3)
					musb_writew(epio, MUSB_RXMAXP,
						qh->maxpacket|0x1000);
				else if (qh->hb_mult == 2)
					musb_writew(epio, MUSB_RXMAXP,
						qh->maxpacket|0x800);
				else
					musb_writew(epio, MUSB_RXMAXP,
						qh->maxpacket);
			} else {
				DBG(4, "!! USB_ENDPOINT_XFER_ISOC\n");
				musb_writew(epio, MUSB_RXMAXP, qh->maxpacket);
			}

			musb_writew(epio, MUSB_RXCSR, MUSB_RXCSR_DMAENAB);
			/*CC: speed */
			musb_writeb(epio, MUSB_RXTYPE,
				(qh->type_reg | usb_pipeendpoint(urb->pipe)));
			musb_writeb(epio, MUSB_RXINTERVAL, qh->intv_reg);
#ifdef CONFIG_USB_MTK_HDRC
			if (musb->is_multipoint) {
				DBG(4, "is_multipoint\n");
				musb_write_rxfunaddr(musb->mregs, hw_ep->epnum,
					qh->addr_reg);
				musb_write_rxhubaddr(musb->mregs, hw_ep->epnum,
					qh->h_addr_reg);
				musb_write_rxhubport(musb->mregs, hw_ep->epnum,
					qh->h_port_reg);
			} else {
				DBG(4, "!! is_multipoint\n");
				musb_writeb(musb->mregs, MUSB_FADDR,
					qh->addr_reg);
			}
#endif
			/*turn off intrRx*/
			intr_e = musb_readw(musb->mregs, MUSB_INTRRXE);
			intr_e = intr_e & (~(1<<(hw_ep->epnum)));
			musb_writew(musb->mregs, MUSB_INTRRXE, intr_e);
		} else {
			musb_writew(epio, MUSB_TXMAXP, qh->maxpacket);
			musb_writew(epio, MUSB_TXCSR, MUSB_TXCSR_DMAENAB);
			/*CC: speed?*/
			musb_writeb(epio, MUSB_TXTYPE,
				(qh->type_reg | usb_pipeendpoint(urb->pipe)));
			musb_writeb(epio, MUSB_TXINTERVAL,
				qh->intv_reg);
#ifdef CONFIG_USB_MTK_HDRC
			if (musb->is_multipoint) {
				DBG(4, "is_multipoint\n");
				musb_write_txfunaddr(mbase, hw_ep->epnum,
							qh->addr_reg);
				musb_write_txhubaddr(mbase, hw_ep->epnum,
							qh->h_addr_reg);
				musb_write_txhubport(mbase, hw_ep->epnum,
							qh->h_port_reg);
				/* FIXME if !epnum, do the same for RX ... */
			} else {
				DBG(4, "!! is_multipoint\n");
				musb_writeb(mbase, MUSB_FADDR, qh->addr_reg);
			}
#endif
			/*
			 * Turn off intrTx ,
			 * but this will be revert
			 * by musb_ep_program
			 */
			intr_e = musb_readw(musb->mregs, MUSB_INTRTXE);
			intr_e = intr_e & (~(1<<hw_ep->epnum));
			musb_writew(musb->mregs, MUSB_INTRTXE, intr_e);
		}

		mtk_qmu_enable(musb, hw_ep->epnum, isRx);
	}

	gpd_free_count = qmu_free_gpd_count(isRx, hw_ep->epnum);
	if (qh->type == USB_ENDPOINT_XFER_ISOC) {
		u32 gpd_used_count;

		DBG(4, "USB_ENDPOINT_XFER_ISOC\n");
		pBuffer = urb->transfer_dma;

		if (gpd_free_count < urb->number_of_packets) {
			DBG(0, "gpd_free_count:%d, number_of_packets:%d\n",
				gpd_free_count, urb->number_of_packets);
			DBG(0, "%s:%d Error Here\n", __func__, __LINE__);
			return -ENOSPC;
		}
		for (i = 0; i < urb->number_of_packets; i++) {
			urb->iso_frame_desc[i].status = 0;
			offset = urb->iso_frame_desc[i].offset;
			dwLength = urb->iso_frame_desc[i].length;
			/* If interrupt on complete ? */
			bIsIoc = (i == (urb->number_of_packets-1)) ? 1 : 0;
			DBG(4, "mtk_qmu_insert_task\n");
			mtk_qmu_insert_task(hw_ep->epnum, isRx,
				pBuffer + offset, dwLength, 0, bIsIoc);

			mtk_qmu_resume(hw_ep->epnum, isRx);
		}

		gpd_used_count = qmu_used_gpd_count(isRx, hw_ep->epnum);

		if (!isRx) {
			if (host_qmu_tx_max_active_isoc_gpd[hw_ep->epnum]
				< gpd_used_count)

				host_qmu_tx_max_active_isoc_gpd[hw_ep->epnum]
				= gpd_used_count;

			if (host_qmu_tx_max_number_of_pkts[hw_ep->epnum]
				< urb->number_of_packets)

				host_qmu_tx_max_number_of_pkts[hw_ep->epnum]
				= urb->number_of_packets;

				DBG_LIMIT(1,
					"TXQ[%d], max_isoc gpd:%d, max_pkts:%d, active_dev:%d\n",
					hw_ep->epnum,
					host_qmu_tx_max_active_isoc_gpd
					[hw_ep->epnum],
					host_qmu_tx_max_number_of_pkts
					[hw_ep->epnum],
					mtk_host_active_dev_cnt);

#ifdef CONFIG_MTK_UAC_POWER_SAVING
			DBG(1,
				"mode:%d, activate:%d, ep:%d-%s, mtk_host_active_dev_cnt:%d\n",
					low_power_timer_mode,
					low_power_timer_activate,
					hw_ep->epnum, isRx?"in":"out",
					mtk_host_active_dev_cnt);
			if (low_power_timer_mode
					&& !low_power_timer_activate
					&& mtk_host_active_dev_cnt == 1
					&& hw_ep->epnum == ISOC_EP_START_IDX
					&& usb_on_sram
					&& audio_on_sram
					&& (host_qmu_rx_max_active_isoc_gpd
						[ISOC_EP_START_IDX] == 0)) {
				if (urb->dev->speed == USB_SPEED_FULL)
					low_power_timer_sleep
						(gpd_used_count * 3 / 4);
				else
					low_power_timer_sleep
						(gpd_used_count * 2 / 3 / 8);
			}
#endif
		} else {
			if (host_qmu_rx_max_active_isoc_gpd[hw_ep->epnum]
				< gpd_used_count)

				host_qmu_rx_max_active_isoc_gpd[hw_ep->epnum]
				= gpd_used_count;

			if (host_qmu_rx_max_number_of_pkts[hw_ep->epnum]
				< urb->number_of_packets)

				host_qmu_rx_max_number_of_pkts[hw_ep->epnum]
				= urb->number_of_packets;

			DBG_LIMIT(1,
				"RXQ[%d], max_isoc gpd:%d, max_pkts:%d, active_dev:%d\n",
				hw_ep->epnum,
				host_qmu_rx_max_active_isoc_gpd
				[hw_ep->epnum],
				host_qmu_rx_max_number_of_pkts
				[hw_ep->epnum],
				mtk_host_active_dev_cnt);
		}
	} else {
		/* Must be the bulk transfer type */
		QMU_WARN("non isoc\n");
		pBuffer = urb->transfer_dma;
		if (urb->transfer_buffer_length < QMU_RX_SPLIT_THRE) {
			if (gpd_free_count < 1) {
				DBG(0,
					"gpd_free_count:%d, number_of_packets:%d\n",
					gpd_free_count, urb->number_of_packets);
				DBG(0, "%s:%d Error Here\n",
					__func__, __LINE__);
				return -ENOSPC;
			}
			DBG(4, "urb->transfer_buffer_length : %d\n",
				urb->transfer_buffer_length);

			dwLength = urb->transfer_buffer_length;
			bIsIoc = 1;

			mtk_qmu_insert_task(hw_ep->epnum, isRx,
				pBuffer + offset, dwLength, 0, bIsIoc);
			mtk_qmu_resume(hw_ep->epnum, isRx);
		} else {
			/*reuse isoc urb->unmber_of_packets*/
			urb->number_of_packets =
				((urb->transfer_buffer_length)
				+ QMU_RX_SPLIT_BLOCK_SIZE-1)
				/(QMU_RX_SPLIT_BLOCK_SIZE);
			if (gpd_free_count < urb->number_of_packets) {
				DBG(0,
					"gpd_free_count:%d, number_of_packets:%d\n",
					gpd_free_count, urb->number_of_packets);
				DBG(0, "%s:%d Error Here\n"
					, __func__, __LINE__);
				return -ENOSPC;
			}
			for (i = 0; i < urb->number_of_packets; i++) {
				offset = QMU_RX_SPLIT_BLOCK_SIZE*i;
				dwLength = QMU_RX_SPLIT_BLOCK_SIZE;

				/* If interrupt on complete ? */
				bIsIoc = (i == (urb->number_of_packets-1)) ?
						1 : 0;
				dwLength = (i == (urb->number_of_packets-1)) ?
					((urb->transfer_buffer_length)
					% QMU_RX_SPLIT_BLOCK_SIZE) : dwLength;
				if (dwLength == 0)
					dwLength = QMU_RX_SPLIT_BLOCK_SIZE;

				mtk_qmu_insert_task(hw_ep->epnum, isRx,
					pBuffer + offset, dwLength, 0, bIsIoc);
				mtk_qmu_resume(hw_ep->epnum, isRx);
			}
		}
	}
	/*sync register*/
	mb();

	DBG(4, "\n");
	return 0;
}
