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

#ifndef _ABE_AESS_H_
#define _ABE_AESS_H_

#define AESS_REVISION			0x00
#define AESS_MCU_IRQSTATUS		0x28
#define AESS_MCU_IRQENABLE_SET		0x3C
#define AESS_MCU_IRQENABLE_CLR		0x40
#define AESS_DMAENABLE_SET		0x60
#define AESS_DMAENABLE_CLR		0x64
#define EVENT_GENERATOR_COUNTER		0x68
#define EVENT_GENERATOR_START		0x6C
#define EVENT_SOURCE_SELECTION		0x70
#define AUDIO_ENGINE_SCHEDULER		0x74

/*
 * AESS_MCU_IRQSTATUS bit field
 */
#define INT_CLEAR			0x01

/*
 * AESS_MCU_IRQENABLE_SET bit field
 */
#define INT_SET				0x01

/*
 * AESS_MCU_IRQENABLE_CLR bit field
 */
#define INT_CLR				0x01

/*
 * AESS_DMAENABLE_SET bit fields
 */
#define DMA_ENABLE_ALL		0xFF

/*
 * AESS_DMAENABLE_CLR bit fields
 */
#define DMA_DISABLE_ALL		0xFF

/*
 * EVENT_GENERATOR_COUNTER COUNTER_VALUE bit field
 */
/* PLL output/desired sampling rate = (32768 * 6000)/96000 */
#define EVENT_GENERATOR_COUNTER_DEFAULT	(2048-1)
/* PLL output/desired sampling rate = (32768 * 6000)/88200 */
#define EVENT_GENERATOR_COUNTER_44100	(2228-1)


int omap_abe_start_event_generator(struct omap_abe *abe);
int omap_abe_stop_event_generator(struct omap_abe *abe);
int omap_abe_write_event_generator(struct omap_abe *abe, u32 e);

void omap_abe_hw_configuration(struct omap_abe *abe);

#endif/* _ABE_AESS_H_ */
