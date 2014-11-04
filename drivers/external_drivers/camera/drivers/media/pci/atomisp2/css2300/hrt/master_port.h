/*
 * Support for Medfield PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * Copyright (c) 2010 Silicon Hive www.siliconhive.com.
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

#ifndef _HRT_MASTER_PORT_H 
#define _HRT_MASTER_PORT_H 


/* Congratulations, you have reached the end of the HRT.
 * Here we split between the hardware implementation (memcpy / assignments)
 * and the software backends (_hrt_master_port_load / _hrt_master_port_store)
 */

#define hrt_master_port_store_char(addr,data)  hrt_master_port_store_8(addr,data)
#define hrt_master_port_store_short(addr,data) hrt_master_port_store_16(addr,data)
#define hrt_master_port_store_int(addr,data)   hrt_master_port_store_32(addr,data)
#define hrt_master_port_store_long(addr,data)  hrt_master_port_store_32(addr,data)
#define hrt_master_port_store_uchar(addr,data)  hrt_master_port_store_8(addr,data)
#define hrt_master_port_store_ushort(addr,data) hrt_master_port_store_16(addr,data)
#define hrt_master_port_store_uint(addr,data)   hrt_master_port_store_32(addr,data)
#define hrt_master_port_store_ulong(addr,data)  hrt_master_port_store_32(addr,data)

#define hrt_master_port_load_char(addr)   hrt_master_port_load_8(addr)
#define hrt_master_port_load_short(addr)  hrt_master_port_load_16(addr)
#define hrt_master_port_load_int(addr)    hrt_master_port_load_32(addr)
#define hrt_master_port_load_long(addr)   hrt_master_port_load_32(addr)
#define hrt_master_port_load_uchar(addr)  hrt_master_port_load_8(addr)
#define hrt_master_port_load_ushort(addr) hrt_master_port_load_16(addr)
#define hrt_master_port_load_uint(addr)   hrt_master_port_load_32(addr)
#define hrt_master_port_load_ulong(addr)  hrt_master_port_load_32(addr)

#define hrt_master_port_store_8(addr,data)  _hrt_master_port_store_8(addr, data)
#define hrt_master_port_store_16(addr,data) _hrt_master_port_store_16(addr, data)
#define hrt_master_port_store_32(addr,data) _hrt_master_port_store_32(addr, data)
#define hrt_master_port_load_8(addr)        _hrt_master_port_load_8(addr)
#define hrt_master_port_load_16(addr)       _hrt_master_port_load_16(addr)
#define hrt_master_port_load_32(addr)       _hrt_master_port_load_32(addr)
#define hrt_master_port_uload_8(addr)       _hrt_master_port_uload_8(addr)
#define hrt_master_port_uload_16(addr)      _hrt_master_port_uload_16(addr)
#define hrt_master_port_uload_32(addr)      _hrt_master_port_uload_32(addr)



#define hrt_master_port_store(addr, data, bytes) \
	_hrt_master_port_unaligned_store((addr), \
					(const void *)(data), bytes)
#define hrt_master_port_load(addr, data, bytes) \
	_hrt_master_port_unaligned_load((addr), \
					(void *)(data), bytes)
#define hrt_master_port_set(addr, data, bytes) \
	_hrt_master_port_unaligned_set((addr), \
					(int)(data), bytes)

/* reduce number of functions */
#define _hrt_master_port_unaligned_store(address, data, size) \
	_hrt_mem_store(address, data, size)
#define _hrt_master_port_unaligned_load(address, data, size) \
	_hrt_mem_load(address, data, size)
#define _hrt_master_port_unaligned_set(address, data, size) \
	_hrt_mem_set(address, data, size)


#if defined(__HIVECC)
#include "master_port_hivecc.h"
#elif defined(HRT_USE_VIR_ADDRS)
/* do nothing, hrt backend is already included */
#elif defined(HRT_HW)
#include "master_port_hw.h"
#else
#include "master_port_sim.h"
#endif



/* To use a combination of variables in crun and bus addresses in other runs, use these: */
#ifdef C_RUN
#define hrt_master_port_scalar_store(type, var, data, addr)             (var) = (data)
#define hrt_master_port_scalar_load(type, var, addr)                    (var)
#define hrt_master_port_indexed_store(type, array, i, data, addr)       ((array)[i]) = (data)
#define hrt_master_port_indexed_load(type, array, i, addr)              ((array)[i])
#else
#define hrt_master_port_scalar_store(type, var, data, addr) \
        HRTCAT(hrt_master_port_store_,type)(addr, data)
#define hrt_master_port_scalar_load(type, var, addr) \
        HRTCAT(hrt_master_port_load_,type)(addr)
#define hrt_master_port_indexed_store(type, array, i, data, addr) \
        HRTCAT(hrt_master_port_store_,type)((addr) + ((i)*sizeof(type)), data)
#define hrt_master_port_indexed_load(type, array, i, addr) \
        HRTCAT(hrt_master_port_load_,type)((addr) + ((i)*sizeof(type)))
#endif /* C_RUN */

#endif /* _HRT_MASTER_PORT_H */
