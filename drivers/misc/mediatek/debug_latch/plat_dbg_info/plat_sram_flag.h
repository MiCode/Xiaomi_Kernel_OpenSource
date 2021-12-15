/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __PLAT_SRAM_FLAG_H__
#define __PLAT_SRAM_FLAG_H__

/* each flag is a word */
struct plat_sram_flag {
	unsigned int plat_magic;
	unsigned int plat_sram_flag0;
	unsigned int plat_sram_flag1;
	unsigned int plat_sram_flag2;
};

#define DEF_PLAT_SRAM_FLAG PLAT_SRAM_FLAG_T

#define PLAT_SRAM_FLAG_KEY	0xDB45
/*
 * PLAT_FLAG0:
 * bit[0:0] = lastpc_valid,
 * bit[1:1] = lastpc_valid_before_reboot,
 * bit[2:4] = user_id_of_multi_user_etb_0,
 * bit[5:7] = user_id_of_multi_user_etb_1,
 * bit[8:10] = user_id_of_multi_user_etb_2,
 * bit[11:13] = user_id_of_multi_user_etb_3,
 * bit[14:16] = user_id_of_multi_user_etb_4,
 * bit[17:19] = user_id_of_multi_user_etb_5,
 * bit[20:22] = user_id_of_multi_user_etb_6,
 * bit[23:25] = user_id_of_multi_user_etb_7,
 * bit[26:28] = user_id_of_multi_user_etb_8,
 * bit[29:31] = user_id_of_multi_user_etb_9,
 */
#define OFFSET_LASTPC_VALID			0
#define OFFSET_LASTPC_VALID_BEFORE_REBOOT	1
#define OFFSET_ETB_0				2

/* available multi-user ETB number
 * (only count for ETB that supports multi-user)
 */
#define MAX_ETB_NUM		10
/* available user type is 0x0~0x7 */
#define MAX_ETB_USER_NUM	8


/*
 * PLAT_FLAG1:
 * bit[0:0] = dfd_valid,
 * bit[1:1] = dfd_before_reboot,
 */
#define OFFSET_DFD_VALID                     0
#define OFFSET_DFD_VALID_BEFORE_REBOOT       1

/*
 * PLAT_FLAG2:
 * bit[0:0] = base_address_for_dfd[32:32],
 * bit[1:31] = base_address_for_dfd[1:31],
 *
 * XXX: due to alignment, base_address_for_dfd[0:0] is always 0
 */


int set_sram_flag_lastpc_valid(void);
int set_sram_flag_dfd_valid(void);
int set_sram_flag_etb_user(unsigned int utb_id, unsigned int user_id);
int set_sram_flag_timestamp(void);

extern void __iomem *get_dbg_info_base(unsigned int key);
extern unsigned int get_dbg_info_size(unsigned int key);

#endif
