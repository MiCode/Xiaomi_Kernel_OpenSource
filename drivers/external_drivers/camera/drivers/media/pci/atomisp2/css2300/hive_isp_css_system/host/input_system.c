
#include "stddef.h"		/* NULL */
#include "stdint.h"

#include "input_system.h"

#include "assert_support.h"

#ifndef __INLINE_INPUT_SYSTEM__
#include "input_system_private.h"
#endif /* __INLINE_INPUT_SYSTEM__ */

static uint8_t	mipi_compressed_bits[N_MIPI_COMPRESSOR_METHODS] = {
	0, 6, 7, 8, 6, 7, 8};

static uint8_t	mipi_uncompressed_bits[N_MIPI_COMPRESSOR_METHODS] = {
	0, 10, 10, 10, 12, 12, 12};

/* STORAGE_CLASS_INLINE void mipi_port_get_state( */
static void mipi_port_get_state(
	const rx_ID_t					ID,
	const mipi_port_ID_t			port_ID,
	mipi_port_state_t				*state);

void input_system_get_state(
	const input_system_ID_t			ID,
	input_system_state_t			*state)
{
assert(ID < N_INPUT_SYSTEM_ID);
assert(state != NULL);

	state->ch_id_fmt_type = input_system_sub_system_reg_load(ID,
		UNIT0_ID,
		_REG_GP_CH_ID_FMT_TYPE_IDX);

return;
}

void receiver_get_state(
	const rx_ID_t				ID,
	receiver_state_t			*state)
{
	mipi_port_ID_t	port_id;

assert(ID < N_RX_ID);
assert(state != NULL);

	for (port_id = (mipi_port_ID_t)0; port_id < N_MIPI_PORT_ID; port_id++) {
		mipi_port_get_state(ID, port_id,
			&(state->mipi_port_state[port_id]));
	}

return;
}

bool is_mipi_format_yuv420(
	const mipi_format_t			mipi_format)
{
	bool	is_yuv420 = (
		(mipi_format == MIPI_FORMAT_YUV420_8) ||
		(mipi_format == MIPI_FORMAT_YUV420_10));
/* MIPI_FORMAT_YUV420_8_LEGACY is not YUV420 */

return is_yuv420;
}

void receiver_set_compression(
	const rx_ID_t				ID,
	const unsigned int			cfg_ID,
	const mipi_compressor_t		comp,
	const mipi_predictor_t		pred)
{
	const mipi_port_ID_t	port_ID = (mipi_port_ID_t)cfg_ID;
	uint8_t		comp_bits, uncomp_bits;
	hrt_data	reg;

assert(ID < N_RX_ID);
assert(cfg_ID < N_MIPI_COMPRESSOR_CONTEXT);
assert(port_ID < N_MIPI_PORT_ID);
assert(comp < N_MIPI_COMPRESSOR_METHODS);
assert(pred < N_MIPI_PREDICTOR_TYPES);

	comp_bits = mipi_compressed_bits[comp];
	uncomp_bits = mipi_uncompressed_bits[comp];

assert(comp_bits != 0);
assert(uncomp_bits != 0);

	reg = (((hrt_data)uncomp_bits) << 8) | comp_bits;
	receiver_port_reg_store(ID,
		port_ID, _HRT_CSS_RECEIVER_COMP_FORMAT_REG_IDX, reg);
	reg = (hrt_data)pred;
	receiver_port_reg_store(ID,
		port_ID, _HRT_CSS_RECEIVER_COMP_PREDICT_REG_IDX, reg);

return;
}

void receiver_port_enable(
	const rx_ID_t				ID,
	const mipi_port_ID_t		port_ID,
	const bool					cnd)
{
	receiver_port_reg_store(ID, port_ID,
		_HRT_CSS_RECEIVER_DEVICE_READY_REG_IDX, (hrt_data)cnd);
return;
}

bool is_receiver_port_enabled(
	const rx_ID_t				ID,
	const mipi_port_ID_t		port_ID)
{
return (receiver_port_reg_load(ID, port_ID,
	_HRT_CSS_RECEIVER_DEVICE_READY_REG_IDX) != 0);
}

void receiver_irq_enable(
	const rx_ID_t				ID,
	const mipi_port_ID_t		port_ID,
	const rx_irq_info_t			irq_info)
{
	receiver_port_reg_store(ID,
		port_ID, _HRT_CSS_RECEIVER_IRQ_ENABLE_REG_IDX, irq_info);
return;
}

rx_irq_info_t receiver_get_irq_info(
	const rx_ID_t				ID,
	const mipi_port_ID_t		port_ID)
{
return receiver_port_reg_load(ID,
	port_ID, _HRT_CSS_RECEIVER_IRQ_STATUS_REG_IDX);
}

void receiver_irq_clear(
	const rx_ID_t				ID,
	const mipi_port_ID_t		port_ID,
	const rx_irq_info_t			irq_info)
{
	receiver_port_reg_store(ID,
		port_ID, _HRT_CSS_RECEIVER_IRQ_STATUS_REG_IDX, irq_info);
return;
}

/* STORAGE_CLASS_INLINE void mipi_port_get_state( */
static void mipi_port_get_state(
	const rx_ID_t					ID,
	const mipi_port_ID_t			port_ID,
	mipi_port_state_t				*state)
{
assert(ID < N_RX_ID);
assert(port_ID < N_MIPI_PORT_ID);
assert(state != NULL);

	state->device_ready = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_DEVICE_READY_REG_IDX);
	state->irq_status = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_IRQ_ENABLE_REG_IDX);
	state->irq_enable = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_IRQ_STATUS_REG_IDX);
	state->func_prog = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_FUNC_PROG_REG_IDX);
	state->init_count = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_INIT_COUNT_REG_IDX);
	state->comp_format = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_COMP_FORMAT_REG_IDX);
	state->comp_predict = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_COMP_PREDICT_REG_IDX);
	state->fs_to_ls_delay = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_FS_TO_LS_DELAY_REG_IDX);
	state->ls_to_data_delay = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_LS_TO_DATA_DELAY_REG_IDX);
	state->data_to_le_delay = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_DATA_TO_LE_DELAY_REG_IDX);
	state->le_to_fe_delay = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_LE_TO_FE_DELAY_REG_IDX);
	state->fe_to_fs_delay = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_FE_TO_FS_DELAY_REG_IDX);
	state->le_to_fs_delay = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_LE_TO_LS_DELAY_REG_IDX);
	state->is_two_ppc = receiver_port_reg_load(ID,
		port_ID, _HRT_CSS_RECEIVER_TWO_PIXEL_EN_REG_IDX);

return;
}
