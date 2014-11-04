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

#ifndef _hive_isp_css_host_ids_hrt_h_
#define _hive_isp_css_host_ids_hrt_h_

/* ISP_CSS identifiers */
#define ISP           testbench_isp_isp
#define SP            testbench_isp_scp
#define IF_PRIM       testbench_isp_ift_prim
#define IF_PRIM_B     testbench_isp_ift_prim_b
#define IF_SEC        testbench_isp_ift_sec
#define STR_TO_MEM    testbench_isp_mem_cpy
#define CSS_RECEIVER  testbench_isp_css_receiver
#define TC            testbench_isp_gpd_tc
#define DMA           testbench_isp_isp_dma
#define GDC           testbench_isp_gdc
#define IRQ_CTRL      testbench_isp_gpd_irq_ctrl
#define GPIO          testbench_isp_gpd_c_gpio
#define GP_REGS       testbench_isp_gpd_gp_reg
#define MMU           testbench_isp_c_mmu
#define ISEL_FA       testbench_isp_gpd_fa_isel
/* next is actually not FIFO but FIFO adapter, or slave to streaming adapter */
#define ISP_SP_FIFO   testbench_isp_gpd_fa_sp_isp
#define GP_FIFO       testbench_isp_gpd_sf_2isel_in
#define FIFO_GPF_ISEL testbench_isp_gpd_sf_2isel_in
#define FIFO_GPF_SP   testbench_isp_gpd_sf_gpf2sp_in
#define FIFO_GPF_ISP  testbench_isp_gpd_sf_gpf2isp_in
#define FIFO_SP_GPF   testbench_isp_gpd_sf_sp2gpf_in
#define FIFO_ISP_GPF  testbench_isp_gpd_sf_isp2gpf_in
#define OCP_MASTER    testbench_isp_cio2ocp_wide_data_out_mt
#define IF_SEC_MASTER testbench_isp_ift_sec_mt_out
#define SP_IN_FIFO    testbench_isp_gpd_sf_gpf2sp_in
#define SP_OUT_FIFO   testbench_isp_gpd_sf_sp2gpf_out
#define ISP_IN_FIFO   testbench_isp_gpd_sf_gpf2isp_in
#define ISP_OUT_FIFO  testbench_isp_gpd_sf_isp2gpf_out
#define GEN_SHORT_PACK_PORT testbench_isp_ModStrMon_out10

/* Testbench identifiers */
#define DDR           testbench_ddram
#define XMEM          DDR
#define GPIO_ADAPTER  testbench_gp_adapter
#define SIG_MONITOR   testbench_sig_mon
#define DDR_SLAVE     testbench_ddram_ip0
#define HOST_MASTER   host_op0

#endif /* _hive_isp_css_host_ids_hrt_h_ */
