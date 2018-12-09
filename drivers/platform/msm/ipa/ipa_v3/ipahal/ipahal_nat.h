/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#ifndef _IPAHAL_NAT_H_
#define _IPAHAL_NAT_H_

/*
 * NAT types
 *
 * NOTE:: Any change to this enum, need to change to ipahal_nat_to_str
 *	  array as well.
 */
enum ipahal_nat_type {
	IPAHAL_NAT_IPV4,
	IPAHAL_NAT_IPV4_INDEX,
	IPAHAL_NAT_IPV4_PDN,
	IPAHAL_NAT_IPV6CT,
	IPA_NAT_MAX
};

/**
 * struct ipahal_nat_pdn_entry - IPA PDN config table entry
 * @public_ip: the PDN's public ip
 * @src_metadata: the PDN's metadata to be replaced for source NAT
 * @dst_metadata: the PDN's metadata to be replaced for destination NAT
 */
struct ipahal_nat_pdn_entry {
	u32 public_ip;
	u32 src_metadata;
	u32 dst_metadata;
};

/* NAT Function APIs */

/*
 * ipahal_nat_type_str() - returns string that represent the NAT type
 * @nat_type: [in] NAT type
 */
const char *ipahal_nat_type_str(enum ipahal_nat_type nat_type);

/*
 * ipahal_nat_entry_size() - Gets the size of HW NAT entry
 * @nat_type: [in] The type of the NAT entry
 * @entry_size: [out] The size of the HW NAT entry
 */
int ipahal_nat_entry_size(enum ipahal_nat_type nat_type, size_t *entry_size);

/*
 * ipahal_nat_is_entry_zeroed() - Determines whether HW NAT entry is
 *                                definitely zero
 * @nat_type: [in] The type of the NAT entry
 * @entry: [in] The NAT entry
 * @entry_zeroed: [out] True if the received entry is definitely zero
 */
int ipahal_nat_is_entry_zeroed(enum ipahal_nat_type nat_type, void *entry,
	bool *entry_zeroed);

/*
 * ipahal_nat_is_entry_valid() - Determines whether HW NAT entry is
 *                                valid.
 *  Validity criterium depends on entry type. E.g. for NAT base table
 *   Entry need to be with valid protocol and enabled.
 * @nat_type: [in] The type of the NAT entry
 * @entry: [in] The NAT entry
 * @entry_valid: [out] True if the received entry is valid
 */
int ipahal_nat_is_entry_valid(enum ipahal_nat_type nat_type, void *entry,
	bool *entry_valid);

/*
 * ipahal_nat_stringify_entry() - Creates a string for HW NAT entry
 * @nat_type: [in] The type of the NAT entry
 * @entry: [in] The NAT entry
 * @buff: [out] Output buffer for the result string
 * @buff_size: [in] The size of the output buffer
 * @return the number of characters written into buff not including
 *         the trailing '\0'
 */
int ipahal_nat_stringify_entry(enum ipahal_nat_type nat_type, void *entry,
	char *buff, size_t buff_size);

/*
 * ipahal_nat_construct_entry() - Create NAT entry using the given fields
 * @nat_type: [in] The type of the NAT entry
 * @fields: [in] The fields need to be written in the entry
 * @address: [in] The address of the memory need to be written
 */
int ipahal_nat_construct_entry(enum ipahal_nat_type nat_type,
	void const *fields,
	void *address);

/*
 * ipahal_nat_parse_entry() - Parse NAT entry to the given fields structure
 * @nat_type: [in] The type of the NAT entry
 * @fields: [in] The fields need to be parsed from the entry
 * @address: [in] The address of the memory need to be parsed
 */
int ipahal_nat_parse_entry(enum ipahal_nat_type nat_type, void *fields,
	const void *address);

#endif /* _IPAHAL_NAT_H_ */
