#ifndef __DMA_GLOBAL_H_INCLUDED__
#define __DMA_GLOBAL_H_INCLUDED__

#define IS_DMA_VERSION_1

#define HIVE_ISP_NUM_DMA_CONNS			3
#define HIVE_ISP_NUM_DMA_CHANNELS_LOG2	3
#define HIVE_ISP_NUM_DMA_CHANNELS		8
#define N_DMA_CHANNEL_ID	HIVE_ISP_NUM_DMA_CHANNELS

#include "dma_v1_defs.h"

/*
 * Command token bit mappings
 *
 * transfer / config
 *    crun  param id[4] channel id[8] cmd id[4]
 *	| b16 | b15 .. b12 | b11 ... b4 | b3 ... b0 |
 */

typedef unsigned int dma_channel;

typedef enum {
  dma_isp_to_bus_connection = HIVE_DMA_ISP_BUS_CONN,
  dma_isp_to_ddr_connection = HIVE_DMA_ISP_DDR_CONN,
  dma_bus_to_ddr_connection = HIVE_DMA_BUS_DDR_CONN,
} dma_connection;

typedef enum {
  dma_zero_extension = _DMA_ZERO_EXTEND,
  dma_sign_extension = _DMA_SIGN_EXTEND
} dma_extension;


#define DMA_PACK_CHANNEL(ch)             ((ch)   << _DMA_CHANNEL_IDX)
#define DMA_PACK_PARAM(par)              ((par)  << _DMA_PARAM_IDX)
#define DMA_PACK_EXTENSION(ext)          ((ext)  << _DMA_EXTENSION_IDX)
#define DMA_PACK_ELEM_ORDER(ord)         ((ord)  << _DMA_ELEM_ORDER_IDX)
#define DMA_PACK_LEFT_CROPPING(crop)     ((crop) << _DMA_LEFT_CROPPING_IDX)

#define DMA_PACK_CMD_CHANNEL(cmd, ch)    ((cmd) | DMA_PACK_CHANNEL(ch))
#define DMA_PACK_SETUP(conn, ext, order) ((conn) | DMA_PACK_EXTENSION(ext) | DMA_PACK_ELEM_ORDER(order))
#define DMA_PACK_CROP_ELEMS(elems, crop) ((elems) | DMA_PACK_LEFT_CROPPING(crop))

#define hive_dma_snd(dma_id, token) OP_std_snd(dma_id, (unsigned int)(token))

#ifdef __HIVECC
#define hive_dma_move_data(dma_id, read, channel, a_addr, b_addr, a_is_var, b_is_var) \
{ \
  hive_dma_snd(dma_id, DMA_PACK_CMD_CHANNEL(read?_DMA_READ_COMMAND:_DMA_WRITE_COMMAND, channel)); \
  hive_dma_snd(dma_id, a_addr); \
  hive_dma_snd(dma_id, b_addr); \
}
#else
#define hive_dma_move_data(dma_id, read, channel, a_addr, b_addr, a_is_var, b_is_var) \
{ \
  hive_dma_snd(dma_id, DMA_PACK_CMD_CHANNEL(read?_DMA_READ_COMMAND:_DMA_WRITE_COMMAND, channel) | (1 << _DMA_CRUN_IDX)); \
  hive_dma_snd(dma_id, a_addr); \
  hive_dma_snd(dma_id, b_addr); \
  hive_dma_snd(dma_id, a_is_var); \
  hive_dma_snd(dma_id, b_is_var); \
}
#endif

#define hive_dma_move_b2a_data(dma_id, channel, a_addr, b_addr, a_is_var, b_is_var) \
{ \
  hive_dma_move_data (dma_id, true, channel, a_addr, b_addr, a_is_var, b_is_var); \
}

#define hive_dma_move_a2b_data(dma_id, channel, a_addr, b_addr, a_is_var, b_is_var) \
{ \
  hive_dma_move_data (dma_id, false, channel, a_addr, b_addr, a_is_var, b_is_var); \
}

#ifdef __HIVECC
#define hive_dma_set_data(dma_id, channel, address, value, is_var) \
{ \
  hive_dma_snd(dma_id, DMA_PACK_CMD_CHANNEL(_DMA_INIT_COMMAND, channel)); \
  hive_dma_snd(dma_id, address); \
  hive_dma_snd(dma_id, value); \
}
#else
#define hive_dma_set_data(dma_id, channel, address, value, is_var) \
{ \
  hive_dma_snd(dma_id, DMA_PACK_CMD_CHANNEL(_DMA_INIT_COMMAND, channel) | (1 << _DMA_CRUN_IDX)); \
  hive_dma_snd(dma_id, address); \
  hive_dma_snd(dma_id, value); \
  hive_dma_snd(dma_id, is_var); \
}
#endif

#define hive_dma_clear_data(dma_id, channel, address, is_var) hive_dma_set_data(dma_id, channel, address, 0, is_var)

#ifdef __HIVECC
#define hive_dma_execute(dma_id, channel, cmd, to_addr, from_addr_value, to_is_var, from_is_var) \
{ \
  hive_dma_snd(dma_id, DMA_PACK_CMD_CHANNEL(cmd, channel)); \
  hive_dma_snd(dma_id, from_addr_value); \
  hive_dma_snd(dma_id, to_addr); \
}
#else
#define hive_dma_execute(dma_id, channel, cmd, to_addr, from_addr_value, to_is_var, from_is_var) \
{ \
  hive_dma_snd(dma_id, DMA_PACK_CMD_CHANNEL(cmd, channel) | (1 << _DMA_CRUN_IDX)); \
  hive_dma_snd(dma_id, from_addr_value); \
  hive_dma_snd(dma_id, to_addr); \
  hive_dma_snd(dma_id, to_is_var); \
  if ((cmd & DMA_CLEAR_CMDBIT) == 0) { \
	hive_dma_snd(dma_id, from_is_var); \
  } \
}
#endif

#ifdef __HIVECC
#define hive_dma_configure(dma_id, channel, connection, extension, height, \
	stride_A, elems_A, cropping_A, width_A, \
	stride_B, elems_B, cropping_B, width_B) \
{ \
  hive_dma_snd(dma_id, DMA_PACK_CMD_CHANNEL(_DMA_CONFIG_CHANNEL_COMMAND, channel)); \
  hive_dma_snd(dma_id, DMA_PACK_SETUP(connection, extension, _DMA_KEEP_ELEM_ORDER)); \
  hive_dma_snd(dma_id, stride_A); \
  hive_dma_snd(dma_id, DMA_PACK_CROP_ELEMS(elems_A, cropping_A)); \
  hive_dma_snd(dma_id, width_A); \
  hive_dma_snd(dma_id, stride_B); \
  hive_dma_snd(dma_id, DMA_PACK_CROP_ELEMS(elems_B, cropping_B)); \
  hive_dma_snd(dma_id, width_B); \
  hive_dma_snd(dma_id, height); \
}

#define hive_dma_configure_fast(dma_id, channel, connection, extension, elems_A, elems_B) \
{ \
  hive_dma_snd(dma_id, DMA_PACK_CMD_CHANNEL(_DMA_CONFIG_CHANNEL_COMMAND, channel)); \
  hive_dma_snd(dma_id, DMA_PACK_SETUP(connection, extension, _DMA_KEEP_ELEM_ORDER)); \
  hive_dma_snd(dma_id, 0); \
  hive_dma_snd(dma_id, DMA_PACK_CROP_ELEMS(elems_A, 0)); \
  hive_dma_snd(dma_id, 0); \
  hive_dma_snd(dma_id, 0); \
  hive_dma_snd(dma_id, DMA_PACK_CROP_ELEMS(elems_B, 0)); \
  hive_dma_snd(dma_id, 0); \
  hive_dma_snd(dma_id, 1); \
}
#else
#define hive_dma_configure(dma_id, channel, connection, extension, height, \
	stride_A, elems_A, cropping_A, width_A, \
	stride_B, elems_B, cropping_B, width_B) \
{ \
  hive_dma_snd(dma_id, DMA_PACK_CMD_CHANNEL(_DMA_CONFIG_CHANNEL_COMMAND, channel) | (1 << _DMA_CRUN_IDX)); \
  hive_dma_snd(dma_id, DMA_PACK_SETUP(connection, extension, _DMA_KEEP_ELEM_ORDER)); \
  hive_dma_snd(dma_id, stride_A); \
  hive_dma_snd(dma_id, DMA_PACK_CROP_ELEMS(elems_A, cropping_A)); \
  hive_dma_snd(dma_id, width_A); \
  hive_dma_snd(dma_id, stride_B); \
  hive_dma_snd(dma_id, DMA_PACK_CROP_ELEMS(elems_B, cropping_B)); \
  hive_dma_snd(dma_id, width_B); \
  hive_dma_snd(dma_id, height); \
}

#define hive_dma_configure_fast(dma_id, channel, connection, extension, elems_A, elems_B) \
{ \
  hive_dma_snd(dma_id, DMA_PACK_CMD_CHANNEL(_DMA_CONFIG_CHANNEL_COMMAND, channel) | (1 << _DMA_CRUN_IDX)); \
  hive_dma_snd(dma_id, DMA_PACK_SETUP(connection, extension, _DMA_KEEP_ELEM_ORDER)); \
  hive_dma_snd(dma_id, 0); \
  hive_dma_snd(dma_id, DMA_PACK_CROP_ELEMS(elems_A, 0)); \
  hive_dma_snd(dma_id, 0); \
  hive_dma_snd(dma_id, 0); \
  hive_dma_snd(dma_id, DMA_PACK_CROP_ELEMS(elems_B, 0)); \
  hive_dma_snd(dma_id, 0); \
  hive_dma_snd(dma_id, 1); \
}
#endif

#define hive_dma_set_parameter(dma_id, channel, param, value) \
{ \
  hive_dma_snd(dma_id, _DMA_SET_CHANNEL_PARAM_COMMAND | DMA_PACK_CHANNEL(channel) | DMA_PACK_PARAM(param)); \
  hive_dma_snd(dma_id, value); \
}

#define	DMA_RW_CMDBIT		0x01
#define	DMA_CFG_CMDBIT		0x02
#define	DMA_CLEAR_CMDBIT	0x08
#define	DMA_PARAM_CMDBIT	0x01

#define	DMA_CFG_CMD			(DMA_CFG_CMDBIT)
#define	DMA_CFGPARAM_CMD	(DMA_CFG_CMDBIT | DMA_PARAM_CMDBIT)

#define DMA_CMD_NEEDS_ACK(cmd) (1)
#define DMA_CMD_IS_TRANSFER(cmd) ((cmd & DMA_CFG_CMDBIT) == 0)
#define DMA_CMD_IS_WR(cmd) ((cmd & DMA_RW_CMDBIT) != 0)
#define DMA_CMD_IS_RD(cmd) ((cmd & DMA_RW_CMDBIT) == 0)
#define DMA_CMD_IS_CLR(cmd) ((cmd & DMA_CLEAR_CMDBIT) != 0)
#define DMA_CMD_IS_CFG(cmd) ((cmd & DMA_CFG_CMDBIT) != 0)
#define DMA_CMD_IS_PARAMCFG(cmd) ((cmd & DMA_CFGPARAM_CMD) == DMA_CFGPARAM_CMD)

/* As a matter of convention */
#define DMA_TRANSFER_READ		DMA_TRANSFER_B2A
#define DMA_TRANSFER_WRITE		DMA_TRANSFER_A2B
/* store/load from the PoV of the system(memory) */
#define DMA_TRANSFER_STORE		DMA_TRANSFER_B2A
#define DMA_TRANSFER_LOAD		DMA_TRANSFER_A2B
#define DMA_TRANSFER_CLEAR		DMA_TRANSFER_CLEAR_A

typedef enum {
	DMA_TRANSFER_CLEAR_A = DMA_CLEAR_CMDBIT,
	DMA_TRANSFER_CLEAR_B = DMA_CLEAR_CMDBIT | DMA_RW_CMDBIT,
	DMA_TRANSFER_A2B = DMA_RW_CMDBIT,
	DMA_TRANSFER_B2A = 0,
} dma_transfer_type_t;

typedef enum {
	DMA_CONFIG_SETUP = _DMA_PACKING_SETUP_PARAM,
	DMA_CONFIG_HEIGHT = _DMA_HEIGHT_PARAM,
	DMA_CONFIG_STRIDE_A_ = _DMA_STRIDE_A_PARAM,
	DMA_CONFIG_CROP_ELEM_A = _DMA_ELEM_CROPPING_A_PARAM,
	DMA_CONFIG_WIDTH_A = _DMA_WIDTH_A_PARAM,
	DMA_CONFIG_STRIDE_B_ = _DMA_STRIDE_B_PARAM,
	DMA_CONFIG_CROP_ELEM_B = _DMA_ELEM_CROPPING_B_PARAM,
	DMA_CONFIG_WIDTH_B = _DMA_WIDTH_B_PARAM,
} dma_config_type_t;

#endif /* __DMA_GLOBAL_H_INCLUDED__ */
