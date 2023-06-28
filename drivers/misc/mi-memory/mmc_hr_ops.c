#include "mi_memory.h"
#include "mmc_hr_ops.h"

int mmc_send_cxd_witharg_data(struct mmc_card *card, struct mmc_host *host,
               u32 opcode, u32 arg, void *buf, unsigned len)
{
       struct mmc_request mrq = {NULL};
       struct mmc_command cmd = {0};
       struct mmc_data data = {0};
       struct scatterlist sg;

       mrq.cmd = &cmd;
       mrq.data = &data;

       cmd.opcode = opcode;
       cmd.arg = arg;

       /* NOTE HACK:  the MMC_RSP_SPI_R1 is always correct here, but we
        * rely on callers to never use this with "native" calls for reading
        * CSD or CID.  Native versions of those commands use the R2 type,
        * not R1 plus a data block.
        */
       cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

       data.blksz = len;
       data.blocks = 1;
       data.flags = MMC_DATA_READ;
       data.sg = &sg;
       data.sg_len = 1;

       sg_init_one(&sg, buf, len);

       if (opcode == MMC_SEND_CSD || opcode == MMC_SEND_CID) {
               /*
                * The spec states that CSR and CID accesses have a timeout
                * of 64 clock cycles.
                */
               data.timeout_ns = 0;
               data.timeout_clks = 64;
       } else
               mmc_set_data_timeout(&data, card);

       mmc_wait_for_req(host, &mrq);

       if (cmd.error)
               return cmd.error;
       if (data.error)
               return data.error;

       return 0;
}

int mmc_get_cxd_witharg_data(struct mmc_card *card, struct mmc_host *host,
               u32 opcode, u32 arg, void *buf, unsigned len)
{
       struct mmc_request mrq = {NULL};
       struct mmc_command cmd = {0};
       struct mmc_data data = {0};
       struct scatterlist sg;

       mrq.cmd = &cmd;
       mrq.data = &data;

       cmd.opcode = opcode;
       cmd.arg = arg;

       /* NOTE HACK:  the MMC_RSP_SPI_R1 is always correct here, but we
        * rely on callers to never use this with "native" calls for reading
        * CSD or CID.  Native versions of those commands use the R2 type,
        * not R1 plus a data block.
        */
       cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

       data.blksz = len;
       data.blocks = 1;
       data.flags = MMC_DATA_WRITE;
       data.sg = &sg;
       data.sg_len = 1;

       sg_init_one(&sg, buf, len);

       if (opcode == MMC_SEND_CSD || opcode == MMC_SEND_CID) {
               /*
                * The spec states that CSR and CID accesses have a timeout
                * of 64 clock cycles.
                */
               data.timeout_ns = 0;
               data.timeout_clks = 64;
       } else
               mmc_set_data_timeout(&data, card);

       mmc_wait_for_req(host, &mrq);

       if (cmd.error)
               return cmd.error;
       if (data.error)
               return data.error;

       return 0;
}

int mmc_send_hr_cmd(struct mmc_card *card, u32 opcode, u32 arg)
{
       int err;
       struct mmc_command cmd = {0};

       BUG_ON(!card);
       BUG_ON(!card->host);

       cmd.opcode = opcode;
       cmd.arg = arg;
       cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
       cmd.busy_timeout = card->host->max_busy_timeout;

       err = mmc_wait_for_cmd(card->host, &cmd, MMC_CMD_RETRIES);
       if (err)
               return err;

       return 0;
}
int mmc_send_hr_cmd_withreq(struct mmc_card *card, u32 opcode, u32 arg,u32 flags, u32 *status)
{
       int err;
       struct mmc_command cmd = {0};

       BUG_ON(!card);
       BUG_ON(!card->host);

       cmd.opcode = opcode;
       cmd.arg = arg;
       cmd.flags = flags;
       cmd.busy_timeout = card->host->max_busy_timeout;

       err = mmc_wait_for_cmd(card->host, &cmd, MMC_CMD_RETRIES);
       if (status)
                *status = cmd.resp[0];
       if (err)
               return err;

       return 0;
}
static void mmc_prepare_mrq(struct mmc_card *card,
    struct mmc_request *mrq, struct scatterlist *sg, unsigned sg_len,
    unsigned dev_addr, unsigned blocks, unsigned blksz, int write)
{
    BUG_ON(!mrq || !mrq->cmd || !mrq->data || !mrq->stop);

    if (blocks > 1) {
        mrq->cmd->opcode = write ?
            MMC_WRITE_MULTIPLE_BLOCK : MMC_READ_MULTIPLE_BLOCK; //25:18
    } else {
        mrq->cmd->opcode = write ?
            MMC_WRITE_BLOCK : MMC_READ_SINGLE_BLOCK;//24:17
    }

    mrq->cmd->arg = dev_addr;
    if (!mmc_card_blockaddr(card))
        mrq->cmd->arg <<= 9;

    mrq->cmd->flags = MMC_RSP_R1 | MMC_CMD_ADTC;

    if (blocks == 1)
        mrq->stop = NULL;
    else {
        mrq->stop->opcode = MMC_STOP_TRANSMISSION;
        mrq->stop->arg = 0;
        mrq->stop->flags = MMC_RSP_R1B | MMC_CMD_AC;
    }

    mrq->data->blksz = blksz;
    mrq->data->blocks = blocks;
    mrq->data->flags = write ? MMC_DATA_WRITE : MMC_DATA_READ;
    mrq->data->sg = sg;
    mrq->data->sg_len = sg_len;
    mmc_set_data_timeout(mrq->data, card);
}

int mmc_rw_blocks(struct mmc_card *card,
    u8 *buffer, unsigned addr, unsigned blksz, int write)
{
    struct mmc_request mrq = {0};
    struct mmc_command cmd = {0};
    struct mmc_command stop = {0};
    struct mmc_data data = {0};

    struct scatterlist sg;

    mrq.cmd = &cmd;
    mrq.data = &data;
    mrq.stop = &stop;

    sg_init_one(&sg, buffer, blksz);

    mmc_prepare_mrq(card, &mrq, &sg, 1, addr, 1, blksz, write);
    cmd.busy_timeout = 10000000;

    data.timeout_ns = 4000000000u; /* 4s */
    data.timeout_clks = 0;
    mmc_wait_for_req(card->host, &mrq);

    if (cmd.error) {
        pr_err("cmd timeout");
        return cmd.error;
    }
    if (data.error) {
        pr_err("data timeout");
        return data.error;
    }
    return 0;
}
int mmc_rw_ymtcblocks(struct mmc_card *card,
    u8 *buffer, unsigned addr, unsigned buflen, unsigned blksz, int write)
{
    struct mmc_request mrq = {0};
    struct mmc_command cmd = {0};
    struct mmc_command stop = {0};
    struct mmc_data data = {0};

    struct scatterlist sg;

    unsigned blks = ((buflen + blksz - 1) & (~(blksz - 1))) >> (ffs(blksz) - 1);
    pr_err("blks = %d\n",blks);

    mrq.cmd = &cmd;
    mrq.data = &data;
    mrq.stop = &stop;

    sg_init_one(&sg, buffer, blks * blksz);

    mmc_prepare_mrq(card, &mrq, &sg, 1, addr, blks, blksz, write);
    cmd.busy_timeout = 10000000;

    data.timeout_ns = 4000000000u; /* 4s */
    data.timeout_clks = 0;

    mmc_wait_for_req(card->host, &mrq);

    if (cmd.error) {
        pr_err("cmd timeout");
        return cmd.error;
    }
    if (data.error) {
        pr_err("data timeout");
        return data.error;
    }

    return 0;
}
#include "sni.h"

int mmc_get_nandinfo_data(struct mmc_card *card, void *buf)
{
    u8 *ext_csd;
    int err = 0;
    u8 *sni_buf;

    ext_csd = kzalloc(512, GFP_KERNEL);
    if (!ext_csd)
        return -ENOMEM;

    sni_buf = kzalloc(512, GFP_KERNEL);
    if (!sni_buf)
        return -ENOMEM;

    pr_err("[mi-memory-hr]:samsung nandinfo-send cxd data.\n");
    err = mmc_send_cxd_witharg_data(card, card->host, MMC_SEND_EXT_CSD, 0, ext_csd, 512);
    if (err) {
        pr_err("get csd failed %d\n", err);
        goto out;
    }
    if (!(ext_csd[493] & (1<<1))) {
        pr_err("VSM is not supported\n");
        goto out;
    }
    err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
            EXT_CSD_MODE_CONFIG, 0x10, card->ext_csd.generic_cmd6_time);
    if (err) {
        pr_err("switch to VSM failed %d", err);
        goto out;
    }

    memcpy(sni_buf, sni, 512);

    pr_err("[mi-memory-hr]:samsung nandinfo-password.\n");
    /*write password*/
    err = mmc_rw_blocks(card, sni_buf, 0xC7810000, 512, 1);
    if(err) {
        pr_err("write password error %d", err);
        goto out;
    }

    /*write password*/
    err = mmc_rw_blocks(card, buf, 0xC7810000, 512, 0);
    if(err) {
        pr_err("read nandinfo error %d", err);
        goto out;
    }
    err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
            EXT_CSD_MODE_CONFIG, 0x0, card->ext_csd.generic_cmd6_time);
    if (err) {
        pr_err("exit VSM failed %d", err);
        goto out;
    }

out:
    kfree(ext_csd);
    kfree(sni_buf);
    return err;
}


int mmc_get_osv_data(struct mmc_card *card,  void *buf)
{
    __aligned(1024) char  request_data_frame[512] = {0x25, 0xa1, 0x98, 0x15,
                    0, 0, 0, 0,
                    0, 0,
                    0, 0,
                    0, 0, 0, 0,
                    0x03, 0x0, 0x02, 0x00};

    int err = 0;

    err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
            EXT_CSD_VENDOR_EXT_FEATURES_ENABLE, 0x2, card->ext_csd.generic_cmd6_time);
    if (err) {
        pr_err("osv switch fail %d", err);
    }
    msleep(100);

    err = mmc_rw_blocks(card, request_data_frame, 0xD7EF0326, 512, 1);
    if (err) {
        pr_err("osv write fail %d", err);
    }
    msleep(100);

    err = mmc_rw_blocks(card, buf, 0xD7EF0326, 512, 0);
    if (err) {
        pr_err("osv read fail%d", err);
    }
    msleep(100);

    return err;
}

int mmc_send_micron_hr(struct mmc_card *card, struct mmc_host *host,
        u32 opcode, void *buf, unsigned len)
{
    struct mmc_request mrq = {NULL};
    struct mmc_command cmd = {0};
    struct mmc_data data = {0};
    struct scatterlist sg;

    mrq.cmd = &cmd;
    mrq.data = &data;

    cmd.opcode = opcode;
    cmd.arg = 0xd;

    /* NOTE HACK:  the MMC_RSP_SPI_R1 is always correct here, but we
     * rely on callers to never use this with "native" calls for reading
     * CSD or CID.  Native versions of those commands use the R2 type,
     * not R1 plus a data block.
     */
    cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

    data.blksz = len;
    data.blocks = 1;
    data.flags = MMC_DATA_READ;
    data.sg = &sg;
    data.sg_len = 1;

    sg_init_one(&sg, buf, len);

    if (opcode == MMC_SEND_CSD || opcode == MMC_SEND_CID) {
        /*
         * The spec states that CSR and CID accesses have a timeout
         * of 64 clock cycles.
         */
        data.timeout_ns = 0;
        data.timeout_clks = 64;
    } else if ( opcode == 18 ) { // ymtc hr data for 2s
        data.timeout_ns = 2000000000;
        data.timeout_clks = 0;
    } else
        mmc_set_data_timeout(&data, card);

    mmc_wait_for_req(host, &mrq);

    if (cmd.error)
        return cmd.error;
    if (data.error)
        return data.error;

    return 0;
}
