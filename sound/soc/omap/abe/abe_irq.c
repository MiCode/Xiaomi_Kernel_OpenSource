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

#include "abe_legacy.h"

extern u32 abe_irq_pingpong_player_id;

/*
 *  initialize the default values for call-backs to subroutines
 *      - FIFO IRQ call-backs for sequenced tasks
 *      - FIFO IRQ call-backs for audio player/recorders (ping-pong protocols)
 *      - Remote debugger interface
 *      - Error monitoring
 *      - Activity Tracing
 */
/**
 * abe_irq_ping_pong
 *
 * Call the respective subroutine depending on the IRQ FIFO content:
 * APS interrupts : IRQ_FIFO[31:28] = IRQtag_APS,
 *	IRQ_FIFO[27:16] = APS_IRQs, IRQ_FIFO[15:0] = loopCounter
 * SEQ interrupts : IRQ_FIFO[31:28] = IRQtag_COUNT,
 *	IRQ_FIFO[27:16] = Count_IRQs, IRQ_FIFO[15:0] = loopCounter
 * Ping-Pong Interrupts : IRQ_FIFO[31:28] = IRQtag_PP,
 *	IRQ_FIFO[27:16] = PP_MCU_IRQ, IRQ_FIFO[15:0] = loopCounter
 */
void abe_irq_ping_pong(void)
{
	abe_call_subroutine(abe_irq_pingpong_player_id, NOPARAMETER,
			    NOPARAMETER, NOPARAMETER, NOPARAMETER);
}
/**
 * abe_irq_check_for_sequences
* @i: sequence ID
 *
 * check the active sequence list
 *
 */
void abe_irq_check_for_sequences(u32 i)
{
}
/**
 * abe_irq_aps
 *
 * call the application subroutines that updates
 * the acoustics protection filters
 */
void abe_irq_aps(u32 aps_info)
{
	abe_call_subroutine(abe_irq_aps_adaptation_id, NOPARAMETER, NOPARAMETER,
			    NOPARAMETER, NOPARAMETER);
}
