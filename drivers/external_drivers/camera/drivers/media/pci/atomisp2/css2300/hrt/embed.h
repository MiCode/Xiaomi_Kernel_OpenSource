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

#ifndef _HRT_EMBED_H_
#define _HRT_EMBED_H_

#define _hrt_cell_dummy_use_blob(prog) \
	HRTCAT(_hrt_dummy_use_blob_, prog)()
#define _hrt_program_transfer_func(prog) \
	HRTCAT(_hrt_transfer_embedded_, prog)
#define _hrt_program_blob(prog) \
	(HRTCAT(_hrt_blob_, prog).data)
#define hrt_embedded_program_size(prog) \
	HRTCAT(_hrt_size_of_, prog)
#define hrt_embedded_program_text_size(prog) \
	HRTCAT(_hrt_text_size_of_, prog)

#endif /* _HRT_EMBED_H_ */
