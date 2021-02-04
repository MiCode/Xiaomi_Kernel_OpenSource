/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/errno.h>	/* error define */
#include <linux/slab.h>
#include <linux/types.h>	/*typedef used */
#include <linux/fs.h>		/*flip_open API */
#include <linux/proc_fs.h>	/*proc_create API */
#include <linux/statfs.h>	/* kstatfs struct */
#include <linux/file.h>		/*kernel write and kernel read */
#include <linux/uaccess.h>	/*copy_to_user copy_from_user */
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/init.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include "mtk_sysenv.h"
#include "mtk_partition.h"

enum {
	ENV_UNINIT = 0,
	ENV_INIT,
	ENV_READY,
};

static char env_get_char(int index);
static char *env_get_addr(int index);
static int envmatch(char *s1, int i2);
static void env_init(void);
static int get_env_valid_length(void);
static char *findenv(const char *name);

struct env_struct g_env;
static int env_valid;
static u8 *env_buffer;
static int env_init_state = ENV_UNINIT;
static struct sock *netlink_sock;
static long user_sysenv_pid = -1;

#define ENV_NAME  "para"
#define MODULE_NAME "KL_ENV"
#define TAG_SET_ENV "SYSENV_SET_ENV"

static int send_sysenv_msg(int pid, int seq, void *payload, int payload_len);
static ssize_t env_proc_read(struct file *file,
				char __user *buf, size_t size, loff_t *ppos)
{
	char p[32];
	char *page = (char *)p;
	int err = 0;
	ssize_t len = 0;
	int env_valid_length = 0;

	if (env_init_state != ENV_READY) {
		pr_notice("[%s] data not ready yet\n", MODULE_NAME);
		return 0;
	}
	if (!env_valid) {
		pr_debug("[%s]read no env valid\n", MODULE_NAME);
		page += snprintf(page, 32, "\nno env valid\n");
		len = page - &p[0];

		if (*ppos >= len)
			return 0;
		err = copy_to_user(buf, (char *)p, len);
		*ppos += len;
		if (err)
			return err;
		return len;
	}
	env_valid_length = get_env_valid_length();
	if (*ppos >= env_valid_length)
		return 0;
	if ((size + *ppos) > env_valid_length)
		size = env_valid_length - *ppos;
	err = copy_to_user(buf, g_env.env_data + *ppos, size);
	if (err)
		return err;
	*ppos += size;
	return size;
}

static ssize_t
env_proc_write(struct file *file,
		const char __user *buf, size_t size, loff_t *ppos)
{
	u8 *buffer = NULL;
	int ret = 0, i, v_index = 0;
	if (size > CFG_ENV_DATA_SIZE) {
		ret = -ERANGE;
		pr_notice("[%s]break for size too large\n", MODULE_NAME);
		goto fail_malloc;
	}

	buffer = kzalloc(size+1, GFP_KERNEL);
	if (!buffer) {
		ret = -ENOMEM;
		pr_notice("[%s]alloc buffer fail\n", MODULE_NAME);
		goto fail_malloc;
	}

	if (copy_from_user(buffer, buf, size)) {
		ret = -EFAULT;
		goto end;
	}
	buffer[size] = 0;
	/*parse buffer into name and value */
	for (i = 0; i < size; i++) {
		if (buffer[i] == '=') {
			v_index = i + 1;
			buffer[i] = '\0';
			buffer[size - 1] = '\0';
			break;
		}
	}
	if (i == size) {
		ret = -EFAULT;
		pr_notice("[%s]write fail\n", MODULE_NAME);
		goto end;
	} else {
		pr_debug("[%s]name :%s,value: %s\n",
			MODULE_NAME, buffer, buffer + v_index);
	}
	ret = set_env(buffer, buffer + v_index);
end:
	kfree(buffer);
fail_malloc:
	if (ret)
		return ret;
	else
		return size;
}

static long env_proc_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	struct env_ioctl en_ctl;
	int ret = 0;
	u8 *name_buf = NULL;
	u8 *value_buf = NULL;
	char *value_r = NULL;

	memset(&en_ctl, 0x00, sizeof(struct env_ioctl));

	if (copy_from_user((void *)&en_ctl,
				(void *)arg, sizeof(struct env_ioctl))) {
		ret = -EFAULT;
		goto fail;
	}
	if (en_ctl.name_len <= 0 || en_ctl.value_len <= 0) {
		ret = -EPERM;
		goto end;
	}
	if (en_ctl.name_len >= CFG_ENV_DATA_SIZE ||
		en_ctl.value_len >= CFG_ENV_DATA_SIZE) {
		ret = -EFAULT;
		goto end;
	}

	name_buf = kmalloc(en_ctl.name_len+1, GFP_KERNEL);
	if (!name_buf) {
		ret = -ENOMEM;
		goto fail;
	}
	value_buf = kmalloc(en_ctl.value_len+1, GFP_KERNEL);
	if (!value_buf) {
		ret = -ENOMEM;
		goto fail_malloc;
	}
	if (copy_from_user((void *)name_buf,
			   (void *)en_ctl.name, en_ctl.name_len)) {
		ret = -EFAULT;
		goto end;
	}
	name_buf[en_ctl.name_len] = 0;
	if (*name_buf == '\0') {
		ret = 0;
		goto end;
	}
	switch (cmd) {
	case ENV_READ:
		value_r = get_env(name_buf);
		if (value_r == NULL) {
			ret = -EPERM;
			pr_notice("[%s]cann't find name=%s\n",
					MODULE_NAME, name_buf);
			goto end;
		}
		if ((strlen(value_r) + 1) > en_ctl.value_len) {
			ret = -EFAULT;
			goto end;
		}
		if (copy_to_user((void *)en_ctl.value,
				 (void *)value_r, strlen(value_r) + 1)) {
			ret = -EFAULT;
			goto end;
		}
		break;
	case ENV_USER_INIT:
		env_init_state = ENV_INIT;
		pr_notice("ENV_USER_INIT!\n");
	/* do not break, ENV_INIT taking the same process as ENV_WRITE */
	case ENV_WRITE:
		if (copy_from_user((void *)value_buf,
				   (void *)en_ctl.value, en_ctl.value_len)) {
			ret = -EFAULT;
			goto end;
		}
		value_buf[en_ctl.value_len] = 0;
		ret = set_env(name_buf, value_buf);
		break;
	case ENV_SET_PID:
		if (copy_from_user((void *)value_buf,
				   (void *)en_ctl.value, en_ctl.value_len)) {
			ret = -EFAULT;
			goto end;
		}
		value_buf[en_ctl.value_len - 1] = '\0';
		ret = kstrtol(value_buf, 10, &user_sysenv_pid);
		pr_debug("[%s] user_sysenv_pid = %ld\n",
				MODULE_NAME, user_sysenv_pid);
		pr_debug("[%s] user space sysenv init done\n",
				MODULE_NAME);
		env_init_state = ENV_READY;
		ret = 0;
		break;
	default:
		pr_debug("[%s]Undefined command\n", MODULE_NAME);
		ret = -EINVAL;
		goto end;
	}
end:
	kfree(value_buf);
fail_malloc:
	kfree(name_buf);
fail:
	return ret;
}
#ifdef CONFIG_COMPAT
struct env_compat_ioctl {
	compat_uptr_t name;
	compat_int_t name_len;
	compat_uptr_t value;
	compat_int_t value_len;
};

static int compat_get_env_ioctl(struct env_compat_ioctl __user *arg32,
				struct env_ioctl __user *arg64)
{
	compat_int_t i;
	compat_uptr_t p;
	int err;

	err = get_user(p, &(arg32->name));
	err |= put_user(compat_ptr(p), &(arg64->name));
	err |= get_user(i, &(arg32->name_len));
	err |= put_user(i, &(arg64->name_len));
	err |= get_user(p, &(arg32->value));
	err |= put_user(compat_ptr(p), &(arg64->value));
	err |= get_user(i, &(arg32->value_len));
	err |= put_user(i, &(arg64->value_len));
	return err;
}

static long env_proc_compat_ioctl(struct file *filp,
					unsigned int cmd, unsigned long arg)
{
	struct env_compat_ioctl *arg32;
	struct env_ioctl *arg64;
	int err;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		pr_notice("f_op or unlocked ioctl is NULL.\n");
		return -ENOTTY;
	}
	arg32 = compat_ptr(arg);
	arg64 = compat_alloc_user_space(sizeof(*arg64));
	err = compat_get_env_ioctl(arg32, arg64);
	if (err)
		return err;
	return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long) arg64);
}
#endif

static const struct file_operations env_proc_fops = {
	.read = env_proc_read,
	.write = env_proc_write,
	.unlocked_ioctl = env_proc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = env_proc_compat_ioctl,
#endif
};

static int get_env_valid_length(void)
{
	int len = 0;

	if (!env_valid)
		return 0;
	for (len = 0; len < CFG_ENV_DATA_SIZE; len++) {
		if (g_env.env_data[len] == '\0' &&
			g_env.env_data[len + 1] == '\0')
			break;
	}
	return len;
}

static void env_init(void)
{
	pr_debug("[%s]ENV initialize\n", MODULE_NAME);

	env_buffer = kzalloc(CFG_ENV_SIZE, GFP_KERNEL);
	if (!env_buffer)
		return;

	g_env.env_data = env_buffer + CFG_ENV_DATA_OFFSET;

	memset(env_buffer, 0x00, CFG_ENV_SIZE);
}

static char *findenv(const char *name)
{
	int i, nxt, val;

	for (i = 0; env_get_char(i) != '\0'; i = nxt + 1) {
		for (nxt = i; env_get_char(nxt) != '\0'; ++nxt) {
			if (nxt >= CFG_ENV_SIZE)
				return NULL;
		}
		val = envmatch((char *)name, i);
		if (val < 0)
			continue;
		return (char *)env_get_addr(val);
	}
	return NULL;
}

char *get_env(const char *name)
{
	pr_debug("[%s]get env name=%s\n", MODULE_NAME, name);

	if (!env_valid)
		return NULL;
	return findenv(name);
}
EXPORT_SYMBOL(get_env);

static char env_get_char(int index)
{
	return *(g_env.env_data + index);
}

static char *env_get_addr(int index)
{
	return g_env.env_data + index;

}

static int envmatch(char *s1, int i2)
{
	while (*s1 == env_get_char(i2++)) {
		if (*s1++ == '=')
			return i2;
	}
	if (*s1 == '\0' && env_get_char(i2 - 1) == '=')
		return i2;
	return -1;
}

int set_user_env(char *name, char *value)
{
	int ret = 0;
	int data_len;
	char *data;
	/* 3 : 2 space and 1 \0 */
	data_len = strlen(name) + strlen(value) + strlen(TAG_SET_ENV) + 3;
	data = kmalloc(data_len, GFP_KERNEL); /* 3 : 2 space and 1 \0 */
	if (data == NULL) {
		pr_notice("[%s] %s allocate %d buffer fail!\n",
				MODULE_NAME, __func__, data_len);
		return -1;
	}
	pr_debug("[%s]set user_env, name=%s,value=%s\n",
				MODULE_NAME, name, value);
	snprintf(data, data_len, "%s %s=%s", TAG_SET_ENV, name, value);
	ret = send_sysenv_msg(user_sysenv_pid, 0, data, data_len);
	kfree(data);
	return ret;
}

int set_env(char *name, char *value)
{
	int len;
	int oldval = -1;
	char *env, *nxt = NULL;
	char *env_data = NULL;
	char *name_base, *value_base;

	pr_debug("[%s]set env, name=%s,value=%s\n", MODULE_NAME, name, value);

	if (env_init_state == ENV_UNINIT) {
		pr_notice("[%s] data not ready yet\n", MODULE_NAME);
		return 0;
	}

	name_base = name;
	value_base = value;
	env_data = g_env.env_data;
	if (!env_buffer)
		return -1;
	if (!env_valid) {
		env = env_data;
		goto add;
	}
/* find match name and return the val header pointer*/
	for (env = env_data; *env; env = nxt + 1) {
		for (nxt = env; *nxt; ++nxt)
			;
		oldval = envmatch((char *)name, env - env_data);
		if (oldval >= 0)
			break;
	}			/* end find */
	if (oldval > 0) {
		if (*++nxt == '\0') {
			if (env > env_data)
				env--;
			else
				*env = '\0';
		} else {
			for (;;) {
				*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
					break;
				++env;
			}
		}
		*++env = '\0';
	}

	for (env = env_data; *env || *(env + 1); ++env)
		;
	if (env > env_data)
		++env;
add:
	if (*value == '\0') {
		pr_debug("[%s]clear env name=%s\n", MODULE_NAME, name);
		goto write_env;
	}

	len = strlen(name) + 2;
	len += strlen(value) + 1;
	if (len > (&env_data[CFG_ENV_DATA_SIZE] - env)) {
		pr_notice("[%s]env data overflow, %s deleted\n",
				MODULE_NAME, name);
		return -1;
	}
	while ((*env = *name++) != '\0')
		env++;
	*env = '=';
	while ((*++env = *value++) != '\0')
		;
write_env:
/* end is marked with double '\0' */
	*++env = '\0';
	memset(env, 0x00, CFG_ENV_DATA_SIZE - (env - env_data));
	if (env_init_state == ENV_READY) {
		if (set_user_env(name_base, value_base) < 0)
			pr_notice("[%s]set user env fail: %s=%s\n",
					MODULE_NAME, name, value);
	}
	env_valid = 1;
	return 0;
}
EXPORT_SYMBOL(set_env);

static int send_sysenv_msg(int pid, int seq, void *payload, int payload_len)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int size = payload_len;
	int len = NLMSG_SPACE(size);
	void *data;
	int ret;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return -1;
	if (len < payload_len) {
		pr_notice("[%s] payload is %d larger than skb len %d\n",
				MODULE_NAME, payload_len, len);
		kfree_skb(skb);
		return -1;
	}
	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
	if (!nlh) {
		kfree_skb(skb);
		return -EMSGSIZE;
	}
	data = nlmsg_data(nlh);
	memcpy(data, payload, size);
	NETLINK_CB(skb).portid = 0; /* from kernel */
	NETLINK_CB(skb).dst_group = 0; /* unicast */
	ret = netlink_unicast(netlink_sock, skb, pid, MSG_DONTWAIT);
	pr_debug("[%s] send %d data to user process(%d), ret = %d\n",
				MODULE_NAME, len, pid, ret);
	if (ret < 0) {
		pr_notice("[%s] send failed\n", MODULE_NAME);
		return -1;
	}
	return 0;
}

static int __init sysenv_init(void)
{
	struct proc_dir_entry *sysenv_proc;

	sysenv_proc = proc_create("lk_env", 0600, NULL, &env_proc_fops);
	if (!sysenv_proc)
		pr_notice("[%s]fail to create /proc/lk_env\n", MODULE_NAME);

	env_init();

	netlink_sock = netlink_kernel_create(&init_net,
						NETLINK_USERSOCK, NULL);
	if (netlink_sock == NULL)
		pr_notice("[%s] netlink_kernel_create fail!\n", MODULE_NAME);

	return 0;
}

static void __exit sysenv_exit(void)
{
	remove_proc_entry("lk_env", NULL);
}

module_init(sysenv_init);
module_exit(sysenv_exit);
