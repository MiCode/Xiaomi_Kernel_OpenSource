/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __SP_PARAMS_H__
#define __SP_PARAMS_H__

#if IS_ENABLED(CONFIG_XT_LOGGING)
int afe_get_sp_xt_logging_data(u16 port_id);
#else
static inline int afe_get_sp_xt_logging_data(u16 port_id)
{
	return 0;
}
#endif

#endif /* __SP_PARAMS_H__ */

