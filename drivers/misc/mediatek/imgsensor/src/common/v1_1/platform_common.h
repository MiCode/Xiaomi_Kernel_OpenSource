/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef __PLATFORM_COMMON_H__
#define __PLATFORM_COMMON_H__


#define IS_MT6893(id) ((id) == 0x6893)
#define IS_MT6885(id) ((id) == 0x6885)
#define IS_MT6877(id) ((id) == 0x6877)
#define IS_MT6873(id) ((id) == 0x6873)
#define IS_MT6855(id) ((id) == 0x6855)
#define IS_MT6853(id) ((id) == 0x6853)
#define IS_MT6833(id) ((id) == 0x6833)
#define IS_MT6789(id) ((id) == 0x6789)
#define IS_MT6785(id) ((id) == 0x6785)
#define IS_MT6781(id) ((id) == 0x6781)
#define IS_MT6779(id) ((id) == 0x6779)
#define IS_MT6768(id) ((id) == 0x6768)
#define IS_MT6739(id) ((id) == 0x6739)

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
		PK_DBG("Found no %s\n", compatible_name);			\
	} else {								\
		if (of_property_read_string(dev_node, "mediatek,platform",	\
			&platform_id_str) < 0) {				\
			PK_DBG("no mediatek,platform name\n");                  \
		} else {							\
			PK_DBG("Found: %s\n", platform_id_str);                 \
			if (strncmp(platform_id_str, "mt6893", 6) == 0)		\
				platform_id = 0x6893;				\
			else if (strncmp(platform_id_str, "mt6885", 6) == 0)	\
				platform_id = 0x6885;				\
			else if (strncmp(platform_id_str, "mt6877", 6) == 0)	\
				platform_id = 0x6877;				\
			else if (strncmp(platform_id_str, "mt6873", 6) == 0)	\
				platform_id = 0x6873;				\
			else if (strncmp(platform_id_str, "mt6855", 6) == 0)	\
				platform_id = 0x6855;				\
			else if (strncmp(platform_id_str, "mt6853", 6) == 0)	\
				platform_id = 0x6853;				\
			else if (strncmp(platform_id_str, "mt6833", 6) == 0)	\
				platform_id = 0x6833;				\
			else if (strncmp(platform_id_str, "mt6789", 6) == 0)	\
				platform_id = 0x6789;				\
			else if (strncmp(platform_id_str, "mt6785", 6) == 0)	\
				platform_id = 0x6785;				\
			else if (strncmp(platform_id_str, "mt6781", 6) == 0)	\
				platform_id = 0x6781;				\
			else if (strncmp(platform_id_str, "mt6779", 6) == 0)	\
				platform_id = 0x6779;				\
			else if (strncmp(platform_id_str, "mt6768", 6) == 0)	\
				platform_id = 0x6768;				\
			else if (strncmp(platform_id_str, "mt6739", 6) == 0)	\
				platform_id = 0x6739;				\
		}								\
	}									\
	platform_id;								\
})

#define GET_SENINF_MAX_NUM_ID(compatible_name) ({				\
	struct device_node *dev_node_seninf_max_num = NULL;			\
	const char * seninf_max_num_id_str;					\
	unsigned int seninf_max_num_id = 0;					\
										\
	dev_node_seninf_max_num =                                               \
                of_find_compatible_node(NULL, NULL, compatible_name);           \
	if (!dev_node_seninf_max_num) {						\
		PK_DBG("Found no %s\n", compatible_name);			\
	} else {								\
		if (of_property_read_string(                                    \
                        dev_node_seninf_max_num, "mediatek,seninf_max_num",     \
			&seninf_max_num_id_str) < 0) {				\
			PK_DBG("no mediatek,seninf_max_num name\n");		\
		} else {							\
			PK_DBG("Found: %s\n", seninf_max_num_id_str);           \
			if (strcmp(seninf_max_num_id_str, "8") == 0)		\
				seninf_max_num_id = 8;				\
			else if (strcmp(seninf_max_num_id_str, "6") == 0)	\
				seninf_max_num_id = 6;				\
		}								\
	}									\
	seninf_max_num_id;							\
})

#define LENGTH_FOR_SNPRINTF 256
#endif