// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define __MTK_UDI_C__

/* system includes */
#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#else
#include <common.h>
#endif
/* local includes */
#include "mtk_udi_internal.h"
#include <mt-plat/mtk_cpufreq_common_api.h>
#include <linux/sched/debug.h>

#ifdef CONFIG_OF
static unsigned long __iomem *udipin_base;
static unsigned long __iomem *udipin_mux1;
static unsigned int udipin_value1;
static unsigned long __iomem *udipin_mux2;
static unsigned int udipin_value2;
static unsigned int udi_offset1;
static unsigned int udi_value1;
static unsigned int udi_offset2;
static unsigned int udi_value2;
static unsigned int ecc_debug;
#endif

static unsigned int ecc_debug_enable;
static unsigned int func_lv_trig_ecc;
static unsigned int func_lv_mask_udi;

/*-----------------------------------------*/
/* Reused code start                       */
/*-----------------------------------------*/
#define udi_read(addr)			readl((void *)addr)
#define udi_write(addr, val) \
	do { writel(val, (void *)addr); wmb(); } while (0) /* sync write */

/*
 * LOG
 */
#define	UDI_TAG	  "[mt_udi] "
#ifdef __KERNEL__
#define udi_info(fmt, args...)		pr_notice(UDI_TAG	fmt, ##args)
#define udi_ver(fmt, args...)	\
	do {	\
		if (func_lv_mask_udi)	\
			pr_notice(UDI_TAG	fmt, ##args);	\
	} while (0)

#else
#define udi_info(fmt, args...)		printf(UDI_TAG fmt, ##args)
#define udi_ver(fmt, args...)		printf(UDI_TAG fmt, ##args)
#endif

unsigned char IR_byte[UDI_FIFOSIZE], DR_byte[UDI_FIFOSIZE];
unsigned int IR_bit_count, IR_pause_count;
unsigned int DR_bit_count, DR_pause_count;
unsigned int jtag_sw_tck; /* default debug channel = 1 */
unsigned int udi_addr_phy;
unsigned int tck_bit, tdi_bit, tms_bit, ntrst_bit, tdo_bit;


#define CTOI(char_ascii) \
	((char_ascii <= 0x39) ? (char_ascii - 0x30) :	\
	((char_ascii <= 0x46) ? (char_ascii - 55) : (char_ascii - 87)))


unsigned int udi_reg_read(unsigned int addr)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_UDI_READ,
		addr,
		0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

void udi_reg_write(unsigned int addr, unsigned int val)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_UDI_WRITE,
		addr,
		val,
		0, 0, 0, 0, 0, &res);

}

unsigned int udi_jtag_clock(unsigned int sw_tck,
				unsigned int i_trst,
				unsigned int i_tms,
				unsigned int i_tdi,
				unsigned int count)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_UDI_JTAG_CLOCK,
		(((1 << (sw_tck & 0x03)) << 3) |
		((i_trst & 0x01) << 2) |
		((i_tms & 0x01) << 1) |
		(i_tdi & 0x01)),
		count,
		(sw_tck & 0x04),
		0, 0, 0, 0, &res);
	return res.a0;
}

unsigned int udi_bit_ctrl(unsigned int sw_tck,
				unsigned int i_tdi,
				unsigned int i_tms,
				unsigned int i_trst)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_UDI_BIT_CTRL,
		((sw_tck & 0x0f) << 3) |
		((i_trst & 0x01) << 2) |
		((i_tms & 0x01) << 1) |
		(i_tdi & 0x01),
		(sw_tck & 0x04),
		0, 0, 0, 0, 0, &res);
	return res.a0;
}


int udi_jtag_clock_read(void)
{
int i, j;

/* support ID or DR zero */
if ((IR_bit_count == 0) && (DR_bit_count == 0))
	return 0;

if (IR_bit_count) {
	/* into idel mode */
	udi_jtag_clock(jtag_sw_tck, 1, 0, 0, 1);
	/* Jog the state machine arround to shift IR */
	udi_jtag_clock(jtag_sw_tck, 1, 1, 0, 2);
	udi_jtag_clock(jtag_sw_tck, 1, 0, 0, 2);

	/* Shift the IR bits, assert TMS=1 for last bit */
	for (i = 0; i < IR_bit_count; i++) {
		j = udi_jtag_clock(jtag_sw_tck, 1,
			((i == (IR_bit_count - 1)) ? 1 : 0),
			(((IR_byte[i >> 3]) >> (i & 7)) & 1), 1);
		IR_byte[i >> 3] &= ~(1 << (i & 7));
		IR_byte[i >> 3] |= (j << (i & 7));
	}

	/* Should be in UPDATE IR */
	if (IR_pause_count) {
		udi_jtag_clock(jtag_sw_tck, 1, 0, 0, IR_pause_count);
		udi_jtag_clock(jtag_sw_tck, 1, 1, 0, 2);
	} else
		udi_jtag_clock(jtag_sw_tck, 1, 1, 0, 1);

	if (DR_bit_count) {
		/* Jog the state machine arround to shift DR */
		udi_jtag_clock(jtag_sw_tck, 1, 1, 0, 1);
		udi_jtag_clock(jtag_sw_tck, 1, 0, 0, 2);
		/* Shift the DR bits, assert TMS=1 for last bit */
		for (i = 0; i < DR_bit_count; i++) {
			j = udi_jtag_clock(jtag_sw_tck, 1,
				((i == (DR_bit_count - 1)) ? 1 : 0),
				(((DR_byte[i >> 3]) >> (i & 7)) & 1), 1);
			DR_byte[i >> 3] &= ~(1 << (i & 7));
			DR_byte[i >> 3] |= (j << (i & 7));
		}

		/* Should be in UPDATE DR */
		if (DR_pause_count) {
			udi_jtag_clock(jtag_sw_tck, 1, 0, 0, DR_pause_count);
			udi_jtag_clock(jtag_sw_tck, 1, 1, 0, 2);
		} else
			udi_jtag_clock(jtag_sw_tck, 1, 1, 0, 1);
	} else
		udi_ver("WARNING: IR-Only JTAG Command\n");

	/* Return the state machine to run-test-idle */
	udi_jtag_clock(jtag_sw_tck, 1, 0, 0, 1);

} else if (DR_bit_count) {
	udi_ver("WARNING: DR-Only JTAG Command\n");

	/* into idel mode */
	udi_jtag_clock(jtag_sw_tck, 1, 0, 0, 1);
	/* Jog the state machine arround to shift DR */
	udi_jtag_clock(jtag_sw_tck, 1, 1, 0, 1);
	udi_jtag_clock(jtag_sw_tck, 1, 0, 0, 2);

	/* Shift the DR bits, assert TMS=1 for last bit */
	for (i = 0; i < DR_bit_count; i++) {
		j = udi_jtag_clock(jtag_sw_tck, 1,
			((i == (DR_bit_count - 1)) ? 1 : 0),
			(((DR_byte[i >> 3]) >> (i & 7)) & 1), 1);
		DR_byte[i >> 3] &= ~(1 << (i & 7));
		DR_byte[i >> 3] |= (j << (i & 7));
	}

	/* Should be in UPDATE DR */
	if (DR_pause_count) {
		udi_jtag_clock(jtag_sw_tck, 1, 0, 0, DR_pause_count);
		udi_jtag_clock(jtag_sw_tck, 1, 1, 0, 2);
	} else
		udi_jtag_clock(jtag_sw_tck, 1, 1, 0, 1);

	/* Return the state machine to run-test-idle */
	udi_jtag_clock(jtag_sw_tck, 1, 0, 0, 1);
} else
	udi_info("SCAN command with #IR=0 and #DR=0. Doing nothing!\n");

#ifndef __KERNEL__
	/* Print the IR/DR readback values to STDOUT */
	printf("Channel = %d, ", jtag_sw_tck);
	if (IR_bit_count) {
		printf("IR %u = ", IR_bit_count);
		for (i = ((IR_bit_count - 1) >> 3); i >= 0; i--)
			printf("%x ", IR_byte[i]);
		printf(" ");
	}

	if (DR_bit_count) {
		printf("DR %u = ", DR_bit_count);
		for (i = ((DR_bit_count - 1) >> 3); i >= 0; i--)
			printf("%x ", DR_byte[i]);
		printf("\n");
	}
#endif

	return 0;
}

struct platform_device udi_pdev = {
	.name   = "mt_udi",
	.id     = -1,
};


#ifdef CONFIG_OF
static const struct of_device_id mt_udi_of_match[] = {
	{ .compatible = "mediatek,udi", },
	{},
};
#endif


#ifdef __KERNEL__ /* __KERNEL__ */

static int udi_probe(struct platform_device *pdev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct device_node *node = NULL;
	int rc = 0;

	node = of_find_matching_node(NULL, mt_udi_of_match);
	if (!node) {
		udi_info("error: cannot find node UDI!\n");
		return 0;
	}

	/* Setup IO addresses and printf */
	udipin_base = of_iomap(node, 0); /* UDI pinmux reg */
	udi_info("udipin_base = 0x%lx.\n", (unsigned long)udipin_base);
	if (!udipin_base) {
		udi_info("udi pinmux get some base NULL.\n");
		return 0;
	}

	rc = of_property_read_u32(node, "udi_offset1", &udi_offset1);
	if (!rc) {
		udi_info("get udi_offset1(0x%x)\n", udi_offset1);
		if (udi_offset1 != 0)
			udipin_mux1 = (unsigned long *)(
					(unsigned long)udipin_base
					+ (unsigned long)udi_offset1);
	}

	rc = of_property_read_u32(node, "udi_value1", &udi_value1);
	if (!rc) {
		udi_info("get udi_value1(0x%x)\n", udi_value1);
		if (udi_value1 != 0)
			udipin_value1 = udi_value1;
	}

	rc = of_property_read_u32(node, "udi_offset2", &udi_offset2);
	if (!rc) {
		udi_info("get udi_offset2(0x%x)\n", udi_offset2);
		if (udi_offset2 != 0)
			udipin_mux2 = (unsigned long *)(
						(unsigned long)udipin_base
						+ (unsigned long)udi_offset2);
	}

	rc = of_property_read_u32(node, "udi_value2", &udi_value2);
	if (!rc) {
		udi_info("get udi_value2(0x%x)\n", udi_value2);
		if (udi_value2 != 0)
			udipin_value2 = udi_value2;
	}

	rc = of_property_read_u32(node, "ecc_debug", &ecc_debug);
	if (!rc) {
		udi_info("get ecc_debug(0x%x)\n", ecc_debug);
		if (ecc_debug == 1)
			ecc_debug_enable = ecc_debug;
	}


#endif

	return 0;
}

static struct platform_driver udi_pdrv = {
	.remove     = NULL,
	.shutdown   = NULL,
	.probe      = udi_probe,
	.suspend    = NULL,
	.resume     = NULL,
	.driver     = {
		.name   = "mt_udi",
#ifdef CONFIG_OF
	.of_match_table = mt_udi_of_match,
#endif
	},
};


#ifdef CONFIG_PROC_FS

static char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	char *buf = (char *)__get_free_page(GFP_USER);

	if (!buf)
		return NULL;
	if (count >= PAGE_SIZE)
		goto out0;
	if (copy_from_user(buf, buffer, count))
		goto out0;

	buf[count] = '\0';
	return buf;

out0:
	free_page((unsigned long)buf);
	return NULL;
}

/* udi_debug_reg */
static int udi_reg_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Reg[%x] = 0x%x.\n",
				udi_addr_phy, udi_reg_read(udi_addr_phy));
	return 0;
}

static ssize_t udi_reg_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);
	unsigned int udi_value = 0, udi_reg_msb, udi_reg_lsb;
	unsigned char udi_rw[5] = { 0, 0, 0, 0, 0 };

	if (!buf)
		return -EINVAL;

	/* protect udi reg read/write */
	if (func_lv_mask_udi == 0) {
		free_page((unsigned long)buf);
		return count;
	}

	if (sscanf(buf, "%1s %x %d %d %x", udi_rw, &udi_addr_phy,
			&udi_reg_msb, &udi_reg_lsb, &udi_value) == 5) {
		/* f format or 'f', addr, MSB, LSB, value */
		udi_reg_field(udi_addr_phy,
			udi_reg_msb : udi_reg_lsb, udi_value);
		udi_info("Read back, Reg[%x] = 0x%x.\n",
				udi_addr_phy, udi_reg_read(udi_addr_phy));
	} else if (sscanf(buf, "%1s %x %x", udi_rw,
				&udi_addr_phy, &udi_value) == 3) {
		/* w format or 'w', addr, value */
		udi_reg_write(udi_addr_phy, udi_value);
		udi_info("Read back, Reg[%x] = 0x%x.\n",
				udi_addr_phy, udi_reg_read(udi_addr_phy));
	} else if (sscanf(buf, "%1s %x", udi_rw, &udi_addr_phy) == 2) {
		/* r format or 'r', addr */
		udi_info("Read back, aReg[%x] = 0x%x.\n",
				udi_addr_phy, udi_reg_read(udi_addr_phy));
	} else {
		udi_info("echo dbg_lv (dec) > /proc/udi/udi_debug\n");
		memset(udi_rw, 0, sizeof(udi_rw));
	}

	free_page((unsigned long)buf);
	return count;
}

/* udi_pinmux_switch */
static int udi_pinmux_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "CPU UDI pinmux reg[0x%lx] = 0x%x.\n",
		(unsigned long)udipin_mux1, udi_read(udipin_mux1));
	seq_printf(m, "CPU UDI pinmux reg[0x%lx] = 0x%x.\n",
		(unsigned long)udipin_mux2, udi_read(udipin_mux2));
	return 0;
}

static ssize_t udi_pinmux_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);
	unsigned int pin_switch = 0U;

	if (buf == NULL)
		return -EINVAL;

	if (kstrtoint(buf, 10, &pin_switch) == 0) {
		if (pin_switch == 1U) {
			udi_write(udipin_mux1, udipin_value1);
			udi_write(udipin_mux2, udipin_value2);
		}
	} else
		udi_info("echo dbg_lv (dec) > /proc/udi/udi_pinmux\n");

	free_page((unsigned long)buf);
	return (long)count;
}

/* udi_debug_info_print_flag */
static int udi_debug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "UDI debug (log level) = %d.\n", func_lv_mask_udi);
	return 0;
}

static ssize_t udi_debug_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);
	unsigned int dbg_lv = 0;

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &dbg_lv))
		func_lv_mask_udi = dbg_lv;
	else
		udi_info("echo dbg_lv (dec) > /proc/udi/udi_debug\n");

	free_page((unsigned long)buf);
	return count;
}

static int udi_jtag_clock_proc_show(struct seq_file *m, void *v)
{
	int i;

	udi_jtag_clock_read();

	/* Print the IR/DR readback values to STDOUT */
	seq_printf(m, "IR %u ", IR_bit_count);
	if (IR_bit_count) {
		for (i = ((IR_bit_count - 1) >> 3); i >= 0; i--)
			seq_printf(m, "%02x", IR_byte[i]);
	} else
		seq_puts(m, "00");

	seq_printf(m, " DR %u ", DR_bit_count);
	if (DR_bit_count) {
		for (i = ((DR_bit_count - 1) >> 3); i >= 0; i--)
			seq_printf(m, "%02x", DR_byte[i]);
	} else
		seq_puts(m, "00 ");

	seq_puts(m, "\n");

	return 0;
}

static ssize_t udi_jtag_clock_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);
	unsigned int i, numdigits, length;
	unsigned int recv_buf[5];
	unsigned char recv_char[2][UDI_FIFOSIZE * 2]; /* two char is one byte */
	unsigned char recv_key_word[10];

	if (!buf)
		return -EINVAL;

	/* data initial */
	jtag_sw_tck = 0;
	IR_bit_count = 0;
	DR_bit_count = 0;
	IR_pause_count = 0;
	DR_pause_count = 0;
	memset(IR_byte, 0, sizeof(IR_byte));
	memset(DR_byte, 0, sizeof(DR_byte));

	/* input check format */
	if (sscanf(buf, "%4s %u %u %512s %u %u %512s %u",
			&recv_key_word[0], &recv_buf[0],
			&recv_buf[1], recv_char[0], &recv_buf[3],
			&recv_buf[2], recv_char[1], &recv_buf[4]) == 8) {
		/* 6 parameter */
		IR_pause_count = recv_buf[3];
		DR_pause_count = recv_buf[4];
	} else if (sscanf(buf, "%4s %u %u %512s %u %512s",
			&recv_key_word[0], &recv_buf[0],
			&recv_buf[1], recv_char[0],
			&recv_buf[2], recv_char[1]) == 6) {
		/* 4 parameter */
		IR_pause_count = 0;
		DR_pause_count = 0;
	} else if (sscanf(buf, "%6s", &recv_key_word[0]) == 1) {
		/* RESET */
		if (!strcmp(recv_key_word, "RESET")) {
			udi_ver("Input data: recv_key_word = RESET\n");
			/* rest mode by TRST = 1 */
			/* jtag_clock(sw_tck, i_trst, i_tms, i_tdi, count) */
			udi_jtag_clock(jtag_sw_tck, 0, 0, 0, 4);
			goto out1;
			}
	} else {
		udi_info("echo wrong format > /proc/udi/udi_jtag_clock\n");
		goto out1;
	}

	udi_ver("Input data: SCAN = %u\n", recv_key_word[0]);
	udi_ver("Input data: Channel = %u\n", recv_buf[0]);
	udi_ver("Input data: 1 = %u\n", recv_buf[1]);
	udi_ver("Input data: 2 = %s\n", recv_char[0]);
	udi_ver("Input data: 3 = %u\n", recv_buf[3]);
	udi_ver("Input data: 4 = %u\n", recv_buf[2]);
	udi_ver("Input data: 5 = %s\n", recv_char[1]);
	udi_ver("Input data: 6 = %u\n", recv_buf[4]);

	/* chekc first key word equ "SCAN" */
	if (strcmp(recv_key_word, "SCAN")) {
		udi_info("echo wrong format > /proc/udi/udi_jtag_clock\n");
		goto out1;
	}

	/* check channel 0~3: gwtap0, 4~7: gwtap1 */
	if ((recv_buf[0] < 0) || (recv_buf[0] > 7)) {
		udi_info("ERROR: Sub-Chains out 1~7\n");
		goto out1;
	} else {
		jtag_sw_tck = recv_buf[0];
	}

	/* chek IR/DR bit counter */
	if ((recv_buf[1] == 0) && (recv_buf[2] == 0)) {
		udi_info("ERROR: IR and DR bit all zero\n");
		goto out1;
	}

	/* Parse the IR command into a bit string,
	 * for a05 must be 9~12bits range
	 */
	if (recv_buf[1] == 0)
		udi_ver("WARNING: DR-Only JTAG Command\n");
	else if ((recv_buf[1] > (strlen(recv_char[0]) * 4)) ||
			(recv_buf[1] < ((strlen(recv_char[0]) * 4) - 3))) {
		udi_info("ERROR: IR %u not match with %u bits\n",
			(unsigned int)strlen(recv_char[0]), recv_buf[1]);
		goto out1;
	} else {
		IR_bit_count = recv_buf[1];
		udi_ver("Input data: IR_bit_count = %u\n", IR_bit_count);
		/* Parse the IR command into a bit string */
		length = strlen(recv_char[0])-1;
		numdigits = length / 2;

	for (i = 0; i <= numdigits; i++) {
		if (length == (i << 1)) {
			IR_byte[i] = CTOI(recv_char[0][length - (2 * i)]);
		} else {
			IR_byte[i] = (CTOI(recv_char[0][length - (2 * i) - 1])
			<< 4) + CTOI(recv_char[0][length - (2 * i)]);
		}
		udi_ver("IR[%d] = 0x%02X\n", i, IR_byte[i]);
	} /* example jtag_adb 1 12 a05 30 9b6a4109, IR[0]=0x05. IR[1]=0x0a */
	}

	/* Parse the DR command into a bit string,
	 * for 9b6a4109 must be 29~32bits range
	 */
	if (recv_buf[2] == 0)
		udi_ver("WARNING: IR-Only JTAG Command\n");
	else if ((recv_buf[2] > (strlen(recv_char[1]) * 4))
			|| (recv_buf[2] < ((strlen(recv_char[1]) * 4) - 3))) {
		udi_info("ERROR: DR %u not match with %u bits)\n",
			(unsigned int)strlen(recv_char[1]), recv_buf[2]);
		goto out1;
	} else {
		DR_bit_count = recv_buf[2];
		udi_ver("Input data: DR_bit_count = %u\n", DR_bit_count);
		/* Parse the DR command into a bit string */
		length = strlen(recv_char[1])-1;
		numdigits = length / 2;

	for (i = 0; i <= numdigits; i++) {
		if (length == (i << 1))
			DR_byte[i] = CTOI(recv_char[1][length - (2 * i)]);
		else
			DR_byte[i] = (CTOI(recv_char[1][length - (2 * i) - 1])
			<< 4) + CTOI(recv_char[1][length - (2 * i)]);
		udi_ver("DR[%d] = 0x%02X\n", i, DR_byte[i]);
	} /* example jtag_adb 1 12 a05 30 9b6a4109, IR[0]=0x05. IR[1]=0x0a */
	}


out1:
	free_page((unsigned long)buf);
	return count;
}

/* udi bit control */
static int udi_bit_ctrl_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "SW UDI: TCK=%x, TDI=%x, TMS=%x, nTRST=%x, TDO=%x\n",
			tck_bit, tdi_bit, tms_bit, ntrst_bit, tdo_bit);

	return 0;
}

static ssize_t udi_bit_ctrl_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int recv[4];

	char *buf =	_copy_from_user_for_proc(buffer, count);

	tck_bit = 0;
	tdi_bit = 0;
	tms_bit = 0;
	ntrst_bit = 0;
	tdo_bit = 0;

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d %d %d", &recv[0],
		&recv[1], &recv[2], &recv[3]) == 4) {
		tck_bit = recv[0];
		tdi_bit = recv[1];
		tms_bit = recv[2];
		ntrst_bit = recv[3];
		tdo_bit = udi_bit_ctrl(recv[0], recv[1], recv[2], recv[3]);
		udi_info("SW UDI: TCK=%x, TDI=%x, TMS=%x, nTRST=%x, TDO=%x\n",
				tck_bit, tdi_bit, tms_bit, ntrst_bit, tdo_bit);
	}

	free_page((unsigned	long)buf);
	return count;


}

/* ECC debug */
void ecc_dump_debug_info(void)
{
	if (ecc_debug_enable) {
		show_stack(current, NULL);
		pr_notice("%s: LCPU %d khz, BCPU %d khz\n",
				__func__,
				mt_cpufreq_get_cur_freq(0),
				mt_cpufreq_get_cur_freq(1));
	} else
		pr_notice("ecc backtrace off.");
}

#define ECC_UE_TRIGGER		(0x80000002)
#define ECC_CE_TRIGGER		(0x80000040)
#define ECC_DE_TRIGGER		(0x80000020)
#define ECC_ENABE			(0x0000010D)
#define ECC_PFG_COUNTER		(0x00000001)

static void write_ERXSELR_EL1(u32 v)
{
	__asm__ volatile ("msr s3_0_c5_c3_1, %0" : : "r" (v));
}

static u64 read_ERR0CTLR_EL1(void)
{
	u64 v;

	__asm__ volatile ("mrs %0, s3_0_c5_c4_1" : "=r" (v));

	return v;
}

static void write_ERR0CTLR_EL1(u64 v)
{
	__asm__ volatile ("msr s3_0_c5_c4_1, %0" : : "r" (v));
}

static void write_ERXPFGCDNR_EL1(u64 v)
{
	__asm__ volatile ("msr s3_0_c15_c2_2, %0" : : "r" (v));
}


static u64 read_ERXPFGCTLR_EL1(void)
{
	u64 v;

	__asm__ volatile ("mrs %0, s3_0_c15_c2_1" : "=r" (v));

	return v;
}

static void write_ERXPFGCTLR_EL1(u64 v)
{
	__asm__ volatile ("msr s3_0_c15_c2_1, %0" : : "r" (v));
}

static int ecc_test_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ECC UE(1)/DE(2)/CE(3)= %d (0x%lx)\n",
			func_lv_trig_ecc,
			(unsigned long)read_ERXPFGCTLR_EL1());

	return 0;
}

static ssize_t ecc_test_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = _copy_from_user_for_proc(buffer, count);
	unsigned int dbg_lv = 0;

	if (!buf)
		return -EINVAL;

	if (!kstrtoint(buf, 10, &dbg_lv)) {
		func_lv_trig_ecc = dbg_lv;

		write_ERXSELR_EL1(0x0);
		write_ERR0CTLR_EL1(read_ERR0CTLR_EL1() | ECC_ENABE);
		udi_info("ecc read_ERR0CTLR_EL1: 0x%lx\n",
				(unsigned long)read_ERR0CTLR_EL1());
		write_ERXPFGCDNR_EL1(ECC_PFG_COUNTER);
		udi_info("ecc write_ERXPFGCDNR_EL1: 0x%x\n",
				ECC_PFG_COUNTER);

		if (dbg_lv == 1) {
			write_ERXPFGCTLR_EL1(read_ERXPFGCTLR_EL1() |
								ECC_UE_TRIGGER);
			udi_info("ecc read_ERXPFGCTLR_EL1 UE: 0x%lx\n",
					(unsigned long)read_ERXPFGCTLR_EL1());
		} else if (dbg_lv == 2) {
			write_ERXPFGCTLR_EL1(read_ERXPFGCTLR_EL1() |
								ECC_DE_TRIGGER);
			udi_info("ecc read_ERXPFGCTLR_EL1 DE: 0x%lx\n",
					(unsigned long)read_ERXPFGCTLR_EL1());
		} else if (dbg_lv == 3) {
			write_ERXPFGCTLR_EL1(read_ERXPFGCTLR_EL1() |
								ECC_CE_TRIGGER);
			udi_info("ecc read_ERXPFGCTLR_EL1 CE: 0x%lx\n",
					(unsigned long)read_ERXPFGCTLR_EL1());
		}
	} else
		udi_info("echo dbg_lv (dec) > /proc/ecc/ecc_test\n");

	free_page((unsigned long)buf);
	return count;
}






#define PROC_FOPS_RW(name)          \
static int name ## _proc_open(struct inode *inode, struct file *file)   \
{                                   \
	return single_open(file, name ## _proc_show, PDE_DATA(inode));  \
}                                   \
static const struct file_operations name ## _proc_fops = {      \
	.owner          = THIS_MODULE,                  \
	.open           = name ## _proc_open,               \
	.read           = seq_read,                 \
	.llseek         = seq_lseek,                    \
	.release        = single_release,               \
	.write          = name ## _proc_write,              \
}

#define PROC_FOPS_RO(name)         \
static int name ## _proc_open(struct inode *inode, struct file *file)   \
{                                   \
	return single_open(file, name ## _proc_show, PDE_DATA(inode));  \
}                                   \
static const struct file_operations name ## _proc_fops = {      \
	.owner          = THIS_MODULE,                  \
	.open           = name ## _proc_open,               \
	.read           = seq_read,                 \
	.llseek         = seq_lseek,                    \
	.release        = single_release,               \
}

#define PROC_ENTRY(name)    {__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(udi_reg);			/* for any register read/write */
PROC_FOPS_RW(udi_pinmux);		/* for udi pinmux switch */
PROC_FOPS_RW(udi_debug);		/* for debug information */
PROC_FOPS_RW(udi_jtag_clock);	/* for udi jtag interface */
PROC_FOPS_RW(udi_bit_ctrl);		/* for udi bit ctrl */
PROC_FOPS_RW(ecc_test);			/* for udi bit ctrl */

static int _create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(udi_reg),
		PROC_ENTRY(udi_pinmux),
		PROC_ENTRY(udi_debug),
		PROC_ENTRY(udi_jtag_clock),
		PROC_ENTRY(udi_bit_ctrl),
		PROC_ENTRY(ecc_test),
	};

	dir = proc_mkdir("udi", NULL);

	if (!dir) {
		udi_info("fail to create /proc/udi @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name,
			0664, dir, entries[i].fops))
			udi_info("%s(), create /proc/udi/%s failed\n",
				__func__, entries[i].name);
	}

	return 0;
}

#endif /* CONFIG_PROC_FS */

/*
 * Module driver
 */
static int __init udi_init(void)
{
	int err = 0;

	/* initial value */
	func_lv_mask_udi = 0;
	IR_bit_count = 0;
	DR_bit_count = 0;
	jtag_sw_tck = 1;

	err = platform_driver_register(&udi_pdrv);
	if (err) {
		udi_info("%s(), UDI driver callback register failed..\n",
				__func__);
		return err;
	}

#ifdef CONFIG_PROC_FS
	/* init proc */
	if (_create_procfs()) {
		err = -ENOMEM;
		return err;
	}
#endif

	return 0;
}

static void __exit udi_exit(void)
{
	udi_info("UDI de-initialization\n");
	platform_driver_unregister(&udi_pdrv);
}

module_init(udi_init);
module_exit(udi_exit);

MODULE_DESCRIPTION("MediaTek UDI Driver v3");
MODULE_LICENSE("GPL");
#endif /* __KERNEL__ */
#undef __MTK_UDI_C__
