/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _PLATFORM_MT6893_POS_H_
#define _PLATFORM_MT6893_POS_H_


unsigned int consys_emi_set_remapping_reg(phys_addr_t, phys_addr_t);

int consys_conninfra_on_power_ctrl(unsigned int enable);
int consys_conninfra_wakeup(void);
int consys_conninfra_sleep(void);
void consys_set_if_pinmux(unsigned int enable);
int consys_polling_chipid(void);

int connsys_d_die_cfg(void);
int connsys_spi_master_cfg(unsigned int);
int connsys_a_die_cfg(void);
int connsys_afe_wbg_cal(void);
int connsys_subsys_pll_initial(void);
int connsys_low_power_setting(unsigned int, unsigned int);

int consys_sema_acquire_timeout(unsigned int index, unsigned int usec);
void consys_sema_release(unsigned int index);

int consys_spi_read(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int *data);
int consys_spi_write(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int data);
int consys_spi_write_offset_range(
	enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int value,
	unsigned int reg_offset, unsigned int value_offset, unsigned int size);

int consys_adie_top_ck_en_on(enum consys_adie_ctl_type type);
int consys_adie_top_ck_en_off(enum consys_adie_ctl_type type);

int consys_spi_clock_switch(enum connsys_spi_speed_type type);
int consys_subsys_status_update(bool, int);
bool consys_is_rc_mode_enable(void);

void consys_config_setup(void);

#endif				/* _PLATFORM_MT6893_POS_H_ */
