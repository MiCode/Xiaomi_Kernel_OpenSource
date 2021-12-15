/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

/*
 * Header files for basic KREE functions.
 */

#ifndef __KREE_H__
#define __KREE_H__

#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) || defined(CONFIG_TRUSTY)

#include "trustzone/tz_cross/trustzone.h"

/* / KREE session handle type. */
typedef uint32_t KREE_SESSION_HANDLE;

typedef uint32_t KREE_SHAREDMEM_HANDLE;


/* Session Management */
/**
 *  Create a new TEE sesssion
 *
 * @param ta_uuid UUID of the TA to connect to.
 * @param pHandle Handle for the new session.
 *	  Return KREE_SESSION_HANDLE_FAIL if fail.
 * @return return code
 */
int KREE_CreateSession(const char *ta_uuid, KREE_SESSION_HANDLE *pHandle);

/**
 *  Create a new TEE sesssion with tag for debug purpose
 *
 * @param ta_uuid UUID of the TA to connect to.
 * @param pHandle Handle for the new session.
 *	  Return KREE_SESSION_HANDLE_FAIL if fail.
 * @param tag string can be printed when querying memory usage.
 * @return return code
 */
int KREE_CreateSessionWithTag(const char *ta_uuid,
				KREE_SESSION_HANDLE *pHandle, const char *tag);

/**
 * Close TEE session
 *
 * @param handle Handle for session to close.
 * @return return code
 */
int KREE_CloseSession(KREE_SESSION_HANDLE handle);


/**
 * Make a TEE service call
 *
 * @param handle      Session handle to make the call
 * @param command     The command to call.
 * @param paramTypes  Types for the parameters, construct by TZ_ParamTypes().
 * @param param       The parameters to pass to TEE. Maximum 4 params.
 * @return            Return value from TEE service.
 */
int KREE_TeeServiceCall(KREE_SESSION_HANDLE handle, uint32_t command,
			      uint32_t paramTypes, union MTEEC_PARAM param[4]);

#endif	/* CONFIG_MTK_IN_HOUSE_TEE_SUPPORT || CONFIG_TRUSTY */
#endif	/* __KREE_H__ */
