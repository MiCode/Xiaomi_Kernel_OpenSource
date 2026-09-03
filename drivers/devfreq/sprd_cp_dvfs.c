// SPDX-License-Identifier: GPL-2.0
//
// Spreadtrum cp dvfs driver
//
// Copyright (C) 2015~2020 Spreadtrum, Inc.
// Author: Bao Yue <bao.yue@unisoc.com>

#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include "sprd_cp_dvfs.h"


/* pubcp and wtlcp use the same sipc sbuf, the variable ensure to create sipc only once */
static u32 sbuf_dst = 0xFF;

/* use globle mutex to exclusive two cp operation */
static DEFINE_MUTEX(cp_dvfs_lock);
static int cp_dvfs_send_data(struct cpdvfs_data *cpdvfs, u8 *buf, u32 len)
{
	int nwrite, dbg_i;
	int sent_len = 0, timeout = 100, retry = 0;

	dev_dbg(cpdvfs->dev, "sipc dst=%d\n", cpdvfs->dst);
	for (dbg_i = 0; dbg_i < len; dbg_i++)
		dev_dbg(cpdvfs->dev, "buf[%d]=0x%02x\n", dbg_i, buf[dbg_i]);
	do {
		nwrite =
			sbuf_write(cpdvfs->dst, SMSG_CH_DVFS, CP_DVFS_SBUFID,
				(void *)(buf + sent_len), len - sent_len,
				msecs_to_jiffies(timeout));
		if (nwrite > 0)
			sent_len += nwrite;
		if (nwrite < len)
			dev_err(cpdvfs->dev, "nwrite=%d,len=%d,sent_len=%d,timeout=%dms\n",
				nwrite, len, sent_len, timeout);

		/* only handle boot exception */
		if (nwrite < 0) {
			if (nwrite == -ENODEV) {
				msleep(SBUF_TRY_WAIT_MS);
				if (++retry > SBUF_TRY_WAIT_TIEMS)
					break;
			} else {
				dev_dbg(cpdvfs->dev, "nwrite=%d\n", nwrite);
				dev_dbg(cpdvfs->dev, "task #: %s, pid = %d, tgid = %d\n",
					current->comm, current->pid,
					current->tgid);
				WARN_ONCE(1, "%s timeout: %dms\n",
						__func__, timeout);
				break;
			}
		}
	} while (sent_len < len);

	return nwrite;
}

/**
 * This function send sent_cmd data to CP
 * this interface is conciser for highest layer SYSFS
 * @return 0 : success
 */
static int cp_dvfs_send_cmd(struct cpdvfs_data *cpdvfs)
{
	int nwrite;

	dev_dbg(cpdvfs->dev, "cmd_len=%d\n", cpdvfs->cmd_len);
	if (cpdvfs->cmd_len > 0) {
		dev_dbg(cpdvfs->dev, "core or dev id=%d, cmd=0x%x\n",
			cpdvfs->sent_cmd->id, cpdvfs->sent_cmd->cmd);
		nwrite = cp_dvfs_send_data(cpdvfs, (u8 *)cpdvfs->sent_cmd,
					   cpdvfs->cmd_len);
		if (nwrite > 0)
			return 0;
		else {
			dev_err(cpdvfs->dev, "send failed,nwrite=%d\n", nwrite);
			return nwrite;
		}
	} else {
		dev_err(cpdvfs->dev, "no data need to send\n");
		return -EINVAL;
	}
}

static int cp_dvfs_send_cmd_nopara(struct cpdvfs_data *cpdvfs,
				enum dvfs_cmd_type cmd)
{
	struct cmd_pkt *sent_cmd = cpdvfs->sent_cmd;

	/* build this cmd */
	/* pubcp&wtlcp send core id */
	if (cpdvfs->core_id < DVFS_CORE_PSCP || cmd == DVFS_GET_AUTO)
		sent_cmd->id = cpdvfs->core_id;
	else /* pscp&phycp send device id */
		sent_cmd->id = cpdvfs->user_data->curr_dev_id;
	sent_cmd->cmd = cmd;
	cpdvfs->cmd_len = sizeof(struct cmd_pkt);

	/* send this cmd */
	return cp_dvfs_send_cmd(cpdvfs);
}

static int cp_dvfs_send_cmd_onepara(struct cpdvfs_data *cpdvfs,
				    enum dvfs_cmd_type cmd, u8 para)
{
	/* build this cmd */
	if (cpdvfs->core_id < DVFS_CORE_PSCP || cmd == DVFS_SET_AUTO)
		cpdvfs->sent_cmd->id = cpdvfs->core_id;
	else
		cpdvfs->sent_cmd->id = cpdvfs->user_data->curr_dev_id;
	cpdvfs->sent_cmd->cmd = cmd;
	cpdvfs->sent_cmd->para[0] = para;
	/* include cmd field size */
	cpdvfs->cmd_len = 3;

	return cp_dvfs_send_cmd(cpdvfs);
}

static int cp_dvfs_send_setreg(struct cpdvfs_data *cpdvfs,
		struct reg_t *reg)
{
	cpdvfs->sent_cmd->id = cpdvfs->core_id;
	cpdvfs->sent_cmd->cmd = DVFS_SET_REG;

	memcpy(cpdvfs->sent_cmd->para, reg, sizeof(struct reg_t));
	/* include cmd field size */
	cpdvfs->cmd_len = sizeof(struct cmd_pkt) + sizeof(struct reg_t);

	dev_dbg(cpdvfs->dev, "para[0]=0x%02x\n", cpdvfs->sent_cmd->para[0]);
	dev_dbg(cpdvfs->dev, "para[1]=0x%02x\n", cpdvfs->sent_cmd->para[1]);
	dev_dbg(cpdvfs->dev, "para[2]=0x%02x\n", cpdvfs->sent_cmd->para[2]);
	dev_dbg(cpdvfs->dev, "para[3]=0x%02x\n", cpdvfs->sent_cmd->para[3]);
	dev_dbg(cpdvfs->dev, "para[4]=0x%02x\n", cpdvfs->sent_cmd->para[4]);
	dev_dbg(cpdvfs->dev, "para[5]=0x%02x\n", cpdvfs->sent_cmd->para[5]);
	dev_dbg(cpdvfs->dev, "para[6]=0x%02x\n", cpdvfs->sent_cmd->para[6]);
	dev_dbg(cpdvfs->dev, "para[7]=0x%02x\n", cpdvfs->sent_cmd->para[7]);

	return cp_dvfs_send_cmd(cpdvfs);
}

static int cp_dvfs_send_inqreg(struct cpdvfs_data *cpdvfs, u32 *reg_addr)
{
	/* all CPs send core id not dev id */
	cpdvfs->sent_cmd->id = cpdvfs->core_id;
	cpdvfs->sent_cmd->cmd = DVFS_GET_REG;
	memcpy(cpdvfs->sent_cmd->para, reg_addr, sizeof(*reg_addr));
	/* include cmd field size */
	cpdvfs->cmd_len = sizeof(struct cmd_pkt) + sizeof(*reg_addr);

	dev_dbg(cpdvfs->dev, "para[0]=0x%02x\n", cpdvfs->sent_cmd->para[0]);
	dev_dbg(cpdvfs->dev, "para[1]=0x%02x\n", cpdvfs->sent_cmd->para[1]);
	dev_dbg(cpdvfs->dev, "para[2]=0x%02x\n", cpdvfs->sent_cmd->para[2]);
	dev_dbg(cpdvfs->dev, "para[3]=0x%02x\n", cpdvfs->sent_cmd->para[3]);

	return cp_dvfs_send_cmd(cpdvfs);
}

static int cp_dvfs_recv(struct cpdvfs_data *cpdvfs, int timeout)
{
	int nread, retry = 0;

	dev_dbg(cpdvfs->dev, "sipc dst=%d\n", cpdvfs->dst);
	do {
		nread =
			sbuf_read(cpdvfs->dst, SMSG_CH_DVFS, CP_DVFS_SBUFID,
				 (void *)cpdvfs->rd_buf, CP_DVFS_RXBUFSIZE,
				 msecs_to_jiffies(timeout));
		if (nread < 0) {
			msleep(SBUF_TRY_WAIT_MS);
			if (++retry > SBUF_TRY_WAIT_TIEMS)
				break;
			dev_info(cpdvfs->dev, "nread=%d,retry=%d\n",
				 nread, retry);
		}
	} while (nread < 0);

	return nread;
}

static int cp_dvfs_recv_cmd_onepara(
		struct cpdvfs_data *cpdvfs, enum dvfs_cmd_type recv_cmd)
{
	u8 core_id_ack, cmd_ack;
	int read_len, para0 = -EINVAL;

	read_len = cp_dvfs_recv(cpdvfs, SBUF_RD_TIMEOUT_MS);
	if (read_len > 0) {
		core_id_ack = cpdvfs->rd_buf[0];
		cmd_ack = cpdvfs->rd_buf[1];

		dev_dbg(cpdvfs->dev, "sent core or dev_id=%d,  cmd=%d\n",
			cpdvfs->sent_cmd->id, cpdvfs->sent_cmd->cmd);
		dev_dbg(cpdvfs->dev, "recv_cmd=%d\n", recv_cmd);
		dev_dbg(cpdvfs->dev, "core_id_ack=%d,  cmd_ack=%d\n",
			core_id_ack, cmd_ack);
		/* received data is response of corresponding cmd, parse it.
		 * pubcp & wtlcp core id is real core id,
		 * but pscp & pycp core_id_ack is dev id.
		 */
		if (cmd_ack == recv_cmd) {
			para0 = cpdvfs->rd_buf[2];
			dev_dbg(cpdvfs->dev, "get resp cmd para0=%d\n", para0);
		} else {
			dev_err(cpdvfs->dev, "this received is not responsed cmd\n");
		}
	} else {
		dev_err(cpdvfs->dev, "read_len=%d\n", read_len);
	}

	return  para0;
}

static int cp_dvfs_recv_reg_val(struct cpdvfs_data *cpdvfs)
{
	int read_len;
	struct cmd_pkt *recv_pkt;

	read_len = cp_dvfs_recv(cpdvfs, SBUF_RD_TIMEOUT_MS);
	if (read_len > 0) {
		recv_pkt = (struct cmd_pkt *)cpdvfs->rd_buf;
		dev_dbg(cpdvfs->dev, "sent id=%d,cmd=%d\n",
			cpdvfs->sent_cmd->id, cpdvfs->sent_cmd->cmd);
		dev_dbg(cpdvfs->dev, "core_id_ack=%d,cmd_ack=%d\n",
			recv_pkt->id, recv_pkt->cmd);
		/* received data is response of corresponding cmd, parse it */
		if (recv_pkt->cmd == DVFS_GET_REG) {
			memcpy(&cpdvfs->user_data->inq_reg,  recv_pkt->para,
			       sizeof(struct reg_t));
			dev_dbg(cpdvfs->dev, "get reg_addr=0x%08x\n",
				cpdvfs->user_data->inq_reg.reg_addr);
			dev_dbg(cpdvfs->dev, "get reg_value=0x%08x\n",
				cpdvfs->user_data->inq_reg.reg_val);
			return  cpdvfs->user_data->inq_reg.reg_val;
		}
	}
	return -EIO;
}

/* attributes functions begin */
static ssize_t name_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct cpdvfs_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", data->name);
}
static DEVICE_ATTR_RO(name);

static ssize_t core_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cpdvfs_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->core_id);
}
static DEVICE_ATTR_RO(core_id);

static ssize_t dev_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	return
		snprintf(buf, 512, "name:%s\ncore_id:%d\nrecord_num:%d\n",
			cpdvfs->name, cpdvfs->core_id, cpdvfs->record_num);
}
static DEVICE_ATTR_RO(dev_info);

static ssize_t index_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	int index = -1, i, ret;
	u8 dev_id = DVFS_DEVICE_MAX;
	/* 1. get core id: pscp or phycp */
	struct device *dev = kobj_to_dev(kobj->parent);
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	mutex_lock(&cp_dvfs_lock);
	/* 2. get device id: core, axi, or apb */
	for (i = 0; i < cpdvfs->devices_num; i++) {
		 if (!strcmp(kobj->name, cpdvfs->user_data->devices[i].name)) {
		    dev_id = cpdvfs->user_data->devices[i].dev_id;
		    cpdvfs->user_data->curr_dev_id = cpdvfs->user_data->devices[i].dev_id;
		 }
	}
	dev_info(dev, "core_id=%d, device_id=%d\n", cpdvfs->core_id, dev_id);
	ret = cp_dvfs_send_cmd_nopara(cpdvfs, DVFS_GET_INDEX);
	if (ret == 0) {
		index = cp_dvfs_recv_cmd_onepara(cpdvfs, DVFS_GET_INDEX);
		cpdvfs->user_data->devices[cpdvfs->user_data->curr_dev_id].index = index;
	} else {
		dev_err(dev, "send command fail\n");
	}
	mutex_unlock(&cp_dvfs_lock);

	return sprintf(buf, "%d\n", index);
}

static ssize_t index_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	u8 index;
	int ret;
	u32 idx;
	u8 dev_id = DVFS_DEVICE_MAX;
	/* 1. get core id: pscp or phycp */
	struct device *dev = kobj_to_dev(kobj->parent);
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);


	mutex_lock(&cp_dvfs_lock);
	/* 2. get device id: core, axi or apb */
	for (idx = 0; idx < cpdvfs->devices_num; idx++) {
		 if (!strcmp(kobj->name, cpdvfs->user_data->devices[idx].name)) {
		    dev_id = cpdvfs->user_data->devices[idx].dev_id;
		    cpdvfs->user_data->curr_dev_id = cpdvfs->user_data->devices[idx].dev_id;
		 }
	}
	dev_dbg(dev, "buf=%s\n", buf);
	dev_info(dev, "core_id=%d, device_id=%d\n", cpdvfs->core_id, dev_id);
	if (sscanf(buf, "%hhu\n", &index) != 1)
		return -EINVAL;
	if (index >= DVFS_INDEX_MAX) {
		dev_err(dev, "input %d, pls input right index [0,7]\n", index);
		return -EINVAL;
	}
	ret = cp_dvfs_send_cmd_onepara(cpdvfs, DVFS_SET_INDEX, index);
	if (ret != 0)
		dev_err(dev, "send command fail\n");
	mutex_unlock(&cp_dvfs_lock);

	return count;
}

static ssize_t idle_index_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	int idle_index = -1, ret;
	u32 idx = 0;
	u8 dev_id = DVFS_DEVICE_MAX;
	/* 1. get core id: pscp or phycp */
	struct device *dev = kobj_to_dev(kobj->parent);
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);


	mutex_lock(&cp_dvfs_lock);
	/* 2. get device id: core, axi, apb */
	for (idx = 0; idx < cpdvfs->devices_num; idx++) {
		 if (!strcmp(kobj->name, cpdvfs->user_data->devices[idx].name)) {
		    dev_id = cpdvfs->user_data->devices[idx].dev_id;
		    cpdvfs->user_data->curr_dev_id = cpdvfs->user_data->devices[idx].dev_id;
		 }
	}
	dev_info(dev, "core_id=%d, device_id=%d\n", cpdvfs->core_id, dev_id);
	ret = cp_dvfs_send_cmd_nopara(cpdvfs, DVFS_GET_IDLE_INDEX);
	if (ret == 0) {
		idle_index = cp_dvfs_recv_cmd_onepara(cpdvfs, DVFS_GET_IDLE_INDEX);
		cpdvfs->user_data->devices[cpdvfs->user_data->curr_dev_id].idle_index = idle_index;
	} else {
		dev_err(dev, "send command fail\n");
	}
	mutex_unlock(&cp_dvfs_lock);

	return sprintf(buf, "%d\n", idle_index);
}

static ssize_t idle_index_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	u8 idle_index;
	int ret;
	u32 idx;
	u8 dev_id = DVFS_DEVICE_MAX;
	/* 1. get core id: pscp or phycp */
	struct device *dev = kobj_to_dev(kobj->parent);
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);


	dev_dbg(dev, "buf=%s\n", buf);
	if (sscanf(buf, "%hhu\n", &idle_index) != 1)
		return -EINVAL;
	if (idle_index >= DVFS_INDEX_MAX) {
		dev_err(dev, "input is %d, pls input right idx [0,7]\n",
				idle_index);
		return -EINVAL;
	}

	mutex_lock(&cp_dvfs_lock);
	/* 2. get device id: core, axi, apb */
	for (idx = 0; idx < cpdvfs->devices_num; idx++) {
		 if (!strcmp(kobj->name, cpdvfs->user_data->devices[idx].name)) {
		    dev_id = cpdvfs->user_data->devices[idx].dev_id;
		    cpdvfs->user_data->curr_dev_id = cpdvfs->user_data->devices[idx].dev_id;
		 }
	}
	dev_info(dev, "core_id=%d, device_id=%d\n", cpdvfs->core_id, dev_id);
	ret = cp_dvfs_send_cmd_onepara(cpdvfs, DVFS_SET_IDLE_INDEX, idle_index);

	if (ret != 0)
		dev_err(dev, "send command fail\n");
	mutex_unlock(&cp_dvfs_lock);

	return count;
}

static ssize_t auto_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int auto_en = -1, ret;
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	mutex_lock(&cp_dvfs_lock);
	ret = cp_dvfs_send_cmd_nopara(cpdvfs, DVFS_GET_AUTO);
	if (ret == 0) {
		auto_en = cp_dvfs_recv_cmd_onepara(cpdvfs, DVFS_GET_AUTO);
		cpdvfs->user_data->auto_enable =  auto_en;
	} else {
		dev_err(dev, "send command fail\n");
	}
	mutex_unlock(&cp_dvfs_lock);

	return sprintf(buf, "%d\n", auto_en);
}

static ssize_t auto_status_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	u8 auto_enable;
	int ret;
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	dev_dbg(dev, "buf=%s\n", buf);
	if (sscanf(buf, "%hhu\n", &auto_enable) != 1)
		return -EINVAL;
	mutex_lock(&cp_dvfs_lock);
	if (auto_enable == 0 || auto_enable == 1) {
		ret = cp_dvfs_send_cmd_onepara(cpdvfs, DVFS_SET_AUTO, auto_enable);
		if (ret)
			dev_err(dev, "send command fail\n");
	} else
		dev_err(dev, "input is %d, pls input right val:[0,1]\n",
				auto_enable);
	mutex_unlock(&cp_dvfs_lock);

	return count;
}
static DEVICE_ATTR_RW(auto_status);

static ssize_t set_reg_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	u32 ret;
	struct reg_t reg;
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	dev_dbg(dev, "buf=%s\n", buf);
	if (sscanf(buf, "%x %x", &reg.reg_addr, &reg.reg_val) != 2)
		return -EINVAL;
	dev_dbg(dev, "reg_addr=0x%x reg_val=0x%x\n", reg.reg_addr, reg.reg_val);
	mutex_lock(&cp_dvfs_lock);
	ret = cp_dvfs_send_setreg(cpdvfs, &reg);
	if (ret != 0)
		dev_err(dev, "send command fail\n");
	memcpy(&cpdvfs->user_data->set_reg, &reg, sizeof(struct reg_t));
	mutex_unlock(&cp_dvfs_lock);

	return count;
}

static ssize_t set_reg_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);
	int len;

	len = sprintf(buf, "addr:0x%08x, value:0x%08x\n",
			cpdvfs->user_data->set_reg.reg_addr,
			cpdvfs->user_data->set_reg.reg_val);
	return len;
}
static DEVICE_ATTR_RW(set_reg);

static ssize_t inq_reg_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);
	int len;

	len = sprintf(buf, "addr:0x%08x, value:0x%08x\n",
			cpdvfs->user_data->inq_reg.reg_addr,
			cpdvfs->user_data->inq_reg.reg_val);
	return len;
}

static ssize_t inq_reg_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	u32 reg_addr, reg_val, ret;
	struct cpdvfs_data *cpdvfs = dev_get_drvdata(dev);

	dev_dbg(dev, "buf=%s\n", buf);
	if (sscanf(buf, "%x\n", &reg_addr) != 1)
		return -EINVAL;
	mutex_lock(&cp_dvfs_lock);
	ret = cp_dvfs_send_inqreg(cpdvfs, &reg_addr);
	if (ret != 0)
		dev_err(dev, "send command fail\n");
	reg_val = cp_dvfs_recv_reg_val(cpdvfs);
	dev_dbg(dev, "reg_addr=0x%08x, reg_val=0x%08x\n", reg_addr, reg_val);
	mutex_unlock(&cp_dvfs_lock);

	return count;
}
static DEVICE_ATTR_RW(inq_reg);

static struct attribute *cpdvfs_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_core_id.attr,
	&dev_attr_dev_info.attr,
	&dev_attr_auto_status.attr,
	&dev_attr_set_reg.attr,
	&dev_attr_inq_reg.attr,
	NULL,
};
ATTRIBUTE_GROUPS(cpdvfs);

static struct kobj_attribute index_kobj_attr = __ATTR_RW(index);
static struct kobj_attribute idle_index_kobj_attr = __ATTR_RW(idle_index);

static struct attribute *dev_attrs[] = {
	&index_kobj_attr.attr,
	&idle_index_kobj_attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(dev);

static int
cp_dvfs_get_of_pdata(struct device *dev, struct cpdvfs_data *pdata)
{
	struct device_node *np = dev->of_node;
	int err;

	err = of_property_read_u32(np, "sprd,core_id", &pdata->core_id);
	if (err) {
		dev_err(dev, "fail to get core id\n");
		return err;
	}

	if (pdata->core_id == DVFS_CORE_PHYCP)
		pdata->dst = SIPC_ID_NR_PHY;
	else
		pdata->dst = SIPC_ID_LTE;

	err = of_property_read_string(dev->of_node, "compatible", &pdata->name);
	if (err) {
		dev_err(dev, "fail to get name\n");
		return err;
	}

	err = of_property_read_u32(np, "sprd,devices-num", &pdata->devices_num);
	if (err) {
		dev_err(dev, "fail to get devices num\n");
		return err;
	}
	return 0;
}

static int cp_dvfs_init(struct cpdvfs_data *cpdvfs)
{
	struct device_node *np = cpdvfs->dev->of_node;
	int ret = 0;
	struct userspace_data *usr_data;
	struct device_info *dev_info;
	struct cmd_pkt *cmd;
	int n;

	usr_data = devm_kzalloc(cpdvfs->dev, sizeof(*usr_data), GFP_KERNEL);
	if (!usr_data)
		return -ENOMEM;
	cpdvfs->user_data = usr_data;

	dev_info = devm_kzalloc(cpdvfs->dev, sizeof(*dev_info) * cpdvfs->devices_num, GFP_KERNEL);
	if (!dev_info)
		return -ENOMEM;
	dev_err(cpdvfs->dev, "cpdvfs->devices_num=%d \n", cpdvfs->devices_num);
	for (n = 0; n < cpdvfs->devices_num; n++) {
		ret = of_property_read_string_index(np, "sprd,devices-name", n,
						    &(dev_info[n].name));
		if (ret) {
			dev_err(cpdvfs->dev, "fail to get devices name\n");
			return ret;
		}

		ret = of_property_read_u32_index(np, "sprd,devices-id", n,
							 &(dev_info[n].dev_id));
		if (ret) {
			dev_err(cpdvfs->dev, "fail to get devices id\n");
			return ret;
		}
	}
	usr_data->devices = dev_info;

	cmd = devm_kzalloc(cpdvfs->dev, sizeof(*cmd) + CMD_PARA_MAX_LEN, GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;
	cpdvfs->sent_cmd = cmd;

	if (sbuf_dst != cpdvfs->dst) {
		ret = sbuf_create(cpdvfs->dst, SMSG_CH_DVFS, CP_DVFS_SBUF_NUM,
				  CP_DVFS_TXBUFSIZE, CP_DVFS_RXBUFSIZE);
		if (ret < 0) {
			dev_err(cpdvfs->dev, "create sbuf fail!\n");
			return ret;
		}
		sbuf_dst = cpdvfs->dst;
		dev_err(cpdvfs->dev, "sbuf_dst=%d\n", sbuf_dst);
	}

	return ret;
}

static int cp_dvfs_sysfs_create(struct cpdvfs_data *cpdvfs)
{
	struct device *dev = cpdvfs->dev;
	struct kobject *dev_kobj;
	const char *dev_name;
	int index;
	int err;

	err = sysfs_create_groups(&dev->kobj, cpdvfs_groups);
	if (err) {
		dev_err(cpdvfs->dev, "failed to create sysfs attributes\n");
	} else {
		for (index = 0; index < cpdvfs->devices_num; index++) {
			dev_name = cpdvfs->user_data->devices[index].name;
			dev_kobj = kobject_create_and_add(dev_name, &dev->kobj);
			if (!(dev_kobj &&
			      !sysfs_create_groups(dev_kobj, dev_groups))) {
				err = -ENOMEM;
				dev_err(cpdvfs->dev, "failed to add '%s' node\n", dev_name);
			}
		}
	}

	return err;
}

static int cp_dvfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cpdvfs_data *data;
	int err;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	data->dev = dev;
	err = cp_dvfs_get_of_pdata(dev, data);
	if (err)
		goto exit;

	dev_info(dev, "dvfs probe\n");
	platform_set_drvdata(pdev, data);
	err = cp_dvfs_init(data);
	if (err) {
		dev_err(dev, "dvfs init fail\n");
		goto exit;
	}

	err = cp_dvfs_sysfs_create(data);
	if (err) {
		dev_err(dev, "failed to create sysfs device attributes\n");
		goto exit;
	}
	return 0;

exit:
	return err;
}

static int cp_dvfs_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	sysfs_remove_groups(&dev->kobj, cpdvfs_groups);
	/* There is no need to call devm_free,
	 * memory alloced through devm_kzalloc will free automatically.
	 */
	return 0;
}

static const struct of_device_id cp_dvfs_match[] = {
	{ .compatible = "sprd,pubcp-dvfs" },
	{ .compatible = "sprd,wtlcp-dvfs" },
	{ .compatible = "sprd,pscp-dvfs" },
	{ .compatible = "sprd,phycp-dvfs" },
	{},
};

MODULE_DEVICE_TABLE(of, cp_dvfs_match);

static struct platform_driver cp_dvfs_driver = {
	.probe = cp_dvfs_probe,
	.remove = cp_dvfs_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd-cp-dvfs",
		.of_match_table = of_match_ptr(cp_dvfs_match),
	},
};

module_platform_driver(cp_dvfs_driver);

MODULE_AUTHOR("Bao Yue <bao.yue@unisoc.com>");
MODULE_DESCRIPTION("CP dvfs driver");
MODULE_LICENSE("GPL v2");
