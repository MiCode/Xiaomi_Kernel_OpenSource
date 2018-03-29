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


#include <trustzone/kree/mem.h>
#include "trustzone/kree/system.h"
#include <tz_cross/ta_mem.h>
#include <linux/mm.h>


/* #define DBG_KREE_MEM */

/* notiec: handle type is the same */
static inline TZ_RESULT _allocFunc(uint32_t cmd, KREE_SESSION_HANDLE session,
				uint32_t *mem_handle, uint32_t alignment,
				uint32_t size, char *dbg, const char *tag)
{
	MTEEC_PARAM p[4];
	TZ_RESULT ret;
	int handle;

	if ((session == 0) || (mem_handle == NULL) || (size == 0))
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	p[0].value.a = alignment;
	p[1].value.a = size;
	switch (cmd) {
	case TZCMD_MEM_SECUREMEM_ALLOC:
	case TZCMD_MEM_SECURECM_ALLOC:
		ret = KREE_TeeServiceCall(session, cmd,
						TZ_ParamTypes3(TZPT_VALUE_INPUT,
								TZPT_VALUE_INPUT,
								TZPT_VALUE_OUTPUT),
						p);
		handle = p[2].value.a;
		break;
	case TZCMD_MEM_SECUREMEM_ALLOC_WITH_TAG:
	case TZCMD_MEM_SECURECM_ALLOC_WITH_TAG:
	case TZCMD_MEM_SECUREMEM_ZALLOC_WITH_TAG:
	case TZCMD_MEM_SECURECM_ZALLOC_WITH_TAG:
		p[2].mem.buffer = (void *)tag;
		if (tag == NULL)
			p[2].mem.size = 0;
		else
			p[2].mem.size = strlen(tag)+1;
		ret = KREE_TeeServiceCall(session, cmd,
						TZ_ParamTypes4(TZPT_VALUE_INPUT,
								TZPT_VALUE_INPUT,
								TZPT_MEM_INPUT,
								TZPT_VALUE_OUTPUT),
						p);
		handle = p[3].value.a;
		break;
	default:
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("[kree] %s Error: %d\n", dbg, ret);
		return ret;
	}

	*mem_handle = (KREE_SECUREMEM_HANDLE) handle;

	return TZ_RESULT_SUCCESS;
}

static inline TZ_RESULT _handleOpFunc(uint32_t cmd, KREE_SESSION_HANDLE session,
				      uint32_t mem_handle, char *dbg)
{
	MTEEC_PARAM p[4];
	TZ_RESULT ret;

	if ((session == 0) || (mem_handle == 0))
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	p[0].value.a = (uint32_t) mem_handle;
	ret = KREE_TeeServiceCall(session, cmd,
					TZ_ParamTypes1(TZPT_VALUE_INPUT), p);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("[kree] %s Error: %d\n", dbg, ret);
		return ret;
	}

	return TZ_RESULT_SUCCESS;
}

static inline TZ_RESULT _handleOpFunc_1(uint32_t cmd,
					KREE_SESSION_HANDLE session,
					uint32_t mem_handle, uint32_t *count,
					char *dbg)
{
	MTEEC_PARAM p[4];
	TZ_RESULT ret;

	if ((session == 0) || (mem_handle == 0) || (count == NULL))
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	p[0].value.a = (uint32_t) mem_handle;
	ret = KREE_TeeServiceCall(session, cmd,
					TZ_ParamTypes2(TZPT_VALUE_INPUT,
							TZPT_VALUE_OUTPUT),
					p);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("[kree] %s Error: %d\n", dbg, ret);
		*count = 0;
		return ret;
	}

	*count = p[1].value.a;

	return TZ_RESULT_SUCCESS;
}


TZ_RESULT kree_register_sharedmem(KREE_SESSION_HANDLE session,
					KREE_SHAREDMEM_HANDLE *mem_handle,
					void *start, uint32_t size, void *map_p,
					const char *tag)
{
	MTEEC_PARAM p[4];
	TZ_RESULT ret;
	shm_buf_t shmbuf;

	shmbuf.buffer = (unsigned long)start;
	shmbuf.size = size;
	p[0].mem.buffer = &shmbuf;
	p[0].mem.size = sizeof(shm_buf_t);
	p[1].mem.buffer = map_p;
	if (map_p != NULL)
		p[1].mem.size = ((*(uint64_t *)map_p)+1)*sizeof(uint64_t);
	else
		p[1].mem.size = 0;
	p[2].mem.buffer = (void *)tag;
	if (tag != NULL)
		p[2].mem.size = strlen(tag)+1;
	else
		p[2].mem.size = 0;
	ret = KREE_TeeServiceCall(session, TZCMD_MEM_SHAREDMEM_REG_WITH_TAG,
					TZ_ParamTypes4(TZPT_MEM_INPUT,
							TZPT_MEM_INPUT,
							TZPT_MEM_INPUT,
							TZPT_VALUE_OUTPUT),
					p);
	if (ret != TZ_RESULT_SUCCESS) {
		*mem_handle = 0;
		return ret;
	}

	*mem_handle = p[3].value.a;

	return TZ_RESULT_SUCCESS;
}

TZ_RESULT kree_unregister_sharedmem(KREE_SESSION_HANDLE session,
					KREE_SHAREDMEM_HANDLE mem_handle)
{
	MTEEC_PARAM p[4];
	TZ_RESULT ret;

	p[0].value.a = (uint32_t) mem_handle;
	ret = KREE_TeeServiceCall(session, TZCMD_MEM_SHAREDMEM_UNREG,
				  TZ_ParamTypes1(TZPT_VALUE_INPUT), p);
	return ret;
}

/* APIs
*/
static TZ_RESULT KREE_RegisterSharedmem_Helper(KREE_SESSION_HANDLE session,
					KREE_SHAREDMEM_HANDLE *shm_handle,
					KREE_SHAREDMEM_PARAM *param,
					const char *tag)
{
	TZ_RESULT ret;

	if ((session == 0) || (shm_handle == NULL) ||
		(param->buffer == NULL) || (param->size == 0))
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	/* only for kmalloc */
	if ((param->buffer >= (void *)PAGE_OFFSET) &&
	    (param->buffer < high_memory)) {
		ret = kree_register_sharedmem(session, shm_handle,
						param->buffer, param->size,
						0, tag); /* set 0 for no remap... */
		if (ret != TZ_RESULT_SUCCESS) {
			pr_warn("[kree] KREE_RegisterSharedmem Error: %d\n",
				ret);
			return ret;
		}
	} else {
		pr_debug("[kree] KREE_RegisterSharedmem Error: support kmalloc only!!!\n");
		return TZ_RESULT_ERROR_NOT_IMPLEMENTED;
	}

	return TZ_RESULT_SUCCESS;
}

TZ_RESULT KREE_RegisterSharedmem(KREE_SESSION_HANDLE session,
					KREE_SHAREDMEM_HANDLE *shm_handle,
					KREE_SHAREDMEM_PARAM *param)
{
	return KREE_RegisterSharedmem_Helper(session, shm_handle, param, NULL);
}

TZ_RESULT KREE_RegisterSharedmemWithTag(KREE_SESSION_HANDLE session,
					KREE_SHAREDMEM_HANDLE *shm_handle,
					KREE_SHAREDMEM_PARAM *param,
					const char *tag)
{
	return KREE_RegisterSharedmem_Helper(session, shm_handle, param, tag);
}

TZ_RESULT KREE_UnregisterSharedmem(KREE_SESSION_HANDLE session,
					KREE_SHAREDMEM_HANDLE shm_handle)
{
	TZ_RESULT ret;

	if ((session == 0) || (shm_handle == 0))
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	ret = kree_unregister_sharedmem(session, shm_handle);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("[kree] KREE_UnregisterSharedmem Error: %d\n", ret);
		return ret;
	}

	return TZ_RESULT_SUCCESS;
}

TZ_RESULT KREE_AllocSecuremem(KREE_SESSION_HANDLE session,
				KREE_SECUREMEM_HANDLE *mem_handle,
				uint32_t alignment, uint32_t size)
{
	TZ_RESULT ret;

	ret =
	    _allocFunc(TZCMD_MEM_SECUREMEM_ALLOC, session, mem_handle,
			alignment, size, "KREE_AllocSecuremem", NULL);

	return ret;
}

TZ_RESULT KREE_AllocSecurememWithTag(KREE_SESSION_HANDLE session,
				KREE_SECUREMEM_HANDLE *mem_handle,
				uint32_t alignment, uint32_t size, const char *tag)
{
	TZ_RESULT ret;

	ret =
	    _allocFunc(TZCMD_MEM_SECUREMEM_ALLOC_WITH_TAG, session, mem_handle,
			alignment, size, "KREE_AllocSecuremem", tag);

	return ret;
}

TZ_RESULT KREE_ZallocSecurememWithTag(KREE_SESSION_HANDLE session,
				KREE_SECUREMEM_HANDLE *mem_handle,
				uint32_t alignment, uint32_t size, const char *tag)
{
	TZ_RESULT ret;

	ret =
	    _allocFunc(TZCMD_MEM_SECUREMEM_ZALLOC_WITH_TAG, session, mem_handle,
			alignment, size, "KREE_ZallocSecuremem", tag);

	return ret;
}

TZ_RESULT KREE_ReferenceSecuremem(KREE_SESSION_HANDLE session,
					KREE_SECUREMEM_HANDLE mem_handle)
{
	TZ_RESULT ret;

	ret =
	    _handleOpFunc(TZCMD_MEM_SECUREMEM_REF, session, mem_handle,
				"KREE_ReferenceSecuremem");

#ifdef DBG_KREE_MEM
	pr_debug("%s: handle=0x%x count=0x%x\n", __func__, mem_handle, count);
#endif

	return ret;
}

TZ_RESULT KREE_UnreferenceSecuremem(KREE_SESSION_HANDLE session,
					KREE_SECUREMEM_HANDLE mem_handle)
{
	TZ_RESULT ret;
	uint32_t count = 0;

	ret =
	    _handleOpFunc_1(TZCMD_MEM_SECUREMEM_UNREF, session, mem_handle,
				&count, "KREE_UnreferenceSecuremem");
#ifdef DBG_KREE_MEM
	pr_debug("%s: handle=0x%x count=0x%x\n", __func__, mem_handle, count);
#endif

	return ret;
}

TZ_RESULT KREE_AllocSecurechunkmem(KREE_SESSION_HANDLE session,
					KREE_SECUREMEM_HANDLE *cm_handle,
					uint32_t alignment,
					uint32_t size)
{
	TZ_RESULT ret;

	ret =
	    _allocFunc(TZCMD_MEM_SECURECM_ALLOC, session, cm_handle,
			alignment, size, "KREE_AllocSecurechunkmem", NULL);

	return ret;
}

TZ_RESULT KREE_AllocSecurechunkmemWithTag(KREE_SESSION_HANDLE session,
					KREE_SECUREMEM_HANDLE *cm_handle,
					uint32_t alignment,
					uint32_t size, const char *tag)
{
	TZ_RESULT ret;

	ret =
	    _allocFunc(TZCMD_MEM_SECURECM_ALLOC_WITH_TAG, session, cm_handle,
			alignment, size, "KREE_AllocSecurechunkmem", tag);

	return ret;
}

TZ_RESULT KREE_ZallocSecurechunkmemWithTag(KREE_SESSION_HANDLE session,
					KREE_SECUREMEM_HANDLE *cm_handle,
					uint32_t alignment,
					uint32_t size, const char *tag)
{
	TZ_RESULT ret;

	ret =
	    _allocFunc(TZCMD_MEM_SECURECM_ZALLOC_WITH_TAG, session, cm_handle,
			alignment, size, "KREE_ZallocSecurechunkmem", tag);

	return ret;
}

TZ_RESULT KREE_ReferenceSecurechunkmem(KREE_SESSION_HANDLE session,
					KREE_SECURECM_HANDLE cm_handle)
{
	TZ_RESULT ret;

	ret =
	    _handleOpFunc(TZCMD_MEM_SECURECM_REF, session, cm_handle,
			  "KREE_ReferenceSecurechunkmem");

#ifdef DBG_KREE_MEM
	pr_debug("%s: handle=0x%x\n", __func__, cm_handle);
#endif
	return ret;
}

TZ_RESULT KREE_UnreferenceSecurechunkmem(KREE_SESSION_HANDLE session,
					 KREE_SECURECM_HANDLE cm_handle)
{
	TZ_RESULT ret;
	uint32_t count = 0;

	ret =
	    _handleOpFunc_1(TZCMD_MEM_SECURECM_UNREF, session, cm_handle,
				&count, "KREE_UnreferenceSecurechunkmem");
#ifdef DBG_KREE_MEM
	pr_debug("%s: handle=0x%x count=0x%x\n", __func__, cm_handle, count);
#endif

	return ret;
}

TZ_RESULT KREE_ReadSecurechunkmem(KREE_SESSION_HANDLE session, uint32_t offset,
					uint32_t size, void *buffer)
{
	MTEEC_PARAM p[4];
	TZ_RESULT ret;

	if ((session == 0) || (size == 0))
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	p[0].value.a = offset;
	p[1].value.a = size;
	p[2].mem.buffer = buffer;
	p[2].mem.size = size;	/* fix me!!!! */
	ret = KREE_TeeServiceCall(session, TZCMD_MEM_SECURECM_READ,
					TZ_ParamTypes3(TZPT_VALUE_INPUT,
							TZPT_VALUE_INPUT,
							TZPT_MEM_OUTPUT),
					p);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("[kree] KREE_ReadSecurechunkmem Error: %d\n", ret);
		return ret;
	}

	return TZ_RESULT_SUCCESS;
}

TZ_RESULT KREE_WriteSecurechunkmem(KREE_SESSION_HANDLE session, uint32_t offset,
					uint32_t size, void *buffer)
{
	MTEEC_PARAM p[4];
	TZ_RESULT ret;

	if ((session == 0) || (size == 0))
		return TZ_RESULT_ERROR_BAD_PARAMETERS;

	p[0].value.a = offset;
	p[1].value.a = size;
	p[2].mem.buffer = buffer;
	p[2].mem.size = size;	/* fix me!!!! */
	ret = KREE_TeeServiceCall(session, TZCMD_MEM_SECURECM_WRITE,
					TZ_ParamTypes3(TZPT_VALUE_INPUT,
							TZPT_VALUE_INPUT,
							TZPT_MEM_INPUT),
					p);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("[kree] KREE_WriteSecurechunkmem Error: %d\n", ret);
		return ret;
	}

	return TZ_RESULT_SUCCESS;
}


TZ_RESULT KREE_GetSecurechunkReleaseSize(KREE_SESSION_HANDLE session,
						uint32_t *size)
{
	MTEEC_PARAM p[4];
	TZ_RESULT ret;

	ret =
	    KREE_TeeServiceCall(session, TZCMD_MEM_SECURECM_RSIZE,
				TZ_ParamTypes1(TZPT_VALUE_OUTPUT), p);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("[kree] KREE_GetSecurechunkReleaseSize Error: %d\n",
			ret);
		return ret;
	}

	*size = p[0].value.a;

	return TZ_RESULT_SUCCESS;
}

TZ_RESULT KREE_GetTEETotalSize(KREE_SESSION_HANDLE session, uint32_t *size)
{
	MTEEC_PARAM p[4];
	TZ_RESULT ret;

	ret = KREE_TeeServiceCall(session, TZCMD_MEM_TOTAL_SIZE,
					TZ_ParamTypes1(TZPT_VALUE_OUTPUT), p);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_warn("[kree] KREE_GetTEETotalSize Error: %d\n", ret);
		return ret;
	}

	*size = p[0].value.a;

	return TZ_RESULT_SUCCESS;
}


