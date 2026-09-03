// SPDX-License-Identifier: GPL-2.0
//
// UNISOC APCPU POWER STAT driver
//
// Copyright (C) 2020 Unisoc, Inc.

#include <linux/device.h>
#include <linux/io.h>
#include <linux/iomap.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/seq_file.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/soc/sprd/sprd_systimer.h>
#include "sprd_pdbg_comm.h"
#include "sprd_slp_info.h"

enum {
	STAGE_SLP_ENTER,
	STAGE_SLP_EXIT,
	STAGE_INFO_GET,
};

enum {
	INFO_SLP_CNT,
	INFO_SLP_STAT,
	INFO_SLP_TIME,
};

#define INFO_LEN (512)
#define TO_MASK(x) BIT(x)
#define MASK_AP_SOC (TO_MASK(SYS_AP) | TO_MASK(SYS_SOC))
#define TYPE_ALL (0xffffffff)
#define HWLOCK_TIMEOUT (5000)
#define SLP_INFO_SIZE (300)

static void slp_info_update_ap(struct subsys_slp_info_data *subsys_data, u32 stage);
static void slp_info_update_soc(struct subsys_slp_info_data *subsys_data, u32 stage);
static void slp_info_update_shmem(struct subsys_slp_info_data *subsys_data, u32 stage);
static void slp_info_update_shmem_ext(struct subsys_slp_info_data *subsys_data, u32 stage);

static inline int slp_hwspin_lock(struct hwspinlock *hwlock)
{
	int ret;

	if (!hwlock)
		return 0;

	ret = hwspin_lock_timeout_raw(hwlock, HWLOCK_TIMEOUT);
	if (ret)
		return ret;

	return 0;
}

static inline void slp_hwspin_unlock(struct hwspinlock *hwlock)
{
	if (!hwlock)
		return;

	hwspin_unlock_raw(hwlock);
}

static struct subsys_data_var subsys_data_vars[] = {
	[SYS_AP]    = {"AP",    slp_info_update_ap,    NULL},
	[SYS_SOC]   = {"SOC",   slp_info_update_soc,   NULL},
	[SYS_PHYCP] = {"PHYCP", slp_info_update_shmem, NULL},
	[SYS_PSCP]  = {"PSCP",  slp_info_update_shmem, NULL},
	[SYS_PUBCP] = {"PUBCP", slp_info_update_shmem, NULL},
	[SYS_WTLCP] = {"WTLCP", slp_info_update_shmem, NULL},
	[SYS_WCN_BTWF] = {"WCN_BTWF", slp_info_update_shmem, slp_info_update_shmem_ext},
	[SYS_WCN_GNSS] = {"WCN_GNSS", slp_info_update_shmem, slp_info_update_shmem_ext},
};

static u32 slp_info_reg_read(struct subsys_slp_info_data *subsys_data, u32 info_type)
{
	u32 out_val = 0;
	int ret;
	struct slp_info_reg *info_reg;

	switch (info_type) {
	case INFO_SLP_CNT:
		info_reg = &subsys_data->slp_cnt;
		break;
	case INFO_SLP_STAT:
		info_reg = &subsys_data->slp_state;
		break;
	case INFO_SLP_TIME:
		info_reg = &subsys_data->slp_time;
		break;
	default:
		return 0;
	}

	ret = regmap_read(info_reg->map, info_reg->reg, &out_val);
	if (ret) {
		SPRD_PDBG_ERR("%s err: reg[%d], ret %d\n", __func__, info_reg->reg, ret);
		return ret;
	}

	out_val = ((out_val >> info_reg->offset) & info_reg->mask);

	return out_val;
}

static void slp_info_caculate(struct subsys_slp_info_data *subsys_data, u32 info_type, u32 cur_val)
{
	struct subsys_slp_info *slp_info = subsys_data->slp_info;
	u32 last_val, mask, delta;
	u64 *cal_val;

	switch (info_type) {
	case INFO_SLP_CNT:
		last_val = subsys_data->slp_cnt.last_val;
		mask = subsys_data->slp_cnt.mask;
		cal_val = &slp_info->total_slp_cnt;
		break;
	case INFO_SLP_TIME:
		last_val = subsys_data->slp_time.last_val;
		mask = subsys_data->slp_time.mask;
		cal_val = &slp_info->total_slp_time;
		break;
	default:
		return;
	}

	delta = ((cur_val >= last_val) ? (cur_val - last_val) : (mask - last_val + cur_val));
	*cal_val += delta;
}

static void slp_info_update_ap(struct subsys_slp_info_data *subsys_data, u32 stage)
{
	u64 slp_time;
	u32 cur_slp_cnt;
	struct subsys_slp_info *slp_info = NULL;
	struct slp_info_reg *slp_cnt = NULL;

	if (!subsys_data || !subsys_data->slp_cnt.map)
		return;

	if (stage > STAGE_INFO_GET)
		return;

	slp_info = subsys_data->slp_info;
	slp_cnt = &subsys_data->slp_cnt;
	switch (stage) {
	case STAGE_SLP_ENTER:
		slp_cnt->last_val = slp_info_reg_read(subsys_data, INFO_SLP_CNT);
		slp_info->last_enter_time = sprd_sysfrt_read();
		break;
	case STAGE_SLP_EXIT:
		cur_slp_cnt = slp_info_reg_read(subsys_data, INFO_SLP_CNT);
		slp_info->last_exit_time = sprd_sysfrt_read();
		slp_time = slp_info->last_exit_time - slp_info->last_enter_time;
		if (cur_slp_cnt != slp_cnt->last_val)
			slp_info->total_slp_time += slp_time;
		slp_info_caculate(subsys_data, INFO_SLP_CNT, cur_slp_cnt);
		break;
	case STAGE_INFO_GET:
		slp_info->cur_slp_state = 0;
		slp_info->total_time = sprd_sysfrt_read();
		subsys_data->slp_info_get = *slp_info;
		break;
	default:
		break;
	}
}

static void slp_info_update_soc(struct subsys_slp_info_data *subsys_data, u32 stage)
{
	u32 cur_slp_time, cur_slp_cnt;
	struct subsys_slp_info *slp_info = NULL;
	struct slp_info_reg *slp_time = NULL, *slp_cnt = NULL;

	if (!subsys_data || !subsys_data->slp_time.map || !subsys_data->slp_cnt.map)
		return;

	if (stage > STAGE_INFO_GET)
		return;

	slp_info = subsys_data->slp_info;
	slp_time = &subsys_data->slp_time;
	slp_cnt = &subsys_data->slp_cnt;
	switch (stage) {
	case STAGE_SLP_ENTER:
		slp_time->last_val = slp_info_reg_read(subsys_data, INFO_SLP_TIME);
		slp_cnt->last_val = slp_info_reg_read(subsys_data, INFO_SLP_CNT);
		break;
	case STAGE_SLP_EXIT:
		cur_slp_time = slp_info_reg_read(subsys_data, INFO_SLP_TIME);
		cur_slp_cnt = slp_info_reg_read(subsys_data, INFO_SLP_CNT);
		slp_info_caculate(subsys_data, INFO_SLP_TIME, cur_slp_time);
		slp_info_caculate(subsys_data, INFO_SLP_CNT, cur_slp_cnt);

		break;
	case STAGE_INFO_GET:
		slp_info->last_enter_time = 0;/* not support */
		slp_info->last_exit_time = 0;/* not support */
		slp_info->cur_slp_state = 0;
		slp_info->total_time = sprd_sysfrt_read();
		subsys_data->slp_info_get = *slp_info;
		break;
	default:
		break;
	}
}

static void slp_total_time_compensate(struct subsys_slp_info *slp_info, u32 cur_state)
{
	u64 delta = 0;

	if (!cur_state)
		return;

	/* not enter sleep yet, no need to compensate */
	if (slp_info->last_enter_time == 0)
		return;

	if (slp_info->total_time < slp_info->last_enter_time) {
		SPRD_PDBG_ERR("time_compensate err: total %llu, enter %llu\n",
			slp_info->total_time, slp_info->last_enter_time);
		return;
	}

	delta = slp_info->total_time - slp_info->last_enter_time;
	slp_info->total_slp_time += delta;
}

static void slp_info_update_shmem(struct subsys_slp_info_data *subsys_data, u32 stage)
{
	int ret = -1;
	struct subsys_slp_info *slp_info_get = NULL;

	if (!subsys_data || !subsys_data->slp_state.map)
		return;

	if (stage != STAGE_INFO_GET)
		return;

	slp_info_get = &subsys_data->slp_info_get;

	ret = slp_hwspin_lock(subsys_data->lock->hwlock);
	if (ret) {
		SPRD_PDBG_ERR("timeout to get the hwspinlock\n");
		return;
	}

	*slp_info_get = *(subsys_data->slp_info);

	slp_hwspin_unlock(subsys_data->lock->hwlock);

	slp_info_get->cur_slp_state = slp_info_reg_read(subsys_data, INFO_SLP_STAT);
	slp_info_get->total_time = sprd_sysfrt_read();
	slp_total_time_compensate(slp_info_get, slp_info_get->cur_slp_state);
}

static void slp_info_update_shmem_ext(struct subsys_slp_info_data *subsys_data, u32 stage)
{
	if (!subsys_data || !subsys_data->slp_info)
		return;

	if (stage != STAGE_INFO_GET)
		return;

	pdbg_notifier_call_chain(subsys_data->index, subsys_data->slp_info);

	subsys_data->slp_info_get = *(subsys_data->slp_info);
}

static int sprd_slp_info_dt_parse(struct device *dev, struct slp_info_data *slp_data)
{
	int i = 0, ret;
	unsigned int args[3];
	u32 index, subsys_data_vars_size = ARRAY_SIZE(subsys_data_vars);
	struct device_node *np_child;
	struct subsys_slp_info_data *subsys_data;

	slp_data->subsys_datas_cnt = of_get_child_count(dev->of_node);
	if (slp_data->subsys_datas_cnt <= 0) {
		SPRD_PDBG_ERR("%s: subsys_datas_cnt error\n", __func__);
		return -ENXIO;
	}

	slp_data->subsys_datas = devm_kcalloc(dev, slp_data->subsys_datas_cnt,
				       sizeof(struct subsys_slp_info_data), GFP_KERNEL);
	if (!slp_data->subsys_datas) {
		SPRD_PDBG_ERR("%s: subsys_slp_info_data alloc error\n", __func__);
		return -ENOMEM;
	}

	for_each_child_of_node(dev->of_node, np_child) {
		subsys_data = &slp_data->subsys_datas[i++];

		ret = of_property_read_u32(np_child, "sprd,subsys-index", &index);
		if (ret) {
			SPRD_PDBG_ERR("Fail to find type property\n");
			return -ENXIO;
		}

		if (index >= subsys_data_vars_size) {
			SPRD_PDBG_ERR("index cfg error\n");
			return -ENXIO;
		}
		subsys_data->index = index;
		subsys_data->info_update = subsys_data_vars[index].info_update;
		subsys_data->info_update_ext = subsys_data_vars[index].info_update_ext;
		subsys_data->update_ext =
			of_property_read_bool(np_child, "sprd,subsys-update-ext");
		subsys_data->lock = &slp_data->lock;

		subsys_data->slp_cnt.map =
			syscon_regmap_lookup_by_phandle_args(np_child, "sprd,subsys-slp-cnt",
							     3, args);
		if (!IS_ERR_OR_NULL(subsys_data->slp_cnt.map)) {
			subsys_data->slp_cnt.reg = args[0];
			subsys_data->slp_cnt.offset = args[1];
			subsys_data->slp_cnt.mask = args[2];
		}

		subsys_data->slp_state.map =
			syscon_regmap_lookup_by_phandle_args(np_child, "sprd,subsys-slp-state",
							     3, args);
		if (!IS_ERR_OR_NULL(subsys_data->slp_state.map)) {
			subsys_data->slp_state.reg = args[0];
			subsys_data->slp_state.offset = args[1];
			subsys_data->slp_state.mask = args[2];
		}

		subsys_data->slp_time.map =
			syscon_regmap_lookup_by_phandle_args(np_child, "sprd,subsys-slp-time",
							     3, args);
		if (!IS_ERR_OR_NULL(subsys_data->slp_time.map)) {
			subsys_data->slp_time.reg = args[0];
			subsys_data->slp_time.offset = args[1];
			subsys_data->slp_time.mask = args[2];
		}
	}

	return 0;
}

static void do_slp_info_update(struct subsys_slp_info_data *subsys_data, u32 stage)
{
	if (subsys_data->update_ext && subsys_data->info_update_ext)
		subsys_data->info_update_ext(subsys_data, stage);
	else if (!subsys_data->update_ext && subsys_data->info_update)
		subsys_data->info_update(subsys_data, stage);
}

static void slp_info_update(struct slp_info_data *slp_data, u32 type_mask, u32 stage)
{
	struct subsys_slp_info_data *subsys_data;
	u32 i;

	for (i = 0; i < slp_data->subsys_datas_cnt; i++) {
		subsys_data = &slp_data->subsys_datas[i];

		if (!subsys_data->slp_info)
			continue;

		if (!(type_mask & TO_MASK(subsys_data->index)))
			continue;

		do_slp_info_update(subsys_data, stage);

		if (type_mask == TO_MASK(subsys_data->index))
			break;
	}
}

static int slp_info_show(struct seq_file *seq, void *offset)
{
	struct subsys_slp_info_data *subsys_data = seq->private;
	struct subsys_slp_info *info = &subsys_data->slp_info_get;
	char str[INFO_LEN];
	u32 num = 0, i;
	const char *name;

	if (!subsys_data->slp_info)
		return 0;

	mutex_lock(&subsys_data->lock->mtx);

	do_slp_info_update(subsys_data, STAGE_INFO_GET);

	name = subsys_data_vars[subsys_data->index].name;
	num += scnprintf(str + num, INFO_LEN - num, "%20s:", "subsystem_name(%s)");
	num += scnprintf(str + num, INFO_LEN - num, "%20s\n", name);

	num += scnprintf(str + num, INFO_LEN - num, "%20s:", "total_time(%x)");
	num += scnprintf(str + num, INFO_LEN - num, "%20llx\n", info->total_time);

	num += scnprintf(str + num, INFO_LEN - num, "%20s:", "total_slp_time(%x)");
	num += scnprintf(str + num, INFO_LEN - num, "%20llx\n", info->total_slp_time);

	num += scnprintf(str + num, INFO_LEN - num, "%20s:", "last_enter_time(%x)");
	num += scnprintf(str + num, INFO_LEN - num, "%20llx\n", info->last_enter_time);

	num += scnprintf(str + num, INFO_LEN - num, "%20s:", "last_exit_time(%x)");
	num += scnprintf(str + num, INFO_LEN - num, "%20llx\n", info->last_exit_time);

	num += scnprintf(str + num, INFO_LEN - num, "%20s:", "total_slp_cnt(%x)");
	num += scnprintf(str + num, INFO_LEN - num, "%20llx\n", info->total_slp_cnt);

	num += scnprintf(str + num, INFO_LEN - num, "%20s:", "cur_slp_state(%x)");
	num += scnprintf(str + num, INFO_LEN - num, "%20x\n", info->cur_slp_state);

	num += scnprintf(str + num, INFO_LEN - num, "%20s:", "boot_cnt(%x)");
	num += scnprintf(str + num, INFO_LEN - num, "%20x\n", info->boot_cnt);

	num += scnprintf(str + num, INFO_LEN - num, "%20s:", "last_ws(%x)");
	num += scnprintf(str + num, INFO_LEN - num, "%10x\n", info->last_ws);

	num += scnprintf(str + num, INFO_LEN - num, "%20s:", "ws_cnt(%x)");
	for (i = 0; i < TOP_IRQ_MAX - 1; i++)
		num += scnprintf(str + num, INFO_LEN - num, "%10x ", info->ws_cnt[i]);
	num += scnprintf(str + num, INFO_LEN - num, "%10x\n", info->ws_cnt[i]);

	mutex_unlock(&subsys_data->lock->mtx);

	seq_printf(seq, "%s\n", str);

	return 0;
}

static int sprd_slp_info_proc_init(struct slp_info_data *slp_data, struct proc_dir_entry *parent)
{
	struct proc_dir_entry *dir;
	struct subsys_slp_info_data *subsys_data;
	const char *name;
	int i;

	dir = proc_mkdir("slp_info", parent);
	if (!dir) {
		SPRD_PDBG_ERR("Proc slp_info dir create failed\n");
		return -EBADF;
	}

	for (i = 0; i < slp_data->subsys_datas_cnt; i++) {
		subsys_data = &slp_data->subsys_datas[i];
		name = subsys_data_vars[subsys_data->index].name;
		if (!proc_create_single_data(name, 0644, dir, slp_info_show, subsys_data)) {
			SPRD_PDBG_ERR("Proc file %s create failed\n", name);
			return -ENODEV;
		}
	}

	return 0;
}

static void slp_info_notify_handler(void *data, unsigned long cmd)
{
	struct slp_info_data *slp_data = data;

	switch (cmd) {
	case SPRD_CPU_PM_ENTER:
		slp_info_update(slp_data, MASK_AP_SOC, STAGE_SLP_ENTER);
		break;
	case SPRD_CPU_PM_EXIT:
		slp_info_update(slp_data, MASK_AP_SOC, STAGE_SLP_EXIT);
		break;
	default:
		break;
	}
}

static void lock_devm_action(void *_data)
{
	struct slp_info_data *slp_data = _data;

	mutex_destroy(&slp_data->lock.mtx);
}

static int sprd_slp_info_lock_init(struct device *dev, struct slp_info_data *data)
{
	int id;

	mutex_init(&data->lock.mtx);

	id = of_hwspin_lock_get_id(dev->of_node, 0);
	if (id < 0) {
		SPRD_PDBG_INFO("hwlock not support in remote side\n");
		return 0;
	}

	data->lock.hwlock = devm_hwspin_lock_request_specific(dev, id);
	if (!data->lock.hwlock) {
		SPRD_PDBG_ERR("failed to request hwlock\n");
		return -ENXIO;
	}

	return 0;
}

static void rmem_devm_action(void *_data)
{
	int i;
	struct slp_info_data *slp_data = _data;
	struct subsys_slp_info_data *subsys_data;

	memunmap(slp_data->virt_base);
	for (i = 0; i < slp_data->subsys_datas_cnt; i++) {
		subsys_data = &slp_data->subsys_datas[i];
		subsys_data->slp_info = NULL;
	}
}

static int sprd_slp_info_rmem_init(struct device *dev, struct slp_info_data *slp_data)
{
	struct device_node *np;
	struct resource r;
	resource_size_t size, size_target = 0;
	int ret, i;
	struct subsys_slp_info_data *subsys_data;
	char *virt_info;

	np = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!np) {
		SPRD_PDBG_ERR("No memory-region specified\n");
		return -EINVAL;
	}

	ret = of_address_to_resource(np, 0, &r);
	of_node_put(np);
	if (ret) {
		SPRD_PDBG_ERR("of_address_to_resource fail\n");
		return ret;
	}

	size = resource_size(&r);
	size_target = SLP_INFO_SIZE * SYS_MAX;
	if (size_target > size) {
		SPRD_PDBG_ERR("rmem_init size error\n");
		return -ENOMEM;
	}

	slp_data->virt_base = memremap(r.start, size, MEMREMAP_WB);
	if (!slp_data->virt_base) {
		SPRD_PDBG_ERR("memremap fail\n");
		return -ENOMEM;
	}

	for (i = 0; i < slp_data->subsys_datas_cnt; i++) {
		subsys_data = &slp_data->subsys_datas[i];
		virt_info = ((char *)slp_data->virt_base + SLP_INFO_SIZE * subsys_data->index);
		subsys_data->slp_info = (struct subsys_slp_info *)virt_info;
		memset(subsys_data->slp_info, 0, sizeof(struct subsys_slp_info));
	}

	ret = devm_add_action(dev, rmem_devm_action, slp_data);
	if (ret) {
		rmem_devm_action(slp_data);
		SPRD_PDBG_ERR("failed to add rmem_devm_action\n");
		return ret;
	}

	return 0;
}

int sprd_slp_info_init(struct device *dev, struct proc_dir_entry *dir, struct slp_info_data **data)
{
	int ret;
	struct slp_info_data *slp_data;

	if (sprd_sysfrt_read() == 0) {
		SPRD_PDBG_ERR("%s: sysfrt not ready, need check\n", __func__);
		return -ENOENT;
	}

	slp_data = devm_kzalloc(dev, sizeof(struct slp_info_data), GFP_KERNEL);
	if (!slp_data) {
		SPRD_PDBG_ERR("%s: slp_info alloc error\n", __func__);
		return -ENOMEM;
	}

	slp_data->notify_cb = slp_info_notify_handler;

	ret = sprd_slp_info_lock_init(dev, slp_data);
	if (ret) {
		SPRD_PDBG_ERR("%s: sprd_slp_info_lock_init error\n", __func__);
		return ret;
	}

	ret = devm_add_action(dev, lock_devm_action, slp_data);
	if (ret) {
		lock_devm_action(slp_data);
		SPRD_PDBG_ERR("failed to add slp_info devm action\n");
		return ret;
	}

	ret = sprd_slp_info_dt_parse(dev, slp_data);
	if (ret) {
		SPRD_PDBG_ERR("%s: sprd_slp_info_dt_parse error\n", __func__);
		return ret;
	}

	ret = sprd_slp_info_rmem_init(dev, slp_data);
	if (ret) {
		SPRD_PDBG_ERR("%s: sprd_slp_info_rmem_init error\n", __func__);
		return ret;
	}

	ret = sprd_slp_info_proc_init(slp_data, dir);
	if (ret) {
		SPRD_PDBG_ERR("%s: sprd_slp_info_proc_init error\n", __func__);
		return ret;
	}

	*data = slp_data;

	return 0;
}
