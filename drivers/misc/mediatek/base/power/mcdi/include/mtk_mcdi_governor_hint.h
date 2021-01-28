/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __MTK_MCDI_GOVERNOR_HINT_H__
#define __MTK_MCDI_GOVERNOR_HINT_H__

unsigned int system_idle_hint_result_raw(void);
bool system_idle_hint_result(void);
bool _system_idle_hint_request(unsigned int id, bool value);

#endif /* __MTK_MCDI_GOVERNOR_HINT_H__ */
