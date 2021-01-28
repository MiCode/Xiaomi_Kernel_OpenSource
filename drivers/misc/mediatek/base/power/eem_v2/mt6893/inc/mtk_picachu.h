/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __PICACHU_H__
#define __PICACHU_H__

enum picachu_log_buffer_id {
picachu_log_buffer_id_B,
picachu_log_buffer_id_C,
picachu_log_buffer_id_D,
picachu_log_buffer_id_E,
picachu_log_buffer_id_Picachu,
};

static unsigned int picachu_log_buffer_size[] = {0x10000, 0x10000, 0x10000, 0x10000, 0x20000};

phys_addr_t picachu_reserve_mem_get_phys(enum picachu_log_buffer_id id);
phys_addr_t picachu_reserve_mem_get_virt(enum picachu_log_buffer_id id);
phys_addr_t picachu_reserve_mem_get_size(enum picachu_log_buffer_id id);

#endif




