/*
 * Copyright (C) 2016-2018 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ioctl.h"
#include "pcie.h"
#include "edma_engine.h"
#include "pcie_dbg.h"

enum PCIE_ARG_INDEX {
	PCIE_ARG_ADDR,
	PCIE_ARG_VALUE,
	PCIE_ARG_SIZE,
	PCIE_ARG_BAR,
	PCIE_ARG_RUN,
	PCIE_ARG_OFFSET,
	PCIE_ARG_VIR,
	PCIE_ARG_REGION,
	PCIE_ARG_TWO_LINK,
	PCIE_ARG_MODE,
	PCIE_ARG_MAX,
};

static struct arg_t pcie_args[PCIE_ARG_MAX] = {
	{PCIE_ARG_ADDR, "addr", 0},
	{PCIE_ARG_VALUE, "value", 0},
	{PCIE_ARG_SIZE, "size", 4},
	{PCIE_ARG_BAR, "bar", 0},
	{PCIE_ARG_OFFSET, "offset", 0},
	{PCIE_ARG_RUN, "run", 0},
	{PCIE_ARG_VIR, "vir", 0},
	{PCIE_ARG_REGION, "region", 0},
	{PCIE_ARG_TWO_LINK, "link", 0},
	{PCIE_ARG_MODE, "mode", 0},
};

struct arg_t *pcie_arg_index(unsigned int index)
{
	int i;

	for (i = 0; i < PCIE_ARG_MAX; i++) {
		if (index == pcie_args[i].id)
			return &pcie_args[i];

	}
	return NULL;
}

static unsigned long args_value(int index)
{
	int i;

	for (i = 0; i < PCIE_ARG_MAX; i++) {
		if (index == pcie_args[i].id)
			return pcie_args[i].def;
	}

	return -1;
}

static int hwcopy(unsigned char *dest, unsigned char *src, int len)
{
	int i;

	for (i = 0; i < (len / 4); i++) {
		*((int *)(dest)) = *((int *)(src));
		dest += 4;
		src += 4;
	}
	for (i = 0; i < (len % 4); i++) {
		*dest = *src;
		dest++;
		src++;
	}

	return 0;
}

static int cmdline_args(char *cmdline, char *cmd, struct arg_t *args, int argc)
{
	int status = 0;
	char *end;
	char *argname;
	char *cp;
	int DONE;
	int FOUND = 0;

	unsigned short base;
	unsigned long result, value;
	unsigned long val;
	unsigned long i;
	unsigned long j;

	for (i = 0; i < argc; i++)
		args[i].def = 0;

	/* get cmd */
	while (*cmdline == ' ' || *cmdline == '\t')
		cmdline++;

	while (*cmdline != ' ' && *cmdline != '\t' && *cmdline != '\0') {
		*cmd = *cmdline;
		cmd++;
		cmdline++;
	}
	*cmd = '\0';
	if (*cmdline == '\0')
		goto WEDONE;

	*cmdline = '\0';
	cmdline++;
	while (*cmdline == ' ' || *cmdline == '\t')
		cmdline++;

	end = cmdline;
	while (*end == ' ' || *end == '\t')
		end++;

	/*
	 * Parse cmdline
	 */
	DONE = (*end == '\0') ? 1 : 0;
	while (!DONE) {
		/* get the register name */
		while (*end != '=' && *end != '\0')
			end++;
		if (*end == '\0') {
			status = 1;
			goto WEDONE;
		}
		*end = '\0';
		argname = cmdline;
		/* now get value to write to register */
		cmdline = ++end;
		/* if there's whitespace after the '=', exit with an error */
		if (*end == ' ' || *end == '\t' || *end == '\n') {
			status = 1;
			goto WEDONE;
		}
		while (*end != ' ' && *end != '\t' && *end != '\n'
		       && *end != '\0')
			end++;
		if (*end == '\0')
			DONE = 1;
		else
			*end = '\0';

		if (!strcmp(argname, "file") || !strcmp(argname, "filec")) {
			val = 1;
		} else {
		/* get the base, convert value to base-10 if necessary */
			val = 0;
			result = 0;
			cp = cmdline;
			if (cp[0] == '0' && (cp[1] == 'x' || cp[1] == 'X')) {
				base = 16;
				cp += 2;

			} else {
				base = 10;
			}
			while (isxdigit(*cp)) {
				value = isdigit(*cp) ? (*cp - '0')
				    : ((islower(*cp) ? toupper(*cp) : *cp) -
				       'A' + 10);

				result = result * base + value;
				cp++;
			}

			val = result;
		}

		FOUND = 0;
		/*
		 * verify the register arg is valid, and if the value is not
		 * too big, write it to the corresponding location in arg_vals
		 */
		for (j = 0; j < argc && !FOUND; j++) {
			if (!strcmp(argname, args[j].name)) {
				args[j].def = val;
				FOUND = 1;
			}
		}
		if (!FOUND) {
			WCN_ERR("arg %s err\n", argname);
			status = 0;
			goto WEDONE;
		}

		/*
		 * point cmdline and end to next non-whitespace
		 * (next argument)
		 */
		cmdline = ++end;
		while (*cmdline == ' ' || *cmdline == '\t' || *cmdline == '\n')
			cmdline++;
		end = cmdline;
		/*
		 * if, after skipping whitespace, we hit end of line or EOF,
		 * we're done
		 */
		if (*end == '\0')
			DONE = 1;
	}

WEDONE:
	return status;
}

static int pcie_cmd_proc(struct char_drv_info *dev, unsigned char *input,
		  int input_len, struct tlv *replay)
{
	int ret, bar, offset, i, run, flag;
	unsigned long addr, value, size, mode;
	unsigned char cmd[64], string[512] = { 0 }, *buf, *mem;
	struct pcicmd *pcicmd;
	struct dma_buf dm;
	struct wcn_pcie_info *priv;

	priv = dev->pcie_dev_info;

	/* IRAM */
	pcicmd = (struct pcicmd *) (pcie_bar_vmem(priv, 0) +
				    cp2_test_addr1 + cp2_test_addr2);

	ret = cmdline_args(input, cmd, pcie_args, PCIE_ARG_MAX);
	if (ret) {
		WCN_INFO("cmdline_args err\n");
		return -1;
	} else if (!strcmp("mread", cmd)) {
		addr = args_value(PCIE_ARG_ADDR);
		size = args_value(PCIE_ARG_SIZE);

		replay->t = 1;
		replay->l = size;
		memcpy(replay->v, (unsigned char *)addr, replay->l);
	} else if (!strcmp("mwrite", cmd)) {
		addr = args_value(PCIE_ARG_ADDR);
		value = args_value(PCIE_ARG_VALUE);
		size = args_value(PCIE_ARG_SIZE);
		WCN_INFO("memwrite addr=0x%lx value=0x%lx size=%ld\n",
			 addr, value, size);
		memcpy((char *)(addr), (char *)&value, 4);
	} else if (!strcmp("init", cmd)) {
		struct inbound_reg *ibreg =
				(struct inbound_reg *) ibreg_base(priv, 0);

		if (ibreg == NULL) {
			WCN_ERR("ibreg(0) NULL\n");
			return -1;
		}

		writel(0x40000000, &ibreg->lower_target_addr);
		writel(0x00000000, &ibreg->upper_target_addr);
		writel(0x00000000, &ibreg->type);
		writel(0x00FFFFFF, &ibreg->limit);
		writel(0xc0000000, &ibreg->en);

		replay->t = 1;
		replay->l = sizeof(struct inbound_reg);
		hwcopy(replay->v, (unsigned char *)ibreg, replay->l);
	} else if (!strcmp("config", cmd)) {
		size = args_value(PCIE_ARG_SIZE);
		if (size < 256)
			replay->l = 256;
		else
			replay->l = size;
		replay->t = 1;
		pcie_config_read(priv, 0, replay->v, replay->l);
	} else if (!strcmp("bwrite", cmd)) {
		bar = args_value(PCIE_ARG_BAR);
		offset = args_value(PCIE_ARG_OFFSET);
		value = args_value(PCIE_ARG_VALUE);

		sprintf(string, "bwrite bar=%d offset=0x%x value=0x%lx\n", bar,
			offset, value);
		WCN_INFO("do %s\n", string);
		pcie_bar_write(priv, bar, offset, (char *)(&value), 4);
		replay->t = 0;
		replay->l = strlen(string);
		memcpy(replay->v, string, replay->l);
	} else if (!strcmp("bread", cmd)) {
		bar = args_value(PCIE_ARG_BAR);
		offset = args_value(PCIE_ARG_OFFSET);
		size = args_value(PCIE_ARG_SIZE);
		buf = pcie_bar_vmem(priv, bar) + offset;
		WCN_INFO("kernel bread bar=%d offset=0x%x size=0x%lx\n", bar,
			 offset, size);
		replay->t = 1;
		replay->l = size;
		hwcopy(replay->v, buf, replay->l);
	} else if (!strcmp("dmalloc", cmd)) {
		size = args_value(PCIE_ARG_SIZE);

		ret = dmalloc(priv, &dm, size);
		if (!ret) {
			sprintf(string, "dmalloc(%ld) 0x%lx, 0x%lx ok\n",
				size, dm.vir, dm.phy);
		} else
			sprintf(string, "dmalloc(%ld) fail\n", size);
		replay->t = 0;
		replay->l = strlen(string);
		memcpy(replay->v, string, replay->l);
	} else if (!strcmp("malloc", cmd)) {
		size = args_value(PCIE_ARG_SIZE);
		mem = (unsigned char *)mpool_malloc(size);

	} else if (!strcmp("outbound", cmd)) {
		addr = args_value(PCIE_ARG_ADDR);
		size = args_value(PCIE_ARG_SIZE);
		run = args_value(PCIE_ARG_RUN);

		pcicmd->addr1 = addr;
		pcicmd->arg[0] = size;
		pcicmd->arg[1] = run;
		pcicmd->cmd = 0x00000002;
		flag = 1;
		i = 10000;
		while (i--) {
			if (pcicmd->cmd == MAGIC_VALUE) {
				replay->t = 0;
				replay->l = 128;
				memcpy(replay->v,
				       pcie_bar_vmem(priv, 0) + cp2_test_addr3 +
				       cp2_test_addr4, 128);
				sprintf(string, "ok\n");
				replay->t = 0;
				replay->l = strlen(string);
				memcpy(replay->v, string, replay->l);
				flag = 0;
				break;
			}
			usleep_range_state(1000, 2000, TASK_UNINTERRUPTIBLE);
		}
		if (flag == 1) {
			sprintf(string, "cmd timeout\n");
			replay->t = 0;
			replay->l = strlen(string);
			memcpy(replay->v, string, replay->l);
		}
	} else if (!strcmp("inbound", cmd)) {
		bar = args_value(PCIE_ARG_BAR);
		offset = args_value(PCIE_ARG_OFFSET);
		size = args_value(PCIE_ARG_SIZE);
		run = args_value(PCIE_ARG_RUN);

		WCN_INFO("inbound(%d,%ld,%d)\n", offset, size, run);

		mem = kmalloc(size, GFP_KERNEL);
		if (!mem) {
			WCN_ERR("kmalloc mem (%ld) err\n", size);
			return -1;
		}
		buf = kmalloc(size, GFP_KERNEL);
		if (!buf) {
			WCN_ERR("kmalloc buf (%ld) err\n", size);
			kfree(mem);
			return -1;
		}
		for (i = 0; i < run; i++) {
			memcpy(buf + 0, (char *)(&i), 4);
			memset(buf + 4, (char)i, size - 4);
			memcpy(pcie_bar_vmem(priv, bar) + offset, buf,
			       size);
			memcpy(mem, pcie_bar_vmem(priv, bar) + offset,
			       size);
			if (memcmp(buf, mem, size)) {
				sprintf(string, "inbound run %d err\n", i);
				break;
			}
		}
		if (i == run) {
			sprintf(string, "inbound(0x%x,0x%lx,0x%x) ok\n", offset,
				size, run);
		}
		WCN_INFO("%s", string);
		replay->t = 0;
		replay->l = strlen(string);
		memcpy(replay->v, string, replay->l);

		kfree(mem);
		kfree(buf);
	} else if (!strcmp("lo_start", cmd)) {
		mode = args_value(PCIE_ARG_MODE);
		lo_start(mode);
	} else if (!strcmp("lo_stop", cmd))
		lo_stop();
	else
		WCN_INFO("unknown cmd %s\n", cmd);

	return 0;
}

int hexdump(char *name, char *buf, int len)
{
	int i, count;
	unsigned int *p;

	count = len / 32;
	count += 1;
	WCN_INFO("%s %s hex(len=%d):\n", __func__, name, len);
	for (i = 0; i < count; i++) {
		p = (unsigned int *)(buf + i * 32);
		WCN_INFO(
			 "mem[0x%04x] 0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x\n",
			 i * 32, p[0], p[1], p[2], p[3], p[4],
			 p[5], p[6], p[7]);
	}

	return 0;
}

static int char_open(struct inode *inode, struct file *filp)
{
	struct char_drv_info *dev;

	WCN_INFO("%s\n", __func__);
	dev = container_of(inode->i_cdev, struct char_drv_info, testcdev);
	filp->private_data = dev;

	return 0;
}

static ssize_t char_write(struct file *filp, const char __user *buffer,
			  size_t count, loff_t *offset)
{
	WCN_INFO("%s\n", __func__);

	return 0;
}

static ssize_t char_read(struct file *filp, char __user *buffer, size_t count,
		  loff_t *offset)
{
	WCN_INFO("%s\n", __func__);

	return 0;
}

static int char_release(struct inode *inode, struct file *filp)
{
	WCN_INFO("%s\n", __func__);

	return 0;
}

static long char_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct tlv *tlv;
	unsigned char *buf[2];
	struct char_drv_info *dev = filp->private_data;

	buf[0] = kmalloc(4096, GFP_KERNEL);
	buf[1] = kmalloc(4096, GFP_KERNEL);

	tlv = (struct tlv *) buf[1];

	ret = copy_from_user(buf[0], (char __user *)arg, 64);
	WCN_INFO("input:%s\n", buf[0]);

	pcie_cmd_proc(dev, buf[0], 4096, (struct tlv *) buf[1]);

	/* sizeof(struct tlv) + tlv->l */
	ret = copy_to_user((char __user *)arg, buf[1], 4096);

	kfree(buf[0]);
	kfree(buf[1]);

	return 0;
}

static const struct file_operations fop = {
	.owner = THIS_MODULE,
	.open = char_open,
	.release = char_release,
	.write = char_write,
	.read = char_read,
	.unlocked_ioctl = char_ioctl,
};

static int ioctlcmd_init(struct wcn_pcie_info *bus)
{
	int ret;
	dev_t dev;
	struct char_drv_info *drv;

	drv = kmalloc(sizeof(struct char_drv_info), GFP_KERNEL);
	if (!drv) {
		ret = -ENOMEM;
		return ret;
	}

	bus->p_char = drv;
	drv->pcie_dev_info = bus;
	drv->major = 321;
	dev = MKDEV(drv->major, 0);
	ret = register_chrdev_region(dev, 1, "char");
	if (ret) {
		alloc_chrdev_region(&dev, 0, 1, "char");
		drv->major = MAJOR(dev);
	}
	drv->testcdev.owner = THIS_MODULE;
	cdev_init(&(drv->testcdev), &fop);
	cdev_add(&(drv->testcdev), dev, 1);

	drv->myclass = class_create(THIS_MODULE, "char_class");
	drv->mydev = device_create(drv->myclass, NULL, dev, NULL, "kchar");
	WCN_INFO("module init ok ...\n");

	return 0;
}

int ioctlcmd_deinit(struct wcn_pcie_info *bus)
{
	dev_t dev;
	struct char_drv_info *drv = bus->p_char;

	dev = MKDEV(drv->major, 0);

	device_destroy(drv->myclass, dev);
	class_destroy(drv->myclass);

	cdev_del(&(drv->testcdev));
	unregister_chrdev_region(dev, 1);
	if (drv)
		kfree(drv);
	WCN_INFO("module exit ok....\n");

	return 0;
}

int dbg_attach_bus(struct wcn_pcie_info *bus)
{
	return ioctlcmd_init(bus);
}
