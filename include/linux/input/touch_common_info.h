/*
 * Copyright (C) 2017 XiaoMi, Inc.
 */
#ifndef _TOUCH_COMMON_INFO_H_
#define _TOUCH_COMMON_INFO_H_

/*
 *vendor info array, vendor index and vendor name, if add new vendor info, need insert before 0xff
 */
struct touch_vendor_info {
	u8 vendor_id;
	char *vendor_name;
};
struct touch_vendor_info tv_info_array[] = {
	{0x31, "Biel"},
	{0x32, "Lens"},
	{0x34, "Ofilm"},
	{0x38, "Sharp"},
	{0x42, "Lg"},
	{0xff, "Unknown"}

#endif
