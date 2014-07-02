/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __I2C_QUP_H__
#define __I2C_QUP_H__
/**
 * msm_i2c_platform_data: i2c-qup driver configuration data
 *
 * @clk_ctl_xfer : When true, the clocks's state (prepare_enable/
 *       unprepare_disable) is controlled by i2c-transaction's begining and
 *       ending. When false, the clock's state is controlled by runtime-pm
 *       events.
 * @master_id master id number of the i2c core or its wrapper (BLSP/GSBI).
 *       When zero, clock path voting is disabled.
 * @noise_rjct_sda Number of low samples on data line to consider it low.
 *       Range of values is 0-3. When missing default to 0.
 * @noise_rjct_scl Number of low samples on clock line to consider it low.
 *       Range of values is 0-3. When missing default to 0.
 */
struct msm_i2c_platform_data {
        int clk_freq;
        bool clk_ctl_xfer;
        uint32_t rmutex;
        const char *rsl_id;
        uint32_t pm_lat;
        int pri_clk;
        int pri_dat;
        int aux_clk;
        int aux_dat;
        int src_clk_rate;
        int noise_rjct_sda;
        int noise_rjct_scl;
        int use_gsbi_shared_mode;
        int keep_ahb_clk_on;
        void (*msm_i2c_config_gpio)(int iface, int config_type);
        uint32_t master_id;
};

#ifdef CONFIG_I2C_MSM_QUP
int __init qup_i2c_init_driver(void);
#else
static inline int __init qup_i2c_init_driver(void) { return 0; }
#endif

#endif /* __I2C_QUP_H__ */
