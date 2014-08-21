/*
 * Intel Baytrail PWM driver.
 *
 * Copyright (C) 2013 Intel corporation.
 *
 * ----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define PWM_BYT_CLK_KHZ	25000
#define PWM_CHT_CLK_KHZ	19200
int pwm_byt_init(struct device *pdev, void __iomem *base,
		int pwm_num, unsigned int clk_khz);
void pwm_byt_remove(struct device *dev);
extern const struct dev_pm_ops pwm_byt_pm;
