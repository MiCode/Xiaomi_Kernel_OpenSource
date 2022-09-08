/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __EMI_H__
#define __EMI_H__

#include <linux/irqreturn.h>

#define MTK_EMIMPU_NO_PROTECTION	0
#define MTK_EMIMPU_SEC_RW		1
#define MTK_EMIMPU_SEC_RW_NSEC_R	2
#define MTK_EMIMPU_SEC_RW_NSEC_W	3
#define MTK_EMIMPU_SEC_R_NSEC_R		4
#define MTK_EMIMPU_FORBIDDEN		5
#define MTK_EMIMPU_SEC_R_NSEC_RW	6

#define MTK_EMIMPU_UNLOCK		false
#define MTK_EMIMPU_LOCK			true

#define MTK_EMIMPU_SET			0
#define MTK_EMIMPU_CLEAR		1
#define MTK_EMIMPU_READ			2
#define MTK_EMIMPU_SLVERR		3
#define MTK_EMIDBG_DUMP			4
#define MTK_EMIDBG_MSG			5
#define MTK_EMIMPU_CLEAR_MD             7

#define MTK_EMIMPU_READ_SA		0
#define MTK_EMIMPU_READ_EA		1
#define MTK_EMIMPU_READ_APC		2
#define MTK_EMIMPU_READ_ENABLE		3
#define MTK_EMIMPU_READ_AID		4

#define MTK_EMI_MAX_TOKEN		4
#define MTK_EMI_MAX_CMD_LEN		4096

#define EMIMPUVER1			1
#define EMIMPUVER2			2

struct emi_addr_map {
	int emi;
	int channel;
	int rank;
	int bank;
	int row;
	int column;
};

struct reg_info_t {
	unsigned int offset;
	unsigned int value;
	unsigned int leng;
};

struct emimpu_region_t {
	unsigned long long start;
	unsigned long long end;
	unsigned int rg_num;
	bool lock;
	unsigned int *apc;
};

typedef irqreturn_t (*emimpu_pre_handler)(
	unsigned int emi_id, struct reg_info_t *dump, unsigned int leng);
typedef void (*emimpu_post_clear)(unsigned int emi_id);
typedef void (*emimpu_md_handler)(const char *vio_msg);
typedef void (*emimpu_iommu_handler)(
	unsigned int emi_id, struct reg_info_t *dump, unsigned int leng);
typedef void (*emimpu_debug_dump)(void);

struct emimpu_dbg_cb {
	emimpu_debug_dump func;
	struct emimpu_dbg_cb *next_dbg_cb;
};

#if IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
struct emicen_dev_t {
	unsigned int emi_cen_cnt;
	unsigned int ch_cnt;
	unsigned int rk_cnt;
	unsigned long long *rk_size;
	void __iomem **emi_cen_base;
	void __iomem **emi_chn_base;
};

struct emimpu_dev_t {
	unsigned int region_cnt;
	unsigned int domain_cnt;
	unsigned int addr_align;
	unsigned long long dram_start;
	unsigned long long dram_end;
	unsigned int dump_cnt;
	unsigned int clear_reg_cnt;
	unsigned int clear_md_reg_cnt;
	unsigned int emi_cen_cnt;
	struct reg_info_t *dump_reg;
	struct reg_info_t *clear_reg;
	struct reg_info_t *clear_md_reg;
	void __iomem **emi_cen_base;
	void __iomem **emi_mpu_base;
	char *violation_msg;
	unsigned int in_msg_dump;
	unsigned int show_region;
	unsigned int ctrl_intf;
	struct emimpu_region_t *ap_rg_info;
	struct emimpu_dbg_cb *dbg_cb_list;
};

struct emiisu_dev_t {
	unsigned int buf_size;
	void __iomem *buf_addr;
	void __iomem *ver_addr;
	void __iomem *con_addr;
	struct dentry *dump_dir;
	struct dentry *dump_buf;
	unsigned int ctrl_intf;
};
#endif

/* mtk emicen api */
unsigned int mtk_emicen_get_ch_cnt(void);
unsigned int mtk_emicen_get_rk_cnt(void);
unsigned int mtk_emicen_get_rk_size(unsigned int rk_id);
int mtk_emicen_addr2dram(unsigned long addr, struct emi_addr_map *map);

/* mtk emidbg api */
void mtk_emidbg_dump(void);

/* mtk emimpu api */
int emimpu_ap_region_init(void);
int mtk_emimpu_init_region(
	struct emimpu_region_t *rg_info, unsigned int rg_num);
int mtk_emimpu_set_addr(struct emimpu_region_t *rg_info,
	unsigned long long start, unsigned long long end);
int mtk_emimpu_set_apc(struct emimpu_region_t *rg_info,
	unsigned int d_num, unsigned int apc);
int mtk_emimpu_lock_region(struct emimpu_region_t *rg_info, bool lock);
int mtk_emimpu_set_protection(struct emimpu_region_t *rg_info);
int mtk_emimpu_free_region(struct emimpu_region_t *rg_info);
int mtk_emimpu_clear_protection(struct emimpu_region_t *rg_info);

#if IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
int mtk_emimpu_prehandle_register(irqreturn_t (*bypass_func)
	(unsigned int emi_id, struct reg_info_t *dump, unsigned int leng));
int mtk_emimpu_postclear_register(void (*clear_func)
	(unsigned int emi_id));
int mtk_emimpu_md_handling_register(void (*md_handling_func)
	(unsigned int emi_id, struct reg_info_t *dump, unsigned int leng));
int mtk_emimpu_debugdump_register(void (*debug_func)(void));
int mtk_emimpu_register_callback(irqreturn_t (*debug_dump)
	(unsigned int emi_id, struct reg_info_t *dump, unsigned int len));
#else
int mtk_emimpu_prehandle_register(emimpu_pre_handler bypass_func);
int mtk_emimpu_postclear_register(emimpu_post_clear clear_func);
int mtk_emimpu_md_handling_register(emimpu_md_handler md_handling_func);
int mtk_emimpu_debugdump_register(emimpu_debug_dump debug_func);
int mtk_emimpu_iommu_handling_register(emimpu_iommu_handler iommu_handling_func);
void mtk_clear_md_violation(void);
#endif

#endif /* __EMI_H__ */
