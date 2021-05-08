/*
 *
 * Copyright (C) 2019 wanghan <wanghan@longcheer.com>
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>

/*
 *
 * String :
 *     'androidboot.board.new_version='
 * Format :
 *     'androidboot.board.new_version=<number>'
 * Example :
 *     androidboot.board.new_version=0
 *     androidboot.board.new_version=1
)
 *******************************************************************
 */
#if defined(CONFIG_TARGET_PROJECT_K7B)
static int board_new_version = 0;

int get_board_new_version(void)
{
	return board_new_version;
}
EXPORT_SYMBOL(get_board_new_version);

static int __init setup_board_new_version(char *str)
{
	board_new_version = simple_strtol(str, NULL, 10);
	pr_info("board_id_hwlevel : %d\n", board_new_version);
	return 1;
}
__setup("androidboot.board.new_version=", setup_board_new_version);
#endif
/*
 *
 * String :
 *     'androidboot.hwname='
 * Format :
 *     'androidboot.hwname=<string>'
 * Example :
 *     androidboot.hwname=secret
 *     androidboot.hwname=rosemary
 *     androidboot.hwname=maltose
 *******************************************************************
 */
static char board_id_hwname[32] = {0};

void board_id_get_hwname(char *str)
{
	if(IS_ERR_OR_NULL(str))
		return;
	strcpy(str, board_id_hwname);
}
EXPORT_SYMBOL(board_id_get_hwname);

static int __init setup_board_id_hwname(char *str)
{
	strcpy(board_id_hwname, str);
	pr_info("board_id_hwname : %s\n", board_id_hwname);
	return 1;
}
__setup("androidboot.hwname=", setup_board_id_hwname);

/*
 *
 * String :
 *     'androidboot.hwversion='
 * Format :
 *     'androidboot.hwversion=<number/string>'
 * Example :
 *     androidboot.hwlevel=0
 *     androidboot.hwlevel=1
 *     androidboot.hwlevel=2
 *     androidboot.hwlevel=MP (9)
 *******************************************************************
 */

static int board_id_hwlevel;

int board_id_get_hwlevel(void)
{
	return board_id_hwlevel;
}
EXPORT_SYMBOL(board_id_get_hwlevel);

static int __init setup_board_id_hwlevel(char *str)
{
	if (!strcmp("MP", str)) {
		board_id_hwlevel = 9;
		pr_info("board_id_hwlevel : %d (MP)\n", board_id_hwlevel);
		return 1;
	}
	board_id_hwlevel = simple_strtol(str, NULL, 10);
	pr_info("board_id_hwlevel : %d\n", board_id_hwlevel);
	return 1;
}
__setup("androidboot.hwlevel=", setup_board_id_hwlevel);

/*
 *
 * String :
 *     'androidboot.hwversion='
 * Format :
 *     xx.xx.xx < product_number . major_number . minor_number >
 * Example :
 *     androidboot.hwversion=xx.xx.xx // product  , major, minor
 *     androidboot.hwversion=1.10.0   // secret   , 13   , 0
 *     androidboot.hwversion=1.20.0   // secret   , 0    , 0
 *     androidboot.hwversion=1.30.0   // secret   , 10   , 0
 *     androidboot.hwversion=2.10.0   // rosemary , 20   , 0
 *     androidboot.hwversion=3.10.0   // maltose  , 90   , 0
 *******************************************************************
 */

static int board_id_hwversion_product_num;
static int board_id_hwversion_major_num;
static int board_id_hwversion_minor_num;

int board_id_get_hwversion_product_num(void)
{
	return board_id_hwversion_product_num;
}
EXPORT_SYMBOL(board_id_get_hwversion_product_num);

int board_id_get_hwversion_major_num(void)
{
	return board_id_hwversion_major_num;
}
EXPORT_SYMBOL(board_id_get_hwversion_major_num);

int board_id_get_hwversion_minor_num(void)
{
	return board_id_hwversion_minor_num;
}
EXPORT_SYMBOL(board_id_get_hwversion_minor_num);

static int __init setup_board_id_hwversion(char *str)
{
	char *str_p = NULL;
	char *str_n = NULL;
	char buf[32] = {0};
	strcpy(buf, str);
	str_n = buf;
	str_p = strsep(&str_n, ".");
	board_id_hwversion_product_num = simple_strtoul(str_p, NULL, 10);
	str_p = strsep(&str_n, ".");
	board_id_hwversion_major_num = simple_strtoul(str_p, NULL, 10);
	board_id_hwversion_minor_num = simple_strtoul(str_n, NULL, 10);
	pr_info("board_id_hwversion_product_num : %d\n", board_id_hwversion_product_num);
	pr_info("board_id_hwversion_major_num : %d\n", board_id_hwversion_major_num);
	pr_info("board_id_hwversion_minor_num : %d\n", board_id_hwversion_minor_num);
	return 1;
}
__setup("androidboot.hwversion=", setup_board_id_hwversion);

