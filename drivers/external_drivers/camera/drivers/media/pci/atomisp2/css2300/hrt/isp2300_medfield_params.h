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

/* Version */
#define ISP_VERSION SDK_VERSION

/* Cell name  */

#define ISP_CELL_TYPE         isp2300_medfield
#define ISP_VMEM              simd_vmem
#define _HRT_ISP_VMEM         isp2300_medfield_simd_vmem

/* instruction pipeline depth */
#define ISP_BRANCHDELAY       3

/* bus */
#define ISP_BUS_PROT          CIO
#define ISP_BUS_WIDTH         32
#define ISP_BUS_ADDR_WIDTH    32
#define ISP_BUS_BURST_SIZE    1

/* data-path */
#define ISP_SCALAR_WIDTH      32
#define ISP_SLICE_NELEMS      4
#define ISP_VEC_NELEMS        64
#define ISP_VEC_ELEMBITS      14
#define ISP_VEC_ELEM8BITS     16
#define ISP_ALPHA_BLEND_SHIFT 13


/* memories */
#define ISP_DMEM_DEPTH            4096
#define ISP_VMEM_DEPTH            2048
#define ISP_VMEM_ELEMBITS         14
#define ISP_VMEM_ELEM_PRECISION   14
#define ISP_PMEM_DEPTH            2048
#define ISP_PMEM_WIDTH            448
#define ISP_VAMEM_ADDRESS_BITS    13
#define ISP_VAMEM_ELEMBITS        12
#define ISP_VAMEM_DEPTH           4096
#define ISP_VAMEM_ALIGNMENT       2
#define ISP_VA_ADDRESS_WIDTH      896
#define ISP_VEC_VALSU_LATENCY     ISP_VEC_NELEMS

/* program counter */
#define ISP_PC_WIDTH          12

/* RSN pipelining */
#define ISP_RSN_PIPE          0

/* shrink instruction set */
#define ISP_SHRINK_IS         0

/* Template experiments */
#define ISP_HAS_VARU_SIMD_IS1 0     
#define ISP_HAS_SIMD_IS5      1     
#define ISP_HAS_SIMD6_FLGU    0
#define ISP_HAS_VALSU         1     
#define ISP_SRU_GUARDING      1
#define ISP_VRF_RAM     	    1
#define ISP_SRF_RAM     	    1
#define ISP_REDUCE_VARUS      0
#define ISP_COMBINE_MAC_SHIFT 0    
#define ISP_COMBINE_MAC_VARU  1    
#define ISP_COMBINE_SHIFT_VARU 0    
#define ISP_SLICE_LATENCY     1    
#define ISP_RFSPLIT_EXP       0
#define ISP_SPILL_MEM_EXP     0
#define ISP_VHSU_NO_WIDE      0
#define ISP_NO_SLICE          0
#define ISP_BLOCK_SLICE       0
#define ISP_IF                1
#define ISP_IF_B              1
#define ISP_DMA               0     
#define ISP_OF                0     
#define ISP_SYS_OF            1   
#define ISP_GDC               1   
#define ISP_GPIO              1    
#define ISP_SP                1  
#define ISP_HAS_IRQ           1

/* derived values */
#define ISP_VEC_WIDTH         896
#define ISP_SLICE_WIDTH       56
#define ISP_VMEM_ALIGN        128
#define ISP_VMEM_WIDTH        896
#define ISP_SIMDLSU           1
#define ISP_LSU_IMM_BITS      12
#define ISP_CRUN_VEC_ALIGN    

/* convenient shortcuts for software*/
#define ISP_NWAY              ISP_VEC_NELEMS 
#define NBITS                       ISP_VEC_ELEMBITS

#define _isp_ceil_div(a,b)          (((a)+(b)-1)/(b))

#ifdef C_RUN
#define ISP_VEC_ALIGN         (_isp_ceil_div(ISP_VEC_WIDTH, 64)*8)
#else
#define ISP_VEC_ALIGN         ISP_VMEM_ALIGN
#endif

/* HRT specific vector support */
#define isp2300_medfield_vector_alignment      ISP_VEC_ALIGN
#define isp2300_medfield_vector_elem_bits      ISP_VMEM_ELEMBITS
#define isp2300_medfield_vector_elem_precision ISP_VMEM_ELEM_PRECISION
#define isp2300_medfield_vector_num_elems      ISP_VEC_NELEMS

/* register file sizes */
#define ISP_RF0_SIZE        64
#define ISP_RF1_SIZE        8
#define ISP_RF2_SIZE        64
#define ISP_RF3_SIZE        32
#define ISP_RF4_SIZE        32
#define ISP_RF5_SIZE        32
#define ISP_RF6_SIZE        16
#define ISP_VRF0_SIZE       16
#define ISP_VRF1_SIZE       16
#define ISP_VRF2_SIZE       16
#define ISP_VRF3_SIZE       16
#define ISP_VRF4_SIZE       16
#define ISP_VRF5_SIZE       16
#define ISP_VRF6_SIZE       16
#define ISP_VRF7_SIZE       16
#define ISP_VRF8_SIZE       16
#define ISP_SRF0_SIZE       64
#define ISP_SRF1_SIZE       64
#define ISP_FRF0_SIZE       16
#define ISP_FRF1_SIZE       16
/* register file read latency */
#define ISP_VRF0_READ_LAT       0
#define ISP_VRF1_READ_LAT       0
#define ISP_VRF2_READ_LAT       0
#define ISP_VRF3_READ_LAT       0
#define ISP_VRF4_READ_LAT       0
#define ISP_VRF5_READ_LAT       0
#define ISP_VRF6_READ_LAT       0
#define ISP_VRF7_READ_LAT       0
#define ISP_VRF8_READ_LAT       0
#define ISP_SRF0_READ_LAT       0
#define ISP_SRF1_READ_LAT       0
/* immediate sizes */
#define ISP_IS1_IMM_BITS        13
#define ISP_IS2_IMM_BITS        10
#define ISP_IS3_IMM_BITS        7
#define ISP_IS4_IMM_BITS        7
#define ISP_IS5_IMM_BITS        13
#define ISP_IS6_IMM_BITS        7
#define ISP_IS7_IMM_BITS        7
#define ISP_IS8_IMM_BITS        7
#define ISP_IS9_IMM_BITS        7
/* fifo depths */
#define ISP_IF_FIFO_DEPTH         0
#define ISP_IF_B_FIFO_DEPTH       0
#define ISP_DMA_FIFO_DEPTH        0
#define ISP_OF_FIFO_DEPTH         0
#define ISP_GDC_FIFO_DEPTH        0
#define ISP_GPIO_FIFO_DEPTH       0
#define ISP_SP_FIFO_DEPTH         0
