
#include "fifo_monitor.h"

#include <stdbool.h>

#include "device_access.h"

#include <hrt/bits.h>

#define __INLINE_GP_DEVICE__
#include "gp_device.h"

#include "assert_support.h"

#ifndef __INLINE_FIFO_MONITOR__
#define STORAGE_CLASS_FIFO_MONITOR_DATA static const

STORAGE_CLASS_FIFO_MONITOR_DATA unsigned int FIFO_SWITCH_ADDR[N_FIFO_SWITCH];

#include "fifo_monitor_private.h"
#else
#define STORAGE_CLASS_FIFO_MONITOR_DATA const

STORAGE_CLASS_FIFO_MONITOR_DATA unsigned int FIFO_SWITCH_ADDR[N_FIFO_SWITCH];
#endif /* __INLINE_FIFO_MONITOR__ */

STORAGE_CLASS_FIFO_MONITOR_DATA unsigned int FIFO_SWITCH_ADDR[N_FIFO_SWITCH] = {
	_REG_GP_SWITCH_IF_ADDR,
	_REG_GP_SWITCH_DMA_ADDR,
	_REG_GP_SWITCH_GDC_ADDR};

static bool fifo_monitor_status_valid(
/* STORAGE_CLASS_INLINE bool fifo_monitor_status_valid ( */
	const fifo_monitor_ID_t		ID,
	const unsigned int			reg,
	const unsigned int			port_id);

static bool fifo_monitor_status_accept(
/* STORAGE_CLASS_INLINE bool fifo_monitor_status_accept( */
	const fifo_monitor_ID_t		ID,
	const unsigned int			reg,
	const unsigned int			port_id);


void fifo_channel_get_state(
	const fifo_monitor_ID_t		ID,
	const fifo_channel_t		channel_id,
	fifo_channel_state_t		*state)
{
assert(state != NULL);
assert(channel_id < N_FIFO_CHANNEL);
	if (state == NULL)
		return;

	switch (channel_id) {
	case FIFO_CHANNEL_ISP0_TO_SP0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_SND_SP);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_SND_SP);
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_ISP);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_ISP);
		break;
	case FIFO_CHANNEL_SP0_TO_ISP0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_ISP);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_ISP);
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_RCV_SP);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_RCV_SP);
		break;
	case FIFO_CHANNEL_ISP0_TO_IF0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_SND_PIF_A); /* even */
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_SND_PIF_A);
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_PIF_A); /* odd */
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_PIF_A);
		break;
	case FIFO_CHANNEL_IF0_TO_ISP0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_PIF_A); /* even */
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_PIF_A);
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_RCV_PIF_A); /* odd */
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_RCV_PIF_A);
		break;
	case FIFO_CHANNEL_ISP0_TO_IF1:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_SND_PIF_B);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_SND_PIF_B);
		state->fifo_valid  = false; /* no monitor connected */
		state->sink_accept = false; /* no monitor connected */
		break;
	case FIFO_CHANNEL_IF1_TO_ISP0:
		state->src_valid   = false; /* no monitor connected */
		state->fifo_accept = false; /* no monitor connected */
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_RCV_PIF_B);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_RCV_PIF_B);
		break;
	case FIFO_CHANNEL_ISP0_TO_DMA0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_SND_DMA);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_SND_DMA);
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_DMA);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_DMA);
		break;
	case FIFO_CHANNEL_DMA0_TO_ISP0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_DMA);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_DMA);
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_RCV_DMA);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_RCV_DMA);
		break;
	case FIFO_CHANNEL_ISP0_TO_GDC0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_SND_GDC);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_SND_GDC);
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_GDC);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_GDC);
		break;
	case FIFO_CHANNEL_GDC0_TO_ISP0:
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_GDC);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_GDC);
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_RCV_GDC);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_RCV_GDC);
		break;
	case FIFO_CHANNEL_ISP0_TO_HOST0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_SND_GPD);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_SND_GPD);
		{
		hrt_data	value = device_load_uint32(0x10200214UL);
		state->fifo_valid  = !_hrt_get_bit(value, 0);
		state->sink_accept = false; /* no monitor connected */
		}
		break;
	case FIFO_CHANNEL_HOST0_TO_ISP0:
		{
		hrt_data	value = device_load_uint32(0x1020021CUL);
		state->fifo_valid  = false; /* no monitor connected */
		state->sink_accept = !_hrt_get_bit(value, 0);
		}
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_RCV_GPD);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			ISP_STR_MON_PORT_RCV_GPD);
		break;
	case FIFO_CHANNEL_SP0_TO_IF0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_PIF_A);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_PIF_A);
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_PIF_A);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_PIF_A);
		break;
	case FIFO_CHANNEL_IF0_TO_SP0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_PIF_A);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_PIF_A);
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_PIF_A);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_PIF_A);
		break;
	case FIFO_CHANNEL_SP0_TO_IF1:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_PIF_B);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_PIF_B);
		state->fifo_valid  = false; /* no monitor connected */
		state->sink_accept = false; /* no monitor connected */
		break;
	case FIFO_CHANNEL_IF1_TO_SP0:
		state->src_valid   = false; /* no monitor connected */
		state->fifo_accept = false; /* no monitor connected */
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_PIF_B);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_PIF_B);
		break;
	case FIFO_CHANNEL_SP0_TO_IF2:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_SIF);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_SIF);
		state->fifo_valid  = false; /* no monitor connected */
		state->sink_accept = false; /* no monitor connected */
		break;
	case FIFO_CHANNEL_IF2_TO_SP0:
		state->src_valid   = false; /* no monitor connected */
		state->fifo_accept = false; /* no monitor connected */
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_SIF);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_ISP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_SIF);
		break;
	case FIFO_CHANNEL_SP0_TO_DMA0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_DMA);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_DMA);
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_DMA);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_DMA);
		break;
	case FIFO_CHANNEL_DMA0_TO_SP0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_DMA);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_DMA);
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_DMA);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_DMA);
		break;
	case FIFO_CHANNEL_SP0_TO_GDC0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_GDC);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_GDC);
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_GDC);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_GDC);
		break;
	case FIFO_CHANNEL_GDC0_TO_SP0:
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_GDC);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_GDC);
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_GDC);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_GDC);
		break;
	case FIFO_CHANNEL_SP0_TO_HOST0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_GPD);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_GPD);
		{
		hrt_data	value = device_load_uint32(0x10200210UL);
		state->fifo_valid  = !_hrt_get_bit(value, 0);
		state->sink_accept = false; /* no monitor connected */
		}
		break;
	case FIFO_CHANNEL_HOST0_TO_SP0:
		{
		hrt_data	value = device_load_uint32(0x10200218UL);
		state->fifo_valid  = false; /* no monitor connected */
		state->sink_accept = !_hrt_get_bit(value, 0);
		}
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_GPD);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_GPD);
		break;
	case FIFO_CHANNEL_SP0_TO_STREAM2MEM0:
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_MC);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_SND_MC);
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_MC);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_RCV_MC);
		break;
	case FIFO_CHANNEL_STREAM2MEM0_TO_SP0:
		state->fifo_valid  = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_MC);
		state->sink_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_MOD_STREAM_STAT_IDX,
			MOD_STR_MON_PORT_SND_MC);
		state->src_valid   = fifo_monitor_status_valid(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_MC);
		state->fifo_accept = fifo_monitor_status_accept(ID,
			HIVE_GP_REGS_SP_STREAM_STAT_IDX,
			SP_STR_MON_PORT_RCV_MC);
		break;
	default:
assert(0);
		break;
	}

return;
}

void fifo_switch_get_state(
	const fifo_monitor_ID_t		ID,
	const fifo_switch_t			switch_id,
	fifo_switch_state_t			*state)
{
	hrt_data	data = (hrt_data)-1;

assert(state != NULL);
assert(switch_id < N_FIFO_SWITCH);
assert(ID == FIFO_MONITOR0_ID);
	if (state == NULL)
		return;

(void)ID;

	data = gp_device_reg_load(GP_DEVICE0_ID, FIFO_SWITCH_ADDR[switch_id]);

	state->is_none = (data == HIVE_ISP_CSS_STREAM_SWITCH_NONE);
	state->is_sp = (data == HIVE_ISP_CSS_STREAM_SWITCH_SP);
	state->is_isp = (data == HIVE_ISP_CSS_STREAM_SWITCH_ISP);

return;
}

void fifo_monitor_get_state(
	const fifo_monitor_ID_t		ID,
	fifo_monitor_state_t		*state)
{
	fifo_channel_t	ch_id;
	fifo_switch_t	sw_id;

assert(state != NULL);
assert(ID < N_FIFO_MONITOR_ID);

	for (ch_id = 0; ch_id < N_FIFO_CHANNEL; ch_id++) /* { */
		fifo_channel_get_state(ID, ch_id,
			&(state->fifo_channels[ch_id]));
	/* } */

	for (sw_id = 0; sw_id < N_FIFO_SWITCH; sw_id++) /* { */
		fifo_switch_get_state(ID, sw_id,
			&(state->fifo_switches[sw_id]));
	/* } */
return;
}

static bool fifo_monitor_status_valid(
/* STORAGE_CLASS_INLINE bool fifo_monitor_status_valid ( */
	const fifo_monitor_ID_t		ID,
	const unsigned int			reg,
	const unsigned int			port_id)
{
hrt_data	data = fifo_monitor_reg_load(ID, reg);
return (data >> (((port_id * 2) + _hive_str_mon_valid_offset))) & 0x1;
}

static bool fifo_monitor_status_accept(
/* STORAGE_CLASS_INLINE bool fifo_monitor_status_accept( */
	const fifo_monitor_ID_t		ID,
	const unsigned int			reg,
	const unsigned int			port_id)
{
hrt_data	data = fifo_monitor_reg_load(ID, reg);
return (data >> (((port_id * 2) + _hive_str_mon_accept_offset))) & 0x1;
}
