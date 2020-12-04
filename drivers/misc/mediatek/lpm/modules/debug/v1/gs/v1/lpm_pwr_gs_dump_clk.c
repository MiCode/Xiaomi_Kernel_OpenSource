// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <gs/lpm_pwr_gs.h>
#include <gs/v1/lpm_power_gs.h>
#include <linux/io.h>
#define LPM_GS_CMP_SEARCH		(1u << 0)
struct lpm_gs_dcm_info_inst {
	struct lpm_gs_clk_info *info;
};
static struct base_remap br;
#define DEBUG_BUF_SIZE 2048
static char buf[DEBUG_BUF_SIZE] = { 0 };
static struct lpm_gs_dcm_info_inst lpm_gs_clks;
void __iomem *_get_virt_base_from_table(unsigned int pa)
{
	unsigned int k;
	void __iomem *io_base = 0;

	if (br.table) {
		for (k = 0; k < br.table_pos; k++)
			if (br.table[k].pa == pa)
				return (io_base = br.table[k].va);
	} else
		pr_info("Power_gs: cannot find virtual address\n");
	return io_base;
}
/* Check phys addr is existed in table or not */
bool _is_exist_in_phys_to_virt_table(unsigned int pa)
{
	unsigned int k;

	if (br.table)
		for (k = 0; k < br.table_pos; k++)
			if (br.table[k].pa == pa)
				return true;
	return false;
}
unsigned int mt_power_gs_base_remap_init(char *scenario, char *pmic_name,
			 const unsigned int *pmic_gs, unsigned int pmic_gs_len)
{
	unsigned int i, base;
	struct phys_to_virt_table *table;

	if (!br.table)
		br.table = kmalloc(sizeof(struct phys_to_virt_table) *
				REMAP_SIZE_MASK + 1, GFP_KERNEL);
	if (br.table) {
		table = br.table;
		for (i = 0; i < pmic_gs_len; i += 3) {
			base = (pmic_gs[i] & (~(unsigned long)REMAP_SIZE_MASK));
			if (!_is_exist_in_phys_to_virt_table(base)) {
				table[br.table_pos].pa = base;
				table[br.table_pos].va =
					ioremap(base,
							REMAP_SIZE_MASK + 1);
				if (!table[br.table_pos].va)
					pr_info("ioremap(0x%x, 0x%x)\n",
						base, REMAP_SIZE_MASK + 1);
				if (br.table_pos < br.table_size)
					br.table_pos++;
				else {
					pr_info("base_remap in maximum size\n");
					return 0;
				}
			}
		}
	}
	return 0;
}
static u16 gs_pmic_read(u16 reg)
{
	u32 reg_val = 0;
#if IS_ENABLED(CONFIG_MTK_PMIC_NEW_ARCH)
	u32 ret = 0;

	ret = pmic_read_interface_nolock(reg, &reg_val, 0xFFFF, 0x0);
#endif
	return (u16)reg_val;
}
static bool _is_pmic_addr(unsigned int addr)
{
	return (addr >> 16) ? 0 : 1;
}
unsigned int _golden_read_reg(unsigned int addr)
{
	unsigned int reg_val;
	unsigned int base = addr & (~(unsigned long)REMAP_SIZE_MASK);
	unsigned int offset = addr & (unsigned long)REMAP_SIZE_MASK;
	void __iomem *io_base;

	if (_is_pmic_addr(addr))
		reg_val = gs_pmic_read(addr);
	else {
		io_base = _get_virt_base_from_table(base);
		if (io_base)
			reg_val = ioread32(io_base + offset);
		else
			reg_val = 0;
	}
	return reg_val;
}
#define PER_LINE_TO_PRINT 8
static void mt_power_gs_clk_compare(char *pmic_name,
				 struct lpm_gs_clk_user *user)
{
	unsigned int i, k, val0, val1, val2, diff, dump_cnt = 0;
	char *p;

	/* dump diff mode */
	p = buf;
	p += snprintf(p, sizeof(buf), "\n");
	p += snprintf(p, sizeof(buf) - (p - buf),
			"Scenario - %s - Addr       - Value      - Mask       - Golden     - Wrong Bit\n",
			pmic_name);
	for (i = 0; i < user->array_sz; i += 3) {
		val0 = _golden_read_reg(user->array[i]);
		val1 = val0 & user->array[i + 1];
		val2 = user->array[i + 2] & user->array[i + 1];
		if (val1 != val2) {
			dump_cnt++;
			p += snprintf(p, sizeof(buf) - (p - buf),
				"%s - %s - 0x%08x - 0x%08x - 0x%08x - 0x%08x -",
				user->name, pmic_name, user->array[i], val0,
				user->array[i + 1], user->array[i + 2]);
			for (k = 0, diff = val1 ^ val2; diff != 0; k++,
					diff >>= 1) {
				if ((diff % 2) != 0)
					p += snprintf(p,
						sizeof(buf) - (p - buf),
						" %d", k);
			}
			p += snprintf(p, sizeof(buf) - (p - buf), "\n");
			if (dump_cnt &&
				((dump_cnt % PER_LINE_TO_PRINT) == 0)) {
				pr_notice("%s", buf);
				p = buf;
				p += snprintf(p, sizeof(buf), "\n");
			}
		}
	}
	if (dump_cnt % PER_LINE_TO_PRINT)
		pr_notice("%s", buf);
}
int lpm_gs_clk_cmp_init(void *data)
{
	int i = 0, ret = 0, idx = 0;
	struct lpm_gs_clk_info *_info;
	struct lpm_gs_clk *const *_dcm;

	if (!data)
		return -EINVAL;
	_info = (struct lpm_gs_clk_info *)data;
	for (i = 0, _dcm = _info->dcm; *_dcm ; i++, _dcm++) {
		if (_info->attach) {
			ret = _info->attach((*_dcm));
			if (!ret) {
				for (idx = 0; idx < LPM_PWR_GS_TYPE_MAX;
				     ++idx) {
					mt_power_gs_base_remap_init(
						(*_dcm)->user[idx].name,
						(*_dcm)->name,
						(*_dcm)->user[idx].array,
						(*_dcm)->user[idx].array_sz);
				}
			}
		}
	}
	lpm_gs_clks.info = _info;
	return 0;
}
int __lpm_gs_clk_cmp(unsigned int flag, unsigned int type, int user)
{
	int i = 0, is_found;
	struct lpm_gs_clk_info *_info = lpm_gs_clks.info;
	struct lpm_gs_clk *const *_dcm;

	for (i = 0, _dcm = _info->dcm; *_dcm ; i++, _dcm++) {
		is_found = (flag & LPM_GS_CMP_SEARCH) ? 0 : 1;
		if (!is_found && ((*_dcm)->type == type))
			is_found = 1;
		if (is_found)
			mt_power_gs_clk_compare((*_dcm)->name,
					&((*_dcm)->user[user]));
	}
	return 0;
}
int lpm_gs_clk_cmp(int user)
{
	return __lpm_gs_clk_cmp(0, 0, user);
}
int lpm_gs_clk_cmp_by_type(int user, unsigned int type)
{
	return __lpm_gs_clk_cmp(LPM_GS_CMP_SEARCH, type, user);
}
struct lpm_gs_cmp lpm_gs_cmp_clk = {
	.cmp_init = lpm_gs_clk_cmp_init,
	.cmp = lpm_gs_clk_cmp,
	.cmp_by_type = lpm_gs_clk_cmp_by_type,
};
int lpm_gs_dump_dcm_init(void)
{
	lpm_pwr_gs_compare_register(LPM_GS_CMP_CLK, &lpm_gs_cmp_clk);
	return 0;
}
void lpm_gs_dump_dcm_deinit(void)
{
	lpm_pwr_gs_compare_unregister(LPM_GS_CMP_CLK);
}

