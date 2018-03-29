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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccci_common.h
 *
 * Project:
 * --------
 *
 * Description:
 * ------------
 *
 * Author:
 * -------
 *
 ****************************************************************************/

#ifndef __CCCI_COMMON_H__
#define __CCCI_COMMON_H__
#include "ccci_cfg.h"
#include "ccci_err_no.h"
#include "ccci_md.h"
#include "ccci_layer.h"
#include "ccci_rpc.h"
#include "ccci_ipc.h"
#include "ccci_fs.h"
#include "ccmni_net.h"
#include <ccci_platform_cfg.h>
#include <mt-plat/mt_ccci_common.h>
/* ======================================================== */
/*  debug log define     */
/* ======================================================== */
#define CCCI_MSG(fmt, args...)        pr_warn("[com] (0)" fmt, ##args)
#define CCCI_MSG_INF(idx, tag, fmt, args...)    pr_warn("[" tag "] (%d)" fmt, (idx+1), ##args)
#define CCCI_DBG_MSG(idx, tag, fmt, args...)    pr_debug("[" tag "] (%d)" fmt, (idx+1), ##args)
#define CCCI_DBG_COM_MSG(fmt, args...)        pr_warn("[com] (0)" fmt, ##args)
#define CCCI_ERR(fmt, args...)        pr_err("[err] (0)" fmt, ##args)
#define CCCI_ERR_INF(idx, tag, fmt, args...)    pr_err("[" tag "] (%d)" fmt, (idx+1), ##args)

/*---------------------------Switchable log--------------------------------*/
/* Debug message switch */
#define CCCI_DBG_NONE        (0x00000000)	/* No debug log */
#define CCCI_DBG_CTL        (0x00000001)	/* Control log */
#define CCCI_DBG_TTY        (0x00000002)	/* TTY channel log */
#define CCCI_DBG_FS            (0x00000004)	/* FS channel log */
#define CCCI_DBG_RPC        (0x00000008)	/* RPC channel log */
#define CCCI_DBG_IPC        (0x00000010)	/* IPC channel log */
#define CCCI_DBG_PMIC        (0x00000020)	/* PMIC channel log */
#define CCCI_DBG_CCMNI        (0x00000040)	/* CCMIN channel log */
#define CCCI_DBG_FUNC        (0x00000080)	/* Functiong entry log */
#define CCCI_DBG_MISC        (0x00000100)	/* Misc log */
#define CCCI_DBG_CHR        (0x00000200)	/* Char dev log */
#define CCCI_DBG_CCIF        (0x00000400)	/* Ccif log */
#define CCCI_DBG_ALL        (0xffffffff)

#define ENABLE_ALL_RX_LOG    (1ULL<<63)

/*---------------------------------------------------------------------------*/
/* Switchable messages */
extern unsigned int ccci_msg_mask[];

#ifdef USING_PRINTK_LOG

#define CCCI_FILTER_MSG(mask, fmt, args...) \
	do {    \
		if (CCCI_DBG_##mask & ccci_msg_mask) \
			pr_debug("[ccci]" fmt, ##args); \
	} while (0)

#define CCCI_CTL_MSG(fmt, args...)		CCCI_FILTER_MSG(CTL, "<ctl>"fmt, ##args)
#define CCCI_TTY_MSG(fmt, args...)		CCCI_FILTER_MSG(TTY, "<tty>"fmt, ##args)
#define CCCI_FS_MSG(fmt, args...)		CCCI_FILTER_MSG(FS, "<fs>"fmt, ##args)
#define CCCI_RPC_MSG(fmt, args...)		CCCI_FILTER_MSG(RPC, "<rpc>"fmt, ##args)
#define CCCI_IPC_MSG(fmt, args...)		CCCI_FILTER_MSG(IPC, "<ipc>"fmt, ##args)
#define CCCI_PMIC_MSG(fmt, args...)		CCCI_FILTER_MSG(PMIC, "<pmic>"fmt, ##args)
#define CCCI_FUNC_ENTRY(f)				CCCI_FILTER_MSG(FUNC, "%s\n", __func__)
#define CCCI_MISC_MSG(fmt, args...)		CCCI_FILTER_MSG(MISC, fmt, ##args)
#define CCCI_CHR_MSG(fmt, args...)		CCCI_FILTER_MSG(CHR, "<chr>"fmt, ##args)
#define CCCI_CCIF_MSG(fmt, args...)		CCCI_FILTER_MSG(CCIF, "<chr>"fmt, ##args)
#define CCCI_CCMNI_MSG(fmt, args...)	CCCI_FILTER_MSG(CCMNI, "<ccmni>"fmt, ##args)

#else
#define CCCI_FILTER_MSG(mask, tag, idx, fmt, args...) \
	do {    \
		if (CCCI_DBG_##mask & ccci_msg_mask[idx]) \
			CCCI_MSG_INF(idx, tag, fmt, ##args); \
	} while (0)

#define CCCI_CTL_MSG(idx, fmt, args...)		CCCI_FILTER_MSG(CTL, "/ctl", idx, fmt, ##args)
#define CCCI_TTY_MSG(idx, fmt, args...)		CCCI_FILTER_MSG(TTY, "/tty", idx, fmt, ##args)
#define CCCI_FS_MSG(idx, fmt, args...)		CCCI_FILTER_MSG(FS, "/fs ", idx, fmt, ##args)
#define CCCI_RPC_MSG(idx, fmt, args...)		CCCI_FILTER_MSG(RPC, "/rpc", idx, fmt, ##args)
#define CCCI_IPC_MSG(idx, fmt, args...)		CCCI_FILTER_MSG(IPC, "/ipc", idx, fmt, ##args)
#define CCCI_PMIC_MSG(idx, fmt, args...)	CCCI_FILTER_MSG(PMIC, "/pmc", idx, fmt, ##args)
#define CCCI_FUNC_ENTRY(idx)				CCCI_FILTER_MSG(FUNC, "/fun", idx, "%s\n", __func__)
#define CCCI_MISC_MSG(idx, fmt, args...)	CCCI_FILTER_MSG(MISC, "/mis", idx, fmt, ##args)
#define CCCI_CHR_MSG(idx, fmt, args...)		CCCI_FILTER_MSG(CHR, "/chr", idx, fmt, ##args)
#define CCCI_CCIF_MSG(idx, fmt, args...)	CCCI_FILTER_MSG(CCIF, "/cci", idx, fmt, ##args)
#define CCCI_CCMNI_MSG(idx, fmt, args...)	CCCI_FILTER_MSG(CCMNI, "/net", idx, fmt, ##args)
#endif

/* ============================================================ */
/*  AEE function and macro define   */
/* ============================================================ */
#define CCCI_AED_DUMP_EX_MEM		(1<<0)
#define CCCI_AED_DUMP_MD_IMG_MEM	(1<<1)
#define CCCI_AED_DUMP_CCIF_REG		(1<<2)
void ccci_aed(int, unsigned int, char *);
/* ============================================================ */
/*  ccci related macro and structure define   */
/* ============================================================ */
#define CAN_BE_RELOAD		(0x1<<1)
#define LOAD_ONE_TIME		(0x1<<0)
#define LOAD_ALL_IMG		(LOAD_ONE_TIME|CAN_BE_RELOAD)
#define RELOAD_ONLY			(CAN_BE_RELOAD)

#define CCCI_LOG_TX 0
#define CCCI_LOG_RX 1

#define DBG_FLAG_DEBUG		(1<<0)
#define DBG_FLAG_JTAG		(1<<1)

enum {
	MD_DEBUG_REL_INFO_NOT_READY = 0,
	MD_IS_DEBUG_VERSION,
	MD_IS_RELEASE_VERSION
};

enum ccif_type_t {
	CCIF_STD_V1 = 0,	/*  16 channel ccif, tx 8, rx 8 */
	CCIF_VIR,			/*  Virtual CCIF type */
};

struct ccif_hw_info_t {
	unsigned long reg_base;
	unsigned long md_reg_base;
	unsigned int irq_id;
	unsigned int irq_attr;
	enum ccif_type_t type;
	unsigned int md_id;
};

struct rpc_cfg_inf_t {
	int rpc_ch_num;
	int rpc_max_buf_size;
};

/* ============================================================ */
/*  share memory layout define   */
/* ============================================================ */
/* share memory table */
struct smem_alloc_t {
	/*  Share memory */
	unsigned int ccci_smem_size;
	unsigned int ccci_smem_vir;
	unsigned int ccci_smem_phy;
	/*  -- Log */
	unsigned int ccci_mdlog_smem_base_virt;
	unsigned int ccci_mdlog_smem_base_phy;
	unsigned int ccci_mdlog_smem_size;
	/*  -- PCM */
	unsigned int ccci_pcm_smem_base_virt;
	unsigned int ccci_pcm_smem_base_phy;
	unsigned int ccci_pcm_smem_size;
	/*  -- PMIC */
	unsigned int ccci_pmic_smem_base_virt;
	unsigned int ccci_pmic_smem_base_phy;
	unsigned int ccci_pmic_smem_size;
	/*  -- FS */
	unsigned int ccci_fs_smem_base_virt;
	unsigned int ccci_fs_smem_base_phy;
	unsigned int ccci_fs_smem_size;
	/*  -- RPC */
	unsigned int ccci_rpc_smem_base_virt;
	unsigned int ccci_rpc_smem_base_phy;
	unsigned int ccci_rpc_smem_size;
	/*  -- TTY */
	unsigned int ccci_uart_smem_base_virt[CCCI_UART_PORT_NUM];
	unsigned int ccci_uart_smem_base_phy[CCCI_UART_PORT_NUM];
	unsigned int ccci_uart_smem_size[CCCI_UART_PORT_NUM];
	/*  -- Exception */
	unsigned int ccci_exp_smem_base_virt;
	unsigned int ccci_exp_smem_base_phy;
	unsigned int ccci_exp_smem_size;
	/*  -- IPC */
	unsigned int ccci_ipc_smem_base_virt;
	unsigned int ccci_ipc_smem_base_phy;
	unsigned int ccci_ipc_smem_size;
	/*  -- SYS - Eint exchagne */
	unsigned int ccci_sys_smem_base_virt;
	unsigned int ccci_sys_smem_base_phy;
	unsigned int ccci_sys_smem_size;
	/*  -- CCMNI new version */
	/*  ----- Up-link */
	unsigned int ccci_ccmni_smem_ul_base_virt;
	unsigned int ccci_ccmni_smem_ul_base_phy;
	unsigned int ccci_ccmni_smem_ul_size;
	/*  ----- Donw-link */
	unsigned int ccci_ccmni_smem_dl_base_virt;
	unsigned int ccci_ccmni_smem_dl_base_phy;
	unsigned int ccci_ccmni_smem_dl_size;
	unsigned int ccci_ccmni_ctl_smem_base_virt[NET_PORT_NUM];
	unsigned int ccci_ccmni_ctl_smem_base_phy[NET_PORT_NUM];
	unsigned int ccci_ccmni_ctl_smem_size[NET_PORT_NUM];
	/*  -- EXT MD Exception */
	unsigned int ccci_md_ex_exp_info_smem_base_virt;
	unsigned int ccci_md_ex_exp_info_smem_base_phy;
	unsigned int ccci_md_ex_exp_info_smem_size;
	/*  -- MD Runtime Data */
	unsigned int ccci_md_runtime_data_smem_base_virt;
	unsigned int ccci_md_runtime_data_smem_base_phy;
	unsigned int ccci_md_runtime_data_smem_size;
	/*  -- Misc Info Data */
	unsigned int ccci_misc_info_base_virt;
	unsigned int ccci_misc_info_base_phy;
	unsigned int ccci_misc_info_size;
};

/*  Memory layout table */
struct ccci_mem_layout_t {
	/*  MD image */
	unsigned int md_region_vir;
	unsigned int md_region_phy;
	unsigned int md_region_size;
	/*  DSP image */
	unsigned int dsp_region_vir;
	unsigned int dsp_region_phy;
	unsigned int dsp_region_size;
	/*  Share memory */
	unsigned int smem_region_vir;
	unsigned int smem_region_phy;
	unsigned int smem_region_size;
	unsigned int smem_region_phy_before_map;
};

/*  Misc info structure */
struct misc_info_t {
	unsigned int prefix;	/*  "CCIF" */
	unsigned int support_mask;
	unsigned int index;
	unsigned int next;
	unsigned int feature_0_val[4];
	unsigned int feature_1_val[4];
	unsigned int feature_2_val[4];
	unsigned int feature_3_val[4];
	unsigned int feature_4_val[4];
	unsigned int feature_5_val[4];
	unsigned int feature_6_val[4];
	unsigned int feature_7_val[4];
	unsigned int feature_8_val[4];
	unsigned int feature_9_val[4];
	unsigned int feature_10_val[4];
	unsigned int feature_11_val[4];
	unsigned int feature_12_val[4];
	unsigned int feature_13_val[4];
	unsigned int feature_14_val[4];
	unsigned int feature_15_val[4];
	unsigned int reserved_2[3];
	unsigned int postfix;	/*  "CCIF" */
};

enum misc_feature_sta_t {
	FEATURE_NOT_EXIST = 0,
	FEATURE_NOT_SUPPORT,
	FEATURE_SUPPORT,
	FEATURE_PARTIALLY_SUPPORT,
};

enum misc_feature_id_t {
	MISC_DMA_ADDR = 0,
	MISC_32K_LESS,
	MISC_RAND_SEED,
	MISC_MD_COCLK_SETTING,
	MISC_MD_SBP_SETTING,
};

/* ========================================================== */
/*  API need implemented by ccci platform */
/* ========================================================== */
int get_dev_major_for_md_sys(int md_id);
int get_ccif_hw_info(int md_id, struct ccif_hw_info_t *ccif_hw_info);
void md_env_setup_before_boot(int md_id);
void md_env_setup_before_ready(int md_id);
void md_boot_up_additional_operation(int md_id);
void md_boot_ready_additional_operation(int md_id);
void additional_operation_before_stop_md(int md_id);
struct smem_alloc_t *get_md_smem_layout(int md_id);
unsigned int get_md_sys_max_num(void);
void ccci_md_wdt_notify_register(int, int (*funcp) (int));
int ccci_power_on_md(int md_id);
int ccci_power_down_md(int md_id);
int let_md_stop(int md_id, unsigned int timeout);
int let_md_go(int md_id);
int ccci_get_sub_module_cfg(int md_id, char name[], char out_buf[], int size);
int ccci_alloc_smem(int md_id);
void ccci_free_smem(int md_id);
struct ccci_mem_layout_t *get_md_sys_layout(int md_id);
int is_modem_debug_ver(int md_id);
char *get_md_info_str(int md_id);
void platform_set_runtime_data(int md_id, struct modem_runtime_t *runtime);
void config_misc_info(int md_id, unsigned int base[], unsigned int size);
void send_battery_info(int md_id);
#ifdef CONFIG_MTK_ICUSB_SUPPORT
void send_icusb_notify(int md_id, unsigned int sim_id);
#endif
void md_fast_dormancy(int md_id);
void start_md_wdt_recov_timer(int md_id);
int platform_init(int md_id, int power_down);
void platform_deinit(int md_id);
unsigned int get_debug_mode_flag(void);
int ccci_ipo_h_platform_restore(int md_id);
int set_sim_type(int md_id, int data);
int get_sim_type(int md_id, int *p_sim_type);
int enable_get_sim_type(int md_id, unsigned int enable);
void ccci_dump_md_register(int md_id);
/* Generally, AP and MD has same share memory address after hw remapp.
 * however, if hardware remapp does not work, then need software remap,
 *This variable is used to fix md phy addr does not equeal with AP.
 * If hardware remap works, then the variable is 0.
 */
int get_md2_ap_phy_addr_fixed(void);

/* API export by ccci_misc.c that moved from mtk_ccci_helper.c */
void ccci_helper_exit(void);

#define MD1_SETTING_ACTIVE	(1<<0)
#define MD2_SETTING_ACTIVE	(1<<1)
#define MD5_SETTING_ACTIVE	(1<<4)

#define MD_2G_FLAG    (1<<0)
#define MD_FDD_FLAG   (1<<1)
#define MD_TDD_FLAG   (1<<2)
#define MD_LTE_FLAG   (1<<3)
#define MD_SGLTE_FLAG (1<<4)

#define MD_WG_FLAG    (MD_FDD_FLAG|MD_2G_FLAG)
#define MD_TG_FLAG    (MD_TDD_FLAG|MD_2G_FLAG)
#define MD_LWG_FLAG   (MD_LTE_FLAG|MD_FDD_FLAG|MD_2G_FLAG)
#define MD_LTG_FLAG   (MD_LTE_FLAG|MD_TDD_FLAG|MD_2G_FLAG)

/*-------------other configure-------------------------*/
#define MAX_SLEEP_API	  (20)
#define MAX_FILTER_MEMBER (4)

/*-------------error code define-----------------------*/
#define E_NO_EXIST		  (-1)
#define E_PARAM			  (-2)

#define CCCI_MEM_ALIGN      (SZ_32M)
#define CCCI_SMEM_ALIGN_MD1 (0x200000)	/*2M */

/*-------------enum define---------------------------*/
/*modem image version definitions*/
typedef enum {
	AP_IMG_INVALID = 0,
	AP_IMG_2G,
	AP_IMG_3G
} AP_IMG_TYPE;

typedef enum {
	RSM_ID_RESUME_WDT_IRQ = 0,
	RSM_ID_MD_LOCK_DORMANT = 1,
	RSM_ID_WAKE_UP_MD = 2,
	RSM_ID_MAX
} RESUME_ID;

typedef enum {
	SLP_ID_MD_FAST_DROMANT = 0,
	SLP_ID_MD_UNLOCK_DORMANT = 1,
	SLP_ID_MAX
} SLEEP_ID;

/*System channel, AP -->(/ <-->) MD message start from 0x100*/
enum {
	MD_DORMANT_NOTIFY = 0x100,
	MD_SLP_REQUEST = 0x101,
	MD_TX_POWER = 0x102,
	MD_RF_TEMPERATURE = 0x103,
	MD_RF_TEMPERATURE_3G = 0x104,
	MD_GET_BATTERY_INFO = 0x105,
	MD_SIM_TYPE = 0x107,	/*for regional phone boot animation */
	MD_SW_2G_TX_POWER = 0x10E,
	MD_SW_3G_TX_POWER = 0x10F,
};

/*-------------structure define------------------------*/
typedef int (*ccci_kern_cb_func_t)(int, char *, unsigned int);
typedef struct {
	KERN_FUNC_ID id;
	ccci_kern_cb_func_t func;
} ccci_kern_func_info;

typedef size_t(*ccci_filter_cb_func_t)(char *, size_t);
typedef struct _cmd_op_map {
	char cmd[8];
	int cmd_len;
	ccci_filter_cb_func_t store;
	ccci_filter_cb_func_t show;
} cmd_op_map_t;

/*-----------------export function declaration----------------------------*/
AP_IMG_TYPE get_ap_img_ver(void);
int get_td_eint_info(int md_id, char *eint_name, unsigned int len);
int get_md_gpio_info(int md_id, char *gpio_name, unsigned int len);
int get_md_gpio_val(int md_id, unsigned int num);
int get_md_adc_info(int md_id, char *adc_name, unsigned int len);
int get_md_adc_val(int md_id, unsigned int num);
int get_dram_type_clk(int *clk, int *type);
int get_eint_attr(char *name, unsigned int name_len, unsigned int type,
		  char *result, unsigned int *len);
int get_bat_info(unsigned int para);

unsigned int get_nr_modem(void);
unsigned int *get_modem_size_list(void);
int parse_ccci_dfo_setting(void *dfo_data, int num);
int parse_meta_md_setting(unsigned char args[]);

unsigned int get_md_mem_start_addr(int md_id);
unsigned int get_md_share_mem_start_addr(int md_id);
unsigned int get_smem_base_addr(int md_id);
unsigned int get_modem_is_enabled(int md_id);
unsigned int get_resv_mem_size_for_md(int md_id);
unsigned int get_resv_share_mem_size_for_md(int md_id);
void get_md_post_fix(int md_id, char buf[], char buf_ex[]);
unsigned int get_modem_support(int md_id);
unsigned int set_modem_support(int md_id, int md_type);

int register_filter_func(char cmd[], ccci_filter_cb_func_t store,
			 ccci_filter_cb_func_t show);

int register_ccci_kern_func(unsigned int id, ccci_kern_cb_func_t func);
int register_ccci_kern_func_by_md_id(int md_id, unsigned int id,
				     ccci_kern_cb_func_t func);
int exec_ccci_kern_func(unsigned int id, char *buf, unsigned int len);
int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf,
				 unsigned int len);

void register_suspend_notify(int md_id, unsigned int id, void (*func)(int));
void register_resume_notify(int md_id, unsigned int id, void (*func)(int));

int register_sys_msg_notify_func(int md_id,
				 int (*func)(int, unsigned int, unsigned int));
int notify_md_by_sys_msg(int md_id, unsigned int msg, unsigned int data);

int register_ccci_sys_call_back(int md_id, unsigned int id,
				ccci_sys_cb_func_t func);
void exec_ccci_sys_call_back(int md_id, int cb_id, int data);
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
extern unsigned long *get_modem_start_addr_list(void);
extern int IMM_get_adc_channel_num(char *channel_name, int len);
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int get_dram_info(int *clk, int *type);
void ccci_helper_exit(void);
void ccci_md_mem_reserve(void);

extern int legacy_boot_md_show(int md_id, char *buf, int size);
extern int legacy_boot_md_store(int md_id);

int ccci_load_firmware_helper(int md_id, char img_err_str[], int len);/* Platform code export */

#ifdef ENABLE_GPS_MD_COCLK
extern unsigned int wmt_get_coclock_setting_for_ccci(void);
#endif
#endif				/* __CCCI_COMMON_H__ */
