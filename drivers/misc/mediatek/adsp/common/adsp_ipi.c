/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/slab.h>

//#include <audio_ipi_platform.h>
//#include <audio_messenger_ipi.h>
//#include <adsp_ipi_queue.h>

#include "adsp_platform.h"
#include "adsp_reg.h"
#include "adsp_core.h"
#include "adsp_ipi.h"

#ifdef ADSP_BASE
#undef ADSP_BASE
#endif
#define ADSP_BASE       ipi_base

/* ipi common member */
static void __iomem *ipi_base;
struct adsp_ipi_desc adsp_ipi_descs[ADSP_NR_IPI];

/* platform implement */
bool adsp_check0_swi(void)
{
	return (readl(ADSP_SW_INT_SET) & ADSP_A_SW_INT) > 0;
}

void adsp_write0_swi(void)
{
	writel(ADSP_A_SW_INT, ADSP_SW_INT_SET);
}

void adsp_clr0_irq(void)
{
	writel(ADSP_A_2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
}

bool adsp_check1_swi(void)
{
	return (readl(ADSP_SW_INT_SET) & ADSP_B_SW_INT) > 0;
}

void adsp_write1_swi(void)
{
	writel(ADSP_B_SW_INT, ADSP_SW_INT_SET);
}

void adsp_clr1_irq(void)
{
	writel(ADSP_B_2HOST_IRQ_BIT, ADSP_GENERAL_IRQ_CLR);
}

irqreturn_t adsp_ipi_handler(int irq, void *data)
{
	struct ipi_ctrl *ipi_c = (struct ipi_ctrl *)data;

	enum adsp_ipi_id ipi_id;
	u8 share_buf[SHARE_BUF_SIZE - 16];
	u32 len;

	ipi_id = ipi_c->recv_obj->id;
	len = ipi_c->recv_obj->len;
	memcpy_fromio(share_buf, (void *)ipi_c->recv_obj->share_buf, len);

	ipi_c->clr_irq();

	if (ipi_id >= ADSP_NR_IPI) {
		pr_info("%s(), ipi_id:%d invalid", __func__, ipi_id);
		return IRQ_HANDLED;
	}

	if (adsp_ipi_descs[ipi_id].handler == NULL) {
		pr_info("%s(), ipi_handle[%d] is null", __func__, ipi_id);
		return IRQ_HANDLED;
	}

	if (ipi_id == ADSP_IPI_ADSP_A_READY ||
	    ipi_id == ADSP_IPI_LOGGER_INIT_A) {
		/*
		 * adsp_ready & logger init ipi bypass send to ipi
		 * queue and do callback directly. (which will in isr)
		 * Must ensure the callback can do in isr
		 */
		adsp_ipi_descs[ipi_id].handler(ipi_id, share_buf, len);
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}

struct ipi_ctrl *adsp_ipi_init0(void *sharedmem, int irq, void *base)
{
	int ret = 0;
	struct ipi_ctrl *ipi_c;

	/* init ipi ctrl */
	ipi_c = kzalloc(sizeof(*ipi_c), GFP_KERNEL);
	if (!ipi_c)
		return ERR_PTR(-ENOMEM);

	if (!sharedmem)
		return ERR_PTR(-EINVAL);


	mutex_init(&ipi_c->s_mutex);
	ipi_c->recv_obj = (struct adsp_share_obj *)sharedmem;
	ipi_c->send_obj = ipi_c->recv_obj + 1;
	if (!ipi_base)
		ipi_base = base;

	/* request irq */
	ret = request_irq(irq, adsp_ipi_handler, IRQF_TRIGGER_LOW,
			"ADSP A IPC2HOST", ipi_c);

	ipi_c->clr_irq = adsp_clr0_irq;
	ipi_c->check_swi = adsp_check0_swi;
	ipi_c->write_swi = adsp_write0_swi;

	return ipi_c;
}

struct ipi_ctrl *adsp_ipi_init1(void *sharedmem, int irq, void *base)
{
	int ret = 0;
	struct ipi_ctrl *ipi_c;

	/* init ipi ctrl */
	ipi_c = kzalloc(sizeof(*ipi_c), GFP_KERNEL);
	if (!ipi_c)
		return ERR_PTR(-ENOMEM);

	if (!sharedmem)
		return ERR_PTR(-EINVAL);


	mutex_init(&ipi_c->s_mutex);
	ipi_c->recv_obj = (struct adsp_share_obj *)sharedmem;
	ipi_c->send_obj = ipi_c->recv_obj + 1;

	/* request irq */
	ret = request_irq(irq, adsp_ipi_handler, IRQF_TRIGGER_LOW,
			"ADSP B IPC2HOST", ipi_c);

	ipi_c->clr_irq = adsp_clr1_irq;
	ipi_c->check_swi = adsp_check1_swi;
	ipi_c->write_swi = adsp_write1_swi;

	return ipi_c;
}

enum adsp_ipi_status adsp_ipi_send_ipc(struct ipi_ctrl *ipi_c, void *buf,
				       unsigned int len, unsigned int wait)
{
	ktime_t start_time;
	s64     time_ipc_us;

	if (mutex_trylock(&ipi_c->s_mutex) == 0) {
		pr_info("%s(), mutex_trylock busy", __func__);
		return ADSP_IPI_BUSY;
	}

	if (ipi_c->check_swi()) {
		mutex_unlock(&ipi_c->s_mutex);
		return ADSP_IPI_BUSY;
	}

	memcpy_toio((void *)ipi_c->send_obj, buf, len);
	dsb(SY);

	/* send host to adsp ipi */
	ipi_c->write_swi();

	if (wait) {
		start_time = ktime_get();
		while (ipi_c->check_swi()) {
			time_ipc_us = ktime_us_delta(ktime_get(), start_time);
			if (time_ipc_us > 2000) /* 1 ms */
				break;
		}
	}

	mutex_unlock(&ipi_c->s_mutex);
	return ADSP_IPI_DONE;
}

/*
 * API let apps can register an ipi handler to receive IPI
 * @param id:      IPI ID
 * @param handler:  IPI handler
 * @param name:  IPI name
 */
enum adsp_ipi_status adsp_ipi_registration(
	enum adsp_ipi_id id,
	void (*ipi_handler)(int id, void *data, unsigned int len),
	const char *name)
{
	if (id < ADSP_NR_IPI) {
		adsp_ipi_descs[id].name = name;

		if (ipi_handler == NULL)
			return ADSP_IPI_ERROR;

		adsp_ipi_descs[id].handler = ipi_handler;
		return ADSP_IPI_DONE;
	} else
		return ADSP_IPI_ERROR;
}
EXPORT_SYMBOL_GPL(adsp_ipi_registration);

/*
 * API let apps unregister an ipi handler
 * @param id:      IPI ID
 */
enum adsp_ipi_status adsp_ipi_unregistration(enum adsp_ipi_id id)
{
	if (id < ADSP_NR_IPI) {
		adsp_ipi_descs[id].name = "";
		adsp_ipi_descs[id].handler = NULL;
		return ADSP_IPI_DONE;
	} else
		return ADSP_IPI_ERROR;
}
EXPORT_SYMBOL_GPL(adsp_ipi_unregistration);
