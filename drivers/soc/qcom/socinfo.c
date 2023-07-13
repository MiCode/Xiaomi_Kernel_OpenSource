// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2009-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2017-2019, Linaro Ltd.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/soc/qcom/smem.h>
#include <linux/string.h>
#include <linux/sys_soc.h>
#include <linux/types.h>
#include <soc/qcom/socinfo.h>
#include <asm/unaligned.h>

/*
 * SoC version type with major number in the upper 16 bits and minor
 * number in the lower 16 bits.
 */
#define SOCINFO_MAJOR(ver) (((ver) >> 16) & 0xffff)
#define SOCINFO_MINOR(ver) ((ver) & 0xffff)
#define SOCINFO_VERSION(maj, min)  ((((maj) & 0xffff) << 16)|((min) & 0xffff))

#define SMEM_SOCINFO_BUILD_ID_LENGTH           32
#define SMEM_SOCINFO_CHIP_ID_LENGTH            32

/*
 * SMEM item id, used to acquire handles to respective
 * SMEM region.
 */
#define SMEM_HW_SW_BUILD_ID            137
#define SMEM_IMAGE_VERSION_TABLE	469

static uint32_t socinfo_format;
static const char *sku;

enum {
	HW_PLATFORM_UNKNOWN = 0,
	HW_PLATFORM_SURF    = 1,
	HW_PLATFORM_FFA     = 2,
	HW_PLATFORM_FLUID   = 3,
	HW_PLATFORM_SVLTE_FFA	= 4,
	HW_PLATFORM_SVLTE_SURF	= 5,
	HW_PLATFORM_MTP_MDM = 7,
	HW_PLATFORM_MTP  = 8,
	HW_PLATFORM_LIQUID  = 9,
	/* Dragonboard platform id is assigned as 10 in CDT */
	HW_PLATFORM_DRAGON	= 10,
	HW_PLATFORM_QRD	= 11,
	HW_PLATFORM_HRD	= 13,
	HW_PLATFORM_DTV	= 14,
	HW_PLATFORM_RCM	= 21,
	HW_PLATFORM_STP = 23,
	HW_PLATFORM_SBC = 24,
	HW_PLATFORM_HDK = 31,
	HW_PLATFORM_ATP = 33,
	HW_PLATFORM_IDP = 34,
	HW_PLATFORM_QXR = 38,
	HW_PLATFORM_INVALID
};

static const char * const hw_platform[] = {
	[HW_PLATFORM_UNKNOWN] = "Unknown",
	[HW_PLATFORM_SURF] = "Surf",
	[HW_PLATFORM_FFA] = "FFA",
	[HW_PLATFORM_FLUID] = "Fluid",
	[HW_PLATFORM_SVLTE_FFA] = "SVLTE_FFA",
	[HW_PLATFORM_SVLTE_SURF] = "SLVTE_SURF",
	[HW_PLATFORM_MTP_MDM] = "MDM_MTP_NO_DISPLAY",
	[HW_PLATFORM_MTP] = "MTP",
	[HW_PLATFORM_RCM] = "RCM",
	[HW_PLATFORM_LIQUID] = "Liquid",
	[HW_PLATFORM_DRAGON] = "Dragon",
	[HW_PLATFORM_QRD] = "QRD",
	[HW_PLATFORM_HRD] = "HRD",
	[HW_PLATFORM_DTV] = "DTV",
	[HW_PLATFORM_STP] = "STP",
	[HW_PLATFORM_SBC] = "SBC",
	[HW_PLATFORM_HDK] = "HDK",
	[HW_PLATFORM_ATP] = "ATP",
	[HW_PLATFORM_IDP] = "IDP",
	[HW_PLATFORM_QXR] = "QXR",
};

enum {
	PLATFORM_SUBTYPE_QRD = 0x0,
	PLATFORM_SUBTYPE_SKUAA = 0x1,
	PLATFORM_SUBTYPE_SKUF = 0x2,
	PLATFORM_SUBTYPE_SKUAB = 0x3,
	PLATFORM_SUBTYPE_SKUG = 0x5,
	PLATFORM_SUBTYPE_QRD_INVALID,
};

static const char * const qrd_hw_platform_subtype[] = {
	[PLATFORM_SUBTYPE_QRD] = "QRD",
	[PLATFORM_SUBTYPE_SKUAA] = "SKUAA",
	[PLATFORM_SUBTYPE_SKUF] = "SKUF",
	[PLATFORM_SUBTYPE_SKUAB] = "SKUAB",
	[PLATFORM_SUBTYPE_SKUG] = "SKUG",
	[PLATFORM_SUBTYPE_QRD_INVALID] = "INVALID",
};

enum {
	PLATFORM_SUBTYPE_UNKNOWN = 0x0,
	PLATFORM_SUBTYPE_CHARM = 0x1,
	PLATFORM_SUBTYPE_STRANGE = 0x2,
	PLATFORM_SUBTYPE_STRANGE_2A = 0x3,
	PLATFORM_SUBTYPE_INVALID,
};

static const char * const hw_platform_subtype[] = {
	[PLATFORM_SUBTYPE_UNKNOWN] = "Unknown",
	[PLATFORM_SUBTYPE_CHARM] = "charm",
	[PLATFORM_SUBTYPE_STRANGE] = "strange",
	[PLATFORM_SUBTYPE_STRANGE_2A] = "strange_2a",
	[PLATFORM_SUBTYPE_INVALID] = "Invalid",
};

enum {
	/* External SKU */
	SKU_UNKNOWN = 0x0,
	SKU_AA = 0x1,
	SKU_AB = 0x2,
	SKU_AC = 0x3,
	SKU_AD = 0x4,
	SKU_AE = 0x5,
	SKU_AF = 0x6,
	SKU_EXT_RESERVE,

	/* Internal SKU */
	SKU_Y0 = 0xf1,
	SKU_Y1 = 0xf2,
	SKU_Y2 = 0xf3,
	SKU_Y3 = 0xf4,
	SKU_Y4 = 0xf5,
	SKU_Y5 = 0xf6,
	SKU_Y6 = 0xf7,
	SKU_Y7 = 0xf8,
	SKU_INT_RESERVE,
};

static const char * const hw_platform_esku[] = {
	[SKU_UNKNOWN] = "Unknown",
	[SKU_AA] = "AA",
	[SKU_AB] = "AB",
	[SKU_AC] = "AC",
	[SKU_AD] = "AD",
	[SKU_AE] = "AE",
	[SKU_AF] = "AF",
};

#define SKU_INT_MASK 0x0f
static const char * const hw_platform_isku[] = {
	[SKU_Y0 & SKU_INT_MASK] = "Y0",
	[SKU_Y1 & SKU_INT_MASK] = "Y1",
	[SKU_Y2 & SKU_INT_MASK] = "Y2",
	[SKU_Y3 & SKU_INT_MASK] = "Y3",
	[SKU_Y4 & SKU_INT_MASK] = "Y4",
	[SKU_Y5 & SKU_INT_MASK] = "Y5",
	[SKU_Y6 & SKU_INT_MASK] = "Y6",
	[SKU_Y7 & SKU_INT_MASK] = "Y7",
};

enum {
	PLATFORM_OEM_VARIANT_QC = 0x0,
	PLATFORM_OEM_VARIANT_OEM,
};

static const char * const hw_platform_oem_variant[] = {
	[PLATFORM_OEM_VARIANT_QC] = "QC",
	[PLATFORM_OEM_VARIANT_OEM] = "OEM",
};

/* Socinfo SMEM item structure */
static struct socinfo {
	__le32 fmt;
	__le32 id;
	__le32 ver;
	char build_id[SMEM_SOCINFO_BUILD_ID_LENGTH];
	/* Version 2 */
	__le32 raw_id;
	__le32 raw_ver;
	/* Version 3 */
	__le32 hw_plat;
	/* Version 4 */
	__le32 plat_ver;
	/* Version 5 */
	__le32 accessory_chip;
	/* Version 6 */
	__le32 hw_plat_subtype;
	/* Version 7 */
	__le32 pmic_model;
	__le32 pmic_die_rev;
	/* Version 8 */
	__le32 pmic_model_1;
	__le32 pmic_die_rev_1;
	__le32 pmic_model_2;
	__le32 pmic_die_rev_2;
	/* Version 9 */
	__le32 foundry_id;
	/* Version 10 */
	__le32 serial_num;
	/* Version 11 */
	__le32 num_pmics;
	__le32 pmic_array_offset;
	/* Version 12 */
	__le32 chip_family;
	__le32 raw_device_family;
	__le32 raw_device_num;
	/* Version 13 */
	__le32 nproduct_id;
	char chip_name[SMEM_SOCINFO_CHIP_ID_LENGTH];
	/* Version 14 */
	__le32 num_clusters;
	__le32 ncluster_array_offset;
	__le32 num_subset_parts;
	__le32 nsubset_parts_array_offset;
	/* Version 15 */
	__le32  nmodem_supported;
	/* Version 16 */
	__le32  esku;
	__le32  nproduct_code;
	__le32  npartnamemap_offset;
	__le32  nnum_partname_mapping;
	/* Version 17 */
	__le32 hw_plat_oem_variant;
} *socinfo;

#define MAX_SOCINFO_ATTRS 50
/* sysfs attributes */
#define ATTR_DEFINE(param)	\
	static DEVICE_ATTR(param, 0444,	\
		   msm_get_##param,	\
		   NULL)

/* sysfs attributes for subpart information */
#define CREATE_PART_FUNCTION(part, part_enum)  \
	static ssize_t \
	msm_get_##part(struct device *dev, \
			struct device_attribute *attr, \
			char *buf) \
	{ \
		u32 *part_info; \
		int num_parts = 0; \
		int str_pos = 0, i = 0, ret = 0; \
		num_parts = socinfo_get_part_count(part_enum); \
		part_info = kmalloc_array(num_parts, sizeof(*part_info), GFP_KERNEL); \
		ret = socinfo_get_subpart_info(part_enum, part_info, num_parts); \
		if (ret < 0) \
			return -EINVAL;  \
		for (i = 0; i < num_parts; i++) { \
			str_pos += scnprintf(buf+str_pos, PAGE_SIZE-str_pos, "0x%x", \
					part_info[i]); \
			if (i != num_parts-1) \
				str_pos += scnprintf(buf+str_pos, PAGE_SIZE-str_pos, ","); \
		} \
		str_pos += scnprintf(buf+str_pos, PAGE_SIZE-str_pos, "\n"); \
		kfree(part_info); \
		return str_pos; \
	} \
	ATTR_DEFINE(part) \

#define BUILD_ID_LENGTH 32
#define CHIP_ID_LENGTH 32
#define SMEM_IMAGE_VERSION_BLOCKS_COUNT 32
#define SMEM_IMAGE_VERSION_SINGLE_BLOCK_SIZE 128
#define SMEM_IMAGE_VERSION_SIZE 4096
#define SMEM_IMAGE_VERSION_NAME_SIZE 75
#define SMEM_IMAGE_VERSION_VARIANT_SIZE 20
#define SMEM_IMAGE_VERSION_VARIANT_OFFSET 75
#define SMEM_IMAGE_VERSION_OEM_SIZE 33
#define SMEM_IMAGE_VERSION_OEM_OFFSET 95
#define SMEM_IMAGE_VERSION_PARTITION_APPS 10

/* Version 2 */
static uint32_t socinfo_get_raw_id(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 2) ?
			le32_to_cpu(socinfo->raw_id) : 0)
		: 0;
}

static uint32_t socinfo_get_raw_version(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 2) ?
			le32_to_cpu(socinfo->raw_ver) : 0)
		: 0;
}

/* Version 3 */
static uint32_t socinfo_get_platform_type(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 3) ?
			le32_to_cpu(socinfo->hw_plat) : 0)
		: 0;
}

/* Version 4 */
static uint32_t socinfo_get_platform_version(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 4) ?
			le32_to_cpu(socinfo->plat_ver) : 0)
		: 0;
}
/* Version 5 */
static uint32_t socinfo_get_accessory_chip(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 5) ?
			le32_to_cpu(socinfo->accessory_chip) : 0)
		: 0;
}

/* Version 6 */
static uint32_t socinfo_get_platform_subtype(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 6) ?
			le32_to_cpu(socinfo->hw_plat_subtype) : 0)
		: 0;
}

/* Version 7 */
static int socinfo_get_pmic_model(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 7) ?
			le32_to_cpu(socinfo->pmic_model) : 0xFFFFFFFF)
		: 0xFFFFFFFF;
}

static uint32_t socinfo_get_pmic_die_revision(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 7) ?
			le32_to_cpu(socinfo->pmic_die_rev) : 0)
		: 0;
}

/* Version 9 */
static uint32_t socinfo_get_foundry_id(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 9) ?
			le32_to_cpu(socinfo->foundry_id) : 0)
		: 0;
}

/* Version 10 */
uint32_t socinfo_get_serial_number(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 10) ?
			le32_to_cpu(socinfo->serial_num) : 0)
		: 0;
}
EXPORT_SYMBOL(socinfo_get_serial_number);

/* Version 12 */
static uint32_t socinfo_get_chip_family(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 12) ?
			le32_to_cpu(socinfo->chip_family) : 0)
		: 0;
}

static uint32_t socinfo_get_raw_device_family(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 12) ?
			le32_to_cpu(socinfo->raw_device_family) : 0)
		: 0;
}

static uint32_t socinfo_get_raw_device_number(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 12) ?
			le32_to_cpu(socinfo->raw_device_num) : 0)
		: 0;
}

/* Version 13 */
static uint32_t socinfo_get_nproduct_id(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 13) ?
			le32_to_cpu(socinfo->nproduct_id) : 0)
		: 0;
}

static char *socinfo_get_chip_name(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 13) ?
			socinfo->chip_name : "N/A")
		: "N/A";
}

/* Version 14 */
static uint32_t socinfo_get_num_clusters(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 14) ?
			le32_to_cpu(socinfo->num_clusters) : 0)
		: 0;
}

static uint32_t socinfo_get_ncluster_array_offset(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 14) ?
			le32_to_cpu(socinfo->ncluster_array_offset) : 0)
		: 0;
}

static uint32_t socinfo_get_num_subset_parts(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 14) ?
			le32_to_cpu(socinfo->num_subset_parts) : 0)
		: 0;
}

static uint32_t socinfo_get_nsubset_parts_array_offset(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 14) ?
			le32_to_cpu(socinfo->nsubset_parts_array_offset) : 0)
		: 0;
}

/* Version 15 */
static uint32_t socinfo_get_nmodem_supported(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 15) ?
			le32_to_cpu(socinfo->nmodem_supported) : 0)
		: 0;
}

/* Version 16 */
static uint32_t socinfo_get_eskuid(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 16) ?
			le32_to_cpu(socinfo->esku) : 0)
		: 0;
}

static const char *socinfo_get_esku_mapping(void)
{
	uint32_t id = socinfo_get_eskuid();

	if (id > SKU_UNKNOWN && id < SKU_EXT_RESERVE)
		return hw_platform_esku[id];
	else if (id >= SKU_Y0 && id < SKU_INT_RESERVE)
		return hw_platform_isku[id & SKU_INT_MASK];

	return NULL;
}

static uint32_t socinfo_get_nproduct_code(void)
{
	return socinfo ?
		(socinfo_format >= SOCINFO_VERSION(0, 16) ?
			le32_to_cpu(socinfo->nproduct_code) : 0)
		: 0;
}

/* Version 17 */
static uint32_t socinfo_get_platform_oem_variant(void)
{
	return (socinfo && socinfo_format >= SOCINFO_VERSION(0, 17)) ?
			le32_to_cpu(socinfo->hw_plat_oem_variant) : 0;
}

/* Version 2 */
static ssize_t
msm_get_raw_id(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n",
		socinfo_get_raw_id());
}
ATTR_DEFINE(raw_id);

static ssize_t
msm_get_raw_version(struct device *dev,
		     struct device_attribute *attr,
		     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n",
		socinfo_get_raw_version());
}
ATTR_DEFINE(raw_version);

/* Version 3 */
static ssize_t
msm_get_hw_platform(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	uint32_t hw_type;

	hw_type = socinfo_get_platform_type();

	return snprintf(buf, PAGE_SIZE, "%-.32s\n",
			hw_platform[hw_type]);
}
ATTR_DEFINE(hw_platform);

/* Version 4 */
static ssize_t
msm_get_platform_version(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n",
		socinfo_get_platform_version());
}
ATTR_DEFINE(platform_version);

/* Version 5 */
static ssize_t
msm_get_accessory_chip(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n",
		socinfo_get_accessory_chip());
}
ATTR_DEFINE(accessory_chip);

/* Version 6 */
static ssize_t
msm_get_platform_subtype_id(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	uint32_t hw_subtype;

	hw_subtype = socinfo_get_platform_subtype();
	return snprintf(buf, PAGE_SIZE, "%u\n",
		hw_subtype);
}
ATTR_DEFINE(platform_subtype_id);

static ssize_t
msm_get_platform_subtype(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	uint32_t hw_subtype;

	hw_subtype = socinfo_get_platform_subtype();
	if (socinfo_get_platform_type() == HW_PLATFORM_QRD) {
		if (hw_subtype >= PLATFORM_SUBTYPE_QRD_INVALID) {
			pr_err("Invalid hardware platform sub type for qrd found\n");
			hw_subtype = PLATFORM_SUBTYPE_QRD_INVALID;
		}
		return snprintf(buf, PAGE_SIZE, "%-.32s\n",
					qrd_hw_platform_subtype[hw_subtype]);
	} else {
		if (hw_subtype >= PLATFORM_SUBTYPE_INVALID) {
			pr_err("Invalid hardware platform subtype\n");
			hw_subtype = PLATFORM_SUBTYPE_INVALID;
		}
		return snprintf(buf, PAGE_SIZE, "%-.32s\n",
			hw_platform_subtype[hw_subtype]);
	}
}
ATTR_DEFINE(platform_subtype);

/* Version 7 */
static ssize_t
msm_get_pmic_model(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n",
		socinfo_get_pmic_model());
}
ATTR_DEFINE(pmic_model);

static ssize_t
msm_get_pmic_die_revision(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n",
			 socinfo_get_pmic_die_revision());
}
ATTR_DEFINE(pmic_die_revision);

/* Version 8 (skip) */
/* Version 9 */
static ssize_t
msm_get_foundry_id(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n",
		socinfo_get_foundry_id());
}
ATTR_DEFINE(foundry_id);

/* Version 10 */
static ssize_t
msm_get_serial_number(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n",
		socinfo_get_serial_number());
}
ATTR_DEFINE(serial_number);

/* Version 11 (skip) */
/* Version 12 */
static ssize_t
msm_get_chip_family(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n",
		socinfo_get_chip_family());
}
ATTR_DEFINE(chip_family);

static ssize_t
msm_get_raw_device_family(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n",
		socinfo_get_raw_device_family());
}
ATTR_DEFINE(raw_device_family);

static ssize_t
msm_get_raw_device_number(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n",
		socinfo_get_raw_device_number());
}
ATTR_DEFINE(raw_device_number);

/* Version 13 */
static ssize_t
msm_get_nproduct_id(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n",
		socinfo_get_nproduct_id());
}
ATTR_DEFINE(nproduct_id);

static ssize_t
msm_get_chip_name(struct device *dev,
		   struct device_attribute *attr,
		   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%-.32s\n",
			socinfo_get_chip_name());
}
ATTR_DEFINE(chip_name);

/* Version 14 */
static ssize_t
msm_get_num_clusters(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n",
		socinfo_get_num_clusters());
}
ATTR_DEFINE(num_clusters);

static ssize_t
msm_get_ncluster_array_offset(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n",
		socinfo_get_ncluster_array_offset());
}
ATTR_DEFINE(ncluster_array_offset);

uint32_t
socinfo_get_cluster_info(enum subset_cluster_type cluster)
{
	uint32_t sub_cluster, num_cluster, offset;
	void *cluster_val;
	void *info = socinfo;

	if (cluster >= NUM_CLUSTERS_MAX) {
		pr_err("Bad cluster\n");
	return -EINVAL;
	}

	num_cluster = socinfo_get_num_clusters();
	offset = socinfo_get_ncluster_array_offset();

	if (!num_cluster || !offset)
		return -EINVAL;

	info += offset;
	cluster_val = info + (sizeof(uint32_t) * cluster);
	sub_cluster = get_unaligned_le32(cluster_val);

	return sub_cluster;
}
EXPORT_SYMBOL(socinfo_get_cluster_info);

static ssize_t
msm_get_subset_cores(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	uint32_t sub_cluster = socinfo_get_cluster_info(CLUSTER_CPUSS);

	return scnprintf(buf, PAGE_SIZE, "%x\n", sub_cluster);
}
ATTR_DEFINE(subset_cores);

static ssize_t
msm_get_num_subset_parts(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n",
		socinfo_get_num_subset_parts());
}
ATTR_DEFINE(num_subset_parts);

static ssize_t
msm_get_nsubset_parts_array_offset(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n",
		socinfo_get_nsubset_parts_array_offset());
}
ATTR_DEFINE(nsubset_parts_array_offset);

static uint32_t
socinfo_get_subset_parts(void)
{
	uint32_t num_parts = socinfo_get_num_subset_parts();
	uint32_t offset = socinfo_get_nsubset_parts_array_offset();
	uint32_t sub_parts = 0;
	void *info = socinfo;
	uint32_t part_entry;
	int i;

	if (!num_parts || !offset)
		return -EINVAL;

	info += offset;
	for (i = 0; i < num_parts; i++) {
		part_entry = get_unaligned_le32(info);
		if (part_entry)
			sub_parts |= BIT(i);
		info += sizeof(uint32_t);
	}
	return sub_parts;
}

bool
socinfo_get_part_info(enum subset_part_type part)
{
	uint32_t partinfo;

	if (part >= NUM_PARTS_MAX) {
		pr_err("Bad part number\n");
		return false;
	}

	partinfo = socinfo_get_subset_parts();
	if (partinfo < 0) {
		pr_err("Failed to get part information\n");
		return false;
	}

	return (partinfo & BIT(part));
}
EXPORT_SYMBOL(socinfo_get_part_info);

static ssize_t
msm_get_subset_parts(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	uint32_t sub_parts = socinfo_get_subset_parts();

	return scnprintf(buf, PAGE_SIZE, "%x\n", sub_parts);
}
ATTR_DEFINE(subset_parts);

/*
 * socinfo_get_part_count - Get part count
 * @part: The subset_part_type to be checked
 *
 * Return the number of instance supported by the
 * firmware for the part on success and a negative
 * errno will be returned in error cases.
 */
int
socinfo_get_part_count(enum subset_part_type part)
{
	int part_count = 1;

	/* TODO: part_count to be read from SMEM after firmware adds support */
	if ((part <= PART_UNKNOWN) || (part >= NUM_PARTS_MAX)) {
		pr_err("Bad part number\n");
		return -EINVAL;
	}

	return part_count;
}
EXPORT_SYMBOL(socinfo_get_part_count);

/*
 * socinfo_get_subpart_info - Get subpart information
 * @part: The subset_part_type to be checked
 * @part_info: Pointer to the subpart information.
 *             Used to store the subpart information
 *             for num_parts instances of the part.
 * @num_parts: Number of instances of the part for
 *             which the subpart information is required.
 *
 * On success subpart information will be stored in the part_info
 * array for minimum of the number of instances requested and
 * the number of instances supported by the firmware.
 *
 * A value of zero will be returned on success and a negative
 * errno will be returned in error cases.
 */
int
socinfo_get_subpart_info(enum subset_part_type part,
		u32 *part_info,
		u32 num_parts)
{
	uint32_t num_subset_parts = 0, offset = 0;
	void *info = socinfo;
	u32 i = 0, count = 0;
	int part_count = 0;

	if (!part_info)
		return -EINVAL;

	part_count = socinfo_get_part_count(part);
	if (part_count <= 0)
		return -EINVAL;

	num_subset_parts = socinfo_get_num_subset_parts();
	offset = socinfo_get_nsubset_parts_array_offset();
	if (!num_subset_parts || !offset)
		return -EINVAL;

	info += (offset + (part * sizeof(u32)));
	count = min_t(u32, num_parts, part_count);
	for (i = 0; i < count; i++) {
		part_info[i] = get_unaligned_le32(info);
		info += sizeof(u32);
	}

	return 0;
}
EXPORT_SYMBOL(socinfo_get_subpart_info);

CREATE_PART_FUNCTION(gpu, PART_GPU);
CREATE_PART_FUNCTION(video, PART_VIDEO);
CREATE_PART_FUNCTION(camera, PART_CAMERA);
CREATE_PART_FUNCTION(display, PART_DISPLAY);
CREATE_PART_FUNCTION(audio, PART_AUDIO);
CREATE_PART_FUNCTION(modem, PART_MODEM);
CREATE_PART_FUNCTION(wlan, PART_WLAN);
CREATE_PART_FUNCTION(comp, PART_COMP);
CREATE_PART_FUNCTION(sensors, PART_SENSORS);
CREATE_PART_FUNCTION(npu, PART_NPU);
CREATE_PART_FUNCTION(spss, PART_SPSS);
CREATE_PART_FUNCTION(nav, PART_NAV);
CREATE_PART_FUNCTION(comp1, PART_COMP1);
CREATE_PART_FUNCTION(display1, PART_DISPLAY1);
CREATE_PART_FUNCTION(nsp, PART_NSP);
CREATE_PART_FUNCTION(eva, PART_EVA);

/* Version 15 */
static ssize_t
msm_get_nmodem_supported(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n",
		socinfo_get_nmodem_supported());
}
ATTR_DEFINE(nmodem_supported);

/* Version 16 */
static ssize_t
msm_get_sku(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return sysfs_emit(buf, "%s\n", sku ? sku : "Unknown");
}
ATTR_DEFINE(sku);

static ssize_t
msm_get_esku(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	const char *esku = socinfo_get_esku_mapping();

	return sysfs_emit(buf, "%s\n", esku ? esku : "Unknown");
}
ATTR_DEFINE(esku);

/* Version 17 */
static ssize_t
msm_get_platform_oem_variant_id(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	uint32_t hw_oem_variant;

	hw_oem_variant = socinfo_get_platform_oem_variant();
	return scnprintf(buf, PAGE_SIZE, "%u\n", hw_oem_variant);
}
ATTR_DEFINE(platform_oem_variant_id);

static ssize_t
msm_get_platform_oem_variant(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	uint32_t hw_oem_variant;

	hw_oem_variant = socinfo_get_platform_oem_variant();
	if (hw_oem_variant != PLATFORM_OEM_VARIANT_QC)
		hw_oem_variant = PLATFORM_OEM_VARIANT_OEM;

	return scnprintf(buf, PAGE_SIZE, "%-.32s\n", hw_platform_oem_variant[hw_oem_variant]);
}
ATTR_DEFINE(platform_oem_variant);

struct qcom_socinfo {
	struct soc_device *soc_dev;
	struct soc_device_attribute attr;
	uint32_t current_image;
	struct rw_semaphore current_image_rwsem;
};

struct soc_id {
	unsigned int id;
	const char *name;
};

static const struct soc_id soc_id[] = {
	{ 87, "MSM8960" },
	{ 109, "APQ8064" },
	{ 122, "MSM8660A" },
	{ 123, "MSM8260A" },
	{ 124, "APQ8060A" },
	{ 126, "MSM8974" },
	{ 130, "MPQ8064" },
	{ 138, "MSM8960AB" },
	{ 139, "APQ8060AB" },
	{ 140, "MSM8260AB" },
	{ 141, "MSM8660AB" },
	{ 178, "APQ8084" },
	{ 184, "APQ8074" },
	{ 185, "MSM8274" },
	{ 186, "MSM8674" },
	{ 194, "MSM8974PRO" },
	{ 206, "MSM8916" },
	{ 207, "MSM8994" },
	{ 208, "APQ8074-AA" },
	{ 209, "APQ8074-AB" },
	{ 210, "APQ8074PRO" },
	{ 211, "MSM8274-AA" },
	{ 212, "MSM8274-AB" },
	{ 213, "MSM8274PRO" },
	{ 214, "MSM8674-AA" },
	{ 215, "MSM8674-AB" },
	{ 216, "MSM8674PRO" },
	{ 217, "MSM8974-AA" },
	{ 218, "MSM8974-AB" },
	{ 233, "MSM8936" },
	{ 239, "MSM8939" },
	{ 240, "APQ8036" },
	{ 241, "APQ8039" },
	{ 246, "MSM8996" },
	{ 247, "APQ8016" },
	{ 248, "MSM8216" },
	{ 249, "MSM8116" },
	{ 250, "MSM8616" },
	{ 251, "MSM8992" },
	{ 253, "APQ8094" },
	{ 291, "APQ8096" },
	{ 305, "MSM8996SG" },
	{ 310, "MSM8996AU" },
	{ 311, "APQ8096AU" },
	{ 312, "APQ8096SG" },
	{ 318, "SDM630" },
	{ 321, "SDM845" },
	{ 341, "SDA845" },
	{ 356, "SM8250" },
	{ 402, "IPQ6018" },
	{ 425, "SC7180" },
	{ 415, "LAHAINA" },
	{ 439, "LAHAINAP" },
	{ 456, "LAHAINA-ATP" },
	{ 450, "SHIMA" },
	{ 454, "HOLI" },
	{ 457, "WAIPIO" },
	{ 458, "SDXLEMUR" },
	{ 482, "WAIPIOP" },
	{ 506, "DIWALI" },
	{ 547, "DIWALIP" },
	{ 564, "DIWALI-LTE" },
	{ 537, "PARROT" },
	{ 583, "PARROTP" },
	{ 613, "PARROT" },
	{ 530, "CAPE" },
	{ 531, "CAPEP" },
	{ 540, "CAPE-V2" },
	{ 591, "UKEE" },
	{ 525, "NEO-LE" },
	{ 552, "WAIPIO-LTE" },
	{ 554, "NEO-LA" },
	{ 568, "RAVELIN" },
	{ 549, "ANORAK" },
	{ 581, "MONTAGUE" },
	{ 582, "MONTAGUEP" },
	{ 602, "RAVELINP" },
};

static struct qcom_socinfo *qsocinfo;
static struct attribute *msm_custom_socinfo_attrs[MAX_SOCINFO_ATTRS];

static char *socinfo_get_image_version_base_address(void)
{
	size_t size;

	return qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_IMAGE_VERSION_TABLE,
			&size);
}

static ssize_t
msm_get_image_version(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	char *string_address;

	string_address = socinfo_get_image_version_base_address();
	if (IS_ERR_OR_NULL(string_address)) {
		pr_err("Failed to get image version base address\n");
		return snprintf(buf, SMEM_IMAGE_VERSION_NAME_SIZE, "Unknown");
	}

	down_read(&qsocinfo->current_image_rwsem);
	string_address +=
		qsocinfo->current_image * SMEM_IMAGE_VERSION_SINGLE_BLOCK_SIZE;
	up_read(&qsocinfo->current_image_rwsem);
	return snprintf(buf, SMEM_IMAGE_VERSION_NAME_SIZE, "%-.75s\n",
			string_address);
}

static ssize_t
msm_set_image_version(struct device *dev,
			struct device_attribute *attr,
			const char *buf,
			size_t count)
{
	char *store_address;

	down_read(&qsocinfo->current_image_rwsem);
	if (qsocinfo->current_image != SMEM_IMAGE_VERSION_PARTITION_APPS) {
		up_read(&qsocinfo->current_image_rwsem);
		return count;
	}
	store_address = socinfo_get_image_version_base_address();
	if (IS_ERR_OR_NULL(store_address)) {
		pr_err("Failed to get image version base address\n");
		up_read(&qsocinfo->current_image_rwsem);
		return count;
	}
	store_address +=
		qsocinfo->current_image * SMEM_IMAGE_VERSION_SINGLE_BLOCK_SIZE;
	up_read(&qsocinfo->current_image_rwsem);
	snprintf(store_address, SMEM_IMAGE_VERSION_NAME_SIZE, "%-.75s", buf);
	return count;
}

static ssize_t
msm_get_image_variant(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	char *string_address;

	string_address = socinfo_get_image_version_base_address();
	if (IS_ERR_OR_NULL(string_address)) {
		pr_err("Failed to get image version base address\n");
		return snprintf(buf, SMEM_IMAGE_VERSION_VARIANT_SIZE,
		"Unknown");
	}

	down_read(&qsocinfo->current_image_rwsem);
	string_address +=
		qsocinfo->current_image * SMEM_IMAGE_VERSION_SINGLE_BLOCK_SIZE;
	up_read(&qsocinfo->current_image_rwsem);
	string_address += SMEM_IMAGE_VERSION_VARIANT_OFFSET;
	return snprintf(buf, SMEM_IMAGE_VERSION_VARIANT_SIZE, "%-.20s\n",
			string_address);
}

static ssize_t
msm_set_image_variant(struct device *dev,
			struct device_attribute *attr,
			const char *buf,
			size_t count)
{
	char *store_address;

	down_read(&qsocinfo->current_image_rwsem);
	if (qsocinfo->current_image != SMEM_IMAGE_VERSION_PARTITION_APPS) {
		up_read(&qsocinfo->current_image_rwsem);
		return count;
	}
	store_address = socinfo_get_image_version_base_address();
	if (IS_ERR_OR_NULL(store_address)) {
		pr_err("Failed to get image version base address\n");
		up_read(&qsocinfo->current_image_rwsem);
		return count;
	}
	store_address +=
		qsocinfo->current_image * SMEM_IMAGE_VERSION_SINGLE_BLOCK_SIZE;
	up_read(&qsocinfo->current_image_rwsem);
	store_address += SMEM_IMAGE_VERSION_VARIANT_OFFSET;
	snprintf(store_address, SMEM_IMAGE_VERSION_VARIANT_SIZE, "%-.20s", buf);
	return count;
}

static ssize_t
msm_get_image_crm_version(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	char *string_address;

	string_address = socinfo_get_image_version_base_address();
	if (IS_ERR_OR_NULL(string_address)) {
		pr_err("Failed to get image version base address\n");
		return snprintf(buf, SMEM_IMAGE_VERSION_OEM_SIZE, "Unknown");
	}
	down_read(&qsocinfo->current_image_rwsem);
	string_address +=
		qsocinfo->current_image * SMEM_IMAGE_VERSION_SINGLE_BLOCK_SIZE;
	up_read(&qsocinfo->current_image_rwsem);
	string_address += SMEM_IMAGE_VERSION_OEM_OFFSET;
	return snprintf(buf, SMEM_IMAGE_VERSION_OEM_SIZE, "%-.33s\n",
			string_address);
}

static ssize_t
msm_set_image_crm_version(struct device *dev,
			struct device_attribute *attr,
			const char *buf,
			size_t count)
{
	char *store_address;

	down_read(&qsocinfo->current_image_rwsem);
	if (qsocinfo->current_image != SMEM_IMAGE_VERSION_PARTITION_APPS) {
		up_read(&qsocinfo->current_image_rwsem);
		return count;
	}
	store_address = socinfo_get_image_version_base_address();
	if (IS_ERR_OR_NULL(store_address)) {
		pr_err("Failed to get image version base address\n");
		up_read(&qsocinfo->current_image_rwsem);
		return count;
	}
	store_address +=
		qsocinfo->current_image * SMEM_IMAGE_VERSION_SINGLE_BLOCK_SIZE;
	up_read(&qsocinfo->current_image_rwsem);
	store_address += SMEM_IMAGE_VERSION_OEM_OFFSET;
	snprintf(store_address, SMEM_IMAGE_VERSION_OEM_SIZE, "%-.33s", buf);
	return count;
}

static ssize_t
msm_get_image_number(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int ret;

	down_read(&qsocinfo->current_image_rwsem);
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
			qsocinfo->current_image);
	up_read(&qsocinfo->current_image_rwsem);
	return ret;

}

static ssize_t
msm_select_image(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret, digit;

	ret = kstrtoint(buf, 10, &digit);
	if (ret)
		return ret;
	down_write(&qsocinfo->current_image_rwsem);
	if (digit >= 0 && digit < SMEM_IMAGE_VERSION_BLOCKS_COUNT)
		qsocinfo->current_image = digit;
	else
		qsocinfo->current_image = 0;
	up_write(&qsocinfo->current_image_rwsem);
	return count;
}

static ssize_t
msm_get_images(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int pos = 0;
	int image;
	char *image_address;

	image_address = socinfo_get_image_version_base_address();
	if (IS_ERR_OR_NULL(image_address))
		return snprintf(buf, PAGE_SIZE, "Unavailable\n");

	*buf = '\0';
	for (image = 0; image < SMEM_IMAGE_VERSION_BLOCKS_COUNT; image++) {
		if (*image_address == '\0') {
			image_address += SMEM_IMAGE_VERSION_SINGLE_BLOCK_SIZE;
			continue;
		}

		pos += snprintf(buf + pos, PAGE_SIZE - pos, "%d:\n",
			image);
		pos += snprintf(buf + pos, PAGE_SIZE - pos,
			"\tCRM:\t\t%-.75s\n", image_address);
		pos += snprintf(buf + pos, PAGE_SIZE - pos,
			"\tVariant:\t%-.20s\n",
			image_address + SMEM_IMAGE_VERSION_VARIANT_OFFSET);
		pos += snprintf(buf + pos, PAGE_SIZE - pos,
			"\tVersion:\t%-.33s\n",
			image_address + SMEM_IMAGE_VERSION_OEM_OFFSET);

		image_address += SMEM_IMAGE_VERSION_SINGLE_BLOCK_SIZE;
	}

	return pos;
}

static struct device_attribute image_version =
	__ATTR(image_version, 0644,
			msm_get_image_version, msm_set_image_version);

static struct device_attribute image_variant =
	__ATTR(image_variant, 0644,
			msm_get_image_variant, msm_set_image_variant);

static struct device_attribute image_crm_version =
	__ATTR(image_crm_version, 0644,
			msm_get_image_crm_version, msm_set_image_crm_version);

static struct device_attribute select_image =
	__ATTR(select_image, 0644,
			msm_get_image_number, msm_select_image);

static struct device_attribute images =
	__ATTR(images, 0444, msm_get_images, NULL);


static umode_t soc_info_attribute(struct kobject *kobj,
						   struct attribute *attr,
						   int index)
{
	return attr->mode;
}

static const struct attribute_group custom_soc_attr_group = {
	.attrs = msm_custom_socinfo_attrs,
	.is_visible = soc_info_attribute,
};

static void socinfo_populate_sysfs(struct qcom_socinfo *qcom_socinfo)
{
	int i = 0;

	switch (socinfo_format) {
	case SOCINFO_VERSION(0, 17):
		msm_custom_socinfo_attrs[i++] = &dev_attr_platform_oem_variant_id.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_platform_oem_variant.attr;
		fallthrough;
	case SOCINFO_VERSION(0, 16):
		msm_custom_socinfo_attrs[i++] = &dev_attr_sku.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_esku.attr;
	case SOCINFO_VERSION(0, 15):
		msm_custom_socinfo_attrs[i++] = &dev_attr_nmodem_supported.attr;
	case SOCINFO_VERSION(0, 14):
		msm_custom_socinfo_attrs[i++] = &dev_attr_num_clusters.attr;
		msm_custom_socinfo_attrs[i++] =
		&dev_attr_ncluster_array_offset.attr;
		msm_custom_socinfo_attrs[i++] =
		&dev_attr_num_subset_parts.attr;
		msm_custom_socinfo_attrs[i++] =
		&dev_attr_nsubset_parts_array_offset.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_subset_cores.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_subset_parts.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_gpu.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_video.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_camera.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_display.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_audio.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_modem.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_wlan.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_comp.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_sensors.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_npu.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_spss.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_nav.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_comp1.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_display1.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_nsp.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_eva.attr;
	case SOCINFO_VERSION(0, 13):
		msm_custom_socinfo_attrs[i++] = &dev_attr_nproduct_id.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_chip_name.attr;
	case SOCINFO_VERSION(0, 12):
		msm_custom_socinfo_attrs[i++] = &dev_attr_chip_family.attr;
		msm_custom_socinfo_attrs[i++] =
		&dev_attr_raw_device_family.attr;
		msm_custom_socinfo_attrs[i++] =
		&dev_attr_raw_device_number.attr;
	case SOCINFO_VERSION(0, 11):
	case SOCINFO_VERSION(0, 10):
		msm_custom_socinfo_attrs[i++] = &dev_attr_serial_number.attr;
	case SOCINFO_VERSION(0, 9):
		msm_custom_socinfo_attrs[i++] = &dev_attr_foundry_id.attr;
	case SOCINFO_VERSION(0, 8):
	case SOCINFO_VERSION(0, 7):
		msm_custom_socinfo_attrs[i++] = &dev_attr_pmic_model.attr;
		msm_custom_socinfo_attrs[i++] =
		&dev_attr_pmic_die_revision.attr;
	case SOCINFO_VERSION(0, 6):
		msm_custom_socinfo_attrs[i++] =
		&dev_attr_platform_subtype_id.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_platform_subtype.attr;
	case SOCINFO_VERSION(0, 5):
		msm_custom_socinfo_attrs[i++] = &dev_attr_accessory_chip.attr;
	case SOCINFO_VERSION(0, 4):
		msm_custom_socinfo_attrs[i++] = &dev_attr_platform_version.attr;
	case SOCINFO_VERSION(0, 3):
		msm_custom_socinfo_attrs[i++] = &dev_attr_hw_platform.attr;
	case SOCINFO_VERSION(0, 2):
		msm_custom_socinfo_attrs[i++] = &dev_attr_raw_id.attr;
		msm_custom_socinfo_attrs[i++] = &dev_attr_raw_version.attr;
	case SOCINFO_VERSION(0, 1):
		break;
	default:
		pr_err("Unknown socinfo format: v%u.%u\n",
				SOCINFO_MAJOR(socinfo_format),
				SOCINFO_MINOR(socinfo_format));
		break;
	}

	msm_custom_socinfo_attrs[i++] = &image_version.attr;
	msm_custom_socinfo_attrs[i++] = &image_variant.attr;
	msm_custom_socinfo_attrs[i++] = &image_crm_version.attr;
	msm_custom_socinfo_attrs[i++] = &select_image.attr;
	msm_custom_socinfo_attrs[i++] = &images.attr;
	msm_custom_socinfo_attrs[i++] = NULL;
	qcom_socinfo->attr.custom_attr_group = &custom_soc_attr_group;
}

static void socinfo_print(void)
{
	uint32_t f_maj = SOCINFO_MAJOR(socinfo_format);
	uint32_t f_min = SOCINFO_MINOR(socinfo_format);
	uint32_t v_maj = SOCINFO_MAJOR(le32_to_cpu(socinfo->ver));
	uint32_t v_min = SOCINFO_MINOR(le32_to_cpu(socinfo->ver));

	switch (socinfo_format) {
	case SOCINFO_VERSION(0, 1):
		pr_info("v%u.%u, id=%u, ver=%u.%u\n",
				f_maj, f_min, socinfo->id, v_maj, v_min);
		break;
	case SOCINFO_VERSION(0, 2):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u\n",
				f_maj, f_min, socinfo->id, v_maj, v_min,
				socinfo->raw_id, socinfo->raw_ver);
		break;
	case SOCINFO_VERSION(0, 3):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u\n",
				f_maj, f_min, socinfo->id, v_maj, v_min,
				socinfo->raw_id, socinfo->raw_ver,
				socinfo->hw_plat);
		break;
	case SOCINFO_VERSION(0, 4):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n",
			f_maj, f_min, socinfo->id, v_maj, v_min,
			socinfo->raw_id, socinfo->raw_ver,
			socinfo->hw_plat,
			socinfo->plat_ver);
		break;
	case SOCINFO_VERSION(0, 5):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n accessory_chip=%u\n",
			f_maj, f_min, socinfo->id, v_maj, v_min,
			socinfo->raw_id, socinfo->raw_ver,
			socinfo->hw_plat,
			socinfo->plat_ver,
			socinfo->accessory_chip);
		break;
	case SOCINFO_VERSION(0, 6):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n accessory_chip=%u hw_plat_subtype=%u\n",
			f_maj, f_min, socinfo->id, v_maj, v_min,
			socinfo->raw_id, socinfo->raw_ver,
			socinfo->hw_plat,
			socinfo->plat_ver,
			socinfo->accessory_chip,
			socinfo->hw_plat_subtype);
		break;
	case SOCINFO_VERSION(0, 7):
	case SOCINFO_VERSION(0, 8):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n accessory_chip=%u, hw_plat_subtype=%u, pmic_model=%u, pmic_die_revision=%u\n",
			f_maj, f_min, socinfo->id, v_maj, v_min,
			socinfo->raw_id, socinfo->raw_ver,
			socinfo->hw_plat,
			socinfo->plat_ver,
			socinfo->accessory_chip,
			socinfo->hw_plat_subtype,
			socinfo->pmic_model,
			socinfo->pmic_die_rev);
		break;
	case SOCINFO_VERSION(0, 9):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n accessory_chip=%u, hw_plat_subtype=%u, pmic_model=%u, pmic_die_revision=%u foundry_id=%u\n",
			f_maj, f_min, socinfo->id, v_maj, v_min,
			socinfo->raw_id, socinfo->raw_ver,
			socinfo->hw_plat,
			socinfo->plat_ver,
			socinfo->accessory_chip,
			socinfo->hw_plat_subtype,
			socinfo->pmic_model,
			socinfo->pmic_die_rev,
			socinfo->foundry_id);
		break;
	case SOCINFO_VERSION(0, 10):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n accessory_chip=%u, hw_plat_subtype=%u, pmic_model=%u, pmic_die_revision=%u foundry_id=%u serial_number=%u\n",
			f_maj, f_min, socinfo->id, v_maj, v_min,
			socinfo->raw_id, socinfo->raw_ver,
			socinfo->hw_plat,
			socinfo->plat_ver,
			socinfo->accessory_chip,
			socinfo->hw_plat_subtype,
			socinfo->pmic_model,
			socinfo->pmic_die_rev,
			socinfo->foundry_id,
			socinfo->serial_num);
		break;
	case SOCINFO_VERSION(0, 11):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n accessory_chip=%u, hw_plat_subtype=%u, pmic_model=%u, pmic_die_revision=%u foundry_id=%u serial_number=%u num_pmics=%u\n",
			f_maj, f_min, socinfo->id, v_maj, v_min,
			socinfo->raw_id, socinfo->raw_ver,
			socinfo->hw_plat,
			socinfo->plat_ver,
			socinfo->accessory_chip,
			socinfo->hw_plat_subtype,
			socinfo->pmic_model,
			socinfo->pmic_die_rev,
			socinfo->foundry_id,
			socinfo->serial_num,
			socinfo->num_pmics);
		break;
	case SOCINFO_VERSION(0, 12):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n accessory_chip=%u, hw_plat_subtype=%u, pmic_model=%u, pmic_die_revision=%u foundry_id=%u serial_number=%u num_pmics=%u chip_family=0x%x raw_device_family=0x%x raw_device_number=0x%x\n",
			f_maj, f_min, socinfo->id, v_maj, v_min,
			socinfo->raw_id, socinfo->raw_ver,
			socinfo->hw_plat,
			socinfo->plat_ver,
			socinfo->accessory_chip,
			socinfo->hw_plat_subtype,
			socinfo->pmic_model,
			socinfo->pmic_die_rev,
			socinfo->foundry_id,
			socinfo->serial_num,
			socinfo->num_pmics,
			socinfo->chip_family,
			socinfo->raw_device_family,
			socinfo->raw_device_num);
		break;
	case SOCINFO_VERSION(0, 13):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n accessory_chip=%u, hw_plat_subtype=%u, pmic_model=%u, pmic_die_revision=%u foundry_id=%u serial_number=%u num_pmics=%u chip_family=0x%x raw_device_family=0x%x raw_device_number=0x%x nproduct_id=0x%x\n",
			f_maj, f_min, socinfo->id, v_maj, v_min,
			socinfo->raw_id, socinfo->raw_ver,
			socinfo->hw_plat,
			socinfo->plat_ver,
			socinfo->accessory_chip,
			socinfo->hw_plat_subtype,
			socinfo->pmic_model,
			socinfo->pmic_die_rev,
			socinfo->foundry_id,
			socinfo->serial_num,
			socinfo->num_pmics,
			socinfo->chip_family,
			socinfo->raw_device_family,
			socinfo->raw_device_num,
			socinfo->nproduct_id);
		break;

	case SOCINFO_VERSION(0, 14):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n accessory_chip=%u, hw_plat_subtype=%u, pmic_model=%u, pmic_die_revision=%u foundry_id=%u serial_number=%u num_pmics=%u chip_family=0x%x raw_device_family=0x%x raw_device_number=0x%x nproduct_id=0x%x num_clusters=0x%x ncluster_array_offset=0x%x num_subset_parts=0x%x nsubset_parts_array_offset=0x%x\n",
			f_maj, f_min, socinfo->id, v_maj, v_min,
			socinfo->raw_id, socinfo->raw_ver,
			socinfo->hw_plat,
			socinfo->plat_ver,
			socinfo->accessory_chip,
			socinfo->hw_plat_subtype,
			socinfo->pmic_model,
			socinfo->pmic_die_rev,
			socinfo->foundry_id,
			socinfo->serial_num,
			socinfo->num_pmics,
			socinfo->chip_family,
			socinfo->raw_device_family,
			socinfo->raw_device_num,
			socinfo->nproduct_id,
			socinfo->num_clusters,
			socinfo->ncluster_array_offset,
			socinfo->num_subset_parts,
			socinfo->nsubset_parts_array_offset);
		break;

	case SOCINFO_VERSION(0, 15):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n accessory_chip=%u, hw_plat_subtype=%u, pmic_model=%u, pmic_die_revision=%u foundry_id=%u serial_number=%u num_pmics=%u chip_family=0x%x raw_device_family=0x%x raw_device_number=0x%x nproduct_id=0x%x num_clusters=0x%x ncluster_array_offset=0x%x num_subset_parts=0x%x nsubset_parts_array_offset=0x%x nmodem_supported=0x%x\n",
			f_maj, f_min, socinfo->id, v_maj, v_min,
			socinfo->raw_id, socinfo->raw_ver,
			socinfo->hw_plat,
			socinfo->plat_ver,
			socinfo->accessory_chip,
			socinfo->hw_plat_subtype,
			socinfo->pmic_model,
			socinfo->pmic_die_rev,
			socinfo->foundry_id,
			socinfo->serial_num,
			socinfo->num_pmics,
			socinfo->chip_family,
			socinfo->raw_device_family,
			socinfo->raw_device_num,
			socinfo->nproduct_id,
			socinfo->num_clusters,
			socinfo->ncluster_array_offset,
			socinfo->num_subset_parts,
			socinfo->nsubset_parts_array_offset,
			socinfo->nmodem_supported);
		break;

	case SOCINFO_VERSION(0, 16):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n accessory_chip=%u, hw_plat_subtype=%u, pmic_model=%u, pmic_die_revision=%u foundry_id=%u serial_number=%u num_pmics=%u chip_family=0x%x raw_device_family=0x%x raw_device_number=0x%x nproduct_id=0x%x num_clusters=0x%x ncluster_array_offset=0x%x num_subset_parts=0x%x nsubset_parts_array_offset=0x%x nmodem_supported=0x%x sku=%s\n",
			f_maj, f_min, socinfo->id, v_maj, v_min,
			socinfo->raw_id, socinfo->raw_ver,
			socinfo->hw_plat,
			socinfo->plat_ver,
			socinfo->accessory_chip,
			socinfo->hw_plat_subtype,
			socinfo->pmic_model,
			socinfo->pmic_die_rev,
			socinfo->foundry_id,
			socinfo->serial_num,
			socinfo->num_pmics,
			socinfo->chip_family,
			socinfo->raw_device_family,
			socinfo->raw_device_num,
			socinfo->nproduct_id,
			socinfo->num_clusters,
			socinfo->ncluster_array_offset,
			socinfo->num_subset_parts,
			socinfo->nsubset_parts_array_offset,
			socinfo->nmodem_supported,
			sku ? sku : "Unknown");
		break;

	case SOCINFO_VERSION(0, 17):
		pr_info("v%u.%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n accessory_chip=%u, hw_plat_subtype=%u, pmic_model=%u, pmic_die_revision=%u foundry_id=%u serial_number=%u num_pmics=%u chip_family=0x%x raw_device_family=0x%x raw_device_number=0x%x nproduct_id=0x%x num_clusters=0x%x ncluster_array_offset=0x%x nmodem_supported=0x%x sku=%s hw_plat_oem_variant=%u\n",
			f_maj, f_min, socinfo->id, v_maj, v_min,
			socinfo->raw_id, socinfo->raw_ver,
			socinfo->hw_plat,
			socinfo->plat_ver,
			socinfo->accessory_chip,
			socinfo->hw_plat_subtype,
			socinfo->pmic_model,
			socinfo->pmic_die_rev,
			socinfo->foundry_id,
			socinfo->serial_num,
			socinfo->num_pmics,
			socinfo->chip_family,
			socinfo->raw_device_family,
			socinfo->raw_device_num,
			socinfo->nproduct_id,
			socinfo->num_clusters,
			socinfo->ncluster_array_offset,
			socinfo->nmodem_supported,
			sku ? sku : "Unknown",
			socinfo->hw_plat_oem_variant);
		break;

	default:
		pr_err("Unknown format found: v%u.%u\n", f_maj, f_min);
		break;
	}
}

static const char *socinfo_machine(unsigned int id)
{
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(soc_id); idx++) {
		if (soc_id[idx].id == id)
			return soc_id[idx].name;
	}

	return NULL;
}

uint32_t socinfo_get_id(void)
{
	return (socinfo) ? le32_to_cpu(socinfo->id) : 0;
}
EXPORT_SYMBOL(socinfo_get_id);

const char *socinfo_get_id_string(void)
{
	uint32_t id = socinfo_get_id();

	return socinfo_machine(id);
}
EXPORT_SYMBOL(socinfo_get_id_string);

int socinfo_get_oem_variant_id(void)
{
	if (socinfo_format < SOCINFO_VERSION(0, 17)) {
		pr_warn("socinfo: oem_variant is not supported by bootloaders\n");
		return -EINVAL;
	}

	return socinfo_get_platform_oem_variant();
}
EXPORT_SYMBOL(socinfo_get_oem_variant_id);

static int qcom_socinfo_probe(struct platform_device *pdev)
{
	struct qcom_socinfo *qs;
	struct socinfo *info;
	size_t item_size;
	const char *machine, *esku;

	info = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_HW_SW_BUILD_ID,
			      &item_size);
	if (IS_ERR(info)) {
		dev_err(&pdev->dev, "Couldn't find socinfo\n");
		return PTR_ERR(info);
	}

	socinfo_format = le32_to_cpu(info->fmt);
	socinfo = info;

	qs = devm_kzalloc(&pdev->dev, sizeof(*qs), GFP_KERNEL);
	if (!qs)
		return -ENOMEM;

	qs->attr.machine = socinfo_machine(le32_to_cpu(info->id));
	qs->attr.family = "Snapdragon";
	qs->attr.soc_id = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%u",
					 le32_to_cpu(info->id));
	qs->attr.revision = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%u.%u",
					   SOCINFO_MAJOR(le32_to_cpu(info->ver)),
					   SOCINFO_MINOR(le32_to_cpu(info->ver)));
	qs->attr.soc_id = kasprintf(GFP_KERNEL, "%d", socinfo_get_id());

	if (socinfo_format >= SOCINFO_VERSION(0, 16)) {
		machine = socinfo_machine(le32_to_cpu(info->id));
		esku = socinfo_get_esku_mapping();
		if (machine && esku)
			sku = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s-%u-%s",
				machine, socinfo_get_nproduct_code(), esku);
	}

	qsocinfo = qs;
	init_rwsem(&qs->current_image_rwsem);
	socinfo_populate_sysfs(qs);
	socinfo_print();

	qs->soc_dev = soc_device_register(&qs->attr);
	if (IS_ERR(qs->soc_dev))
		return PTR_ERR(qs->soc_dev);

	/* Feed the soc specific unique data into entropy pool */
	add_device_randomness(info, item_size);

	platform_set_drvdata(pdev, qs);

	return 0;
}

static int qcom_socinfo_remove(struct platform_device *pdev)
{
	struct qcom_socinfo *qs = platform_get_drvdata(pdev);

	soc_device_unregister(qs->soc_dev);

	return 0;
}

static struct platform_driver qcom_socinfo_driver = {
	.probe = qcom_socinfo_probe,
	.remove = qcom_socinfo_remove,
	.driver  = {
		.name = "qcom-socinfo",
	},
};

module_platform_driver(qcom_socinfo_driver);

MODULE_DESCRIPTION("Qualcomm SoCinfo driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:qcom-socinfo");
