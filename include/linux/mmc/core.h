/*
 *  linux/include/linux/mmc/core.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LINUX_MMC_CORE_H
#define LINUX_MMC_CORE_H

#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/blkdev.h>

struct mmc_data;
struct mmc_request;

enum mmc_blk_status {
	MMC_BLK_SUCCESS = 0,
	MMC_BLK_PARTIAL,
	MMC_BLK_CMD_ERR,
	MMC_BLK_RETRY,
	MMC_BLK_ABORT,
	MMC_BLK_DATA_ERR,
	MMC_BLK_ECC_ERR,
	MMC_BLK_NOMEDIUM,
	MMC_BLK_NEW_REQUEST,
};

struct mmc_command {
	u32			opcode;
	u32			arg;
#define MMC_CMD23_ARG_REL_WR	(1 << 31)
#define MMC_CMD23_ARG_PACKED	((0 << 31) | (1 << 30))
#define MMC_CMD23_ARG_TAG_REQ	(1 << 29)
	u32			resp[4];
	unsigned int		flags;		/* expected response type */
#define MMC_RSP_PRESENT	(1 << 0)
#define MMC_RSP_136	(1 << 1)		/* 136 bit response */
#define MMC_RSP_CRC	(1 << 2)		/* expect valid crc */
#define MMC_RSP_BUSY	(1 << 3)		/* card may send busy */
#define MMC_RSP_OPCODE	(1 << 4)		/* response contains opcode */

#define MMC_CMD_MASK	(3 << 5)		/* non-SPI command type */
#define MMC_CMD_AC	(0 << 5)
#define MMC_CMD_ADTC	(1 << 5)
#define MMC_CMD_BC	(2 << 5)
#define MMC_CMD_BCR	(3 << 5)

#define MMC_RSP_SPI_S1	(1 << 7)		/* one status byte */
#define MMC_RSP_SPI_S2	(1 << 8)		/* second byte */
#define MMC_RSP_SPI_B4	(1 << 9)		/* four data bytes */
#define MMC_RSP_SPI_BUSY (1 << 10)		/* card may send busy */

/*
 * These are the native response types, and correspond to valid bit
 * patterns of the above flags.  One additional valid pattern
 * is all zeros, which means we don't expect a response.
 */
#define MMC_RSP_NONE	(0)
#define MMC_RSP_R1	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R1B	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE|MMC_RSP_BUSY)
#define MMC_RSP_R2	(MMC_RSP_PRESENT|MMC_RSP_136|MMC_RSP_CRC)
#define MMC_RSP_R3	(MMC_RSP_PRESENT)
#define MMC_RSP_R4	(MMC_RSP_PRESENT)
#define MMC_RSP_R5	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R6	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)
#define MMC_RSP_R7	(MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE)

/* Can be used by core to poll after switch to MMC HS mode */
#define MMC_RSP_R1_NO_CRC	(MMC_RSP_PRESENT|MMC_RSP_OPCODE)

#define mmc_resp_type(cmd)	((cmd)->flags & (MMC_RSP_PRESENT|MMC_RSP_136|MMC_RSP_CRC|MMC_RSP_BUSY|MMC_RSP_OPCODE))

/*
 * These are the SPI response types for MMC, SD, and SDIO cards.
 * Commands return R1, with maybe more info.  Zero is an error type;
 * callers must always provide the appropriate MMC_RSP_SPI_Rx flags.
 */
#define MMC_RSP_SPI_R1	(MMC_RSP_SPI_S1)
#define MMC_RSP_SPI_R1B	(MMC_RSP_SPI_S1|MMC_RSP_SPI_BUSY)
#define MMC_RSP_SPI_R2	(MMC_RSP_SPI_S1|MMC_RSP_SPI_S2)
#define MMC_RSP_SPI_R3	(MMC_RSP_SPI_S1|MMC_RSP_SPI_B4)
#define MMC_RSP_SPI_R4	(MMC_RSP_SPI_S1|MMC_RSP_SPI_B4)
#define MMC_RSP_SPI_R5	(MMC_RSP_SPI_S1|MMC_RSP_SPI_S2)
#define MMC_RSP_SPI_R7	(MMC_RSP_SPI_S1|MMC_RSP_SPI_B4)

#define mmc_spi_resp_type(cmd)	((cmd)->flags & \
		(MMC_RSP_SPI_S1|MMC_RSP_SPI_BUSY|MMC_RSP_SPI_S2|MMC_RSP_SPI_B4))

/*
 * These are the command types.
 */
#define mmc_cmd_type(cmd)	((cmd)->flags & MMC_CMD_MASK)

	unsigned int		retries;	/* max number of retries */
	int			error;		/* command error */

/*
 * Standard errno values are used for errors, but some have specific
 * meaning in the MMC layer:
 *
 * ETIMEDOUT    Card took too long to respond
 * EILSEQ       Basic format problem with the received or sent data
 *              (e.g. CRC check failed, incorrect opcode in response
 *              or bad end bit)
 * EINVAL       Request cannot be performed because of restrictions
 *              in hardware and/or the driver
 * ENOMEDIUM    Host can determine that the slot is empty and is
 *              actively failing requests
 */

	unsigned int		busy_timeout;	/* busy detect timeout in ms */
	/* Set this flag only for blocking sanitize request */
	bool			sanitize_busy;
	/* Set this flag only for blocking bkops request */
	bool			bkops_busy;

	struct mmc_data		*data;		/* data segment associated with cmd */
	struct mmc_request	*mrq;		/* associated request */
};

struct mmc_data {
	unsigned int		timeout_ns;	/* data timeout (in ns, max 80ms) */
	unsigned int		timeout_clks;	/* data timeout (in clocks) */
	unsigned int		blksz;		/* data block size */
	unsigned int		blocks;		/* number of blocks */
	unsigned int		blk_addr;	/* block address */
	int			error;		/* data error */
	unsigned int		flags;

#define MMC_DATA_WRITE		BIT(8)
#define MMC_DATA_READ		BIT(9)
/* Extra flags used by CQE */
#define MMC_DATA_QBR		BIT(10)		/* CQE queue barrier*/
#define MMC_DATA_PRIO		BIT(11)		/* CQE high priority */
#define MMC_DATA_REL_WR		BIT(12)		/* Reliable write */
#define MMC_DATA_DAT_TAG	BIT(13)		/* Tag request */
#define MMC_DATA_FORCED_PRG	BIT(14)		/* Forced programming */

	unsigned int		bytes_xfered;

	struct mmc_command	*stop;		/* stop command */
	struct mmc_request	*mrq;		/* associated request */

	unsigned int		sg_len;		/* size of scatter list */
	int			sg_count;	/* mapped sg entries */
	struct scatterlist	*sg;		/* I/O scatter list */
	s32			host_cookie;	/* host private data */
	bool			fault_injected; /* fault injected */
};

struct mmc_host;
struct mmc_request {
	struct mmc_command	*sbc;		/* SET_BLOCK_COUNT for multiblock */
	struct mmc_command	*cmd;
	struct mmc_data		*data;
	struct mmc_command	*stop;

	struct completion	completion;
	struct completion	cmd_completion;
	void			(*done)(struct mmc_request *);/* completion function */
	/*
	 * Notify uppers layers (e.g. mmc block driver) that recovery is needed
	 * due to an error associated with the mmc_request. Currently used only
	 * by CQE.
	 */
	void			(*recovery_notifier)(struct mmc_request *);
	struct mmc_host		*host;
	struct mmc_cmdq_req	*cmdq_req;

	struct request		*req;
	/* Allow other commands during this ongoing data transfer or busy wait */
	bool			cap_cmd_during_tfr;
	ktime_t			io_start;
#ifdef CONFIG_BLOCK
	int			lat_hist_enabled;
#endif

	int			tag;
#ifdef CONFIG_MMC_CRYPTO
	int crypto_key_slot;
	u64 data_unit_num;
	const struct blk_crypto_key *crypto_key;
#endif
};

#ifdef CONFIG_MMC_CRYPTO
static inline bool mmc_request_crypto_enabled(const struct mmc_request *mrq)
{
	return mrq->crypto_key != NULL;
}
#else
static inline bool mmc_request_crypto_enabled(const struct mmc_request *mrq)
{
	return false;
}
#endif

struct mmc_card;
struct mmc_cmdq_req;

extern int mmc_cmdq_discard_queue(struct mmc_host *host, u32 tasks);
extern int mmc_cmdq_halt(struct mmc_host *host, bool enable);
extern int mmc_cmdq_halt_on_empty_queue(struct mmc_host *host);
extern void mmc_cmdq_post_req(struct mmc_host *host, int tag, int err);
extern int mmc_cmdq_start_req(struct mmc_host *host,
			      struct mmc_cmdq_req *cmdq_req);
extern int mmc_cmdq_prepare_flush(struct mmc_command *cmd);
extern int mmc_cmdq_wait_for_dcmd(struct mmc_host *host,
			struct mmc_cmdq_req *cmdq_req);
extern int mmc_cmdq_erase(struct mmc_cmdq_req *cmdq_req,
	      struct mmc_card *card, unsigned int from, unsigned int nr,
	      unsigned int arg);
extern void mmc_check_bkops(struct mmc_card *card);
extern void mmc_start_manual_bkops(struct mmc_card *card);
extern int mmc_set_auto_bkops(struct mmc_card *card, bool enable);
extern void mmc_flush_detect_work(struct mmc_host *host);
extern int mmc_cmdq_hw_reset(struct mmc_host *host);
extern int mmc_try_claim_host(struct mmc_host *host, unsigned int delay);

extern void mmc_get_card(struct mmc_card *card);
extern void mmc_put_card(struct mmc_card *card);
extern void __mmc_put_card(struct mmc_card *card);
extern void mmc_blk_init_bkops_statistics(struct mmc_card *card);

extern void mmc_deferred_scaling(struct mmc_host *host);
extern void mmc_cmdq_clk_scaling_start_busy(struct mmc_host *host,
	bool lock_needed);
extern void mmc_cmdq_clk_scaling_stop_busy(struct mmc_host *host,
	bool lock_needed, bool is_cmdq_dcmd);
extern void mmc_cmdq_up_rwsem(struct mmc_host *host);
extern int mmc_cmdq_down_rwsem(struct mmc_host *host, struct request *rq);
extern int __mmc_switch_cmdq_mode(struct mmc_command *cmd, u8 set, u8 index,
				  u8 value, unsigned int timeout_ms,
				  bool use_busy_signal, bool ignore_timeout);

void mmc_wait_for_req(struct mmc_host *host, struct mmc_request *mrq);
int mmc_wait_for_cmd(struct mmc_host *host, struct mmc_command *cmd,
		int retries);

int mmc_hw_reset(struct mmc_host *host);
void mmc_set_data_timeout(struct mmc_data *data, const struct mmc_card *card);

#endif /* LINUX_MMC_CORE_H */
