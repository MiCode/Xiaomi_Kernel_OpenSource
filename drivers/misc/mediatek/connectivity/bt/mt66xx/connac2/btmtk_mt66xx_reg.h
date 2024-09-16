/*
*  Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _BT_REG_H
#define _BT_REG_H

//#include "typedef.h"
#include "conninfra.h"
#include "connsys_debug_utility.h"

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#ifndef BIT
#define BIT(n)                          (1UL << (n))
#endif /* BIT */

#ifndef BITS
/* bits range: for example BITS(16,23) = 0xFF0000
 *   ==>  (BIT(m)-1)   = 0x0000FFFF     ~(BIT(m)-1)   => 0xFFFF0000
 *   ==>  (BIT(n+1)-1) = 0x00FFFFFF
 */
#define BITS(m, n)                      (~(BIT(m)-1) & ((BIT(n) - 1) | BIT(n)))
#endif /* BIT */

/*
 * This macro returns the byte offset of a named field in a known structure
 * type.
 * _type - structure name,
 * _field - field name of the structure
 */
#ifndef OFFSET_OF
#define OFFSET_OF(_type, _field)    ((unsigned long)&(((_type *)0)->_field))
#endif /* OFFSET_OF */

/*
 * This macro returns the base address of an instance of a structure
 * given the type of the structure and the address of a field within the
 * containing structure.
 * _addr_of_field - address of a field within the structure,
 * _type - structure name,
 * _field - field name of the structure
 */
#ifndef ENTRY_OF
#define ENTRY_OF(_addr_of_field, _type, _field) \
	((_type *)((unsigned char *)(_addr_of_field) - (unsigned char *)OFFSET_OF(_type, _field)))
#endif /* ENTRY_OF */

#define BT_CR_DUMP_BUF_SIZE	(1024)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
struct consys_reg_base_addr {
	unsigned long vir_addr;
	unsigned long phy_addr;
	unsigned long long size;
};

enum bt_base_addr_index {
	CONN_INFRA_RGU_BASE_INDEX = 0,
	CONN_INFRA_CFG_BASE_INDEX = 1,
	CONN_INFRA_SYS_BASE_INDEX = 2,
	SPM_BASE_INDEX		  = 3,

	BGFSYS_BASE_INDEX	  = 4,
	BGFSYS_INFO_BASE_INDEX	  = 5,

	INFRACFG_AO_BASE_INDEX    = 6,
#if 0
	CONN_INFRA_CFG_CCIF_BASE  = 7,
	BGF2MD_BASE_INDEX	  = 8,
#endif

	CONSYS_BASE_ADDR_MAX
};

struct bt_base_addr {
	struct consys_reg_base_addr reg_base_addr[CONSYS_BASE_ADDR_MAX];
};

extern struct bt_base_addr bt_reg;
extern int32_t bgfsys_check_conninfra_ready(void);

#define CON_REG_INFRA_RGU_ADDR		bt_reg.reg_base_addr[CONN_INFRA_RGU_BASE_INDEX].vir_addr /* 0x18000000 0x1000 */
#define CON_REG_INFRA_CFG_ADDR		bt_reg.reg_base_addr[CONN_INFRA_CFG_BASE_INDEX].vir_addr /* 0x18001000 0x1000 */
#define CON_REG_INFRA_SYS_ADDR		bt_reg.reg_base_addr[CONN_INFRA_SYS_BASE_INDEX].vir_addr /* 0x18050000 0x1000 */
#define CON_REG_SPM_BASE_ADDR 		bt_reg.reg_base_addr[SPM_BASE_INDEX].vir_addr		 /* 0x18060000 0x1000 */
#define BGF_REG_BASE_ADDR		bt_reg.reg_base_addr[BGFSYS_BASE_INDEX].vir_addr	 /* 0x18800000 0x1000 */
#define BGF_REG_INFO_BASE_ADDR		bt_reg.reg_base_addr[BGFSYS_INFO_BASE_INDEX].vir_addr    /* 0x18812000 0x1000 */
#define CON_REG_INFRACFG_AO_ADDR	bt_reg.reg_base_addr[INFRACFG_AO_BASE_INDEX].vir_addr    /* 0x10001000 0x1000 */
#if 0
#define CON_REG_INFRA_CCIF_ADDR		bt_reg.reg_base_addr[CONN_INFRA_CFG_CCIF_BASE].vir_addr
#define BGF2MD_BASE_ADDR		bt_reg.reg_base_addr[BGF2MD_BASE_INDEX].vir_addr
#endif

static uint8_t g_dump_cr_buffer[BT_CR_DUMP_BUF_SIZE];

/*********************************************************************
*
*  Connsys Control Register Definition
*
**********************************************************************
*/
#define SET_BIT(addr, bit) \
			(*((volatile uint32_t *)(addr))) |= ((uint32_t)bit)
#define CLR_BIT(addr, bit) \
			(*((volatile uint32_t *)(addr))) &= ~((uint32_t)bit)
#define REG_READL(addr) \
			readl((volatile uint32_t *)(addr))
#define REG_WRITEL(addr, val) \
			writel(val, (volatile uint32_t *)(addr))

/*
 * EMI region
 */
#define BT_EMI_BASE_OFFSET		0x000000000

/*
 * ConnInfra Control Register Region:
 *	  (CONN_INFRA_BASE) ~ (CONN_INFRA_BASE + CONN_INFRA_LENGTH)
 */
#define CONN_INFRA_BASE                     	0x18000000 /* variable */

/*
 * Conninfra CFG AO
 *
 */
#define CONN_INFRA_CFG_AO_BASE			CON_REG_INFRACFG_AO_ADDR
#define BGF_PAD_EINT				(CONN_INFRA_CFG_AO_BASE + 0xF00)

#if 0
/*
 * CCIF Base
 *
 */
#define CONN_INFRA_CFG_CCIF_BASE		CON_REG_INFRA_CCIF_ADDR
#define CCIF_CONSYS_BGF_ADDR			(CONN_INFRA_CFG_CCIF_BASE + 0x318)

/*
 * BGF2MD Base
 *
 */
#define CONN_BGF2MD_BASE			BGF2MD_BASE_ADDR
#define CONN_PCCIF5_RX_ACK			(BGF2MD_BASE_ADDR + 0x14)
#endif

/*
 * ConnInfra RGU Region
 */
#define CONN_INFRA_RGU_START                    CON_REG_INFRA_RGU_ADDR

#define CONN_INFRA_RGU_BGFSYS_ON_TOP_PWR_CTL    (CONN_INFRA_RGU_START + 0x0008)
#define BGF_PWR_CTL_B                           BIT(7)

#define CONN_INFRA_RGU_BGFSYS_CPU_SW_RST        (CONN_INFRA_RGU_START + 0x0014)
#define BGF_CPU_SW_RST_B                        BIT(0)

#define CONN_INFRA_RGU_BGFSYS_SW_RST_B		(CONN_INFRA_RGU_START + 0x001C)
#define BGF_SW_RST_B				BIT(0)

#define CONN_INFRA_RGU_BGFSYS_ON_TOP_PWR_ACK_ST (CONN_INFRA_RGU_START + 0x0114)
#define BGF_ON_PWR_ACK_B			BITS(24, 25)

#define CONN_INFRA_RGU_BGFSYS_OFF_TOP_PWR_ACK_ST (CONN_INFRA_RGU_START + 0x0124)
#define BGF_OFF_PWR_ACK_B			BIT(24)
#define BGF_OFF_PWR_ACK_S			BITS(0, 1)





/*
 * ConnInfra CFG Region
 */
#define CONN_INFRA_CFG_START			CON_REG_INFRA_CFG_ADDR

#define CONN_INFRA_CFG_VERSION			(CONN_INFRA_CFG_START)

#define CONN_INFRA_CONN2BT_GALS_SLP_CTL		(CONN_INFRA_CFG_START + 0x0610)
#define CONN2BT_SLP_PROT_TX_EN_B		BIT(0)
#define CONN2BT_SLP_PROT_TX_ACK_B		BIT(3)
#define CONN2BT_SLP_PROT_RX_EN_B		BIT(4)
#define CONN2BT_SLP_PROT_RX_ACK_B		BIT(7)

#define CONN_INFRA_BT2CONN_GALS_SLP_CTL		(CONN_INFRA_CFG_START + 0x0614)
#define BT2CONN_SLP_PROT_TX_EN_B		BIT(0)
#define BT2CONN_SLP_PROT_TX_ACK_B		BIT(3)
#define BT2CONN_SLP_PROT_RX_EN_B		BIT(4)
#define BT2CONN_SLP_PROT_RX_ACK_B		BIT(7)

#define CONN_INFRA_CFG_BT_PWRCTLCR0		(CONN_INFRA_CFG_START + 0x0874)
#define BT_FUNC_EN_B				BIT(0)

#define CONN_INFRA_CFG_EMI_CTL_BT_EMI_REQ_BT	(CONN_INFRA_CFG_START + 0x0C18)
#define BT_EMI_CTRL_BIT				BIT(0)
#define BT_EMI_CTRL_BIT1			BIT(1)

/*
 * Connsys Host CSR Top Region
 */
#define CONN_HOST_CSR_TOP_START			CON_REG_SPM_BASE_ADDR

#define CONN_INFRA_WAKEUP_BT			(CONN_HOST_CSR_TOP_START + 0x01A8)
#define CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL	(CONN_HOST_CSR_TOP_START + 0x0184)

#define CONN_REMAP_ADDR				(CONN_HOST_CSR_TOP_START + 0x01C4)

#define CONN_INFRA_ON2OFF_SLP_PROT_ACK		BIT(5)

#define CONN_MCU_PC				(CONN_HOST_CSR_TOP_START + 0x22C)

#define BGF_LPCTL                           	(CONN_HOST_CSR_TOP_START + 0x0030)
#define BGF_HOST_SET_FW_OWN_B			BIT(0)
#define BGF_HOST_CLR_FW_OWN_B			BIT(1)
#define BGF_OWNER_STATE_SYNC_B			BIT(2)

#define BGF_IRQ_STAT				(CONN_HOST_CSR_TOP_START + 0x0034)
#define BGF_IRQ_FW_OWN_CLR_B			BIT(0)

#define BGF_IRQ_EN				(CONN_HOST_CSR_TOP_START + 0x0038)
#define BGF_IRQ_DRV_OWN_EN_B			BIT(0)
#define BGF_IRQ_FW_OWN_EN_B			BIT(1)

#define BGF_IRQ_STAT2				(CONN_HOST_CSR_TOP_START + 0x003C)
#define BGF_IRQ_FW_OWN_SET_B			BIT(0)

/*
 * BGFSYS Control Register Region:
 *     (BGFSYS_BASE) ~ (BGFSYS_BASE + BGFSYS_LENGTH)
 */
#define BGFSYS_BASE				0x18800000

#define BGF_MCCR				(BGF_REG_BASE_ADDR + 0x0100)
#define BGF_CON_CR_AHB_AUTO_DIS			BIT(31)

#define BGF_MCCR_SET				(BGF_REG_BASE_ADDR + 0x0104)
#define BGF_CON_CR_AHB_STOP			BIT(2)

#define BGF_SW_IRQ_RESET_ADDR			(BGF_REG_BASE_ADDR + 0x014C)
#define BGF_SW_IRQ_STATUS			(BGF_REG_BASE_ADDR + 0x0150)
#define BGF_WHOLE_CHIP_RESET			BIT(26)
#define BGF_SUBSYS_CHIP_RESET			BIT(25)
#define BGF_FW_LOG_NOTIFY			BIT(24)

#define BGF_MCU_CFG_SW_DBG_CTL			(BGF_REG_BASE_ADDR + 0x016C)

#define BGF_HW_VERSION				BGF_REG_INFO_BASE_ADDR
#define BGF_HW_VER_ID				0x00008A00

#define BGF_FW_VERSION				(BGF_REG_INFO_BASE_ADDR + 0x04)
#define BGF_FW_VER_ID				0x00008A00

#define BGF_HW_CODE				(BGF_REG_INFO_BASE_ADDR + 0x08)
#define BGF_HW_CODE_ID				0x00000000

#define BGF_IP_VERSION				(BGF_REG_INFO_BASE_ADDR + 0x10)
#define BGF_IP_VER_ID				0x20010000


#define CAN_DUMP_HOST_CSR(reason) \
	(reason != CONNINFRA_INFRA_BUS_HANG && \
	 reason != CONNINFRA_AP2CONN_RX_SLP_PROT_ERR && \
	 reason != CONNINFRA_AP2CONN_TX_SLP_PROT_ERR && \
	 reason != CONNINFRA_AP2CONN_CLK_ERR)

/*********************************************************************
*
* CR/Constant value for specific project
*
**********************************************************************
*/
#define _BIN_NAME_POSTFIX 			".bin"
#define _BIN_NAME_UPDATE_POSTFIX 	"-u.bin"

#if (CONNAC20_CHIPID == 6885)
#ifdef BT_CUS_FEATURE
	#define _BIN_NAME_MCU			"soc3_0_ram_mcu_1b_1_hdr"
	#define _BIN_NAME_BT			"soc3_0_ram_bt_1b_1_hdr"
#else
	#define _BIN_NAME_MCU			"soc3_0_ram_mcu_1_1_hdr"
	#define _BIN_NAME_BT			"soc3_0_ram_bt_1_1_hdr"
#endif
	#define CONN_INFRA_CFG_ID		(0x20010000)
#elif (CONNAC20_CHIPID == 6893)
	#define _BIN_NAME_MCU			"soc3_0_ram_mcu_1a_1_hdr"
	#define _BIN_NAME_BT			"soc3_0_ram_bt_1a_1_hdr"
	#define CONN_INFRA_CFG_ID		(0x20010101)
#endif

#define BIN_NAME_MCU 	(_BIN_NAME_MCU _BIN_NAME_POSTFIX)
#define BIN_NAME_BT 	(_BIN_NAME_BT _BIN_NAME_POSTFIX)
#define BIN_NAME_MCU_U 	(_BIN_NAME_MCU _BIN_NAME_UPDATE_POSTFIX)
#define BIN_NAME_BT_U 	(_BIN_NAME_BT _BIN_NAME_UPDATE_POSTFIX)

/* bt_dump_cpucpr
 *
 *    Dump cpu counter for a period of time
 *
 * Arguments:
 *     [IN] times  	how many times to dump CR
 *     [IN] sleep_ms	sleep interval b/w each dump
 *
 * Return Value:
 *     N/A
 *
 */
static void inline bt_dump_cpupcr(uint32_t times, uint32_t sleep_ms)
{
	uint32_t i = 0;
	uint32_t value = 0;

	for(i = 0; i < times; i++) {
		value = REG_READL(CONN_MCU_PC);
		BTMTK_DBG("%s: bt pc=0x%08x", __func__, value);
		if (sleep_ms > 0)
			msleep(sleep_ms);
	}
}

static uint32_t inline bt_read_cr(uint32_t addr)
{
	uint32_t value = 0;
	uint8_t *base = ioremap_nocache(addr, 0x10);

	if (base == NULL) {
		BTMTK_ERR("%s: remapping 0x%08x fail", addr);
	} else {
		value = REG_READL(base);
		iounmap(base);
	}
	return value;
}

static void inline bt_write_cr(uint32_t addr, uint32_t value)
{
	uint32_t *base = ioremap_nocache(addr, 0x10);

	if (base == NULL) {
		BTMTK_ERR("%s: remapping 0x%08x fail", addr);
	} else {
		*base = value;
		iounmap(base);
	}
}

static void inline bt_dump_memory8(uint8_t *buf, uint32_t len)
{
	uint32_t i = 0;
	uint8_t *pos = NULL, *end = NULL;
	int32_t ret = 0;

	memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
	pos = &g_dump_cr_buffer[0];
	end = pos + BT_CR_DUMP_BUF_SIZE - 1;

	BTMTK_INFO("%s: length = (%d)", __func__, len);
	for (i = 0; i <= len; i++) {
		ret = snprintf(pos, (end - pos + 1), "%02x ", buf[i]);
		if (ret < 0 || ret >= (end - pos + 1))
			break;
		pos += ret;

		if ((i & 0xF) == 0xF || i == len - 1) {
			BTMTK_INFO("%s", g_dump_cr_buffer);
			memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
			pos = &g_dump_cr_buffer[0];
			end = pos + BT_CR_DUMP_BUF_SIZE - 1;
		}
	}

}

static inline u_int8_t bt_is_bgf_bus_timeout(void)
{
	int32_t mailbox_status = 0;

	/*
	 * Conn-infra bus timeout : 160ms
	 * Bgf bus timeout : 80ms
	 *
	 * There's still a case that we pass conninfra check but still get
	 * bus hang, so we have to check mail box for sure
	 */
	mailbox_status = REG_READL(CON_REG_SPM_BASE_ADDR + 0x268);
	if (mailbox_status != 0 &&
		(mailbox_status & 0xFF000000) != 0x87000000) {
		BTMTK_INFO("mailbox_status = 0x%08x", mailbox_status);
		return TRUE;
	}

	return FALSE;
}

static void inline bt_dump_bgfsys_host_csr(void)
{
	uint32_t value = 0;
	uint32_t i = 0;
	uint8_t *pos = NULL, *end = NULL;
	int32_t ret = 0;

	memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
	pos = &g_dump_cr_buffer[0];
	end = pos + BT_CR_DUMP_BUF_SIZE - 1;

	BTMTK_INFO("[BGF host csr] Count = 11");
	for (i = 0x22C; i <= 0x244; i+=4) {
		value = REG_READL(CON_REG_SPM_BASE_ADDR + i);
		ret = snprintf(pos, (end - pos + 1), "%08x ", value);
		if (ret < 0 || ret >= (end - pos + 1))
			break;
		pos += ret;
	}

	for (i = 0x264; i <= 0x270; i+=4) {
		value = REG_READL(CON_REG_SPM_BASE_ADDR + i);
		ret = snprintf(pos, (end - pos + 1), "%08x ", value);
		if (ret < 0 || ret >= (end - pos + 1))
			break;
		pos += ret;
	}

	BTMTK_INFO("%s", g_dump_cr_buffer);
}

/* please make sure check bus hang before calling this dump */
static void inline bt_dump_bgfsys_mcusys_flag(void)
{
	uint32_t value = 0;
	uint32_t i = 0, count = 1, cr_count = 56;
	uint8_t *pos = NULL, *end = NULL;
	int32_t ret = 0;
	uint16_t switch_flag = 0x2A;

	memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
	pos = &g_dump_cr_buffer[0];
	end = pos + BT_CR_DUMP_BUF_SIZE - 1;
	value = REG_READL(CON_REG_SPM_BASE_ADDR + 0xA0);
	value &= 0xFFFFFF03; /* ignore [7:2] */
	value |= (switch_flag << 2);
	REG_WRITEL(CON_REG_SPM_BASE_ADDR + 0xA0, value);

	BTMTK_INFO("[BGF MCUSYS debug flag] Count = (%d)", cr_count);
	for (i = 0x00020101; i <= 0x00706F01; i+= 0x20200, count++) {
		REG_WRITEL(CON_REG_SPM_BASE_ADDR + 0xA8, i);
		value = REG_READL(CON_REG_SPM_BASE_ADDR + 0x22C);
		ret = snprintf(pos, (end - pos + 1), "%08x ", value);
		if (ret < 0 || ret >= (end - pos + 1))
			break;
		pos += ret;

		if ((count & 0xF) == 0 || count == cr_count) {
			BTMTK_INFO("%s", g_dump_cr_buffer);
			memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
			pos = &g_dump_cr_buffer[0];
			end = pos + BT_CR_DUMP_BUF_SIZE - 1;
		}
	}
}

/* please make sure check bus hang before calling this dump */
static void inline bt_dump_bgfsys_bus_flag(void)
{
	uint32_t value = 0;
	uint32_t i = 0, j = 0, count = 1, cr_count = 20;
	uint8_t *pos = NULL, *end = NULL;
	int32_t ret = 0;
	uint16_t switch_flag = 0x2A;


	memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
	pos = &g_dump_cr_buffer[0];
	end = pos + BT_CR_DUMP_BUF_SIZE - 1;
	value = REG_READL(CON_REG_SPM_BASE_ADDR + 0xA0);
	value &= 0xFFFFFF03; /* ignore [7:2] */
	value |= (switch_flag << 2);
	REG_WRITEL(CON_REG_SPM_BASE_ADDR + 0xA0, value);

	if (conninfra_reg_readable() && !bt_is_bgf_bus_timeout()) {
		value = bt_read_cr(0x1881E000);
		value &= 0xFFFFFFEF;
		value |= (1 << 4);
		bt_write_cr(0x1881E000, value);
	} else
		BTMTK_ERR("conninfra is not readable");

	BTMTK_INFO("[BGF BUS debug flag] Count = (%d)", cr_count);
	for (i = 0x104A4901; i <= 0xA04A4901; i += 0x10000000) {
		for (j = i; j <= i + 0x20200; j += 0x20200) {
			REG_WRITEL(CON_REG_SPM_BASE_ADDR + 0xA8, j);
			value = REG_READL(CON_REG_SPM_BASE_ADDR + 0x22C);
			ret = snprintf(pos, (end - pos + 1), "%08x ", value);
			if (ret < 0 || ret >= (end - pos + 1))
				break;
			pos += ret;
			count++;

			if ((count & 0xF) == 0 || count == cr_count) {
				BTMTK_INFO("%s", g_dump_cr_buffer);
				memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
				pos = &g_dump_cr_buffer[0];
				end = pos + BT_CR_DUMP_BUF_SIZE - 1;
			}
		}
	}
}

static void inline bt_dump_bgfsys_top_common_flag(void)
{
	uint32_t value = 0;
	uint32_t i = 0, count = 1, cr_count = 20;
	uint8_t *pos = NULL, *end = NULL;
	int32_t ret = 0;

	memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
	pos = &g_dump_cr_buffer[0];
	end = pos + BT_CR_DUMP_BUF_SIZE - 1;

	BTMTK_INFO("[BGF TOP common/bt part debug flag] Count = (%d)", cr_count);
	for (i = 0x80; i <= 0x93; i++, count++) {
		REG_WRITEL(CON_REG_SPM_BASE_ADDR + 0xAC, i);
		value = REG_READL(CON_REG_SPM_BASE_ADDR + 0x23C);
		ret = snprintf(pos, (end - pos + 1), "%08x ", value);
		if (ret < 0 || ret >= (end - pos + 1))
			break;
		pos += ret;

		if ((count & 0xF) == 0 || count == cr_count) {
			BTMTK_INFO("%s", g_dump_cr_buffer);
			memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
			pos = &g_dump_cr_buffer[0];
			end = pos + BT_CR_DUMP_BUF_SIZE - 1;
		}
	}
}

/* please make sure check bus hang before calling this dump */
static void inline bt_dump_bgfsys_mcu_core_flag(void)
{
	uint32_t value = 0;
	uint32_t i = 0, count = 1, cr_count = 38;
	uint8_t *pos = NULL, *end = NULL;
	int32_t ret = 0;
	uint16_t switch_flag = 0x2B;

	memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
	pos = &g_dump_cr_buffer[0];
	end = pos + BT_CR_DUMP_BUF_SIZE - 1;
	value = REG_READL(CON_REG_SPM_BASE_ADDR + 0xA0);
	value &= 0xFFFFFF03; /* ignore [7:2] */
	value |= (switch_flag << 2);
	REG_WRITEL(CON_REG_SPM_BASE_ADDR + 0xA0, value);

	BTMTK_INFO("[BGF MCU core debug flag] Count = (%d)", cr_count);

	/* gpr0 ~ ipc */
	for (i = 0x3; i <= 0x25000003; i+= 0x1000000, count++) {
		REG_WRITEL(CON_REG_SPM_BASE_ADDR + 0xA8, i);
		value = REG_READL(CON_REG_SPM_BASE_ADDR + 0x22C);
		ret = snprintf(pos, (end - pos + 1), "%08x ", value);
		if (ret < 0 || ret >= (end - pos + 1))
			break;
		pos += ret;

		if ((count & 0xF) == 0 || count == cr_count) {
			BTMTK_INFO("%s", g_dump_cr_buffer);
			memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
			pos = &g_dump_cr_buffer[0];
			end = pos + BT_CR_DUMP_BUF_SIZE - 1;
		}
	}
}

static inline void bt_dump_bgfsys_mcu_pc_log(void)
{
	uint32_t value = 0;
	uint32_t i = 0, count = 1, cr_count = 45;
	uint8_t *pos = NULL, *end = NULL;
	int32_t ret = 0;


	memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
	pos = &g_dump_cr_buffer[0];
	end = pos + BT_CR_DUMP_BUF_SIZE - 1;

	BTMTK_INFO("[BGF MCU PC log] Count = (%d)", cr_count);
	for (i = 0x00; i <= 0x2C; i++, count++) {
		value = REG_READL(CON_REG_SPM_BASE_ADDR + 0xA0);
		value &= 0xFFFFFF03; /* ignore [7:2] */
		value |= (i << 2);
		REG_WRITEL(CON_REG_SPM_BASE_ADDR + 0xA0, value);

		value = REG_READL(CON_REG_SPM_BASE_ADDR + 0x22C);
		ret = snprintf(pos, (end - pos + 1), "%08x ", value);
		if (ret < 0 || ret >= (end - pos + 1))
			break;
		pos += ret;

		if ((count & 0xF) == 0 || count == cr_count) {
			BTMTK_INFO("%s", g_dump_cr_buffer);
			memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
			pos = &g_dump_cr_buffer[0];
			end = pos + BT_CR_DUMP_BUF_SIZE - 1;
		}
	}
}

static void bt_dump_bgfsys_all(void)
{
	/* these dump all belongs to host_csr */
	bt_dump_cpupcr(10, 0);
	bt_dump_bgfsys_host_csr();
	bt_dump_bgfsys_mcu_pc_log();
	bt_dump_bgfsys_mcu_core_flag();
	bt_dump_bgfsys_mcusys_flag();
	bt_dump_bgfsys_bus_flag();
	bt_dump_bgfsys_top_common_flag();
}

/* bt_dump_bgfsys_debug_cr()
 *
 *    Dump all bgfsys debug cr for debuging bus hang
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     N/A
 *
 */
static inline void bt_dump_bgfsys_debug_cr(void)
{
	uint32_t offset = 0x410, value = 0, i = 0;
	uint8_t *pos = NULL, *end = NULL;
	int32_t ret = 0;

	ret = conninfra_is_bus_hang();
	BTMTK_INFO("%s: conninfra_is_bus_hang ret = %d", __func__, ret);

	if (!CAN_DUMP_HOST_CSR(ret)) {
		BTMTK_ERR("%s; host csr is not readable", __func__);
		return;
	}

	if(bgfsys_check_conninfra_ready())
		goto host_csr_only;

	BTMTK_INFO("%s: M0 - M3 semaphore status:", __func__);
	for (i = 0x18070400; i <= 0x18073400; i += 0x1000) {
		value = bt_read_cr(i);
		BTMTK_INFO("[0x%08x] = [0x%08x]", i, value);
	}

	if (conninfra_reg_readable() && !bt_is_bgf_bus_timeout()) {
		BTMTK_INFO("[BGF Bus hang debug CR (18800410~18000444)] Count = (14)");

		memset(g_dump_cr_buffer, 0, BT_CR_DUMP_BUF_SIZE);
		pos = &g_dump_cr_buffer[0];
		end = pos + BT_CR_DUMP_BUF_SIZE - 1;
		for (offset = 0x410; offset <= 0x0444; offset += 4) {
			ret = snprintf(pos, (end - pos + 1), "%08x ",
					 REG_READL(BGF_REG_BASE_ADDR + offset));
			if (ret < 0 || ret >= (end - pos + 1))
				break;
			pos += ret;
		}
		BTMTK_INFO("%s", g_dump_cr_buffer);
	} else
		BTMTK_INFO("conninfra is not readable, skip [BGF Bus hang debug CR]");

host_csr_only:
	bt_dump_bgfsys_all();
	/* release conn_infra force on */
	CLR_BIT(CONN_INFRA_WAKEUP_BT, BIT(0));
}

/* bt_cif_dump_own_cr
 *
 *    Dump fw/driver own relative cr (plus cpucpr) if
 *    driver own or fw own fail
 *
 * Arguments:
 *     N/A
 *
 * Return Value:
 *     N/A
 *
 */
static inline void bt_dump_cif_own_cr(void)
{
	uint32_t value = 0, i = 0;
	int32_t ret = 0;

	ret = conninfra_is_bus_hang();
	BTMTK_INFO("%s: conninfra_is_bus_hang ret = %d", __func__, ret);

	if (!CAN_DUMP_HOST_CSR(ret)) {
		BTMTK_ERR("%s; host csr is not readable", __func__);
		return;
	}

	if(bgfsys_check_conninfra_ready())
		goto host_csr_only;

	/* following CR only accessible while bus is not hang */
	if (!ret ) {
		value = REG_READL(CON_REG_INFRA_CFG_ADDR + 0x400);
		BTMTK_INFO("0x18001400 = [0x%08x]", value);

		value = REG_READL(CON_REG_INFRA_CFG_ADDR + 0x41C);
		BTMTK_INFO("0x1800141C = [0x%08x]", value);

		value = REG_READL(CON_REG_INFRA_CFG_ADDR + 0x420);
		BTMTK_INFO("0x18001420 = [0x%08x]", value);

		value = 0x87654321;
		REG_WRITEL(CON_REG_INFRA_CFG_ADDR + 0x10, value);
		value = REG_READL(CON_REG_INFRA_CFG_ADDR + 0x10);
		BTMTK_INFO("0x18001010 = [0x%08x]", value);

		value = REG_READL(CON_REG_INFRA_CFG_ADDR + 0x160);
		BTMTK_INFO("0x18001160 = [0x%08x]", value);

		value = REG_READL(CONN_INFRA_CFG_START + 0x0168);
		BTMTK_INFO("0x18001168 = [0x%08x]", value);

		value = REG_READL(CONN_INFRA_CFG_START + 0x0170);
		BTMTK_INFO("0x18001170 = [0x%08x]", value);

		value = REG_READL(CONN_INFRA_CFG_BT_PWRCTLCR0);
		BTMTK_INFO("0x18001874 = [0x%08x]", value);

		value = REG_READL(CONN_INFRA_CFG_START + 0x0C00);
		BTMTK_INFO("0x18001C00 = [0x%08x]", value);

		value = REG_READL(CONN_INFRA_CFG_START + 0x0C04);
		BTMTK_INFO("0x18001C04 = [0x%08x]", value);

		BTMTK_INFO("%s: M0 - M3 semaphore status:", __func__);
		for (i = 0x18070400; i <= 0x18073400; i += 0x1000) {
			value = bt_read_cr(i);
			BTMTK_INFO("[0x%08x] = [0x%08x]", i, value);
		}

		value = bt_read_cr(0x18071400);
		BTMTK_INFO("0x18070400 = [0x%08x]", value);

		value = bt_read_cr(0x18070400);
		BTMTK_INFO("0x18070400 = [0x%08x]", value);

		value = bt_read_cr(0x18070400);
		BTMTK_INFO("0x18070400 = [0x%08x]", value);
	}

host_csr_only:
	value = REG_READL(BGF_LPCTL);
	BTMTK_INFO("0x18060030 = [0x%08x]", value);

	value = REG_READL(BGF_IRQ_STAT2);
	BTMTK_INFO("0x1806003C = [0x%08x]", value);

	value = 0x12345678;
	REG_WRITEL(CON_REG_SPM_BASE_ADDR + 0x188, value);
	value = REG_READL(CON_REG_SPM_BASE_ADDR + 0x188);
	BTMTK_INFO("0x18060188 = [0x%08x]", value);

	REG_WRITEL(CON_REG_SPM_BASE_ADDR + 0xA8, 0x194C4BA7);
	BTMTK_INFO("Write [0x180600A8] = [0x194C4BA7]");

	bt_dump_bgfsys_all();
	/* release conn_infra force on */
	CLR_BIT(CONN_INFRA_WAKEUP_BT, BIT(0));
}

/* bgfsys_power_on_dump_cr
 *
 *    Dump CR for debug if power on sequence fail
 *
 * Arguments:
 *    N/A
 *
 * Return Value:
 *    N/A
 *
 */
static inline void bgfsys_power_on_dump_cr(void)
{
	uint32_t i;
	uint32_t val_w, val_r;
	int32_t is_bus_hang = 0;

	is_bus_hang = conninfra_is_bus_hang();
	BTMTK_INFO("%s: conninfra_is_bus_hang ret = %d", __func__, is_bus_hang);

	if (!CAN_DUMP_HOST_CSR(is_bus_hang)) {
		BTMTK_ERR("%s; host csr is not readable", __func__);
		return;
	}

	if(bgfsys_check_conninfra_ready())
		goto host_csr_only;

	BTMTK_INFO("%s: M0 - M3 semaphore status:", __func__);
	for (i = 0x18070400; i <= 0x18073400; i += 0x1000) {
		val_r = bt_read_cr(i);
		BTMTK_INFO("[0x%08x] = [0x%08x]", i, val_r);
	}

	BTMTK_INFO("%s: conninfra part:", __func__);
	val_r = REG_READL(CONN_HOST_CSR_TOP_START + 0x02CC);
	BTMTK_INFO("%s: REG[0x180602CC] read[0x%08x]", __func__, val_r);

	if (!is_bus_hang) {
		val_r = REG_READL(CONN_INFRA_CFG_START + 0x0610);
		BTMTK_INFO("%s: REG[0x18001610] read[0x%08x]", __func__, val_r);
		val_r = REG_READL(CONN_INFRA_CFG_START + 0x0614);
		BTMTK_INFO("%s: REG[0x18001614] read[0x%08x]", __func__, val_r);
		val_r = REG_READL(CONN_INFRA_CFG_START + 0x0150);
		BTMTK_INFO("%s: REG[0x18001150] read[0x%08x]", __func__, val_r);
		val_r = REG_READL(CONN_INFRA_CFG_START + 0x0170);
		BTMTK_INFO("%s: REG[0x18001170] read[0x%08x]", __func__, val_r);

	//REG_WRITEL(CONN_INFRA_CFG_START + 0x0610, BIT(1));
	//val_r = REG_READL(CONN_INFRA_CFG_START + 0x0610);
	//BTMTK_INFO("%s: REG[0x18001610] write[0x%08x], REG[0x18001610] read[0x%08x]", __func__, BIT(1), val_r);
		bt_write_cr(0x1800F000, 0x32C8001C);
	}

host_csr_only:
	for(i = 0x0F; i >= 0x01; i--) {
		val_w = (i << 16) + 0x0001;
		REG_WRITEL(CONN_HOST_CSR_TOP_START + 0x0128, val_w);
		val_r = REG_READL(CONN_HOST_CSR_TOP_START + 0x0148);
		BTMTK_INFO("%s: REG[0x18060128] write[0x%08x], REG[0x18060148] read[0x%08x]", __func__, val_w, val_r);
	}
	for(i = 0x03; i >= 0x01; i--) {
		val_w = (i << 16) + 0x0002;
		REG_WRITEL(CONN_HOST_CSR_TOP_START + 0x0128, val_w);
		val_r = REG_READL(CONN_HOST_CSR_TOP_START + 0x0148);
		BTMTK_INFO("%s: REG[0x18060128] write[0x%08x], REG[0x18060148] read[0x%08x]", __func__, val_w, val_r);
	}
	for(i = 0x04; i >= 0x01; i--) {
		val_w = (i << 16) + 0x0003;
		REG_WRITEL(CONN_HOST_CSR_TOP_START + 0x0128, val_w);
		val_r = REG_READL(CONN_HOST_CSR_TOP_START + 0x0148);
		BTMTK_INFO("%s: REG[0x18060128] write[0x%08x], REG[0x18060148] read[0x%08x]", __func__, val_w, val_r);
	}

	BTMTK_INFO("%s: bgf part:", __func__);
	val_r = REG_READL(CONN_HOST_CSR_TOP_START + 0x022C);
	BTMTK_INFO("%s: REG[0x1806022C] read[0x%08x]", __func__, val_r);
	val_r = REG_READL(CONN_HOST_CSR_TOP_START + 0x0230);
	BTMTK_INFO("%s: REG[0x18060230] read[0x%08x]", __func__, val_r);
	val_r = REG_READL(CONN_HOST_CSR_TOP_START + 0x0234);
	BTMTK_INFO("%s: REG[0x18060234] read[0x%08x]", __func__, val_r);
	val_r = REG_READL(CONN_HOST_CSR_TOP_START + 0x0238);
	BTMTK_INFO("%s: REG[0x18060238] read[0x%08x]", __func__, val_r);
	val_r = REG_READL(CON_REG_SPM_BASE_ADDR + 0x268);
	BTMTK_INFO("%s: REG[0x18060268] read[0x%08x]", __func__, val_r);
	val_r = REG_READL(CON_REG_SPM_BASE_ADDR + 0x26C);
	BTMTK_INFO("%s: REG[0x1806026C] read[0x%08x]", __func__, val_r);

	bt_dump_bgfsys_all();
	/* release conn_infra force on */
	CLR_BIT(CONN_INFRA_WAKEUP_BT, BIT(0));
}
#endif
