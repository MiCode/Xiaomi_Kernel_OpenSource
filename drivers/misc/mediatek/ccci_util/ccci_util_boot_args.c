// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/skbuff.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

#include "mt-plat/mtk_ccci_common.h"
#include "ccci_util_log.h"
#include "ccci_util_lib_main.h"


#define ARGS_KEY_VAL_KEY_SIZE		36
#define ARGS_KEY_VAL_BUF_NUM		8
#define ARGS_KEY_VAL_BUF_SIZE		4096
#define ARGS_KEY_VAL_MAX_KEY_NUM	73


struct args_key_val {
	char key[ARGS_KEY_VAL_KEY_SIZE];
	unsigned int key_size;
	unsigned int val_size;
	unsigned int source;
	unsigned char *pdata;
};

static unsigned char *s_buf[ARGS_KEY_VAL_BUF_NUM];
static unsigned char *s_curr_ptr;
static unsigned int s_curr_buf_idx;
static unsigned int s_curr_buf_free;
static unsigned int s_args_num;
static unsigned int s_init_done;

static struct args_key_val s_args_tbl[ARGS_KEY_VAL_MAX_KEY_NUM];

static int is_key_exist(const char key[], unsigned int size)
{
	unsigned int i;

	for (i = 0; i < s_args_num; i++) {
		if (s_args_tbl[i].key_size != size)
			continue;

		if (strncmp(s_args_tbl[i].key, key, size) == 0)
			return i;
	}

	return -1;
}

int mtk_ccci_find_args_val(const char key[], unsigned char o_val[], unsigned int val_buf_size)
{
	int idx;
	unsigned int key_size;

	if (!s_init_done) {
		pr_info("ccci: %s(line:%d): Not init done\n", __func__, __LINE__);
		return -1;
	}

	if (!key) {
		pr_info("ccci: %s(line:%d): Key is NULL\n", __func__, __LINE__);
		return -1;
	}

	key_size = strlen(key) + 1;
	if (key_size > ARGS_KEY_VAL_KEY_SIZE) {
		pr_info("ccci: %s(line:%d): Key[%s] size is too big\n", __func__, __LINE__, key);
		return -1;
	}


	idx = is_key_exist(key, key_size);
	if (idx < 0) {
		pr_info("ccci: %s(line:%d): Key[%s] not exist\n", __func__, __LINE__, key);
		return -1;
	}

	if (!o_val)
		return (int)s_args_tbl[idx].val_size;

	if (val_buf_size < s_args_tbl[idx].val_size) {
		pr_info("ccci: %s(line:%d): Val buf size is not enough(%u:%u) for key[%s]\n",
				__func__, __LINE__, s_args_tbl[idx].val_size, val_buf_size, key);
		return -1;
	}

	if (s_args_tbl[idx].source == FROM_LK_TAG)
		memcpy_fromio(o_val, s_args_tbl[idx].pdata, s_args_tbl[idx].val_size);
	else
		memcpy(o_val, s_args_tbl[idx].pdata, s_args_tbl[idx].val_size);

	return (int)s_args_tbl[idx].val_size;
}
EXPORT_SYMBOL(mtk_ccci_find_args_val);

int mtk_ccci_add_new_args(const char key[], unsigned char val[],
				unsigned int val_size, enum args_src source)
{
	unsigned int align_sz;
	unsigned int key_size;

	if (!s_init_done) {
		pr_info("ccci: %s(line:%d): Not init done\n", __func__, __LINE__);
		return -1;
	}

	if (!key) {
		pr_info("ccci: %s(line:%d): Key is NULL\n", __func__, __LINE__);
		return -1;
	}
	key_size = strlen(key) + 1;
	if (key_size > ARGS_KEY_VAL_KEY_SIZE) {
		pr_info("ccci: %s(line:%d): Key size is too big\n", __func__, __LINE__);
		return -1;
	}
	if (!val) {
		pr_info("ccci: %s(line:%d): Val is NULL\n", __func__, __LINE__);
		return -1;
	}

	if (is_key_exist(key, key_size) >= 0) {
		pr_info("ccci: %s(line:%d): Get duplicate key: %s\n",
				__func__, __LINE__, key);
		return -1;
	}

	if (source == FROM_LK_TAG) {
		memcpy(s_args_tbl[s_args_num].key, key, key_size);
		s_args_tbl[s_args_num].key_size = key_size;
		s_args_tbl[s_args_num].val_size = val_size;
		s_args_tbl[s_args_num].pdata = val;
		s_args_tbl[s_args_num].source = source;

		s_args_num++;

		return (int)val_size;
	}

	if (val_size > ARGS_KEY_VAL_BUF_SIZE) {
		pr_info("ccci: %s(line:%d): Val size is too big\n", __func__, __LINE__);
		return -1;
	}

	if (val_size > s_curr_buf_free) {
		s_curr_buf_idx++;
		if (s_curr_buf_idx >= ARGS_KEY_VAL_BUF_NUM) {
			pr_info("ccci: %s(line:%d): buff number is %d\n",
					__func__, __LINE__, ARGS_KEY_VAL_BUF_NUM);
			return -1;
		}
		s_buf[s_curr_buf_idx] = kmalloc(ARGS_KEY_VAL_BUF_SIZE, GFP_KERNEL);
		if (!s_buf[s_curr_buf_idx])
			return -1;
		s_curr_buf_free = ARGS_KEY_VAL_BUF_SIZE;
		s_curr_ptr = s_buf[s_curr_buf_idx];
	}

	memcpy(s_args_tbl[s_args_num].key, key, key_size);
	memcpy(s_curr_ptr, val, val_size);
	s_args_tbl[s_args_num].key_size = key_size;
	s_args_tbl[s_args_num].val_size = val_size;
	s_args_tbl[s_args_num].pdata = s_curr_ptr;
	s_args_tbl[s_args_num].source = source;

	s_args_num++;
	align_sz = (val_size + 8) & (~7);
	s_curr_ptr += val_size + align_sz;
	if (s_curr_buf_free > (val_size + align_sz))
		s_curr_buf_free = s_curr_buf_free - val_size - align_sz;
	else
		s_curr_buf_free = 0;

	return (int)val_size;
}
EXPORT_SYMBOL(mtk_ccci_add_new_args);


int mtk_ccci_args_key_val_init(void)
{
	struct device_node *node = NULL;
	int ret = 0;
	unsigned int dt_value = 0;

	s_curr_buf_idx = 0;
	s_buf[s_curr_buf_idx] = kmalloc(ARGS_KEY_VAL_BUF_SIZE, GFP_KERNEL);
	if (!s_buf[s_curr_buf_idx])
		return -1;
	s_curr_ptr = s_buf[s_curr_buf_idx];
	s_curr_buf_free = ARGS_KEY_VAL_BUF_SIZE;
	s_args_num = 0;
	s_init_done = 1;

	/* Add md relate dts info to args table */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mddriver");
	if (!node) {
		CCCI_UTIL_INF_MSG("Legacy chip: mediatek,mddriver node not support\n");
		return -1;
	}

	// From dts: AP Platform
	ret = of_property_read_u32(node, "mediatek,ap-plat-info", &dt_value);
	if (ret < 0)
		CCCI_UTIL_INF_MSG("Attr: [mediatek,ap-plat-info] not support\n");
	else
		mtk_ccci_add_new_args("ap_platform", (unsigned char *)&dt_value,
					(unsigned int)sizeof(unsigned int), FROM_KERNEL);

	// From dts: MD Generation
	dt_value = 0;
	ret = of_property_read_u32(node, "mediatek,md-generation", &dt_value);
	if (ret < 0)
		CCCI_UTIL_INF_MSG("Attr: [mediatek,md-generation] not support\n");
	else
		mtk_ccci_add_new_args("md_generation", (unsigned char *)&dt_value,
					(unsigned int)sizeof(unsigned int), FROM_KERNEL);

	return 0;
}

void mtk_ccci_dump_args_info(void)
{
	unsigned int i;
	unsigned int *ptr = NULL;
	unsigned char *ch;

	CCCI_UTIL_INF_MSG("------ Dump args: [key]:[val] --------\n");
	for (i = 0; i < s_args_num; i++) {
		if (strncmp(s_args_tbl[i].key, "err_trace", 9) == 0) {
			s_args_tbl[i].pdata[s_args_tbl[i].val_size] = 0;
			CCCI_UTIL_INF_MSG("<key>[err_trace(%u)]:[%s]\n", s_args_tbl[i].val_size,
						s_args_tbl[i].pdata);
		} else {
			if (s_args_tbl[i].val_size >= 8) {
				ptr = (unsigned int *)s_args_tbl[i].pdata;
				CCCI_UTIL_INF_MSG("<key>[%s(%u)]:[%x:%x]\n", s_args_tbl[i].key,
						s_args_tbl[i].val_size, ptr[0], ptr[1]);
			} else if (s_args_tbl[i].val_size >= 4) {
				ptr = (unsigned int *)s_args_tbl[i].pdata;
				CCCI_UTIL_INF_MSG("<key>[%s(%u)]:[%x]\n", s_args_tbl[i].key,
						s_args_tbl[i].val_size, ptr[0]);
			} else {
				ch = (unsigned char *)s_args_tbl[i].pdata;
				CCCI_UTIL_INF_MSG("<key>[%s(%u)]:[%x]\n", s_args_tbl[i].key,
						s_args_tbl[i].val_size, (unsigned int)(*ch));
			}
		}
	}
}
