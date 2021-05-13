// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include "ccci_util_lib_main.h"

/*------------------------------------------*/
#define MD_CAP_BIT_NUM		(7)

/*------------------------------------------*/
/* Bit map defination at MD side diff to AP */
/* 0 | 0 | Lf | Lt | W | C | T | G          */
#define MD_CAP_GSM			(1<<0)
#define MD_CAP_TDS_CDMA		(1<<1)
#define MD_CAP_WCDMA		(1<<3)
#define MD_CAP_TDD_LTE		(1<<4)
#define MD_CAP_FDD_LTE		(1<<5)
#define MD_CAP_CDMA2000		(1<<2)
#define MD_CAP_NR		(1<<6)

#define MD_CAP_FULL_SET		(MD_CAP_GSM|MD_CAP_TDS_CDMA| \
				MD_CAP_WCDMA|MD_CAP_TDD_LTE| \
				MD_CAP_FDD_LTE|MD_CAP_CDMA2000|MD_CAP_NR)

static const unsigned int md_img_capability_map[] = { /* At check header */

	/* 0 - None */
	0,

	/* 1 - G */
	MD_CAP_GSM,

	/* 2 - W */
	MD_CAP_WCDMA,

	/* 3 - WG = W + G */
	(MD_CAP_WCDMA|MD_CAP_GSM),

	/* 4 - TG = T + G */
	(MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* 5 - LWG = Lf + Lt + T + G */
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA|MD_CAP_GSM),

	/* 6 - LTG = Lf + Lt + T + G */
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* 7 - SGLTE = Lf + Lt + T + G (Phase out)*/
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* 8 - LTG = Lf + Lt + T + G */
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* 9 - LWG = Lf + Lt + W + G */
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA|MD_CAP_GSM),

	/* 10 - LWTG = Lf + Lt + W + T + G */
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* 11 - LWCG = Lf + Lt + W + C + G */
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA|MD_CAP_CDMA2000|MD_CAP_GSM),

	/* 12 - LWCTG = N + Lf + Lt + W + C + T + G */
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA|MD_CAP_CDMA2000
	|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* 13 - NLWTG = N + Lf + Lt + W + T + G */
	(MD_CAP_NR|MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA
	|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* 14 - NLWCG = N + Lf + Lt + W + C + G */
	(MD_CAP_NR|MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA
	|MD_CAP_CDMA2000|MD_CAP_GSM),
};

static const unsigned int md_rat_map[] = { /* Options supplied by MD */
	/* 0 ~ 7 not userd */
	0, 0, 0, 0, 0, 0, 0, 0,

	/* ultg - 8 */
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* ulwg - 9 */
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA|MD_CAP_GSM),

	/* ulwtg - 10 */
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* ulwcg - 11 */
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA|MD_CAP_CDMA2000|MD_CAP_GSM),

	/* ulwctg -12 */
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA|MD_CAP_CDMA2000
	|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* ulttg - 13 */
	(MD_CAP_TDD_LTE|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* ulfwg - 14 */
	(MD_CAP_FDD_LTE|MD_CAP_WCDMA|MD_CAP_GSM),

	/* ulfwcg - 15 */
	(MD_CAP_FDD_LTE|MD_CAP_WCDMA|MD_CAP_CDMA2000|MD_CAP_GSM),

	/* ulctg - 16 */
	(MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_CDMA2000|MD_CAP_TDS_CDMA
	|MD_CAP_GSM),

	/* ultctg - 17 */
	(MD_CAP_TDD_LTE|MD_CAP_CDMA2000|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/*ultwg - 18 */
	(MD_CAP_TDD_LTE|MD_CAP_WCDMA|MD_CAP_GSM),

	/* ultwcg -19 */
	(MD_CAP_TDD_LTE|MD_CAP_WCDMA|MD_CAP_CDMA2000|MD_CAP_GSM),

	/* ulftg - 20 */
	(MD_CAP_FDD_LTE|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* ulfctg - 21 */
	(MD_CAP_FDD_LTE|MD_CAP_CDMA2000|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* unlwg - 22 */
	(MD_CAP_NR|MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA
	|MD_CAP_GSM),

	/* unlwtg - 23 */
	(MD_CAP_NR|MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA
	|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* unlwctg - 24 */
	(MD_CAP_NR|MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA|MD_CAP_CDMA2000
	|MD_CAP_TDS_CDMA|MD_CAP_GSM),

	/* unlwcg - 25 */
	(MD_CAP_NR|MD_CAP_FDD_LTE|MD_CAP_TDD_LTE|MD_CAP_WCDMA
	|MD_CAP_CDMA2000|MD_CAP_GSM),

	/* unltctg - 26 */
	(MD_CAP_NR|MD_CAP_TDD_LTE|MD_CAP_CDMA2000
	|MD_CAP_TDS_CDMA|MD_CAP_GSM),
};

static unsigned int get_bit_set_num(unsigned int bitmap)
{
	unsigned int i;
	unsigned int num = 0;
	unsigned int curr_bit = 1;

	for (i = 0; i < MD_CAP_BIT_NUM; i++) {
		if (bitmap & curr_bit)
			num++;
		curr_bit = curr_bit << 1;
	}
	return num;
}

static unsigned int find_prefer_val(unsigned int ref_rat)
{
	unsigned int i;
	unsigned int min_bitmap, min_bitmap_num, num;

	for (i = 0; i < ARRAY_SIZE(md_rat_map); i++) {
		if (ref_rat == md_rat_map[i])
			return ref_rat;
	}

	/* Use the most similar settings */
	min_bitmap = MD_CAP_FULL_SET;
	min_bitmap_num = get_bit_set_num(min_bitmap);

	for (i = 0; i < ARRAY_SIZE(md_rat_map); i++) {
		if ((ref_rat & md_rat_map[i]) != ref_rat)
			continue;
		num =  get_bit_set_num(md_rat_map[i]);
		if (min_bitmap_num > num) {
			min_bitmap = md_rat_map[i];
			min_bitmap_num = num;
		}
	}
	return min_bitmap;
}

static unsigned int get_capability_bit(char cap_str[])
{
	if (cap_str == NULL)
		return 0;

	if ((strcmp(cap_str, "N") == 0) || (strcmp(cap_str, "n") == 0))
		return MD_CAP_NR;

	if ((strcmp(cap_str, "LF") == 0) || (strcmp(cap_str, "Lf") == 0)
		|| (strcmp(cap_str, "lf") == 0))
		return MD_CAP_FDD_LTE;

	if ((strcmp(cap_str, "LT") == 0) || (strcmp(cap_str, "Lt") == 0)
		|| (strcmp(cap_str, "lt") == 0))
		return MD_CAP_TDD_LTE;

	if ((strcmp(cap_str, "W") == 0) || (strcmp(cap_str, "w") == 0))
		return MD_CAP_WCDMA;

	if ((strcmp(cap_str, "C") == 0) || (strcmp(cap_str, "c") == 0))
		return MD_CAP_CDMA2000;

	if ((strcmp(cap_str, "T") == 0) || (strcmp(cap_str, "t") == 0))
		return MD_CAP_TDS_CDMA;

	if ((strcmp(cap_str, "G") == 0) || (strcmp(cap_str, "g") == 0))
		return MD_CAP_GSM;

	return 0;
}

#define MAX_CAP_STR_LENGTH    64
static unsigned int ccci_rat_str_to_bitmap(char str[])
{
	char tmp_str[MAX_CAP_STR_LENGTH];
	unsigned int tmp_str_curr_pos = 0;
	unsigned int bitmap = 0;
	int str_len;
	int i;

	if (str == NULL)
		return 0;

	str_len = strlen(str);
	for (i = 0; i < str_len; i++) {
		if (str[i] == ' ')
			continue;
		if (str[i] == '\t')
			continue;
		if ((str[i] == '/') || (str[i] == '\\') || (str[i] == '_')) {
			if (tmp_str_curr_pos) {
				tmp_str[tmp_str_curr_pos] = 0;
				bitmap |= get_capability_bit(tmp_str);
			}
			tmp_str_curr_pos = 0;
			continue;
		}
		if (tmp_str_curr_pos < (MAX_CAP_STR_LENGTH-1)) {
			tmp_str[tmp_str_curr_pos] = str[i];
			tmp_str_curr_pos++;
		} else
			break;
		}

	if (tmp_str_curr_pos) {
		tmp_str[tmp_str_curr_pos] = 0;
		bitmap |= get_capability_bit(tmp_str);
	}

	return bitmap;
}

unsigned int get_md_bin_capability(int md_id)
{
	int img_type;

	img_type = get_md_img_type(md_id);
	if (img_type < 0)
		return 0;

	if (img_type < ARRAY_SIZE(md_img_capability_map))
		return md_img_capability_map[img_type];

	return 0;
}
EXPORT_SYMBOL(get_md_bin_capability);

int check_rat_at_md_img(int md_id, char str[])
{
	unsigned int rat, cap;

	rat = get_capability_bit(str);
	cap = get_md_bin_capability(md_id);

	if (rat & cap)
		return 1;
	return 0;
}
EXPORT_SYMBOL(check_rat_at_md_img);


static unsigned int s_md_rt_wm_id[MAX_MD_NUM_AT_LK];

void set_soc_md_rt_rat(int md_id, unsigned int bitmap)
{
	if ((md_id < 0) || (md_id >= MAX_MD_NUM_AT_LK))
		return;
	s_md_rt_wm_id[md_id] = bitmap;
}
EXPORT_SYMBOL(set_soc_md_rt_rat);

unsigned int get_soc_md_rt_rat(int md_id)
{
	if ((md_id < 0) || (md_id >= MAX_MD_NUM_AT_LK))
		return 0;
	return s_md_rt_wm_id[md_id];
}
EXPORT_SYMBOL(get_soc_md_rt_rat);


int check_rat_at_rt_setting(int md_id, char str[])
{
	unsigned int rat, cap;

	rat = get_capability_bit(str);
	cap = get_soc_md_rt_rat(md_id);

	if (rat & cap)
		return 1;
	return 0;
}
EXPORT_SYMBOL(check_rat_at_rt_setting);

int set_soc_md_rt_rat_str(int md_id, char str[])
{
	unsigned int rat_bitmap, prefer, cap;

	cap = get_md_bin_capability(md_id);
	if (!str) {
		pr_info("CCCI: %s get NULL ptr!\n", __func__);
		set_soc_md_rt_rat(md_id, cap);
		return -1;
	}

	if (strlen(str) == 0) {
		pr_info("CCCI: %s str empty, set default value!\n", __func__);
		set_soc_md_rt_rat(md_id, cap);
		return -1;
	}

	rat_bitmap = ccci_rat_str_to_bitmap(str);
	prefer = find_prefer_val(rat_bitmap);

	if ((prefer == 0) || ((prefer & cap) != prefer)) {
		pr_info("CCCI:%s:rat[%s](r:0x%x|p:0x%x|c:0x%x) not support!\n",
				__func__, str, rat_bitmap, prefer, cap);
		set_soc_md_rt_rat(md_id, cap);
		return -1;
	}
	set_soc_md_rt_rat(md_id, prefer);
	pr_info("CCCI:%s:rat[%s](r:0x%x|p:0x%x|c:0x%x)\n",
				__func__, str, rat_bitmap, prefer, cap);
	return 0;
}
EXPORT_SYMBOL(set_soc_md_rt_rat_str);
