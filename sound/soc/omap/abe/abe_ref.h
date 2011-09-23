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

#ifndef _ABE_REF_H_
#define _ABE_REF_H_

#include "abe_api.h"

/*
 * 'ABE_PRO.H' all non-API prototypes for INI, IRQ, SEQ ...
 */
/*
 * HAL EXTERNAL AP
 */
/*
 * HAL INTERNAL AP
 */
void abe_decide_main_port(void);
void abe_reset_all_ports(void);
void abe_reset_all_fifo(void);
void abe_reset_all_sequence(void);
u32 abe_dma_port_iteration(abe_data_format_t *format);
void abe_read_sys_clock(u32 *time);
void abe_enable_atc(u32 id);
void abe_disable_atc(u32 id);
void abe_init_io_tasks(u32 id, abe_data_format_t *format,
			abe_port_protocol_t *prot);
void abe_init_dma_t(u32 id, abe_port_protocol_t *prot);
u32 abe_dma_port_iter_factor(abe_data_format_t *f);
u32 abe_dma_port_copy_subroutine_id(u32 i);
void abe_call_subroutine(u32 idx, u32 p1, u32 p2, u32 p3, u32 p4);
void abe_monitoring(void);
void abe_add_subroutine(u32 *id, abe_subroutine2 f, u32 nparam, u32 *params);
abehal_status abe_read_next_ping_pong_buffer(u32 port, u32 *p, u32 *n);
void abe_irq_ping_pong(void);
void abe_irq_check_for_sequences(u32 seq_info);
void abe_default_irq_pingpong_player(void);
void abe_default_irq_pingpong_player_32bits(void);
void abe_rshifted16_irq_pingpong_player_32bits(void);
void abe_1616_irq_pingpong_player_1616bits(void);
void abe_default_irq_aps_adaptation(void);
void abe_irq_aps(u32 aps_info);
void abe_dbg_error_log(u32 x);
void abe_init_asrc_vx_dl(s32 dppm);
void abe_init_asrc_vx_ul(s32 dppm);
void abe_init_asrc_mm_ext_in(s32 dppm);
void abe_init_asrc_bt_ul(s32 dppm);
void abe_init_asrc_bt_dl(s32 dppm);

void omap_abe_hw_configuration(struct omap_abe *abe);
void omap_abe_gain_offset(struct omap_abe *abe, u32 id, u32 *mixer_offset);
int omap_abe_use_compensated_gain(struct omap_abe *abe, int on_off);

/*
 * HAL INTERNAL DATA
 */
extern const u32 abe_port_priority[LAST_PORT_ID - 1];
extern const u32 abe_firmware_array[ABE_FIRMWARE_MAX_SIZE];
extern const u32 abe_atc_srcid[];
extern const u32 abe_atc_dstid[];
extern const abe_port_t abe_port_init[];
extern const abe_seq_t all_sequence_init[];
extern const abe_router_t abe_router_ul_table_preset
	[NBROUTE_CONFIG][NBROUTE_UL];
extern const abe_sequence_t seq_null;

extern abe_port_t abe_port[];
extern abe_seq_t all_sequence[];
extern abe_router_t abe_router_ul_table[NBROUTE_CONFIG_MAX][NBROUTE_UL];
/* table of new subroutines called in the sequence */
extern abe_subroutine2 abe_all_subsubroutine[MAXNBSUBROUTINE];
/* number of parameters per calls */
extern u32 abe_all_subsubroutine_nparam[MAXNBSUBROUTINE];
extern u32 abe_subroutine_id[MAXNBSUBROUTINE];
extern u32 *abe_all_subroutine_params[MAXNBSUBROUTINE];
extern u32 abe_subroutine_write_pointer;
extern abe_sequence_t abe_all_sequence[MAXNBSEQUENCE];
extern u32 abe_sequence_write_pointer;
/* current number of pending sequences (avoids to look in the table) */
extern u32 abe_nb_pending_sequences;
/* pending sequences due to ressource collision */
extern u32 abe_pending_sequences[MAXNBSEQUENCE];
/* mask of unsharable ressources among other sequences */
extern u32 abe_global_sequence_mask;
/* table of active sequences */
extern abe_seq_t abe_active_sequence[MAXACTIVESEQUENCE][MAXSEQUENCESTEPS];
/* index of the plugged subroutine doing ping-pong cache-flush
	DMEM accesses */
extern u32 abe_irq_aps_adaptation_id;
/* base addresses of the ping pong buffers */
extern u32 abe_base_address_pingpong[MAX_PINGPONG_BUFFERS];
/* size of each ping/pong buffers */
extern u32 abe_size_pingpong;
/* number of ping/pong buffer being used */
extern u32 abe_nb_pingpong;

#endif/* _ABE_REF_H_ */
