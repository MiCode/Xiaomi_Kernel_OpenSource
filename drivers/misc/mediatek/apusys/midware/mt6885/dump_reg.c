/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/sched/clock.h>
#include <apusys_secure.h>
#include <apusys_plat.h>

#define APUSYS_REG_SIZE (0x100000)
#define APUSYS_BASE (0x19000000)
#define APUSYS_TO_INFRA_BASE (0x10000000)
#define NA	(-1)
char reg_all_mem[APUSYS_REG_SIZE];
bool apusys_dump_force;
bool apusys_dump_skip_gals;
static void *apu_top;
static void *apu_to_infra_top;
static struct dentry *debug_node;
static struct mutex dbg_lock;


static void set_vcore_dbg_sel(int val)
{
	u32 tmp = ioread32(apu_top + 0x29010);

	tmp = (tmp & ~(0x1 << 1)) | (val << 1);
	iowrite32(tmp, apu_top + 0x29010);
	tmp = ioread32(apu_top + 0x29010);
}

static void set_vcore_dbg_sel0(int val)
{
	u32 tmp = ioread32(apu_top + 0x29010);

	tmp = (tmp & ~(0x7 << 2)) | (val << 2);
	iowrite32(tmp, apu_top + 0x29010);
	tmp = ioread32(apu_top + 0x29010);
}

static void set_vcore_dbg_sel1(int val)
{
	u32 tmp = ioread32(apu_top + 0x29010);

	tmp = (tmp & ~(0x7 << 5)) | (val << 5);
	iowrite32(tmp, apu_top + 0x29010);
	ioread32(apu_top + 0x29010);
}

static void set_conn_dbg0_sel(int val)
{
	u32 tmp = ioread32(apu_top + 0x20138);

	tmp = (tmp & ~(0x7)) | (val);
	*(u32 *)(apu_top + 0x20138) = tmp;
	iowrite32(tmp, apu_top + 0x20138);
	ioread32(apu_top + 0x20138);
}

static void set_conn_dbg3_sel(int val)
{
	u32 tmp = ioread32(apu_top + 0x20138);

	tmp = (tmp & ~(0x7 << 9)) | (val << 9);
	iowrite32(tmp, apu_top + 0x20138);
	ioread32(apu_top + 0x20138);
}

static void set_conn_dbg4_sel(int val)
{
	u32 tmp = ioread32(apu_top + 0x20138);

	tmp = (tmp & ~(0x7 << 12)) | (val << 12);
	iowrite32(tmp, apu_top + 0x20138);
	ioread32(apu_top + 0x20138);
}

static void set_conn_dbg5_sel(int val)
{
	u32 tmp = ioread32(apu_top + 0x20138);

	tmp = (tmp & ~(0x7 << 15)) | (val << 15);
	iowrite32(tmp, apu_top + 0x20138);
	ioread32(apu_top + 0x20138);
}

static void set_conn_dbg6_sel(int val)
{
	u32 tmp = ioread32(apu_top + 0x20138);

	tmp = (tmp & ~(0x7 << 18)) | (val << 18);
	iowrite32(tmp, apu_top + 0x20138);
	ioread32(apu_top + 0x20138);
}

static void set_mdla0_axi_gals_dbg_sel(int val)
{
	u32 tmp = ioread32(apu_top + 0x34130);

	tmp = (tmp & ~(0x1 << 10)) | (val << 10);
	iowrite32(tmp, apu_top + 0x34130);
	ioread32(apu_top + 0x34130);
}

static void set_mdla1_axi_gals_dbg_sel(int val)
{
	u32 tmp = ioread32(apu_top + 0x38130);

	tmp = (tmp & ~(0x1 << 10)) | (val << 10);
	iowrite32(tmp, apu_top + 0x38130);
	ioread32(apu_top + 0x38130);
}

static void set_vpu0_apu_gals_m_ctl_sel(int val)
{
	u32 tmp = ioread32(apu_top + 0x30a10);

	tmp = (tmp & ~(0x1 << 10)) | (val << 10);
	iowrite32(tmp, apu_top + 0x30a10);
	ioread32(apu_top + 0x30a10);
}

static void set_vpu1_apu_gals_m_ctl_sel(int val)
{
	u32 tmp = ioread32(apu_top + 0x31a10);

	tmp = (tmp & ~(0x1 << 10)) | (val << 10);
	iowrite32(tmp, apu_top + 0x31a10);
	ioread32(apu_top + 0x31a10);
}

static void set_vpu2_apu_gals_m_ctl_sel(int val)
{
	u32 tmp = ioread32(apu_top + 0x32a10);

	tmp = (tmp & ~(0x1 << 10)) | (val << 10);
	iowrite32(tmp, apu_top + 0x32a10);
	ioread32(apu_top + 0x32a10);
}

static void set_ao_dbg_sel(int val)
{
	mt_secure_call(MTK_SIP_APUSYS_CONTROL,
			MTK_APUSYS_KERNEL_OP_SET_AO_DBG_SEL, val, 0, 0);
}

u32 dbg_read(void *addr, int d, int e, int f,
		int g, int h, int i, int j, int k,
		int l, int m, int n, int o, int p, int q)
{
	if (d >= 0)
		set_vcore_dbg_sel(d);
	if (e >= 0)
		set_vcore_dbg_sel0(e);
	if (f >= 0)
		set_vcore_dbg_sel1(f);
	if (g >= 0)
		set_conn_dbg0_sel(g);
	if (h >= 0)
		set_conn_dbg3_sel(h);
	if (i >= 0)
		set_conn_dbg4_sel(i);
	if (j >= 0)
		set_conn_dbg5_sel(j);
	if (k >= 0)
		set_conn_dbg6_sel(k);
	if (l >= 0)
		set_mdla0_axi_gals_dbg_sel(l);
	if (m >= 0)
		set_mdla1_axi_gals_dbg_sel(m);
	if (n >= 0)
		set_vpu0_apu_gals_m_ctl_sel(n);
	if (o >= 0)
		set_vpu1_apu_gals_m_ctl_sel(o);
	if (p >= 0)
		set_vpu2_apu_gals_m_ctl_sel(p);
	if (q >= 0)
		set_ao_dbg_sel(q);
	return ioread32(addr);
}

static u32 gals_reg[64];

void dump_gals_reg(void)
{
	void *addr = apu_top+0x2901c;

	gals_reg[0] = dbg_read(addr, 0, 1, NA, NA, NA, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[1] = dbg_read(addr, 0, 2, NA, NA, NA, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[2] = dbg_read(addr, 0, 3, NA, NA, NA, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[3] = dbg_read(addr, 0, 4, NA, NA, NA, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);

	gals_reg[4] = dbg_read(addr, 1, NA, 1, NA, NA, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[5] = dbg_read(addr, 1, NA, 2, NA, NA, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[6] = dbg_read(addr, 1, NA, 3, NA, NA, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[7] = dbg_read(addr, 1, NA, 4, NA, NA, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[8] = dbg_read(addr, 1, NA, 6, NA, NA, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[9] = dbg_read(addr, 0, 6, NA, NA, NA, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);

	gals_reg[10] = dbg_read(addr, 0, 7, NA, 3, NA, NA, 0,
			NA, NA, NA, NA, NA, NA, NA);

	gals_reg[11] = dbg_read(addr, 0, 7, NA, 1, 1, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[12] = dbg_read(addr, 0, 7, NA, 1, 2, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[13] = dbg_read(addr, 0, 7, NA, 1, 3, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[14] = dbg_read(addr, 0, 7, NA, 1, 4, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[15] = dbg_read(addr, 0, 7, NA, 1, 5, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[16] = dbg_read(addr, 0, 7, NA, 1, 0, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);

	gals_reg[17] = dbg_read(addr, 0, 7, NA, 2, NA, 1, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[18] = dbg_read(addr, 0, 7, NA, 2, NA, 2, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[19] = dbg_read(addr, 0, 7, NA, 2, NA, 3, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[20] = dbg_read(addr, 0, 7, NA, 2, NA, 0, NA,
			NA, NA, NA, NA, NA, NA, NA);

	gals_reg[21] = dbg_read(addr, 0, 7, NA, 3, NA, NA, 3,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[22] = dbg_read(addr, 0, 7, NA, 3, NA, NA, 4,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[23] = dbg_read(addr, 0, 7, NA, 3, NA, NA, 1,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[24] = dbg_read(addr, 0, 7, NA, 3, NA, NA, 2,
			NA, NA, NA, NA, NA, NA, NA);

	addr = apu_top + 0x3413C;
	gals_reg[25] = dbg_read(addr, NA, NA, NA, NA, NA, NA,
			NA, NA, 0, NA, NA, NA, NA, NA);
	gals_reg[26] = dbg_read(addr, NA, NA, NA, NA, NA, NA,
			NA, NA, 1, NA, NA, NA, NA, NA);
	gals_reg[27] = dbg_read(addr, NA, NA, NA, NA, NA, NA,
			NA, NA, NA, 0, NA, NA, NA, NA);
	gals_reg[28] = dbg_read(addr, NA, NA, NA, NA, NA, NA,
			NA, NA, NA, 1, NA, NA, NA, NA);

	addr = apu_top+0x2901c;
	gals_reg[35] = dbg_read(addr, 0, 5, NA, NA, NA, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[36] = dbg_read(addr, 1, NA, 5, NA, NA, NA, NA,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[37] = dbg_read(addr, 0, 7, NA, 3, NA, NA, 5,
			NA, NA, NA, NA, NA, NA, NA);
	gals_reg[38] = dbg_read(addr, 1, NA, 7, NA, NA, NA, NA,
				NA, NA, NA, NA, NA, NA, 1);
	gals_reg[39] = dbg_read(addr, 1, NA, 7, NA, NA, NA, NA,
				NA, NA, NA, NA, NA, NA, 0);

	gals_reg[40] = ioread32(apu_to_infra_top + 0x12C0);
	gals_reg[41] = ioread32(apu_to_infra_top + 0x1220);
	gals_reg[42] = ioread32(apu_to_infra_top + 0x612C);
	gals_reg[43] = ioread32(apu_to_infra_top + 0x2050);

}


void dump_gals(struct seq_file *sfile)
{
	seq_printf(sfile, "VCORE2EMI_N0_GALS_TX: 0x%08x\n", gals_reg[0]);
	seq_printf(sfile, "VCORE2EMI_N1_GALS_TX: 0x%08x\n", gals_reg[1]);
	seq_printf(sfile, "VCORE2EMI_S0_GALS_TX: 0x%08x\n", gals_reg[2]);
	seq_printf(sfile, "VCORE2EMI_S1_GALS_TX: 0x%08x\n", gals_reg[3]);
	seq_printf(sfile, "CONN2VCORE_EMI_N0_GALS_RX: 0x%08x\n", gals_reg[4]);
	seq_printf(sfile, "CONN2VCORE_EMI_N1_GALS_RX: 0x%08x\n", gals_reg[5]);
	seq_printf(sfile, "CONN2VCORE_EMI_S0_GALS_RX: 0x%08x\n", gals_reg[6]);
	seq_printf(sfile, "CONN2VCORE_EMI_S1_GALS_RX: 0x%08x\n", gals_reg[7]);
	seq_printf(sfile, "XPU2APUSYS_VCORE_GALS_TX: 0x%08x\n",  gals_reg[8]);
	seq_printf(sfile, "XPU2APUSYS_VCORE_GALS_RX: 0x%08x\n", gals_reg[9]);
	seq_printf(sfile, "XPU2APUSYS_CONN_GALS_RX: 0x%08x\n", gals_reg[10]);
	seq_printf(sfile, "VPU02ONN_GALS_RX: 0x%08x\n", gals_reg[11]);
	seq_printf(sfile, "VPU12CONN_GALS_RX: 0x%08x\n", gals_reg[12]);
	seq_printf(sfile, "VPU22CONN_GALS_RX: 0x%08x\n", gals_reg[13]);
	seq_printf(sfile, "CONN2VPU0_GALS_TX: 0x%08x\n", gals_reg[14]);
	seq_printf(sfile, "CONN2VPU1_GALS_TX: 0x%08x\n", gals_reg[15]);
	seq_printf(sfile, "CONN2VPU2_GALS_TX: 0x%08x\n", gals_reg[16]);
	seq_printf(sfile, "MDLA0M02CONN_GALS_RX: 0x%08x\n", gals_reg[17]);
	seq_printf(sfile, "MDLA0M12CONN_GALS_RX: 0x%08x\n", gals_reg[18]);
	seq_printf(sfile, "MDLA1M02CONN_GALS_RX: 0x%08x\n", gals_reg[19]);
	seq_printf(sfile, "MDLA1M12CONN_GALS_RX: 0x%08x\n", gals_reg[20]);
	seq_printf(sfile, "CONN2VCORE_EMI_N0_GALS_TX: 0x%08x\n", gals_reg[21]);
	seq_printf(sfile, "CONN2VCORE_EMI_N1_GALS_TX: 0x%08x\n", gals_reg[22]);
	seq_printf(sfile, "CONN2VCORE_EMI_S0_GALS_TX: 0x%08x\n", gals_reg[23]);
	seq_printf(sfile, "CONN2VCORE_EMI_S1_GALS_TX: 0x%08x\n", gals_reg[24]);

	seq_printf(sfile, "MDLA0M02CONN_GALS_TX: 0x%08x\n", gals_reg[25]);
	seq_printf(sfile, "MDLA0M12CONN_GALS_TX: 0x%08x\n", gals_reg[26]);
	seq_printf(sfile, "MDLA1M02CONN_GALS_TX: 0x%08x\n", gals_reg[27]);
	seq_printf(sfile, "MDLA1M12CONN_GALS_TX: 0x%08x\n", gals_reg[28]);

	seq_printf(sfile, "APUSYS2ACP_VCORE_GALS_TX: 0x%08x\n", gals_reg[35]);
	seq_printf(sfile, "APUSYS2ACP_VCORE_GALS_RX: 0x%08x\n", gals_reg[36]);
	seq_printf(sfile, "APUSYS2ACP_CONN_GALS_TX: 0x%08x\n", gals_reg[37]);
	seq_printf(sfile, "APUSYS_AO_DBG_RPC: 0x%08x\n", gals_reg[38]);
	seq_printf(sfile, "APUSYS_AO_DBG_PCU: 0x%08x\n", gals_reg[39]);
	seq_printf(sfile, "0x100012C0: 0x%08x\n", gals_reg[40]);
	seq_printf(sfile, "0x10001220: 0x%08x\n", gals_reg[41]);
	seq_printf(sfile, "0x1000612C: 0x%08x\n", gals_reg[42]);
	seq_printf(sfile, "0x10002050: 0x%08x\n", gals_reg[43]);
}


void apusys_dump_reg_skip_gals(int onoff)
{
	mutex_lock(&dbg_lock);
	if (onoff)
		apusys_dump_skip_gals = true;
	else
		apusys_dump_skip_gals = false;
	mutex_unlock(&dbg_lock);
}

void apusys_reg_dump(void)
{
	mutex_lock(&dbg_lock);

	if (!apusys_dump_skip_gals)
		dump_gals_reg();

	/* Skip undefine reg */
	memcpy_fromio(reg_all_mem + 0x01000, apu_top + 0x01000, 0x2000);
	memcpy_fromio(reg_all_mem + 0x10000, apu_top + 0x10000, 0x10000);
	memcpy_fromio(reg_all_mem + 0x20000, apu_top + 0x20000, 0x1000);
	/* Skip 0x1902_1000 for security reason */
	memcpy_fromio(reg_all_mem + 0x22000, apu_top + 0x22000, 0x8000);
	/* Skip 0x1903_0000 for VPU secure reason */
	memcpy_fromio(reg_all_mem + 0x34000, apu_top + 0x34000, 0x2534);
	memcpy_fromio(reg_all_mem + 0x36538, apu_top + 0x36538, 0x4000);
	memcpy_fromio(reg_all_mem + 0x3A538, apu_top + 0x3A538, 0xAC8);
	memcpy_fromio(reg_all_mem + 0x50000, apu_top + 0x50000, 0x1000);
	memcpy_fromio(reg_all_mem + 0x64000, apu_top + 0x64000, 0x2000);
	memcpy_fromio(reg_all_mem + 0x6C000, apu_top + 0x6C000, 0x1000);
	memcpy_fromio(reg_all_mem + 0x6C000, apu_top + 0x6C000, 0x3000);
	memcpy_fromio(reg_all_mem + 0xF0000, apu_top + 0xF0000, 0x3000);
	memcpy_fromio(reg_all_mem + 0xF8000, apu_top + 0xF8000, 0x8000);
	mutex_unlock(&dbg_lock);
}

static void *dump_start(struct seq_file *sfile, loff_t *pos)
{
	mutex_lock(&dbg_lock);
	return pos + (*pos == 0);
}

static void *dump_next(struct seq_file *sfile, void *v, loff_t *pos)
{
	++(*pos);
	return NULL;
}


int apusys_dump_show(struct seq_file *sfile, void *v)
{
	u64 t;
	u64 nanosec_rem;

	t = sched_clock();
	nanosec_rem = do_div(t, 1000000000);

	if (apusys_dump_force)
		apusys_reg_dump();

	seq_printf(sfile, "[%5lu.%06lu] ------- dump GALS -------\n",
		(unsigned long) t, (unsigned long) (nanosec_rem / 1000));
	dump_gals(sfile);
	seq_puts(sfile, "------- dump from 0x1900_0000 to 0x1902_FFFF -------\n");
	seq_puts(sfile, "------- dump from 0x1905_0000 to 0x190F_1FFF -------\n");
	seq_hex_dump(sfile, "", DUMP_PREFIX_OFFSET, 16, 4,
			reg_all_mem, APUSYS_REG_SIZE, false);

	return 0;
}

static void dump_stop(struct seq_file *sfile, void *v)
{
	mutex_unlock(&dbg_lock);
}

static const struct seq_operations dump_sops = {
	.start = dump_start,
	.next = dump_next,
	.stop = dump_stop,
	.show = apusys_dump_show
};

static int dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_dump_show, NULL);
}

static const struct file_operations apu_dump_debug_fops = {
	.open = dump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

void apusys_dump_init(void)
{
	mutex_init(&dbg_lock);
	debug_node = debugfs_create_dir("debug", apusys_dbg_root);
	debugfs_create_file("apusys_reg_all", 0444,
			debug_node, NULL, &apu_dump_debug_fops);

	debugfs_create_bool("force_dump", 0644,
			debug_node, &apusys_dump_force);
	apu_top = ioremap_nocache(APUSYS_BASE, APUSYS_REG_SIZE);
	apu_to_infra_top = ioremap_nocache(APUSYS_TO_INFRA_BASE, 0x10000);

	memset(reg_all_mem, 0, APUSYS_REG_SIZE);
	apusys_dump_force = false;
	apusys_dump_skip_gals = false;
}

void apusys_dump_exit(void)
{
	debugfs_remove_recursive(debug_node);
	iounmap(apu_top);
	iounmap(apu_to_infra_top);
}
