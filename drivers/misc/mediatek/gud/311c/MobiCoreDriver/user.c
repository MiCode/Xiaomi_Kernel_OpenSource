/*
 * Copyright (c) 2013-2017 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/mm_types.h>	/* struct vm_area_struct */

#include "public/mc_user.h"

#include "main.h"
#include "admin.h"	/* is_authenticator_pid */
#include "user.h"
#include "client.h"
#include "mcp.h"	/* mcp_get_version */

/*
 * Get client object from file pointer
 */
static inline struct tee_client *get_client(struct file *file)
{
	return (struct tee_client *)file->private_data;
}

/*
 * Callback for system open()
 * A set of internal client data are created and initialized.
 *
 * @inode
 * @file
 * Returns 0 if OK or -ENOMEM if no allocation was possible.
 */
static int user_open(struct inode *inode, struct file *file)
{
	struct tee_client *client;

	/* Create client */
	mc_dev_devel("from %s (%d)\n", current->comm, current->pid);
	client = client_create(false);
	if (!client)
		return -ENOMEM;

	/* Store client in user file */
	file->private_data = client;
	return 0;
}

/*
 * Callback for system close()
 * The client object is freed.
 * @inode
 * @file
 * Returns 0
 */
static int user_release(struct inode *inode, struct file *file)
{
	struct tee_client *client = get_client(file);

	/* Close client */
	mc_dev_devel("from %s (%d)\n", current->comm, current->pid);
	if (WARN(!client, "No client data available"))
		return -EPROTO;

	/* Detach client from user file */
	file->private_data = NULL;

	/* Destroy client, including remaining sessions */
	client_close(client);
	return 0;
}

/*
 * Check r/w access to referenced memory
 */
static inline int ioctl_check_pointer(unsigned int cmd, int __user *uarg)
{
	int err = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, uarg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, uarg, _IOC_SIZE(cmd));

	if (err)
		return -EFAULT;

	return 0;
}

/*
 * Callback for system ioctl()
 * Implement most of ClientLib API functions
 * @file	pointer to file
 * @cmd		command
 * @arg		arguments
 *
 * Returns 0 for OK and an errno in case of error
 */
static long user_ioctl(struct file *file, unsigned int id, unsigned long arg)
{
	struct tee_client *client = get_client(file);
	int __user *uarg = (int __user *)arg;
	int ret = -EINVAL;

	mc_dev_devel("%u from %s\n", _IOC_NR(id), current->comm);

	if (WARN(!client, "No client data available"))
		return -EPROTO;

	if (ioctl_check_pointer(id, uarg))
		return -EFAULT;

	switch (id) {
	case MC_IO_HAS_SESSIONS:
		/* Freeze the client */
		if (client_has_sessions(client))
			ret = -ENOTEMPTY;
		else
			ret = 0;
		break;

	case MC_IO_OPEN_SESSION: {
		struct mc_ioctl_open_session session;

		if (copy_from_user(&session, uarg, sizeof(session))) {
			ret = -EFAULT;
			break;
		}

		ret = client_open_session(client, &session.sid, &session.uuid,
					  session.tci, session.tcilen,
					  session.is_gp_uuid,
					  &session.identity, session.pid,
					  session.flags);
		if (ret)
			break;

		if (copy_to_user(uarg, &session, sizeof(session))) {
			ret = -EFAULT;
			client_remove_session(client, session.sid);
			break;
		}
		break;
	}
	case MC_IO_OPEN_TRUSTLET: {
		struct mc_ioctl_open_trustlet trustlet;

		if (copy_from_user(&trustlet, uarg, sizeof(trustlet))) {
			ret = -EFAULT;
			break;
		}

		/* Call internal api */
		ret = client_open_trustlet(client, &trustlet.sid, trustlet.spid,
					   trustlet.buffer, trustlet.tlen,
					   trustlet.tci, trustlet.tcilen,
					   trustlet.pid, trustlet.flags);
		if (ret)
			break;

		if (copy_to_user(uarg, &trustlet, sizeof(trustlet))) {
			ret = -EFAULT;
			client_remove_session(client, trustlet.sid);
			break;
		}
		break;
	}
	case MC_IO_CLOSE_SESSION: {
		u32 sid = (u32)arg;

		ret = client_remove_session(client, sid);
		break;
	}
	case MC_IO_NOTIFY: {
		u32 sid = (u32)arg;

		ret = client_notify_session(client, sid);
		break;
	}
	case MC_IO_WAIT: {
		struct mc_ioctl_wait wait;

		if (copy_from_user(&wait, uarg, sizeof(wait))) {
			ret = -EFAULT;
			break;
		}
		ret = client_waitnotif_session(client, wait.sid, wait.timeout,
					       wait.partial);
		break;
	}
	case MC_IO_MAP: {
		struct mc_ioctl_map map;

		if (copy_from_user(&map, uarg, sizeof(map))) {
			ret = -EFAULT;
			break;
		}

		ret = client_map_session_wsms(client, map.sid, map.bufs);
		if (ret)
			break;

		/* Fill in return struct */
		if (copy_to_user(uarg, &map, sizeof(map))) {
			ret = -EFAULT;
			client_unmap_session_wsms(client, map.sid, map.bufs);
			break;
		}
		break;
	}
	case MC_IO_UNMAP: {
		struct mc_ioctl_map map;

		if (copy_from_user(&map, uarg, sizeof(map))) {
			ret = -EFAULT;
			break;
		}

		ret = client_unmap_session_wsms(client, map.sid, map.bufs);
		break;
	}
	case MC_IO_ERR: {
		struct mc_ioctl_geterr __user *uerr =
			(struct mc_ioctl_geterr __user *)uarg;
		u32 sid;
		s32 exit_code;

		if (get_user(sid, &uerr->sid)) {
			ret = -EFAULT;
			break;
		}

		ret = client_get_session_exitcode(client, sid, &exit_code);
		if (ret)
			break;

		/* Fill in return struct */
		if (put_user(exit_code, &uerr->value)) {
			ret = -EFAULT;
			break;
		}

		break;
	}
	case MC_IO_VERSION: {
		struct mc_version_info version_info;

		ret = mcp_get_version(&version_info);
		if (ret)
			break;

		if (copy_to_user(uarg, &version_info, sizeof(version_info)))
			ret = -EFAULT;

		break;
	}
	case MC_IO_AUTHENTICATOR_CHECK: {
		struct mc_authenticator_check info;

		if (copy_from_user(&info, uarg, sizeof(info))) {
			ret = -EFAULT;
			break;
		}
		ret = is_authenticator_pid(info.pid);
		break;
	}
	default:
		mc_dev_notice("unsupported command no %d\n", id);
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

/*
 * Callback for system mmap()
 */
static int user_mmap(struct file *file, struct vm_area_struct *vmarea)
{
	struct tee_client *client = get_client(file);

	if ((vmarea->vm_end - vmarea->vm_start) > BUFFER_LENGTH_MAX)
		return -EINVAL;

	/* Alloc contiguous buffer for this client */
	return client_cbuf_create(client,
				  (u32)(vmarea->vm_end - vmarea->vm_start),
				  NULL, vmarea);
}

static const struct file_operations mc_user_fops = {
	.owner = THIS_MODULE,
	.open = user_open,
	.release = user_release,
	.unlocked_ioctl = user_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = user_ioctl,
#endif
	.mmap = user_mmap,
};

int mc_user_init(struct cdev *cdev)
{
	cdev_init(cdev, &mc_user_fops);
	return 0;
}
