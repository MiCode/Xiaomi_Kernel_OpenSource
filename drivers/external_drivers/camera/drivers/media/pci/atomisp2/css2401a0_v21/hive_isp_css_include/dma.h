/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
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

#ifndef __DMA_H_INCLUDED__
#define __DMA_H_INCLUDED__

/*
 * This file is included on every cell {SP,ISP,host} and on every system
 * that uses the DMA device. It defines the API to DLI bridge
 *
 * System and cell specific interfaces and inline code are included
 * conditionally through Makefile path settings.
 *
 *  - .        system and cell agnostic interfaces, constants and identifiers
 *	- public:  system agnostic, cell specific interfaces
 *	- private: system dependent, cell specific interfaces & inline implementations
 *	- global:  system specific constants and identifiers
 *	- local:   system and cell specific constants and identifiers
 *
 */

#include "storage_class.h"

#include "system_local.h"
#include "dma_local.h"

#ifndef __INLINE_DMA__
#define STORAGE_CLASS_DMA_H STORAGE_CLASS_EXTERN
#define STORAGE_CLASS_DMA_C 
#include "dma_public.h"
#else  /* __INLINE_DMA__ */
#define STORAGE_CLASS_DMA_H STORAGE_CLASS_INLINE
#define STORAGE_CLASS_DMA_C STORAGE_CLASS_INLINE
#include "dma_private.h"
#endif /* __INLINE_DMA__ */

#endif /* __DMA_H_INCLUDED__ */
