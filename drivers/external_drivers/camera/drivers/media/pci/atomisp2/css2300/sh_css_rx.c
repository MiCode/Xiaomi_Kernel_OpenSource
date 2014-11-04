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

#define __INLINE_INPUT_SYSTEM__
#include "input_system.h"

#include "sh_css.h"
#include "sh_css_rx.h"
#include "sh_css_internal.h"

void
sh_css_rx_enable_all_interrupts(void)
{
	hrt_data	bits = receiver_port_reg_load(RX0_ID,
		MIPI_PORT1_ID, _HRT_CSS_RECEIVER_IRQ_ENABLE_REG_IDX);

	bits |= (1U << _HRT_CSS_RECEIVER_IRQ_OVERRUN_BIT) |
#if defined(HAS_RX_VERSION_2)
		(1U << _HRT_CSS_RECEIVER_IRQ_INIT_TIMEOUT_BIT) |
#endif
		(1U << _HRT_CSS_RECEIVER_IRQ_SLEEP_MODE_ENTRY_BIT) |
		(1U << _HRT_CSS_RECEIVER_IRQ_SLEEP_MODE_EXIT_BIT) |
		(1U << _HRT_CSS_RECEIVER_IRQ_ERR_SOT_HS_BIT) |
		(1U << _HRT_CSS_RECEIVER_IRQ_ERR_SOT_SYNC_HS_BIT) |
		(1U << _HRT_CSS_RECEIVER_IRQ_ERR_CONTROL_BIT) |
		(1U << _HRT_CSS_RECEIVER_IRQ_ERR_ECC_DOUBLE_BIT) |
		(1U << _HRT_CSS_RECEIVER_IRQ_ERR_ECC_CORRECTED_BIT) |
/*		(1U << _HRT_CSS_RECEIVER_IRQ_ERR_ECC_NO_CORRECTION_BIT) | */
		(1U << _HRT_CSS_RECEIVER_IRQ_ERR_CRC_BIT) |
		(1U << _HRT_CSS_RECEIVER_IRQ_ERR_ID_BIT) |
		(1U << _HRT_CSS_RECEIVER_IRQ_ERR_FRAME_SYNC_BIT) |
		(1U << _HRT_CSS_RECEIVER_IRQ_ERR_FRAME_DATA_BIT) |
		(1U << _HRT_CSS_RECEIVER_IRQ_DATA_TIMEOUT_BIT) |
		(1U << _HRT_CSS_RECEIVER_IRQ_ERR_ESCAPE_BIT);
/*		(1U << _HRT_CSS_RECEIVER_IRQ_ERR_LINE_SYNC_BIT); */

	receiver_port_reg_store(RX0_ID,
		MIPI_PORT1_ID, _HRT_CSS_RECEIVER_IRQ_ENABLE_REG_IDX, bits);
return;
}

unsigned int sh_css_rx_get_interrupt_reg(void)
{
return receiver_port_reg_load(RX0_ID,
	MIPI_PORT1_ID, _HRT_CSS_RECEIVER_IRQ_STATUS_REG_IDX);
}

void
sh_css_rx_get_interrupt_info(unsigned int *irq_infos)
{
	unsigned long	infos = 0;

	hrt_data	bits = receiver_port_reg_load(RX0_ID,
		MIPI_PORT1_ID, _HRT_CSS_RECEIVER_IRQ_STATUS_REG_IDX);

	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_OVERRUN_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_BUFFER_OVERRUN;
#if defined(HAS_RX_VERSION_2)
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_INIT_TIMEOUT_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_INIT_TIMEOUT;
#endif
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_SLEEP_MODE_ENTRY_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_ENTER_SLEEP_MODE;
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_SLEEP_MODE_EXIT_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_EXIT_SLEEP_MODE;
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_ERR_ECC_CORRECTED_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_ECC_CORRECTED;
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_ERR_SOT_HS_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_ERR_SOT;
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_ERR_SOT_SYNC_HS_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_ERR_SOT_SYNC;
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_ERR_CONTROL_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_ERR_CONTROL;
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_ERR_ECC_DOUBLE_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_ERR_ECC_DOUBLE;
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_ERR_CRC_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_ERR_CRC;
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_ERR_ID_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_ERR_UNKNOWN_ID;
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_ERR_FRAME_SYNC_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_ERR_FRAME_SYNC;
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_ERR_FRAME_DATA_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_ERR_FRAME_DATA;
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_DATA_TIMEOUT_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_ERR_DATA_TIMEOUT;
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_ERR_ESCAPE_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_ERR_UNKNOWN_ESC;
	if (bits & (1U << _HRT_CSS_RECEIVER_IRQ_ERR_LINE_SYNC_BIT))
		infos |= SH_CSS_RX_IRQ_INFO_ERR_LINE_SYNC;

	*irq_infos = infos;
}

void
sh_css_rx_clear_interrupt_info(unsigned int irq_infos)
{
	hrt_data	bits = receiver_port_reg_load(RX0_ID,
		MIPI_PORT1_ID, _HRT_CSS_RECEIVER_IRQ_ENABLE_REG_IDX);

/* MW: Why do we remap the receiver bitmap */
	if (irq_infos & SH_CSS_RX_IRQ_INFO_BUFFER_OVERRUN)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_OVERRUN_BIT;
#if defined(HAS_RX_VERSION_2)
	if (irq_infos & SH_CSS_RX_IRQ_INFO_INIT_TIMEOUT)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_INIT_TIMEOUT_BIT;
#endif
	if (irq_infos & SH_CSS_RX_IRQ_INFO_ENTER_SLEEP_MODE)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_SLEEP_MODE_ENTRY_BIT;
	if (irq_infos & SH_CSS_RX_IRQ_INFO_EXIT_SLEEP_MODE)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_SLEEP_MODE_EXIT_BIT;
	if (irq_infos & SH_CSS_RX_IRQ_INFO_ECC_CORRECTED)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_ERR_ECC_CORRECTED_BIT;
	if (irq_infos & SH_CSS_RX_IRQ_INFO_ERR_SOT)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_ERR_SOT_HS_BIT;
	if (irq_infos & SH_CSS_RX_IRQ_INFO_ERR_SOT_SYNC)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_ERR_SOT_SYNC_HS_BIT;
	if (irq_infos & SH_CSS_RX_IRQ_INFO_ERR_CONTROL)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_ERR_CONTROL_BIT;
	if (irq_infos & SH_CSS_RX_IRQ_INFO_ERR_ECC_DOUBLE)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_ERR_ECC_DOUBLE_BIT;
	if (irq_infos & SH_CSS_RX_IRQ_INFO_ERR_CRC)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_ERR_CRC_BIT;
	if (irq_infos & SH_CSS_RX_IRQ_INFO_ERR_UNKNOWN_ID)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_ERR_ID_BIT;
	if (irq_infos & SH_CSS_RX_IRQ_INFO_ERR_FRAME_SYNC)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_ERR_FRAME_SYNC_BIT;
	if (irq_infos & SH_CSS_RX_IRQ_INFO_ERR_FRAME_DATA)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_ERR_FRAME_DATA_BIT;
	if (irq_infos & SH_CSS_RX_IRQ_INFO_ERR_DATA_TIMEOUT)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_DATA_TIMEOUT_BIT;
	if (irq_infos & SH_CSS_RX_IRQ_INFO_ERR_UNKNOWN_ESC)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_ERR_ESCAPE_BIT;
	if (irq_infos & SH_CSS_RX_IRQ_INFO_ERR_LINE_SYNC)
		bits |= 1U << _HRT_CSS_RECEIVER_IRQ_ERR_LINE_SYNC_BIT;

	receiver_port_reg_store(RX0_ID,
		MIPI_PORT1_ID, _HRT_CSS_RECEIVER_IRQ_ENABLE_REG_IDX, bits);
return;
}

enum sh_css_err sh_css_input_format_type(
	enum sh_css_input_format input_format,
	mipi_predictor_t compression,
	unsigned int *fmt_type)
{
/*
 * Custom (user defined) modes. Used for compressed
 * MIPI transfers
 *
 * Checkpatch thinks the indent before "if" is suspect
 * I think the only suspect part is the missing "else"
 * because of the return.
 */
	if (compression != MIPI_PREDICTOR_NONE) {
		switch (input_format) {
		case SH_CSS_INPUT_FORMAT_RAW_6:
			*fmt_type = 6;
			break;
		case SH_CSS_INPUT_FORMAT_RAW_7:
			*fmt_type = 7;
			break;
		case SH_CSS_INPUT_FORMAT_RAW_8:
			*fmt_type = 8;
			break;
		case SH_CSS_INPUT_FORMAT_RAW_10:
			*fmt_type = 10;
			break;
		case SH_CSS_INPUT_FORMAT_RAW_12:
			*fmt_type = 12;
			break;
		case SH_CSS_INPUT_FORMAT_RAW_14:
			*fmt_type = 14;
			break;
		case SH_CSS_INPUT_FORMAT_RAW_16:
			*fmt_type = 16;
			break;
		default:
			return sh_css_err_internal_error;
		}
		return sh_css_success;
	}
/*
 * This mapping comes from the Arasan CSS function spec
 * (CSS_func_spec1.08_ahb_sep29_08.pdf).
 *
 * MW: For some reason the mapping is not 1-to-1
 */
	switch (input_format) {
	case SH_CSS_INPUT_FORMAT_RGB_888:
		*fmt_type = MIPI_FORMAT_RGB888;
		break;
	case SH_CSS_INPUT_FORMAT_RGB_555:
		*fmt_type = MIPI_FORMAT_RGB555;
		break;
	case SH_CSS_INPUT_FORMAT_RGB_444:
		*fmt_type = MIPI_FORMAT_RGB444;
		break;
	case SH_CSS_INPUT_FORMAT_RGB_565:
		*fmt_type = MIPI_FORMAT_RGB565;
		break;
	case SH_CSS_INPUT_FORMAT_RGB_666:
		*fmt_type = MIPI_FORMAT_RGB666;
		break;
	case SH_CSS_INPUT_FORMAT_RAW_8:
		*fmt_type = MIPI_FORMAT_RAW8;
		break;
	case SH_CSS_INPUT_FORMAT_RAW_10:
		*fmt_type = MIPI_FORMAT_RAW10;
		break;
	case SH_CSS_INPUT_FORMAT_RAW_6:
		*fmt_type = MIPI_FORMAT_RAW6;
		break;
	case SH_CSS_INPUT_FORMAT_RAW_7:
		*fmt_type = MIPI_FORMAT_RAW7;
		break;
	case SH_CSS_INPUT_FORMAT_RAW_12:
		*fmt_type = MIPI_FORMAT_RAW12;
		break;
	case SH_CSS_INPUT_FORMAT_RAW_14:
		*fmt_type = MIPI_FORMAT_RAW14;
		break;
	case SH_CSS_INPUT_FORMAT_YUV420_8:
		*fmt_type = MIPI_FORMAT_YUV420_8;
		break;
	case SH_CSS_INPUT_FORMAT_YUV420_10:
		*fmt_type = MIPI_FORMAT_YUV420_10;
		break;
	case SH_CSS_INPUT_FORMAT_YUV422_8:
		*fmt_type = MIPI_FORMAT_YUV422_8;
		break;
	case SH_CSS_INPUT_FORMAT_YUV422_10:
		*fmt_type = MIPI_FORMAT_YUV422_10;
		break;
	case SH_CSS_INPUT_FORMAT_BINARY_8:
		*fmt_type = MIPI_FORMAT_BINARY_8;
		break;
	case SH_CSS_INPUT_FORMAT_YUV420_8_LEGACY:
		*fmt_type = MIPI_FORMAT_YUV420_8_LEGACY;
		break;
	case SH_CSS_INPUT_FORMAT_RAW_16:
		/* This is not specified by Arasan, so we use
		 * 17 for now.
		 */
		*fmt_type = MIPI_FORMAT_RAW16;
		break;
#if defined(HAS_RX_VERSION_2)
	default:
		if (input_format > (enum sh_css_input_format)N_MIPI_FORMAT)
			return sh_css_err_internal_error;
		*fmt_type = input_format;
		break;
#else
	default:
		return sh_css_err_internal_error;
#endif
	}
return sh_css_success;
}

#if defined(HAS_RX_VERSION_1)

/* This is a device function, shouldn't be here */
static void sh_css_rx_set_bits(
	const mipi_port_ID_t	port,
	const unsigned int		reg,
	const unsigned int		lsb,
	const unsigned int		bits,
	const unsigned int		val)
{
	hrt_data	data = receiver_port_reg_load(RX0_ID, port, reg);
/* prevent writing out of range */
	hrt_data	tmp = val & ((1U << bits) - 1);
/* shift into place */
	data |= (tmp << lsb);
	receiver_port_reg_store(RX0_ID, port, reg, data);
return;
}

static void sh_css_rx_set_num_lanes(
	const mipi_port_ID_t	port,
	const unsigned int		lanes)
{
	sh_css_rx_set_bits(port,
		_HRT_CSS_RECEIVER_FUNC_PROG_REG_IDX,
		_HRT_CSS_RECEIVER_AHB_CSI2_NUM_DATA_LANES_IDX,
		_HRT_CSS_RECEIVER_AHB_CSI2_NUM_DATA_LANES_BITS,
		lanes);
return;
}

static void sh_css_rx_set_timeout(
	const mipi_port_ID_t	port,
	const unsigned int		timeout)
{
	sh_css_rx_set_bits(port,
		_HRT_CSS_RECEIVER_FUNC_PROG_REG_IDX,
		_HRT_CSS_RECEIVER_DATA_TIMEOUT_IDX,
		_HRT_CSS_RECEIVER_DATA_TIMEOUT_BITS,
		timeout);
return;
}

static void sh_css_rx_set_compression(
	const mipi_port_ID_t				port,
	const mipi_predictor_t				comp)
{
	unsigned int reg = _HRT_CSS_RECEIVER_COMP_PREDICT_REG_IDX;

assert(comp < N_MIPI_PREDICTOR_TYPES);

	receiver_port_reg_store(RX0_ID, port, reg, comp);
return;
}

static void sh_css_rx_set_uncomp_size(
	const mipi_port_ID_t	port,
	const unsigned int		size)
{
	sh_css_rx_set_bits(port,
		_HRT_CSS_RECEIVER_AHB_COMP_FORMAT_REG_IDX,
		_HRT_CSS_RECEIVER_AHB_COMP_NUM_BITS_IDX,
		_HRT_CSS_RECEIVER_AHB_COMP_NUM_BITS_BITS,
		size);
return;
}

static void sh_css_rx_set_comp_size(
	const mipi_port_ID_t	port,
	const unsigned int		size)
{
	sh_css_rx_set_bits(port,
		_HRT_CSS_RECEIVER_AHB_COMP_FORMAT_REG_IDX,
		_HRT_CSS_RECEIVER_AHB_COMP_RAW_BITS_IDX,
		_HRT_CSS_RECEIVER_AHB_COMP_RAW_BITS_BITS,
		size);
return;
}
#endif /* defined(HAS_RX_VERSION_1) */

void sh_css_rx_configure(
	const rx_cfg_t		*config)
{
	mipi_port_ID_t	port = config->port;

/* turn off all ports just in case */
	sh_css_rx_disable();

#if defined(HAS_RX_VERSION_2)
	if (MIPI_PORT_LANES[config->mode][port] != MIPI_0LANE_CFG) {
		receiver_port_reg_store(RX0_ID, port,
			_HRT_CSS_RECEIVER_FUNC_PROG_REG_IDX,
			config->timeout);
		receiver_port_reg_store(RX0_ID, port,
			_HRT_CSS_RECEIVER_2400_INIT_COUNT_REG_IDX,
			config->initcount);
		receiver_port_reg_store(RX0_ID, port,
			_HRT_CSS_RECEIVER_2400_SYNC_COUNT_REG_IDX,
			config->synccount);
		receiver_port_reg_store(RX0_ID, port,
			_HRT_CSS_RECEIVER_2400_RX_COUNT_REG_IDX,
			config->rxcount);
/*
 * MW: A bit of a hack, straight wiring of the capture units,
 * assuming they are linearly enumerated
 */
		input_system_sub_system_reg_store(INPUT_SYSTEM0_ID,
			GPREGS_UNIT0_ID, HIVE_ISYS_GPREG_MULTICAST_A_IDX +
			(unsigned int)port, INPUT_SYSTEM_CSI_BACKEND);
		input_system_sub_system_reg_store(INPUT_SYSTEM0_ID,
			GPREGS_UNIT0_ID, HIVE_ISYS_GPREG_MUX_IDX,
			(input_system_multiplex_t)port);
	}
/*
 * signal input
 *
	receiver_reg_store(RX0_ID,
		_HRT_CSS_RECEIVER_TWO_PIXEL_EN_REG_IDX, config->mode);
 */
	receiver_reg_store(RX0_ID,
		_HRT_CSS_RECEIVER_TWO_PIXEL_EN_REG_IDX, config->is_two_ppc);

/* enable the selected port(s) */
	for (port = (mipi_port_ID_t)0; port < N_MIPI_PORT_ID; port++) {
		if (MIPI_PORT_LANES[config->mode][port] != MIPI_0LANE_CFG)
			receiver_port_reg_store(RX0_ID, port,
				_HRT_CSS_RECEIVER_DEVICE_READY_REG_IDX, true);
	}

#elif defined(HAS_RX_VERSION_1)

/* All settings are per port */
	sh_css_rx_set_timeout(port, config->timeout);
/* configure the selected port */
	sh_css_rx_set_num_lanes(port, config->num_lanes);
	sh_css_rx_set_compression(port, config->comp);
	sh_css_rx_set_uncomp_size(port, config->uncomp_bpp);
	sh_css_rx_set_comp_size(port, config->comp_bpp);

	receiver_port_reg_store(RX0_ID, port,
		_HRT_CSS_RECEIVER_TWO_PIXEL_EN_REG_IDX, config->is_two_ppc);

/* enable the selected port */
	receiver_port_reg_store(RX0_ID, port,
		_HRT_CSS_RECEIVER_DEVICE_READY_REG_IDX, true);
#else
#error "sh_css_rx.c: RX version must be one of {RX_VERSION_1, RX_VERSION_2}"
#endif

return;
}

void sh_css_rx_disable(void)
{
	mipi_port_ID_t	port;
	for (port = (mipi_port_ID_t)0; port < N_MIPI_PORT_ID; port++) {
		receiver_port_reg_store(RX0_ID, port,
			_HRT_CSS_RECEIVER_DEVICE_READY_REG_IDX, false);
	}
return;
}

