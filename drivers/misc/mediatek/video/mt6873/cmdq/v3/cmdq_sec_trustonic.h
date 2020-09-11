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

#ifndef __CMDQ_SEC_TRUSTONIC_H__
#define __CMDQ_SEC_TRUSTONIC_H__

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
#include "mobicore_driver_api.h"
#endif

/* context for tee vendor */
struct cmdq_sec_tee_context {
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
	/* Universally Unique Identifier of secure tl/dr */
	struct mc_uuid_t uuid;
	struct mc_session_handle session;	/* session handle */
#endif
	/* true if someone has opened mobicore device
	 * in this prpocess context
	 */
	u32 open_mobicore_by_other;
	u32 wsm_size;
};

#endif	/* __CMDQ_SEC_TRUSTONIC_H__ */
