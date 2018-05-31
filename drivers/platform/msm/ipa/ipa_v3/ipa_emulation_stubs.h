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

#if !defined(_IPA_EMULATION_STUBS_H_)
# define _IPA_EMULATION_STUBS_H_

# define clk_get(x, y) ((struct clk *) -(MAX_ERRNO+1))
# define clk_put(x)                do { } while (0)
# define clk_prepare(x)            do { } while (0)
# define clk_enable(x)             do { } while (0)
# define clk_set_rate(x, y)        do { } while (0)
# define clk_disable_unprepare(x)  do { } while (0)

# define outer_flush_range(x, y)
# define __flush_dcache_area(x, y)
# define __cpuc_flush_dcache_area(x, y) __flush_dcache_area(x, y)

/* Point several API calls to these new EMULATION functions */
# define of_property_read_bool(np, propname)	 \
	emulator_of_property_read_bool(NULL, propname)
# define of_property_read_u32(np, propname, out_value)   \
	emulator_of_property_read_u32(NULL, propname, out_value)
# define of_property_read_u32_array(np, propname, out_values, sz)	\
	emulator_of_property_read_u32_array(NULL, propname, out_values, sz)
# define platform_get_resource_byname(dev, type, name) \
	emulator_platform_get_resource_byname(NULL, type, name)
# define of_property_count_elems_of_size(np, propname, elem_size) \
	emulator_of_property_count_elems_of_size(NULL, propname, elem_size)
# define of_property_read_variable_u32_array( \
	np, propname, out_values, sz_min, sz_max) \
	emulator_of_property_read_variable_u32_array( \
		NULL, propname, out_values, sz_min, sz_max)
# define resource_size(res) \
	emulator_resource_size(res)

/**
 * emulator_of_property_read_bool - Findfrom a property
 * @np:         device node used to find the property value. (not used)
 * @propname:   name of the property to be searched.
 *
 * Search for a property in a device node.
 * Returns true if the property exists false otherwise.
 */
bool emulator_of_property_read_bool(
	const struct device_node *np,
	const char *propname);

int emulator_of_property_read_u32(
	const struct device_node *np,
	const char *propname,
	u32 *out_value);

/**
 * emulator_of_property_read_u32_array - Find and read an array of 32
 * bit integers from a property.
 *
 * @np:         device node used to find the property value. (not used)
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
	size_t sz);

/**
 * emulator_platform_get_resource_byname - get a resource for a device
 * by name
 *
 * @dev: platform device
 * @type: resource type
 * @name: resource name
 */
struct resource *emulator_platform_get_resource_byname(
	struct platform_device *dev,
	unsigned int type,
	const char *name);

/**
 * emulator_of_property_count_elems_of_size - Count the number of
 * elements in a property
 *
 * @np:         device node used to find the property value. (not used)
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
	int elem_size);

int emulator_of_property_read_variable_u32_array(
	const struct device_node *np,
	const char *propname,
	u32 *out_values,
	size_t sz_min,
	size_t sz_max);

resource_size_t emulator_resource_size(
	const struct resource *res);

#endif /* #if !defined(_IPA_EMULATION_STUBS_H_) */
