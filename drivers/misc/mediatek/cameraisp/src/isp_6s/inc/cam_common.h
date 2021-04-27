/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#define IS_MT6893(id) ((id) == 0x6893)
#define IS_MT6885(id) ((id) == 0x6885)
#define IS_MT6877(id) ((id) == 0x6877)
#define IS_MT6873(id) ((id) == 0x6873)
#define IS_MT6853(id) ((id) == 0x6853)
#define IS_MT6833(id) ((id) == 0x6833)
#define IS_MT6781(id) ((id) == 0x6781)

/* Get platform id from dts node with "compatible_name".
 * The platform id is got from the member, "mediatek,platform".
 */
#define GET_PLATFORM_ID(compatible_name) ({					\
	struct device_node *dev_node = NULL;					\
	const char * platform_id_str;						\
	unsigned int platform_id = 0x0;						\
										\
	dev_node = of_find_compatible_node(NULL, NULL, compatible_name);	\
	if (!dev_node) {							\
		LOG_NOTICE("Found no %s\n", compatible_name);			\
	} else {								\
		if (of_property_read_string(dev_node, "mediatek,platform",	\
			&platform_id_str) < 0) {				\
			LOG_NOTICE("no mediatek,platform name\n");		\
		} else {							\
			if (strncmp(platform_id_str, "mt6893", 6) == 0)		\
				platform_id = 0x6893;				\
			else if (strncmp(platform_id_str, "mt6885", 6) == 0)	\
				platform_id = 0x6885;				\
			else if (strncmp(platform_id_str, "mt6877", 6) == 0)	\
				platform_id = 0x6877;				\
			else if (strncmp(platform_id_str, "mt6873", 6) == 0)	\
				platform_id = 0x6873;				\
			else if (strncmp(platform_id_str, "mt6853", 6) == 0)	\
				platform_id = 0x6853;				\
			else if (strncmp(platform_id_str, "mt6833", 6) == 0)	\
				platform_id = 0x6833;				\
			else if (strncmp(platform_id_str, "mt6781", 6) == 0)	\
				platform_id = 0x6781;				\
		}								\
	}									\
	platform_id;								\
})