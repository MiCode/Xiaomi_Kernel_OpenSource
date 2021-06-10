/*
 * Copyright (c) 2018 MediaTek Inc.
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

#include <linux/kernel.h>
#include <linux/module.h>

#include "xhci.h"
#include "xhci-mtk.h"
#include "mtu3.h"
#include "mtu3_hal.h"

static int dpidle_hs_max = -1;
module_param(dpidle_hs_max, int, 0644);
MODULE_PARM_DESC(dpidle_hs_max, "MAX dpidle time for HS UAC");

static int dpidle_fs_max = -1;
module_param(dpidle_fs_max, int, 0644);
MODULE_PARM_DESC(dpidle_fs_max, "MAX dpidle time for FS UAC");

static int dpidle_min = 10; /* default is 10 ms */
module_param(dpidle_min, int, 0644);
MODULE_PARM_DESC(dpidle_min, "MIN dpidle time for UAC");

static struct xhci_mtk_sram_block xhci_sram[XHCI_SRAM_BLOCK_NUM] = {
	[XHCI_EVENTRING] = {0, NULL, TRB_SEGMENT_SIZE, STATE_UNINIT},
	[XHCI_EPTX] = {0, NULL, TRB_SEGMENT_SIZE, STATE_UNINIT},
	[XHCI_EPRX] = {0, NULL, TRB_SEGMENT_SIZE, STATE_UNINIT},
	/* add 56 bytes for alignment */
	[XHCI_DCBAA] = {0, NULL, 8 * MAX_HC_SLOTS + 8 + 56, STATE_UNINIT},
	/* add 48 bytes for alignment */
	[XHCI_ERST] = {0, NULL, 16 * ERST_NUM_SEGS + 48, STATE_UNINIT}
};

static struct xhci_mtk_sram_block usb_audio_sram[USB_AUDIO_DATA_BLOCK_NUM] = {
	[USB_AUDIO_DATA_OUT_EP] = {0, NULL, 0, STATE_UNINIT},
	[USB_AUDIO_DATA_IN_EP] = {0, NULL, 0, STATE_UNINIT},
	[USB_AUDIO_DATA_SYNC_EP] = {0, NULL, 0, STATE_UNINIT}
};

static atomic_t power_status = ATOMIC_INIT(USB_DPIDLE_FORBIDDEN);

static int xhci_mtk_set_power_mode(int mode)
{
	/* TODO */
	if (atomic_read(&power_status) != mode) {
		ssusb_dpidle_request(mode);
		atomic_set(&power_status, mode);
	}
	return 0;
}

static void xhci_mtk_wakeup_timer_func(unsigned long data)
{
	xhci_mtk_set_power_mode(USB_DPIDLE_FORBIDDEN);
}

static DEFINE_TIMER(xhci_wakeup_timer, xhci_mtk_wakeup_timer_func,
					0, 0);

void xhci_mtk_allow_sleep(unsigned int idle_ms, int speed)
{
	int i;
	bool data_sram = false;
	unsigned int sleep_ms = 0;

	switch (speed) {
	case USB_SPEED_SUPER:
	case USB_SPEED_HIGH:
		sleep_ms = idle_ms * 2 / 3 / 8;
		if (dpidle_hs_max >= 0 && sleep_ms > dpidle_hs_max)
			sleep_ms = dpidle_hs_max;
		break;
	case USB_SPEED_FULL:
	case USB_SPEED_LOW:
	default:
		sleep_ms = idle_ms * 3 / 4;
		if (dpidle_fs_max >= 0 && sleep_ms > dpidle_fs_max)
			sleep_ms = dpidle_fs_max;
		break;
	}

	/* step1: check if have enough sleep time */
	if (unlikely(sleep_ms <= dpidle_min))
		goto not_sleep;

	/* step2: check if xhci state is normal */
	for (i = 0; i < XHCI_SRAM_BLOCK_NUM; i++)
		if (unlikely(xhci_sram[i].state == STATE_NOMEM))
			goto not_sleep;

	/* setp3: check if usb audio data on sram */
	for (i = 0; i < USB_AUDIO_DATA_BLOCK_NUM; i++) {
		if (unlikely(usb_audio_sram[i].state == STATE_NOMEM))
			goto not_sleep;
		else if (usb_audio_sram[i].state == STATE_USE)
			data_sram = true;
	}

	if (likely(data_sram)) {
		static DEFINE_RATELIMIT_STATE(ratelimit, 2 * HZ, 1);

		if (__ratelimit(&ratelimit))
			pr_info("mtk_xhci_allow_sleep (%d) ms\n", sleep_ms);
		mod_timer(&xhci_wakeup_timer,
				  jiffies + msecs_to_jiffies(sleep_ms));
		xhci_mtk_set_power_mode(USB_DPIDLE_SRAM);
	}
	return;

not_sleep:
	xhci_mtk_set_power_mode(USB_DPIDLE_FORBIDDEN);
}
EXPORT_SYMBOL_GPL(xhci_mtk_allow_sleep);

void xhci_mtk_set_sleep(bool enable)
{
	if (enable)
		xhci_mtk_set_power_mode(USB_DPIDLE_SRAM);
	else
		xhci_mtk_set_power_mode(USB_DPIDLE_FORBIDDEN);
}
EXPORT_SYMBOL_GPL(xhci_mtk_set_sleep);

int xhci_mtk_init_sram(struct xhci_hcd *xhci)
{
	int i;
	int offset = 0;
	unsigned int xhci_sram_size = 0;

	/* init xhci sram */
	for (i = 0; i < XHCI_SRAM_BLOCK_NUM; i++)
		xhci_sram_size += xhci_sram[i].mlength;

	pr_info("%s size=%d\n", __func__, xhci_sram_size);

	if (mtk_audio_request_sram(&xhci->msram_phys_addr,
			(unsigned char **) &xhci->msram_virt_addr,
			xhci_sram_size, &xhci_sram)) {

		xhci->msram_virt_addr = NULL;

		for (i = 0; i < XHCI_SRAM_BLOCK_NUM; i++)
			xhci_sram[i].state = STATE_NOMEM;

		pr_info("mtk_audio_request_sram fail\n");
		return -ENOMEM;
	}

	for (i = 0; i < XHCI_SRAM_BLOCK_NUM; i++) {
		xhci_sram[i].msram_phys_addr =
			xhci->msram_phys_addr + offset;
		xhci_sram[i].msram_virt_addr =
			(void *)((char *)xhci->msram_virt_addr + offset);
		offset += xhci_sram[i].mlength;
		memset_io(xhci_sram[i].msram_virt_addr,
				  0, xhci_sram[i].mlength);
		xhci_sram[i].state = STATE_INIT;

		pr_debug("[%d] p :%llx, v=%p, len=%d\n",
				 i, xhci_sram[i].msram_phys_addr,
				 xhci_sram[i].msram_virt_addr,
				 xhci_sram[i].mlength);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(xhci_mtk_init_sram);

int xhci_mtk_deinit_sram(struct xhci_hcd *xhci)
{
	int i;

	if (xhci->msram_virt_addr)
		mtk_audio_free_sram(&xhci_sram);

	for (i = 0; i < XHCI_SRAM_BLOCK_NUM; i++) {
		xhci_sram[i].msram_phys_addr = 0;
		xhci_sram[i].msram_virt_addr = NULL;
		xhci_sram[i].state = STATE_UNINIT;
	}
	xhci->msram_virt_addr = NULL;

	pr_info("%s\n", __func__);
	return 0;
}
EXPORT_SYMBOL_GPL(xhci_mtk_deinit_sram);

int xhci_mtk_allocate_sram(unsigned int id, dma_addr_t *sram_phys_addr,
		unsigned char **msram_virt_addr)
{
	if (xhci_sram[id].state == STATE_NOMEM)
		return -ENOMEM;

	if (xhci_sram[id].state == STATE_USE) {
		/* use the same sram block, set state to nomem*/
		xhci_sram[id].state = STATE_NOMEM;
		return -ENOMEM;
	}

	*sram_phys_addr = xhci_sram[id].msram_phys_addr;
	*msram_virt_addr =
		(unsigned char *)xhci_sram[id].msram_virt_addr;

	memset_io(xhci_sram[id].msram_virt_addr,
			  0, xhci_sram[id].mlength);

	xhci_sram[id].state = STATE_USE;

	pr_info("%s get [%d] p :%llx, v=%p, len=%d\n",
			__func__, id, xhci_sram[id].msram_phys_addr,
			xhci_sram[id].msram_virt_addr,
			xhci_sram[id].mlength);
	return 0;
}
EXPORT_SYMBOL_GPL(xhci_mtk_allocate_sram);

int xhci_mtk_free_sram(unsigned int id)
{
	xhci_sram[id].state = STATE_INIT;
	pr_info("%s, id=%d\n", __func__, id);
	return 0;
}
EXPORT_SYMBOL_GPL(xhci_mtk_free_sram);

void *mtk_usb_alloc_sram(unsigned int id, size_t size, dma_addr_t *dma)
{
	void *sram_virt_addr = NULL;

	/* check if xhci control buffer on sram */
	if (xhci_sram[0].state != STATE_USE)
		return NULL;

	mtk_audio_request_sram(dma, (unsigned char **)&sram_virt_addr,
					   size, &usb_audio_sram[id]);

	if (sram_virt_addr) {
		usb_audio_sram[id].mlength = size;
		usb_audio_sram[id].msram_phys_addr = *dma;
		usb_audio_sram[id].msram_virt_addr =  sram_virt_addr;
		usb_audio_sram[id].state = STATE_USE;
		pr_debug("%s, id=%d\n", __func__, id);
	} else {
		usb_audio_sram[id].state = STATE_NOMEM;
		pr_info("%s fail id=%d\n", __func__, id);
	}

	return sram_virt_addr;
}
EXPORT_SYMBOL_GPL(mtk_usb_alloc_sram);

void mtk_usb_free_sram(unsigned int id)
{
	if (usb_audio_sram[id].state == STATE_USE) {
		mtk_audio_free_sram(&usb_audio_sram[id]);
		usb_audio_sram[id].mlength = 0;
		usb_audio_sram[id].msram_phys_addr = 0;
		usb_audio_sram[id].msram_virt_addr =  NULL;
		pr_debug("%s, id=%d\n", __func__, id);
	} else {
		pr_info("%s, fail id=%d\n", __func__, id);
	}
	usb_audio_sram[id].state = STATE_UNINIT;
}
EXPORT_SYMBOL_GPL(mtk_usb_free_sram);
