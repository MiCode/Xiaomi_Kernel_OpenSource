/* Copyright (c) 2009-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LIBRA_SDIOIF_H__
#define __LIBRA_SDIOIF_H__

/*
 * Header for SDIO Card Interface Functions
 */
#include <linux/kthread.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>

/*
 * Common Defines
 */
#define LIBRA_MAN_ID              0x70
#define LIBRA_REV_1_0_CARD_ID     0x0

#define VOLANS_MAN_ID             0x70
#define VOLANS_REV_1_0_CARD_ID    0x0
#define VOLANS_REV_2_0_CARD_ID    0x2881

typedef int (suspend_handler_t)(struct sdio_func *);
typedef void (resume_handler_t)(struct sdio_func *);
typedef void (notify_card_removal_t)(void);

int libra_enable_sdio_irq_in_chip(struct sdio_func *func, u8 enable);
int    libra_sdio_configure(sdio_irq_handler_t libra_sdio_rxhandler,
		void (*func_drv_fn)(int *status),
		u32 funcdrv_timeout, u32 blksize);
void   libra_sdio_deconfigure(struct sdio_func *func);
struct sdio_func *libra_getsdio_funcdev(void);
void   libra_sdio_setprivdata(struct sdio_func *sdio_func_dev,
		void *padapter);
void   *libra_sdio_getprivdata(struct sdio_func *sdio_func_dev);
void   libra_claim_host(struct sdio_func *sdio_func_dev,
		pid_t *curr_claimed, pid_t current_pid,
		atomic_t *claim_count);
void   libra_release_host(struct sdio_func *sdio_func_dev,
		pid_t *curr_claimed, pid_t current_pid,
		atomic_t *claim_count);
void   libra_sdiocmd52(struct sdio_func *sdio_func_dev,
		u32 addr, u8 *b, int write, int *err_ret);
u8     libra_sdio_readsb(struct sdio_func *func, void *dst,
		unsigned int addr, int count);
int    libra_sdio_memcpy_fromio(struct sdio_func *func,
		void *dst, unsigned int addr, int count);
int    libra_sdio_writesb(struct sdio_func *func,
		unsigned int addr, void *src, int count);
int    libra_sdio_memcpy_toio(struct sdio_func *func,
		unsigned int addr, void *src, int count);
int    libra_sdio_enable_polling(void);

int libra_sdio_configure_suspend_resume(
		suspend_handler_t *libra_sdio_suspend_hdlr,
		resume_handler_t *libra_sdio_resume_hdlr);

int libra_detect_card_change(void);

void libra_sdio_set_clock(struct sdio_func *func, unsigned int clk_freq);
void libra_sdio_get_card_id(struct sdio_func *func, unsigned short *card_id);
void libra_sdio_release_irq(struct sdio_func *func);
int libra_enable_sdio_irq(struct sdio_func *func, u8 enable);
void libra_sdio_disable_func(struct sdio_func *func);
int libra_disable_sdio_irq_capability(struct sdio_func *func, u8 disable);
int libra_sdio_notify_card_removal(
		notify_card_removal_t *libra_sdio_notify_card_removal_hdlr);
#endif /* __LIBRA_SDIOIF_H__ */
