#ifndef _NET_MAP_H_
#define _NET_MAP_H_

struct rmnet_map_header_s {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint8_t  pad_len:6;
	uint8_t  reserved_bit:1;
	uint8_t  cd_bit:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	uint8_t  cd_bit:1;
	uint8_t  reserved_bit:1;
	uint8_t  pad_len:6;
#else
#error "Please fix <asm/byteorder.h>"
#endif
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

#define RMNET_IP_VER_MASK 0xF0
#define RMNET_IPV4        0x40
#define RMNET_IPV6        0x60

#endif /* _NET_MAP_H_ */
