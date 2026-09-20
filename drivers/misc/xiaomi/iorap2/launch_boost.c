
// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024-2024 MI. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>

#include <trace/hooks/mm.h>

#include "mi_lb_def.h"
#include "launch_boost_collect.h"
#include "launch_boost_preread.h"
#include "launch_boost_com.h"
#include "launch_boost_debug.h"
#include "launch_boost_clear.h"
#include "launch_boost_flush.h"

#include "pte_hot_preread.h"

#define MAX_PREREAD_BUFFER_SIZE (10 * 1024 * 1024UL)


static struct lb_data g_lb_data;

static void mi_lb_unregister_hooks(void)
{
        unregister_trace_android_vh_filemap_read(
                        mi_lb_page_collect_on_readfile, NULL);
        unregister_trace_android_vh_filemap_map_pages_range(
                        mi_lb_page_collect_on_pagefault, NULL);
}

static int mi_lb_register_hooks(void)
{
        int ret = 0;

        ret = register_trace_android_vh_filemap_read(
                        mi_lb_page_collect_on_readfile, NULL);
        if (ret) {
                MI_LB_ERR("register hook mi_lb_page_collect_on_readfile failed");
                goto err;
        }

        ret = register_trace_android_vh_filemap_map_pages_range(
                        mi_lb_page_collect_on_pagefault, NULL);
        if (ret) {
                MI_LB_ERR("register hook mi_lb_page_collect_on_pagefault failed");
                goto err_unregister1;
        }

        return 0;

err_unregister1:
        unregister_trace_android_vh_filemap_read(
                        mi_lb_page_collect_on_readfile, NULL);
err:
        return ret;
}

static long mi_lb_ioctl(struct file *filp, uint32_t cmd, unsigned long arg)
{
        int ret = 0;
	struct lb_owner owner;
        struct lb_app_preread_info_size app_preread_info_size;
        unsigned long buf_size;
        struct lb_app_preread_info *app_preread_info;
        struct lb_app_list *app_list;
        void __user *argp = (void __user *) arg;
        struct lb_app_preread_info __user *user_app_preread_info;

        switch (cmd) {
        case LB_PREREAD:
                if (copy_from_user(&owner, (unsigned long *)arg, sizeof(struct lb_owner))) {
                        MI_LB_ERR("LB_PREREAD copy from user fail");
                        return -EFAULT;
                }
                debug_owner.uid = owner.uid;
                #ifdef MI_LB_PREREAD
                ret = mi_lb_dealwith_preread_cmd(&owner);
                if (ret)
                        MI_LB_ERR("lb preread cmd fail");
                #endif
                break;
        case LB_COLLECT:
                if (copy_from_user(&owner, (unsigned long *)arg, sizeof(struct lb_owner))) {
                	MI_LB_ERR("copy from user fail");
                        return -EFAULT;
		}
                ret = mi_lb_dealwith_collect_cmd(APP_COLLECT_START, &owner);
                if (ret)
                        MI_LB_ERR("lb collect cmd fail");
                break;
        case LB_GET_PREREAD_INFO_SIZE:
                if(g_only_collect)
                        return -1;
                if (copy_from_user(&app_preread_info_size,
                                   (unsigned long *)arg, sizeof(struct lb_app_preread_info_size))) {
                        MI_LB_ERR("copy from user fail");
                        return -EFAULT;
                }
                ret = mi_lb_dealwith_preread_size_cmd(&app_preread_info_size);
                if (ret < -1) {
                        MI_LB_ERR("lb preread size fail");
                        return -1;
                }
                if (ret == -1) {
                        MI_LB_ERR("lb app %s cold start", app_preread_info_size.owner.package_name);
                        return -2;
                }
                if (copy_to_user(argp, &app_preread_info_size, sizeof(app_preread_info_size))) {
                        MI_LB_ERR("LB_GET_PREREAD_INFO_SIZE: copy to user fail");
                        return -EFAULT;
                }
                break;
        case LB_GET_PREREAD_INFO:
                user_app_preread_info = (struct lb_app_preread_info __user *) arg;
                if (get_user(buf_size, &user_app_preread_info->file_info_buf_size)) {
                        MI_LB_ERR("get_user fail");
                        return -EFAULT;
                }

                if (buf_size > MAX_PREREAD_BUFFER_SIZE) {
                        MI_LB_ERR("buf_size %lu too large", buf_size);
                        return -EINVAL;
                }

                app_preread_info = kvmalloc(sizeof(struct lb_app_preread_info)
                                            + buf_size, GFP_KERNEL);
                if (!app_preread_info) {
	                MI_LB_ERR("alloc app_preread_info err");
                        return -ENOMEM;
                }
                if (copy_from_user(app_preread_info, (unsigned long *)arg, sizeof(struct lb_app_preread_info))) {
        	        MI_LB_ERR("copy from user fail");
                        kvfree(app_preread_info);
        	        return -EFAULT;
	        }
                ret = mi_lb_dealwith_preread_info_cmd(app_preread_info);
                if (ret) {
	                MI_LB_ERR("lb pre read fail");
                        kvfree(app_preread_info);
                        return -1;
                }
                if (copy_to_user(argp, app_preread_info, app_preread_info->file_info_buf_size)) {
                        MI_LB_ERR("LB_GET_PREREAD_INFO_SIZE: copy to user fail");
                        ret = -EFAULT;
                }
                kvfree(app_preread_info);
                break;
        case LB_STOP:
                if (copy_from_user(&owner, (unsigned long *)arg, sizeof(struct lb_owner))) {
  	                MI_LB_ERR("copy from user fail");
  	                return -EFAULT;
                }
                debug_owner.uid = -1;
                ret = mi_lb_dealwith_collect_cmd(APP_LAUNCH_FINISH,  &owner);
                if (ret)
                        MI_LB_ERR("lb collect finish fail");
                break;
        case LB_ABORT:
                if (copy_from_user(&owner, (unsigned long *)arg, sizeof(struct lb_owner))) {
                        MI_LB_ERR("copy from user fail");
                        return -EFAULT;
                }
                debug_owner.uid = -1;
                ret = mi_lb_dealwith_collect_cmd(APP_LAUNCH_ABORT,  &owner);
                if (ret)
                        MI_LB_ERR("lb collect abort fail");
		break;
        case LB_CLEAR:
                if (copy_from_user(&owner, (unsigned long *)arg, sizeof(struct lb_owner))) {
                	MI_LB_ERR("copy from user fail");
                	return -EFAULT;
		}
		mi_lb_dealwith_pte_clean_cmd(&owner);
		mi_lb_dealwith_clear_cmd(&owner);
		break;
        case LB_GET_APP_NAMES:
                app_list = kvmalloc(sizeof(struct lb_app_list), GFP_KERNEL);
                if (!app_list) {
	                MI_LB_ERR("alloc app_list err");
                        return -ENOMEM;
                }
                ret = mi_lb_dealwith_get_app_names(app_list);
                if (ret)
                        MI_LB_ERR("lb get app names fail");
                if (copy_to_user(argp, app_list, sizeof(struct lb_app_list))) {
                        MI_LB_ERR("LB_GET_APP_NAMES: copy to user fail");
                        ret = -EFAULT;
                }
                kvfree(app_list);
                break;
        case LB_PTE_SNAPSHOT:
                if (copy_from_user(&owner, (unsigned long *)arg, sizeof(struct lb_owner))) {
                	MI_LB_ERR("copy from user fail");
                	return -EFAULT;
		}
                ret = mi_lb_dealwith_pte_snapshot_cmd(&owner);
                if (ret)
			MI_LB_ERR("lb pte snapshot fail");
                break;
        case LB_PTE_PREREAD:
                if (copy_from_user(&owner, (unsigned long *)arg, sizeof(struct lb_owner))) {
                	MI_LB_ERR("copy from user fail");
                	return -EFAULT;
		}
                ret = mi_lb_dealwith_pte_preread_cmd(&owner);
                if (ret)
			MI_LB_ERR("lb pte preread fail");
                break;
	case LB_PTE_INTERRUPT:
		if (copy_from_user(&owner, (unsigned long *)arg, sizeof(struct lb_owner))) {
			MI_LB_ERR("copy from user fail");
			return -EFAULT;
		}
		ret = mi_lb_dealwith_pte_interrupt_cmd(&owner);
		if (ret)
			MI_LB_ERR("lb pte interrupt fail");
		break;
        default:
                MI_LB_ERR("unknown cmd, cmd is %u", cmd);
                ret = -ENOIOCTLCMD;
                break;
        }

        return ret;
}


static const struct file_operations lb_fops = {
        .owner = THIS_MODULE,
        .open = NULL,
        .release = NULL,
        .compat_ioctl = mi_lb_ioctl,
        .unlocked_ioctl = mi_lb_ioctl,
};

static int mi_lb_chrdev_register(struct lb_data *data)
{
        int ret = 0;

        data->chr_major = register_chrdev(0, LB_DEV_NAME, &lb_fops);
        if (data->chr_major < 0) {
                MI_LB_ERR("register chrdev fail\n");
                return -ENXIO;
        }
        data->chr_class = class_create("iorap");
        if (IS_ERR_OR_NULL(data->chr_class)) {
                MI_LB_ERR("class create fail");
                ret = PTR_ERR(data->chr_class);
                unregister_chrdev(data->chr_major, LB_DEV_NAME);
                return ret;
        }
        data->chr_dev = device_create(
                data->chr_class, 0, MKDEV(data->chr_major, 0), NULL, LB_DEV_NAME);
        if (IS_ERR_OR_NULL(data->chr_class)) {
                MI_LB_ERR("device create fail");
                ret = PTR_ERR(data->chr_class);
                unregister_chrdev(data->chr_major, LB_DEV_NAME);
                return ret;
        }

        dev_set_drvdata(data->chr_dev, data);

        return 0;
}

static int lb_cold_init(void)
{
        int ret;
	struct lb_data *data = &g_lb_data;

	memset(data, 0, sizeof(*data));
        ret = mi_lb_chrdev_register(data);
        if (ret)
                return ret;

	ret = mi_lb_init_manager();
	if (ret)
                return ret;

        ret = mi_lb_register_hooks();
	if (ret)
		return ret;
        return ret;
}

static void lb_cold_exit(void)
{
        mi_lb_unregister_hooks();
        mi_lb_exit_manager();
        if (g_lb_data.chr_class)
                class_destroy(g_lb_data.chr_class);
        if (g_lb_data.chr_dev)
                unregister_chrdev(g_lb_data.chr_major, LB_DEV_NAME);
}

static int __init mi_lb_init(void)
{
        int ret;

        ret = lb_cold_init();
        if (ret) {
                MI_LB_ERR("lb_cold_init failed\n");
                goto err_cold_init;
        }
        ret = lb_hot_init();
        if (ret) {
                MI_LB_ERR("lb_cold_init failed\n");
                goto err_hot_init;
        }

        ret = mi_lb_dbg_init();
	if (ret) {
	        MI_LB_ERR("dbg init failed\n");
                goto err_dbg_init;
	}

        return 0;
err_dbg_init:
        lb_hot_exit();
err_hot_init:
        lb_cold_exit();
err_cold_init:
        return ret;
}

static void __exit mi_lb_exit(void)
{
        lb_hot_exit();
        lb_cold_exit();
}

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

MODULE_LICENSE("GPL v2");
module_init(mi_lb_init);
module_exit(mi_lb_exit);
