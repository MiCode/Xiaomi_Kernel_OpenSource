// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT

/*
 * (C) COPYRIGHT 2020-2021 Arm Limited or its affiliates. All rights reserved.
 */

/*
 * Part of the Mali reference arbiter
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/string.h>
#include <linux/io.h>
#include "mali_gpu_assign.h"


/*
 * ptm_config: string parsed to assign GPU resources from the command line
 * Example:
 * ptm_config='A:S0:S1:S2:P0:P1:W0:W1,B:S3:P2:W2'
 * This string correspond to the following configuration:
 * - first group assigned to Bus A with slices 0, 1, 2; partition 0, 1
 * and access windows 0, 1
 * - second group assigned to Bus B with slice 3; partition 2 and
 * access window 2
 */
static char *ptm_config;
module_param(ptm_config, charp, 0660);
MODULE_PARM_DESC(ptm_config,
	"Specify the GPU resources to assign to buses\n\
Example:\n\
ptm_config='A:S0:S1:S2:P0:P1:W0:W1,B:S3:P2:W2'\n\
This string correspond to the following configuration:\n\
- first group assigned to Bus A with slices 0, 1, 2; partition 0, 1 and\
 access windows 0, 1\n\
- second group assigned to Bus B with slice 3; partition 2 and\
 access window 2");

/* Device tree compatible ID of the Assign Module*/
#define MALI_ASSIGN_DT_NAME "arm,mali-gpu-assign"

/*
 * These registers are common to System, Assign,
 * Partition config and Partition Control blocks.
 */
#define PTM_DEVICE_ID                           (0x0000)
#define PTM_UNIT_FEATURES                       (0x0004)
#define PTM_SLICE_FEATURES                      (0x0008)
#define PTM_SLICE_CORES                         ((uint64_t)(0x000C))


/* We check PTM_DEVICE_ID against this value to determine compatibility */
#define PTM_SUPPORTED_VER			(0x9e550000)
/* Required and masked fields: */
/* Bits [3:0] VERSION_STATUS - masked */
/* Bits [11:4] VERSION_MINOR - masked */
/* Bits [15:12] VERSION_MAJOR - masked */
/* Bits [19:16] PRODUCT_MAJOR - required */
/* Bits [23:20] ARCH_REV - masked */
/* Bits [27:24] ARCH_MINOR - required */
/* Bits [31:28] ARCH_MAJOR - required */
#define PTM_VER_MASK				(0xFF0F0000)

/*
 * GPU Assignment
 *
 * The assignment block is used to assign resources to resource groups.
 * It contains Bus, Partition and Slice group assignments and configuration
 * of slice isolation.
 *
 */
#define PTM_ASSIGN_RESOURCE_GROUP_BUS           (0x0040)
#define PTM_ASSIGN_PARTITION_RESOURCE_GROUP     (0x0044)
#define PTM_ASSIGN_SLICE_RESOURCE_GROUP         (0x0048)
#define PTM_ASSIGN_SLICE_ISOLATION_STATUS       (0x004C)
#define PTM_ASSIGN_SLICE_ISOLATION_SET          (0x0050)
#define PTM_ASSIGN_AW_RESOURCE_GROUP            (0x0080)    /* 128 bits */


#define PTM_UNIT_FEATURES_PARTITIONS_OFFSET	(0)
#define PTM_UNIT_FEATURES_PARTITIONS_MASK	(0x0F)
#define PTM_UNIT_FEATURES_AWS_OFFSET		(8)
#define PTM_UNIT_FEATURES_AWS_MASK		(0x3F)

#define PTM_SLICE_CORES_CORE_BITS		(8)
#define PTM_SLICE_CORES_CORE_MASK		(0xFF)

/* How many access windows are in each DWORD */
#define AWS_PER_DWORD				(8)

/* Defines how many bits are used to represent a group and its mask */
#define GROUP_BITS				(4)
#define GROUP_MASK				(0x3)

/* Defines how many bits are used to represent a bus and its mask */
#define BUS_BITS				(4)
#define BUS_MASK				(0x1)

/* Defines how many bits are used to represent a slice isolation */
#define ISOLATION_BITS				(1)
#define ISOLATION_MASK				(0x1)

/* Define how big is the dmesg buffer to represent the assign configuration */
#define PRINT_BUFF_SIZE				(512)

/**
 * check_ptm_version() - Returns 0 if ptm_device_id versions don't match what
 * our code is expecting.
 * @ptm_device_id: 32bit integer read out of PTM_DEVICE_ID register.
 *
 * Return: 1 if version ok or 0 for version mismatch.
 */
static int check_ptm_version(u32 ptm_device_id)
{
	/* Mask the status, minor, major versions and arch_rev. */
	ptm_device_id &= PTM_VER_MASK;

	if (ptm_device_id != (PTM_SUPPORTED_VER & PTM_VER_MASK))
		return 0;
	else
		return 1;
}


enum PTM_BUS_AB_ENUM {
	BUS_A = 0,
	BUS_B = 1,
	BUS_COUNT = 2
};

static struct _resource_group {
	bool is_set;
	u32 bus;
} resource_group_defaults = {
	.is_set = false,
	.bus = BUS_A
};

static struct _access_window {
	bool is_set;
	u32 rg;
} access_window_defaults = {
	.is_set = false,
	.rg = 0
};

static struct _partition {
	bool is_set;
	u32 rg;
} partition_defaults = {
	.is_set = false,
	.rg = 0
};

static struct _slice {
	bool is_set;
	u32 rg;
	u32 isolation_set;
} slice_defaults = {
	.is_set = false,
	.rg = 0,
	.isolation_set = 0
};

struct _gpu_assign {
	struct _resource_group res_grps[MALI_PTM_RESOURCE_GROUP_COUNT];
	struct _access_window access_windows[MALI_PTM_ACCESS_WINDOW_COUNT];
	struct _partition partitions[MALI_PTM_PARTITION_COUNT];
	struct _slice slices[MALI_PTM_SLICES_COUNT];
	u32 partition_count, slice_count, aw_count, rg_count;
};

/**
 * struct mali_gpu_ptm_assign - Assign module data.
 * @dev:           Pointr to the Assign device instance.
 * @base_addr:     Virtual memory address to the Assign device.
 * @reg_res:       Resource data from the Assign device.
 * @bl_state:      Bus logger state
 * @buslogger:     Pointer to the buglogger client
 * @reg_data:      Register data store
 * Stores the relevant data for the Assign driver.
 */
struct mali_gpu_ptm_assign {
	struct device *dev;
	void __iomem *base_addr;
	struct resource reg_res;
};

/**
 * init_data() - Initialise the assign structure.
 * @dev: pointer to assign device (only used for logging)
 * @assign: pointer to a _gpu_assign struct to be initialised
 * @base_addr: pointer to the start of the register block, needed to read
 * hardware limits.
 *
 * Sets values to specification defaults, and queries the hardware limits.
 *
 * Return: true for success or false for failure.
 */
static bool init_data(struct device *dev, struct _gpu_assign *assign,
			void __iomem *base_addr)
{
	int i;
	u32 v;
	u64 cores;

	if (!dev || !assign || !base_addr)
		return false;

	for (i = 0; i < MALI_PTM_RESOURCE_GROUP_COUNT; ++i)
		assign->res_grps[i] = resource_group_defaults;
	for (i = 0; i < MALI_PTM_ACCESS_WINDOW_COUNT; ++i)
		assign->access_windows[i] = access_window_defaults;
	for (i = 0; i < MALI_PTM_PARTITION_COUNT; ++i)
		assign->partitions[i] = partition_defaults;
	for (i = 0; i < MALI_PTM_SLICES_COUNT; ++i)
		assign->slices[i] = slice_defaults;

	/* read the number of partitions and access windows the hardware has */
	v = readl(base_addr + PTM_UNIT_FEATURES);
	assign->partition_count = (v >> PTM_UNIT_FEATURES_PARTITIONS_OFFSET) &
					PTM_UNIT_FEATURES_PARTITIONS_MASK;
	assign->aw_count = (v >> PTM_UNIT_FEATURES_AWS_OFFSET) &
					PTM_UNIT_FEATURES_AWS_MASK;

	/* count the number of slices the hardware has by iterating through
	 * all the slices until we reach a slice with a core count of zero.
	 */
	cores = readl(base_addr + PTM_SLICE_CORES);
	cores |= (u64)readl(base_addr + PTM_SLICE_CORES + 4) << 32;
	for (i = 0; i < MALI_PTM_SLICES_COUNT; ++i) {
		if (((cores >> i*PTM_SLICE_CORES_CORE_BITS)
			& PTM_SLICE_CORES_CORE_MASK) == 0)
			break;
	}
	assign->slice_count = i;

	assign->rg_count = MALI_PTM_RESOURCE_GROUP_COUNT;

	/* check these things won't overflow our structure, clamp if they do */
	if (assign->partition_count > MALI_PTM_PARTITION_COUNT) {
		dev_err(dev, "Hardware has more partitions than driver\n");
		assign->partition_count = MALI_PTM_PARTITION_COUNT;
	}
	if (assign->aw_count > MALI_PTM_ACCESS_WINDOW_COUNT) {
		dev_err(dev, "Hardware has more windows than driver\n");
		assign->aw_count = MALI_PTM_ACCESS_WINDOW_COUNT;
	}
	if (assign->slice_count > MALI_PTM_SLICES_COUNT) {
		dev_err(dev, "Hardware has more slices than driver\n");
		assign->slice_count = MALI_PTM_SLICES_COUNT;
	}

	return true;
}

/**
 * set_rg_bus() - assign a bus to a resource group
 * @dev: pointer to assign device (only used for logging)
 * @assign: pointer to _gpu_assign struct
 * @rg: index of resource group to change
 * @bus: bus number to assign to resource group (PTM_BUS_AB_ENUM)
 *
 * Will fail if any parameter is out of range or resource group already assigned
 * a bus.
 *
 * Return: true for success or false for failure.
 */
static bool set_rg_bus(struct device *dev, struct _gpu_assign *assign, u32 rg,
			u32 bus)
{
	if (rg >= assign->rg_count) {
		dev_err(dev, "Group %d out of range (max %d)\n",
			rg, assign->rg_count-1);
		return false;
	}
	if (bus >= BUS_COUNT) {
		dev_err(dev, "Group bus %d out of range (max %d)\n",
			bus, BUS_COUNT-1);
		return false;
	}

	if (!assign->res_grps[rg].is_set || assign->res_grps[rg].bus == bus) {
		assign->res_grps[rg].bus = bus;
		assign->res_grps[rg].is_set = 1;
	} else {
		dev_err(dev, "Group %d already assigned to bus %d\n",
			rg, assign->res_grps[rg].bus);
		return false;
	}
	return true;
}

/**
 * set_aw_rg() - assign a resource group to an access window
 * @dev: pointer to assign device (only used for logging)
 * @assign: pointer to _gpu_assign struct
 * @aw: index of access window to change
 * @rg: index of resource group to assign to access window
 *
 * Will fail if any parameter is out of range or access window already assigned
 * to a resource group.
 *
 * Return: true for success or false for failure.
 */
static bool set_aw_rg(struct device *dev, struct _gpu_assign *assign, u32 aw,
			u32 rg)
{
	if (aw >= assign->aw_count) {
		dev_err(dev, "Window %d out of range (max %d)\n",
			aw, assign->aw_count-1);
		return false;
	}
	if (rg >= assign->rg_count) {
		dev_err(dev, "Window Group %d out of range (max %d)\n",
			rg, assign->rg_count-1);
		return false;
	}

	if (!assign->access_windows[aw].is_set ||
		assign->access_windows[aw].rg == rg) {
		assign->access_windows[aw].rg = rg;
		assign->access_windows[aw].is_set = 1;
	} else {
		dev_err(dev, "Window %d already assigned to group %d\n",
			aw, assign->access_windows[aw].rg);
		return false;
	}
	return true;
}

/**
 * set_partition_rg() - assign a resource group to a partition
 * @dev: pointer to assign device (only used for logging)
 * @assign: pointer to _gpu_assign struct
 * @partition: index of partition to change
 * @rg: index of resource group to assign to partition
 *
 * Will fail if any parameter is out of range or partition already assigned
 * to a resource group.
 *
 * Return: true for success or false for failure.
 */
static bool set_partition_rg(struct device *dev, struct _gpu_assign *assign,
				u32 partition, u32 rg)
{
	if (partition >= assign->partition_count) {
		dev_err(dev, "Partition %d out of range (max %d)\n",
			partition, assign->partition_count-1);
		return false;
	}
	if (rg >= assign->rg_count) {
		dev_err(dev, "Partition Group %d out of range (max %d)\n",
			rg, assign->rg_count-1);
		return false;
	}

	if (!assign->partitions[partition].is_set ||
		assign->partitions[partition].rg == rg) {
		assign->partitions[partition].rg = rg;
		assign->partitions[partition].is_set = 1;
	} else {
		dev_err(dev, "Partition %d already assigned to group %d\n",
			partition, assign->partitions[partition].rg);
		return false;
	}
	return true;
}

/**
 * set_slice_rg() - assign a resource group to a slice
 * @dev: pointer to assign device (only used for logging)
 * @assign: pointer to _gpu_assign struct
 * @slice: index of slice to change
 * @rg: index of resource group to assign to slice
 *
 * Will fail if any parameter is out of range or slice already assigned
 * to a resource group.
 *
 * Return: true for success or false for failure.
 */
static bool set_slice_rg(struct device *dev, struct _gpu_assign *assign,
			u32 slice, u32 rg)
{
	if (slice >= MALI_PTM_SLICES_COUNT) {
		dev_err(dev, "Slice %d out of driver range (max %d)\n",
			slice, MALI_PTM_SLICES_COUNT);
		return false;
	}
	/* Trying to configure more slices than the hardware allows should not
	 * cause probe failure, as the RG module reads the hardware directly and
	 * uses that to configure the Arbiter
	 */
	if (slice >= assign->slice_count) {
		dev_warn(dev, "Slice %d out of range (max %d)\n",
			slice, assign->slice_count-1);
	}
	if (rg >= assign->rg_count) {
		dev_err(dev, "Slice Group %d out of range (max %d)\n",
			rg, assign->rg_count-1);
		return false;
	}

	if (!assign->slices[slice].is_set || assign->slices[slice].rg == rg) {
		assign->slices[slice].rg = rg;
		assign->slices[slice].is_set = 1;
	} else {
		dev_err(dev, "Slice %d already assigned to group %d\n",
			slice, assign->slices[slice].rg);
		return false;
	}
	return true;
}

/**
 * get_token() - extract a token from string separated from others by one of the
 * delimiters.
 * @str: pointer to a string pointer, used as the start of the token search and
 *	set to the start of next token on return (you should use this same
 *	variable to extract the next token in a subsequent call to get_token).
 * @delimiters: string with one or more characters used as token delimiters.
 *
 * The string pointed to by str will be modified to insert null terminators, so
 * make sure it's writeable memory.
 * If delimiters contains no characters it will consider the whole string a
 * token.
 *
 * Return: pointer to a null terminated token string, or NULL if no further
 * tokens.
 */
static char *get_token(char **str, const char *delimiters)
{
	char *start, *end;
	const char *d;
	bool match;

	if (!str || !*str || !delimiters)
		return NULL;

	start = *str;
	end = start;

	/* scan forward for the terminating delimiter */
	while (*end != '\0') {
		for (match = false, d = delimiters; *d != '\0' && !match; ++d)
			match = (*end == *d);
		if (!match)
			++end;
		else
			break;
	}

	if (*end != '\0') {
		*end = '\0';
		*str = end + 1;
	} else
		*str = NULL;

	return start;
}

/**
 * to_uint32() - convert a string to a u32, logging any errors.
 * @dev: pointer to assign device (only used for logging)
 * @str: string to convert
 * @ui: pointer to receiving variable
 * @param: name of parameter being converted, used for logging.
 *
 * Converts a string to a u32 and logs an error if it fails.
 *
 * Return: true for success or false for failure.
 */
static bool to_uint32(struct device *dev, const char *str, u32 *ui,
			const char *param)
{
	if (str && kstrtouint(str, 10, ui) == 0)
		return true;

	dev_err(dev, "%s '%s' invalid. An unsigned integer is required\n",
		param ? param:"", str ? str:"null");
	return false;
}

/**
 * get_data_from_command_line() - Populate the structures by parsing a string
 * from the command line.
 * @dev: pointer to assign device (only used for logging)
 * @assign: pointer to a _gpu_assign struct which will have its values set by
 * this function.
 * @str: command line string. Example: A:S0:S1:P2:P3:W12:W4,B:S3:P1:W5
 *
 * Performs checks for validity and logs errors.
 *
 * Return: true for success or false for failure. Any invalid data will return
 * false.
 */
static bool get_data_from_command_line(struct device *dev,
					struct _gpu_assign *assign,
					const char *str)
{
	bool retval = false;
	u32 grp, i;
	char *string_copy, *grp_tok, *next_grp_str, *tok;
	size_t slen;

	if (!dev || !assign || !str)
		return false;

	/* copy the string ready for tokenizing, which modifies the string */
	slen = strlen(str);
	string_copy = kmalloc(slen + 1, GFP_KERNEL);
	if (!string_copy)
		return false;
	strlcpy(string_copy, str, slen + 1);

	/* iterate through the comma-separated resource groups */
	grp = 0;
	next_grp_str = string_copy;
	while ((grp_tok = get_token(&next_grp_str, ","))) {
		/* within group iterate through the colon-separated resources */
		while ((tok = get_token(&grp_tok, ":"))) {

			if (grp >= assign->rg_count) {
				dev_err(dev, "Group %d out of range (max %d)\n",
					grp, assign->rg_count-1);
				goto parse_fail;
			}

			if (*tok == '\0') {
				dev_err(dev, "empty resource token\n");
				goto parse_fail;
			}

			/* interpret the resource token meaning */
			switch (*tok) {
			case 'A':	/* a bus specifier? */
			case 'B':
			case 'C':
				if (*(tok+1) != ':' && *(tok+1) != '\0') {
					dev_err(dev,
						"Invalid '%c' after bus '%c'\n",
						*(tok+1), *tok);
					goto parse_fail;
				}
				if (!set_rg_bus(dev, assign, grp, (*tok) - 'A'))
					goto parse_fail;
				break;
			case 'S':	/* a slice specifier? */
				++tok;
				if (!to_uint32(dev, tok, &i, "Slice") ||
					!set_slice_rg(dev, assign, i, grp))
					goto parse_fail;
				break;
			case 'P':	/* a partition specifier? */
				++tok;
				if (!to_uint32(dev, tok, &i, "Partition") ||
					!set_partition_rg(dev, assign, i, grp))
					goto parse_fail;
				break;
			case 'W':	/* an access window specifier? */
				++tok;
				if (!to_uint32(dev, tok, &i, "Window") ||
					!set_aw_rg(dev, assign, i, grp))
					goto parse_fail;
				break;
			default:
				dev_err(dev, "invalid token '%c'\n", *tok);
				goto parse_fail;
			}
		}

		++grp;
	}

	retval = true;

parse_fail:
	kfree(string_copy);

	return retval;
}

/**
 * validate_data() - does any final validation of assignment and sets slice
 * isolation registers
 * @dev: pointer to assign device (only used for logging)
 * @assign: pointer to _gpu_assign struct
 *
 * Intended to do any final validation checks on the gpu_assign structure and to
 * set the slice isolation registers to prevent slices in one resource group
 * accessing slices in a different resource group.
 *
 * Return: true for success or false for failure.
 */
static bool validate_data(struct device *dev, struct _gpu_assign *assign)
{
	int i;
	u32 prev_slice_rg;

	/* set the isolation flag on each slice whose preceding slice is in a
	 * different resource group
	 */
	prev_slice_rg = assign->rg_count;
	for (i = 0; i < assign->slice_count; ++i) {
		if (assign->slices[i].rg != prev_slice_rg)
			assign->slices[i].isolation_set = 1;
		prev_slice_rg = assign->slices[i].rg;
	}

	return true;
}

/**
 * print_config() - prints the assignment state to the kernel log
 * @dev: pointer to assign device (only used for logging)
 * @assign: pointer to _gpu_assign struct containing assignment state
 *
 * Example output for hardware with 4 resource groups, 4 slices, 4 partitions,
 * and 16 access windows, and everything assigned evenly:-
 * RG0 BUS[A] S[0      ] P[0      ] W[0 1 2 3                              ]
 * RG1 BUS[A] S[  1    ] P[  1    ] W[        4 5 6 7                      ]
 * RG2 BUS[B] S[    2  ] P[    2  ] W[                8 9 10 11            ]
 * RG3 BUS[B] S[      3] P[      3] W[                          12 13 14 15]
 */
static void print_config(struct device *dev, struct _gpu_assign *assign)
{
	char buff[PRINT_BUFF_SIZE];
	char buses[BUS_COUNT] = { 'A', 'B' };
	u32 c = 0, rg;

	if (!dev || !assign)
		return;

	for (rg = 0; rg < assign->rg_count; ++rg) {
		u32 bus = assign->res_grps[rg].bus;
		u32 i;

		c += snprintf(&buff[c], PRINT_BUFF_SIZE - c,
		"RG%d BUS[%c]", rg, bus < BUS_COUNT ? buses[bus]:'-');

		c += snprintf(&buff[c], PRINT_BUFF_SIZE - c, " S[");

		for (i = 0; i < assign->slice_count; ++i) {
			if (i > 0)
				c += snprintf(&buff[c], PRINT_BUFF_SIZE - c,
					" ");

			if (assign->slices[i].rg == rg)
				c += snprintf(&buff[c], PRINT_BUFF_SIZE - c,
					"%d", i);
			else
				c += snprintf(&buff[c], PRINT_BUFF_SIZE - c,
						i < 10 ? " ":"  ");
		}

		c += snprintf(&buff[c], PRINT_BUFF_SIZE - c, "] P[");

		for (i = 0; i < assign->partition_count; ++i) {
			if (i > 0)
				c += snprintf(&buff[c], PRINT_BUFF_SIZE - c,
					" ");

			if (assign->partitions[i].rg == rg)
				c += snprintf(&buff[c], PRINT_BUFF_SIZE - c,
					"%d", i);
			else
				c += snprintf(&buff[c], PRINT_BUFF_SIZE - c,
						i < 10 ? " ":"  ");
		}

		c += snprintf(&buff[c], PRINT_BUFF_SIZE - c, "] W[");

		for (i = 0; i < assign->aw_count; ++i) {
			if (i > 0)
				c += snprintf(&buff[c], PRINT_BUFF_SIZE - c,
					" ");

			if (assign->access_windows[i].rg == rg)
				c += snprintf(&buff[c], PRINT_BUFF_SIZE - c,
					"%d", i);
			else
				c += snprintf(&buff[c], PRINT_BUFF_SIZE - c,
						i < 10 ? " ":"  ");
		}

		c += snprintf(&buff[c], PRINT_BUFF_SIZE - c, "]\n");
	}

	dev_info(dev, "Resource Group Assignment:-\n%s", buff);
}

/**
 * pack_data_into_register_block() - Copy data from structures into the register
 * block, bit packing if necessary.
 * @dev: device
 * @assign: pointer to a _gpu_assign struct which this function will read from.
 * @base_addr: pointer to the start of the register block, from which we offset
 * to set the individual registers.
 *
 */
static void pack_data_into_register_block(
	struct device *dev,
	struct _gpu_assign *assign,
	void __iomem *base_addr)
{
	int i;
	u32 value;

	/* Set the busses for each resource group */
	value = 0;
	for (i = 0; i < assign->rg_count; ++i)
		value |= (assign->res_grps[i].bus & BUS_MASK) << (i * BUS_BITS);

	writel(value, base_addr + PTM_ASSIGN_RESOURCE_GROUP_BUS);

	/* Set the group for each partition */
	value = 0;
	for (i = 0; i < assign->partition_count; ++i) {
		value |= (assign->partitions[i].rg & GROUP_MASK) <<
				(i * GROUP_BITS);
	}
	writel(value, base_addr + PTM_ASSIGN_PARTITION_RESOURCE_GROUP);

	/* Set the group for each access window (first DWORD) */
	value = 0;
	for (i = 0; i < assign->aw_count && i < AWS_PER_DWORD; ++i) {
		value |= (assign->access_windows[i].rg & GROUP_MASK) <<
				(i * GROUP_BITS);
	}
	writel(value, base_addr + PTM_ASSIGN_AW_RESOURCE_GROUP);

	/* Set the group for each access window (second DWORD) */
	value = 0;
	for (i = AWS_PER_DWORD; i < assign->aw_count; ++i) {
		value |= (assign->access_windows[i].rg & GROUP_MASK) <<
				(i - AWS_PER_DWORD)  * GROUP_BITS;
	}
	writel(value, base_addr + PTM_ASSIGN_AW_RESOURCE_GROUP + 0x4);

	/* Set the group for each slice */
	value = 0;
	for (i = 0; i < assign->slice_count; ++i) {
		value |= (assign->slices[i].rg & GROUP_MASK) <<
				(i * GROUP_BITS);
	}
	writel(value, base_addr + PTM_ASSIGN_SLICE_RESOURCE_GROUP);

	/* Set the isolation for each slice */
	value = 0;
	for (i = 0; i < assign->slice_count; ++i) {
		value |= (assign->slices[i].isolation_set & ISOLATION_MASK) <<
				(i * ISOLATION_BITS);
	}
	writel(value, base_addr + PTM_ASSIGN_SLICE_ISOLATION_SET);
}


/**
 * gpu_assign_probe() - Called when device is matched in device tree
 * @pdev: Platform device
 *
 * Read values from the device tree into this modules register block.
 *
 * Return: 0 if success or a Linux error code
 */
static int gpu_assign_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource res;
	unsigned long remap_size;
	void __iomem *base_addr;
	struct device *dev;
	struct _gpu_assign assign;
	u32 value;
	struct mali_gpu_ptm_assign *ptm_assign_data;

	dev = &pdev->dev;

	if (!ptm_config)
		dev_warn(dev,
			"No command line configuration set, using defaults\n");

	ptm_assign_data = devm_kzalloc(dev, sizeof(struct mali_gpu_ptm_assign),
			GFP_KERNEL);
	if (!ptm_assign_data)
		return -ENOMEM;

	ret = of_address_to_resource(dev->of_node, 0, &res);
	if (ret) {
		dev_err(dev, "Failed to fetch address for resource 0\n");
		ret = -ENOENT;
		goto fail_resources;
	}

	remap_size = resource_size(&res);

	if (!request_mem_region(res.start, remap_size, pdev->name)) {
		dev_err(dev,
			"req mem reg(%pa,%lu,%s) FAILED! Reg window unavailable"
			, &res.start, remap_size, pdev->name);
		ret = -EIO;
		goto fail_resources;
	}

	base_addr = ioremap(res.start, remap_size);
	if (!base_addr) {
		dev_err(dev,
			"ioremap(%pa,%lu) FAILED! Can't remap register window",
				&res.start, remap_size);
		ret = -EIO;
		goto fail_ioremap;
	}

	value = readl(base_addr + PTM_DEVICE_ID);
	if (!check_ptm_version(value)) {
		dev_err(dev, "Unsupported partition manager version.\n");
		ret = -ENODEV;
		goto fail_getdata;
	}

	if (!init_data(dev, &assign, base_addr)) {
		dev_err(dev, "init_data() failed\n");
		ret = -ENODEV;
		goto fail_getdata;
	}

	if (ptm_config && !get_data_from_command_line(
		dev, &assign, ptm_config)) {
		ret = -ENODEV;
		goto fail_getdata;
	}

	if (!validate_data(dev, &assign)) {
		ret = -ENODEV;
		goto fail_getdata;
	}

	print_config(dev, &assign);

	pack_data_into_register_block(dev, &assign, base_addr);

	ptm_assign_data->dev = dev;
	ptm_assign_data->reg_res = res;
	ptm_assign_data->base_addr = base_addr;
	dev_set_drvdata(dev, ptm_assign_data);

	dev_info(dev, "Probed\n");
	return ret;

fail_getdata:
	iounmap(base_addr);
fail_ioremap:
	release_mem_region(res.start, remap_size);
fail_resources:
	return ret;
}

/**
 * gpu_assign_remove() - Called when device is removed
 * @pdev: Platform device
 *
 * Return: 0
 */
static int gpu_assign_remove(struct platform_device *pdev)
{
	struct mali_gpu_ptm_assign *ptm_assign_data;

	ptm_assign_data = dev_get_drvdata(&pdev->dev);
	if (ptm_assign_data) {
		iounmap(ptm_assign_data->base_addr);
		release_mem_region(ptm_assign_data->reg_res.start,
			resource_size(&ptm_assign_data->reg_res));
		dev_set_drvdata(&pdev->dev, NULL);
		devm_kfree(&pdev->dev, ptm_assign_data);
	}
	return 0;
}

/*
 * gpu_assign_dt_match: Match the platform device with the Device Tree.
 */
static const struct of_device_id gpu_assign_dt_match[] = {
	{ .compatible = MALI_ASSIGN_DT_NAME },
	{}
};

/*
 * gpu_assign_driver: Platform driver data.
 */
static struct platform_driver gpu_assign_driver = {
	.probe = gpu_assign_probe,
	.remove = gpu_assign_remove,
	.driver = {
		.name = "mali_gpu_assign",
		.of_match_table = gpu_assign_dt_match,
	},
};

/**
 * gpu_assign_init() - Register platform driver
 *
 * Return: struct platform_device pointer on success, or ERR_PTR()
 */
static int __init gpu_assign_init(void)
{
	return platform_driver_register(&gpu_assign_driver);
}
module_init(gpu_assign_init);

/**
 * gpu_assign_exit() - Unregister platform driver
 */
static void __exit gpu_assign_exit(void)
{
	platform_driver_unregister(&gpu_assign_driver);
}
module_exit(gpu_assign_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("mali-gpu-assign");
