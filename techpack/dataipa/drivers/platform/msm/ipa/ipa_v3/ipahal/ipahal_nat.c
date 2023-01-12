// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include "ipahal_nat.h"
#include "ipahal_nat_i.h"
#include "ipahal_i.h"

#define IPA_64_LOW_32_MASK (0xFFFFFFFF)
#define IPA_64_HIGH_32_MASK (0xFFFFFFFF00000000ULL)

static const char *ipahal_nat_type_to_str[IPA_NAT_MAX] = {
	__stringify(IPAHAL_NAT_IPV4),
	__stringify(IPAHAL_NAT_IPV4_INDEX),
	__stringify(IPAHAL_NAT_IPV4_PDN),
	__stringify(IPAHAL_NAT_IPV6CT)
};

static size_t ipa_nat_ipv4_entry_size_v_3_0(void)
{
	return sizeof(struct ipa_nat_hw_ipv4_entry);
}

static size_t ipa_nat_ipv4_index_entry_size_v_3_0(void)
{
	return sizeof(struct ipa_nat_hw_indx_entry);
}

static size_t ipa_nat_ipv4_pdn_entry_size_v_4_0(void)
{
	return sizeof(struct ipa_nat_hw_pdn_entry);
}

static size_t ipa_nat_ipv6ct_entry_size_v_4_0(void)
{
	return sizeof(struct ipa_nat_hw_ipv6ct_entry);
}

static bool ipa_nat_ipv4_is_entry_zeroed_v_3_0(const void *entry)
{
	struct ipa_nat_hw_ipv4_entry zero_entry = { 0 };

	return (memcmp(&zero_entry, entry, sizeof(zero_entry))) ? false : true;
}

static bool ipa_nat_ipv4_is_index_entry_zeroed_v_3_0(const void *entry)
{
	struct ipa_nat_hw_indx_entry zero_entry = { 0 };

	return (memcmp(&zero_entry, entry, sizeof(zero_entry))) ? false : true;
}

static bool ipa_nat_ipv4_is_pdn_entry_zeroed_v_4_0(const void *entry)
{
	struct ipa_nat_hw_pdn_entry zero_entry = { 0 };

	return (memcmp(&zero_entry, entry, sizeof(zero_entry))) ? false : true;
}

static bool ipa_nat_ipv6ct_is_entry_zeroed_v_4_0(const void *entry)
{
	struct ipa_nat_hw_ipv6ct_entry zero_entry = { 0 };

	return (memcmp(&zero_entry, entry, sizeof(zero_entry))) ? false : true;
}

static bool ipa_nat_ipv4_is_entry_valid_v_3_0(const void *entry)
{
	struct ipa_nat_hw_ipv4_entry *hw_entry =
		(struct ipa_nat_hw_ipv4_entry *)entry;

	return hw_entry->enable &&
		hw_entry->protocol != IPAHAL_NAT_INVALID_PROTOCOL;
}

static bool ipa_nat_ipv4_is_index_entry_valid_v_3_0(const void *entry)
{
	struct ipa_nat_hw_indx_entry *hw_entry =
		(struct ipa_nat_hw_indx_entry *)entry;

	return hw_entry->tbl_entry != 0;
}

static bool ipa_nat_ipv4_is_pdn_entry_valid_v_4_0(const void *entry)
{
	struct ipa_nat_hw_pdn_entry *hw_entry =
		(struct ipa_nat_hw_pdn_entry *)entry;

	return hw_entry->public_ip != 0;
}

static bool ipa_nat_ipv6ct_is_entry_valid_v_4_0(const void *entry)
{
	struct ipa_nat_hw_ipv6ct_entry *hw_entry =
		(struct ipa_nat_hw_ipv6ct_entry *)entry;

	return hw_entry->enable &&
		hw_entry->protocol != IPAHAL_NAT_INVALID_PROTOCOL;
}

static int ipa_nat_ipv4_stringify_entry_v_3_0(const void *entry,
	char *buff, size_t buff_size)
{
	const struct ipa_nat_hw_ipv4_entry *nat_entry =
		(const struct ipa_nat_hw_ipv4_entry *)entry;

	return scnprintf(buff, buff_size,
		"\t\tPrivate_IP=%pI4h  Target_IP=%pI4h\n"
		"\t\tNext_Index=%d  Public_Port=%d\n"
		"\t\tPrivate_Port=%d  Target_Port=%d\n"
		"\t\tIP_CKSM_delta=0x%x  Enable=%s  Redirect=%s\n"
		"\t\tTime_stamp=0x%x Proto=%d\n"
		"\t\tPrev_Index=%d  Indx_tbl_entry=%d\n"
		"\t\tTCP_UDP_cksum_delta=0x%x\n",
		&nat_entry->private_ip, &nat_entry->target_ip,
		nat_entry->next_index, nat_entry->public_port,
		nat_entry->private_port, nat_entry->target_port,
		nat_entry->ip_chksum,
		(nat_entry->enable) ? "true" : "false",
		(nat_entry->redirect) ? "Direct_To_APPS" : "Fwd_to_route",
		nat_entry->time_stamp, nat_entry->protocol,
		nat_entry->prev_index, nat_entry->indx_tbl_entry,
		nat_entry->tcp_udp_chksum);
}

static int ipa_nat_ipv4_stringify_entry_v_4_0(const void *entry,
	char *buff, size_t buff_size)
{
	int length;
	const struct ipa_nat_hw_ipv4_entry *nat_entry =
		(const struct ipa_nat_hw_ipv4_entry *)entry;

	length = ipa_nat_ipv4_stringify_entry_v_3_0(entry, buff, buff_size);

	length += scnprintf(buff + length, buff_size - length,
		"\t\tPDN_Index=%d\n", nat_entry->pdn_index);

	return length;
}

static int ipa_nat_ipv4_index_stringify_entry_v_3_0(const void *entry,
	char *buff, size_t buff_size)
{
	const struct ipa_nat_hw_indx_entry *index_entry =
		(const struct ipa_nat_hw_indx_entry *)entry;

	return scnprintf(buff, buff_size,
		"\t\tTable_Entry=%d  Next_Index=%d\n",
		index_entry->tbl_entry, index_entry->next_index);
}

static int ipa_nat_ipv4_pdn_stringify_entry_v_4_0(const void *entry,
	char *buff, size_t buff_size)
{
	const struct ipa_nat_hw_pdn_entry *pdn_entry =
		(const struct ipa_nat_hw_pdn_entry *)entry;

	return scnprintf(buff, buff_size,
		"ip=%pI4h src_metadata=0x%X, dst_metadata=0x%X\n",
		&pdn_entry->public_ip,
		pdn_entry->src_metadata, pdn_entry->dst_metadata);
}

static inline int ipa_nat_ipv6_stringify_addr(char *buff, size_t buff_size,
	const char *msg, u64 lsb, u64 msb)
{
	struct in6_addr addr;

	addr.s6_addr32[0] = cpu_to_be32((msb & IPA_64_HIGH_32_MASK) >> 32);
	addr.s6_addr32[1] = cpu_to_be32(msb & IPA_64_LOW_32_MASK);
	addr.s6_addr32[2] = cpu_to_be32((lsb & IPA_64_HIGH_32_MASK) >> 32);
	addr.s6_addr32[3] = cpu_to_be32(lsb & IPA_64_LOW_32_MASK);

	return scnprintf(buff, buff_size,
		"\t\t%s_IPv6_Addr=%pI6c\n", msg, &addr);
}

static int ipa_nat_ipv6ct_stringify_entry_v_4_0(const void *entry,
	char *buff, size_t buff_size)
{
	int length = 0;
	const struct ipa_nat_hw_ipv6ct_entry *ipv6ct_entry =
		(const struct ipa_nat_hw_ipv6ct_entry *)entry;

	length += ipa_nat_ipv6_stringify_addr(
		buff + length,
		buff_size - length,
		"Src",
		ipv6ct_entry->src_ipv6_lsb,
		ipv6ct_entry->src_ipv6_msb);

	length += ipa_nat_ipv6_stringify_addr(
		buff + length,
		buff_size - length,
		"Dest",
		ipv6ct_entry->dest_ipv6_lsb,
		ipv6ct_entry->dest_ipv6_msb);

	length += scnprintf(buff + length, buff_size - length,
		"\t\tEnable=%s Redirect=%s Time_Stamp=0x%x Proto=%d\n"
		"\t\tNext_Index=%d Dest_Port=%d Src_Port=%d\n"
		"\t\tDirection Settings: Out=%s In=%s\n"
		"\t\tPrev_Index=%d\n",
		(ipv6ct_entry->enable) ? "true" : "false",
		(ipv6ct_entry->redirect) ? "Direct_To_APPS" : "Fwd_to_route",
		ipv6ct_entry->time_stamp,
		ipv6ct_entry->protocol,
		ipv6ct_entry->next_index,
		ipv6ct_entry->dest_port,
		ipv6ct_entry->src_port,
		(ipv6ct_entry->out_allowed) ? "Allow" : "Deny",
		(ipv6ct_entry->in_allowed) ? "Allow" : "Deny",
		ipv6ct_entry->prev_index);

	return length;
}

static void ipa_nat_ipv4_pdn_construct_entry_v_4_0(const void *fields,
	u32 *address)
{
	const struct ipahal_nat_pdn_entry *pdn_entry =
		(const struct ipahal_nat_pdn_entry *)fields;

	struct ipa_nat_hw_pdn_entry *pdn_entry_address =
		(struct ipa_nat_hw_pdn_entry *)address;

	memset(pdn_entry_address, 0, sizeof(struct ipa_nat_hw_pdn_entry));

	pdn_entry_address->public_ip = pdn_entry->public_ip;
	pdn_entry_address->src_metadata = pdn_entry->src_metadata;
	pdn_entry_address->dst_metadata = pdn_entry->dst_metadata;
}

static void ipa_nat_ipv4_pdn_parse_entry_v_4_0(void *fields,
	const u32 *address)
{
	struct ipahal_nat_pdn_entry *pdn_entry =
		(struct ipahal_nat_pdn_entry *)fields;

	const struct ipa_nat_hw_pdn_entry *pdn_entry_address =
		(const struct ipa_nat_hw_pdn_entry *)address;

	pdn_entry->public_ip = pdn_entry_address->public_ip;
	pdn_entry->src_metadata = pdn_entry_address->src_metadata;
	pdn_entry->dst_metadata = pdn_entry_address->dst_metadata;
}

/*
 * struct ipahal_nat_obj - H/W information for specific IPA version
 * @entry_size - CB to get the size of the entry
 * @is_entry_zeroed - CB to determine whether an entry is definitely zero
 * @is_entry_valid - CB to determine whether an entry is valid
 *  Validity criterium depends on entry type. E.g. for NAT base table
 *   Entry need to be with valid protocol and enabled.
 * @stringify_entry - CB to create string that represents an entry
 * @construct_entry - CB to create NAT entry using the given fields
 * @parse_entry - CB to parse NAT entry to the given fields structure
 */
struct ipahal_nat_obj {
	size_t (*entry_size)(void);
	bool (*is_entry_zeroed)(const void *entry);
	bool (*is_entry_valid)(const void *entry);
	int (*stringify_entry)(const void *entry, char *buff, size_t buff_size);
	void (*construct_entry)(const void *fields, u32 *address);
	void (*parse_entry)(void *fields, const u32 *address);
};

/*
 * This table contains the info regard each NAT type for IPAv3 and later.
 * Information like: get entry size and stringify entry functions.
 * All the information on all the NAT types on IPAv3 are statically
 * defined below. If information is missing regard some NAT type on some
 * IPA version, the init function will fill it with the information from the
 * previous IPA version.
 * Information is considered missing if all of the fields are 0
 */
static struct ipahal_nat_obj ipahal_nat_objs[IPA_HW_MAX][IPA_NAT_MAX] = {
	/* IPAv3 */
	[IPA_HW_v3_0][IPAHAL_NAT_IPV4] = {
			ipa_nat_ipv4_entry_size_v_3_0,
			ipa_nat_ipv4_is_entry_zeroed_v_3_0,
			ipa_nat_ipv4_is_entry_valid_v_3_0,
			ipa_nat_ipv4_stringify_entry_v_3_0
		},
	[IPA_HW_v3_0][IPAHAL_NAT_IPV4_INDEX] = {
			ipa_nat_ipv4_index_entry_size_v_3_0,
			ipa_nat_ipv4_is_index_entry_zeroed_v_3_0,
			ipa_nat_ipv4_is_index_entry_valid_v_3_0,
			ipa_nat_ipv4_index_stringify_entry_v_3_0
		},

	/* IPAv4 */
	[IPA_HW_v4_0][IPAHAL_NAT_IPV4] = {
			ipa_nat_ipv4_entry_size_v_3_0,
			ipa_nat_ipv4_is_entry_zeroed_v_3_0,
			ipa_nat_ipv4_is_entry_valid_v_3_0,
			ipa_nat_ipv4_stringify_entry_v_4_0
		},
	[IPA_HW_v4_0][IPAHAL_NAT_IPV4_PDN] = {
			ipa_nat_ipv4_pdn_entry_size_v_4_0,
			ipa_nat_ipv4_is_pdn_entry_zeroed_v_4_0,
			ipa_nat_ipv4_is_pdn_entry_valid_v_4_0,
			ipa_nat_ipv4_pdn_stringify_entry_v_4_0,
			ipa_nat_ipv4_pdn_construct_entry_v_4_0,
			ipa_nat_ipv4_pdn_parse_entry_v_4_0
		},
	[IPA_HW_v4_0][IPAHAL_NAT_IPV6CT] = {
			ipa_nat_ipv6ct_entry_size_v_4_0,
			ipa_nat_ipv6ct_is_entry_zeroed_v_4_0,
			ipa_nat_ipv6ct_is_entry_valid_v_4_0,
			ipa_nat_ipv6ct_stringify_entry_v_4_0
		}
};

static void ipahal_nat_check_obj(struct ipahal_nat_obj *obj,
	int nat_type, int ver)
{
	WARN(obj->entry_size == NULL, "%s missing entry_size for version %d\n",
		ipahal_nat_type_str(nat_type), ver);
	WARN(obj->is_entry_zeroed == NULL,
		"%s missing is_entry_zeroed for version %d\n",
		ipahal_nat_type_str(nat_type), ver);
	WARN(obj->stringify_entry == NULL,
		"%s missing stringify_entry for version %d\n",
		ipahal_nat_type_str(nat_type), ver);
}

/*
 * ipahal_nat_init() - Build the NAT information table
 *  See ipahal_nat_objs[][] comments
 */
int ipahal_nat_init(enum ipa_hw_type ipa_hw_type)
{
	int i;
	int j;
	struct ipahal_nat_obj zero_obj, *next_obj;

	IPAHAL_DBG("Entry - HW_TYPE=%d\n", ipa_hw_type);

	memset(&zero_obj, 0, sizeof(zero_obj));

	if ((ipa_hw_type < 0) || (ipa_hw_type >= IPA_HW_MAX)) {
		IPAHAL_ERR("invalid IPA HW type (%d)\n", ipa_hw_type);
		return -EINVAL;
	}

	for (i = IPA_HW_v3_0 ; i < ipa_hw_type ; ++i) {
		for (j = 0; j < IPA_NAT_MAX; ++j) {
			next_obj = &ipahal_nat_objs[i + 1][j];
			if (!memcmp(next_obj, &zero_obj, sizeof(*next_obj))) {
				memcpy(next_obj, &ipahal_nat_objs[i][j],
					sizeof(*next_obj));
			} else {
				ipahal_nat_check_obj(next_obj, j, i + 1);
			}
		}
	}

	return 0;
}

const char *ipahal_nat_type_str(enum ipahal_nat_type nat_type)
{
	if (nat_type < 0 || nat_type >= IPA_NAT_MAX) {
		IPAHAL_ERR("requested NAT type %d is invalid\n", nat_type);
		return "Invalid NAT type";
	}

	return ipahal_nat_type_to_str[nat_type];
}

int ipahal_nat_entry_size(enum ipahal_nat_type nat_type, size_t *entry_size)
{
	if (WARN(entry_size == NULL, "entry_size is NULL\n"))
		return -EINVAL;
	if (WARN(nat_type < 0 || nat_type >= IPA_NAT_MAX,
		"requested NAT type %d is invalid\n", nat_type))
		return -EINVAL;

	IPAHAL_DBG("Get the entry size for NAT type=%s\n",
		ipahal_nat_type_str(nat_type));

	*entry_size =
		ipahal_nat_objs[ipahal_ctx->hw_type][nat_type].entry_size();

	IPAHAL_DBG("The entry size is %zu\n", *entry_size);

	return 0;
}

int ipahal_nat_is_entry_zeroed(enum ipahal_nat_type nat_type, void *entry,
	bool *entry_zeroed)
{
	struct ipahal_nat_obj *nat_ptr;

	if (WARN(entry == NULL || entry_zeroed == NULL,
		"NULL pointer received\n"))
		return -EINVAL;
	if (WARN(nat_type < 0 || nat_type >= IPA_NAT_MAX,
		"requested NAT type %d is invalid\n", nat_type))
		return -EINVAL;

	IPAHAL_DBG("Determine whether the entry is zeroed for NAT type=%s\n",
		ipahal_nat_type_str(nat_type));

	nat_ptr =
		&ipahal_nat_objs[ipahal_ctx->hw_type][nat_type];

	*entry_zeroed = nat_ptr->is_entry_zeroed(entry);

	IPAHAL_DBG("The entry is %szeroed\n", (*entry_zeroed) ? "" : "not ");

	return 0;
}

int ipahal_nat_is_entry_valid(enum ipahal_nat_type nat_type, void *entry,
	bool *entry_valid)
{
	struct ipahal_nat_obj *nat_obj;

	if (WARN(entry == NULL || entry_valid == NULL,
		"NULL pointer received\n"))
		return -EINVAL;
	if (WARN(nat_type < 0 || nat_type >= IPA_NAT_MAX,
		"requested NAT type %d is invalid\n", nat_type))
		return -EINVAL;

	IPAHAL_DBG("Determine whether the entry is valid for NAT type=%s\n",
		ipahal_nat_type_str(nat_type));
	nat_obj = &ipahal_nat_objs[ipahal_ctx->hw_type][nat_type];
	*entry_valid = nat_obj->is_entry_valid(entry);
	IPAHAL_DBG("The entry is %svalid\n", (*entry_valid) ? "" : "not ");

	return 0;
}

int ipahal_nat_stringify_entry(enum ipahal_nat_type nat_type, void *entry,
	char *buff, size_t buff_size)
{
	int result;
	struct ipahal_nat_obj *nat_obj_ptr;

	if (WARN(entry == NULL || buff == NULL, "NULL pointer received\n"))
		return -EINVAL;
	if (WARN(!buff_size, "The output buff size is zero\n"))
		return -EINVAL;
	if (WARN(nat_type < 0 || nat_type >= IPA_NAT_MAX,
		"requested NAT type %d is invalid\n", nat_type))
		return -EINVAL;

	nat_obj_ptr =
		&ipahal_nat_objs[ipahal_ctx->hw_type][nat_type];

	IPAHAL_DBG("Create the string for the entry of NAT type=%s\n",
		ipahal_nat_type_str(nat_type));

	result = nat_obj_ptr->stringify_entry(entry, buff, buff_size);

	IPAHAL_DBG("The string successfully created with length %d\n",
		result);

	return result;
}

int ipahal_nat_construct_entry(enum ipahal_nat_type nat_type,
	const void *fields,
	void *address)
{
	struct ipahal_nat_obj *nat_obj_ptr;

	if (WARN(address == NULL || fields == NULL, "NULL pointer received\n"))
		return -EINVAL;
	if (WARN(nat_type < 0 || nat_type >= IPA_NAT_MAX,
		"requested NAT type %d is invalid\n", nat_type))
		return -EINVAL;

	IPAHAL_DBG("Create %s entry using given fields\n",
		ipahal_nat_type_str(nat_type));

	nat_obj_ptr =
		&ipahal_nat_objs[ipahal_ctx->hw_type][nat_type];

	nat_obj_ptr->construct_entry(fields, address);

	return 0;
}

int ipahal_nat_parse_entry(enum ipahal_nat_type nat_type, void *fields,
	const void *address)
{
	struct ipahal_nat_obj *nat_obj_ptr;

	if (WARN(address == NULL || fields == NULL, "NULL pointer received\n"))
		return -EINVAL;
	if (WARN(nat_type < 0 || nat_type >= IPA_NAT_MAX,
		"requested NAT type %d is invalid\n", nat_type))
		return -EINVAL;

	IPAHAL_DBG("Get the parsed values for NAT type=%s\n",
		ipahal_nat_type_str(nat_type));

	nat_obj_ptr =
		&ipahal_nat_objs[ipahal_ctx->hw_type][nat_type];

	nat_obj_ptr->parse_entry(fields, address);

	return 0;
}
