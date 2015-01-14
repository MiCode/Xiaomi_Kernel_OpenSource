/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2015 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __IA_CSS_TAGGER_COMMON_H__
#define __IA_CSS_TAGGER_COMMON_H__

#include <system_local.h>
#include <type_support.h>

/**
 * @brief The tagger's circular buffer.
 *
 * Should be one less than NUM_CONTINUOUS_FRAMES in sh_css_internal.h
 */
#if defined(HAS_SP_2400)
#define MAX_CB_ELEMS_FOR_TAGGER 14
#else
#define MAX_CB_ELEMS_FOR_TAGGER 9
#endif

/**
 * @brief Data structure for the tagger buffer element.
 */
typedef struct {
	uint32_t frame;	/* the frame value stored in the element */
	uint32_t param;	/* the param value stored in the element */
	uint8_t mark;	/* the mark on the element */
	uint8_t lock;	/* the lock on the element */
	uint8_t exp_id; /* exp_id of frame, for debugging only */
} ia_css_tagger_buf_sp_elem_t;

#endif /* __IA_CSS_TAGGER_COMMON_H__ */
