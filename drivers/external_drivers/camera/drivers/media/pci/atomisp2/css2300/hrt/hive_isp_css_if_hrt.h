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

#ifndef _HIVE_ISP_CSS_IF_HRT_H_
#define _HIVE_ISP_CSS_IF_HRT_H_

#ifdef _HIVE_ISP_CSS_FPGA_SYSTEM
#define dev_sys_isp_clus_ift_prim_vector_alignment   ISP_VEC_ALIGN
#define dev_sys_isp_clus_ift_prim_b_vector_alignment ISP_VEC_ALIGN
#define dev_sys_isp_clus_ift_sec_vector_alignment \
	(dev_sys_isp_clus_ift_sec_mt_out_data_width / 8)
#define dev_sys_isp_clus_mem_cpy_vector_alignment \
	(dev_sys_isp_clus_mem_cpy_mt_out_data_width / 8)
#else
#define testbench_isp_ift_prim_vector_alignment   ISP_VEC_ALIGN
#define testbench_isp_ift_prim_b_vector_alignment ISP_VEC_ALIGN
#define testbench_isp_ift_sec_vector_alignment \
	(testbench_isp_ift_sec_mt_out_data_width / 8)
#define testbench_isp_mem_cpy_vector_alignment \
	(testbench_isp_mem_cpy_mt_out_data_width / 8)
#endif

#endif /* _HIVE_ISP_CSS_IF_HRT_H_ */
