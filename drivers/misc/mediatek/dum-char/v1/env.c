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

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/statfs.h>
#include <asm/uaccess.h>	/*set_fs get_fs mm_segment_t */
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
#include "partition_define_tlc.h"
#else
#include "partition_define_mlc.h"
#endif

#include <env.h>

#ifndef CONFIG_MTK_EMMC_SUPPORT
#include <linux/syscalls.h>
#include <mtd/mtd-abi.h>
#include <linux/mtd/mtd.h>
#include <linux/mutex.h>
static DEFINE_MUTEX(env_first_read_mutex);
static int is_first_read_write;
static int misc_mtd_number = -1;
#endif

#define MODULE_NAME "LK_ENV"

#define DATA_FREE_SIZE_TH_NAME "data_free_size_th"

env_t g_env;
static int env_valid;
static char *env_buffer;
#ifdef CONFIG_MTK_EMMC_SUPPORT
static loff_t env_addr;
#endif
static char env_get_char(int index);
static char *env_get_addr(int index);
static int envmatch(char *s1, int i2);
static int write_env_area(char *env_buf);
static int read_env_area(char *env_buf);
static void load_default_env(void);
static int get_env_valid_length(void);
static int ENV_Nand_Write(char *env_buf);
static int ENV_Nand_Read(char *env_buf);

#ifdef LIMIT_SDCARD_SIZE
long long data_free_size_th = DATA_FREE_SIZE_TH_DEFAULT;
#else
long long data_free_size_th = 0;
#endif

#ifndef CONFIG_MTK_EMMC_SUPPORT

/* /dev/misc is not exist until Android is started, postpone read Nand to first read/write function */
static int ENV_Self_Init(char *env_buf)
{
	int checksum = 0;
	int i;
	int ret = 0;

	mutex_lock(&env_first_read_mutex);
	if (is_first_read_write < 2)	/* limit variable count to avoid overflow */
		is_first_read_write++;


	if (is_first_read_write == 1) {

		ret = read_env_area(env_buf);
		if (ret < 0) {
			is_first_read_write = 0;
			mutex_unlock(&env_first_read_mutex);
			return 0;
		}

		memcpy(g_env.sig, env_buffer, sizeof(g_env.sig));
		memcpy(g_env.sig_1, env_buffer + CFG_ENV_SIG_1_OFFSET, sizeof(g_env.sig_1));

		if (!strcmp(g_env.sig, ENV_SIG) && !strcmp(g_env.sig_1, ENV_SIG)) {
			g_env.checksum = *((int *)env_buffer + CFG_ENV_CHECKSUM_OFFSET / 4);
			for (i = 0; i < CFG_ENV_DATA_SIZE; i++)
				checksum += g_env.env_data[i];

			if (checksum != g_env.checksum) {
				pr_err("[%s]checksum mismatch s %d d %d!\n", MODULE_NAME,
				       g_env.checksum, checksum);
				env_valid = 0;
				goto end;
			} else {
				pr_err("[%s]ENV initialize success\n", MODULE_NAME);
				env_valid = 1;
			}

		} else {
			pr_err("[%s]ENV SIG Wrong\n", MODULE_NAME);
			env_valid = 0;
			goto end;
		}

end:
		if (!env_valid)
			memset(env_buffer, 0x00, CFG_ENV_SIZE);

		mutex_unlock(&env_first_read_mutex);

		load_default_env();

	} else {
		mutex_unlock(&env_first_read_mutex);
	}
	return 0;

}
#endif

static ssize_t env_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int err = 0;
	int env_valid_length = get_env_valid_length();

#ifndef CONFIG_MTK_EMMC_SUPPORT
	ENV_Self_Init(env_buffer);
#endif

	if (!env_valid) {
		char p[32];
		char *page = (char *)p;
		int err = 0;
		ssize_t len = 0;

		pr_err("no env valid\n");
		page += sprintf(page, "\nno env valid\n");
		len = page - &p[0];

		if (*ppos >= len)
			return 0;

		err = copy_to_user(buf, (char *)p, len);
		*ppos += len;
		if (err)
			return err;
		return len;
	}
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

static ssize_t env_proc_write(struct file *file, const char __user *buf, size_t size,
			      loff_t *ppos)
{
	char *buffer = NULL;
	int ret, i, v_index = 0;

#ifndef CONFIG_MTK_EMMC_SUPPORT
	ENV_Self_Init(env_buffer);
#endif


	buffer = kmalloc(size, GFP_KERNEL);

	if (buffer == NULL) {
		ret = -ENOMEM;
		pr_err("[env_proc_write]alloc buffer fail\n");
		goto fail_malloc;
	}
	memset(buffer, 0x00, size);
	if (copy_from_user(buffer, buf, size)) {
		ret = -EFAULT;
		goto end;
	}

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
		pr_err("write fail, buffer:%s\n", buffer);
		ret = -EFAULT;
		goto end;
	} else {
		pr_err("[env_proc_write]name :%s,value:%s\n", buffer, buffer + v_index);
	}

	if (!strcmp(buffer, DATA_FREE_SIZE_TH_NAME)) {
#ifdef LIMIT_SDCARD_SIZE
		struct kstatfs stat;
		long long data_free_size = 0;
		char *value = buffer + v_index;
		long long value_new = 0;
		int mum = 1;
		char value_buf[20] = { 0 };
		struct file *fp = NULL;
		char tmp_buff[20];

		for (i = 0; i < strlen(value); i++) {
			if (value[i] == 'M' || value[i] == 'm') {
				mum = 1024 * 1024;
				break;
			} else if (value[i] == 'K' || value[i] == 'k') {
				mum = 1024;
				break;
			}
			value_buf[i] = value[i];

		}
		kstrtoll(value_buf, &value_new);
		value_new *= num;
		fp = filp_open("/data", O_RDONLY, 0777);
		if (IS_ERR(fp)) {
			pr_err("open data fail\n");
			ret = -EFAULT;
			goto end;
		}

		vfs_statfs(&fp->f_path, &stat);
		data_free_size = stat.f_bfree * stat.f_bsize;
		filp_close(fp, NULL);
		if (value_new >= data_free_size) {
			pr_err("new value %llx more than data free size %llx, setting fail\n",
			       value_new, data_free_size);
			ret = -EFAULT;
			goto end;
		}

		sprintf(tmp_buff, "%lld", value_new);
		ret = set_env(buffer, tmp_buff);
#else
		pr_err("[env_proc_write]it don't support limit sdcard size\n");
		ret = -EFAULT;
		goto end;
#endif
	} else {
		ret = set_env(buffer, buffer + v_index);
	}

 /**/ end:
	kfree(buffer);
fail_malloc:
	if (ret)
		return ret;
	else
		return size;

}

static long env_proc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct env_ioctl en_ctl;
	int ret = 0;
	char *name_buf = NULL;
	char *value_buf = NULL;
	char *value_r = NULL;

#ifndef CONFIG_MTK_EMMC_SUPPORT
	ENV_Self_Init(env_buffer);
#endif

	memset(&en_ctl, 0x00, sizeof(struct env_ioctl));

	if (copy_from_user((void *)&en_ctl, (void *)arg, sizeof(struct env_ioctl))) {
		ret = -EFAULT;
		goto fail;
	}

	if (en_ctl.name_len <= 0 || en_ctl.value_len <= 0) {
		ret = 0;
		goto end;
	}

	name_buf = kmalloc(en_ctl.name_len, GFP_KERNEL);
	if (!name_buf) {
		ret = -ENOMEM;
		goto fail;
	}
	value_buf = kmalloc(en_ctl.value_len, GFP_KERNEL);
	if (!value_buf) {
		ret = -ENOMEM;
		goto fail_malloc;
	}

	if (copy_from_user((void *)name_buf, (void *)en_ctl.name, en_ctl.name_len)) {
		ret = -EFAULT;
		goto end;
	}

	if (*name_buf == '\0') {
		ret = 0;
		goto end;
	}

	switch (cmd) {
	case ENV_READ:
		value_r = get_env(name_buf);
		if (value_r == NULL) {
			ret = 0;
			pr_err("[lk_env]cann't find %s\n", name_buf);
			goto end;
		}
		if ((strlen(value_r) + 1) > en_ctl.value_len) {
			ret = -EFAULT;
			goto end;
		}
		if (copy_to_user((void *)en_ctl.value, (void *)value_r, strlen(value_r) + 1)) {
			ret = -EFAULT;
			goto end;
		}
		break;
	case ENV_WRITE:
		if (copy_from_user((void *)value_buf, (void *)en_ctl.value, en_ctl.value_len)) {
			ret = -EFAULT;
			goto end;
		}
		ret = set_env(name_buf, value_buf);
		break;
	default:
		pr_err("[lk_env]wrong cmd\n");
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

static const struct file_operations env_proc_fops = {
	.read = env_proc_read,
	.write = env_proc_write,
	.unlocked_ioctl = env_proc_ioctl,
};

static int get_env_valid_length(void)
{
	int len = 0;

	if (env_valid) {
		for (len = 0; len < CFG_ENV_DATA_SIZE; len++) {
			if (g_env.env_data[len] == '\0' && g_env.env_data[len + 1] == '\0')
				break;
		}
		return len;
	} else {
		return 0;
	}
}

#ifdef CONFIG_MTK_EMMC_SUPPORT
void env_init(loff_t env_part_addr)
#else
void env_init(loff_t env_part_addr, int mtd_number)
#endif
{
#ifdef CONFIG_MTK_EMMC_SUPPORT
	int ret, i;
	int checksum = 0;
#endif
	struct proc_dir_entry *env_proc;

	pr_err("[%s]ENV initialize env_part_addr %llx\n", MODULE_NAME, env_part_addr);

#ifdef CONFIG_MTK_EMMC_SUPPORT
	if (!env_part_addr) {
		pr_err("env_part_addr is 0, env_init fail\n");
		return;
	}

	env_addr = env_part_addr + CFG_ENV_OFFSET;
#endif

	env_buffer = kmalloc(CFG_ENV_SIZE, GFP_KERNEL);
	if (!env_buffer)
		return;

	memset(env_buffer, 0x00, CFG_ENV_SIZE);
	g_env.env_data = env_buffer + CFG_ENV_DATA_OFFSET;

	env_proc = proc_create("lk_env", 0600, NULL, &env_proc_fops);
	if (env_proc == NULL) {
		pr_err("create /proc/lk_env fail\n");
		return;
	}
#ifdef CONFIG_MTK_EMMC_SUPPORT	/* postpone Nand init to first read/write operation */
	ret = read_env_area(env_buffer);

	if (ret < 0) {
		pr_err("[%s]read_env_area fail, ret = %x\n", MODULE_NAME, ret);
		env_valid = 0;
		goto end;
	}

	memcpy(g_env.sig, env_buffer, sizeof(g_env.sig));
	memcpy(g_env.sig_1, env_buffer + CFG_ENV_SIG_1_OFFSET, sizeof(g_env.sig_1));

	if (!strcmp(g_env.sig, ENV_SIG) && !strcmp(g_env.sig_1, ENV_SIG)) {
		g_env.checksum = *((int *)env_buffer + CFG_ENV_CHECKSUM_OFFSET / 4);
		for (i = 0; i < CFG_ENV_DATA_SIZE; i++)
			checksum += g_env.env_data[i];

		if (checksum != g_env.checksum) {
			pr_err("[%s]checksum mismatch s %d d %d!\n", MODULE_NAME, g_env.checksum,
			       checksum);
			env_valid = 0;
			goto end;
		} else {
			pr_err("[%s]ENV initialize success\n", MODULE_NAME);
			env_valid = 1;
		}

	} else {
		pr_err("[%s]ENV SIG Wrong\n", MODULE_NAME);
		env_valid = 0;
		goto end;
	}

end:
	if (!env_valid)
		memset(env_buffer, 0x00, CFG_ENV_SIZE);

	load_default_env();
#else				/* not def MTK_EMMC_SUPPORT */

	/* spin_lock_init(&env_lock); */
	misc_mtd_number = mtd_number;

#endif				/* #ifdef MTK_EMMC_SUPPORT */
}
EXPORT_SYMBOL(env_init);

static void load_default_env(void)
{
	int i, ret;
	char *tmp;
	int mum = 1;
	char load_buf[20] = { 0 };

	pr_err("[%s]load default env\n", MODULE_NAME);

	tmp = get_env(DATA_FREE_SIZE_TH_NAME);
	if (tmp == NULL) {
		pr_err("[%s]can not find %s,set the default value\n", MODULE_NAME,
		       DATA_FREE_SIZE_TH_NAME);
		sprintf(load_buf, "%d", DATA_FREE_SIZE_TH_DEFAULT);
		set_env(DATA_FREE_SIZE_TH_NAME, load_buf);
		return;
	}
	for (i = 0; i < strlen(tmp); i++) {
		if (tmp[i] == 'M' || tmp[i] == 'm') {
			mum = 1024 * 1024;
			break;
		} else if (tmp[i] == 'K' || tmp[i] == 'k') {
			mum = 1024;
			break;
		}
		load_buf[i] = tmp[i];
	}
	ret = kstrtoll(load_buf, 10, &data_free_size_th);
	data_free_size_th *= mum;

	pr_err("[%s]find %s = %llx\n", MODULE_NAME, DATA_FREE_SIZE_TH_NAME, data_free_size_th);
}

char *get_env(char *name)
{
	int i, nxt;
#ifndef CONFIG_MTK_EMMC_SUPPORT
	ENV_Self_Init(env_buffer);
#endif
	pr_err("[%s]get_env %s\n", MODULE_NAME, name);
	if (!env_valid)
		return NULL;

	for (i = 0; env_get_char(i) != '\0'; i = nxt + 1) {
		int val;

		for (nxt = i; env_get_char(nxt) != '\0'; ++nxt) {
			if (nxt >= CFG_ENV_SIZE)
				return NULL;

		}
		val = envmatch((char *)name, i);
		if (val < 0)
			continue;
		return ((char *)env_get_addr(val));
	}

	return NULL;
}
EXPORT_SYMBOL(get_env);
static char env_get_char(int index)
{
	return *(g_env.env_data + index);
}

static char *env_get_addr(int index)
{
	return (g_env.env_data + index);

}

static int envmatch(char *s1, int i2)
{

	while (*s1 == env_get_char(i2++))
		if (*s1++ == '=')
			return i2;
	if (*s1 == '\0' && env_get_char(i2 - 1) == '=')
		return i2;
	return (-1);
}


int set_env(char *name, char *value)
{
	int len, oldval;
	char *env, *nxt = NULL;

	int ret;

	char *env_data = g_env.env_data;

#ifndef CONFIG_MTK_EMMC_SUPPORT
	ENV_Self_Init(env_buffer);
#endif

	pr_err("[%s]set_env %s %s\n", MODULE_NAME, name, value);

	oldval = -1;
	if (!env_buffer)
		return -1;

	if (!env_valid) {
		env = env_data;
		goto add;
	}

	for (env = env_data; *env; env = nxt + 1) {
		for (nxt = env; *nxt; ++nxt)
			;
		oldval = envmatch((char *)name, env - env_data);
		if (oldval >= 0)
			break;
	}

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
		pr_err("[LK_ENV]clear %s\n", name);
		goto write_env;
	}
	/*
	 * Overflow when:
	 * "name" + "=" + "val" +"\0\0"  > ENV_SIZE - (env-env_data)
	 */
	len = strlen(name) + 2;
	/* add '=' for first arg, ' ' for all others */
	len += strlen(value) + 1;

	if (len > (&env_data[CFG_ENV_DATA_SIZE] - env)) {
		pr_err("## Error: environment overflow, \"%s\" deleted\n", name);
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

	ret = write_env_area(env_buffer);
	if (ret < 0) {
		pr_err("[%s]write env fail\n", MODULE_NAME);
		memset(env_buffer, 0x00, CFG_ENV_SIZE);
		return -1;
	}
	env_valid = 1;
	return 0;

}
EXPORT_SYMBOL(set_env);
static int write_env_area(char *env_buf)
{
	int reval;
	int i, checksum = 0;


	memcpy(env_buf, ENV_SIG, sizeof(g_env.sig));
	memcpy(env_buf + CFG_ENV_SIG_1_OFFSET, ENV_SIG, sizeof(g_env.sig));

	for (i = 0; i < (CFG_ENV_DATA_SIZE); i++)
		checksum += *(env_buf + CFG_ENV_DATA_OFFSET + i);

	*((int *)env_buf + CFG_ENV_CHECKSUM_OFFSET / 4) = checksum;

#ifdef CONFIG_MTK_EMMC_SUPPORT
#ifdef CONFIG_MTK_NEW_COMBO_EMMC_SUPPORT
	reval =
	    eMMC_rw_x((loff_t) env_addr, (u32 *) env_buf, 0, 1, CFG_ENV_SIZE, 1, EMMC_PART_USER);
#else
	reval = eMMC_rw_x((loff_t) env_addr, (u32 *) env_buf, 0, 1, CFG_ENV_SIZE, 1, USER);
#endif
	if (reval)
		return -EIO;

#else
	reval = ENV_Nand_Write(env_buf);
	if (reval < 0)
		return -EIO;

#endif
	return 0;

}

static int read_env_area(char *env_buf)
{
#ifdef CONFIG_MTK_EMMC_SUPPORT
	int reval;
#ifdef CONFIG_MTK_NEW_COMBO_EMMC_SUPPORT
	reval =
	    eMMC_rw_x((loff_t) env_addr, (u32 *) env_buf, 0, 0, CFG_ENV_SIZE, 1, EMMC_PART_USER);
#else
	reval = eMMC_rw_x((loff_t) env_addr, (u32 *) env_buf, 0, 0, CFG_ENV_SIZE, 1, USER);
#endif
	if (reval)
		return -EIO;

#else
	int reval;

	reval = ENV_Nand_Read(env_buf);
	if (reval < 0)
		return -EIO;

#endif
	return 0;
}


#ifndef CONFIG_MTK_EMMC_SUPPORT

#define LOG_DEVICE "/dev/misc"


int ENV_Nand_Read(char *env_buf)
{
	int result;
	loff_t offset = 0;
	char *buffer = env_buf;

	size_t read;
	struct mtd_info *mtd;

	if (misc_mtd_number < 0) {
		pr_err("ENV_Nand_Read: wrong misc mtd number: %d!!!\n", misc_mtd_number);
		return -1;
	}
	mtd = get_mtd_device(NULL, misc_mtd_number);
	if (!mtd) {
		pr_err("ENV_Nand_Read: get device info fail!!!\n");
		put_mtd_device(mtd);
		return -1;
	}
	offset = mtd->erasesize;
	result = mtd_read(mtd, offset, CFG_ENV_SIZE, &read, buffer);
	if (result) {
		pr_err("ENV_Nand_Read: read env data fail, result = %d !!!\n", result);
		put_mtd_device(mtd);
		return -1;
	}
	put_mtd_device(mtd);
	return 0;



}

static void env_erase_callback(struct erase_info *instr)
{
	wake_up((wait_queue_head_t *) instr->priv);
}

int ENV_Nand_Write(char *env_buf)
{
	int result = 0;
	loff_t offset = 0;
	char *buffer = env_buf;

	wait_queue_head_t waitq;
	DECLARE_WAITQUEUE(wait, current);
	size_t write;
	struct mtd_info *mtd;
	struct erase_info erase;

	if (misc_mtd_number < 0) {
		pr_err("ENV_Nand_Read: wrong misc mtd number: %d!!!\n", misc_mtd_number);
		return -1;
	}
	mtd = get_mtd_device(NULL, misc_mtd_number);
	if (!mtd) {
		pr_err("ENV_Nand_Read: get device info fail!!!\n");
		put_mtd_device(mtd);
		return -1;
	}
	offset = mtd->erasesize;

	init_waitqueue_head(&waitq);

	erase.mtd = mtd;
	erase.addr = offset;
	erase.len = mtd->erasesize;
	erase.callback = env_erase_callback;
	erase.priv = (unsigned long)&waitq;

	result = mtd_erase(mtd, &erase);
	if (!result) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&waitq, &wait);
		if (erase.state != MTD_ERASE_DONE && erase.state != MTD_ERASE_FAILED)
			schedule();
		remove_wait_queue(&waitq, &wait);
		set_current_state(TASK_RUNNING);

		result = (erase.state == MTD_ERASE_FAILED) ? -EIO : 0;
	}

	if (result) {
		pr_err("ENV_Nand_Write: erase device failed");
		put_mtd_device(mtd);
		return -1;
	}

	result = mtd_write(mtd, offset, CFG_ENV_SIZE, &write, buffer);
	if (result) {
		pr_err("ENV_Nand_Write: Write env data fail, result = %d !!!\n", result);
		put_mtd_device(mtd);
		return -1;
	}

	put_mtd_device(mtd);
	return 0;

}


#endif
