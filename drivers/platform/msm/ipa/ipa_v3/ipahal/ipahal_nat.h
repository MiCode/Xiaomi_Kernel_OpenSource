/* Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
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

#endif /* _IPAHAL_NAT_H_ */
