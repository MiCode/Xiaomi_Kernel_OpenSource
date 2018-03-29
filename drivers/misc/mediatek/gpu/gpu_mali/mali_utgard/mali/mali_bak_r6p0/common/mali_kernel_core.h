/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2015 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __MALI_KERNEL_CORE_H__
#define __MALI_KERNEL_CORE_H__

#include "mali_osk.h"

typedef enum {
	_MALI_PRODUCT_ID_UNKNOWN,
	_MALI_PRODUCT_ID_MALI200,
	_MALI_PRODUCT_ID_MALI300,
	_MALI_PRODUCT_ID_MALI400,
	_MALI_PRODUCT_ID_MALI450,
	_MALI_PRODUCT_ID_MALI470,
} _mali_product_id_t;

extern mali_bool mali_gpu_class_is_mali450;
extern mali_bool mali_gpu_class_is_mali470;

_mali_osk_errcode_t mali_initialize_subsystems(void);

void mali_terminate_subsystems(void);

_mali_product_id_t mali_kernel_core_get_product_id(void);

u32 mali_kernel_core_get_gpu_major_version(void);

u32 mali_kernel_core_get_gpu_minor_version(void);

u32 _mali_kernel_core_dump_state(char *buf, u32 size);

MALI_STATIC_INLINE mali_bool mali_is_mali470(void)
{
	return mali_gpu_class_is_mali470;
}

MALI_STATIC_INLINE mali_bool mali_is_mali450(void)
{
	return mali_gpu_class_is_mali450;
}

MALI_STATIC_INLINE mali_bool mali_is_mali400(void)
{
	if (mali_gpu_class_is_mali450 || mali_gpu_class_is_mali470)
		return MALI_FALSE;

	return MALI_TRUE;
}
#endif /* __MALI_KERNEL_CORE_H__ */
