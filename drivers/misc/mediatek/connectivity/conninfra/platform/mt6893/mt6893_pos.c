// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "consys_hw.h" /* for semaphore index */
/* platform dependent */
#include "plat_def.h"
/* macro for read/write cr */
#include "consys_reg_util.h"
#include "consys_reg_mng.h"
/* cr base address */
#include "mt6893_consys_reg.h"
/* cr offset */
#include "mt6893_consys_reg_offset.h"
/* For function declaration */
#include "mt6893_pos.h"
#include "mt6893.h"

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#define CONN_INFRA_SYSRAM__A_DIE_DIG_TOP_CK_EN_MASK   0x7f

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
struct a_die_reg_config {
	unsigned int reg;
	unsigned int mask;
	unsigned int config;
};

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
const static char* gAdieCtrlType[CONNSYS_ADIE_CTL_MAX] =
{
	"CONNSYS_ADIE_CTL_HOST_BT",
	"CONNSYS_ADIE_CTL_HOST_FM",
	"CONNSYS_ADIE_CTL_HOST_GPS",
	"CONNSYS_ADIE_CTL_HOST_WIFI",
	"CONNSYS_ADIE_CTL_HOST_CONNINFRA",
	"CONNSYS_ADIE_CTL_FW_BT",
	"CONNSYS_ADIE_CTL_FW_WIFI",
};

const static char* g_spi_system_name[SYS_SPI_MAX] = {
	"SYS_SPI_WF1",
	"SYS_SPI_WF",
	"SYS_SPI_BT",
	"SYS_SPI_FM",
	"SYS_SPI_GPS",
	"SYS_SPI_TOP",
	"SYS_SPI_WF2",
	"SYS_SPI_WF3",
};

#define ADIE_CONFIG_NUM	2

// E1 WF/GPS/FM on(default)
const static struct a_die_reg_config adie_e1_default[ADIE_CONFIG_NUM] =
{
	{ATOP_RG_TOP_XTAL_01, 0xc180, 0xc180},
	{ATOP_RG_TOP_XTAL_02, 0xf0ff0080, 0xd0550080},
};

const static struct a_die_reg_config adie_e1_bt_only[ADIE_CONFIG_NUM] =
{
	{ATOP_RG_TOP_XTAL_01, 0xc180, 0x100},
	{ATOP_RG_TOP_XTAL_02, 0xf0ff0080, 0xf0ff0000},
};

const static struct a_die_reg_config adie_e2_default[ADIE_CONFIG_NUM] =
{
	{ATOP_RG_TOP_XTAL_01, 0xc180, 0xc180},
	{ATOP_RG_TOP_XTAL_02, 0xf0ff0080, 0x50550080},
};

const static struct a_die_reg_config adie_e2_bt_only[ADIE_CONFIG_NUM] =
{
	{ATOP_RG_TOP_XTAL_01, 0xc180, 0x100},
	{ATOP_RG_TOP_XTAL_02, 0xf0ff0080, 0x70ff0000},
};

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static int consys_spi_read_nolock(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int *data);
static int consys_spi_write_nolock(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int data);
static void consys_spi_write_offset_range_nolock(
	enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int value,
	unsigned int reg_offset, unsigned int value_offset, unsigned int size);
static int connsys_a_die_thermal_cal(int efuse_valid, unsigned int efuse);

unsigned int consys_emi_set_remapping_reg(
	phys_addr_t con_emi_base_addr,
	phys_addr_t md_shared_emi_base_addr)
{
	/* EMI Registers remapping */
	CONSYS_REG_WRITE_OFFSET_RANGE(CON_REG_HOST_CSR_ADDR + CONN2AP_REMAP_MCU_EMI_BASE_ADDR_OFFSET,
					  con_emi_base_addr, 0, 16, 20);

	CONSYS_REG_WRITE_MASK(
		CON_REG_HOST_CSR_ADDR + CONN2AP_REMAP_WF_PERI_BASE_ADDR_OFFSET,
		0x01000, 0xFFFFF);
	CONSYS_REG_WRITE_MASK(
		CON_REG_HOST_CSR_ADDR + CONN2AP_REMAP_BT_PERI_BASE_ADDR_OFFSET,
		0x01000, 0xFFFFF);
	CONSYS_REG_WRITE_MASK(
		CON_REG_HOST_CSR_ADDR + CONN2AP_REMAP_GPS_PERI_BASE_ADDR_OFFSET,
		0x01000, 0xFFFFF);

	if (md_shared_emi_base_addr) {
		CONSYS_REG_WRITE_OFFSET_RANGE(
			CON_REG_HOST_CSR_ADDR + CONN2AP_REMAP_MD_SHARE_EMI_BASE_ADDR_OFFSET,
			md_shared_emi_base_addr, 0, 16, 20);
	}
	pr_info("connsys_emi_base=[0x%llx] mcif_emi_base=[0x%llx] remap cr: connsys=[0x%08x] mcif=[0x%08x]\n",
		con_emi_base_addr, md_shared_emi_base_addr,
		CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN2AP_REMAP_MCU_EMI_BASE_ADDR_OFFSET),
		CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN2AP_REMAP_MD_SHARE_EMI_BASE_ADDR_OFFSET));
	return 0;
}

int consys_conninfra_on_power_ctrl(unsigned int enable)
{
	int check;
	if (enable) {
#ifndef CONFIG_FPGA_EARLY_PORTING
		/* Turn on SPM clock (apply this for SPM's CONNSYS power control related CR accessing)
		 * address: 0x1000_6000[0]
		 *          0x1000_6000[31:16]
		 * Data: [0]=1'b1
		 *       [31:16]=16'h0b16 (key)
		 * Action: write
		 */
		CONSYS_REG_WRITE_MASK(
			CON_REG_SPM_BASE_ADDR + SPM_POWERON_CONFIG_EN, 0x0b160001, 0xffff0001);
#endif

		/* Turn on ap2conn host_ck CG (ECO)
		 * Address: INFRA_AP2MD_GALS_CTL[0] (0x1020E504[0])
		 * Value: 1'b1
		 * Action: write
		 */
		CONSYS_SET_BIT(CON_REG_INFRACFG_BASE_ADDR + INFRA_AP2MD_GALS_CTL, 0x1);

#if MTK_CONNINFRA_CLOCK_BUFFER_API_AVAILABLE
		check = consys_platform_spm_conn_ctrl(enable);
		if (check) {
			pr_err("Turn on conn_infra power fail\n");
			return -1;
		}
#else
		pr_info("Turn on conn_infra power by POS steps\n");
		/* Assert "conn_infra_on" primary part power on, set "connsys_on_domain_pwr_on"=1
		 * Address: 0x1000_6304[2]
		 * Data: 1'b1
		 * Action: write
		 */
		CONSYS_SET_BIT(CON_REG_SPM_BASE_ADDR + SPM_CONN_PWR_CON, 0x4);

#ifndef CONFIG_FPGA_EARLY_PORTING
		/* Check "conn_infra_on" primary part power status, check "connsys_on_domain_pwr_ack"=1
		 * (polling "10 times" and each polling interval is "0.5ms")
		 * Address: 0x1000_616C[1]
		 * Data: 1'b1
		 * Action: polling
		 */
		check = 0;
		CONSYS_REG_BIT_POLLING(CON_REG_SPM_BASE_ADDR + SPM_PWR_STATUS, 1, 1, 10, 500, check);
		if (check != 0)
			pr_err("Check conn_infra_on primary power fail. 0x1000_616C is 0x%08x. Expect [1] as 1.\n",
				CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_PWR_STATUS));
#endif

		/* Assert "conn_infra_on" secondary part power on, set "connsys_on_domain_pwr_on_s"=1
		 * Address: 0x1000_6304[3]
		 * Data: 1'b1
		 * Action: write
		 */
		CONSYS_SET_BIT(CON_REG_SPM_BASE_ADDR + SPM_CONN_PWR_CON, 0x8);

#ifndef CONFIG_FPGA_EARLY_PORTING
		/* Check "conn_infra_on" secondary part power status,
		 * check "connsys_on_domain_pwr_ack_s"=1
		 * (polling "10 times" and each polling interval is "0.5ms")
		 * Address: 0x1000_6170[1]
		 * Data: 1'b1
		 * Action: polling
		 */
		check = 0;
		CONSYS_REG_BIT_POLLING(CON_REG_SPM_BASE_ADDR + SPM_PWR_STATUS_2ND, 1, 1, 10, 500, check);
		if (check != 0)
			pr_err("Check conn_infra_on secondary power fail. 0x1000_6170 is 0x%08x. Expect [1] as 1.\n",
				CONSYS_REG_READ(CON_REG_SPM_BASE_ADDR + SPM_PWR_STATUS_2ND));
#endif

		/* Turn on AP-to-CONNSYS bus clock, set "conn_clk_dis"=0
		 * (apply this for bus clock toggling)
		 * Address: 0x1000_6304[4]
		 * Data: 1'b0
		 * Action: write
		 */
		CONSYS_CLR_BIT(CON_REG_SPM_BASE_ADDR + SPM_CONN_PWR_CON, 0x10);

		/* Wait 1 us */
		udelay(1);

		/* De-assert "conn_infra_on" isolation, set "connsys_iso_en"=0
		 * Address: 0x1000_6304[1]
		 * Data: 1'b0
		 * Action: write
		 */
		CONSYS_CLR_BIT(CON_REG_SPM_BASE_ADDR + SPM_CONN_PWR_CON, 0x2);

		/* De-assert CONNSYS S/W reset (TOP RGU CR),
		 * set "ap_sw_rst_b"=1
		 * Address: WDT_SWSYSRST[9] (0x1000_7018[9])
		 *          WDT_SWSYSRST[31:24] (0x1000_7018[31:24])
		 * Data: [9]=1'b0
		 *       [31:24]=8'h88 (key)
		 * Action: Write
		 */
		CONSYS_REG_WRITE_MASK(
			CON_REG_TOP_RGU_ADDR + TOP_RGU_WDT_SWSYSRST,0x88000000, 0xff000200);

		/* De-assert CONNSYS S/W reset (SPM CR), set "ap_sw_rst_b"=1
		 * Address: CONN_PWR_CON[0] (0x1000_6304[0])
		 * Data: 1'b1
		 * Action: write
		 */
		CONSYS_SET_BIT(CON_REG_SPM_BASE_ADDR + SPM_CONN_PWR_CON, 0x1);

		/* Wait 0.5ms  */
		udelay(500);

		/* Disable AXI bus sleep protect */
		/* Turn off AXI RX bus sleep protect (AP2CONN AXI Bus protect)
		 * (apply this for INFRA AXI bus accessing when CONNSYS had been turned on)
		 * Address: 0x1000_1718[31:0] (INFRA_TOPAXI_PROTECTEN2_CLR)
		 * Data: 0x0000_0002
		 * Action: write
		 */
		CONSYS_REG_WRITE(
			CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN2_CLR_OFFSET,
			0x00000002);

#ifndef CONFIG_FPGA_EARLY_PORTING
		/* Check AXI RX bus sleep protect turn off
		 * (polling "100 times" and each polling interval is "0.5ms")
		 * Address: 0x1000_1724[2] (INFRA_TOPAXI_PROTECTEN2_STA1[2])
		 * Data: 1'b0
		 * Action: polling
		 */
		check = 0;
		CONSYS_REG_BIT_POLLING(
			CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN2_STA1_OFFSET,
			2, 0, 100, 500, check);
		if (check != 0)
			pr_err("Polling AXI RX bus sleep protect turn off fail. status=0x%08x\n",
				CONSYS_REG_READ(CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN2_STA1_OFFSET));
#endif

		/* Turn off AXI Rx bus sleep protect (CONN2AP AXI Rx Bus protect)
		 * (disable sleep protection when CONNSYS had been turned on)
		 * Note : Should turn off AXI Rx sleep protection first.
		 */
		CONSYS_REG_WRITE(
			CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN_CLR_OFFSET,
			0x00004000);
#if 0 /* POS update 20190819: Skip check */
#ifndef CONFIG_FPGA_EARLY_PORTING
		/* Check AXI Rx bus sleep protect turn off
		 * (polling "10 times" and each polling interval is "0.5ms")
		 * Address: 0x1000_1228[14] (INFRA_TOPAXI_PROTECTEN_STA1[14])
		 * Data: 1'b0
		 * Action: polling
		 */
		CONSYS_REG_BIT_POLLING(
			CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN_STA1_OFFSET,
			14, 0, 10, 50, check);
		if (check != 0)
			pr_err("Polling AXI bus sleep protect turn off fail. Status=0x%08x\n",
				CONSYS_REG_READ(CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN_STA1_OFFSET));
#endif
#endif
		/* Turn off AXI TX bus sleep protect (AP2CONN AXI Bus protect)
		 * (apply this for INFRA AXI bus accessing when CONNSYS had been turned on)
		 */
		CONSYS_REG_WRITE(
			CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN_CLR_OFFSET,
			0x00002000);

#ifndef CONFIG_FPGA_EARLY_PORTING
		/* Check AXI TX bus sleep protect turn off
		 * (polling "100 times" and each polling interval is "0.5ms")
		 * Address: 0x1000_1228[13] (INFRA_TOPAXI_PROTECTEN_STA1[13])
		 * Data: 1'b0
		 * Action: polling
		 */
		CONSYS_REG_BIT_POLLING(
			CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN_STA1_OFFSET,
			13, 0, 100, 500, check);
		if (check != 0)
			pr_err("polling AXI TX bus sleep protect turn off fail. Status=0x%08x\n",
				CONSYS_REG_READ(CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN_STA1_OFFSET));
#endif
#endif /* MTK_CONNINFRA_CLOCK_BUFFER_API_AVAILABLE */
		/* Wait 6ms (apply this for CONNSYS XO clock ready)
		 * [NOTE]
		 *   This setting could be changed at different design architecture
		 * and the characteristic of AFE WBG)
		 */
		mdelay(6);
	} else {

		/* Enable AXI bus sleep protect */
#if MTK_CONNINFRA_CLOCK_BUFFER_API_AVAILABLE
		pr_info("Turn off conn_infra power by SPM API\n");
		check = consys_platform_spm_conn_ctrl(enable);
		if (check) {
			pr_err("Turn off conn_infra power fail, ret=%d\n", check);
			return -1;
		}
#else
		/* Turn on AXI TX bus sleep protect (AP2CONN AXI Bus protect)
		 * (apply this for INFRA AXI bus protection to prevent bus hang when
		 * CONNSYS had been turned off)
		 * Address: 0x1000_12a0[31:0]
		 * Data: 0x0000_2000
		 * Action: write
		 */
		CONSYS_REG_WRITE(
			CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN_SET_OFFSET,
			0x00002000);

		/* check AXI TX bus sleep protect turn on (polling "100 times")
		 * Address: 0x1000_1228[13]
		 * Data: 1'b1
		 * Action: polling
		 */
		CONSYS_REG_BIT_POLLING(
			CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN_STA1_OFFSET,
			13, 1, 100, 500, check);
		if (check)
			pr_err("Polling AXI TX bus sleep protect turn on fail.\n");

		/* Turn on AXI Rx bus sleep protect (CONN2AP AXI RX Bus protect)
		 * (apply this for INFRA AXI bus protection to prevent bus hang when
		 * CONNSYS had been turned off)
		 * Note:
		 *	Should turn on AXI Rx sleep protection after
		 *	AXI Tx sleep protection has been turn on.
		 * Address: 0x1000_12A0[31:0]
		 * Data: 0x0000_4000
		 * Action: write
		 */
		CONSYS_REG_WRITE(
			CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN_SET_OFFSET,
			0x00004000);

		/* check AXI Rx bus sleep protect turn on
		 * (polling "100 times", polling interval is 1ms)
		 * Address: 0x1000_1228[14]
		 * Data: 1'b1
		 * Action: polling
		 */
		CONSYS_REG_BIT_POLLING(
			CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN_STA1_OFFSET,
			14, 1, 100, 1000, check);
		if (check)
			pr_err("Polling AXI Rx bus sleep protect turn on fail.\n");

		/* Turn on AXI RX bus sleep protect (AP2CONN AXI Bus protect)
		 * (apply this for INFRA AXI bus protection to prevent bus hang when
		 * CONNSYS had been turned off)
		 * Address: 0x1000_1714[31:0] (INFRA_TOPAXI_PROTECTEN2_SET)
		 * Data: 0x0000_0002
		 * Action: write
		 */
		CONSYS_REG_WRITE(
			CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN2_SET_OFFSET,
			0x00000002);
		/* check AXI RX bus sleep protect turn on (polling "10 times")
		 * Address: 0x1000_1724[2] (INFRA_TOPAXI_PROTECTEN2_STA1[2])
		 * Value: 1'b1
		 * Action: polling
		 */
		CONSYS_REG_BIT_POLLING(
			CON_REG_INFRACFG_AO_ADDR + INFRA_TOPAXI_PROTECTEN2_STA1_OFFSET,
			2, 1, 10, 1000, check);
		if (check)
			pr_err("Polling AXI RX bus sleep protect turn on fail.\n");

		/* Assert "conn_infra_on" isolation, set "connsys_iso_en"=1
		 * Address: CONN_PWR_CON[1] (0x1000_6304[1])
		 * Value: 1'b1
		 * Action: write
		 */
		CONSYS_SET_BIT(CON_REG_SPM_BASE_ADDR + SPM_CONN_PWR_CON, 0x2);

		/* Assert CONNSYS S/W reset (SPM CR), set "ap_sw_rst_b"=0
		 * Address: CONN_PWR_CON[0] (0x1000_6304[0])
		 * Value: 1'b0
		 * Action: write
		 */
		CONSYS_CLR_BIT(CON_REG_SPM_BASE_ADDR + SPM_CONN_PWR_CON, 0x1);

		/* Assert CONNSYS S/W reset(TOP RGU CR), set "ap_sw_rst_b"=0
		 * Address: WDT_SWSYSRST[9] (0x1000_7018[9])
		 *          WDT_SWSYSRST[31:24] (0x1000_7018[31:24])
		 * Value: [9]=1'b1
		 *        [31:24]=8'h88 (key)
		 * Action: write
		 * Note: this CR value for reset control is active high (0: not reset, 1: reset)
		 */
		CONSYS_REG_WRITE_MASK(
			CON_REG_TOP_RGU_ADDR + TOP_RGU_WDT_SWSYSRST,
			0x88000200,
			0xff000200);

		/* Turn off AP-to-CONNSYS bus clock, set "conn_clk_dis"=1
		 * (apply this for bus clock gating)
		 * Address: CONN_PWR_CON[4] (0x1000_6304[4])
		 * Value: 1'b1
		 * Action: write
		 */
		CONSYS_SET_BIT(CON_REG_SPM_BASE_ADDR + SPM_CONN_PWR_CON, 0x10);

		/* wait 1us (?) */
		udelay(1);

		/* De-assert "conn_infra_on" primary part power on,
		 * set "connsys_on_domain_pwr_on"=0
		 * Address: CONN_PWR_CON[2] (0x1000_6304[2])
		 * Value: 1'b0
		 * Action: write
		 */
		CONSYS_CLR_BIT(CON_REG_SPM_BASE_ADDR + SPM_CONN_PWR_CON, 0x4);

		/* De-assert "conn_infra_on" secondary part power on,
		 * set "connsys_on_domain_pwr_on_s"=0
		 * Address: CONN_PWR_CON[3] (0x1000_6304[3])
		 * Value: 1'b0
		 * Action: write
		 */
		CONSYS_CLR_BIT(CON_REG_SPM_BASE_ADDR + SPM_CONN_PWR_CON, 0x8);
#endif /* MTK_CONNINFRA_CLOCK_BUFFER_API_AVAILABLE */

		/* Turn off ap2conn host_ck CG (ECO)
		 * Address: INFRA_AP2MD_GALS_CTL[0] (0x1020E504[0])
		 * Data: 1'b0
		 * Action: write
		 */
		CONSYS_CLR_BIT(CON_REG_INFRACFG_BASE_ADDR + INFRA_AP2MD_GALS_CTL, 0x1);
	}
	return 0;
}

int consys_conninfra_wakeup(void)
{
	int check, r1, r2;
	unsigned int retry = 10;
	unsigned int consys_hw_ver = 0;
	int polling_fail_retry = 2;
	unsigned long polling_fail_delay = 20000; /* 20ms */

	/* Wake up conn_infra
	 * Address: CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_CONN_INFRA_WAKEPU_TOP (0x180601a0)
	 * Data: 1'b1
	 * Action: write
	 */
	CONSYS_REG_WRITE(
		CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_CONN_INFRA_WAKEPU_TOP,
		0x1);

	/* Check ap2conn slpprot_rdy
	 * (polling "10 times" for specific project code and each polling interval is "1ms")
	 * Address: CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL_CONN_INFRA_ON2OFF_SLP_PROT_ACK (0x1806_0184[5])
	 * Data: 1'b0
	 * Action: polling
	 */
	while (polling_fail_retry > 0) {
		check = 0;
		CONSYS_REG_BIT_POLLING(
			CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL, 5, 0, 10, 1000, check);

		if (check != 0) {
			pr_err("[%s] Check ap2conn slpprot_rdy fail. value=0x%x WAKEUP_TOP=[0x%x]\n",
				__func__,
				CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL),
				CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_CONN_INFRA_WAKEPU_TOP));
		}

		if (consys_reg_mng_reg_readable() == 0) {
			check = consys_reg_mng_is_bus_hang();
			r1 = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_DBG_DUMMY_3);
			r2 = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_CONN_INFRA_WAKEPU_TOP);
			pr_info("[%s] check=[%d] r1=[0x%x] r2=[0x%x]", __func__, check, r1, r2);
			consys_reg_mng_dump_cpupcr(CONN_DUMP_CPUPCR_TYPE_ALL, 10, 200);
			if (check > 0) {
				return -1;
			}
			check = -1;
		}

		if (check == 0)
			break;

		/* delay 20 ms */
		usleep_range(polling_fail_delay - 200, polling_fail_delay + 200);
		polling_fail_retry--;
	}

	if (check != 0) {
		pr_err("[%s] wakeup fail retry %d", __func__, polling_fail_retry);
		return -1;
	}

	/* Check CONNSYS version ID
	 * (polling "10 times" for specific project code and each polling interval is "1ms")
	 * Address: CONN_HW_VER (0x1800_1000[31:0])
	 * Data: 32'h2001_0101
	 * Action: polling
	 */
	check = 0;
	while (retry-- > 0) {
		consys_hw_ver = CONSYS_REG_READ(
					CON_REG_INFRA_CFG_ADDR +
					CONN_HW_VER_OFFSET);
		if (consys_hw_ver == CONN_HW_VER) {
			check = 0;
			pr_info("[%s] Polling chip id success (0x%x)\n", __func__, consys_hw_ver);
			break;
		}
		check = -1;
	}
	if (check != 0)
		pr_err("[%s] Polling chip id fail (0x%x)\n", __func__, consys_hw_ver);

	return check;
}

int consys_conninfra_sleep(void)
{
	/* Release conn_infra force on
	 * Address: CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_CONN_INFRA_WAKEPU_TOP (0x180601a0)
	 * Data: 1'b0
	 * Action: write
	 */
	CONSYS_REG_WRITE(
		CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_CONN_INFRA_WAKEPU_TOP,
		0x0);

	return 0;
}

void consys_set_if_pinmux(unsigned int enable)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	if (enable) {
		/* Set pinmux for the interface between D-die and A-die
		 * (CONN_HRST_B / CONN_TOP_CLK / CONN_TOP_DATA / CONN_WB_PTA /
		 *  CONN_BT_CLK / CONN_BT_DATA / CONN_WF_CTRL0 / CONN_WF_CTRL1 /
		 *  CONN_WF_CTRL2 / CONN_WF_CTRL3 / CONN_WF_CTRL4)
		 */
		/* GPIO 172: 0x1000_5450[18:16] */
		/* GPIO 173: 0x1000_5450[22:20] */
		/* GPIO 174: 0x1000_5450[26:24] */
		/* GPIO 175: 0x1000_5450[30:28] */
		CONSYS_REG_WRITE_MASK(GPIO_BASE_ADDR + GPIO_MODE21, 0x11110000, 0xffff0000);

		/* GPIO 176: 0x1000_5460[2:0] */
		/* GPIO 177: 0x1000_5460[6:4] */
		/* GPIO 178: 0x1000_5460[10:8] */
		/* GPIO 179: 0x1000_5460[14:12] */
		/* GPIO 180: 0x1000_5460[18:16] */
		/* GPIO 181: 0x1000_5460[22:20] */
		/* GPIO 182: 0x1000_5460[26:24] */
		CONSYS_REG_WRITE_MASK(GPIO_BASE_ADDR + GPIO_MODE22, 0x01111111, 0x0fffffff);

		/* Set pinmux driving to 2mA
		 * Address:
		 *   0x11EA_0000[5:3]/0x11EA_0000[8:6]/0x11EA_0000[11:9]
		 *   0x11EA_0000[14:12]/0x11EA_0000[17:15]
		 * Data: 3'b000
		 * Action: write
		 * Note: IOCFG_RT
		 */
		CONSYS_REG_WRITE_MASK(IOCFG_RT_ADDR + IOCFG_RT_DRV_CFG0, 0x0, 0x3fff8);

		/* Set CONN_TOP_CLK/CONN_TOP_DATA/CONN_BT_CLK/CONN_BT_DATA driving to 4mA
		 * (CONN_TOP_DATA driving config is the same position with
		 *  CONN_WF_CTRL0/CONN_WF_CTRL1/CONN_WF_CTRL2/CONN_WF_CTRL3/CONN_WF_CTRL4)
		 * Address:
		 *   CONN_TOP_CLK	0x11EA_0000[17:15] = 3b'001
		 *   CONN_TOP_DATA	0x11EA_0000[5:3] = 3b'001
		 *   CONN_BT_CLK	0x11EA_0000[8:6] = 3b'001
		 *   CONN_BT_DATA	0x11EA_0000[11:9] = 3b'001
		 * Action: write
		 */
		CONSYS_REG_WRITE_MASK(IOCFG_RT_ADDR + IOCFG_RT_DRV_CFG0, 0x8248, 0x38ff8);

		/* Set pinmux PUPD setting
		 * Clear CONN_TOP_DATA/CONN_BT_DATA PD setting
		 * Address: 0x11EA_0058[14][11]
		 * Data: 2'b11
		 * Action: write
		 * Note: IOCFG_RT
		 */
		CONSYS_REG_WRITE_MASK(IOCFG_RT_ADDR + IOCFG_RT_PD_CFG0_CLR, 0x4800, 0x4800);

		/* Set pinmux PUPD
		 * Setting CONN_TOP_DATA/CONN_BT_DATA as PU
		 * Address: 0x11EA_0074[14][11]
		 * Data: 2'b11
		 * Action: write
		 * Note: IOCFG_RT
		 */
		CONSYS_REG_WRITE_MASK(IOCFG_RT_ADDR + IOCFG_RT_PU_CFG0_SET, 0x4800, 0x4800);

		/* If TCXO mode, set GPIO155 pinmux for TCXO mode (AUX4)
		 * (CONN_TCXOENA_REQ)
		 * Address: 0x1000_5430[14:12]
		 * Data: 3'b100
		 * Action: write
		 */
		if (consys_co_clock_type() == CONNSYS_CLOCK_SCHEMATIC_26M_EXTCXO) {
			/* TODO: need customization for TCXO GPIO */
			CONSYS_REG_WRITE_MASK(GPIO_BASE_ADDR + GPIO_MODE19, 0x4000, 0x7000);
		}
	} else {
		/* Set pinmux for the interface between D-die and A-die (Aux0)
		 * Address:
		 *   0x1000_5450[26:24]/0x1000_5450[18:16]/0x1000_5450[22:20]
		 *   0x1000_5450[30:28]/0x1000_5460[2:0]/0x1000_5460[6:4]
		 *   0x1000_5460[10:8]/0x1000_5460[14:12]/0x1000_5460[18:16]
		 *   0x1000_5460[22:20]/0x1000_5460[26:24]
		 * Data: 3'b000
		 * Action: write
		 */
		CONSYS_REG_WRITE_MASK(GPIO_BASE_ADDR + GPIO_MODE21, 0x0, 0xffff0000);
		CONSYS_REG_WRITE_MASK(GPIO_BASE_ADDR + GPIO_MODE22, 0x0, 0x0fffffff);

		/* Set pinmux PUPD setting
		 * Clear CONN_TOP_DATA/CONN_BT_DATA PU setting
		 * Address: 0x11EA_0078[14][11]
		 * Data: 2'b11
		 * Action: write
		 */
		CONSYS_REG_WRITE_MASK(IOCFG_RT_ADDR + IOCFG_RT_PU_CFG0_CLR, 0x4800, 0x4800);

		/* Set pinmux PUPD setting
		 * CONN_TOP_DATA/CONN_BT_DATA as PD
		 * Address: 0x11EA_0054[14][11]
		 * Data: 2'b11
		 *Action: write
		 */
		CONSYS_REG_WRITE_MASK(IOCFG_RT_ADDR + IOCFG_RT_PD_CFG0_SET, 0x4800, 0x4800);

		/* If TCXO mode, set GPIO155 pinmux to GPIO mode
		 * Address: 0x1000_5430[14:12]
		 * Data: 3'b000
		 * Action: write
		 */
		if (consys_co_clock_type() == CONNSYS_CLOCK_SCHEMATIC_26M_EXTCXO) {
			CONSYS_REG_WRITE_MASK(GPIO_BASE_ADDR + GPIO_MODE19, 0x0, 0x7000);
		}
	}
#else
	pr_info("[%s] not for FPGA\n", __func__);
#endif
}

int consys_polling_chipid(void)
{
	unsigned int retry = 11;
	unsigned int consys_hw_ver = 0;
	unsigned int consys_configuration_id = 0;
	int ret = -1;
	int check;

	/* Check ap2conn slpprot_rdy
	 * (polling "10 times" for specific project code and each polling interval is "1ms")
	 * Address:
	 * 	CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL_CONN_INFRA_ON2OFF_SLP_PROT_ACK
	 * 	0x1806_0184[5]
	 * Value: 1'b0
	 * Action: polling
	 * Note: deadfeed CDC issue
	 */
	check = 0;
	CONSYS_REG_BIT_POLLING(
		CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL, 5, 0, 10, 1000, check);
	if (check) {
		pr_err("[%s] Check ap2conn slpprot_rdy fail. value=0x%x WAKEUP_TOP=[0x%x]\n",
			__func__,
			CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL),
			CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_CONN_INFRA_WAKEPU_TOP));
	}

	while (--retry > 0) {
		consys_hw_ver = CONSYS_REG_READ(
					CON_REG_INFRA_CFG_ADDR +
					CONN_HW_VER_OFFSET);
		if (consys_hw_ver == CONN_HW_VER) {
			consys_configuration_id = CONSYS_REG_READ(
				CON_REG_INFRA_CFG_ADDR + CONN_CFG_ID_OFFSET);
			pr_info("Consys HW version id(0x%x) cfg_id=(0x%x)\n",
				consys_hw_ver, consys_configuration_id);
			ret = 0;
			break;
		}
		msleep(20);
	}

	if (retry == 0) {
		check = consys_reg_mng_is_bus_hang();
		pr_err("Read CONSYS version id fail. Expect 0x%x but get 0x%x\n, bus hang check=[%d]",
			CONN_HW_VER, consys_hw_ver, check);
		return -1;
	}

	/* Disable conn_infra deadfeed function(workaround)
	 * Address: CONN_HOST_CSR_TOP_CSR_DEADFEED_EN_CR_AP2CONN_DEADFEED_EN (0x1806_0124[0])
	 * Data: 1'b0
	 * Action: write
	 * Note: workaround for deadfeed CDC issue
	 */
	CONSYS_CLR_BIT(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CSR_DEADFEED_EN_CR, 0x1);

#ifdef CONFIG_FPGA_EARLY_PORTING
	/* For FPGA workaround */
	CONSYS_SET_BIT(CON_REG_INFRA_CFG_ADDR + 0x0c04,
		((0x1 << 1) | (0x1 << 9) | (0x1 << 17) | (0x1 << 25)));
	pr_info("Workaround for FPGA: Check %x\n", CON_REG_INFRA_CFG_ADDR + 0x0c04);
#endif

	return ret;
}

int connsys_d_die_cfg(void)
{
	/* Read D-die Efuse
	 * Address: AP2CONN_EFUSE_DATA (0x1800_1818)
	 * Data:
	 * Action: read
	 */
	CONSYS_REG_WRITE(
		CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_D_DIE_EFUSE,
		CONSYS_REG_READ(CON_REG_INFRA_CFG_ADDR + AP2CONN_EFUSE_DATA));

	/* Request from MCU, pass build type to FW
	 * 1: eng build
	 * 2: userdebug build
	 * 3: user build
	 */
#if defined(CONNINFRA_PLAT_BUILD_MODE)
	CONSYS_REG_WRITE(
		CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_BUILD_MODE,
		CONNINFRA_PLAT_BUILD_MODE);
	pr_info("Write CONN_INFRA_SYSRAM_SW_CR_BUILD_MODE to 0x%08x\n",
		CONSYS_REG_READ(CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_BUILD_MODE));
#endif

	/* conn_infra sysram hw control setting -> disable hw power down
	 * Address: CONN_INFRA_RGU_SYSRAM_HWCTL_PDN_SYSRAM_HWCTL_PDN (0x1800_0038)
	 * Data: 32'h0
	 * Action: write
	 */
	CONSYS_REG_WRITE(CON_REG_INFRA_RGU_ADDR + CONN_INFRA_RGU_SYSRAM_HWCTL_PDN, 0x0);

	/* conn_infra sysram hw control setting -> enable hw sleep
	 * Address: CONN_INFRA_RGU_SYSRAM_HWCTL_SLP_SYSRAM_HWCTL_SLP (0x1800_003C)
	 * Data: 32'h0000_00FF
	 * Action: write
	 */
	CONSYS_REG_WRITE(CON_REG_INFRA_RGU_ADDR + CONN_INFRA_RGU_SYSRAM_HWCTL_SLP, 0x000000ff);

	/* co-ext memory  hw control setting -> disable hw power down
	 * Address: CONN_INFRA_RGU_CO_EXT_MEM_HWCTL_PDN_CO_EXT_MEM_HWCTL_PDN (0x1800_0050)
	 * Data: 32'h0
	 * Action: write
	 */
	CONSYS_REG_WRITE(CON_REG_INFRA_RGU_ADDR + CONN_INFRA_RGU_CO_EXT_MEM_HWCTL_PDN, 0x0);

	/* co-ext memory  hw control setting -> enable hw sleep
	 * Address: CONN_INFRA_RGU_CO_EXT_MEM_HWCTL_SLP_CO_EXT_MEM_HWCTL_SLP (0x1800_0054)
	 * Data: 32'h0000_0001
	 * Action: write
	 */
	CONSYS_REG_WRITE(CON_REG_INFRA_RGU_ADDR + CONN_INFRA_RGU_CO_EXT_MEM_HWCTL_SLP, 0x1);

	return 0;
}

int connsys_spi_master_cfg(unsigned int next_status)
{
	unsigned int bt_only = 0;

	if ((next_status & (~(0x1 << CONNDRV_TYPE_BT))) == 0)
		bt_only = 1;

	/* CONN_WT_SLP_CTL_REG_WB_WF_CK_ADDR_ADDR(0x070) = 0x0AF40A04
	 * CONN_WT_SLP_CTL_REG_WB_WF_WAKE_ADDR_ADDR(0x74) = 0x00A00090
	 * CONN_WT_SLP_CTL_REG_WB_WF_ZPS_ADDR_ADDR(0x78) = 0x009c008c
	 * CONN_WT_SLP_CTL_REG_WB_BT_CK_ADDR_ADDR(0x7c[11:0]) = 0xa08
	 * CONN_WT_SLP_CTL_REG_WB_BT_WAKE_ADDR_ADDR(0x80[11:0]) = 0x094
	 * CONN_WT_SLP_CTL_REG_WB_TOP_CK_ADDR_ADDR(0x84[11:0]) = 0x02c
	 * CONN_WT_SLP_CTL_REG_WB_GPS_CK_ADDR_ADDR(0x88[11:0]) = 0x0AFC0A0C
	 * CONN_WT_SLP_CTL_REG_WB_WF_B0_CMD_ADDR_ADDR(0x8c[11:0]) = 0x0F0
	 * CONN_WT_SLP_CTL_REG_WB_WF_B1_CMD_ADDR_ADDR(0x90[11:0])  = 0x0F4
	 * CONN_WT_SLP_CTL_REG_WB_GPS_RFBUF_ADDR(0x18005094[11:0])  = 0x0FC
	 * CONN_WT_SLP_CTL_REG_WB_GPS_L5_EN_ADDR(0x18005098[11:0]) = 0x0F8
	 */
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_WF_CK_ADDR, 0x0AF40A04);
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_WF_WAKE_ADDR, 0x00A00090);
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_WF_ZPS_ADDR, 0x009C008C);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BT_CK_ADDR,
		0xa08, 0xfff);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BT_WAKE_ADDR,
		0x094, 0xfff);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_TOP_CK_ADDR,
		0x02c, 0xfff);
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_GPS_CK_ADDR,
		0x0AFC0A0C);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_WF_B0_CMD_ADDR,
		0x0f0, 0xfff);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_WF_B1_CMD_ADDR,
		0x0f4, 0xfff);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_GPS_RFBUF_ADDR,
		0x0FC, 0xfff);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_GPS_L5_EN_ADDR,
		0x0F8, 0xfff);

	/* CONN_WT_SLP_CTL_REG_WB_SLP_CTL_CMD_LENGTH(0x004[4:0]) = 0x8
	 * CONN_WT_SLP_CTL_REG_WB_BG_ADDR1_WB_BG_ADDR1(0x10[15:0]) = 0xA03C
	 * CONN_WT_SLP_CTL_REG_WB_BG_ADDR2_WB_BG_ADDR2(0x14[15:0]) = 0xA03C
	 * CONN_WT_SLP_CTL_REG_WB_BG_ADDR3_WB_BG_ADDR3(0x18[15:0]) = 0xAA18
	 * CONN_WT_SLP_CTL_REG_WB_BG_ADDR4_WB_BG_ADDR4(0x1c[15:0]) = 0xAA18
	 * CONN_WT_SLP_CTL_REG_WB_BG_ADDR5_WB_BG_ADDR5(0x20[15:0]) = 0xA0C8
	 * CONN_WT_SLP_CTL_REG_WB_BG_ADDR6_WB_BG_ADDR6(0x24[15:0]) = 0xAA00
	 * CONN_WT_SLP_CTL_REG_WB_BG_ADDR7_WB_BG_ADDR7(0x28[15:0]) = 0xA0B4
	 * CONN_WT_SLP_CTL_REG_WB_BG_ADDR8_WB_BG_ADDR8(0x2c[15:0]) = 0xA34C
	 * CONN_WT_SLP_CTL_REG_WB_BG_ON1_WB_BG_ON1(0x30)     = 0x00000000
	 * CONN_WT_SLP_CTL_REG_WB_BG_ON2_WB_BG_ON2(0x34)     = 0x00000000
	 * if (BT_only) {
	 *    CONN_WT_SLP_CTL_REG_WB_BG_ON3_WB_BG_ON3(0x38)     = 0x74E03F75
	 *    CONN_WT_SLP_CTL_REG_WB_BG_ON4_WB_BG_ON4(0x3c)     = 0x76E83F75
	 * } else {
	 *    CONN_WT_SLP_CTL_REG_WB_BG_ON3_WB_BG_ON3(0x38)     = 0x74E0FFF5
	 *    CONN_WT_SLP_CTL_REG_WB_BG_ON4_WB_BG_ON4(0x3c)     = 0x76E8FFF5
	 * }
	 * CONN_WT_SLP_CTL_REG_WB_BG_ON5_WB_BG_ON5(0x40)     = 0x00000000
	 * CONN_WT_SLP_CTL_REG_WB_BG_ON6_WB_BG_ON6(0x44)     = 0xFFFFFFFF
	 * CONN_WT_SLP_CTL_REG_WB_BG_ON7_WB_BG_ON7(0x48)     = 0x00000019
	 * CONN_WT_SLP_CTL_REG_WB_BG_ON8_WB_BG_ON8(0x4c)     = 0x00010400
	 * CONN_WT_SLP_CTL_REG_WB_BG_OFF1_WB_BG_OFF1(0x50)   = 0x57400000
	 * CONN_WT_SLP_CTL_REG_WB_BG_OFF2_WB_BG_OFF2(0x54)   = 0x57400000
	 * if (BT only) {
	 *    CONN_WT_SLP_CTL_REG_WB_BG_OFF3_WB_BG_OFF3(0x58)   = 0x44E03F75
	 *    CONN_WT_SLP_CTL_REG_WB_BG_OFF4_WB_BG_OFF4(0x5c)   = 0x44E03F75
	 * } else {
	 *    CONN_WT_SLP_CTL_REG_WB_BG_OFF3_WB_BG_OFF3(0x58)   = 0x44E0FFF5
	 *    CONN_WT_SLP_CTL_REG_WB_BG_OFF4_WB_BG_OFF4(0x5c)   = 0x44E0FFF5
	 * }
	 * CONN_WT_SLP_CTL_REG_WB_BG_OFF5_WB_BG_OFF5(0x60)   = 0x00000001
	 * CONN_WT_SLP_CTL_REG_WB_BG_OFF6_WB_BG_OFF6(0x64)   = 0x00000000
	 * CONN_WT_SLP_CTL_REG_WB_RG_OFF7_WB_BG_OFF7(0x68)   = 0x00040019
	 * CONN_WT_SLP_CTL_REG_WB_RG_OFF8_WB_BG_OFF8(0x6c)   = 0x00410440
	 */
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_SLP_CTL,
		0x8, 0x1f);

	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ADDR1,
		0xa03c, 0xffff);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ADDR2,
		0xa03c, 0xffff);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ADDR3,
		0xaa18, 0xffff);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ADDR4,
		0xaa18, 0xffff);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ADDR5,
		0xa0c8, 0xffff);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ADDR6,
		0xaa00, 0xffff);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ADDR7,
		0xa0b4, 0xffff);
	CONSYS_REG_WRITE_MASK(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ADDR8,
		0xa34c, 0xffff);
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ON1, 0x00000000);
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ON2, 0x00000000);
	if (bt_only) {
		CONSYS_REG_WRITE(
			CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ON3, 0x74E03F75);
		CONSYS_REG_WRITE(
			CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ON4, 0x76E83F75);
	} else {
		CONSYS_REG_WRITE(
			CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ON3, 0x74E0fff5);
		CONSYS_REG_WRITE(
			CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ON4, 0x76E8FFF5);
	}
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ON5, 0x00000000);
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ON6, 0xFFFFFFFF);
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ON7, 0x00000019);
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_ON8, 0x00010400);

	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_OFF1, 0x57400000);
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_OFF2, 0x57400000);
	if (bt_only) {
		CONSYS_REG_WRITE(
			CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_OFF3, 0x44E03F75);
		CONSYS_REG_WRITE(
			CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_OFF4, 0x44E03F75);
	} else {
		CONSYS_REG_WRITE(
			CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_OFF3, 0x44e0fff5);
		CONSYS_REG_WRITE(
			CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_OFF4, 0x44e0fff5);
	}
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_OFF5, 0x00000001);
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_OFF6, 0x00000000);
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_OFF7, 0x00040019);
	CONSYS_REG_WRITE(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WT_SLP_CTL_REG_WB_BG_OFF8, 0x00410440);

	return 0;
}

//#ifndef CONFIG_FPGA_EARLY_PORTING
/*****************************************************************************
* FUNCTION
*  connsys_a_die_efuse_read
* DESCRIPTION
*  Read a-die efuse
* PARAMETERS
*  efuse_addr: read address
* RETURNS
*  int
*	0: fail, efuse is invalid
*	1: success, efuse is valid
*****************************************************************************/
static int connsys_a_die_efuse_read(unsigned int efuse_addr)
{
	int ret = 0;
	int retry = 0;
	unsigned int efuse0 = 0, efuse1 = 0, efuse2 = 0, efuse3 = 0;
	int ret0, ret1, ret2, ret3;

	/* Get semaphore before read */
	if (consys_sema_acquire_timeout(CONN_SEMA_RFSPI_INDEX, CONN_SEMA_TIMEOUT) == CONN_SEMA_GET_FAIL) {
		pr_err("[EFUSE READ] Require semaphore fail\n");
		return 0;
	}

	/* Efuse control clear, clear Status /trigger
	 * Address: ATOP EFUSE_CTRL_write_efsrom_kick_and_read_kick_busy_flag (0x108[30])
	 * Data: 1'b0
	 * Action: TOPSPI_WR
	 */
	consys_spi_read_nolock(SYS_SPI_TOP, ATOP_EFUSE_CTRL, &ret);
	ret &= ~(0x1 << 30);
	consys_spi_write_nolock(SYS_SPI_TOP, ATOP_EFUSE_CTRL, ret);

	/* Efuse Read 1st 16byte
	 * Address:
	 *    ATOP EFUSE_CTRL_efsrom_mode (0x108[7:6]) = 2'b00
	 *    ATOP EFUSE_CTRL_efsrom_ain (0x108[25:16]) = efuse_addr (0)
	 *    ATOP EFUSE_CTRL_write_efsrom_kick_and_read_kick_busy_flag (0x108[30]) = 1'b1
	 * Action: TOPSPI_WR
	 */
	consys_spi_read_nolock(SYS_SPI_TOP, ATOP_EFUSE_CTRL, &ret);
	ret &= ~(0x43ff00c0);
	ret |= (0x1 << 30);
	ret |= ((efuse_addr << 16) & 0x3ff0000);
	consys_spi_write_nolock(SYS_SPI_TOP, ATOP_EFUSE_CTRL, ret);

	/* Polling EFUSE busy = low
	 * (each polling interval is "30us" and polling timeout is 2ms)
	 * Address:
	 *    ATOP EFUSE_CTRL_write_efsrom_kick_and_read_kick_busy_flag (0x108[30]) = 1'b0
	 * Action: TOPSPI_Polling
	 */
	consys_spi_read_nolock(SYS_SPI_TOP, ATOP_EFUSE_CTRL, &ret);
	while ((ret & (0x1 << 30)) != 0 && retry < 70) {
		retry++;
		udelay(30);
		consys_spi_read_nolock(SYS_SPI_TOP, ATOP_EFUSE_CTRL, &ret);
	}
	if ((ret & (0x1 << 30)) != 0) {
		pr_info("[%s] EFUSE busy, retry failed(%d)\n", __func__, retry);
	}

	/* Check efuse_valid & return
	 * Address: ATOP EFUSE_CTRL_csri_efsrom_dout_vld_sync_1_ (0x108[29])
	 * Action: TOPSPI_RD
	 */
	/* if (efuse_valid == 1'b1)
	 *     Read Efuse Data to global var
	 */
	consys_spi_read_nolock(SYS_SPI_TOP, ATOP_EFUSE_CTRL, &ret);
	if (((ret & (0x1 << 29)) >> 29) == 1) {
		ret0 = consys_spi_read_nolock(SYS_SPI_TOP, ATOP_EFUSE_RDATA0, &efuse0);

		CONSYS_REG_WRITE(
			CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_EFUSE_DATA_0,
			efuse0);

		ret1 = consys_spi_read_nolock(SYS_SPI_TOP, ATOP_EFUSE_RDATA1, &efuse1);
		CONSYS_REG_WRITE(
			CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_EFUSE_DATA_1,
			efuse1);
		/* Sub-task: thermal cal */
		connsys_a_die_thermal_cal(1, efuse1);

		ret2 = consys_spi_read_nolock(SYS_SPI_TOP, ATOP_EFUSE_RDATA2, &efuse2);
		CONSYS_REG_WRITE(
			CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_EFUSE_DATA_2,
			efuse2);

		ret3 = consys_spi_read_nolock(SYS_SPI_TOP, ATOP_EFUSE_RDATA3, &efuse3);
		CONSYS_REG_WRITE(
			CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_EFUSE_DATA_3,
			efuse3);

		pr_info("efuse = [0x%08x, 0x%08x, 0x%08x, 0x%08x]", efuse0, efuse1, efuse2, efuse3);
		if (ret0 || ret1 || ret2 || ret3)
			pr_err("efuse read error: [%d, %d, %d, %d]", ret0, ret1, ret2, ret3);
		ret = 1;
	} else {
		connsys_a_die_thermal_cal(0, 0);
		pr_err("EFUSE is invalid\n");
		ret = 0;
	}

	consys_sema_release(CONN_SEMA_RFSPI_INDEX);
	return ret;
}

static int connsys_a_die_thermal_cal(int efuse_valid, unsigned int efuse)
{
	struct consys_plat_thermal_data input;
	memset(&input, 0, sizeof(struct consys_plat_thermal_data));

	if (efuse_valid) {
		if (efuse & (0x1 << 7)) {
			consys_spi_write_offset_range_nolock(
				SYS_SPI_TOP, ATOP_RG_TOP_THADC_BG, efuse, 12, 3, 4);
			consys_spi_write_offset_range_nolock(
				SYS_SPI_TOP, ATOP_RG_TOP_THADC, efuse, 23, 0, 3);
		}
		if(efuse & (0x1 << 15)) {
			consys_spi_write_offset_range_nolock(
				SYS_SPI_TOP, ATOP_RG_TOP_THADC, efuse, 26, 13, 2);
			input.slop_molecule = (efuse & 0x1f00) >> 8;
			pr_info("slop_molecule=[%d]", input.slop_molecule);
		}
		if (efuse & (0x1 << 23)) {
			/* [22:16] */
			input.thermal_b = (efuse & 0x7f0000) >> 16;
			pr_info("thermal_b =[%d]", input.thermal_b);
		}
		if (efuse & (0x1 << 31)) {
			input.offset = (efuse & 0x7f000000) >> 24;
			pr_info("offset=[%d]", input.offset);
		}
	}
	update_thermal_data(&input);
	return 0;
}
//#endif

int connsys_a_die_cfg(void)
{
	int efuse_valid;
	bool adie_26m = true;
	unsigned int adie_id = 0;

	if (consys_co_clock_type() == CONNSYS_CLOCK_SCHEMATIC_52M_COTMS) {
		pr_info("A-die clock 52M\n");
		adie_26m = false;
	}
	/* First time to setup conninfra sysram, clean it. */
	memset_io(
		(volatile void*)CONN_INFRA_SYSRAM_BASE_ADDR,
		0x0,
		CONN_INFRA_SYSRAM_SIZE);


	/* if(A-die XTAL = 26MHz ) {
	 *    CONN_WF_CTRL2 swtich to GPIO mode, GPIO output value
	 *    before patch download swtich back to CONN mode.
	 *  }
	 */
	/* Address:
	 *     0x1000_5054 = 32'h0010_0000
	 *     0x1000_5154 = 32'h0010_0000
	 *     0x1000_5460[18:16] = 3'b000
	 * Actin: write
	 * Note: MT6635 strap pinmux, set CONN_WF_CTRL2 as GPIO
	 */
	if (adie_26m) {
		CONSYS_REG_WRITE(GPIO_BASE_ADDR + GPIO_DIR5_SET, 0x00100000);
		CONSYS_REG_WRITE(GPIO_BASE_ADDR + GPIO_DOUT5_SET, 0x00100000);
		CONSYS_REG_WRITE_MASK(GPIO_BASE_ADDR + GPIO_MODE22, 0x0, 0x70000);
	}
	/* sub-task: a-die cfg */
	/* De-assert A-die reset
	 * Address: CONN_INFRA_CFG_ADIE_CTL_ADIE_RSTB (0x18001900[0])
	 * Data: 1'b1
	 * Action: write
	 */
	CONSYS_SET_BIT(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_ADIE_CTL, 0x1);

	/* Read MT6635 ID
	 * Address: ATOP CHIP_ID
	 *    0x02C[31:16]: hw_code
	 *    0x02C[15:0]: hw_ver
	 * Data:
	 *    MT6635 E1 : read 0x02C = 0x66358A00
	 *    MT6635 E2 : read 0x02C = 0x66358A10
	 *    MT6635 E3 : read 0x02C = 0x66358A11
	 * Action: TOPSPI_RD
	 */
	consys_spi_read(SYS_SPI_TOP, ATOP_CHIP_ID, &adie_id);
	conn_hw_env.adie_hw_version = adie_id;
	conn_hw_env.is_rc_mode = consys_is_rc_mode_enable();
	pr_info("A-die CHIP_ID=0x%08x rc=%d\n", adie_id, conn_hw_env.is_rc_mode);

	CONSYS_REG_WRITE(
		CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_CHIP_ID,
		adie_id);

	/* Patch to FW from 7761(WF0_WRI_SX_CAL_MAP/WF1_WRI_SX_CAL_MAP)
	 * Address: ATOP WRI_CTR2 (0x064)
	 * Data: 32'h00007700
	 * Action: TOPSPI_WR
	 */
	consys_spi_write(SYS_SPI_TOP, ATOP_WRI_CTR2, 0x00007700);

	/* Set WF low power cmd as DBDC mode & legacy interface
	 * Address: ATOP SMCTK11 (0x0BC)
	 * Data: 32'h00000021
	 * Action: TOPSPI_WR
	 */
	consys_spi_write(SYS_SPI_TOP, ATOP_SMCTK11, 0x00000021);

	/* Update spi fm read extra bit setting
	 * Address:
	 *    CONN_RF_SPI_MST_REG_FM_CTRL_FM_RD_EXT_EN (0x1800400C[15])
	 *    CONN_RF_SPI_MST_REG_FM_CTRL_FM_RD_EXT_CNT (0x1800400C[7:0])
	 * Data:
	 *    0x1800400C[15] = 1'b0
	 *    0x1800400C[7:0] = 8'h0
	 * Action: write
	 */
	CONSYS_REG_WRITE_MASK(
		CONN_REG_RFSPI_ADDR + CONN_RF_SPI_MST_REG_FM_CTRL, 0x0, 0x80ff);

	/* Update Thermal addr for 6635
	 * Address: CONN_TOP_THERM_CTL_THERM_AADDR (0x18002018)
	 * Data: 32'h50305A00
	 * Action: write
	 */
	CONSYS_REG_WRITE(CONN_TOP_THERM_CTL_ADDR + CONN_TOP_THERM_CTL_THERM_AADDR, 0x50305A00);

	/* Sub-task: read a-die efuse */
	efuse_valid = connsys_a_die_efuse_read(0);

	/* Set WF_PAD to HighZ
	 * Address: ATOP RG_ENCAL_WBTAC_IF_SW (0x070)
	 * Data: 32'h80000000
	 * Action: TOPSPI_WR
	 */
	consys_spi_write(SYS_SPI_TOP, ATOP_RG_ENCAL_WBTAC_IF_SW, 0x80000000);

	/* Disable CAL LDO
	 * Address: ATOP RG_WF0_TOP_01 (0x380)
	 * Data: 32'h000E8002
	 * Action: TOPSPI_WR
	 * Note: AC mode
	 */
	consys_spi_write(SYS_SPI_TOP, ATOP_RG_WF0_TOP_01, 0x000e8002);

	/* Disable CAL LDO
	 * Address: ATOP RG_WF1_TOP_01 (0x390)
	 * Data: 32'h000E8002
	 * Action: TOPSPI_WR
	 * Note: AC mode
	 */
	consys_spi_write(SYS_SPI_TOP, ATOP_RG_WF1_TOP_01, 0x000e8002);

	/* Increase XOBUF supply-V
	 * Address: ATOP RG_TOP_XTAL_01 (0xA18)
	 * Data: 32'hF6E8FFF5
	 * Action: TOPSPI_WR
	 */
	consys_spi_write(SYS_SPI_TOP, ATOP_RG_TOP_XTAL_01, 0xF6E8FFF5);

	/* Increase XOBUF supply-R for MT6635 E1
	 * Address: ATOP RG_TOP_XTAL_02 (0xA1C)
	 * Data:
	 * if(MT6635 E1) //rf_hw_ver = 0x8a00
	 *   32'hD5555FFF
	 * else
	 *   32'h0x55555FFF
	 * Action: TOPSPI_WR
	 */
	if (adie_id == 0x66358a00) {
		consys_spi_write(SYS_SPI_TOP, ATOP_RG_TOP_XTAL_02, 0xD5555FFF);
	} else {
		consys_spi_write(SYS_SPI_TOP, ATOP_RG_TOP_XTAL_02, 0x55555FFF);
	}

	/* Initial IR value for WF0 THADC
	 * Address: ATOP RG_WF0_BG (0x384)
	 * Data: 0x00002008
	 * Action: TOPSPI_WR
	 */
	consys_spi_write(SYS_SPI_TOP, ATOP_RG_WF0_BG, 0x2008);

	/* Initial IR value for WF1 THADC
	 * Address: ATOP RG_WF1_BG (0x394)
	 * Data: 0x00002008
	 * Action: TOPSPI_WR
	 */
	consys_spi_write(SYS_SPI_TOP, ATOP_RG_WF1_BG, 0x2008);

	/* if(A-die XTAL = 26MHz ) {
	 *    CONN_WF_CTRL2 swtich to CONN mode
	 * }
	 */
	/* Adress: 0x1000_5460[18:16] = 3'b001
	 * Action: write
	 * Note: MT6635 strap pinmux, set CONN_WF_CTRL2 as conn mode
	 */
	if (adie_26m) {
		CONSYS_REG_WRITE_MASK(GPIO_BASE_ADDR + GPIO_MODE22, 0x10000, 0x70000);
	}
	return 0;
}

int connsys_afe_wbg_cal(void)
{
	/* Default value update; 1:   AFE WBG CR (if needed)
	 * note that this CR must be backuped and restored by command batch engine
	 * Address:
	 *     CONN_AFE_CTL_RG_WBG_AFE_01(0x18003010) = 32'h00000000
	 *     CONN_AFE_CTL_RG_WBG_RCK_01(0x18003018) = 32'h144B0160
	 *     CONN_AFE_CTL_RG_WBG_GL1_01(0x18003040) = 32'h10990C13
	 *     CONN_AFE_CTL_RG_WBG_GL5_01(0x18003100) = 32'h10990C13
	 *     CONN_AFE_CTL_RG_WBG_BT_TX_03 (0x18003058) = 32'hCD258051
	 *     CONN_AFE_CTL_RG_WBG_WF0_TX_03 (0x18003078) = 32'hC5258251
	 *     CONN_AFE_CTL_RG_WBG_WF1_TX_03 (0x18003094) = 32'hC5258251
	 */
	CONSYS_REG_WRITE(
		CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_WBG_AFE_01,
		0x0);
	CONSYS_REG_WRITE(
		CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_WBG_RCK_01,
		0x144B0160);
	CONSYS_REG_WRITE(
		CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_WBG_GL1_01,
		0x10990C13);
	CONSYS_REG_WRITE(
		CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_WBG_GL5_01,
		0x10990C13);
	CONSYS_REG_WRITE(
		CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_WBG_BT_TX_03,
		0xCD258051);
	CONSYS_REG_WRITE(
		CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_WBG_WF0_TX_03,
		0xC5258251);
	CONSYS_REG_WRITE(
		CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_WBG_WF1_TX_03,
		0xC5258251);

	/* AFE WBG CAL SEQ1 (RC calibration) */
	/* AFE WBG RC calibration, set "AFE RG_WBG_EN_RCK" = 1
	 * Address: CONN_AFE_CTL_RG_DIG_EN_01_RG_WBG_EN_RCK (0x18003000[0])
	 * Data: 1'b1
	 * Action: write
	 */
	CONSYS_SET_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_01, 0x1);
	udelay(60);
	/* AFE WBG RC calibration */
	/* AFE WBG RC calibration, set "AFE RG_WBG_EN_RCK" = 0
	 * Address: CONN_AFE_CTL_RG_DIG_EN_01_RG_WBG_EN_RCK (0x18003000[0])
	 * Data: 1'b0
	 * Action: write
	 */
	CONSYS_CLR_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_01, 0x1);

	/* AFE WBG CAL SEQ2 (TX calibration) */
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_BPLL_UP" = 1
	 * Address: CONN_AFE_CTL_RG_DIG_EN_03_RG_WBG_EN_BPLL_UP (0x18003008[21])
	 * Data: 1'b1
	 * Action: write
	 */
	CONSYS_SET_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_03, (0x1 << 21));
	udelay(30);
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_WPLL_UP" = 1
	 * Address: CONN_AFE_CTL_RG_DIG_EN_03_RG_WBG_EN_WPLL_UP (0x18003008[20])
	 * Data: 1'b1
	 * Action: write
	 */
	CONSYS_SET_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_03, (0x1 << 20));
	udelay(60);
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_TXCAL_BT" = 1
	 * Address: CONN_AFE_CTL_RG_DIG_EN_01_RG_WBG_EN_TXCAL_BT (0x18003000[21])
	 * Data: 1'b1
	 * Action: write
	 */
	CONSYS_SET_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_01, (0x1 << 21));
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_TXCAL_WF0" = 1
	 * Address: CONN_AFE_CTL_RG_DIG_EN_01_RG_WBG_EN_TXCAL_WF0 (0x18003000[20])
	 * Data: 1'b1
	 * Action: write
	 */
	CONSYS_SET_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_01, (0x1 << 20));
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_TXCAL_WF1" = 1
	 * Address: CONN_AFE_CTL_RG_DIG_EN_01_RG_WBG_EN_TXCAL_WF1 (0x18003000[19])
	 * Data: 1'b1
	 * Action: write
	 */
	CONSYS_SET_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_01, (0x1 << 19));
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_TXCAL_WF2" = 1
	 * Address: CONN_AFE_CTL_RG_DIG_EN_01_RG_WBG_EN_TXCAL_WF2 (0x18003000[18])
	 * Data: 1'b1
	 * Action: write
	 */
	CONSYS_SET_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_01, (0x1 << 18));
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_TXCAL_WF3" = 1
	 * Addres: CONN_AFE_CTL_RG_DIG_EN_01_RG_WBG_EN_TXCAL_WF3 (0x18003000[17])
	 * Data: 1'b1
	 * Action: write
	 */
	CONSYS_SET_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_01, (0x1 << 17));
	udelay(800);
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_TXCAL_BT" = 0
	 * Address: CONN_AFE_CTL_RG_DIG_EN_01_RG_WBG_EN_TXCAL_BT (0x18003000[21])
	 * Data: 1'b0
	 * Action: write
	 */
	CONSYS_CLR_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_01, (0x1 << 21));
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_TXCAL_WF0" = 0
	 * Address: CONN_AFE_CTL_RG_DIG_EN_01_RG_WBG_EN_TXCAL_WF0 (0x18003000[20])
	 * Data: 1'b0
	 * Action: write
	 */
	CONSYS_CLR_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_01, (0x1 << 20));
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_TXCAL_WF1" = 0
	 * Address: CONN_AFE_CTL_RG_DIG_EN_01_RG_WBG_EN_TXCAL_WF1 (0x18003000[19])
	 * Data: 1'b0
	 * Action: write
	 */
	CONSYS_CLR_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_01, (0x1 << 19));
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_TXCAL_WF2" = 0
	 * Address: CONN_AFE_CTL_RG_DIG_EN_01_RG_WBG_EN_TXCAL_WF2 (0x18003000[18])
	 * Data: 1'b0
	 * Action: write
	 */
	CONSYS_CLR_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_01, (0x1 << 18));
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_TXCAL_WF3" = 0
	 * Address: CONN_AFE_CTL_RG_DIG_EN_01_RG_WBG_EN_TXCAL_WF3 (0x18003000[17])
	 * Data: 1'b0
	 * Action: write
	 */
	CONSYS_CLR_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_01, (0x1 << 17));
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_BPLL_UP" = 0
	 * Address: CONN_AFE_CTL_RG_DIG_EN_03_RG_WBG_EN_BPLL_UP (0x18003008[21])
	 * Data: 1'b0
	 * Action: write
	 */
	CONSYS_CLR_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_03, (0x1 << 21));
	/* AFE WBG TX calibration, set "AFE RG_WBG_EN_WPLL_UP" = 0
	 * Address:i CONN_AFE_CTL_RG_DIG_EN_03_RG_WBG_EN_WPLL_UP (0x18003008[20])
	 * Data: 1'b0
	 * Action: write
	 */
	CONSYS_CLR_BIT(CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_03, (0x1 << 20));

	/* Initial BT path if WF is in cal(need set this CR after WBG cal)
	 * Address: ATOP RG_ENCAL_WBTAC_IF_SW (0x070)
	 * Data: 32'h00000005
	 * Action: write
	 */
	consys_spi_write(SYS_SPI_TOP, ATOP_RG_ENCAL_WBTAC_IF_SW, 0x5);
	return 0;
}

int connsys_subsys_pll_initial(void)
{
	/* Check with DE, only 26M on mobile phone */
	/* Set BPLL stable time = 30us (value = 30 * 1000 *1.01 / 38.46ns)
	 * 	CONN_AFE_CTL_RG_PLL_STB_TIME_RG_WBG_BPLL_STB_TIME (0x180030F4[30:16]) = 0x314
	 * Set WPLL stable time = 50us (value = 50 * 1000 *1.01 / 38.46ns)
	 * 	CONN_AFE_CTL_RG_PLL_STB_TIME_RG_WBG_WPLL_STB_TIME (0x180030F4[14:0]) = 0x521
	 */
	CONSYS_REG_WRITE_MASK(
		CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_PLL_STB_TIME,
		0x03140521, 0x7fff7fff);

	/* BT pll_en will turn on  BPLL only (may change in different XTAL option)
	 * 	CONN_AFE_CTL_RG_DIG_EN_02_RG_WBG_EN_BT_PLL (0x18003004[7:6])=0x1
	 * WF pll_en will turn on  BPLL + WPLL only (may change in different XTAL option)
	 * 	CONN_AFE_CTL_RG_DIG_EN_02_RG_WBG_EN_WF_PLL (0x18003004[1:0])=0x3
	 * MCU pll_en will turn on  BPLL + WPLL (may change in different XTAL option)
	 * 	CONN_AFE_CTL_RG_DIG_EN_02_RG_WBG_EN_MCU_PLL (0x18003004[3:2])=0x3
	 */
	CONSYS_REG_WRITE_MASK(
		CONN_AFE_CTL_BASE_ADDR + CONN_AFE_CTL_RG_DIG_EN_02,
		0x4f, 0xcf);

	return 0;
}

// Special setting for BT low power
static int connsys_bt_low_power_setting(bool bt_only)
{
	int hw_version;
	const struct a_die_reg_config* config = NULL;
	unsigned int ret, i;

	hw_version = CONSYS_REG_READ(
		CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_CHIP_ID);

	if (bt_only) {
		/* E1 */
		if (hw_version == 0x66358A00) {
			config = adie_e1_bt_only;
		} else if (hw_version == 0x66358A10 || hw_version == 0x66358A11) {
			config = adie_e2_bt_only;
		} else
			pr_err("[%s] wrong adie version (0x%08x)\n", __func__, hw_version);
	} else {
		if (hw_version == 0x66358A00) {
			config = adie_e1_default;
		} else if (hw_version == 0x66358A10 || hw_version == 0x66358A11) {
			config = adie_e2_default;
		} else
			pr_err("[%s] wrong adie version (0x%08x)\n", __func__, hw_version);
	}

	if (config == NULL)
		return -1;

	consys_adie_top_ck_en_on(CONNSYS_ADIE_CTL_HOST_CONNINFRA);

	/* Get semaphore before read */
	if (consys_sema_acquire_timeout(CONN_SEMA_RFSPI_INDEX, CONN_SEMA_TIMEOUT) == CONN_SEMA_GET_FAIL) {
		pr_err("[EFUSE READ] Require semaphore fail\n");
		consys_adie_top_ck_en_off(CONNSYS_ADIE_CTL_HOST_CONNINFRA);
		return -1;
	}

	for (i = 0; i < ADIE_CONFIG_NUM; i++) {
		consys_spi_read_nolock(SYS_SPI_TOP, config[i].reg, &ret);
		ret &= (~config[i].mask);
		ret |= config[i].config;
		consys_spi_write_nolock(SYS_SPI_TOP, config[i].reg, ret);
	}

	consys_sema_release(CONN_SEMA_RFSPI_INDEX);

	consys_adie_top_ck_en_off(CONNSYS_ADIE_CTL_HOST_CONNINFRA);

	return 0;
}

void connsys_debug_select_config(void)
{
#if 1
	/* select conn_infra_cfg debug_sel to low pwoer related
	 * Address: 0x18001B00[2:0]
	 * Data: 3'b000
	 * Action: write
	 */
	CONSYS_REG_WRITE_MASK(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_DBG_MUX_SEL,
			0x0, 0x7);
#else
	/* select conn_infra_cfg debug_sel to BPLL/WPLL status
	 * Address: 0x18001B00[2:0]
	 * Data: 3’b001
	 * Action: write
	 */
	CONSYS_REG_WRITE_MASK(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_DBG_MUX_SEL,
			0x1, 0x7);
	{
		void __iomem *vir_addr = NULL;
		vir_addr = ioremap_nocache(0x18006000, 0x1000);
		if (vir_addr) {
			/* wpll_rdy/bpll_rdy status dump
			 * 1.???Set 0x1800_604C = 0xFFFF_FFFF
			 * 2.???Set 0c1800_6058[1] = 0x1
			 * 3.???Set 0x1800_603C = 0x0000_0100
			 * 4.???Set 0x1800_601C = 0x0302_0100
			 * 5.???Set 0x1800_6020 = 0x0706_0504
			 * 6.???Set 0x1800_6024 = 0x0b0a_0908
			 * 7.???Set 0x1800_6028 = 0x0f0e_0d0c
			 * 8.???Set 0x1800_602C = 0x1312_1110
			 * 9.???Set 0x1800_6030 = 0x1716_1514
			 * 10.??Set 0x1800_6034 = 0x1b1a_1918
			 * 11.??Set 0x1800_6038 = 0x1f1e_1d1c
			 */
			CONSYS_REG_WRITE(vir_addr + 0x004c, 0xffffffff);
			CONSYS_SET_BIT(vir_addr + 0x0058, 0x1);
			CONSYS_REG_WRITE(vir_addr + 0x3c, 0x00000100);
			CONSYS_REG_WRITE(vir_addr + 0x1c, 0x03020100);
			CONSYS_REG_WRITE(vir_addr + 0x20, 0x07060504);
			CONSYS_REG_WRITE(vir_addr + 0x24, 0x0b0a0908);
			CONSYS_REG_WRITE(vir_addr + 0x28, 0x0f0e0d0c);
			CONSYS_REG_WRITE(vir_addr + 0x2c, 0x13121110);
			CONSYS_REG_WRITE(vir_addr + 0x30, 0x17161514);
			CONSYS_REG_WRITE(vir_addr + 0x34, 0x1b1a1918);
			CONSYS_REG_WRITE(vir_addr + 0x38, 0x1f1e1d1c);
			iounmap(vir_addr);
		} else {
			pr_err("remap 0x1800_6000 fail\n");
		}
	}
#endif

}


int connsys_low_power_setting(unsigned int curr_status, unsigned int next_status)
{
	bool bt_only = false;

	if ((next_status & (~(0x1 << CONNDRV_TYPE_BT))) == 0)
		bt_only = true;

	pr_info("[%s] current_status=%d bt_only = %d\n", __func__, curr_status, bt_only);

	/* First subsys on */
	if (curr_status == 0) {
		/* Enable AP2CONN GALS Slave sleep protect en with conn_infra on2off/off2on & wfdma2conn
		 * sleep protect en
		 * Address: CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL_WFDMA2CONN_SLP_PROT_AP2CONN_EN_ENABLE
		 *          CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL_CONN_INFRA_ON2OFF_SLP_PROT_AP2CONN_EN_ENABLE
		 *          CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL_CONN_INFRA_OFF2ON_SLP_PROT_AP2CONN_EN_ENABLE (0x1806_0184[11:9])
		 * Data: 3'b111
		 * Action: write
		 */
		CONSYS_REG_WRITE_MASK(
			CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_SLP_PROT_CTRL,
			0xe00, 0xe00);

		/* Unmask on2off/off2on slpprot_rdy enable checker @conn_infra off power off=> check slpprot_rdy = 1'b1 and go to sleep
		 * Address: CONN_INFRA_CFG_PWRCTRL0_CONN_INFRA_CFG_SLP_RDY_MASK (0x18001860[15:12])
		 * Data: 4'h1
		 * Action: write
		 */
		CONSYS_REG_WRITE_MASK(
			CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_PWRCTRL0,
			0x1000, 0xf000);

		/* conn_infra low power setting */
		if (!consys_is_rc_mode_enable()) {
			/* Default mode (non-RC) */
			/* Disable conn_top rc osc_ctrl_top
			 * Address: CONN_INFRA_CFG_RC_CTL_0_CONN_INFRA_OSC_RC_EN (0x18001834[7])
			 * Data: 1'b0
			 * Action: write
			 */
			CONSYS_CLR_BIT(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_RC_CTL_0, (0x1 << 7));
			/* Legacy OSC control stable time
			 * Address:
			 *  CONN_INFRA_CFG_OSC_CTL_0_XO_VCORE_RDY_STABLE_TIME (0x18001800[7:0]) = 8'd6
			 *  CONN_INFRA_CFG_OSC_CTL_0_XO_INI_STABLE_TIME (0x18001800[15:8]) = 8'd7
			 *  CONN_INFRA_CFG_OSC_CTL_0_XO_BG_STABLE_TIME (0x18001800[23:16]) = 8'd8
			 * Action: write
			 */
			CONSYS_REG_WRITE_MASK(
				CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_OSC_CTL_0, 0x080706, 0xffffff);
			/* Legacy OSC control unmask conn_srcclkena_ack
			 * Address: CONN_INFRA_CFG_OSC_CTL_1_ACK_FOR_XO_STATE_MASK (0x18001804[16])
			 * Data: 1'b0
			 * Action: write
			 */
			CONSYS_CLR_BIT(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_OSC_CTL_1, (0x1 << 16));
		} else {
			/* RC mode */
			/* GPS RC OSC control stable time
			 * Address:
			 * CONN_INFRA_CFG_RC_CTL_1_GPS_XO_VCORE_RDY_STABLE_TIME_0 (0x1800183C[7:0]) = 8'd6
			 * CONN_INFRA_CFG_RC_CTL_1_GPS_XO_INI_STABLE_TIME_0 (0x1800183C[15:8]) = 8'd7
			 * CONN_INFRA_CFG_RC_CTL_1_GPS_XO_BG_STABLE_TIME_0 (0x1800183C[23:16]) = 8'd8
			 * CONN_INFRA_CFG_RC_CTL_1_GPS_XO_VCORE_OFF_STABLE_TIME_0 (0x1800183C[31:24]) = 8'd2
			 * Action: write
			*/
			CONSYS_REG_WRITE(
				CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_RC_CTL_1_GPS,
				0x02080706);

			/* GPS RC OSC control unmask conn_srcclkena_ack
			 * Address: CONN_INFRA_CFG_RC_CTL_0_GPS_ACK_FOR_XO_STATE_MASK_0 (0x18001838[15])
			 * Data: 1'b0
			 * Action: write
			 */
			CONSYS_CLR_BIT(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_RC_CTL_0_GPS, (0x1 << 15));

			/* BT RC OSC control stable time
			 * Address:
			 * CONN_INFRA_CFG_RC_CTL_1_BT_XO_VCORE_RDY_STABLE_TIME_1 (0x18001844[7:0]) = 8'd6
			 * CONN_INFRA_CFG_RC_CTL_1_BT_XO_INI_STABLE_TIME_1 (0x18001844[15:8]) = 8'd7
			 * CONN_INFRA_CFG_RC_CTL_1_BT_XO_BG_STABLE_TIME_1 (0x18001844[23:16]) = 8'd8
			 * CONN_INFRA_CFG_RC_CTL_1_BT_XO_VCORE_OFF_STABLE_TIME_1 (0x18001844[31:24]) = 8'd2
			 * Action: write
			*/
			CONSYS_REG_WRITE(
				CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_RC_CTL_1_BT,
				0x02080706);

			/* BT RC OSC control unmask conn_srcclkena_ack
			 * Address: CONN_INFRA_CFG_RC_CTL_0_BT_ACK_FOR_XO_STATE_MASK_1 (0x18001840[15])
			 * Data: 1'b0
			 * Action: write
			 */
			CONSYS_CLR_BIT(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_RC_CTL_0_BT, (0x1 << 15));

			/* WF RC OSC control stable time
			 * Address:
			 * CONN_INFRA_CFG_RC_CTL_1_WF_XO_VCORE_RDY_STABLE_TIME_2 (0x1800184C[7:0]) = 8'd6
			 * CONN_INFRA_CFG_RC_CTL_1_WF_XO_INI_STABLE_TIME_2 (0x1800184C[15:8]) = 8'd7
			 * CONN_INFRA_CFG_RC_CTL_1_WF_XO_BG_STABLE_TIME_2 (0x1800184C[23:16]) = 8'd8
			 * CONN_INFRA_CFG_RC_CTL_1_WF_XO_VCORE_OFF_STABLE_TIME_2 (0x1800184C[31:24])= 8'd2
			 * Action: write
			 */
			CONSYS_REG_WRITE(
				CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_RC_CTL_1_WF,
				0x02080706);

			/* WF RC OSC control unmask conn_srcclkena_ack
			 * Address: CONN_INFRA_CFG_RC_CTL_0_WF_ACK_FOR_XO_STATE_MASK_2	(0x18001848[15])
			 * Data: 1'b0
			 * Action: write
			 */
			CONSYS_CLR_BIT(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_RC_CTL_0_WF, (0x1 << 15));

			/* TOP RC OSC control stable time
			 * Address:
			 * CONN_INFRA_CFG_RC_CTL_1_TOP_XO_VCORE_RDY_STABLE_TIME_3 (0x18001854[7:0]) = 8'd6
			 * CONN_INFRA_CFG_RC_CTL_1_TOP_XO_INI_STABLE_TIME_3 (0x18001854[15:8]) = 8'd7
			 * CONN_INFRA_CFG_RC_CTL_1_TOP_XO_BG_STABLE_TIME_3 (0x18001854[23:16]) = 8'd8
			 * CONN_INFRA_CFG_RC_CTL_1_TOP_XO_VCORE_OFF_STABLE_TIME_3 (0x18001854[31:24]) = 8'd2
			 * Action: write
			 */
			CONSYS_REG_WRITE(
				CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_RC_CTL_1_TOP,
				0x02080706);

			/* TOP RC OSC control unmask conn_srcclkena_ack
			 * Address: CONN_INFRA_CFG_RC_CTL_0_TOP_ACK_FOR_XO_STATE_MASK_3 (0x18001850[15])
			 * Data: 1'b0
			 * Action: write
			 */
			CONSYS_CLR_BIT(
				CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_RC_CTL_0_TOP,
				(0x1 << 15));

			/* Enable conn_top rc osc_ctrl_gps
			 * Address: CONN_INFRA_CFG_RC_CTL_0_GPSSYS_OSC_RC_EN (0x18001834[4])
			 * Data: 1'b1
			 * Action: write
			 */
			CONSYS_SET_BIT(
				CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_RC_CTL_0,
				(0x1 << 4));

			/* Enable conn_top rc osc_ctrl_bt
			 * Address: CONN_INFRA_CFG_RC_CTL_0_BTSYS_OSC_RC_EN (0x18001834[5])
			 * Data: 1'b1
			 * Action: write
			 */
			CONSYS_SET_BIT(
				CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_RC_CTL_0,
				(0x1 << 5));

			/* Enable conn_top rc osc_ctrl_wf
			 * Address: CONN_INFRA_CFG_RC_CTL_0_WFSYS_OSC_RC_EN (0x18001834[6])
			 * Data: 1'b1
			 * Action: write
			 */
			CONSYS_SET_BIT(
				CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_RC_CTL_0,
				(0x1 << 6));

			/* set conn_srcclkena control by conn_infra_emi_ctl
			 * Address: CONN_INFRA_CFG_EMI_CTL_0_CONN_EMI_RC_EN (0x18001C00[0])
			 * Data: 1'b1
			 * Action: write
			 */
			CONSYS_SET_BIT(
				CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_EMI_CTL_0, 0x1);

			/* Disable legacy osc control
			 * Address: CONN_INFRA_CFG_RC_CTL_0_OSC_LEGACY_EN (0x18001834[0])
			 * Data: 1'b0
			 * Action: write
			 */
			CONSYS_CLR_BIT(
				CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_RC_CTL_0,
				0x1);
		}
		/* conn2ap sleep protect release bypass ddr_en_ack check
		 * Address: CONN_INFRA_CFG_EMI_CTL_0_EMI_SLPPROT_BP_DDR_EN (0x18001C00[18])
		 * Data: 1'b1
		 * Action: write
		 */
		CONSYS_SET_BIT(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_EMI_CTL_0, 0x40000);

		/* Enable ddr_en timeout, timeout value = 1023 T (Bus clock)
		 * Address: CONN_INFRA_CFG_EMI_CTL_0_DDR_CNT_LIMIT (0x18001C00[14:4])
		 * Data: 11'd1023
		 * Action: write
		 */
		CONSYS_REG_WRITE_MASK(
			CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_EMI_CTL_0,
			0x3ff0, 0x7ff0);

		/* A-die clock buffer setting for BT only and others mode */
		connsys_bt_low_power_setting(bt_only);

		/* Enable conn_infra_clkgen BPLL source (hw workaround)
		 * Address: CONN_INFRA_CFG_CKGEN_BUS_RFSPI_DIV_EN (0x1800_1A00[28])
		 * Data: 1'b1
		 * Action: write
		 */
		CONSYS_SET_BIT(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_CKGEN_BUS, (0x1 << 28));

		/* Bus light security
		 * Address:
		 * CONN_INFRA_CFG_LIGHT_SECURITY_CTRL_R_CONN_INFRA_BT_PAIR1_EN
		 * CONN_INFRA_CFG_LIGHT_SECURITY_CTRL_R_CONN_INFRA_BT_PAIR0_EN
		 * CONN_INFRA_CFG_LIGHT_SECURITY_CTRL_R_CONN_INFRA_WF_PAIR1_EN
		 * CONN_INFRA_CFG_LIGHT_SECURITY_CTRL_R_CONN_INFRA_WF_PAIR0_EN
		 * 0x1800_10F0[4][3][1][0] = 6'h1B
		 * Action: write
		 */
		CONSYS_REG_WRITE_MASK(
			CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_LIGHT_SECURITY_CTRL,
			((0x1 << 4) | (0x1 << 3) | (0x1 << 1) | 0x1),
			((0x1 << 4) | (0x1 << 3) | (0x1 << 1) | 0x1));

		/* if(BT on or GPS on)
		 * Conn_infrapower on bgfsys on (woraround)
		 * Address:
		 *     0x18000008[31:16] = 16'h4254 (key)
		 *     CONN_INFRA_RGU_BGFSYS_ON_TOP_PWR_CTL[7] (0x18000008[7]) = 1'b1
		 * Action: write
		 */
		/* Check with DE, write 1 -> 1 is ok */
		if ((next_status & ((0x1 << CONNDRV_TYPE_BT) | (0x1 << CONNDRV_TYPE_GPS))) != 0) {
			CONSYS_REG_WRITE_MASK(
				CON_REG_INFRA_RGU_ADDR + CONN_INFRA_RGU_BGFSYS_ON_TOP_PWR_CTL,
				((0x42540000) | (0x1 << 7)), 0xffff0080);
		}

		consys_config_setup();
		connsys_debug_select_config();

		/*
		 * set 0x1800_0090 = 4'h6
		 */
		CONSYS_REG_WRITE(CON_REG_INFRA_RGU_ADDR + CONN_INFRA_RGU_DEBUG_SEL, 0x6);

		/******************************************************/
		/* power ctrl : 0x1800_1860[9] = 1'b1 */
		/******************************************************/
		CONSYS_SET_BIT(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_PWRCTRL0, 0x200);

		/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
		/* !!!!!!!!!!!!!!!!!!!!!! CANNOT add code after HERE!!!!!!!!!!!!!!!!!!!!!!!!!! */
		/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

		/* Disable conn_infra bus clock sw control  ==> conn_infra bus clock hw control
		 * Address: CONN_INFRA_CFG_CKGEN_BUS_HCLK_CKSEL_SWCTL (0x1800_1A00[23])
		 * Data: 1'b0
		 * Action: write
		 */
		CONSYS_CLR_BIT(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_CKGEN_BUS, (0x1 << 23));

		/* Conn_infra HW_CONTROL => conn_infra enter dsleep mode
		 * Address: CONN_INFRA_CFG_PWRCTRL0_HW_CONTROL (0x1800_1860[0])
		 * Data: 1'b1
		 * Action: write
		 * Note: enable conn_infra off domain as HW control
		 */
		CONSYS_SET_BIT(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_PWRCTRL0, 0x1);
	} else {
		/* for subsys on/off, only update BT low power setting */
		connsys_bt_low_power_setting(bt_only);

		/* Workaround */
		if ((next_status & ((0x1 << CONNDRV_TYPE_BT) | (0x1 << CONNDRV_TYPE_GPS))) != 0) {
			CONSYS_REG_WRITE_MASK(
				CON_REG_INFRA_RGU_ADDR + CONN_INFRA_RGU_BGFSYS_ON_TOP_PWR_CTL,
				((0x42540000) | (0x1 << 7)), 0xffff0080);
		} else {
			CONSYS_REG_WRITE_MASK(
				CON_REG_INFRA_RGU_ADDR + CONN_INFRA_RGU_BGFSYS_ON_TOP_PWR_CTL,
				0x42540000, 0xffff0080);
		}

		consys_config_setup();
		connsys_debug_select_config();
	}
	/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
	/* !!!!!!!!!!!!!!!!!!!!!!!!CANNOT add code HERE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
	/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

	return 0;
}

static int consys_sema_acquire(unsigned int index)
{
	if (CONSYS_REG_READ_BIT(
		CONN_REG_SEMAPHORE_ADDR + CONN_SEMAPHORE_M2_OWN_STA + index*4, 0x1) == 0x1) {
		return CONN_SEMA_GET_SUCCESS;
	} else {
		return CONN_SEMA_GET_FAIL;
	}
}

int consys_sema_acquire_timeout(unsigned int index, unsigned int usec)
{
	int i, check, r1, r2;

	if (index >= CONN_SEMA_NUM_MAX) {
		pr_err("[%s] wrong parameter: %d", __func__, index);
		return CONN_SEMA_GET_FAIL;
	}

	/* debug for bus hang */
	if (consys_reg_mng_reg_readable() == 0) {
		check = consys_reg_mng_is_bus_hang();
		if (check > 0) {
			r1 = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_DBG_DUMMY_3);
			r2 = CONSYS_REG_READ(CON_REG_HOST_CSR_ADDR + CONN_HOST_CSR_TOP_CONN_INFRA_WAKEPU_TOP_CONN_INFRA_WAKEPU_TOP);
			pr_info("[%s] check=[%d] r1=[0x%x] r2=[0x%x]", __func__, check, r1, r2);
			consys_reg_mng_dump_bus_status();
			consys_reg_mng_dump_cpupcr(CONN_DUMP_CPUPCR_TYPE_ALL, 10, 200);
			return CONN_SEMA_GET_FAIL;
		}
	}

	for (i = 0; i < usec; i++) {
		if (consys_sema_acquire(index) == CONN_SEMA_GET_SUCCESS) {
			return CONN_SEMA_GET_SUCCESS;
		}
		udelay(1);
	}

	pr_err("Get semaphore 0x%x timeout, dump status:\n", index);
	pr_err("M0:[0x%x] M1:[0x%x] M2:[0x%x] M3:[0x%x]\n",
		CONSYS_REG_READ(CONN_REG_SEMAPHORE_ADDR + CONN_SEMA_OWN_BY_M0_STA_REP),
		CONSYS_REG_READ(CONN_REG_SEMAPHORE_ADDR + CONN_SEMA_OWN_BY_M1_STA_REP),
		CONSYS_REG_READ(CONN_REG_SEMAPHORE_ADDR + CONN_SEMA_OWN_BY_M2_STA_REP),
		CONSYS_REG_READ(CONN_REG_SEMAPHORE_ADDR + CONN_SEMA_OWN_BY_M3_STA_REP));
	consys_reg_mng_dump_cpupcr(CONN_DUMP_CPUPCR_TYPE_ALL, 10, 200);

	return CONN_SEMA_GET_FAIL;
}

void consys_sema_release(unsigned int index)
{
	if (index >= CONN_SEMA_NUM_MAX)
		return;
	CONSYS_REG_WRITE(
		(CONN_REG_SEMAPHORE_ADDR + CONN_SEMAPHORE_M2_OWN_REL + index*4), 0x1);
}

struct spi_op {
	unsigned int busy_cr;
	unsigned int polling_bit;
	unsigned int addr_cr;
	unsigned int read_addr_format;
	unsigned int write_addr_format;
	unsigned int write_data_cr;
	unsigned int read_data_cr;
	unsigned int read_data_mask;
};

static const struct spi_op spi_op_array[SYS_SPI_MAX] = {
	/* SYS_SPI_WF1 */
	{
		CONN_RF_SPI_MST_REG_SPI_STA, 1,
		CONN_RF_SPI_MST_REG_SPI_WF_ADDR, 0x00001000, 0x00000000,
		CONN_RF_SPI_MST_REG_SPI_WF_WDAT,
		CONN_RF_SPI_MST_REG_SPI_WF_RDAT, 0xffffffff
	},
	/* SYS_SPI_WF */
	{
		CONN_RF_SPI_MST_REG_SPI_STA, 1,
		CONN_RF_SPI_MST_REG_SPI_WF_ADDR, 0x00003000, 0x00002000,
		CONN_RF_SPI_MST_REG_SPI_WF_WDAT,
		CONN_RF_SPI_MST_REG_SPI_WF_RDAT, 0xffffffff
	},
	/* SYS_SPI_BT */
	{
		CONN_RF_SPI_MST_REG_SPI_STA, 2,
		CONN_RF_SPI_MST_REG_SPI_BT_ADDR, 0x00005000, 0x00004000,
		CONN_RF_SPI_MST_REG_SPI_BT_WDAT,
		CONN_RF_SPI_MST_REG_SPI_BT_RDAT, 0x000000ff
	},
	/* SYS_SPI_FM */
	{
		CONN_RF_SPI_MST_REG_SPI_STA, 3,
		CONN_RF_SPI_MST_REG_SPI_FM_ADDR, 0x00007000, 0x00006000,
		CONN_RF_SPI_MST_REG_SPI_FM_WDAT,
		CONN_RF_SPI_MST_REG_SPI_FM_RDAT, 0x0000ffff
	},
	/* SYS_SPI_GPS */
	{
		CONN_RF_SPI_MST_REG_SPI_STA, 4,
		CONN_RF_SPI_MST_REG_SPI_GPS_GPS_ADDR, 0x00009000, 0x00008000,
		CONN_RF_SPI_MST_REG_SPI_GPS_GPS_WDAT,
		CONN_RF_SPI_MST_REG_SPI_GPS_GPS_RDAT, 0x0000ffff
	},
	/* SYS_SPI_TOP */
	{
		CONN_RF_SPI_MST_REG_SPI_STA, 5,
		CONN_RF_SPI_MST_REG_SPI_TOP_ADDR, 0x0000b000, 0x0000a000,
		CONN_RF_SPI_MST_REG_SPI_TOP_WDAT,
		CONN_RF_SPI_MST_REG_SPI_TOP_RDAT, 0xffffffff
	},
	/* SYS_SPI_WF2 */
	{
		CONN_RF_SPI_MST_REG_SPI_STA, 1,
		CONN_RF_SPI_MST_REG_SPI_WF_ADDR, 0x0000d000, 0x0000c000,
		CONN_RF_SPI_MST_REG_SPI_WF_WDAT,
		CONN_RF_SPI_MST_REG_SPI_WF_RDAT, 0xffffffff
	},
	/* SYS_SPI_WF3 */
	{
		CONN_RF_SPI_MST_REG_SPI_STA, 1,
		CONN_RF_SPI_MST_REG_SPI_WF_ADDR, 0x0000f000, 0x0000e000,
		CONN_RF_SPI_MST_REG_SPI_WF_WDAT,
		CONN_RF_SPI_MST_REG_SPI_WF_RDAT, 0xffffffff
	},
};

static int consys_spi_read_nolock(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int *data)
{
	/* Read action:
	 * 1. Polling busy_cr[polling_bit] should be 0
	 * 2. Write addr_cr with data being {read_addr_format | addr[11:0]}
	 * 3. Trigger SPI by writing write_data_cr as 0
	 * 4. Polling busy_cr[polling_bit] as 0
	 * 5. Read data_cr[data_mask]
	 */
	int check = 0;
	const struct spi_op* op = &spi_op_array[subsystem];

	if (!data) {
		pr_err("[%s] invalid data ptr\n", __func__);
		return CONNINFRA_SPI_OP_FAIL;
	}

	CONSYS_REG_BIT_POLLING(
		CONN_REG_RFSPI_ADDR + op->busy_cr,
		op->polling_bit, 0, 100, 50, check);
	if (check != 0) {
		pr_err("[%s][%d][STEP1] polling 0x%08x bit %d fail. Value=0x%08x\n",
			__func__, subsystem, CONN_REG_RFSPI_ADDR + op->busy_cr,
			op->polling_bit,
			CONSYS_REG_READ(CONN_REG_RFSPI_ADDR + op->busy_cr));
		return CONNINFRA_SPI_OP_FAIL;
	}

	CONSYS_REG_WRITE(
		CONN_REG_RFSPI_ADDR + op->addr_cr,
		(op->read_addr_format | addr));

	CONSYS_REG_WRITE(CONN_REG_RFSPI_ADDR + op->write_data_cr, 0);

	check = 0;
	CONSYS_REG_BIT_POLLING(
		CONN_REG_RFSPI_ADDR + op->busy_cr,
		op->polling_bit, 0, 100, 50, check);
	if (check != 0) {
		pr_err("[%s][%d][STEP4] polling 0x%08x bit %d fail. Value=0x%08x\n",
			__func__, subsystem, CONN_REG_RFSPI_ADDR + op->busy_cr,
			op->polling_bit,
			CONSYS_REG_READ(CONN_REG_RFSPI_ADDR + op->busy_cr));
		return CONNINFRA_SPI_OP_FAIL;
	}

	check = CONSYS_REG_READ_BIT(CONN_REG_RFSPI_ADDR + op->read_data_cr, op->read_data_mask);
	*data = check;

	return 0;
}

int consys_spi_read(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int *data)
{
	int ret;

	/* Get semaphore before read */
	if (consys_sema_acquire_timeout(CONN_SEMA_RFSPI_INDEX, CONN_SEMA_TIMEOUT) == CONN_SEMA_GET_FAIL) {
		pr_err("[SPI READ] Require semaphore fail\n");
		return CONNINFRA_SPI_OP_FAIL;
	}

	ret = consys_spi_read_nolock(subsystem, addr, data);

	consys_sema_release(CONN_SEMA_RFSPI_INDEX);

	return ret;
}

static int consys_spi_write_nolock(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int data)
{
	int check = 0;
	const struct spi_op* op = &spi_op_array[subsystem];

	/* Write action:
	 * 1. Wait busy_cr[polling_bit] as 0
	 * 2. Write addr_cr with data being {write_addr_format | addr[11:0]
	 * 3. Write write_data_cr ad data
	 * 4. Wait busy_cr[polling_bit] as 0
	 */
	CONSYS_REG_BIT_POLLING(
		CONN_REG_RFSPI_ADDR + op->busy_cr,
		op->polling_bit, 0, 100, 50, check);
	if (check != 0) {
		pr_err("[%s][%d][STEP1] polling 0x%08x bit %d fail. Value=0x%08x\n",
			__func__, subsystem, CONN_REG_RFSPI_ADDR + op->busy_cr,
			op->polling_bit,
			CONSYS_REG_READ(CONN_REG_RFSPI_ADDR + op->busy_cr));
		return CONNINFRA_SPI_OP_FAIL;
	}

	CONSYS_REG_WRITE(CONN_REG_RFSPI_ADDR + op->addr_cr, (op->write_addr_format | addr));

	CONSYS_REG_WRITE(CONN_REG_RFSPI_ADDR + op->write_data_cr, data);

	check = 0;
	CONSYS_REG_BIT_POLLING(
		CONN_REG_RFSPI_ADDR + op->busy_cr,
		op->polling_bit, 0, 100, 50, check);
	if (check != 0) {
		pr_err("[%s][%d][STEP4] polling 0x%08x bit %d fail. Value=0x%08x\n",
			__func__, subsystem, CONN_REG_RFSPI_ADDR + op->busy_cr,
			op->polling_bit,
			CONSYS_REG_READ(CONN_REG_RFSPI_ADDR + op->busy_cr));
		return CONNINFRA_SPI_OP_FAIL;
	}

	return 0;
}


int consys_spi_write(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int data)
{
	int ret;

	/* Get semaphore before read */
	if (consys_sema_acquire_timeout(CONN_SEMA_RFSPI_INDEX, CONN_SEMA_TIMEOUT) == CONN_SEMA_GET_FAIL) {
		pr_err("[SPI WRITE] Require semaphore fail\n");
		return CONNINFRA_SPI_OP_FAIL;
	}

	ret = consys_spi_write_nolock(subsystem, addr, data);

	consys_sema_release(CONN_SEMA_RFSPI_INDEX);
	return ret;
}

int consys_spi_write_offset_range(
	enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int value,
	unsigned int reg_offset, unsigned int value_offset, unsigned int size)
{
	if (consys_sema_acquire_timeout(CONN_SEMA_RFSPI_INDEX, CONN_SEMA_TIMEOUT) == CONN_SEMA_GET_FAIL) {
		pr_err("[SPI READ] Require semaphore fail\n");
		return CONNINFRA_SPI_OP_FAIL;
	}
	consys_spi_write_offset_range_nolock(
		subsystem, addr, value, reg_offset, value_offset, size);

	consys_sema_release(CONN_SEMA_RFSPI_INDEX);
	return 0;
}

static void consys_spi_write_offset_range_nolock(
	enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int value,
	unsigned int reg_offset, unsigned int value_offset, unsigned int size)
{
	unsigned int data = 0, data2;
	unsigned int reg_mask;
	int ret;

	pr_info("[%s][%s] addr=0x%04x value=0x%08x reg_offset=%d value_offset=%d size=%d",
		__func__, g_spi_system_name[subsystem], addr, value, reg_offset, value_offset, size);
	value = (value >> value_offset);
	value = GET_BIT_RANGE(value, size, 0);
	value = (value << reg_offset);
	ret = consys_spi_read_nolock(subsystem, addr, &data);
	if (ret) {
		pr_err("[%s][%s] Get 0x%08x error, ret=%d",
			__func__, g_spi_system_name[subsystem], addr, ret);
		return;
	}
	reg_mask = GENMASK(reg_offset + size - 1, reg_offset);
	data2 = data & (~reg_mask);
	data2 = (data2 | value);
	consys_spi_write_nolock(subsystem, addr, data2);
	pr_info("[%s][%s] Write CR:0x%08x from 0x%08x to 0x%08x",
		__func__, g_spi_system_name[subsystem],
		addr, data, data2);
}


static int consys_adie_top_ck_en_ctrl(bool on)
{
	int check = 0;

	if (on)
		CONSYS_SET_BIT(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_ADIE_CTL, (0x1 << 1));
	else
		CONSYS_CLR_BIT(CON_REG_INFRA_CFG_ADDR + CONN_INFRA_CFG_ADIE_CTL, (0x1 << 1));

	CONSYS_REG_BIT_POLLING(
		CON_REG_WT_SPL_CTL_ADDR + CONN_WTSLP_CTL_REG_WB_STA,
		26, 0, 100, 5, check);
	if (check == -1) {
		pr_err("[%s] op=%d fail\n", __func__, on);
	}
	return check;

}

int consys_adie_top_ck_en_on(enum consys_adie_ctl_type type)
{
	unsigned int status;
	int ret;

	if (type >= CONNSYS_ADIE_CTL_MAX) {
		pr_err("[%s] invalid parameter(%d)\n", __func__, type);
		return -1;
	}

	if (consys_sema_acquire_timeout(CONN_SEMA_CONN_INFRA_COMMON_SYSRAM_INDEX, CONN_SEMA_TIMEOUT) == CONN_SEMA_GET_FAIL) {
		pr_err("[%s][%s] acquire semaphore (%d) timeout\n",
			__func__, gAdieCtrlType[type], CONN_SEMA_CONN_INFRA_COMMON_SYSRAM_INDEX);
		return -1;
	}

	status = CONSYS_REG_READ(
		CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_TOP_CK_EN_CTRL);
	if ((status & CONN_INFRA_SYSRAM__A_DIE_DIG_TOP_CK_EN_MASK) == 0) {
		ret = consys_adie_top_ck_en_ctrl(true);
	}
	CONSYS_SET_BIT(
		CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_TOP_CK_EN_CTRL, (0x1 << type));

	consys_sema_release(CONN_SEMA_CONN_INFRA_COMMON_SYSRAM_INDEX);
	return 0;
}

int consys_adie_top_ck_en_off(enum consys_adie_ctl_type type)
{
	unsigned int status;
	int ret = 0;

	if (type >= CONNSYS_ADIE_CTL_MAX) {
		pr_err("[%s] invalid parameter(%d)\n", __func__, type);
		return -1;
	}

	if (consys_sema_acquire_timeout(CONN_SEMA_CONN_INFRA_COMMON_SYSRAM_INDEX, CONN_SEMA_TIMEOUT) == CONN_SEMA_GET_FAIL) {
		pr_err("[%s][%s] acquire semaphoreaore (%d) timeout\n",
			__func__, gAdieCtrlType[type], CONN_SEMA_CONN_INFRA_COMMON_SYSRAM_INDEX);
		return -1;
	}

	status = CONSYS_REG_READ(
		CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_TOP_CK_EN_CTRL);
	if ((status & (0x1 << type)) == 0) {
		pr_warn("[%s][%s] already off\n", __func__, gAdieCtrlType[type]);
	} else {
		CONSYS_CLR_BIT(
			CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_TOP_CK_EN_CTRL, (0x1 << type));

		status = CONSYS_REG_READ(
			CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_A_DIE_TOP_CK_EN_CTRL);
		if (0 == (status & CONN_INFRA_SYSRAM__A_DIE_DIG_TOP_CK_EN_MASK)) {
			ret = consys_adie_top_ck_en_ctrl(false);
		}
	}

	consys_sema_release(CONN_SEMA_CONN_INFRA_COMMON_SYSRAM_INDEX);
	return ret;
}

int consys_spi_clock_switch(enum connsys_spi_speed_type type)
{
#define MAX_SPI_CLOCK_SWITCH_COUNT	100
	unsigned int status;
	unsigned int counter = 0;
	int ret = 0;

	/* Get semaphore before read */
	if (consys_sema_acquire_timeout(CONN_SEMA_RFSPI_INDEX, CONN_SEMA_TIMEOUT) == CONN_SEMA_GET_FAIL) {
		pr_err("[SPI CLOCK SWITCH] Require semaphore fail\n");
		return -1;
	}

	if (type == CONNSYS_SPI_SPEED_26M) {
		CONSYS_REG_WRITE_MASK(
			CONN_REG_RFSPI_ADDR + CONN_RF_SPI_MST_REG_SPI_CRTL,
			0x0, 0x5);
		status = CONSYS_REG_READ(CONN_REG_RFSPI_ADDR + CONN_RF_SPI_MST_REG_SPI_CRTL) & 0x18;
		while (status != 0x8 && counter < MAX_SPI_CLOCK_SWITCH_COUNT) {
			udelay(10);
			status = CONSYS_REG_READ(CONN_REG_RFSPI_ADDR + CONN_RF_SPI_MST_REG_SPI_CRTL) & 0x18;
			counter++;
		}
		if (counter == MAX_SPI_CLOCK_SWITCH_COUNT) {
			pr_err("[%s] switch to 26M fail\n", __func__);
			ret = -1;
		}
	} else if (type == CONNSYS_SPI_SPEED_64M) {
		CONSYS_REG_WRITE_MASK(
			CONN_REG_RFSPI_ADDR + CONN_RF_SPI_MST_REG_SPI_CRTL, 0x5, 0x5);
		status = CONSYS_REG_READ(CONN_REG_RFSPI_ADDR + CONN_RF_SPI_MST_REG_SPI_CRTL) & 0x18;
		while (status != 0x10 && counter < MAX_SPI_CLOCK_SWITCH_COUNT) {
			udelay(10);
			status = CONSYS_REG_READ(CONN_REG_RFSPI_ADDR + CONN_RF_SPI_MST_REG_SPI_CRTL) & 0x18;
			counter++;
		}
		if (counter == MAX_SPI_CLOCK_SWITCH_COUNT) {
			pr_err("[%s] switch to 64M fail\n", __func__);
			ret = -1;
		}
	} else {
		ret = -1;
		pr_err("[%s] wrong parameter %d\n", __func__, type);
	}

	consys_sema_release(CONN_SEMA_RFSPI_INDEX);

	return ret;
}

int consys_subsys_status_update(bool on, int radio)
{
	if (radio < CONNDRV_TYPE_BT || radio > CONNDRV_TYPE_WIFI) {
		pr_err("[%s] wrong parameter: %d\n", __func__, radio);
		return -1;
	}

	if (consys_sema_acquire_timeout(CONN_SEMA_CONN_INFRA_COMMON_SYSRAM_INDEX, CONN_SEMA_TIMEOUT) == CONN_SEMA_GET_FAIL) {
		pr_err("[%s] acquire semaphore (%d) timeout\n",
			__func__, CONN_SEMA_CONN_INFRA_COMMON_SYSRAM_INDEX);
		return -1;
	}

	if (on) {
		CONSYS_SET_BIT(
			CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_RADIO_STATUS,
			(0x1 << radio));
	} else {
		CONSYS_CLR_BIT(
			CONN_INFRA_SYSRAM_BASE_ADDR + CONN_INFRA_SYSRAM_SW_CR_RADIO_STATUS,
			(0x1 << radio));
	}

	consys_sema_release(CONN_SEMA_CONN_INFRA_COMMON_SYSRAM_INDEX);
	return 0;
}


bool consys_is_rc_mode_enable(void)
{
	int ret;

	ret = CONSYS_REG_READ_BIT(CON_REG_SPM_BASE_ADDR + SPM_RC_CENTRAL_CFG1, 0x1);

	return ret;
}

void consys_config_setup(void)
{
	/* To access CR in conninfra off domain, Conninfra should be on state */
	/* Enable conn_infra bus hang detect function
	 * Address: 0x1800_F000
	 * Data: 32'h32C8_001C
	 * Action: write
	 */
	CONSYS_REG_WRITE(CONN_DEBUG_CTRL_ADDR + CONN_DEBUG_CTRL_REG_OFFSET, 0x32c8001c);
}
