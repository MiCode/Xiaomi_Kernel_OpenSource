/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <net/ip.h>
#include <linux/genalloc.h>	/* gen_pool_alloc() */
#include <linux/io.h>
#include <linux/ratelimit.h>
#include "ipa_i.h"

static const int ipa_ofst_meq32[] = { IPA_OFFSET_MEQ32_0,
					IPA_OFFSET_MEQ32_1, -1 };
static const int ipa_ofst_meq128[] = { IPA_OFFSET_MEQ128_0,
					IPA_OFFSET_MEQ128_1, -1 };
static const int ipa_ihl_ofst_rng16[] = { IPA_IHL_OFFSET_RANGE16_0,
					IPA_IHL_OFFSET_RANGE16_1, -1 };
static const int ipa_ihl_ofst_meq32[] = { IPA_IHL_OFFSET_MEQ32_0,
					IPA_IHL_OFFSET_MEQ32_1, -1 };

static const int ep_mapping[IPA_MODE_MAX][IPA_CLIENT_MAX] = {
	{ -1, -1, -1, -1, -1, 11, -1, 8, 6, 2, 1, 5, -1, -1, -1, -1, -1, 10, 9, 7, 3, 4 },
	{ -1, -1, -1, -1, -1, 11, -1, 8, 6, 2, 1, 5, -1, -1, -1, -1, -1, 10, 9, 7, 3, 4 },
	{ 11, 13, 15, 17, 19, -1, -1, 8, 6, 2, 1, 5, 10, 12, 14, 16, 18, -1, 9, 7, 3, 4 },
	{ 19, -1, -1, -1, -1, 11, 15, 8, 6, 2, 1, 5, 14, 16, 17, 18, -1, 10, 9, 7, 3, 4 },
	{ 19, -1, -1, -1, -1, 11, 15, 8, 6, 2, 1, 5, 14, 16, 17, 18, -1, 10, 9, 7, 3, 4 },
	{ 19, -1, -1, -1, -1, 11, 15, 8, 6, 2, 1, 5, 14, 16, 17, 18, -1, 10, 9, 7, 3, 4 },
};

/**
 * ipa_cfg_route() - configure IPA route
 * @route: IPA route
 *
 * Return codes:
 * 0: success
 */
int ipa_cfg_route(struct ipa_route *route)
{
	u32 ipa_route_offset = IPA_ROUTE_OFST_v1;

	if (ipa_ctx->ipa_hw_type != IPA_HW_v1_0)
		ipa_route_offset = IPA_ROUTE_OFST_v2;

	ipa_inc_client_enable_clks();
	ipa_write_reg(ipa_ctx->mmio, ipa_route_offset,
		     IPA_SETFIELD(route->route_dis,
				  IPA_ROUTE_ROUTE_DIS_SHFT,
				  IPA_ROUTE_ROUTE_DIS_BMSK) |
			IPA_SETFIELD(route->route_def_pipe,
				     IPA_ROUTE_ROUTE_DEF_PIPE_SHFT,
				     IPA_ROUTE_ROUTE_DEF_PIPE_BMSK) |
			IPA_SETFIELD(route->route_def_hdr_table,
				     IPA_ROUTE_ROUTE_DEF_HDR_TABLE_SHFT,
				     IPA_ROUTE_ROUTE_DEF_HDR_TABLE_BMSK) |
			IPA_SETFIELD(route->route_def_hdr_ofst,
				     IPA_ROUTE_ROUTE_DEF_HDR_OFST_SHFT,
				     IPA_ROUTE_ROUTE_DEF_HDR_OFST_BMSK));
	ipa_dec_client_disable_clks();

	return 0;
}
/**
 * ipa_cfg_filter() - configure filter
 * @disable: disable value
 *
 * Return codes:
 * 0: success
 */
int ipa_cfg_filter(u32 disable)
{
	u32 ipa_filter_ofst = IPA_FILTER_OFST_v1;

	if (ipa_ctx->ipa_hw_type != IPA_HW_v1_0)
		ipa_filter_ofst = IPA_FILTER_OFST_v2;
	ipa_inc_client_enable_clks();
	ipa_write_reg(ipa_ctx->mmio, ipa_filter_ofst,
			IPA_SETFIELD(!disable,
					IPA_FILTER_FILTER_EN_SHFT,
					IPA_FILTER_FILTER_EN_BMSK));
	ipa_dec_client_disable_clks();

	return 0;
}

/**
 * ipa_init_hw() - initialize HW
 *
 * Return codes:
 * 0: success
 */
int ipa_init_hw(void)
{
	u32 ipa_version = 0;

	/* do soft reset of IPA */
	ipa_write_reg(ipa_ctx->mmio, IPA_COMP_SW_RESET_OFST, 1);
	ipa_write_reg(ipa_ctx->mmio, IPA_COMP_SW_RESET_OFST, 0);

	/* enable IPA */
	ipa_write_reg(ipa_ctx->mmio, IPA_COMP_CFG_OFST, 1);

	/* Read IPA version and make sure we have access to the registers */
	ipa_version = ipa_read_reg(ipa_ctx->mmio, IPA_VERSION_OFST);
	if (ipa_version == 0)
		return -EFAULT;

	return 0;
}

/**
 * ipa_get_ep_mapping() - provide endpoint mapping
 * @mode: IPA operating mode
 * @client: client type
 *
 * Return value: endpoint mapping
 */
int ipa_get_ep_mapping(enum ipa_operating_mode mode,
		enum ipa_client_type client)
{
	return ep_mapping[mode][client];
}

/**
 * ipa_get_client_mapping() - provide client mapping
 * @mode: IPA operating mode
 * @pipe_idx: IPA end-point number
 *
 * Return value: client mapping
 */
int ipa_get_client_mapping(enum ipa_operating_mode mode, int pipe_idx)
{
	int i;

	for (i = 0; i < IPA_CLIENT_MAX; i++)
		if (ep_mapping[mode][i] == pipe_idx)
			break;
	return i;
}

/**
 * ipa_write_32() - convert 32 bit value to byte array
 * @w: 32 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa_write_32(u32 w, u8 *dest)
{
	*dest++ = (u8)((w) & 0xFF);
	*dest++ = (u8)((w >> 8) & 0xFF);
	*dest++ = (u8)((w >> 16) & 0xFF);
	*dest++ = (u8)((w >> 24) & 0xFF);

	return dest;
}

/**
 * ipa_write_16() - convert 16 bit value to byte array
 * @hw: 16 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa_write_16(u16 hw, u8 *dest)
{
	*dest++ = (u8)((hw) & 0xFF);
	*dest++ = (u8)((hw >> 8) & 0xFF);

	return dest;
}

/**
 * ipa_write_8() - convert 8 bit value to byte array
 * @hw: 8 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa_write_8(u8 b, u8 *dest)
{
	*dest++ = (b) & 0xFF;

	return dest;
}

/**
 * ipa_pad_to_32() - pad byte array to 32 bit value
 * @dest: byte array
 *
 * Return value: padded value
 */
u8 *ipa_pad_to_32(u8 *dest)
{
	int i = (u32)dest & 0x3;
	int j;

	if (i)
		for (j = 0; j < (4 - i); j++)
			*dest++ = 0;

	return dest;
}

/**
 * ipa_generate_hw_rule() - generate HW rule
 * @ip: IP address type
 * @attrib: IPA rule attribute
 * @buf: output buffer
 * @en_rule: rule
 *
 * Return codes:
 * 0: success
 * -EPERM: wrong input
 */
int ipa_generate_hw_rule(enum ipa_ip_type ip,
		const struct ipa_rule_attrib *attrib, u8 **buf, u16 *en_rule)
{
	u8 ofst_meq32 = 0;
	u8 ihl_ofst_rng16 = 0;
	u8 ihl_ofst_meq32 = 0;
	u8 ofst_meq128 = 0;

	if (ip == IPA_IP_v4) {

		/* error check */
		if (attrib->attrib_mask & IPA_FLT_NEXT_HDR ||
		    attrib->attrib_mask & IPA_FLT_TC || attrib->attrib_mask &
		    IPA_FLT_FLOW_LABEL) {
			IPAERR("v6 attrib's specified for v4 rule\n");
			return -EPERM;
		}

		if (attrib->attrib_mask & IPA_FLT_TOS) {
			*en_rule |= IPA_TOS_EQ;
			*buf = ipa_write_8(attrib->u.v4.tos, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

		if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			/* 0 => offset of TOS in v4 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_32((attrib->tos_mask << 16), *buf);
			*buf = ipa_write_32(attrib->tos_value, *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_PROTOCOL) {
			*en_rule |= IPA_PROTOCOL_EQ;
			*buf = ipa_write_8(attrib->u.v4.protocol, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			/* 12 => offset of src ip in v4 header */
			*buf = ipa_write_8(12, *buf);
			*buf = ipa_write_32(attrib->u.v4.src_addr_mask, *buf);
			*buf = ipa_write_32(attrib->u.v4.src_addr, *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			/* 16 => offset of dst ip in v4 header */
			*buf = ipa_write_8(16, *buf);
			*buf = ipa_write_32(attrib->u.v4.dst_addr_mask, *buf);
			*buf = ipa_write_32(attrib->u.v4.dst_addr, *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			if (attrib->src_port_hi < attrib->src_port_lo) {
				IPAERR("bad src port range param\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 0  => offset of src port after v4 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_16(attrib->src_port_hi, *buf);
			*buf = ipa_write_16(attrib->src_port_lo, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			if (attrib->dst_port_hi < attrib->dst_port_lo) {
				IPAERR("bad dst port range param\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 2  => offset of dst port after v4 header */
			*buf = ipa_write_8(2, *buf);
			*buf = ipa_write_16(attrib->dst_port_hi, *buf);
			*buf = ipa_write_16(attrib->dst_port_lo, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_TYPE) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			/* 0  => offset of type after v4 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_32(0xFF, *buf);
			*buf = ipa_write_32(attrib->type, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_CODE) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			/* 1  => offset of code after v4 header */
			*buf = ipa_write_8(1, *buf);
			*buf = ipa_write_32(0xFF, *buf);
			*buf = ipa_write_32(attrib->code, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SPI) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			/* 0  => offset of SPI after v4 header FIXME */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_32(0xFFFFFFFF, *buf);
			*buf = ipa_write_32(attrib->spi, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 0  => offset of src port after v4 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_16(attrib->src_port, *buf);
			*buf = ipa_write_16(attrib->src_port, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 2  => offset of dst port after v4 header */
			*buf = ipa_write_8(2, *buf);
			*buf = ipa_write_16(attrib->dst_port, *buf);
			*buf = ipa_write_16(attrib->dst_port, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_META_DATA) {
			*en_rule |= IPA_METADATA_COMPARE;
			*buf = ipa_write_8(0, *buf);    /* offset, reserved */
			*buf = ipa_write_32(attrib->meta_data_mask, *buf);
			*buf = ipa_write_32(attrib->meta_data, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

		if (attrib->attrib_mask & IPA_FLT_FRAGMENT) {
			*en_rule |= IPA_IPV4_IS_FRAG;
			*buf = ipa_pad_to_32(*buf);
		}

	} else if (ip == IPA_IP_v6) {

		/* v6 code below assumes no extension headers TODO: fix this */

		/* error check */
		if (attrib->attrib_mask & IPA_FLT_TOS ||
		    attrib->attrib_mask & IPA_FLT_PROTOCOL ||
		    attrib->attrib_mask & IPA_FLT_FRAGMENT) {
			IPAERR("v4 attrib's specified for v6 rule\n");
			return -EPERM;
		}

		if (attrib->attrib_mask & IPA_FLT_NEXT_HDR) {
			*en_rule |= IPA_PROTOCOL_EQ;
			*buf = ipa_write_8(attrib->u.v6.next_hdr, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

		if (attrib->attrib_mask & IPA_FLT_TYPE) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			/* 0  => offset of type after v6 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_32(0xFF, *buf);
			*buf = ipa_write_32(attrib->type, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_CODE) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			/* 1  => offset of code after v6 header */
			*buf = ipa_write_8(1, *buf);
			*buf = ipa_write_32(0xFF, *buf);
			*buf = ipa_write_32(attrib->code, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SPI) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			/* 0  => offset of SPI after v6 header FIXME */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_32(0xFFFFFFFF, *buf);
			*buf = ipa_write_32(attrib->spi, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 0  => offset of src port after v6 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_16(attrib->src_port, *buf);
			*buf = ipa_write_16(attrib->src_port, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 2  => offset of dst port after v6 header */
			*buf = ipa_write_8(2, *buf);
			*buf = ipa_write_16(attrib->dst_port, *buf);
			*buf = ipa_write_16(attrib->dst_port, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			if (attrib->src_port_hi < attrib->src_port_lo) {
				IPAERR("bad src port range param\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 0  => offset of src port after v6 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_16(attrib->src_port_hi, *buf);
			*buf = ipa_write_16(attrib->src_port_lo, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			if (attrib->dst_port_hi < attrib->dst_port_lo) {
				IPAERR("bad dst port range param\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 2  => offset of dst port after v6 header */
			*buf = ipa_write_8(2, *buf);
			*buf = ipa_write_16(attrib->dst_port_hi, *buf);
			*buf = ipa_write_16(attrib->dst_port_lo, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];
			/* 8 => offset of src ip in v6 header */
			*buf = ipa_write_8(8, *buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr_mask[0],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr_mask[1],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr_mask[2],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr_mask[3],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr[0], *buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr[1], *buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr[2], *buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr[3], *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];
			/* 24 => offset of dst ip in v6 header */
			*buf = ipa_write_8(24, *buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr_mask[0],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr_mask[1],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr_mask[2],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr_mask[3],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr[0], *buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr[1], *buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr[2], *buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr[3], *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_TC) {
			*en_rule |= IPA_FLT_TC;
			*buf = ipa_write_8(attrib->u.v6.tc, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

		if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			/* 0 => offset of TOS in v4 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_32((attrib->tos_mask << 20), *buf);
			*buf = ipa_write_32(attrib->tos_value, *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_FLOW_LABEL) {
			*en_rule |= IPA_FLT_FLOW_LABEL;
			 /* FIXME FL is only 20 bits */
			*buf = ipa_write_32(attrib->u.v6.flow_label, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

		if (attrib->attrib_mask & IPA_FLT_META_DATA) {
			*en_rule |= IPA_METADATA_COMPARE;
			*buf = ipa_write_8(0, *buf);    /* offset, reserved */
			*buf = ipa_write_32(attrib->meta_data_mask, *buf);
			*buf = ipa_write_32(attrib->meta_data, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

	} else {
		IPAERR("unsupported ip %d\n", ip);
		return -EPERM;
	}

	/*
	 * default "rule" means no attributes set -> map to
	 * OFFSET_MEQ32_0 with mask of 0 and val of 0 and offset 0
	 */
	if (attrib->attrib_mask == 0) {
		if (ipa_ofst_meq32[ofst_meq32] == -1) {
			IPAERR("ran out of meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq32[ofst_meq32];
		*buf = ipa_write_8(0, *buf);    /* offset */
		*buf = ipa_write_32(0, *buf);   /* mask */
		*buf = ipa_write_32(0, *buf);   /* val */
		*buf = ipa_pad_to_32(*buf);
		ofst_meq32++;
	}

	return 0;
}

/**
 * ipa_cfg_ep - IPA end-point configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * This includes nat, header, mode, aggregation and route settings and is a one
 * shot API to configure the IPA end-point fully
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep(u32 clnt_hdl, const struct ipa_ep_cfg *ipa_ep_cfg)
{
	int result = -EINVAL;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ipa_ep_cfg == NULL) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	result = ipa_cfg_ep_hdr(clnt_hdl, &ipa_ep_cfg->hdr);
	if (result)
		return result;

	result = ipa_cfg_ep_aggr(clnt_hdl, &ipa_ep_cfg->aggr);
	if (result)
		return result;

	if (IPA_CLIENT_IS_PROD(ipa_ctx->ep[clnt_hdl].client)) {
		result = ipa_cfg_ep_nat(clnt_hdl, &ipa_ep_cfg->nat);
		if (result)
			return result;

		result = ipa_cfg_ep_mode(clnt_hdl, &ipa_ep_cfg->mode);
		if (result)
			return result;

		result = ipa_cfg_ep_route(clnt_hdl, &ipa_ep_cfg->route);
		if (result)
			return result;
	}

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep);

/**
 * ipa_cfg_ep_nat() - IPA end-point NAT configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_nat(u32 clnt_hdl, const struct ipa_ep_cfg_nat *ipa_ep_cfg)
{
	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ipa_ep_cfg == NULL) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa_ctx->ep[clnt_hdl].client)) {
		IPAERR("NAT does not apply to IPA out EP %d\n", clnt_hdl);
		return -EINVAL;
	}
	/* copy over EP cfg */
	ipa_ctx->ep[clnt_hdl].cfg.nat = *ipa_ep_cfg;
	ipa_inc_client_enable_clks();
	/* clnt_hdl is used as pipe_index */
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0)
		ipa_write_reg(ipa_ctx->mmio,
			      IPA_ENDP_INIT_NAT_n_OFST_v1(clnt_hdl),
			      IPA_SETFIELD(ipa_ctx->ep[clnt_hdl].cfg.nat.nat_en,
					   IPA_ENDP_INIT_NAT_n_NAT_EN_SHFT,
					   IPA_ENDP_INIT_NAT_n_NAT_EN_BMSK));
	else
		ipa_write_reg(ipa_ctx->mmio,
			      IPA_ENDP_INIT_NAT_n_OFST_v2(clnt_hdl),
			      IPA_SETFIELD(ipa_ctx->ep[clnt_hdl].cfg.nat.nat_en,
					   IPA_ENDP_INIT_NAT_n_NAT_EN_SHFT,
					   IPA_ENDP_INIT_NAT_n_NAT_EN_BMSK));
	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_nat);

/**
 * ipa_cfg_ep_hdr() -  IPA end-point header configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_hdr(u32 clnt_hdl, const struct ipa_ep_cfg_hdr *ipa_ep_cfg)
{
	u32 val;
	struct ipa_ep_context *ep;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ipa_ep_cfg == NULL) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa_ctx->ep[clnt_hdl];

	/* copy over EP cfg */
	ep->cfg.hdr = *ipa_ep_cfg;

	val = IPA_SETFIELD(ep->cfg.hdr.hdr_len,
		   IPA_ENDP_INIT_HDR_n_HDR_LEN_SHFT,
		   IPA_ENDP_INIT_HDR_n_HDR_LEN_BMSK) |
	      IPA_SETFIELD(ep->cfg.hdr.hdr_ofst_metadata_valid,
		   IPA_ENDP_INIT_HDR_n_HDR_OFST_METADATA_VALID_SHFT,
		   IPA_ENDP_INIT_HDR_n_HDR_OFST_METADATA_VALID_BMSK) |
	      IPA_SETFIELD(ep->cfg.hdr.hdr_ofst_metadata,
		   IPA_ENDP_INIT_HDR_n_HDR_OFST_METADATA_SHFT,
		   IPA_ENDP_INIT_HDR_n_HDR_OFST_METADATA_BMSK) |
	      IPA_SETFIELD(ep->cfg.hdr.hdr_additional_const_len,
		   IPA_ENDP_INIT_HDR_n_HDR_ADDITIONAL_CONST_LEN_SHFT,
		   IPA_ENDP_INIT_HDR_n_HDR_ADDITIONAL_CONST_LEN_BMSK) |
	      IPA_SETFIELD(ep->cfg.hdr.hdr_ofst_pkt_size_valid,
		   IPA_ENDP_INIT_HDR_n_HDR_OFST_PKT_SIZE_VALID_SHFT,
		   IPA_ENDP_INIT_HDR_n_HDR_OFST_PKT_SIZE_VALID_BMSK) |
	      IPA_SETFIELD(ep->cfg.hdr.hdr_ofst_pkt_size,
		   IPA_ENDP_INIT_HDR_n_HDR_OFST_PKT_SIZE_SHFT,
		   IPA_ENDP_INIT_HDR_n_HDR_OFST_PKT_SIZE_BMSK) |
	      IPA_SETFIELD(ep->cfg.hdr.hdr_a5_mux,
		   IPA_ENDP_INIT_HDR_n_HDR_A5_MUX_SHFT,
		   IPA_ENDP_INIT_HDR_n_HDR_A5_MUX_BMSK);

	ipa_inc_client_enable_clks();
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0)
		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HDR_n_OFST_v1(clnt_hdl), val);
	else
		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HDR_n_OFST_v2(clnt_hdl), val);
	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_hdr);

/**
 * ipa_cfg_ep_mode() - IPA end-point mode configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_mode(u32 clnt_hdl, const struct ipa_ep_cfg_mode *ipa_ep_cfg)
{
	u32 val;
	int ep;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ipa_ep_cfg == NULL) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa_ctx->ep[clnt_hdl].client)) {
		IPAERR("MODE does not apply to IPA out EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	ep = ipa_get_ep_mapping(ipa_ctx->mode, ipa_ep_cfg->dst);
	if (ep == -1 && ipa_ep_cfg->mode == IPA_DMA) {
		IPAERR("dst %d does not exist in mode %d\n", ipa_ep_cfg->dst,
		       ipa_ctx->mode);
		return -EINVAL;
	}

	/* copy over EP cfg */
	ipa_ctx->ep[clnt_hdl].cfg.mode = *ipa_ep_cfg;
	ipa_ctx->ep[clnt_hdl].dst_pipe_index = ep;

	val = IPA_SETFIELD(ipa_ctx->ep[clnt_hdl].cfg.mode.mode,
			   IPA_ENDP_INIT_MODE_n_MODE_SHFT,
			   IPA_ENDP_INIT_MODE_n_MODE_BMSK) |
	      IPA_SETFIELD(ipa_ctx->ep[clnt_hdl].dst_pipe_index,
			   IPA_ENDP_INIT_MODE_n_DEST_PIPE_INDEX_SHFT,
			   IPA_ENDP_INIT_MODE_n_DEST_PIPE_INDEX_BMSK);

	ipa_inc_client_enable_clks();
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0)
		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_MODE_n_OFST_v1(clnt_hdl), val);
	else
		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_MODE_n_OFST_v2(clnt_hdl), val);
	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_mode);

/**
 * ipa_cfg_ep_aggr() - IPA end-point aggregation configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_aggr(u32 clnt_hdl, const struct ipa_ep_cfg_aggr *ipa_ep_cfg)
{
	u32 val;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ipa_ep_cfg == NULL) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}
	/* copy over EP cfg */
	ipa_ctx->ep[clnt_hdl].cfg.aggr = *ipa_ep_cfg;

	val = IPA_SETFIELD(ipa_ctx->ep[clnt_hdl].cfg.aggr.aggr_en,
			   IPA_ENDP_INIT_AGGR_n_AGGR_EN_SHFT,
			   IPA_ENDP_INIT_AGGR_n_AGGR_EN_BMSK) |
	      IPA_SETFIELD(ipa_ctx->ep[clnt_hdl].cfg.aggr.aggr,
			   IPA_ENDP_INIT_AGGR_n_AGGR_TYPE_SHFT,
			   IPA_ENDP_INIT_AGGR_n_AGGR_TYPE_BMSK) |
	      IPA_SETFIELD(ipa_ctx->ep[clnt_hdl].cfg.aggr.aggr_byte_limit,
			   IPA_ENDP_INIT_AGGR_n_AGGR_BYTE_LIMIT_SHFT,
			   IPA_ENDP_INIT_AGGR_n_AGGR_BYTE_LIMIT_BMSK) |
	      IPA_SETFIELD(ipa_ctx->ep[clnt_hdl].cfg.aggr.aggr_time_limit,
			   IPA_ENDP_INIT_AGGR_n_AGGR_TIME_LIMIT_SHFT,
			   IPA_ENDP_INIT_AGGR_n_AGGR_TIME_LIMIT_BMSK);

	ipa_inc_client_enable_clks();
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0)
		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_AGGR_n_OFST_v1(clnt_hdl), val);
	else
		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_AGGR_n_OFST_v2(clnt_hdl), val);
	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_aggr);

/**
 * ipa_cfg_ep_route() - IPA end-point routing configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_route(u32 clnt_hdl, const struct ipa_ep_cfg_route *ipa_ep_cfg)
{
	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ipa_ep_cfg == NULL) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa_ctx->ep[clnt_hdl].client)) {
		IPAERR("ROUTE does not apply to IPA out EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	/*
	 * if DMA mode was configured previously for this EP, return with
	 * success
	 */
	if (ipa_ctx->ep[clnt_hdl].cfg.mode.mode == IPA_DMA) {
		IPADBG("DMA mode for EP %d\n", clnt_hdl);
		return 0;
	}

	if (ipa_ep_cfg->rt_tbl_hdl)
		IPAERR("client specified non-zero RT TBL hdl - ignore it\n");

	/* always use the "default" routing tables whose indices are 0 */
	ipa_ctx->ep[clnt_hdl].rt_tbl_idx = 0;

	ipa_inc_client_enable_clks();
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0) {
		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_ROUTE_n_OFST_v1(clnt_hdl),
			IPA_SETFIELD(ipa_ctx->ep[clnt_hdl].rt_tbl_idx,
			   IPA_ENDP_INIT_ROUTE_n_ROUTE_TABLE_INDEX_SHFT,
			   IPA_ENDP_INIT_ROUTE_n_ROUTE_TABLE_INDEX_BMSK));
	} else {
		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_ROUTE_n_OFST_v2(clnt_hdl),
			IPA_SETFIELD(ipa_ctx->ep[clnt_hdl].rt_tbl_idx,
			   IPA_ENDP_INIT_ROUTE_n_ROUTE_TABLE_INDEX_SHFT,
			   IPA_ENDP_INIT_ROUTE_n_ROUTE_TABLE_INDEX_BMSK));
	}
	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_route);

/**
 * ipa_cfg_ep_holb() - IPA end-point holb configuration
 *
 * If an IPA producer pipe is full, IPA HW by default will block
 * indefinitely till space opens up. During this time no packets
 * including those from unrelated pipes will be processed. Enabling
 * HOLB means IPA HW will be allowed to drop packets as/when needed
 * and indefinite blocking is avoided.
 *
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_cfg_ep_holb(u32 clnt_hdl, const struct ipa_ep_cfg_holb *ipa_ep_cfg)
{
	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ipa_ep_cfg == NULL || ipa_ep_cfg->tmr_val > 511 ||
			ipa_ep_cfg->en > 1) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_PROD(ipa_ctx->ep[clnt_hdl].client)) {
		IPAERR("HOLB does not apply to IPA in EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0) {
		IPAERR("per EP HOLB not supported\n");
		return -EPERM;
	} else {
		ipa_ctx->ep[clnt_hdl].holb = *ipa_ep_cfg;
		ipa_inc_client_enable_clks();
		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_EN_n_OFST(clnt_hdl),
			ipa_ep_cfg->en);
		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_TIMER_n_OFST(clnt_hdl),
			ipa_ep_cfg->tmr_val);
		ipa_dec_client_disable_clks();
		IPAERR("cfg holb %u ep=%d tmr=%d\n", ipa_ep_cfg->en, clnt_hdl,
				ipa_ep_cfg->tmr_val);
	}

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_holb);

/**
 * ipa_cfg_ep_holb_by_client() - IPA end-point holb configuration
 *
 * Wrapper function for ipa_cfg_ep_holb() with client name instead of
 * client handle. This function is used for clients that does not have
 * client handle.
 *
 * @client:	[in] client name
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_cfg_ep_holb_by_client(enum ipa_client_type client,
				const struct ipa_ep_cfg_holb *ipa_ep_cfg)
{
	return ipa_cfg_ep_holb(ipa_get_ep_mapping(ipa_ctx->mode, client),
			       ipa_ep_cfg);
}
EXPORT_SYMBOL(ipa_cfg_ep_holb_by_client);

/**
 * ipa_dump_buff_internal() - dumps buffer for debug purposes
 * @base: buffer base address
 * @phy_base: buffer physical base address
 * @size: size of the buffer
 */
void ipa_dump_buff_internal(void *base, dma_addr_t phy_base, u32 size)
{
	int i;
	u32 *cur = (u32 *)base;
	u8 *byt;
	IPADBG("START phys=%x\n", phy_base);
	for (i = 0; i < size / 4; i++) {
		byt = (u8 *)(cur + i);
		IPADBG("%2d %08x   %02x %02x %02x %02x\n", i, *(cur + i),
				byt[0], byt[1], byt[2], byt[3]);
	}
	IPADBG("END\n");
}

/**
 * ipa_dump() - dumps part of driver data structures for debug purposes
 */
void ipa_dump(void)
{
	struct ipa_mem_buffer hdr_mem = { 0 };
	struct ipa_mem_buffer rt_mem = { 0 };
	struct ipa_mem_buffer flt_mem = { 0 };

	mutex_lock(&ipa_ctx->lock);

	if (ipa_generate_hdr_hw_tbl(&hdr_mem))
		IPAERR("fail\n");
	if (ipa_generate_rt_hw_tbl(IPA_IP_v4, &rt_mem))
		IPAERR("fail\n");
	if (ipa_generate_flt_hw_tbl(IPA_IP_v4, &flt_mem))
		IPAERR("fail\n");
	IPAERR("PHY hdr=%x rt=%x flt=%x\n", hdr_mem.phys_base, rt_mem.phys_base,
			flt_mem.phys_base);
	IPAERR("VIRT hdr=%x rt=%x flt=%x\n", (u32)hdr_mem.base,
			(u32)rt_mem.base, (u32)flt_mem.base);
	IPAERR("SIZE hdr=%d rt=%d flt=%d\n", hdr_mem.size, rt_mem.size,
			flt_mem.size);
	IPA_DUMP_BUFF(hdr_mem.base, hdr_mem.phys_base, hdr_mem.size);
	IPA_DUMP_BUFF(rt_mem.base, rt_mem.phys_base, rt_mem.size);
	IPA_DUMP_BUFF(flt_mem.base, flt_mem.phys_base, flt_mem.size);
	if (hdr_mem.phys_base)
		dma_free_coherent(NULL, hdr_mem.size, hdr_mem.base,
				hdr_mem.phys_base);
	if (rt_mem.phys_base)
		dma_free_coherent(NULL, rt_mem.size, rt_mem.base,
				rt_mem.phys_base);
	if (flt_mem.phys_base)
		dma_free_coherent(NULL, flt_mem.size, flt_mem.base,
				flt_mem.phys_base);
	mutex_unlock(&ipa_ctx->lock);
}

/**
 * ipa_search() - search for handle in RB tree
 * @root: tree root
 * @hdl: handle
 *
 * Return value: tree node corresponding to the handle
 */
struct ipa_tree_node *ipa_search(struct rb_root *root, u32 hdl)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct ipa_tree_node *data = container_of(node,
				struct ipa_tree_node, node);

		if (hdl < data->hdl)
			node = node->rb_left;
		else if (hdl > data->hdl)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

/**
 * ipa_insert() - insert new node to RB tree
 * @root: tree root
 * @data: new data to insert
 *
 * Return value:
 * 0: success
 * -EPERM: tree already contains the node with provided handle
 */
int ipa_insert(struct rb_root *root, struct ipa_tree_node *data)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct ipa_tree_node *this = container_of(*new,
				struct ipa_tree_node, node);

		parent = *new;
		if (data->hdl < this->hdl)
			new = &((*new)->rb_left);
		else if (data->hdl > this->hdl)
			new = &((*new)->rb_right);
		else
			return -EPERM;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, root);

	return 0;
}

/**
 * ipa_pipe_mem_init() - initialize the pipe memory
 * @start_ofst: start offset
 * @size: size
 *
 * Return value:
 * 0: success
 * -ENOMEM: no memory
 */
int ipa_pipe_mem_init(u32 start_ofst, u32 size)
{
	int res;
	u32 aligned_start_ofst;
	u32 aligned_size;
	struct gen_pool *pool;

	if (!size) {
		IPAERR("no IPA pipe mem alloted\n");
		goto fail;
	}

	aligned_start_ofst = IPA_HW_TABLE_ALIGNMENT(start_ofst);
	aligned_size = size - (aligned_start_ofst - start_ofst);

	IPADBG("start_ofst=%u aligned_start_ofst=%u size=%u aligned_size=%u\n",
	       start_ofst, aligned_start_ofst, size, aligned_size);

	/* allocation order of 8 i.e. 128 bytes, global pool */
	pool = gen_pool_create(8, -1);
	if (!pool) {
		IPAERR("Failed to create a new memory pool.\n");
		goto fail;
	}

	res = gen_pool_add(pool, aligned_start_ofst, aligned_size, -1);
	if (res) {
		IPAERR("Failed to add memory to IPA pipe pool\n");
		goto err_pool_add;
	}

	ipa_ctx->pipe_mem_pool = pool;
	return 0;

err_pool_add:
	gen_pool_destroy(pool);
fail:
	return -ENOMEM;
}

/**
 * ipa_pipe_mem_alloc() - allocate pipe memory
 * @ofst: offset
 * @size: size
 *
 * Return value:
 * 0: success
 */
int ipa_pipe_mem_alloc(u32 *ofst, u32 size)
{
	u32 vaddr;
	int res = -1;

	if (!ipa_ctx->pipe_mem_pool || !size) {
		IPAERR("failed size=%u pipe_mem_pool=%p\n", size,
				ipa_ctx->pipe_mem_pool);
		return res;
	}

	vaddr = gen_pool_alloc(ipa_ctx->pipe_mem_pool, size);

	if (vaddr) {
		*ofst = vaddr;
		res = 0;
		IPADBG("size=%u ofst=%u\n", size, vaddr);
	} else {
		IPAERR("size=%u failed\n", size);
	}

	return res;
}

/**
 * ipa_pipe_mem_free() - free pipe memory
 * @ofst: offset
 * @size: size
 *
 * Return value:
 * 0: success
 */
int ipa_pipe_mem_free(u32 ofst, u32 size)
{
	IPADBG("size=%u ofst=%u\n", size, ofst);
	if (ipa_ctx->pipe_mem_pool && size)
		gen_pool_free(ipa_ctx->pipe_mem_pool, ofst, size);
	return 0;
}

/**
 * ipa_set_aggr_mode() - Set the aggregation mode which is a global setting
 * @mode:	[in] the desired aggregation mode for e.g. straight MBIM, QCNCM,
 * etc
 *
 * Returns:	0 on success
 */
int ipa_set_aggr_mode(enum ipa_aggr_mode mode)
{
	u32 reg_val;

	ipa_inc_client_enable_clks();
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0) {
		reg_val = ipa_read_reg(ipa_ctx->mmio,
				IPA_AGGREGATION_SPARE_REG_2_OFST);
		ipa_write_reg(ipa_ctx->mmio,
				IPA_AGGREGATION_SPARE_REG_2_OFST,
				((mode & IPA_AGGREGATION_MODE_MSK) <<
					IPA_AGGREGATION_MODE_SHFT) |
					(reg_val & IPA_AGGREGATION_MODE_BMSK));
	} else {
		reg_val = ipa_read_reg(ipa_ctx->mmio, IPA_QCNCM_OFST);
		ipa_write_reg(ipa_ctx->mmio, IPA_QCNCM_OFST, (mode & 0x1) |
				(reg_val & 0xfffffffe));

	}
	ipa_dec_client_disable_clks();
	return 0;
}
EXPORT_SYMBOL(ipa_set_aggr_mode);

/**
 * ipa_set_qcncm_ndp_sig() - Set the NDP signature used for QCNCM aggregation
 * mode
 * @sig:	[in] the first 3 bytes of QCNCM NDP signature (expected to be
 * "QND")
 *
 * Set the NDP signature used for QCNCM aggregation mode. The fourth byte
 * (expected to be 'P') needs to be set using the header addition mechanism
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_set_qcncm_ndp_sig(char sig[3])
{
	u32 reg_val;

	if (sig == NULL) {
		IPAERR("bad argument for ipa_set_qcncm_ndp_sig/n");
		return -EINVAL;
	}
	ipa_inc_client_enable_clks();
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0) {
		reg_val = ipa_read_reg(ipa_ctx->mmio,
				IPA_AGGREGATION_SPARE_REG_2_OFST);
		ipa_write_reg(ipa_ctx->mmio,
				IPA_AGGREGATION_SPARE_REG_2_OFST, sig[0] <<
				IPA_AGGREGATION_QCNCM_SIG0_SHFT |
				(sig[1] << IPA_AGGREGATION_QCNCM_SIG1_SHFT) |
				sig[2] |
				(reg_val & IPA_AGGREGATION_QCNCM_SIG_BMSK));
	} else {
		reg_val = ipa_read_reg(ipa_ctx->mmio, IPA_QCNCM_OFST);
		ipa_write_reg(ipa_ctx->mmio, IPA_QCNCM_OFST, sig[0] << 20 |
				(sig[1] << 12) | (sig[2] << 4) |
				(reg_val & 0xf000000f));
	}
	ipa_dec_client_disable_clks();
	return 0;
}
EXPORT_SYMBOL(ipa_set_qcncm_ndp_sig);

/**
 * ipa_set_single_ndp_per_mbim() - Enable/disable single NDP per MBIM frame
 * configuration
 * @enable:	[in] true for single NDP/MBIM; false otherwise
 *
 * Returns:	0 on success
 */
int ipa_set_single_ndp_per_mbim(bool enable)
{
	u32 reg_val;

	ipa_inc_client_enable_clks();
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0) {
		reg_val = ipa_read_reg(ipa_ctx->mmio,
				IPA_AGGREGATION_SPARE_REG_1_OFST);
		ipa_write_reg(ipa_ctx->mmio,
				IPA_AGGREGATION_SPARE_REG_1_OFST, (enable &
				IPA_AGGREGATION_SINGLE_NDP_MSK) |
				(reg_val & IPA_AGGREGATION_SINGLE_NDP_BMSK));
	} else {
		reg_val = ipa_read_reg(ipa_ctx->mmio, IPA_SINGLE_NDP_MODE_OFST);
		ipa_write_reg(ipa_ctx->mmio, IPA_SINGLE_NDP_MODE_OFST,
				(enable & 0x1) | (reg_val & 0xfffffffe));
	}
	ipa_dec_client_disable_clks();
	return 0;
}
EXPORT_SYMBOL(ipa_set_single_ndp_per_mbim);

/**
 * ipa_set_hw_timer_fix_for_mbim_aggr() - Enable/disable HW timer fix
 * for MBIM aggregation.
 * @enable:	[in] true for enable HW fix; false otherwise
 *
 * Returns:	0 on success
 */
int ipa_set_hw_timer_fix_for_mbim_aggr(bool enable)
{
	u32 reg_val;
	ipa_inc_client_enable_clks();
	reg_val = ipa_read_reg(ipa_ctx->mmio, IPA_AGGREGATION_SPARE_REG_1_OFST);
	ipa_write_reg(ipa_ctx->mmio, IPA_AGGREGATION_SPARE_REG_1_OFST,
		(enable << IPA_AGGREGATION_HW_TIMER_FIX_MBIM_AGGR_SHFT) |
		(reg_val & ~IPA_AGGREGATION_HW_TIMER_FIX_MBIM_AGGR_BMSK));
	ipa_dec_client_disable_clks();
	return 0;
}
EXPORT_SYMBOL(ipa_set_hw_timer_fix_for_mbim_aggr);

/**
 * ipa_straddle_boundary() - Checks whether a memory buffer straddles a boundary
 * @start: start address of the memory buffer
 * @end: end address of the memory buffer
 * @boundary: boundary
 *
 * Return value:
 * 1: if the interval [start, end] straddles boundary
 * 0: otherwise
 */
int ipa_straddle_boundary(u32 start, u32 end, u32 boundary)
{
	u32 next_start;
	u32 prev_end;

	IPADBG("start=%u end=%u boundary=%u\n", start, end, boundary);

	next_start = (start + (boundary - 1)) & ~(boundary - 1);
	prev_end = ((end + (boundary - 1)) & ~(boundary - 1)) - boundary;

	while (next_start < prev_end)
		next_start += boundary;

	if (next_start == prev_end)
		return 1;
	else
		return 0;
}

/**
 * ipa_bam_reg_dump() - Dump selected BAM registers for IPA and DMA-BAM
 *
 * Function is rate limited to avoid flooding kernel log buffer
 */
void ipa_bam_reg_dump(void)
{
	static DEFINE_RATELIMIT_STATE(_rs, 500*HZ, 1);
	if (__ratelimit(&_rs)) {
		ipa_inc_client_enable_clks();
		pr_err("IPA BAM START\n");
		sps_get_bam_debug_info(ipa_ctx->bam_handle, 5, 479182, 0, 0);
		sps_get_bam_debug_info(ipa_ctx->bam_handle, 93, 0, 0, 0);
		ipa_dec_client_disable_clks();
	}
}
EXPORT_SYMBOL(ipa_bam_reg_dump);

