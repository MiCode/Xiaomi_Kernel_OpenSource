/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#if defined(CONFIG_MTK_WATCHDOG_COMMON)
/* use common wd api */
#include <mt-plat/mtk_wd_api.h>
#else
#error "watchdog: common wd_api.h shall be used."
#endif
