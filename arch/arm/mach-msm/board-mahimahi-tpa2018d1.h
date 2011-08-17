/* include/linux/tpa2018d1.h - tpa2018d1 speaker amplifier driver
 *
 * Copyright (C) 2009 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#ifndef __ASM_ARM_ARCH_TPA2018D1_H
#define __ASM_ARM_ARCH_TPA2018D1_H

#define TPA2018D1_I2C_NAME "tpa2018d1"
#define TPA2018D1_CMD_LEN 8

struct tpa2018d1_platform_data {
	uint32_t gpio_tpa2018_spk_en;
};

struct tpa2018d1_config_data {
	unsigned char *cmd_data;  /* [mode][cmd_len][cmds..] */
	unsigned int mode_num;
	unsigned int data_len;
};

extern void tpa2018d1_set_speaker_amp(int on);

#endif /* __ASM_ARM_ARCH_TPA2018D1_H */
