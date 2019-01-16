
#ifndef __EMMC_INIT__
#define __EMMC_INIT__

#include <linux/mmc/card.h>
#include <mach/mt_typedefs.h>
#define MTK_MMC_DUMP_DBG	(0)
#define MAX_POLLING_STATUS (50000)

struct simp_mmc_card;
struct simp_msdc_host;
#define MSDC_PS_DAT0            (0x1  << 16)	/* R  */
struct simp_mmc_host {
	int index;
	unsigned int f_min;
	unsigned int f_max;
	unsigned int f_init;
	u32 ocr_avail;
	u32 ocr_avail_sdio;	/* SDIO-specific OCR */
	u32 ocr_avail_sd;	/* SD-specific OCR */
	u32 ocr_avail_mmc;	/* MMC-specific OCR */

	unsigned long caps;	/* Host capabilities */
	unsigned int caps2;	/* More host capabilities */

	/* host specific block data */
	unsigned int		max_seg_size;	/* see blk_queue_max_segment_size */
	unsigned short		max_segs;	/* see blk_queue_max_segments */
	unsigned short		unused;
	unsigned int		max_req_size;	/* maximum number of bytes in one req */
	unsigned int		max_blk_size;	/* maximum size of one mmc block */
	unsigned int		max_blk_count;	/* maximum number of blocks in one req */
	unsigned int		max_discard_to;	/* max. discard timeout in ms */
	
	u32			ocr;		/* the current OCR setting */

	struct simp_mmc_card		*card;		/* device attached to this host */

	unsigned int		actual_clock;	/* Actual HC clock rate */

    /* add msdc struct */
    struct simp_msdc_host* mtk_host;
};

struct simp_mmc_card {
	struct simp_mmc_host *host;	/* the host this device belongs to */
	unsigned int rca;	/* relative card address of device */
	unsigned int type;	/* card type */
	unsigned int state;	/* (our) card state */
	unsigned int quirks;	/* card quirks */

	unsigned int erase_size;	/* erase size in sectors */
	unsigned int erase_shift;	/* if erase unit is power 2 */
	unsigned int pref_erase;	/* in sectors */
	u8 erased_byte;		/* value of erased bytes */

	u32 raw_cid[4];		/* raw card CID */
	u32 raw_csd[4];		/* raw card CSD */
	u32 raw_scr[2];		/* raw card SCR */
	struct mmc_cid cid;	/* card identification */
	struct mmc_csd csd;	/* card specific */
	struct mmc_ext_csd ext_csd;	/* mmc v4 extended card specific */
	struct sd_scr scr;	/* extra SD information */
	struct sd_ssr ssr;	/* yet more SD information */
	struct sd_switch_caps sw_caps;	/* switch (CMD6) caps */

	unsigned int sd_bus_speed;	/* Bus Speed Mode set for the card */
};

struct simp_msdc_card {
	unsigned int rca;	/* relative card address of device */
	unsigned int type;	/* card type */
	unsigned short state;	/* (our) card state */
	unsigned short file_system;	/* FAT16/FAT32 */
	unsigned short card_cap;	/* High Capcity/standard */
};

struct simp_msdc_host {
	struct simp_msdc_card *card;
	void __iomem      *base;         /* host base address */
	unsigned char id;	/* host id number */
	unsigned int clk;	/* host clock speed *//* clock value from clock source */
	unsigned int sclk;	/* SD/MS clock speed *//* working clock */
	unsigned char clksrc;	/* clock source */
	void *priv;		/* private data */
};

typedef enum {
	MSDC_CLKSRC_200M = 0
} CLK_SOURCE_T;

enum {
	FAT16 = 0,
	FAT32 = 1,
	exFAT = 2,
	_RAW_ = 3,
};

enum {
	standard_capacity = 0,
	high_capacity = 1,
	extended_capacity = 2,
};

/* command define */
#define CMD0     (0)		/* GO_IDLE_STATE             */
#define CMD1     (1)		/* mmc: SEND_OP_COND         */
#define CMD2     (2)		/* mmc: ALL_SEND_CID         */
#define CMD3     (3)		/* mmc: SET_RELATIVE_ADDR    */
#define CMD7     (7)		/* mmc: SELECT/DESELECT_CARD */
#define CMD8     (8)		/* SEND_IF_COND              */
#define CMD9     (9)		/* mmc: SEND_CSD             */
#define CMD10    (10)		/* mmc: SEND_CID             */
#define CMD55    (55)		/* APP_CMD                   */
#define ACMD41   (41)		/* SD_SEND_OP_COND           */
#define CMD13    (13)		/* SEND_STATUS               */
#define ACMD42   (42)		/* SET_CLR_CARD_DETECT       */
#define ACMD6    (6)		/* SET_BUS_WIDTH             */

/* #define CMD16    (16) */ /* don't need CMD16 [Fix me] how to confirm block_len is 512 bytes */
#define CMD17    (17)
#define CMD18    (18)
#define CMD24    (24)
#define CMD25    (25)
#define CMD12    (12)

/* command argument */
#define CMD0_ARG             (0)

#define CMD8_ARG_VOL_27_36   (0x100)
#define CMD8_ARG_PATTERN     (0x5a)	/* or 0xAA */
#define CMD8_ARG             (CMD8_ARG_VOL_27_36 | CMD8_ARG_PATTERN)

#define CMD55_ARG            (phost->card->rca)

#define ACMD41_ARG_HCS       (1 << 30)
#define ACMD41_ARG_VOL_27_36 (0xff8000)
#define ACMD41_ARG_20        (ACMD41_ARG_VOL_27_36 | ACMD41_ARG_HCS)
#define ACMD41_ARG_10        (ACMD41_ARG_VOL_27_36)

#define CMD2_ARG             (0)
#define CMD3_ARG             (0)
#define CMD9_ARG             (phost->card->rca)
#define CMD10_ARG            (phost->card->rca)
#define CMD13_ARG            (phost->card->rca)
#define CMD7_ARG             (phost->card->rca)

#define ACMD42_ARG_CLR_CD    (0)
#define ACMD42_ARG_SET_CD    (1)
#define ACMD42_ARG           (ACMD42_ARG_CLR_CD)

#define ACMD6_ARG_BUS_WIDTH_4 (0x2)
#define ACMD6_ARG             (ACMD6_ARG_BUS_WIDTH_4)
#define EXT_CSD_CMD_SET_NORMAL	(1<<0)
/* #define MMC_SWITCH_MODE_WRITE_BYTE (0x03) */
/* #define EXT_CSD_BUS_WIDTH             (183) */
/* #define EXT_CSD_BUS_WIDTH_4           (1) */
#define ACMD6_ARG_EMMC		  ((MMC_SWITCH_MODE_WRITE_BYTE << 24) | (EXT_CSD_BUS_WIDTH << 16) | (EXT_CSD_BUS_WIDTH_4 << 8) | EXT_CSD_CMD_SET_NORMAL)
#ifdef MTK_MSDC_USE_CACHE
#define ACMD6_ARG_DISABLE_CACHE		  ((MMC_SWITCH_MODE_WRITE_BYTE << 24) | (EXT_CSD_CACHE_CTRL << 16) | (0 << 8) | EXT_CSD_CMD_SET_NORMAL)
#endif

#define CMD17_ARG    (data_address)	/* in bytes units in a SDSC */
#define CMD18_ARG    (data_address)	/* in block units in a SDHC (512 bytes) */
#define CMD24_ARG    (data_address)
#define CMD25_ARG    (data_address)
#define CMD12_ARG    (0)
#define CMD8_RAW_EMMC     CMD_RAW(8 , msdc_rsp[RESP_R1]  , 1, 0,   512, 0)	/* 0x88 -> R1 */

#define CMD_RAW(cmd, rspt, dtyp, rw, len, stop) \
	  (cmd) | (rspt << 7) | \
	  (dtyp << 11) | (rw << 13) | (len << 16) | \
	  (stop << 14)

/* compare the value with mt6573 [Fix me]*/
#define CMD0_RAW     CMD_RAW(0 , msdc_rsp[RESP_NONE], 0, 0,   0, 0)
#define CMD1_RAW     CMD_RAW(1 , msdc_rsp[RESP_R3], 0, 0,   0, 0)
#define CMD2_RAW     CMD_RAW(2 , msdc_rsp[RESP_R2], 0, 0,   0, 0)
#define CMD3_RAW     CMD_RAW(3 , msdc_rsp[RESP_R1], 0, 0,   0, 0)
#define CMD7_RAW     CMD_RAW(7 , msdc_rsp[RESP_R1], 0, 0,   0, 0)
#define CMD8_RAW     CMD_RAW(8 , msdc_rsp[RESP_R7]  , 0, 0,   0, 0)	/* 0x88 -> R1 */
#define CMD9_RAW     CMD_RAW(9 , msdc_rsp[RESP_R2]  , 0, 0,   0, 0)
#define CMD10_RAW    CMD_RAW(10, msdc_rsp[RESP_R2]  , 0, 0,   0, 0)
#define CMD55_RAW    CMD_RAW(55, msdc_rsp[RESP_R1]  , 0, 0,   0, 0)	/* R1 not R3! */
#define ACMD41_RAW   CMD_RAW(41, msdc_rsp[RESP_R3]  , 0, 0,   0, 0)
#define CMD13_RAW    CMD_RAW(13, msdc_rsp[RESP_R1]  , 0, 0,   0, 0)
#define ACMD42_RAW   CMD_RAW(42, msdc_rsp[RESP_R1]  , 0, 0,   0, 0)
#define ACMD6_RAW    CMD_RAW(6 , msdc_rsp[RESP_R1]  , 0, 0,   0, 0)
#define ACMD6_RAW_EMMC    CMD_RAW(6 , msdc_rsp[RESP_R1B]  , 0, 0,   0, 0)

/* block size always 512 [Fix me] */
#define CMD17_RAW    CMD_RAW(17, msdc_rsp[RESP_R1]  , 1, 0, 512, 0)	/* single   + read  +  */
#define CMD18_RAW    CMD_RAW(18, msdc_rsp[RESP_R1]  , 2, 0, 512, 0)	/* multiple + read  +  */
#define CMD24_RAW    CMD_RAW(24, msdc_rsp[RESP_R1]  , 1, 1, 512, 0)	/* single   + write +  */
#define CMD25_RAW    CMD_RAW(25, msdc_rsp[RESP_R1]  , 2, 1, 512, 0)	/* multiple + write +  */
#define CMD12_RAW    CMD_RAW(12, msdc_rsp[RESP_R1B] , 0, 0,   0, 1)

/* command response */
#define R3_OCR_POWER_UP_BIT        (1 << 31)
#define R3_OCR_CARD_CAPACITY_BIT   (1 << 30)

#define REG_VEMC33_VOLSEL (0x56a)
#define REG_VEMC33_EN     (0x552)
#define REG_VMC_VOLSEL    (0x566)
#define REG_VMC_EN        (0x54e)
#define REG_VMCH_VOLSEL   (0x568)
#define REG_VMCH_EN       (0x550)

#define MASK_VEMC33_VOLSEL (0x1 << 6)
#define MASK_VEMC33_EN     (0x1 << 10)
#define MASK_VMC_VOLSEL    (0x7 << 4)
#define MASK_VMC_EN        (0x1 << 10)
#define MASK_VMCH_VOLSEL   (0x1 << 6)
#define MASK_VMCH_EN       (0x1 << 10)

#ifdef FPGA_PLATFORM
extern bool hwPowerOn_fpga(void);
extern bool hwPowerSwitch_fpga(void);
extern bool hwPowerDown_fpga(void);
#else
extern S32 pwrap_read_nochk(U32  adr, U32 *rdata);
extern S32 pwrap_write_nochk(U32  adr, U32  wdata);
#define msdc_power_set_field(reg, field, val) \
	do {    \
		volatile unsigned int tv;  \
		pwrap_read_nochk(reg, &tv);	\
		tv &= ~(field); \
		tv |= ((val) << (uffs((unsigned int)field) - 1)); \
		pwrap_write_nochk(reg, tv); \
	} while (0)
#define msdc_power_get_field(reg, field, val) \
	do {	\
		volatile unsigned int tv;  \
		pwrap_read_nochk(reg, &tv);	\
		val = ((tv & (field)) >> (uffs((unsigned int)field) - 1)); \
	} while (0)
#if 0
static unsigned int simp_msdc_ldo_power(unsigned int on, MT65XX_POWER powerId,
					MT65XX_POWER_VOLTAGE powerVolt)
{
	/* Fixme: must realize access register directly */
	if (on)
		hwPowerOn(powerId, powerVolt, "msdc");
	else
		hwPowerDown(powerId, "msdc");
	return SIMP_SUCCESS;
}
#endif
#endif

extern struct msdc_hw msdc0_hw;
#if defined(CFG_DEV_MSDC1)
extern struct msdc_hw msdc1_hw;
#endif

#endif /* end of __EMMC_INIT__ */




