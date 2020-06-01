/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 *
 * Legacy RMNET Data MAP header definitions
 */

#ifndef _RMNET_CONFIG_H_
#define _RMNET_CONFIG_H_

#include <linux/types.h>

struct rmnet_map_header_s {
#ifndef RMNET_USE_BIG_ENDIAN_STRUCTS
	uint8_t  pad_len:6;
	uint8_t  reserved_bit:1;
	uint8_t  cd_bit:1;
#else
	uint8_t  cd_bit:1;
	uint8_t  reserved_bit:1;
	uint8_t  pad_len:6;
#endif /* RMNET_USE_BIG_ENDIAN_STRUCTS */
	uint8_t  mux_id;
	uint16_t pkt_len;
}  __aligned(1);

#define RMNET_MAP_GET_MUX_ID(Y) (((struct rmnet_map_header_s *)Y->data)->mux_id)
#define RMNET_MAP_GET_CD_BIT(Y) (((struct rmnet_map_header_s *)Y->data)->cd_bit)
#define RMNET_MAP_GET_PAD(Y) (((struct rmnet_map_header_s *)Y->data)->pad_len)
#define RMNET_MAP_GET_CMD_START(Y) ((struct rmnet_map_control_command_s *) \
				  (Y->data + sizeof(struct rmnet_map_header_s)))
#define RMNET_MAP_GET_LENGTH(Y) (ntohs( \
			       ((struct rmnet_map_header_s *)Y->data)->pkt_len))

#endif /* _RMNET_CONFIG_H_ */
