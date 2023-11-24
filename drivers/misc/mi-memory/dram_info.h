#ifndef __DRAM_INFO_H__
#define __DRAM_INFO_H__
#include <linux/string.h>
#include <linux/types.h>

#define DDR_IS_POP_PACKAGE 1

struct mr_info_t {
	unsigned int mr_index;
	unsigned int mr_value;
};

struct dram_info_t
{
    char ddr_vendor[8];
	unsigned int ddr_type;
	unsigned int ddr_size;
	unsigned int ddr_id;
	unsigned int support_ch_cnt;
	unsigned int ch_cnt;
	unsigned int rk_cnt;
	unsigned int mr_cnt;
	unsigned int freq_cnt;
	unsigned int *rk_size;
	unsigned int *freq_step;
	unsigned int mr4_version;
	struct mr_info_t *mr_info_ptr;
};

//const struct attribute_group dram_sysfs_group;
struct dram_info_t *init_dram_info(void);
#endif