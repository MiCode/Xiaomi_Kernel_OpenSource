// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.

#include <linux/mfd/mt6397/core.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/regmap.h>
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_ipi_pin.h>
#include <sspm_ipi.h>
#endif

/* Legacy PMIC SSPM/IPI driver interface */
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#if IS_ENABLED(CONFIG_MACH_MT6761)
/*|| IS_ENABLED(CONFIG_MACH_MT6779)*/
#define IPIMB
/* disable for Bring up */
#endif
#endif

#if defined(IPIMB)
#define MAIN_PMIC_WRITE_REGISTER		0x00000201
#define MAIN_PMIC_READ_REGISTER			0x00000202
#define MAIN_PMIC_REGULATOR			0x00000203

#define PMIC_IPI_SEND_SLOT_SIZE			0x5
#define PMIC_IPI_ACK_SLOT_SIZE			0x2

struct pmic_ipi_cmds {
	unsigned int cmd[PMIC_IPI_SEND_SLOT_SIZE];
};

struct pmic_ipi_ret_datas {
	unsigned int data[PMIC_IPI_ACK_SLOT_SIZE];
};

static struct regmap *pmic_read_regmap;

static unsigned int pmic_ipi_write_to_sspm(void *buffer, void *retbuf)
{
	int ret = 0;
	int ipi_ret = 0;
	bool is_err_pnt = false;

	ret = sspm_ipi_send_sync(IPI_ID_PMIC, IPI_OPT_POLLING, buffer,
				 PMIC_IPI_SEND_SLOT_SIZE, retbuf,
				 PMIC_IPI_ACK_SLOT_SIZE);

	if ((ret == IPI_BUSY || ret == IPI_TIMEOUT_ACK) && ipi_ret == 0)
		is_err_pnt = false; /* don't print */
	else if (ret || ipi_ret)
		is_err_pnt = true;

	if (is_err_pnt)
		pr_notice_ratelimited("%s ap_ret=%d ipi_ret=%d\n"
				      , __func__, ret, ipi_ret);
	ret = ipi_ret;

	return ret;
}

static unsigned int pmic_ipi_config_interface(unsigned int RegNum,
					      unsigned int val,
					      unsigned int MASK,
					      unsigned int SHIFT,
					      unsigned char _unused)
{
	struct pmic_ipi_cmds send = { {0} };
	struct pmic_ipi_ret_datas recv = { {0} };
	unsigned int ret = 0;

	send.cmd[0] = MAIN_PMIC_WRITE_REGISTER;
	send.cmd[1] = RegNum;
	send.cmd[2] = val;
	send.cmd[3] = MASK;
	send.cmd[4] = SHIFT;

	ret = pmic_ipi_write_to_sspm(&send, &recv);

	return ret;
}

static int pmic_ipi_reg_write(void *context, const void *data, size_t count)
{
	unsigned int ret;
	unsigned short *dout = (unsigned short *)data;
	unsigned short reg = dout[0], val = dout[1];

	if (count != 4) {
		pr_notice("%s: reg=0x%x, val=0x%x, count=%zu\n",
			  __func__, reg, val, count);
		return -EINVAL;
	}
	ret = pmic_ipi_config_interface(reg, val, 0xFFFF, 0, 1);
	if (ret) {
		pr_info("[%s]fail with ret=%d, reg=0x%x val=0x%x\n",
			__func__, ret, reg, val);
		return -EINVAL;
	}
	return 0;
}

static int pmic_ipi_reg_read(void *context,
			     const void *reg_buf, size_t reg_size,
			     void *val_buf, size_t val_size)
{
	unsigned short reg = *(unsigned short *)reg_buf;
	unsigned int val = 0;
	int ret = 0;

	if (reg_size != 2 || val_size != 2) {
		pr_notice("%s: reg=0x%x, reg_size=%zu, val_size=%zu\n",
			__func__, reg, reg_size, val_size);
		return -EINVAL;
	}
	ret = regmap_read(pmic_read_regmap, reg, &val);
	*(u16 *)val_buf = val;
	return ret;
}

static int pmic_ipi_reg_update_bits(void *context, unsigned int reg,
				    unsigned int mask, unsigned int val)
{
	unsigned int ret;

	ret = pmic_ipi_config_interface(reg, val, mask, 0, 1);
	if (ret) {
		pr_info("[%s]fail with ret=%d, reg=0x%x mask=0x%x val=0x%x\n",
			__func__, ret, reg, mask, val);
		return -EINVAL;
	}
	return 0;
}

static const struct regmap_config pmic_ipi_regmap_config = {
	.name = "pmic_ipi",
	.reg_bits = 16,
	.val_bits = 16,
	.reg_stride = 2,
	.max_register = 0xffff,
	.reg_format_endian = REGMAP_ENDIAN_NATIVE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
	.use_single_rw = true,
};

static struct regmap_bus regmap_pmic_ipi_bus = {
	.write = pmic_ipi_reg_write,
	.read = pmic_ipi_reg_read,
	.reg_update_bits = pmic_ipi_reg_update_bits,
	.fast_io = true,
};
#endif

int mt6358_ipi_init(struct mt6397_chip *chip)
{
	int ret = 0;

#if defined(IPIMB)
	pmic_read_regmap = chip->regmap;
	chip->regmap = devm_regmap_init(chip->dev, &regmap_pmic_ipi_bus,
					NULL, &pmic_ipi_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_notice(chip->dev, "failed to init IPI regmap\n");
	}
#endif
	return ret;
}
