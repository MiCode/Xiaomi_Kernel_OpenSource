/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/msm_ipa.h>
#include "ipa_i.h"
#include "ipa_emulation_stubs.h"

# undef strsame
# define strsame(x, y) \
	(!strcmp((x), (y)))

/*
 * The following enum values used to index tables below.
 */
enum dtsi_index_e {
	DTSI_INDEX_3_5_1 = 0,
	DTSI_INDEX_4_0   = 1,
};

struct dtsi_replacement_u32 {
	char *key;
	u32 value;
};

struct dtsi_replacement_u32_table {
	struct dtsi_replacement_u32 *p_table;
	u32 num_entries;
};

struct dtsi_replacement_bool {
	char *key;
	bool value;
};

struct dtsi_replacement_bool_table {
	struct dtsi_replacement_bool *p_table;
	u32 num_entries;
};

struct dtsi_replacement_u32_array {
	char *key;
	u32 *p_value;
	u32 num_elements;
};

struct dtsi_replacement_u32_array_table {
	struct dtsi_replacement_u32_array *p_table;
	u32 num_entries;
};

struct dtsi_replacement_resource_table {
	struct resource *p_table;
	u32 num_entries;
};

/*
 * Any of the data below with _4_0 in the name represent data taken
 * from the 4.0 dtsi file.
 *
 * Any of the data below with _3_5_1 in the name represent data taken
 * from the 3.5.1 dtsi file.
 */
static struct dtsi_replacement_bool ipa3_plat_drv_bool_4_0[] = {
	{"qcom,use-ipa-tethering-bridge",       true},
	{"qcom,modem-cfg-emb-pipe-flt",         true},
	{"qcom,ipa-wdi2",                       true},
	{"qcom,use-64-bit-dma-mask",            false},
	{"qcom,bandwidth-vote-for-ipa",         false},
	{"qcom,skip-uc-pipe-reset",             false},
	{"qcom,tethered-flow-control",          true},
	{"qcom,use-rg10-limitation-mitigation", false},
	{"qcom,do-not-use-ch-gsi-20",           false},
	{"qcom,use-ipa-pm",                     false},
};

static struct dtsi_replacement_bool ipa3_plat_drv_bool_3_5_1[] = {
	{"qcom,use-ipa-tethering-bridge",       true},
	{"qcom,modem-cfg-emb-pipe-flt",         true},
	{"qcom,ipa-wdi2",                       true},
	{"qcom,use-64-bit-dma-mask",            false},
	{"qcom,bandwidth-vote-for-ipa",         true},
	{"qcom,skip-uc-pipe-reset",             false},
	{"qcom,tethered-flow-control",          false},
	{"qcom,use-rg10-limitation-mitigation", false},
	{"qcom,do-not-use-ch-gsi-20",           false},
	{"qcom,use-ipa-pm",                     false},
};

static struct dtsi_replacement_bool_table
ipa3_plat_drv_bool_table[] = {
	{ ipa3_plat_drv_bool_3_5_1,
	  ARRAY_SIZE(ipa3_plat_drv_bool_3_5_1) },
	{ ipa3_plat_drv_bool_4_0,
	  ARRAY_SIZE(ipa3_plat_drv_bool_4_0) },
};

static struct dtsi_replacement_u32 ipa3_plat_drv_u32_4_0[] = {
	{"qcom,ipa-hw-ver",                     IPA_HW_v4_0},
	{"qcom,ipa-hw-mode",                    3},
	{"qcom,wan-rx-ring-size",               192},
	{"qcom,lan-rx-ring-size",               192},
	{"qcom,ee",                             0},
	{"emulator-bar0-offset",                0x01C00000},
};

static struct dtsi_replacement_u32 ipa3_plat_drv_u32_3_5_1[] = {
	{"qcom,ipa-hw-ver",                     IPA_HW_v3_5_1},
	{"qcom,ipa-hw-mode",                    3},
	{"qcom,wan-rx-ring-size",               192},
	{"qcom,lan-rx-ring-size",               192},
	{"qcom,ee",                             0},
	{"emulator-bar0-offset",                0x01C00000},
};

static struct dtsi_replacement_u32_table ipa3_plat_drv_u32_table[] = {
	{ ipa3_plat_drv_u32_3_5_1,
	  ARRAY_SIZE(ipa3_plat_drv_u32_3_5_1) },
	{ ipa3_plat_drv_u32_4_0,
	  ARRAY_SIZE(ipa3_plat_drv_u32_4_0) },
};

static u32 mhi_event_ring_id_limits_array_4_0[] = {
	9, 10
};

static u32 mhi_event_ring_id_limits_array_3_5_1[] = {
	IPA_MHI_GSI_EVENT_RING_ID_START, IPA_MHI_GSI_EVENT_RING_ID_END
};

static u32 ipa_tz_unlock_reg_array_4_0[] = {
	0x04043583c, 0x00001000
};

static u32 ipa_tz_unlock_reg_array_3_5_1[] = {
	0x04043583c, 0x00001000
};

static u32 ipa_ram_mmap_array_4_0[] = {
	0x00000280, 0x00000000, 0x00000000, 0x00000288, 0x00000078,
	0x00004000, 0x00000308, 0x00000078, 0x00004000, 0x00000388,
	0x00000078, 0x00004000, 0x00000408, 0x00000078, 0x00004000,
	0x0000000F, 0x00000000, 0x00000007, 0x00000008, 0x0000000E,
	0x00000488, 0x00000078, 0x00004000, 0x00000508, 0x00000078,
	0x00004000, 0x0000000F, 0x00000000, 0x00000007, 0x00000008,
	0x0000000E, 0x00000588, 0x00000078, 0x00004000, 0x00000608,
	0x00000078, 0x00004000, 0x00000688, 0x00000140, 0x000007C8,
	0x00000000, 0x00000800, 0x000007D0, 0x00000200, 0x000009D0,
	0x00000200, 0x00000000, 0x00000000, 0x00000000, 0x000013F0,
	0x0000100C, 0x000023FC, 0x00000000, 0x000023FC, 0x00000000,
	0x000023FC, 0x00000000, 0x000023FC, 0x00000000, 0x00000080,
	0x00000200, 0x00002800, 0x000023FC, 0x00000000, 0x000023FC,
	0x00000000, 0x000023FC, 0x00000000, 0x000023FC, 0x00000000,
	0x00002400, 0x00000400, 0x00000BD8, 0x00000050, 0x00000C30,
	0x00000060, 0x00000C90, 0x00000140, 0x00000DD0, 0x00000180,
	0x00000F50, 0x00000180, 0x000010D0, 0x00000180, 0x00001250,
	0x00000180, 0x000013D0, 0x00000020
};

static u32 ipa_ram_mmap_array_3_5_1[] = {
	0x00000280, 0x00000000, 0x00000000, 0x00000288, 0x00000078,
	0x00004000, 0x00000308, 0x00000078, 0x00004000, 0x00000388,
	0x00000078, 0x00004000, 0x00000408, 0x00000078, 0x00004000,
	0x0000000F, 0x00000000, 0x00000007, 0x00000008, 0x0000000E,
	0x00000488, 0x00000078, 0x00004000, 0x00000508, 0x00000078,
	0x00004000, 0x0000000F, 0x00000000, 0x00000007, 0x00000008,
	0x0000000E, 0x00000588, 0x00000078, 0x00004000, 0x00000608,
	0x00000078, 0x00004000, 0x00000688, 0x00000140, 0x000007C8,
	0x00000000, 0x00000800, 0x000007D0, 0x00000200, 0x000009D0,
	0x00000200, 0x00000000, 0x00000000, 0x00000000, 0x00000BD8,
	0x00001024, 0x00002000, 0x00000000, 0x00002000, 0x00000000,
	0x00002000, 0x00000000, 0x00002000, 0x00000000, 0x00000080,
	0x00000200, 0x00002000, 0x00002000, 0x00000000, 0x00002000,
	0x00000000, 0x00002000, 0x00000000, 0x00002000, 0x00000000,
	0x00001C00, 0x00000400
};

struct dtsi_replacement_u32_array ipa3_plat_drv_u32_array_4_0[] = {
	{"qcom,mhi-event-ring-id-limits",
	 mhi_event_ring_id_limits_array_4_0,
	 ARRAY_SIZE(mhi_event_ring_id_limits_array_4_0) },
	{"qcom,ipa-tz-unlock-reg",
	 ipa_tz_unlock_reg_array_4_0,
	 ARRAY_SIZE(ipa_tz_unlock_reg_array_4_0) },
	{"qcom,ipa-ram-mmap",
	 ipa_ram_mmap_array_4_0,
	 ARRAY_SIZE(ipa_ram_mmap_array_4_0) },
};

struct dtsi_replacement_u32_array ipa3_plat_drv_u32_array_3_5_1[] = {
	{"qcom,mhi-event-ring-id-limits",
	 mhi_event_ring_id_limits_array_3_5_1,
	 ARRAY_SIZE(mhi_event_ring_id_limits_array_3_5_1) },
	{"qcom,ipa-tz-unlock-reg",
	 ipa_tz_unlock_reg_array_3_5_1,
	 ARRAY_SIZE(ipa_tz_unlock_reg_array_3_5_1) },
	{"qcom,ipa-ram-mmap",
	 ipa_ram_mmap_array_3_5_1,
	 ARRAY_SIZE(ipa_ram_mmap_array_3_5_1) },
};

struct dtsi_replacement_u32_array_table
ipa3_plat_drv_u32_array_table[] = {
	{ ipa3_plat_drv_u32_array_3_5_1,
	  ARRAY_SIZE(ipa3_plat_drv_u32_array_3_5_1) },
	{ ipa3_plat_drv_u32_array_4_0,
	  ARRAY_SIZE(ipa3_plat_drv_u32_array_4_0) },
};

#define INTCTRL_OFFSET       0x083C0000
#define INTCTRL_SIZE         0x00000110

#define IPA_BASE_OFFSET_4_0  0x01e00000
#define IPA_BASE_SIZE_4_0    0x00034000
#define GSI_BASE_OFFSET_4_0  0x01e04000
#define GSI_BASE_SIZE_4_0    0x00028000

struct resource ipa3_plat_drv_resource_4_0[] = {
	/*
	 * PLEASE NOTE WELL: The following offset values below
	 * ("ipa-base", "gsi-base", and "intctrl-base") are used to
	 * calculate offsets relative to the PCI BAR0 address provided
	 * by the PCI probe.  After their use to calculate the
	 * offsets, they are not used again, since PCI ultimately
	 * dictates where things live.
	 */
	{
		IPA_BASE_OFFSET_4_0,
		(IPA_BASE_OFFSET_4_0 + IPA_BASE_SIZE_4_0),
		"ipa-base",
		IORESOURCE_MEM,
		0,
		NULL,
		NULL,
		NULL
	},

	{
		GSI_BASE_OFFSET_4_0,
		(GSI_BASE_OFFSET_4_0 + GSI_BASE_SIZE_4_0),
		"gsi-base",
		IORESOURCE_MEM,
		0,
		NULL,
		NULL,
		NULL
	},

	/*
	 * The following entry is germane only to the emulator
	 * environment.  It is needed to locate the emulator's PCI
	 * interrupt controller...
	 */
	{
		INTCTRL_OFFSET,
		(INTCTRL_OFFSET + INTCTRL_SIZE),
		"intctrl-base",
		IORESOURCE_MEM,
		0,
		NULL,
		NULL,
		NULL
	},

	{
		IPA_PIPE_MEM_START_OFST,
		(IPA_PIPE_MEM_START_OFST + IPA_PIPE_MEM_SIZE),
		"ipa-pipe-mem",
		IORESOURCE_MEM,
		0,
		NULL,
		NULL,
		NULL
	},

	{
		0,
		0,
		"gsi-irq",
		IORESOURCE_IRQ,
		0,
		NULL,
		NULL,
		NULL
	},

	{
		0,
		0,
		"ipa-irq",
		IORESOURCE_IRQ,
		0,
		NULL,
		NULL,
		NULL
	},
};

#define IPA_BASE_OFFSET_3_5_1  0x01e00000
#define IPA_BASE_SIZE_3_5_1    0x00034000
#define GSI_BASE_OFFSET_3_5_1  0x01e04000
#define GSI_BASE_SIZE_3_5_1    0x0002c000

struct resource ipa3_plat_drv_resource_3_5_1[] = {
	/*
	 * PLEASE NOTE WELL: The following offset values below
	 * ("ipa-base", "gsi-base", and "intctrl-base") are used to
	 * calculate offsets relative to the PCI BAR0 address provided
	 * by the PCI probe.  After their use to calculate the
	 * offsets, they are not used again, since PCI ultimately
	 * dictates where things live.
	 */
	{
		IPA_BASE_OFFSET_3_5_1,
		(IPA_BASE_OFFSET_3_5_1 + IPA_BASE_SIZE_3_5_1),
		"ipa-base",
		IORESOURCE_MEM,
		0,
		NULL,
		NULL,
		NULL
	},

	{
		GSI_BASE_OFFSET_3_5_1,
		(GSI_BASE_OFFSET_3_5_1 + GSI_BASE_SIZE_3_5_1),
		"gsi-base",
		IORESOURCE_MEM,
		0,
		NULL,
		NULL,
		NULL
	},

	/*
	 * The following entry is germane only to the emulator
	 * environment.  It is needed to locate the emulator's PCI
	 * interrupt controller...
	 */
	{
		INTCTRL_OFFSET,
		(INTCTRL_OFFSET + INTCTRL_SIZE),
		"intctrl-base",
		IORESOURCE_MEM,
		0,
		NULL,
		NULL,
		NULL
	},

	{
		IPA_PIPE_MEM_START_OFST,
		(IPA_PIPE_MEM_START_OFST + IPA_PIPE_MEM_SIZE),
		"ipa-pipe-mem",
		IORESOURCE_MEM,
		0,
		NULL,
		NULL,
		NULL
	},

	{
		0,
		0,
		"gsi-irq",
		IORESOURCE_IRQ,
		0,
		NULL,
		NULL,
		NULL
	},

	{
		0,
		0,
		"ipa-irq",
		IORESOURCE_IRQ,
		0,
		NULL,
		NULL,
		NULL
	},
};

struct dtsi_replacement_resource_table
ipa3_plat_drv_resource_table[] = {
	{ ipa3_plat_drv_resource_3_5_1,
	  ARRAY_SIZE(ipa3_plat_drv_resource_3_5_1) },
	{ ipa3_plat_drv_resource_4_0,
	  ARRAY_SIZE(ipa3_plat_drv_resource_4_0) },
};

/*
 * The following code uses the data above...
 */
static u32 emulator_type_to_index(void)
{
	/*
	 * Use the input parameter to the IPA driver loadable module,
	 * which specifies the type of hardware the driver is running
	 * on.
	 */
	u32 index = DTSI_INDEX_4_0;
	uint emulation_type = ipa3_get_emulation_type();

	switch (emulation_type) {
	case IPA_HW_v3_5_1:
		index = DTSI_INDEX_3_5_1;
		break;
	case IPA_HW_v4_0:
		index = DTSI_INDEX_4_0;
		break;
	default:
		break;
	}

	IPADBG("emulation_type(%u) emulation_index(%u)\n",
	       emulation_type, index);

	return index;
}

/* From include/linux/of.h */
/**
 * emulator_of_property_read_bool - Find from a property
 * @np:         device node from which the property value is to be read.
 * @propname:   name of the property to be searched.
 *
 * Search for a property in a device node.
 * Returns true if the property exists false otherwise.
 */
bool emulator_of_property_read_bool(
	const struct device_node *np,
	const char *propname)
{
	u16 i;
	u32 index;
	struct dtsi_replacement_bool *ipa3_plat_drv_boolP;

	/*
	 * Get the index for the type of hardware we're running on.
	 * This is used as a table index.
	 */
	index = emulator_type_to_index();
	if (index >= ARRAY_SIZE(ipa3_plat_drv_bool_table)) {
		IPADBG(
		    "Did not find ipa3_plat_drv_bool_table for index %u\n",
		    index);
		return false;
	}

	ipa3_plat_drv_boolP =
	    ipa3_plat_drv_bool_table[index].p_table;

	for (i = 0;
	     i < ipa3_plat_drv_bool_table[index].num_entries;
	     i++) {
		if (strsame(ipa3_plat_drv_boolP[i].key, propname)) {
			IPADBG(
			    "Found value %u for propname %s index %u\n",
			    ipa3_plat_drv_boolP[i].value,
			    propname,
			    index);
			return ipa3_plat_drv_boolP[i].value;
		}
	}

	IPADBG("Did not find match for propname %s index %u\n",
	       propname,
	       index);

	return false;
}

/* From include/linux/of.h */
int emulator_of_property_read_u32(
	const struct device_node *np,
	const char *propname,
	u32 *out_value)
{
	u16 i;
	u32 index;
	struct dtsi_replacement_u32 *ipa3_plat_drv_u32P;

	/*
	 * Get the index for the type of hardware we're running on.
	 * This is used as a table index.
	 */
	index = emulator_type_to_index();
	if (index >= ARRAY_SIZE(ipa3_plat_drv_u32_table)) {
		IPADBG(
		    "Did not find ipa3_plat_drv_u32_table for index %u\n",
		    index);
		return false;
	}

	ipa3_plat_drv_u32P =
	    ipa3_plat_drv_u32_table[index].p_table;

	for (i = 0;
	     i < ipa3_plat_drv_u32_table[index].num_entries;
	     i++) {
		if (strsame(ipa3_plat_drv_u32P[i].key, propname)) {
			*out_value = ipa3_plat_drv_u32P[i].value;
			IPADBG(
			    "Found value %u for propname %s index %u\n",
			    ipa3_plat_drv_u32P[i].value,
			    propname,
			    index);
			return 0;
		}
	}

	IPADBG("Did not find match for propname %s index %u\n",
	       propname,
	       index);

	return -EINVAL;
}

/* From include/linux/of.h */
/**
 * emulator_of_property_read_u32_array - Find and read an array of 32
 * bit integers from a property.
 *
 * @np:         device node from which the property value is to be read.
 * @propname:   name of the property to be searched.
 * @out_values: pointer to return value, modified only if return value is 0.
 * @sz:         number of array elements to read
 *
 * Search for a property in a device node and read 32-bit value(s) from
 * it. Returns 0 on success, -EINVAL if the property does not exist,
 * -ENODATA if property does not have a value, and -EOVERFLOW if the
 * property data isn't large enough.
 *
 * The out_values is modified only if a valid u32 value can be decoded.
 */
int emulator_of_property_read_u32_array(
	const struct device_node *np,
	const char *propname,
	u32 *out_values,
	size_t sz)
{
	u16 i;
	u32 index;
	struct dtsi_replacement_u32_array *u32_arrayP;

	/*
	 * Get the index for the type of hardware we're running on.
	 * This is used as a table index.
	 */
	index = emulator_type_to_index();
	if (index >= ARRAY_SIZE(ipa3_plat_drv_u32_array_table)) {
		IPADBG(
		    "Did not find ipa3_plat_drv_u32_array_table for index %u\n",
		    index);
		return false;
	}

	u32_arrayP =
		ipa3_plat_drv_u32_array_table[index].p_table;
	for (i = 0;
	     i < ipa3_plat_drv_u32_array_table[index].num_entries;
	     i++) {
		if (strsame(
			u32_arrayP[i].key, propname)) {
			u32 num_elements =
			    u32_arrayP[i].num_elements;
			u32 *p_element =
			    &u32_arrayP[i].p_value[0];
			size_t j = 0;

			if (num_elements > sz) {
				IPAERR(
				    "Found array of %u values for propname %s; only room for %u elements in copy buffer\n",
				    num_elements,
				    propname,
				    (unsigned int) sz);
				return -EOVERFLOW;
			}

			while (j++ < num_elements)
				*out_values++ = *p_element++;

			IPADBG(
			    "Found array of values starting with %u for propname %s index %u\n",
			    u32_arrayP[i].p_value[0],
			    propname,
			    index);

			return 0;
		}
	}

	IPADBG("Did not find match for propname %s index %u\n",
	       propname,
	       index);

	return -EINVAL;
}

/* From drivers/base/platform.c */
/**
 * emulator_platform_get_resource_byname - get a resource for a device by name
 * @dev: platform device
 * @type: resource type
 * @name: resource name
 */
struct resource *emulator_platform_get_resource_byname(
	struct platform_device *dev,
	unsigned int type,
	const char *name)
{
	u16 i;
	u32 index;
	struct resource *ipa3_plat_drv_resourceP;

	/*
	 * Get the index for the type of hardware we're running on.
	 * This is used as a table index.
	 */
	index = emulator_type_to_index();
	if (index >= ARRAY_SIZE(ipa3_plat_drv_resource_table)) {
		IPADBG(
		    "Did not find ipa3_plat_drv_resource_table for index %u\n",
		    index);
		return false;
	}

	ipa3_plat_drv_resourceP =
		ipa3_plat_drv_resource_table[index].p_table;
	for (i = 0;
	     i < ipa3_plat_drv_resource_table[index].num_entries;
	     i++) {
		struct resource *r = &ipa3_plat_drv_resourceP[i];

		if (type == resource_type(r) && strsame(r->name, name)) {
			IPADBG(
			    "Found start 0x%x size %u for name %s index %u\n",
			    (unsigned int) (r->start),
			    (unsigned int) (resource_size(r)),
			    name,
			    index);
			return r;
		}
	}

	IPADBG("Did not find match for name %s index %u\n",
	       name,
	       index);

	return NULL;
}

/* From drivers/of/base.c */
/**
 * emulator_of_property_count_elems_of_size - Count the number of
 * elements in a property
 *
 * @np:         device node from which the property value is to
 *              be read. Not used.
 * @propname:   name of the property to be searched.
 * @elem_size:  size of the individual element
 *
 * Search for a property and count the number of elements of size
 * elem_size in it. Returns number of elements on success, -EINVAL if
 * the property does not exist or its length does not match a multiple
 * of elem_size and -ENODATA if the property does not have a value.
 */
int emulator_of_property_count_elems_of_size(
	const struct device_node *np,
	const char *propname,
	int elem_size)
{
	u32 index;

	/*
	 * Get the index for the type of hardware we're running on.
	 * This is used as a table index.
	 */
	index = emulator_type_to_index();

	/*
	 * Use elem_size to determine which table to search for the
	 * specified property name
	 */
	if (elem_size == sizeof(u32)) {
		u16 i;
		struct dtsi_replacement_u32_array *u32_arrayP;

		if (index >= ARRAY_SIZE(ipa3_plat_drv_u32_array_table)) {
			IPADBG(
			    "Did not find ipa3_plat_drv_u32_array_table for index %u\n",
			    index);
			return false;
		}

		u32_arrayP =
			ipa3_plat_drv_u32_array_table[index].p_table;

		for (i = 0;
		     i < ipa3_plat_drv_u32_array_table[index].num_entries;
		     i++) {
			if (strsame(u32_arrayP[i].key, propname)) {
				if (u32_arrayP[i].p_value == NULL) {
					IPADBG(
					    "Found no elements for propname %s index %u\n",
					    propname,
					    index);
					return -ENODATA;
				}

				IPADBG(
				    "Found %u elements for propname %s index %u\n",
				    u32_arrayP[i].num_elements,
				    propname,
				    index);

				return u32_arrayP[i].num_elements;
			}
		}

		IPADBG(
		    "Found no match in table with elem_size %d for propname %s index %u\n",
		    elem_size,
		    propname,
		    index);

		return -EINVAL;
	}

	IPAERR(
	    "Found no tables with element size %u to search for propname %s index %u\n",
	    elem_size,
	    propname,
	    index);

	return -EINVAL;
}

int emulator_of_property_read_variable_u32_array(
	const struct device_node *np,
	const char *propname,
	u32 *out_values,
	size_t sz_min,
	size_t sz_max)
{
	return emulator_of_property_read_u32_array(
	    np, propname, out_values, sz_max);
}

resource_size_t emulator_resource_size(const struct resource *res)
{
	return res->end - res->start;
}
