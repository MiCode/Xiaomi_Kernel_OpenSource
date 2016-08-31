/*
 * arch/arm/mach-tegra/tegra_apb2jtag.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <asm/io.h>
#include "iomap.h"
#include "tegra_apb2jtag.h"

#define APB2JTAG_BASE				IO_ADDRESS(0x700e1000)
#define APB2JTAG_ACCESS_CTRL_0			(APB2JTAG_BASE + 0x0)
#define APB2JTAG_ACCESS_DATA_0			(APB2JTAG_BASE + 0x4)
#define APB2JTAG_ACCESS_CONFIG_0		(APB2JTAG_BASE + 0x8)
#define APB2JTAG_ACCESS_CTRL_0_INSTR_ID_LSB	0
#define APB2JTAG_ACCESS_CTRL_0_INSTR_ID_MSB	7
#define APB2JTAG_ACCESS_CTRL_0_REG_LEN_LSB	8
#define APB2JTAG_ACCESS_CTRL_0_REG_LEN_MSB	18
#define APB2JTAG_ACCESS_CTRL_0_DWORD_EN_LSB	19
#define APB2JTAG_ACCESS_CTRL_0_DWORD_EN_MSB	24
#define APB2JTAG_ACCESS_CTRL_0_CHIP_SEL_LSB	25
#define APB2JTAG_ACCESS_CTRL_0_CHIP_SEL_MSB	29
#define APB2JTAG_ACCESS_CTRL_0_STATUS_LSB	30
#define APB2JTAG_ACCESS_CTRL_0_STATUS_MSB	30
#define APB2JTAG_ACCESS_CTRL_0_REQ_CTRL_LSB	31
#define APB2JTAG_ACCESS_CTRL_0_REQ_CTRL_MSB	31
#define APB2JTAG_ACCESS_CONFIG_0_REG_LEN_MSB_LSB	0
#define APB2JTAG_ACCESS_CONFIG_0_REG_LEN_MSB_MSB	7
#define APB2JTAG_ACCESS_CONFIG_0_DWORD_EN_LSB		8
#define APB2JTAG_ACCESS_CONFIG_0_DWORD_EN_MSB		15

static DEFINE_MUTEX(apb2jtag_lock);

void apb2jtag_get(void)
{
	mutex_lock(&apb2jtag_lock);
}

void apb2jtag_put(void)
{
	mutex_unlock(&apb2jtag_lock);
}

static u32 set_bits(u32 cur_val, u32 new_val, u32 lsb, u32 msb)
{
	u32 mask = 0xFFFFFFFF;
	u32 num_bits = msb - lsb + 1;
	if (num_bits + lsb > 32)
		return 0;
	if (num_bits < 32)
		mask = (1 << num_bits) - 1;
	return (cur_val & ~(mask << lsb)) | ((new_val & mask) << lsb);
}

int apb2jtag_read_locked(u32 instr_id, u32 len, u32 chiplet, u32 *buf)
{
	u32 i, reg, len_msb, len_lsb;
	u32 reg_len_msb, reg_len_lsb;
	u32 count = 0;
	int ret = 0;
	reg_len_msb = (len + 3) >> 11;
	reg_len_lsb = (len + 3) & 0x7FF;

	/* Read 32 bits at a time */
	for (i = 0; i * 32 < len; i++) {
		len_lsb = i & 0x3F;
		len_msb = i >> 6;
		reg = readl(APB2JTAG_ACCESS_CONFIG_0);
		reg = set_bits(reg, reg_len_msb,
				   APB2JTAG_ACCESS_CONFIG_0_REG_LEN_MSB_LSB,
				   APB2JTAG_ACCESS_CONFIG_0_REG_LEN_MSB_MSB);
		reg = set_bits(reg, len_msb,
				   APB2JTAG_ACCESS_CONFIG_0_DWORD_EN_LSB,
				   APB2JTAG_ACCESS_CONFIG_0_DWORD_EN_MSB);
		writel(reg, APB2JTAG_ACCESS_CONFIG_0);
		writel(0, APB2JTAG_ACCESS_CTRL_0);
		reg = set_bits(0, instr_id,
				   APB2JTAG_ACCESS_CTRL_0_INSTR_ID_LSB,
				   APB2JTAG_ACCESS_CTRL_0_INSTR_ID_MSB);
		reg = set_bits(reg, reg_len_lsb,
				   APB2JTAG_ACCESS_CTRL_0_REG_LEN_LSB,
				   APB2JTAG_ACCESS_CTRL_0_REG_LEN_MSB);
		reg = set_bits(reg, len_lsb,
				   APB2JTAG_ACCESS_CTRL_0_DWORD_EN_LSB,
				   APB2JTAG_ACCESS_CTRL_0_DWORD_EN_MSB);
		reg = set_bits(reg, chiplet,
				   APB2JTAG_ACCESS_CTRL_0_CHIP_SEL_LSB,
				   APB2JTAG_ACCESS_CTRL_0_CHIP_SEL_MSB);
		reg = set_bits(reg, 1,
				   APB2JTAG_ACCESS_CTRL_0_REQ_CTRL_LSB,
				   APB2JTAG_ACCESS_CTRL_0_REQ_CTRL_MSB);
		writel(reg, APB2JTAG_ACCESS_CTRL_0);

		/* Wait for data */
		count = 0;
		reg = 0;
		while (count < 100 && !reg) {
			reg = readl(APB2JTAG_ACCESS_CTRL_0);
			reg = (reg >> APB2JTAG_ACCESS_CTRL_0_STATUS_LSB) & 0x1;
			count++;
		}
		if (!reg) {
			ret = -ENODATA;
			goto err;
		}
		buf[i] = readl(APB2JTAG_ACCESS_DATA_0);
		if (((i + 1) * 32 >= len) && ((len - 1) % 32 < 27)) {
			buf[i] = buf[i] & 0x0FFFFFFF;
			buf[i] = buf[i] >> (32 - ((len + 4) % 32));
		}
	}
err:
	reg = readl(APB2JTAG_ACCESS_CTRL_0);
	reg = set_bits(reg, 0, APB2JTAG_ACCESS_CTRL_0_REQ_CTRL_LSB,
			   APB2JTAG_ACCESS_CTRL_0_REQ_CTRL_MSB);
	writel(reg, APB2JTAG_ACCESS_CTRL_0);

	return ret;
}

int apb2jtag_read(u32 instr_id, u32 len, u32 chiplet, u32 *buf)
{
	int ret;

	apb2jtag_get();
	ret = apb2jtag_read_locked(instr_id, len, chiplet, buf);
	apb2jtag_put();

	return ret;
}

int apb2jtag_write_locked(u32 instr_id, u32 len, u32 chiplet, const u32 *buf)
{
	u32 reg, reg_len_msb, reg_len_lsb;
	u32 count = 0;
	int ret = 0;

	reg_len_msb = (len - 1) >> 11;
	reg_len_lsb = (len - 1) & 0x7FF;
	reg = 0;

	reg = readl(APB2JTAG_ACCESS_CONFIG_0);
	reg = set_bits(reg, reg_len_msb,
			APB2JTAG_ACCESS_CONFIG_0_REG_LEN_MSB_LSB,
			APB2JTAG_ACCESS_CONFIG_0_REG_LEN_MSB_MSB);
	reg = set_bits(reg, 0,
			APB2JTAG_ACCESS_CONFIG_0_DWORD_EN_LSB,
			APB2JTAG_ACCESS_CONFIG_0_DWORD_EN_MSB);
	writel(reg, APB2JTAG_ACCESS_CONFIG_0);
	writel(0, APB2JTAG_ACCESS_CTRL_0);
	reg = set_bits(0, instr_id,
			APB2JTAG_ACCESS_CTRL_0_INSTR_ID_LSB,
			APB2JTAG_ACCESS_CTRL_0_INSTR_ID_MSB);
	reg = set_bits(reg, reg_len_lsb,
			APB2JTAG_ACCESS_CTRL_0_REG_LEN_LSB,
			APB2JTAG_ACCESS_CTRL_0_REG_LEN_MSB);
	reg = set_bits(reg, 0,
			APB2JTAG_ACCESS_CTRL_0_DWORD_EN_LSB,
			APB2JTAG_ACCESS_CTRL_0_DWORD_EN_MSB);
	reg = set_bits(reg, chiplet,
			APB2JTAG_ACCESS_CTRL_0_CHIP_SEL_LSB,
			APB2JTAG_ACCESS_CTRL_0_CHIP_SEL_MSB);
	reg = set_bits(reg, 1,
			APB2JTAG_ACCESS_CTRL_0_REQ_CTRL_LSB,
			APB2JTAG_ACCESS_CTRL_0_REQ_CTRL_MSB);
	writel(reg, APB2JTAG_ACCESS_CTRL_0);

	/* Wait for data */
	count = 0;
	reg = 0;
	while (count < 100 && !reg) {
		reg = readl(APB2JTAG_ACCESS_CTRL_0);
		reg = (reg >> APB2JTAG_ACCESS_CTRL_0_STATUS_LSB) & 0x1;
		count++;
	}
	if (!reg) {
		ret = -ENODATA;
		goto err;
	}
	count = 0;
	while (count * 32 < len) {
		writel(buf[count], APB2JTAG_ACCESS_DATA_0);
		count++;
	}
err:
	reg = readl(APB2JTAG_ACCESS_CTRL_0);
	reg = set_bits(reg, 0, APB2JTAG_ACCESS_CTRL_0_REQ_CTRL_LSB,
			   APB2JTAG_ACCESS_CTRL_0_REQ_CTRL_MSB);
	writel(reg, APB2JTAG_ACCESS_CTRL_0);

	return ret;
}

int apb2jtag_write(u32 instr_id, u32 len, u32 chiplet, const u32 *buf)
{
	int ret;

	apb2jtag_get();
	ret = apb2jtag_write_locked(instr_id, len, chiplet, buf);
	apb2jtag_put();

	return ret;
}
