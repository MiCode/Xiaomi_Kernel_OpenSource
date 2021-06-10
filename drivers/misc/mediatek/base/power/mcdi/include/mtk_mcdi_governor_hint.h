/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MTK_MCDI_GOVERNOR_HINT_H__
#define __MTK_MCDI_GOVERNOR_HINT_H__

unsigned int system_idle_hint_result_raw(void);
bool system_idle_hint_result(void);
bool _system_idle_hint_request(unsigned int id, bool value);

#endif /* __MTK_MCDI_GOVERNOR_HINT_H__ */
