/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt) "COMPAT-QSEECOM: %s: " fmt, __func__

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/qseecom.h>
#include <linux/compat.h>
#include <linux/compat_qseecom.h>

static int compat_get_qseecom_register_listener_req(
		struct compat_qseecom_register_listener_req __user *data32,
		struct qseecom_register_listener_req __user *data)
{
	int err;
	compat_ulong_t listener_id;
	compat_long_t ifd_data_fd;
	compat_uptr_t virt_sb_base;
	compat_ulong_t sb_size;

	err = get_user(listener_id, &data32->listener_id);
	err |= put_user(listener_id, &data->listener_id);
	err |= get_user(ifd_data_fd, &data32->ifd_data_fd);
	err |= put_user(ifd_data_fd, &data->ifd_data_fd);

	err |= get_user(virt_sb_base, &data32->virt_sb_base);
	/* upper bits won't get set, zero them */
	err |= put_user(NULL, &data->virt_sb_base);
	err |= put_user(virt_sb_base, (compat_uptr_t *)&data->virt_sb_base);

	err |= get_user(sb_size, &data32->sb_size);
	err |= put_user(sb_size, &data->sb_size);
	return err;
}

static int compat_get_qseecom_load_img_req(
		struct compat_qseecom_load_img_req __user *data32,
		struct qseecom_load_img_req __user *data)
{
	int err;
	compat_ulong_t mdt_len;
	compat_ulong_t img_len;
	compat_long_t ifd_data_fd;
	compat_ulong_t app_arch;
	compat_int_t app_id;

	err = get_user(mdt_len, &data32->mdt_len);
	err |= put_user(mdt_len, &data->mdt_len);
	err |= get_user(img_len, &data32->img_len);
	err |= put_user(img_len, &data->img_len);
	err |= get_user(ifd_data_fd, &data32->ifd_data_fd);
	err |= put_user(ifd_data_fd, &data->ifd_data_fd);
	err |= copy_in_user(data->img_name, data32->img_name,
				MAX_APP_NAME_SIZE);
	err |= get_user(app_arch, &data32->app_arch);
	err |= put_user(app_arch, &data->app_arch);
	err |= get_user(app_id, &data32->app_id);
	err |= put_user(app_id, &data->app_id);
	return err;
}

static int compat_get_qseecom_send_cmd_req(
		struct compat_qseecom_send_cmd_req __user *data32,
		struct qseecom_send_cmd_req __user *data)
{
	int err;
	compat_uptr_t cmd_req_buf;
	compat_uint_t cmd_req_len;
	compat_uptr_t resp_buf;
	compat_uint_t resp_len;

	err = get_user(cmd_req_buf, &data32->cmd_req_buf);
	err |= put_user(NULL, &data->cmd_req_buf);
	err |= put_user(cmd_req_buf, (compat_uptr_t *)&data->cmd_req_buf);
	err |= get_user(cmd_req_len, &data32->cmd_req_len);
	err |= put_user(cmd_req_len, &data->cmd_req_len);

	err |= get_user(resp_buf, &data32->resp_buf);
	err |= put_user(NULL, &data->resp_buf);
	err |= put_user(resp_buf, (compat_uptr_t *)&data->resp_buf);
	err |= get_user(resp_len, &data32->resp_len);
	err |= put_user(resp_len, &data->resp_len);
	return err;
}

static int compat_get_qseecom_send_modfd_cmd_req(
		struct compat_qseecom_send_modfd_cmd_req __user *data32,
		struct qseecom_send_modfd_cmd_req __user *data)
{
	int err;
	unsigned int i;
	compat_uptr_t cmd_req_buf;
	compat_uint_t cmd_req_len;
	compat_uptr_t resp_buf;
	compat_uint_t resp_len;
	compat_long_t fd;
	compat_ulong_t cmd_buf_offset;

	err = get_user(cmd_req_buf, &data32->cmd_req_buf);
	err |= put_user(NULL, &data->cmd_req_buf);
	err |= put_user(cmd_req_buf, (compat_uptr_t *)&data->cmd_req_buf);
	err |= get_user(cmd_req_len, &data32->cmd_req_len);
	err |= put_user(cmd_req_len, &data->cmd_req_len);
	err |= get_user(resp_buf, &data32->resp_buf);
	err |= put_user(NULL, &data->resp_buf);
	err |= put_user(resp_buf, (compat_uptr_t *)&data->resp_buf);
	err |= get_user(resp_len, &data32->resp_len);
	err |= put_user(resp_len, &data->resp_len);
	for (i = 0; i < MAX_ION_FD; i++) {
		err |= get_user(fd, &data32->ifd_data[i].fd);
		err |= put_user(fd, &data->ifd_data[i].fd);
		err |= get_user(cmd_buf_offset,
				&data32->ifd_data[i].cmd_buf_offset);
		err |= put_user(cmd_buf_offset,
				&data->ifd_data[i].cmd_buf_offset);
	}
	return err;
}

static int compat_get_qseecom_set_sb_mem_param_req(
		struct compat_qseecom_set_sb_mem_param_req __user *data32,
		struct qseecom_set_sb_mem_param_req __user *data)
{
	int err;
	compat_long_t ifd_data_fd;
	compat_uptr_t virt_sb_base;
	compat_ulong_t sb_len;

	err = get_user(ifd_data_fd, &data32->ifd_data_fd);
	err |= put_user(ifd_data_fd, &data->ifd_data_fd);
	err |= get_user(virt_sb_base, &data32->virt_sb_base);
	err |= put_user(NULL, &data->virt_sb_base);
	err |= put_user(virt_sb_base, (compat_uptr_t *)&data->virt_sb_base);
	err |= get_user(sb_len, &data32->sb_len);
	err |= put_user(sb_len, &data->sb_len);
	return err;
}

static int compat_get_qseecom_qseos_version_req(
		struct compat_qseecom_qseos_version_req __user *data32,
		struct qseecom_qseos_version_req __user *data)
{
	int err;
	compat_uint_t qseos_version;

	err = get_user(qseos_version, &data32->qseos_version);
	err |= put_user(qseos_version, &data->qseos_version);
	return err;
}

static int compat_get_qseecom_qseos_app_load_query(
		struct compat_qseecom_qseos_app_load_query __user *data32,
		struct qseecom_qseos_app_load_query __user *data)
{
	int err = 0;
	unsigned int i;
	compat_int_t app_id;
	char app_name;
	compat_ulong_t app_arch;

	for (i = 0; i < MAX_APP_NAME_SIZE; i++) {
		err |= get_user(app_name, &(data32->app_name[i]));
		err |= put_user(app_name, &(data->app_name[i]));
	}
	err |= get_user(app_id, &data32->app_id);
	err |= put_user(app_id, &data->app_id);
	err |= get_user(app_arch, &data32->app_arch);
	err |= put_user(app_arch, &data->app_arch);
	return err;
}

static int compat_get_qseecom_send_svc_cmd_req(
		struct compat_qseecom_send_svc_cmd_req __user *data32,
		struct qseecom_send_svc_cmd_req __user *data)
{
	int err;
	compat_ulong_t cmd_id;
	compat_uptr_t cmd_req_buf;
	compat_uint_t cmd_req_len;
	compat_uptr_t resp_buf;
	compat_uint_t resp_len;

	err = get_user(cmd_id, &data32->cmd_id);
	err |= put_user(cmd_id, &data->cmd_id);
	err |= get_user(cmd_req_buf, &data32->cmd_req_buf);
	err |= put_user(NULL, &data->cmd_req_buf);
	err |= put_user(cmd_req_buf, (compat_uptr_t *)&data->cmd_req_buf);
	err |= get_user(cmd_req_len, &data32->cmd_req_len);
	err |= put_user(cmd_req_len, &data->cmd_req_len);
	err |= get_user(resp_buf, &data32->resp_buf);
	err |= put_user(NULL, &data->resp_buf);
	err |= put_user(resp_buf, (compat_uptr_t *)&data->resp_buf);
	err |= get_user(resp_len, &data32->resp_len);
	err |= put_user(resp_len, &data->resp_len);
	return err;
}

static int compat_get_qseecom_create_key_req(
		struct compat_qseecom_create_key_req __user *data32,
		struct qseecom_create_key_req __user *data)
{
	int err;
	compat_uint_t usage;

	err = copy_in_user(data->hash32, data32->hash32, QSEECOM_HASH_SIZE);
	err |= get_user(usage, &data32->usage);
	err |= put_user(usage, &data->usage);

	return err;
}

static int compat_get_qseecom_wipe_key_req(
		struct compat_qseecom_wipe_key_req __user *data32,
		struct qseecom_wipe_key_req __user *data)
{
	int err;
	compat_uint_t usage;
	compat_int_t wipe_key_flag;

	err = get_user(usage, &data32->usage);
	err |= put_user(usage, &data->usage);
	err |= get_user(wipe_key_flag, &data32->wipe_key_flag);
	err |= put_user(wipe_key_flag, &data->wipe_key_flag);

	return err;
}

static int compat_get_qseecom_update_key_userinfo_req(
		struct compat_qseecom_update_key_userinfo_req __user *data32,
		struct qseecom_update_key_userinfo_req __user *data)
{
	int err = 0;
	compat_uint_t usage;

	err = copy_in_user(data->current_hash32, data32->current_hash32,
				QSEECOM_HASH_SIZE);
	err |= copy_in_user(data->new_hash32, data32->new_hash32,
				QSEECOM_HASH_SIZE);
	err |= get_user(usage, &data32->usage);
	err |= put_user(usage, &data->usage);

	return err;
}

static int compat_get_qseecom_save_partition_hash_req(
		struct compat_qseecom_save_partition_hash_req __user *data32,
		struct qseecom_save_partition_hash_req __user *data)
{
	int err;
	compat_int_t partition_id;

	err = get_user(partition_id, &data32->partition_id);
	err |= put_user(partition_id, &data->partition_id);
	err |= copy_in_user(data->digest, data32->digest,
				SHA256_DIGEST_LENGTH);
	return err;
}

static int compat_get_qseecom_is_es_activated_req(
		struct compat_qseecom_is_es_activated_req __user *data32,
		struct qseecom_is_es_activated_req __user *data)
{
	compat_int_t is_activated;
	int err;

	err = get_user(is_activated, &data32->is_activated);
	err |= put_user(is_activated, &data->is_activated);
	return err;
}

static int compat_get_qseecom_mdtp_cipher_dip_req(
		struct compat_qseecom_mdtp_cipher_dip_req __user *data32,
		struct qseecom_mdtp_cipher_dip_req __user *data)
{
	int err;
	compat_int_t in_buf_size;
	compat_uptr_t in_buf;
	compat_int_t out_buf_size;
	compat_uptr_t out_buf;
	compat_int_t direction;

	err = get_user(in_buf_size, &data32->in_buf_size);
	err |= put_user(in_buf_size, &data->in_buf_size);
	err |= get_user(out_buf_size, &data32->out_buf_size);
	err |= put_user(out_buf_size, &data->out_buf_size);
	err |= get_user(direction, &data32->direction);
	err |= put_user(direction, &data->direction);
	err |= get_user(in_buf, &data32->in_buf);
	err |= put_user(NULL, &data->in_buf);
	err |= put_user(in_buf, (compat_uptr_t *)&data->in_buf);
	err |= get_user(out_buf, &data32->out_buf);
	err |= put_user(NULL, &data->out_buf);
	err |= put_user(out_buf, (compat_uptr_t *)&data->out_buf);

	return err;
}

static int compat_get_qseecom_send_modfd_listener_resp(
		struct compat_qseecom_send_modfd_listener_resp __user *data32,
		struct qseecom_send_modfd_listener_resp __user *data)
{
	int err;
	unsigned int i;
	compat_uptr_t resp_buf_ptr;
	compat_uint_t resp_len;
	compat_long_t fd;
	compat_ulong_t cmd_buf_offset;

	err = get_user(resp_buf_ptr, &data32->resp_buf_ptr);
	err |= put_user(NULL, &data->resp_buf_ptr);
	err |= put_user(resp_buf_ptr, (compat_uptr_t *)&data->resp_buf_ptr);
	err |= get_user(resp_len, &data32->resp_len);
	err |= put_user(resp_len, &data->resp_len);

	for (i = 0; i < MAX_ION_FD; i++) {
		err |= get_user(fd, &data32->ifd_data[i].fd);
		err |= put_user(fd, &data->ifd_data[i].fd);
		err |= get_user(cmd_buf_offset,
				&data32->ifd_data[i].cmd_buf_offset);
		err |= put_user(cmd_buf_offset,
				&data->ifd_data[i].cmd_buf_offset);
	}
	return err;
}


static int compat_get_qseecom_qteec_req(
		struct compat_qseecom_qteec_req __user *data32,
		struct qseecom_qteec_req __user *data)
{
	compat_uptr_t req_ptr;
	compat_ulong_t req_len;
	compat_uptr_t resp_ptr;
	compat_ulong_t resp_len;
	int err;

	err = get_user(req_ptr, &data32->req_ptr);
	err |= put_user(NULL, &data->req_ptr);
	err |= put_user(req_ptr, (compat_uptr_t *)&data->req_ptr);
	err |= get_user(req_len, &data32->req_len);
	err |= put_user(req_len, &data->req_len);

	err |= get_user(resp_ptr, &data32->resp_ptr);
	err |= put_user(NULL, &data->resp_ptr);
	err |= put_user(resp_ptr, (compat_uptr_t *)&data->resp_ptr);
	err |= get_user(resp_len, &data32->resp_len);
	err |= put_user(resp_len, &data->resp_len);
	return err;
}

static int compat_get_qseecom_qteec_modfd_req(
		struct compat_qseecom_qteec_modfd_req __user *data32,
		struct qseecom_qteec_modfd_req __user *data)
{
	compat_uptr_t req_ptr;
	compat_ulong_t req_len;
	compat_uptr_t resp_ptr;
	compat_ulong_t resp_len;
	compat_long_t fd;
	compat_ulong_t cmd_buf_offset;
	int err, i;

	err = get_user(req_ptr, &data32->req_ptr);
	err |= put_user(NULL, &data->req_ptr);
	err |= put_user(req_ptr, (compat_uptr_t *)&data->req_ptr);
	err |= get_user(req_len, &data32->req_len);
	err |= put_user(req_len, &data->req_len);

	err |= get_user(resp_ptr, &data32->resp_ptr);
	err |= put_user(NULL, &data->resp_ptr);
	err |= put_user(resp_ptr, (compat_uptr_t *)&data->resp_ptr);
	err |= get_user(resp_len, &data32->resp_len);
	err |= put_user(resp_len, &data->resp_len);

	for (i = 0; i < MAX_ION_FD; i++) {
		err |= get_user(fd, &data32->ifd_data[i].fd);
		err |= put_user(fd, &data->ifd_data[i].fd);
		err |= get_user(cmd_buf_offset,
				&data32->ifd_data[i].cmd_buf_offset);
		err |= put_user(cmd_buf_offset,
				&data->ifd_data[i].cmd_buf_offset);
	}
	return err;
}

static int compat_get_int(compat_int_t __user *data32,
		int __user *data)
{
	compat_int_t x;
	int err;

	err = get_user(x, data32);
	err |= put_user(x, data);
	return err;
}

static int compat_put_qseecom_load_img_req(
		struct compat_qseecom_load_img_req __user *data32,
		struct qseecom_load_img_req __user *data)
{
	int err;
	compat_ulong_t mdt_len;
	compat_ulong_t img_len;
	compat_long_t ifd_data_fd;
	compat_ulong_t app_arch;
	compat_int_t app_id;

	err = get_user(mdt_len, &data->mdt_len);
	err |= put_user(mdt_len, &data32->mdt_len);
	err |= get_user(img_len, &data->img_len);
	err |= put_user(img_len, &data32->img_len);
	err |= get_user(ifd_data_fd, &data->ifd_data_fd);
	err |= put_user(ifd_data_fd, &data32->ifd_data_fd);
	err |= copy_in_user(data32->img_name, data->img_name,
				MAX_APP_NAME_SIZE);
	err |= get_user(app_arch, &data->app_arch);
	err |= put_user(app_arch, &data32->app_arch);
	err |= get_user(app_id, &data->app_id);
	err |= put_user(app_id, &data32->app_id);
	return err;
}

static int compat_put_qseecom_qseos_version_req(
		struct compat_qseecom_qseos_version_req __user *data32,
		struct qseecom_qseos_version_req __user *data)
{
	compat_uint_t qseos_version;
	int err;

	err = get_user(qseos_version, &data->qseos_version);
	err |= put_user(qseos_version, &data32->qseos_version);
	return err;
}

static int compat_put_qseecom_qseos_app_load_query(
		struct compat_qseecom_qseos_app_load_query __user *data32,
		struct qseecom_qseos_app_load_query __user *data)
{
	int err = 0;
	unsigned int i;
	compat_int_t app_id;
	compat_ulong_t app_arch;
	char app_name;

	for (i = 0; i < MAX_APP_NAME_SIZE; i++) {
		err |= get_user(app_name, &(data->app_name[i]));
		err |= put_user(app_name, &(data32->app_name[i]));
	}
	err |= get_user(app_id, &data->app_id);
	err |= put_user(app_id, &data32->app_id);
	err |= get_user(app_arch, &data->app_arch);
	err |= put_user(app_arch, &data32->app_arch);

	return err;
}

static int compat_put_qseecom_is_es_activated_req(
		struct compat_qseecom_is_es_activated_req __user *data32,
		struct qseecom_is_es_activated_req __user *data)
{
	compat_int_t is_activated;
	int err;

	err = get_user(is_activated, &data->is_activated);
	err |= put_user(is_activated, &data32->is_activated);
	return err;
}

static unsigned int convert_cmd(unsigned int cmd)
{
	switch (cmd) {
	case COMPAT_QSEECOM_IOCTL_REGISTER_LISTENER_REQ:
		return QSEECOM_IOCTL_REGISTER_LISTENER_REQ;
	case COMPAT_QSEECOM_IOCTL_UNREGISTER_LISTENER_REQ:
		return QSEECOM_IOCTL_UNREGISTER_LISTENER_REQ;
	case COMPAT_QSEECOM_IOCTL_LOAD_APP_REQ:
		return QSEECOM_IOCTL_LOAD_APP_REQ;
	case COMPAT_QSEECOM_IOCTL_RECEIVE_REQ:
		return QSEECOM_IOCTL_RECEIVE_REQ;
	case COMPAT_QSEECOM_IOCTL_SEND_RESP_REQ:
		return QSEECOM_IOCTL_SEND_RESP_REQ;
	case COMPAT_QSEECOM_IOCTL_UNLOAD_APP_REQ:
		return QSEECOM_IOCTL_UNLOAD_APP_REQ;
	case COMPAT_QSEECOM_IOCTL_PERF_ENABLE_REQ:
		return QSEECOM_IOCTL_PERF_ENABLE_REQ;
	case COMPAT_QSEECOM_IOCTL_PERF_DISABLE_REQ:
		return QSEECOM_IOCTL_PERF_DISABLE_REQ;
	case COMPAT_QSEECOM_IOCTL_UNLOAD_EXTERNAL_ELF_REQ:
		return QSEECOM_IOCTL_UNLOAD_EXTERNAL_ELF_REQ;
	case COMPAT_QSEECOM_IOCTL_SET_BUS_SCALING_REQ:
		return QSEECOM_IOCTL_SET_BUS_SCALING_REQ;
	case COMPAT_QSEECOM_IOCTL_SEND_CMD_REQ:
		return QSEECOM_IOCTL_SEND_CMD_REQ;
	case COMPAT_QSEECOM_IOCTL_SEND_MODFD_CMD_REQ:
		return QSEECOM_IOCTL_SEND_MODFD_CMD_REQ;
	case COMPAT_QSEECOM_IOCTL_SET_MEM_PARAM_REQ:
		return QSEECOM_IOCTL_SET_MEM_PARAM_REQ;
	case COMPAT_QSEECOM_IOCTL_GET_QSEOS_VERSION_REQ:
		return QSEECOM_IOCTL_GET_QSEOS_VERSION_REQ;
	case COMPAT_QSEECOM_IOCTL_LOAD_EXTERNAL_ELF_REQ:
		return QSEECOM_IOCTL_LOAD_EXTERNAL_ELF_REQ;
	case COMPAT_QSEECOM_IOCTL_APP_LOADED_QUERY_REQ:
		return QSEECOM_IOCTL_APP_LOADED_QUERY_REQ;
	case COMPAT_QSEECOM_IOCTL_SEND_CMD_SERVICE_REQ:
		return QSEECOM_IOCTL_SEND_CMD_SERVICE_REQ;
	case COMPAT_QSEECOM_IOCTL_CREATE_KEY_REQ:
		return QSEECOM_IOCTL_CREATE_KEY_REQ;
	case COMPAT_QSEECOM_IOCTL_WIPE_KEY_REQ:
		return QSEECOM_IOCTL_WIPE_KEY_REQ;
	case COMPAT_QSEECOM_IOCTL_UPDATE_KEY_USER_INFO_REQ:
		return QSEECOM_IOCTL_UPDATE_KEY_USER_INFO_REQ;
	case COMPAT_QSEECOM_IOCTL_SAVE_PARTITION_HASH_REQ:
		return QSEECOM_IOCTL_SAVE_PARTITION_HASH_REQ;
	case COMPAT_QSEECOM_IOCTL_IS_ES_ACTIVATED_REQ:
		return QSEECOM_IOCTL_IS_ES_ACTIVATED_REQ;
	case COMPAT_QSEECOM_IOCTL_SEND_MODFD_RESP:
		return QSEECOM_IOCTL_SEND_MODFD_RESP;
	case COMPAT_QSEECOM_QTEEC_IOCTL_OPEN_SESSION_REQ:
		return QSEECOM_QTEEC_IOCTL_OPEN_SESSION_REQ;
	case COMPAT_QSEECOM_QTEEC_IOCTL_CLOSE_SESSION_REQ:
		return QSEECOM_QTEEC_IOCTL_CLOSE_SESSION_REQ;
	case COMPAT_QSEECOM_QTEEC_IOCTL_INVOKE_MODFD_CMD_REQ:
		return QSEECOM_QTEEC_IOCTL_INVOKE_MODFD_CMD_REQ;
	case COMPAT_QSEECOM_QTEEC_IOCTL_REQUEST_CANCELLATION_REQ:
		return QSEECOM_QTEEC_IOCTL_REQUEST_CANCELLATION_REQ;
	case COMPAT_QSEECOM_IOCTL_MDTP_CIPHER_DIP_REQ:
		return QSEECOM_IOCTL_MDTP_CIPHER_DIP_REQ;
	case COMPAT_QSEECOM_IOCTL_SEND_MODFD_CMD_64_REQ:
		return QSEECOM_IOCTL_SEND_MODFD_CMD_64_REQ;
	case COMPAT_QSEECOM_IOCTL_SEND_MODFD_RESP_64:
		return QSEECOM_IOCTL_SEND_MODFD_RESP_64;

	default:
		return cmd;
	}
}

long compat_qseecom_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	long ret;
	switch (cmd) {

	case COMPAT_QSEECOM_IOCTL_UNREGISTER_LISTENER_REQ:
	case COMPAT_QSEECOM_IOCTL_RECEIVE_REQ:
	case COMPAT_QSEECOM_IOCTL_SEND_RESP_REQ:
	case COMPAT_QSEECOM_IOCTL_UNLOAD_APP_REQ:
	case COMPAT_QSEECOM_IOCTL_PERF_ENABLE_REQ:
	case COMPAT_QSEECOM_IOCTL_PERF_DISABLE_REQ:
	case COMPAT_QSEECOM_IOCTL_UNLOAD_EXTERNAL_ELF_REQ: {
		return qseecom_ioctl(file, convert_cmd(cmd), 0);
	}
	break;
	case COMPAT_QSEECOM_IOCTL_REGISTER_LISTENER_REQ: {
		struct compat_qseecom_register_listener_req __user *data32;
		struct qseecom_register_listener_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_register_listener_req(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_IOCTL_LOAD_APP_REQ: {
		struct compat_qseecom_load_img_req __user *data32;
		struct qseecom_load_img_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_load_img_req(data32, data);
		if (err)
			return err;

		ret = qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
		err = compat_put_qseecom_load_img_req(data32, data);
		return ret ? ret : err;
	}
	break;
	case COMPAT_QSEECOM_IOCTL_SEND_CMD_REQ: {
		struct compat_qseecom_send_cmd_req __user *data32;
		struct qseecom_send_cmd_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_send_cmd_req(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_IOCTL_SEND_MODFD_CMD_REQ:
	case COMPAT_QSEECOM_IOCTL_SEND_MODFD_CMD_64_REQ: {
		struct compat_qseecom_send_modfd_cmd_req __user *data32;
		struct qseecom_send_modfd_cmd_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_send_modfd_cmd_req(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_IOCTL_SET_MEM_PARAM_REQ: {
		struct compat_qseecom_set_sb_mem_param_req __user *data32;
		struct qseecom_set_sb_mem_param_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_set_sb_mem_param_req(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_IOCTL_GET_QSEOS_VERSION_REQ: {
		struct compat_qseecom_qseos_version_req __user *data32;
		struct qseecom_qseos_version_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_qseos_version_req(data32, data);
		if (err)
			return err;

		ret = qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
		err = compat_put_qseecom_qseos_version_req(data32, data);

		return ret ? ret : err;
	}
	break;
	case COMPAT_QSEECOM_IOCTL_SET_BUS_SCALING_REQ: {
		compat_int_t __user *data32;
		int __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;
		err = compat_get_int(data32, data);
		if (err)
			return err;
		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_IOCTL_LOAD_EXTERNAL_ELF_REQ: {
		struct compat_qseecom_load_img_req __user *data32;
		struct qseecom_load_img_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_load_img_req(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_IOCTL_APP_LOADED_QUERY_REQ: {
		struct compat_qseecom_qseos_app_load_query __user *data32;
		struct qseecom_qseos_app_load_query __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_qseos_app_load_query(data32, data);
		if (err)
			return err;

		ret = qseecom_ioctl(file, convert_cmd(cmd),
					(unsigned long)data);
		err = compat_put_qseecom_qseos_app_load_query(data32, data);
		return ret ? ret : err;
	}
	break;
	case COMPAT_QSEECOM_IOCTL_SEND_CMD_SERVICE_REQ: {
		struct compat_qseecom_send_svc_cmd_req __user *data32;
		struct qseecom_send_svc_cmd_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_send_svc_cmd_req(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_IOCTL_CREATE_KEY_REQ: {
		struct compat_qseecom_create_key_req __user *data32;
		struct qseecom_create_key_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_create_key_req(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_IOCTL_WIPE_KEY_REQ: {
		struct compat_qseecom_wipe_key_req __user *data32;
		struct qseecom_wipe_key_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_wipe_key_req(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_IOCTL_UPDATE_KEY_USER_INFO_REQ: {
		struct compat_qseecom_update_key_userinfo_req __user *data32;
		struct qseecom_update_key_userinfo_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_update_key_userinfo_req(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_IOCTL_SAVE_PARTITION_HASH_REQ: {
		struct compat_qseecom_save_partition_hash_req __user *data32;
		struct qseecom_save_partition_hash_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_save_partition_hash_req(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_IOCTL_IS_ES_ACTIVATED_REQ: {
		struct compat_qseecom_is_es_activated_req __user *data32;
		struct qseecom_is_es_activated_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_is_es_activated_req(data32, data);
		if (err)
			return err;

		ret = qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
		err = compat_put_qseecom_is_es_activated_req(data32, data);
		return ret ? ret : err;
	}
	break;
	case COMPAT_QSEECOM_IOCTL_MDTP_CIPHER_DIP_REQ: {
		struct compat_qseecom_mdtp_cipher_dip_req __user *data32;
		struct qseecom_mdtp_cipher_dip_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_mdtp_cipher_dip_req(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_IOCTL_SEND_MODFD_RESP:
	case COMPAT_QSEECOM_IOCTL_SEND_MODFD_RESP_64: {
		struct compat_qseecom_send_modfd_listener_resp __user *data32;
		struct qseecom_send_modfd_listener_resp __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_send_modfd_listener_resp(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_QTEEC_IOCTL_CLOSE_SESSION_REQ: {
		struct compat_qseecom_qteec_req __user *data32;
		struct qseecom_qteec_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_qteec_req(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	case COMPAT_QSEECOM_QTEEC_IOCTL_OPEN_SESSION_REQ:
	case COMPAT_QSEECOM_QTEEC_IOCTL_INVOKE_MODFD_CMD_REQ:
	case COMPAT_QSEECOM_QTEEC_IOCTL_REQUEST_CANCELLATION_REQ: {
		struct compat_qseecom_qteec_modfd_req __user *data32;
		struct qseecom_qteec_modfd_req __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_qseecom_qteec_modfd_req(data32, data);
		if (err)
			return err;

		return qseecom_ioctl(file, convert_cmd(cmd),
						(unsigned long)data);
	}
	break;
	default:
		return -ENOIOCTLCMD;
	break;
	}
	return 0;
}

