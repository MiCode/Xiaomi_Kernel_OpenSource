/*
 * drivers/gpu/ion/compat_ion.c
 *
 * Copyright (C) 2013 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include "ion.h"
#include "compat_ion.h"
#include <linux/ion_drv.h>

/* See drivers/staging/android/uapi/ion.h for the definition of these structs */
struct compat_ion_allocation_data {
	compat_size_t len;
	compat_size_t align;
	compat_uint_t heap_id_mask;
	compat_uint_t flags;
	compat_int_t handle;
};

struct compat_ion_custom_data {
	compat_uint_t cmd;
	compat_ulong_t arg;
};

struct compat_ion_handle_data {
	compat_int_t handle;
};

//ion_custom_ioctl
struct compat_ion_sys_cache_sync_param {
	union
	{
		compat_int_t handle;
		compat_uptr_t kernel_handle;
	};
	compat_uptr_t va;
	compat_size_t size;
	compat_uint_t sync_type;
};

struct compat_ion_sys_get_phys_param {
	union
	{
		compat_int_t handle;
		compat_uptr_t kernel_handle;
	};
	compat_uint_t phy_addr;
    compat_size_t len;
};

struct compat_ion_sys_client_name {
    char name[ION_MM_DBG_NAME_LEN];
};

struct compat_ion_sys_get_client_param {
	compat_uint_t client;
};

struct compat_ion_sys_data
{
	compat_uint_t sys_cmd;
    union
    {
        struct compat_ion_sys_cache_sync_param cache_sync_param;
        struct compat_ion_sys_get_phys_param   get_phys_param;
        struct compat_ion_sys_get_client_param get_client_param;
        struct compat_ion_sys_client_name client_name_param;
    };
};

struct compat_ion_mm_config_buffer_param {
	union
	{
		compat_int_t handle;
		compat_uptr_t kernel_handle;
	};
    compat_uint_t eModuleID;
    compat_uint_t security;
    compat_uint_t coherent;
};

struct compat_ion_mm_buf_debug_info {
	union
	{
		compat_int_t handle;
		compat_uptr_t kernel_handle;
	};
    char dbg_name[ION_MM_DBG_NAME_LEN];
    compat_uint_t value1;
    compat_uint_t value2;
    compat_uint_t value3;
    compat_uint_t value4;
};

struct compat_ion_mm_sf_buf_info {
	union
	{
		compat_int_t handle;
		compat_uptr_t kernel_handle;
	};
    unsigned int info[ION_MM_SF_BUF_INFO_LEN];
};

struct compat_ion_mm_data {
	compat_uint_t mm_cmd;
    union
    {
    	struct compat_ion_mm_config_buffer_param config_buffer_param;
        struct compat_ion_mm_buf_debug_info  buf_debug_info_param;
        struct compat_ion_mm_sf_buf_info sf_buf_info_param;
    };
};

#define COMPAT_ION_IOC_ALLOC	_IOWR(ION_IOC_MAGIC, 0, \
				      struct compat_ion_allocation_data)
#define COMPAT_ION_IOC_FREE	_IOWR(ION_IOC_MAGIC, 1, \
				      struct compat_ion_handle_data)
#define COMPAT_ION_IOC_CUSTOM	_IOWR(ION_IOC_MAGIC, 6, \
				      struct compat_ion_custom_data)


static int compat_get_ion_allocation_data(
			struct compat_ion_allocation_data __user *data32,
			struct ion_allocation_data __user *data)
{
	compat_size_t s;
	compat_uint_t u;
	compat_int_t i;
	int err;

	err = get_user(s, &data32->len);
	err |= put_user(s, &data->len);
	err |= get_user(s, &data32->align);
	err |= put_user(s, &data->align);
	err |= get_user(u, &data32->heap_id_mask);
	err |= put_user(u, &data->heap_id_mask);
	err |= get_user(u, &data32->flags);
	err |= put_user(u, &data->flags);
	err |= get_user(i, &data32->handle);
	err |= put_user(i, &data->handle);

	return err;
}

static int compat_get_ion_handle_data(
			struct compat_ion_handle_data __user *data32,
			struct ion_handle_data __user *data)
{
	compat_int_t i;
	int err;

	err = get_user(i, &data32->handle);
	err |= put_user(i, &data->handle);

	return err;
}

static int compat_put_ion_allocation_data(
			struct compat_ion_allocation_data __user *data32,
			struct ion_allocation_data __user *data)
{
	compat_size_t s;
	compat_uint_t u;
	compat_int_t i;
	int err;

	err = get_user(s, &data->len);
	err |= put_user(s, &data32->len);
	err |= get_user(s, &data->align);
	err |= put_user(s, &data32->align);
	err |= get_user(u, &data->heap_id_mask);
	err |= put_user(u, &data32->heap_id_mask);
	err |= get_user(u, &data->flags);
	err |= put_user(u, &data32->flags);
	err |= get_user(i, &data->handle);
	err |= put_user(i, &data32->handle);

	return err;
}

static int compat_get_ion_mm_config_buffer_param(
			struct compat_ion_mm_config_buffer_param __user *data32,
			struct ion_mm_config_buffer_param __user *data)
{
	compat_ulong_t handle;
	compat_uint_t eModuleID;
	compat_uint_t security;
	compat_uint_t coherent;

	int err;

	err = get_user(handle, &data32->handle);
	err |= put_user(handle, &data->handle);
	err |= get_user(eModuleID, &data32->eModuleID);
	err |= put_user(eModuleID, &data->eModuleID);
	err |= get_user(security, &data32->security);
	err |= put_user(security, &data->security);
	err |= get_user(coherent, &data32->coherent);
	err |= put_user(coherent, &data->coherent);

	return err;
}

static int compat_get_ion_mm_buf_debug_info_set(
			struct compat_ion_mm_buf_debug_info __user *data32,
			struct __ion_mm_buf_debug_info __user *data)
{
	compat_ulong_t handle;
    char dbg_name;
    compat_uint_t value1;
    compat_uint_t value2;
    compat_uint_t value3;
    compat_uint_t value4;

	int i, err;

	err = get_user(handle, &data32->handle);
	err |= put_user(handle, &data->handle);
	for (i = 0; i < ION_MM_DBG_NAME_LEN; i++) {
		err = get_user(dbg_name, &data32->dbg_name[i]);
		err |= put_user(dbg_name, &data->dbg_name[i]);
	}
	err |= get_user(value1, &data32->value1);
	err |= put_user(value1, &data->value1);
	err |= get_user(value2, &data32->value2);
	err |= put_user(value2, &data->value2);
	err |= get_user(value3, &data32->value3);
	err |= put_user(value3, &data->value3);
	err |= get_user(value4, &data32->value4);
	err |= put_user(value4, &data->value4);

	return err;
}

static int compat_get_ion_mm_buf_debug_info(
			struct compat_ion_mm_buf_debug_info __user *data32,
			struct __ion_mm_buf_debug_info __user *data)
{
	compat_ulong_t handle;

	int err;

	err = get_user(handle, &data32->handle);
	err |= put_user(handle, &data->handle);

	return err;
}

static int compat_put_ion_mm_buf_debug_info(
			struct compat_ion_mm_buf_debug_info __user *data32,
			struct __ion_mm_buf_debug_info __user *data)
{
	compat_ulong_t handle;
    char dbg_name;
    compat_uint_t value1;
    compat_uint_t value2;
    compat_uint_t value3;
    compat_uint_t value4;

	int i, err = 0;

	err = get_user(handle, &data->handle);
	err |= put_user(handle, &data32->handle);
	for (i = 0; i < ION_MM_DBG_NAME_LEN; i++) {
		err = get_user(dbg_name, &data->dbg_name[i]);
		err |= put_user(dbg_name, &data32->dbg_name[i]);
	}
	err |= get_user(value1, &data->value1);
	err |= put_user(value1, &data32->value1);
	err |= get_user(value2, &data->value2);
	err |= put_user(value2, &data32->value2);
	err |= get_user(value3, &data->value3);
	err |= put_user(value3, &data32->value3);
	err |= get_user(value4, &data->value4);
	err |= put_user(value4, &data32->value4);

	return err;
}

static int compat_get_ion_mm_sf_buf_info_set(
			struct compat_ion_mm_sf_buf_info __user *data32,
			struct __ion_mm_sf_buf_info __user *data)
{
	compat_ulong_t handle;
	compat_uint_t info;

	int i, err;

	err = get_user(handle, &data32->handle);
	err |= put_user(handle, &data->handle);
	for(i = 0; i < ION_MM_SF_BUF_INFO_LEN; i++) {
		err |= get_user(info, &data32->info[i]);
		err |= put_user(info, &data->info[i]);
	}

	return err;
}

static int compat_get_ion_mm_sf_buf_info(
			struct compat_ion_mm_sf_buf_info __user *data32,
			struct __ion_mm_sf_buf_info __user *data)
{
	compat_ulong_t handle;
	compat_uint_t info;

	int i, err;

	err = get_user(handle, &data32->handle);
	err |= put_user(handle, &data->handle);

	return err;
}

static int compat_put_ion_mm_sf_buf_info(
			struct compat_ion_mm_sf_buf_info __user *data32,
			struct __ion_mm_sf_buf_info __user *data)
{
	compat_ulong_t handle;
	compat_uint_t info;

	int i, err;

	err |= get_user(handle, &data->handle);
	err |= put_user(handle, &data32->handle);

	for(i = 0; i < ION_MM_SF_BUF_INFO_LEN; i++) {
		err |= get_user(info, &data->info[i]);
		err |= put_user(info, &data32->info[i]);
	}

	return err;
}

static int compat_get_ion_mm_data(struct compat_ion_mm_data* data32, struct ion_mm_data* data)
{
	compat_uint_t mm_cmd;

	int err;

	err = get_user(mm_cmd, &data32->mm_cmd);
	err |= put_user(mm_cmd, &data->mm_cmd);

	switch(mm_cmd) {
	case ION_MM_CONFIG_BUFFER:
	{
		err |= compat_get_ion_mm_config_buffer_param(&data32->config_buffer_param, &data->config_buffer_param);
		break;
	}
	case ION_MM_SET_DEBUG_INFO:
	{
		err |= compat_get_ion_mm_buf_debug_info_set(&data32->buf_debug_info_param, &data->buf_debug_info_param);
		break;
	}
	case ION_MM_GET_DEBUG_INFO:
	{
		err |= compat_get_ion_mm_buf_debug_info(&data32->buf_debug_info_param, &data->buf_debug_info_param);
		break;
	}
	case ION_MM_SET_SF_BUF_INFO:
        {
		err |= compat_get_ion_mm_sf_buf_info_set(&data32->sf_buf_info_param, &data->sf_buf_info_param);
		break;
	}
	case ION_MM_GET_SF_BUF_INFO: 
        {
		err |= compat_get_ion_mm_sf_buf_info(&data32->sf_buf_info_param, &data->sf_buf_info_param);
		break;
	}
	}

	return err;
}

static int compat_put_ion_mm_data(struct compat_ion_mm_data* data32, struct ion_mm_data* data)
{
	compat_uint_t mm_cmd;

	int err = 0;

	err = get_user(mm_cmd, &data->mm_cmd);
	err |= put_user(mm_cmd, &data32->mm_cmd);

	switch(mm_cmd) {
	case ION_MM_GET_DEBUG_INFO:
	{
		err |= compat_put_ion_mm_buf_debug_info(&data32->buf_debug_info_param, &data->buf_debug_info_param);
		break;
	}
	case ION_MM_GET_SF_BUF_INFO: {
		err |= compat_put_ion_mm_sf_buf_info(&data32->sf_buf_info_param, &data->sf_buf_info_param);
		break;
	}
	default:
		err = 0;
	}

	return err;
}

static int compat_get_ion_sys_cache_sync_param(
			struct compat_ion_sys_cache_sync_param __user *data32,
			struct ion_sys_cache_sync_param __user *data)
{
	compat_int_t handle;
	compat_uptr_t va;
	compat_size_t size;
	compat_uint_t sync_type;

	int err;

	err = get_user(handle, &data32->handle);
	err |= put_user(handle, &data->handle);
	err |= get_user(va, &data32->va);
	err |= put_user(va, &data->va);
	err |= get_user(size, &data32->size);
	err |= put_user(size, &data->size);
	err |= get_user(sync_type, &data32->sync_type);
	err |= put_user(sync_type, &data->sync_type);

	return err;
}

static int compat_get_ion_sys_get_phys_param(
			struct compat_ion_sys_get_phys_param __user *data32,
			struct ion_sys_get_phys_param __user *data)
{
	compat_int_t handle;
	compat_uint_t phy_addr;
    compat_size_t len;

	int err;

	err = get_user(handle, &data32->handle);
	err |= put_user(handle, &data->handle);
	err |= get_user(phy_addr, &data32->phy_addr);
	err |= put_user(phy_addr, &data->phy_addr);
	err |= get_user(len, &data32->len);
	err |= put_user(len, &data->len);

	return err;
}

static int compat_put_ion_sys_get_phys_param(
			struct compat_ion_sys_get_phys_param __user *data32,
			struct ion_sys_get_phys_param __user *data)
{
	compat_int_t handle;
	compat_uint_t phy_addr;
    compat_size_t len;

	int err = 0;

	err = get_user(handle, &data->handle);
	err |= put_user(handle, &data32->handle);
	err |= get_user(phy_addr, &data->phy_addr);
	err |= put_user(phy_addr, &data32->phy_addr);
	err |= get_user(len, &data->len);
	err |= put_user(len, &data32->len);

	return err;
}

static int compat_get_ion_sys_client_name(
			struct compat_ion_sys_client_name __user *data32,
			struct __ion_sys_client_name __user *data)
{
	char name;

	int i, err;

	for (i = 0; i < ION_MM_DBG_NAME_LEN; i++) {
	    err = get_user(name, &data32->name[i]);
	    err |= put_user(name, &data->name[i]);
	}

	return err;
}

static int compat_get_ion_sys_get_client_param(
			struct compat_ion_sys_get_client_param __user *data32,
			struct ion_sys_get_client_param __user *data)
{
	compat_uint_t client;

	int err;

	err = get_user(client, &data32->client);
	err |= put_user(client, &data->client);

	return err;
}

static int compat_put_ion_sys_get_client_param(
			struct compat_ion_sys_get_client_param __user *data32,
			struct ion_sys_get_client_param __user *data)
{
	compat_uint_t client;

	int err = 0;

	err = get_user(client, &data->client);
	err |= put_user(client, &data32->client);

	return err;
}

static int compat_get_ion_sys_data(
			struct compat_ion_sys_data __user *data32,
			struct ion_sys_data __user *data)
{
	compat_uint_t sys_cmd;

	int err;

	err = get_user(sys_cmd, &data32->sys_cmd);
	err |= put_user(sys_cmd, &data->sys_cmd);

	switch(sys_cmd) {
	case ION_SYS_CACHE_SYNC:
	{
		err |= compat_get_ion_sys_cache_sync_param(&data32->cache_sync_param, &data->cache_sync_param);
		break;
	}
	case ION_SYS_GET_PHYS:
	{
		err |= compat_get_ion_sys_get_phys_param(&data32->get_phys_param, &data->get_phys_param);
		break;
	}
	case ION_SYS_GET_CLIENT:
	{
		err |= compat_get_ion_sys_get_client_param(&data32->get_client_param, &data->get_client_param);
		break;
	}
	case ION_SYS_SET_CLIENT_NAME:
	{
		err |= compat_get_ion_sys_client_name(&data32->client_name_param, &data->client_name_param);
		break;
	}
	}

	return err;
}

static int compat_put_ion_sys_data(
			struct compat_ion_sys_data __user *data32,
			struct ion_sys_data __user *data)
{
	compat_uint_t sys_cmd;

	int err = 0;

	err = get_user(sys_cmd, &data->sys_cmd);
	err |= put_user(sys_cmd, &data32->sys_cmd);

	switch(sys_cmd) {
	case ION_SYS_GET_PHYS:
	{
		err |= compat_put_ion_sys_get_phys_param(&data32->get_phys_param, &data->get_phys_param);
		break;
	}
	case ION_SYS_GET_CLIENT:
	{
		err |= compat_put_ion_sys_get_client_param(&data32->get_client_param, &data->get_client_param);
		break;
	}
	default:
		err = 0;
	}

	return err;
}

static int compat_get_ion_custom_data(
			struct compat_ion_custom_data __user *data32,
			struct ion_custom_data __user *data)
{
	compat_uint_t cmd;
	compat_ulong_t arg;
	int err;

	err = get_user(cmd, &data32->cmd);
	err |= put_user(cmd, &data->cmd);

	//err = get_user(arg, &data32->arg);
	//err |= put_user(arg, &data->arg);

	switch (cmd) {
	case ION_CMD_SYSTEM:
	{
		struct compat_ion_sys_data* sys_data32;
		struct ion_sys_data* sys_data;

		sys_data32 = (struct compat_ion_sys_data*)compat_ptr(data32->arg);
		sys_data = compat_alloc_user_space(sizeof(*data) + sizeof(*sys_data));
		if (sys_data == NULL)
		    return -EFAULT;

		err = compat_get_ion_sys_data(sys_data32, sys_data);
		err |= put_user((unsigned long)sys_data, &data->arg);
		break;
	}
	case ION_CMD_MULTIMEDIA:
	{
		struct compat_ion_mm_data* mm_data32;
		struct ion_mm_data* mm_data;

		mm_data32 = (struct compat_ion_mm_data*)compat_ptr(data32->arg);
		mm_data = compat_alloc_user_space(sizeof(*data) + sizeof(*mm_data));
		if (mm_data == NULL)
		    return -EFAULT;
		err = compat_get_ion_mm_data(mm_data32, mm_data);
		err |= put_user((unsigned long)mm_data, &data->arg);
		break;
	}
	}

	return err;
}

static int compat_put_ion_custom_data(
			struct compat_ion_custom_data __user *data32,
		    struct ion_custom_data __user *data)
{
	compat_uint_t cmd;
	compat_ulong_t arg;
	int err = 0;

	err = get_user(cmd, &data->cmd);
	err |= put_user(cmd, &data32->cmd);

	//err = get_user(arg, &data->arg);
	//err |= put_user(arg, &data32->arg);

	switch (cmd) {
	case ION_CMD_SYSTEM:
	{
		struct compat_ion_sys_data* sys_data32;
		struct ion_sys_data* sys_data;

		sys_data32 = (struct compat_ion_sys_data*)compat_ptr(data32->arg);
		sys_data = (struct ion_sys_data*)compat_ptr(data->arg);

		err = compat_put_ion_sys_data(sys_data32, sys_data);
		//err |= put_user((unsigned long)sys_data32, &data32->arg);
		break;
	}
	case ION_CMD_MULTIMEDIA:
	{
		struct compat_ion_mm_data* mm_data32;
		struct ion_mm_data* mm_data;

		mm_data32 = (struct compat_ion_mm_data*)compat_ptr(data32->arg);
		mm_data = (struct ion_mm_data*)compat_ptr(data->arg);

		err = compat_put_ion_mm_data(mm_data32, mm_data);
		//err |= put_user((unsigned long)mm_data32, &data32->arg);
		break;
	}
	default:
		err = 0;
	}

	return err;
}

long compat_ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
                IONMSG("compat_ion_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
  		return -ENOTTY;
        }

	switch (cmd) {
	case COMPAT_ION_IOC_ALLOC:
	{
		struct compat_ion_allocation_data __user *data32;
		struct ion_allocation_data __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL) {
                        IONMSG("COMPAT_ION_IOC_ALLOC alloc user space fail!.\n");
			return -EFAULT;
                }

		err = compat_get_ion_allocation_data(data32, data);
		if (err) {
                        IONMSG("COMPAT_ION_IOC_ALLOC get ion allocation data fail!. err = %d.\n", err);
			return err;
                }
		ret = filp->f_op->unlocked_ioctl(filp, ION_IOC_ALLOC,
							(unsigned long)data);
                if (ret) {
                    IONMSG("COMPAT_ION_IOC_ALLOC unlocked_ioctl fail! ret = %ld.\n", ret); 
                }

		err = compat_put_ion_allocation_data(data32, data);
                if (err) {
                    IONMSG("COMPAT_ION_IOC_ALLOC put ion allocation data fail! err = %d.\n", err);
                }

		return ret ? ret : err;
	}
	case COMPAT_ION_IOC_FREE:
	{
		struct compat_ion_handle_data __user *data32;
		struct ion_handle_data __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL) {
                        IONMSG("COMPAT_ION_IOC_FREE alloc user space fail!.\n");
			return -EFAULT;
                }

		err = compat_get_ion_handle_data(data32, data);
		if (err) {
                        IONMSG("COMPAT_ION_IOC_FREE get ion handle data fail!. err = %d.\n", err);
			return err;
                }

		ret =  filp->f_op->unlocked_ioctl(filp, ION_IOC_FREE,
							(unsigned long)data);
                if (ret) {
                       IONMSG("COMPAT_ION_IOC_FREE unlocked_ioctl fail! ret = %ld.\n", ret); 
                }

                return ret;
	}
	case COMPAT_ION_IOC_CUSTOM: {
		struct compat_ion_custom_data __user *data32;
		struct ion_custom_data __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL) {
                        IONMSG("COMPAT_ION_IOC_CUSTOM alloc user space fail!.\n");
			return -EFAULT;
                }

		err = compat_get_ion_custom_data(data32, data);
		if (err) {
                        IONMSG("COMPAT_ION_IOC_CUSTOM get ion custom data fail!. err = %d.\n", err);
			return err;
                }

		ret = filp->f_op->unlocked_ioctl(filp, ION_IOC_CUSTOM,
							(unsigned long)data);
                if (ret) {
                        IONMSG("COMPAT_ION_IOC_CUSTOM unlocked_ioctl fail! ret = %ld.\n", ret);
                }
                
                err = compat_put_ion_custom_data(data32, data);
                if (err) {
                    IONMSG("COMPAT_ION_IOC_CUSTOM put ion custom data fail! err = %d.\n", err);
                }
		return ret ? ret : err;
	}
	case ION_IOC_SHARE:
	case ION_IOC_MAP:
	case ION_IOC_IMPORT:
	case ION_IOC_SYNC:
		return filp->f_op->unlocked_ioctl(filp, cmd,
						(unsigned long)compat_ptr(arg));
	default: {
                IONMSG("compat_ion_ioctl : No such command!! 0x%x\n", cmd);
		return -ENOIOCTLCMD;
        }
	}
}
