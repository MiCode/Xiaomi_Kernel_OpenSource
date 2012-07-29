/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _AUDIO_OCMEM_H_
#define _AUDIO_OCMEM_H_

#include <linux/module.h>
#include <linux/notifier.h>

#include <mach/ocmem.h>

#define AUDIO 0
#define VOICE 1

#ifdef CONFIG_AUDIO_OCMEM
int audio_ocmem_process_req(int id, bool enable);
int voice_ocmem_process_req(int cid, bool enable);
int enable_ocmem_after_voice(int cid);
int disable_ocmem_for_voice(int cid);
#else
static inline int audio_ocmem_process_req(int id, bool enable)\
						{ return 0; }
static inline int voice_ocmem_process_req(int cid, bool enable)\
						{ return 0; }
static inline int enable_ocmem_after_voice(int cid) { return 0; }
static inline int disable_ocmem_for_voice(int cid) { return 0; }
#endif

#endif
