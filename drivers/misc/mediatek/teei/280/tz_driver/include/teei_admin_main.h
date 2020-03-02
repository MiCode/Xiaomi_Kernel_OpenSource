/*
 * Copyright (c) 2015-2017 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __TZ_TEEI_ADMIN_MAIN_H__
#define __TZ_TEEI_ADMIN_MAIN_H__

extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
extern int mt_eint_set_deint(int eint_num, int irq_num);
extern int mt_eint_clr_deint(int eint_num);
extern void neu_disable_touch_irq(void);
extern void neu_enable_touch_irq(void);
extern void *tz_malloc_shared_mem(size_t size, int flags);
extern void tz_free_shared_mem(void *addr, size_t size);

#endif /* __TZ_TEEI_ADMIN_MAIN_H__ */
