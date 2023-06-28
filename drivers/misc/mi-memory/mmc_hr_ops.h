#ifndef _MMC_HR_OPS_H_
#define _MMC_HR_OPS_H_


int mmc_send_cxd_witharg_data(struct mmc_card *card, struct mmc_host *host,
               u32 opcode, u32 arg, void *buf, unsigned len);

int mmc_get_cxd_witharg_data(struct mmc_card *card, struct mmc_host *host,
               u32 opcode, u32 arg, void *buf, unsigned len);

int mmc_send_hr_cmd(struct mmc_card *card, u32 opcode, u32 arg);
int mmc_send_hr_cmd_withreq(struct mmc_card *card, u32 opcode, u32 arg,u32 flags, u32 *status);
int mmc_rw_blocks(struct mmc_card *card,
    u8 *buffer, unsigned addr, unsigned blksz, int write);
int mmc_rw_ymtcblocks(struct mmc_card *card,
        u8 *buffer, unsigned addr, unsigned buflen, unsigned blksz, int write);
int mmc_get_nandinfo_data(struct mmc_card *card, void *buf);
int mmc_get_osv_data(struct mmc_card *card,  void *buf);
int mmc_send_micron_hr(struct mmc_card *card, struct mmc_host *host,
        u32 opcode, void *buf, unsigned len);

#endif/*_MMC_HR_OPS_H_*/

