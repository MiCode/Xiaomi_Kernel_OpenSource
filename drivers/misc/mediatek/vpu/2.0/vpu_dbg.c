/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>

#include <m4u.h>

#include "vpu_dbg.h"
#include "vpu_drv.h"
#include "vpu_cmn.h"

#define ALGO_OF_MAX_POWER  (3)

/* global variables */
int g_vpu_log_level = 1;
unsigned int g_func_mask;

#ifdef MTK_VPU_DVT

#include "test/vpu_data_wpp.h"

#if 0
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>

static void vpu_save_file(char *filename, char *buf, int size)
{
	struct file *f = NULL;
	mm_segment_t fs;

	fs = get_fs();
	set_fs(KERNEL_DS);

	if (f == NULL)
		f = filp_open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);

	if (IS_ERR(f))
		LOG_DBG("fail to open %s\n", filename);

	f->f_op->write(f, buf, size, &f->f_pos);

	set_fs(fs);
	filp_close(f, NULL);
}
#else
#define vpu_save_file(...)
#endif

static void vpu_test_wpp(void)
{
#if 0
	const int width = 640;
	const int height = 360;
	const int sett_len = 1024;
	const int data_len = SIZE_OF_DATAPP_WPP;
	const int img_size = width * height;
	/* workaround: dsp accesses the illegal address */
	const int buf_size = img_size * 2 + data_len + sett_len + 0x200000;
	/* CHRISTODO */
	int TEMP_CORE = 0;

	struct vpu_user *user;
	struct vpu_request *req;
	struct vpu_buffer *buf;
	struct vpu_algo *algo;

	struct vpu_shared_memory_param mem_param;
	struct vpu_shared_memory *shared_mem;
	unsigned char *buf_va;
	unsigned int buf_pa;
	int ret;

	if (vpu_find_algo_by_name(TEMP_CORE, "ipu_flo_d2d_k3",
						&algo, true) != 0) {
		LOG_ERR("vpu test: can not find algo!\n");
		return;
	}

	mem_param.require_va = true;
	mem_param.require_pa = true;
	mem_param.size = buf_size;
	mem_param.fixed_addr = 0;
	if (vpu_alloc_shared_memory(&shared_mem, &mem_param)) {
		LOG_ERR("vpu test: alloc memory failed.\n");
		return;
	}

	buf_pa = shared_mem->pa;
	buf_va = (unsigned char *) shared_mem->va;
	LOG_DBG("vpu test: pa=0x%x, va=0x%p\n", buf_pa, buf_va);

	vpu_create_user(&user);

	vpu_alloc_request(&req);
	buf = &req->buffers[0];
	req->algo_id = algo->id;
	req->buffer_count = 3;
	/* buffer 0 */
	buf->format = VPU_BUF_FORMAT_IMG_Y8;
	buf->width = width;
	buf->height = height;
	buf->plane_count = 1;
	buf->planes[0].stride = width;
	buf->planes[0].ptr = buf_pa;
	buf->planes[0].length = width * height;
	buf->port_id = 0;

	/* buffer 1 */
	buf = &req->buffers[1];
	buf->format = VPU_BUF_FORMAT_IMG_Y8;
	buf->width = width;
	buf->height = height;
	buf->plane_count = 1;
	buf->planes[0].stride = width;
	buf->planes[0].ptr = buf_pa + width * height;
	buf->planes[0].length = width * height;
	buf->port_id = 1;

	/* buffer 2 */
	buf = &req->buffers[2];
	buf->format = VPU_BUF_FORMAT_DATA;
	buf->plane_count = 1;
	buf->planes[0].ptr = buf_pa + 2 * width * height;
	buf->planes[0].length = data_len;
	buf->port_id = 2;

	/* setting */
	req->sett_ptr = buf_pa + img_size * 2 + data_len;
	req->sett_length = sett_len;

	memcpy(buf_va, g_datasrc_640x360_wpp, img_size);
	memset(buf_va + img_size, 0x19, img_size);
	memcpy(buf_va + img_size * 2, g_datapp_640x360_wpp, data_len);

	vpu_push_request_to_queue(user, req);
	vpu_pop_request_from_queue(user, &req);

	/* compare with golden */
	ret = memcmp(buf_va + img_size,
				g_datadst_640x360_golden_wpp, img_size);
	LOG_INF("comparison result:%d", ret);
	vpu_save_file("/data/vpu_result_wpp.raw",
				buf_va + img_size, img_size);

	vpu_free_request(req);
	vpu_delete_user(user);
	vpu_free_shared_memory(shared_mem);
#endif
}

static void vpu_test_be_true(void)
{
#if 0
	struct emu_setting {
		int param1;
		int param2;
		int param3;
		int param4;
		int param5;
	};

	const int width = 64;
	const int height = 64;
	const int sett_len = sizeof(struct emu_setting);
	const int img_size = width * height;
	const int buf_size = img_size * 2 + sett_len;

	struct vpu_user *user;
	struct vpu_request *req;
	struct vpu_buffer *buf;
	struct vpu_algo *algo;
	struct emu_setting *sett;

	struct vpu_shared_memory_param mem_param;
	struct vpu_shared_memory *shared_mem;
	unsigned char *buf_va;
	unsigned int buf_pa;
	int ret;
	/* CHRISTODO */
	int TEMP_CORE = 0;

	if (vpu_find_algo_by_name(TEMP_CORE, "ipu_flo_d2d_k5",
						&algo, true) != 0) {
		LOG_ERR("vpu test: can not find algo!\n");
		return;
	}

	mem_param.require_va = true;
	mem_param.require_pa = true;
	mem_param.size = buf_size;
	mem_param.fixed_addr = 0;
	if (vpu_alloc_shared_memory(&shared_mem, &mem_param)) {
		LOG_ERR("vpu test: alloc memory failed.\n");
		return;
	}

	buf_pa = shared_mem->pa;
	buf_va = (unsigned char *) shared_mem->va;
	LOG_DBG("vpu test: pa=0x%x, va=0x%p\n", buf_pa, buf_va);

	vpu_create_user(&user);

	vpu_alloc_request(&req);
	buf = &req->buffers[0];
	req->algo_id = algo->id;
	req->buffer_count = 0;

	memset(buf_va, 0x1, width * height);

	/* update request's setting */
	sett = (struct emu_setting *)(buf_va + img_size * 2);
	sett->param1 = 1;
	sett->param2 = 2;
	sett->param3 = 3;
	sett->param4 = 4;
	sett->param5 = 0;
	req->sett_ptr = buf_pa + img_size * 2;
	req->sett_length = sett_len;

	vpu_push_request_to_queue(user, req);
	vpu_pop_request_from_queue(user, &req);

	/* set source buffer to the expected result, and compare
	 * with destination buffer
	 */
	memset(buf_va, 0x2, width * height);
	ret = memcmp(buf_va, buf_va + width * height, width * height);
	LOG_INF("vpu test: comparison result=%d and param5=%d",
			ret, sett->param5);

	vpu_free_request(req);
	vpu_delete_user(user);
	vpu_free_shared_memory(shared_mem);
#endif
}

static u64 test_value;
static struct vpu_user *test_user[10];
static int vpu_user_test_case1(void *arg)
{
#if 0
	struct vpu_user *user;
	struct vpu_request *req;
	int i;

	vpu_create_user(&user);

	for (i = 0; i < 100; i++) {
		vpu_alloc_request(&req);
		req->algo_id = ALGO_OF_MAX_POWER;
		req->buffer_count = 0;
		req->priv = i;
		vpu_push_request_to_queue(user, req);
	}

	for (i = 0; i < 100;) {
		if (vpu_pop_request_from_queue(user, &req)) {
			LOG_ERR("deque failed. i=%d\n", i);
		} else {
			if (req->priv != i) {
				LOG_ERR("outoforder req deque. i=%d,priv=%d\n",
						i, (int) req->priv);
			}
			i++;
			vpu_free_request(req);
		}
		msleep(30);
	}

	vpu_delete_user(user);
#endif
	return 0;
}

static int vpu_user_test_case2(void *arg)
{
#if 0
	struct vpu_user *user;
	int i;

	vpu_create_user(&user);

	for (i = 0; i < 100; i++) {
		struct vpu_request *req;

		if (vpu_alloc_request(&req)) {
			LOG_ERR("allocate request failed. i=%d\n", i);
			return 0;
		}

		req->algo_id = ALGO_OF_MAX_POWER;
		req->buffer_count = 0;
		vpu_push_request_to_queue(user, req);

		if (vpu_pop_request_from_queue(user, &req)) {
			LOG_ERR("deque request failed. i=%d\n", i);
			vpu_free_request(req);
			break;
		}

		vpu_free_request(req);
	}

	vpu_delete_user(user);
#endif
	return 0;
}

static int vpu_user_test_case3(void *arg)
{
#if 0
	struct vpu_user *user;
	struct vpu_request *req;
	int i;

	vpu_create_user(&user);

	for (i = 0; i < 100; i++) {
		vpu_alloc_request(&req);
		req->algo_id = ALGO_OF_MAX_POWER;
		req->buffer_count = 0;
		req->priv = i;
		vpu_push_request_to_queue(user, req);
	}

	for (i = 0; i < 100; i++) {
		if (i == 50 && vpu_flush_requests_from_queue(user))
			LOG_ERR("flush request failed!\n");

		if (vpu_pop_request_from_queue(user, &req)) {
			LOG_ERR("deque request failed. i=%d\n", i);
			break;
		}

		if (req->priv != i) {
			LOG_ERR("out of order of req deque. i=%d, priv=%d\n",
					i, (int) req->priv);
		}
		vpu_free_request(req);
	}

	vpu_delete_user(user);
#endif
	return 0;
}

static int vpu_test_lock(void)
{
	struct vpu_user *user;

	vpu_create_user(&user);

	vpu_hw_lock(user);
	msleep(10 * 1000);
	vpu_hw_unlock(user);

	vpu_delete_user(user);

	return 0;
}

static int vpu_test_set_power(void)
{
	struct vpu_user *user;
#if 0
	struct vpu_power power;
#endif

	vpu_create_user(&user);
#if 0
	/* keep power on for 10s */
	power.mode = VPU_POWER_MODE_ON;
	power.opp = 0;
	vpu_set_power(user, &power);
	msleep(10 * 1000);
#endif
	vpu_delete_user(user);

	return 0;
}


/*
 * 1: boot up
 * 1X: use algo id to load algo
 * 4X: create user X
 * 5X: push a request to user X
 * 6X: pop a request to user X
 * 7X: flush requests of user X
 * 8X: delete user X
 * 9X: delete user X
 * 100: let emulator busy
 * 101: run test case1
 * 102: run test case2
 * 103: run test case3
 */
static int vpu_test_set(void *data, u64 val)
{
	struct vpu_algo *algo;
	struct vpu_request *req;
	/* CHRISTODO */
	int TEMP_CORE = 0;

	LOG_INF("%s:val=%llu\n", __func__, val);

	switch (val) {
	case 0:
		/* do nothing */
		break;
	case 1:
		vpu_boot_up(TEMP_CORE);
		LOG_INF("[vpu_%d] vpu_boot_up\n", TEMP_CORE);
		break;
	case 2:
		vpu_shut_down(TEMP_CORE);
		LOG_INF("[vpu_%d] vpu_shut_down\n", TEMP_CORE);
		break;

	case 10 ... 39: /* use algo's id to load algo */
	{
		vpu_id_t id = (int) val - 10;

		if (vpu_find_algo_by_id(TEMP_CORE, id, &algo)) {
			LOG_DBG("vpu test: algo(%d) is not existed\n", id);
		} else {
			LOG_INF("vpu test: load algo(%d)\n", id);

			if (vpu_hw_load_algo(TEMP_CORE, algo)) {
				LOG_ERR("[vpu_%d] vpu_hw_load_algo failed!\n\n",
						TEMP_CORE);
			}

			LOG_INF("[vpu_%d] vpu_hw_load_algo done\n", TEMP_CORE);
		}

		break;
	}
	case 40 ... 49: /* create user X */
	{
		int index = val - 40;

		if (test_user[index] == NULL)
			vpu_create_user(&test_user[index]);
		break;
	}
	case 50 ... 59: /* push a request to user X */
	{
		int index = val - 50;

		vpu_alloc_request(&req);
		/* req->algo_id = ALGO_OF_MAX_POWER; */
		req->buffer_count = 0;
		vpu_push_request_to_queue(test_user[index], req);
		break;
	}
	case 60 ... 69: /* pop a request to user N */
	{
		int index = val - 60;

		if (vpu_pop_request_from_queue(test_user[index], &req))
			LOG_ERR("vpu test: user(%d) deque failed!\n", index);
		else
			vpu_free_request(req);

		break;
	}
	case 70 ... 79: /* flush requests of user X */
	{
		int index = val - 70;

		vpu_flush_requests_from_queue(test_user[index]);
		break;
	}
	case 80 ... 89: /* delete user X */
	{
		int index = val - 80;

		vpu_delete_user(test_user[index]);
		test_user[index] = NULL;
		break;
	}
	case 90:
		vpu_test_wpp();
		break;
	case 91:
		vpu_test_be_true();
		break;
	case 92:
		vpu_test_lock();
		break;
	case 93:
		vpu_test_set_power();
		break;
	case 100:
		vpu_ext_be_busy();
		break;
	case 101:
	{
		struct task_struct *task;

		task = kthread_create(vpu_user_test_case1, NULL,
						"vpu-test1-thread");
		wake_up_process(task);
		break;
	}
	case 102:
	{
		struct task_struct *task;

		task = kthread_create(vpu_user_test_case2, NULL,
						"vpu-test2-thread");
		wake_up_process(task);
		break;
	}
	case 103:
	{
		struct task_struct *task;

		task = kthread_create(vpu_user_test_case3, NULL,
						"vpu-test3-thread");
		wake_up_process(task);
		break;
	}
	case 111:
		vpu_user_test_case1(NULL);
		break;
	case 112:
		vpu_user_test_case2(NULL);
		break;
	case 113:
		vpu_user_test_case3(NULL);
		break;
	default:
		LOG_INF("%s error,val=%llu\n", __func__, val);
	}

	test_value = val;
	return 0;
}

static int vpu_test_get(void *data, u64 *val)
{
	*val = test_value;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(vpu_debug_test_fops, vpu_test_get,
				vpu_test_set, "%llu\n");
#endif

static int vpu_log_level_set(void *data, u64 val)
{
	g_vpu_log_level = val & 0xf;
	LOG_INF("g_vpu_log_level: %d\n", g_vpu_log_level);

	return 0;
}

static int vpu_log_level_get(void *data, u64 *val)
{
	*val = g_vpu_log_level;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(vpu_debug_log_level_fops, vpu_log_level_get,
				vpu_log_level_set, "%llu\n");

static int vpu_func_mask_set(void *data, u64 val)
{
	g_func_mask = val & 0xffffffff;
	LOG_INF("g_func_mask: 0x%x\n", g_func_mask);

	return 0;
}

static int vpu_func_mask_get(void *data, u64 *val)
{
	*val = g_func_mask;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(vpu_debug_func_mask_fops, vpu_func_mask_get,
				vpu_func_mask_set, "%llu\n");


#define IMPLEMENT_VPU_DEBUGFS(name)					\
static int vpu_debug_## name ##_show(struct seq_file *s, void *unused)\
{					\
	vpu_dump_## name(s);		\
	return 0;			\
}					\
static int vpu_debug_## name ##_open(struct inode *inode, struct file *file) \
{					\
	return single_open(file, vpu_debug_ ## name ## _show, \
				inode->i_private); \
}                                                                             \
static const struct file_operations vpu_debug_ ## name ## _fops = {   \
	.open = vpu_debug_ ## name ## _open,                               \
	.read = seq_read,                                                    \
	.llseek = seq_lseek,                                                \
	.release = seq_release,                                             \
}

/*IMPLEMENT_VPU_DEBUGFS(algo);*/
IMPLEMENT_VPU_DEBUGFS(register);
IMPLEMENT_VPU_DEBUGFS(user);
IMPLEMENT_VPU_DEBUGFS(vpu);
IMPLEMENT_VPU_DEBUGFS(image_file);
IMPLEMENT_VPU_DEBUGFS(mesg);
IMPLEMENT_VPU_DEBUGFS(opp_table);
IMPLEMENT_VPU_DEBUGFS(device_dbg);


#undef IMPLEMENT_VPU_DEBUGFS

static int vpu_debug_power_show(struct seq_file *s, void *unused)
{
	vpu_dump_power(s);
	return 0;
}

static int vpu_debug_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, vpu_debug_power_show, inode->i_private);
}

static ssize_t vpu_debug_power_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *tmp, *token, *cursor;
	int ret, i, param;
	const int max_arg = 5;
	unsigned int args[max_arg];

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		LOG_ERR("copy_from_user failed, ret=%d\n", ret);
		goto out;
	}

	tmp[count] = '\0';

	cursor = tmp;

	/* parse a command */
	token = strsep(&cursor, " ");
	if (strcmp(token, "fix_opp") == 0)
		param = VPU_POWER_PARAM_FIX_OPP;
	else if (strcmp(token, "dvfs_debug") == 0)
		param = VPU_POWER_PARAM_DVFS_DEBUG;
	else if (strcmp(token, "jtag") == 0)
		param = VPU_POWER_PARAM_JTAG;
	else if (strcmp(token, "lock") == 0)
		param = VPU_POWER_PARAM_LOCK;
	else if (strcmp(token, "volt_step") == 0)
		param = VPU_POWER_PARAM_VOLT_STEP;
	else {
		ret = -EINVAL;
		LOG_ERR("no power param[%s]!\n", token);
		goto out;
	}

	/* parse arguments */
	for (i = 0; i < max_arg && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtouint(token, 10, &args[i]);
		if (ret) {
			LOG_ERR("fail to parse args[%d]\n", i);
			goto out;
		}
	}

	vpu_set_power_parameter(param, i, args);

	ret = count;
out:

	kfree(tmp);
	return ret;
}

static const struct file_operations vpu_debug_power_fops = {
	.open = vpu_debug_power_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = vpu_debug_power_write,
};

static int vpu_debug_algo_show(struct seq_file *s, void *unused)
{
	vpu_dump_algo(s);
	return 0;
}

static int vpu_debug_algo_open(struct inode *inode, struct file *file)
{
	return single_open(file, vpu_debug_algo_show, inode->i_private);
}

static ssize_t vpu_debug_algo_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *tmp, *token, *cursor;
	int ret, i, param;
	const int max_arg = 5;
	unsigned int args[max_arg];

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		LOG_ERR("copy_from_user failed, ret=%d\n", ret);
		goto out;
	}

	tmp[count] = '\0';

	cursor = tmp;

	/* parse a command */
	token = strsep(&cursor, " ");
	if (strcmp(token, "dump_algo") == 0)
		param = VPU_DEBUG_ALGO_PARAM_DUMP_ALGO;
	else {
		ret = -EINVAL;
		LOG_ERR("no power param[%s]!\n", token);
		goto out;
	}

	/* parse arguments */
	for (i = 0; i < max_arg && (token = strsep(&cursor, " ")); i++) {
		ret = kstrtouint(token, 10, &args[i]);
		if (ret) {
			LOG_ERR("fail to parse args[%d]\n", i);
			goto out;
		}
	}

	vpu_set_algo_parameter(param, i, args);

	ret = count;
out:

	kfree(tmp);
	return ret;
}

static const struct file_operations vpu_debug_algo_fops = {
	.open = vpu_debug_algo_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = vpu_debug_algo_write,
};

int vpu_init_debug(struct vpu_device *vpu_dev)
{
	int ret;
	struct dentry *debug_file;

	vpu_dev->debug_root = debugfs_create_dir("vpu", NULL);

	ret = IS_ERR_OR_NULL(vpu_dev->debug_root);
	if (ret) {
		LOG_ERR("failed to create debug dir.\n");
		goto out;
	}

#define CREATE_VPU_DEBUGFS(name)                         \
	{                                                           \
		debug_file = debugfs_create_file(#name, 0644, \
				vpu_dev->debug_root,         \
				NULL, &vpu_debug_ ## name ## _fops);       \
		if (IS_ERR_OR_NULL(debug_file))                          \
			LOG_ERR("failed to create debug file[" #name "].\n"); \
	}

	CREATE_VPU_DEBUGFS(algo);
	CREATE_VPU_DEBUGFS(func_mask);
	CREATE_VPU_DEBUGFS(log_level);
	CREATE_VPU_DEBUGFS(register);
	CREATE_VPU_DEBUGFS(user);
	CREATE_VPU_DEBUGFS(image_file);
	CREATE_VPU_DEBUGFS(mesg);
	CREATE_VPU_DEBUGFS(vpu);
	CREATE_VPU_DEBUGFS(opp_table);
	CREATE_VPU_DEBUGFS(power);
	CREATE_VPU_DEBUGFS(device_dbg);

#ifdef MTK_VPU_DVT
	CREATE_VPU_DEBUGFS(test);
#endif

#undef CREATE_VPU_DEBUGFS

out:
	return ret;
}

