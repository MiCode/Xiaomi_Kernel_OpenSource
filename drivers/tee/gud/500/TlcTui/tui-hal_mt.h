/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __TUI_HAL_MT__H__
#define __TUI_HAL_MT__H__

/* for TUI mepping to Security World */
extern void gt1x_power_reset(void);
extern int mt_eint_set_deint(int eint_num, int irq_num);
extern int mt_eint_clr_deint(int eint_num);
extern int tpd_reregister_from_tui(void);
extern int secmem_api_alloc(u32 alignment, u32 size, u32 *refcount,
	u32 *sec_handle, uint8_t *owner, uint32_t id);
extern int secmem_api_unref(u32 sec_handle, uint8_t *owner, uint32_t id);

extern int tpd_enter_tui(void);
extern int tpd_exit_tui(void);
extern int i2c_tui_enable_clock(int id);
extern int i2c_tui_disable_clock(int id);
extern int tui_region_offline(phys_addr_t *pa, unsigned long *size);
extern int tui_region_offline64(phys_addr_t *pa, unsigned long *size);
extern int tui_region_online(void);
extern int display_enter_tui(void);
extern int display_exit_tui(void);
#endif
