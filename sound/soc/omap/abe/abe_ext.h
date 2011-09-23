/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  BSD LICENSE

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Texas Instruments Incorporated nor the names of
      its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef _ABE_EXT_H_
#define _ABE_EXT_H_

/*
 * OS DEPENDENT MMU CONFIGURATION
 */
#define ABE_PMEM_BASE_OFFSET_MPU	0xe0000
#define ABE_CMEM_BASE_OFFSET_MPU	0xa0000
#define ABE_SMEM_BASE_OFFSET_MPU	0xc0000
#define ABE_DMEM_BASE_OFFSET_MPU	0x80000
#define ABE_ATC_BASE_OFFSET_MPU		0xf1000
/* default base address for io_base */
#define ABE_DEFAULT_BASE_ADDRESS_L3 0x49000000L
#define ABE_DEFAULT_BASE_ADDRESS_L4 0x40100000L
#define ABE_DEFAULT_BASE_ADDRESS_DEFAULT ABE_DEFAULT_BASE_ADDRESS_L3
/*
 * HARDWARE AND PERIPHERAL DEFINITIONS
 */
/* PMEM SIZE in bytes (1024 words of 64 bits: : #32bits words x 4)*/
#define ABE_PMEM_SIZE 8192
/* CMEM SIZE in bytes (2048 coeff : #32bits words x 4)*/
#define ABE_CMEM_SIZE 8192
/* SMEM SIZE in bytes (3072 stereo samples : #32bits words x 4)*/
#define ABE_SMEM_SIZE 24576
/* DMEM SIZE in bytes */
#define ABE_DMEM_SIZE 65536L
/* ATC REGISTERS SIZE in bytes */
#define ABE_ATC_DESC_SIZE 512
/* holds the MCU Irq signal */
#define ABE_MCU_IRQSTATUS_RAW 0x24
/* status : clear the IRQ */
#define ABE_MCU_IRQSTATUS	0x28
/* holds the DSP Irq signal */
#define ABE_DSP_IRQSTATUS_RAW 0x4C
/* holds the DMA req lines to the sDMA */
#define ABE_DMASTATUS_RAW 0x84
#define EVENT_GENERATOR_COUNTER 0x68
/* PLL output/desired sampling rate = (32768 * 6000)/96000 */
#define EVENT_GENERATOR_COUNTER_DEFAULT (2048-1)
/* PLL output/desired sampling rate = (32768 * 6000)/88200 */
#define EVENT_GENERATOR_COUNTER_44100 (2228-1)
/* start / stop the EVENT generator */
#define EVENT_GENERATOR_START 0x6C
#define EVENT_GENERATOR_ON 1
#define EVENT_GENERATOR_OFF 0
/* selection of the EVENT generator source */
#define EVENT_SOURCE_SELECTION 0x70
#define EVENT_SOURCE_DMA 0
#define EVENT_SOURCE_COUNTER 1
/* selection of the ABE DMA req line from ATC */
#define AUDIO_ENGINE_SCHEDULER 0x74
#define ABE_ATC_DMIC_DMA_REQ 1
#define ABE_ATC_MCPDMDL_DMA_REQ 2
#define ABE_ATC_MCPDMUL_DMA_REQ 3
/* Direction=0 means input from ABE point of view */
#define ABE_ATC_DIRECTION_IN 0
/* Direction=1 means output from ABE point of view */
#define ABE_ATC_DIRECTION_OUT 1
/*
 * DMA requests
 */
/*Internal connection doesn't connect at ABE boundary */
#define External_DMA_0	0
/*Transmit request digital microphone */
#define DMIC_DMA_REQ	1
/*Multichannel PDM downlink */
#define McPDM_DMA_DL	2
/*Multichannel PDM uplink */
#define McPDM_DMA_UP	3
/*MCBSP module 1 - transmit request */
#define MCBSP1_DMA_TX	4
/*MCBSP module 1 - receive request */
#define MCBSP1_DMA_RX	5
/*MCBSP module 2 - transmit request */
#define MCBSP2_DMA_TX	6
/*MCBSP module 2 - receive request */
#define MCBSP2_DMA_RX	7
/*MCBSP module 3 - transmit request */
#define MCBSP3_DMA_TX	8
/*MCBSP module 3 - receive request */
#define MCBSP3_DMA_RX	9
/*SLIMBUS module 1 - transmit request channel 0 */
#define SLIMBUS1_DMA_TX0	10
/*SLIMBUS module 1 - transmit request channel 1 */
#define SLIMBUS1_DMA_TX1	11
/*SLIMBUS module 1 - transmit request channel 2 */
#define SLIMBUS1_DMA_TX2	12
/*SLIMBUS module 1 - transmit request channel 3 */
#define SLIMBUS1_DMA_TX3	13
/*SLIMBUS module 1 - transmit request channel 4 */
#define SLIMBUS1_DMA_TX4	14
/*SLIMBUS module 1 - transmit request channel 5 */
#define SLIMBUS1_DMA_TX5	15
/*SLIMBUS module 1 - transmit request channel 6 */
#define SLIMBUS1_DMA_TX6	16
/*SLIMBUS module 1 - transmit request channel 7 */
#define SLIMBUS1_DMA_TX7	17
/*SLIMBUS module 1 - receive request channel 0 */
#define SLIMBUS1_DMA_RX0	18
/*SLIMBUS module 1 - receive request channel 1 */
#define SLIMBUS1_DMA_RX1	19
/*SLIMBUS module 1 - receive request channel 2 */
#define SLIMBUS1_DMA_RX2	20
/*SLIMBUS module 1 - receive request channel 3 */
#define SLIMBUS1_DMA_RX3	21
/*SLIMBUS module 1 - receive request channel 4 */
#define SLIMBUS1_DMA_RX4	22
/*SLIMBUS module 1 - receive request channel 5 */
#define SLIMBUS1_DMA_RX5	23
/*SLIMBUS module 1 - receive request channel 6 */
#define SLIMBUS1_DMA_RX6	24
/*SLIMBUS module 1 - receive request channel 7 */
#define SLIMBUS1_DMA_RX7	25
/*McASP - Data transmit DMA request line */
#define McASP1_AXEVT	26
/*McASP - Data receive DMA request line */
#define McASP1_AREVT	29
/*DUMMY FIFO @@@ */
#define _DUMMY_FIFO_	30
/*DMA of the Circular buffer peripheral 0 */
#define CBPr_DMA_RTX0	32
/*DMA of the Circular buffer peripheral 1 */
#define CBPr_DMA_RTX1	33
/*DMA of the Circular buffer peripheral 2 */
#define CBPr_DMA_RTX2	34
/*DMA of the Circular buffer peripheral 3 */
#define CBPr_DMA_RTX3	35
/*DMA of the Circular buffer peripheral 4 */
#define CBPr_DMA_RTX4	36
/*DMA of the Circular buffer peripheral 5 */
#define CBPr_DMA_RTX5	37
/*DMA of the Circular buffer peripheral 6 */
#define CBPr_DMA_RTX6	38
/*DMA of the Circular buffer peripheral 7 */
#define CBPr_DMA_RTX7	39
/*
 * ATC DESCRIPTORS - DESTINATIONS
 */
#define DEST_DMEM_access	0x00
#define DEST_MCBSP1_ TX	 0x01
#define DEST_MCBSP2_ TX	 0x02
#define DEST_MCBSP3_TX	 0x03
#define DEST_SLIMBUS1_TX0 0x04
#define DEST_SLIMBUS1_TX1 0x05
#define DEST_SLIMBUS1_TX2 0x06
#define DEST_SLIMBUS1_TX3 0x07
#define DEST_SLIMBUS1_TX4 0x08
#define DEST_SLIMBUS1_TX5 0x09
#define DEST_SLIMBUS1_TX6 0x0A
#define DEST_SLIMBUS1_TX7 0x0B
#define DEST_MCPDM_DL 0x0C
#define DEST_MCASP_TX0 0x0D
#define DEST_MCASP_TX1 0x0E
#define DEST_MCASP_TX2 0x0F
#define DEST_MCASP_TX3 0x10
#define DEST_EXTPORT0 0x11
#define DEST_EXTPORT1 0x12
#define DEST_EXTPORT2 0x13
#define DEST_EXTPORT3 0x14
#define DEST_MCPDM_ON 0x15
#define DEST_CBP_CBPr 0x3F
/*
 * ATC DESCRIPTORS - SOURCES
 */
#define SRC_DMEM_access	0x0
#define SRC_MCBSP1_ RX 0x01
#define SRC_MCBSP2_RX 0x02
#define SRC_MCBSP3_RX 0x03
#define SRC_SLIMBUS1_RX0 0x04
#define SRC_SLIMBUS1_RX1 0x05
#define SRC_SLIMBUS1_RX2 0x06
#define SRC_SLIMBUS1_RX3 0x07
#define SRC_SLIMBUS1_RX4 0x08
#define SRC_SLIMBUS1_RX5 0x09
#define SRC_SLIMBUS1_RX6 0x0A
#define SRC_SLIMBUS1_RX7 0x0B
#define SRC_DMIC_UP 0x0C
#define SRC_MCPDM_UP 0x0D
#define SRC_MCASP_RX0 0x0E
#define SRC_MCASP_RX1 0x0F
#define SRC_MCASP_RX2 0x10
#define SRC_MCASP_RX3 0x11
#define SRC_CBP_CBPr 0x3F
#endif/* _ABE_EXT_H_ */
