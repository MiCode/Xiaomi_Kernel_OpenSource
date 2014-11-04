
#include <stddef.h>		/* NULL */

#include "input_formatter.h"

#include "assert_support.h"

#ifndef __INLINE_INPUT_FORMATTER__
#include "input_formatter_private.h"
#endif /* __INLINE_INPUT_FORMATTER__ */

void input_formatter_rst(
	const input_formatter_ID_t		ID)
{
assert(ID < N_INPUT_FORMATTER_ID);
	input_formatter_reg_store(ID,
		HIVE_IF_RESET_ADDRESS, 1);
return;
}

unsigned int input_formatter_get_alignment(
	const input_formatter_ID_t		ID)
{
assert(ID < N_INPUT_FORMATTER_ID);

return input_formatter_alignment[ID];
}

void input_formatter_set_fifo_blocking_mode(
	const input_formatter_ID_t		ID,
	const bool						enable)
{
assert(ID < N_INPUT_FORMATTER_ID);
	input_formatter_reg_store(ID,
		 HIVE_IF_BLOCK_FIFO_NO_REQ_ADDRESS, enable);
return;
}

void input_formatter_get_state(
	const input_formatter_ID_t		ID,
	input_formatter_state_t			*state)
{
assert(ID < N_INPUT_FORMATTER_ID);
assert(state != NULL);
	if (state == NULL)
		return;

	state->reset = input_formatter_reg_load(ID,
		HIVE_IF_RESET_ADDRESS);
	state->start_line = input_formatter_reg_load(ID,
		HIVE_IF_START_LINE_ADDRESS);
	state->start_column = input_formatter_reg_load(ID,
		HIVE_IF_START_COLUMN_ADDRESS);
	state->cropped_height = input_formatter_reg_load(ID,
		HIVE_IF_CROPPED_HEIGHT_ADDRESS);
	state->cropped_width = input_formatter_reg_load(ID,
		HIVE_IF_CROPPED_WIDTH_ADDRESS);
	state->ver_decimation = input_formatter_reg_load(ID,
		HIVE_IF_VERTICAL_DECIMATION_ADDRESS);
	state->hor_decimation = input_formatter_reg_load(ID,
		HIVE_IF_HORIZONTAL_DECIMATION_ADDRESS);
	state->deinterleaving = input_formatter_reg_load(ID,
		HIVE_IF_H_DEINTERLEAVING_ADDRESS);
	state->left_padding = input_formatter_reg_load(ID,
		HIVE_IF_LEFTPADDING_WIDTH_ADDRESS);
	state->eol_offset = input_formatter_reg_load(ID,
		HIVE_IF_END_OF_LINE_OFFSET_ADDRESS);
	state->vmem_start_address = input_formatter_reg_load(ID,
		HIVE_IF_VMEM_START_ADDRESS_ADDRESS);
	state->vmem_end_address = input_formatter_reg_load(ID,
		HIVE_IF_VMEM_END_ADDRESS_ADDRESS);
	state->vmem_increment = input_formatter_reg_load(ID,
		HIVE_IF_VMEM_INCREMENT_ADDRESS);
	state->is_yuv420 = input_formatter_reg_load(ID,
		HIVE_IF_YUV_420_FORMAT_ADDRESS);
	state->vsync_active_low = input_formatter_reg_load(ID,
		HIVE_IF_VSYNCK_ACTIVE_LOW_ADDRESS);
	state->hsync_active_low = input_formatter_reg_load(ID,
		HIVE_IF_HSYNCK_ACTIVE_LOW_ADDRESS);
	state->allow_fifo_overflow = input_formatter_reg_load(ID,
		HIVE_IF_ALLOW_FIFO_OVERFLOW_ADDRESS);
	state->fsm_sync_status = input_formatter_reg_load(ID,
		HIVE_IF_FSM_SYNC_STATUS);
	state->fsm_sync_counter = input_formatter_reg_load(ID,
		HIVE_IF_FSM_SYNC_COUNTER);
	state->fsm_crop_status = input_formatter_reg_load(ID,
		HIVE_IF_FSM_CROP_STATUS);
	state->fsm_crop_line_counter = input_formatter_reg_load(ID,
		HIVE_IF_FSM_CROP_LINE_COUNTER);
	state->fsm_crop_pixel_counter = input_formatter_reg_load(ID,
		HIVE_IF_FSM_CROP_PIXEL_COUNTER);
	state->fsm_deinterleaving_index = input_formatter_reg_load(ID,
		HIVE_IF_FSM_DEINTERLEAVING_IDX);
	state->fsm_dec_h_counter = input_formatter_reg_load(ID,
		HIVE_IF_FSM_DECIMATION_H_COUNTER);
	state->fsm_dec_v_counter = input_formatter_reg_load(ID,
		HIVE_IF_FSM_DECIMATION_V_COUNTER);
	state->fsm_dec_block_v_counter = input_formatter_reg_load(ID,
		HIVE_IF_FSM_DECIMATION_BLOCK_V_COUNTER);
	state->fsm_padding_status = input_formatter_reg_load(ID,
		HIVE_IF_FSM_PADDING_STATUS);
	state->fsm_padding_elem_counter = input_formatter_reg_load(ID,
		HIVE_IF_FSM_PADDING_ELEMENT_COUNTER);
	state->fsm_vector_support_error = input_formatter_reg_load(ID,
		HIVE_IF_FSM_VECTOR_SUPPORT_ERROR);
	state->fsm_vector_buffer_full = input_formatter_reg_load(ID,
		HIVE_IF_FSM_VECTOR_SUPPORT_BUFF_FULL);
	state->vector_support = input_formatter_reg_load(ID,
		HIVE_IF_FSM_VECTOR_SUPPORT);
	state->sensor_data_lost = input_formatter_reg_load(ID,
		HIVE_IF_FIFO_SENSOR_STATUS);

return;
}
