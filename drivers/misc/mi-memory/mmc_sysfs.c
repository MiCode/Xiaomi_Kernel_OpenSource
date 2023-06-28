#include "mi_memory.h"
#include "mmc_hr_ops.h"
#include "../../mmc/core/mmc_ops.h"
//#include "../../mmc/core/core.h"

#define MMC_STATE_CMDQ      (1<<12)         /* card is in cmd queue mode */
#define mmc_card_cmdq(c)        ((c)->state & MMC_STATE_CMDQ)

extern struct mmc_card *mv_card;
#ifdef CONFIG_MTK_EMMC_HW_CQ
extern int mmc_cmdq_halt_on_empty_queue(struct mmc_host *host);
extern int mmc_cmdq_halt(struct mmc_host *host, bool halt);
#endif

int get_hynix_hr(struct mmc_card *card, char *buf)
{
    int err = 0;
    if (!strncmp(card->cid.prod_name, "hDEaP3", 6) ||
        !strncmp(card->cid.prod_name, "hC8aP>", 6) ||
        !strncmp(card->cid.prod_name, "hB8aP>", 6) ||
        !strncmp(card->cid.prod_name, "hB8aP?>", 6)||
        !strncmp(card->cid.prod_name, "hC9aP3>", 6)) {
        err = mmc_send_cxd_witharg_data(card, card->host, MMC_SEND_EXT_CSD, 0x53454852, buf, 512);
        if (err)
            pr_mem_err("HY: fail to get hr of hynix.err:%d\n", err);
    } else {
        /* Get the Extended Health Roport for Hynix */
        err = mmc_send_hr_cmd(card, 60, 0x534D4900);
        if (err) {
            pr_mem_err("HY: vc 1st cmd failed %d\n", err);
            goto out;
        }

        err = mmc_send_hr_cmd(card, 60, 0x48525054);
        if (err) {
            pr_mem_err("HY: vc 2nd cmd failed %d\n", err);
            goto out;
        }

        err = mmc_send_cxd_witharg_data(card, card->host, MMC_SEND_EXT_CSD, 0, buf, 512);
        if (err)
            pr_mem_err("HY: fail to get hr of hynix.err:%d\n", err);
    }

out:
    return err;
}

#undef MICRON_HR_CMD56 1

int get_micron_hr(struct mmc_card *card, char *buf)
{
    int err = 0;
#ifdef MICRON_HR_CMD56
    /* Get the Extended Health Roport for Micron */
    /* 1. Set blocklen of 512bytes using CMD16 */
    err = mmc_set_blocklen(card, 512);
    if (err) {
        pr_err("%s: Set blocklen of 512bytes using CMD16 failed %d\n", __func__, err);
        goto out;
    }
    /* 2. Get HR data using CMD56 */
    err = mmc_send_micron_hr(card, card->host, MMC_GEN_CMD, buf, 512);
    if (err) {
        pr_mem_err("MC: fail to get hr of micron.err:%d\n", err);
        goto out;
    }
out:
#else
    err = mmc_send_cxd_witharg_data(card, card->host, MMC_SEND_EXT_CSD, 0x0d, buf, 512);
    if (err)
        pr_mem_err("MC: fail to get hr of micron.err:%d\n", err);
#endif

    return err;
}

int get_ss_hr(struct mmc_card *card, char *buf)
{
    int err;
    err = mmc_set_blocklen(card, 512);
    if (err) {
        pr_err("SS: set blocklen to 512 fail.err:%d\n", err);
        goto out;
    }

#ifdef OSV
    err = mmc_get_osv_data(card, buf);
    if (err) {
        pr_err("SS: get osv data fail.err:%d\n", err);
        goto out;
    }
#else
    err = mmc_get_nandinfo_data(card, buf);
    if (err)
        pr_err("SS: get nandinfo fail.err:%d\n", err);
#endif
out:
    return err;

}

int get_wc_hr(struct mmc_card *card, char *buf)
{
    int err;
    /*1.Enable device report mode*/
    err = mmc_send_hr_cmd(card, 62, 0x96c9d71c);
    if (err) {
        pr_mem_err("WC: vc 1st cmd failed %d\n", __func__, err);
        goto out;
    }
    /*2.read device report data*/
    err = mmc_send_cxd_witharg_data(card, card->host, 63, 0, buf, 512);
    if (err)
        pr_mem_err("WC: fail to get hr of wdc.err:%d\n", err);

out:
    return err;
}

int get_ymtc_hr(struct mmc_card *card, char *buf)
{
    int err = 0, i = 0;
    int status;
    if (!strncmp(card->cid.prod_name, "Y0S128", 6) ||
        !strncmp(card->cid.prod_name, "Y0S064", 6) ||
        !strncmp(card->cid.prod_name, "Y0S256", 6)){
        /* disable cmdq ,or the following operation will fail.*/
        if (mmc_cmdq_disable(card))
            pr_err("disable cmdq failed befor hr_ymtc\n");
        //cqhci_disable(card->host);
        pr_err("disable cmdq befor hr_ymtc\n");
        err = mmc_send_hr_cmd_withreq(card, 60, 0x594d5443, MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC, &status);
        if (err) {
            mmc_cmdq_enable(card);
            pr_err("%s: CMD60 1 failed %d,the status 0x%04x\n", __func__, err, status);
            goto out;
        }
        err = mmc_send_hr_cmd_withreq(card, 60, 0x5130FFFD, MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC, &status);
        if (err) {
            mmc_cmdq_enable(card);
            pr_err("%s: CMD60 2 failed %d,the status 0x%04x\n", __func__, err, status);
            goto out;
        }
        err = mmc_rw_ymtcblocks(card, buf, 0x1, 512*3, 512, 0);
        if (err) {
            mmc_cmdq_enable(card);
            pr_err("%s: CMD18 failed %d,the status 0x%04x\n", __func__, err, status);
            goto out;
        }
        pr_err("%s: CMD18 the status 0x%04x\n", __func__, status);
        mmc_cmdq_enable(card);
        pr_err("enable cmdq after hr_ymtc\n");
    }else{
        if (mmc_cmdq_disable(card))
            pr_err("disable cmdq failed befor hr_ymtc\n");
        //cqhci_disable(card->host);
        pr_err("disable cmdq befor hr_ymtc\n");
        err = mmc_send_cxd_witharg_data(card, card->host, 56, 0x594d54fb, buf, 512);
        if (err) {
            mmc_cmdq_enable(card);
            pr_mem_err("YMTC: CMD56 failed.err:%d\n", err);
            goto out;
        }
        err = mmc_send_status(card, &status);
        if (err) {
            mmc_cmdq_enable(card);
            pr_mem_err("YMTC: CMD13 failed.status:%d err:%d\n", status, err);
            goto out;
        }
        err = mmc_send_cxd_witharg_data(card, card->host, 56, 0x29, buf, 512);
        if (err) {
            mmc_cmdq_enable(card);
            pr_mem_err("YMTC: CMD56 failed.err:%d\n", err);
            goto out;
        }
        err = mmc_send_status(card, &status);
        if (err) {
            mmc_cmdq_enable(card);
            pr_mem_err("YMTC: CMD13 failed,status:%d err:%d\n", status, err);
            goto out;
        }
        mmc_cmdq_enable(card);
        pr_err("enable cmdq after hr_ymtc\n");
    }
out:
    return err;

}
int get_hosin_hr(struct mmc_card *card, char *buf)
{
    int err = 0;
    int status;
    err = mmc_send_cxd_witharg_data(card, card->host, 56, 0x4b534bfb, buf, 512);
    if (err) {
        pr_err("%s: CMD56 failed %d.\n", __func__, err);
        goto out_free;
    }
    //err = get_card_status(card, &status, 5);
    err = mmc_send_status(card, &status);
    if (err) {
        pr_err("%s: CMD13 failed, the status %d\n", __func__, status);
        goto out_free;
    }

    err = mmc_send_cxd_witharg_data(card, card->host, 56, 0x29, buf, 512);
    if (err) {
        pr_err("%s: CMD56 failed %d.\n", __func__, err);
        goto out_free;
    }
    //err = get_card_status(card, &status, 5);
    err = mmc_send_status(card, &status);
    if (err) {
        pr_err("%s: CMD13 failed, the status %d\n", __func__, status);
        goto out_free;
    }

out_free:
    return err;
}

/*
 * user can add relevant spec control block
 * to hr.
 *
 * struct mmc_card *card, manufacture id, func
 *
 */
struct mcb_spec_t mmc_array_hr[] = {
    {NULL, CID_MANFID_HYNIX, get_hynix_hr},
    {NULL, CID_MANFID_MICRON, get_micron_hr},
    {NULL, CID_MANFID_SAMSUNG, get_ss_hr},
    {NULL, CID_MANFID_WC, get_wc_hr},
    {NULL, CID_MANFID_YMTC, get_ymtc_hr},
    {NULL, CID_MANFID_HOSIN, get_hosin_hr},
};

static ssize_t hr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct mmc_card *card = mv_card;
    ssize_t n = 0;
    u8 *hr = NULL;
    int err, i;

    struct mcb_spec_t *mmc_cb;

    pr_mem_err("manfid = 0x%02x\n", card->cid.manfid);
    for (i = 0; i < (sizeof(mmc_array_hr) / sizeof(struct mcb_spec_t)); i++) {
        mmc_cb = &mmc_array_hr[i];
        if (card->cid.manfid == mmc_cb->manfid)
            break;
    }

    if (i == sizeof(mmc_array_hr) / sizeof(struct mcb_spec_t)) {
        n +=  sprintf(buf + n, "NOT SUPPORTED\n");
        return n;
    }

    hr = kzalloc(512 * 3, GFP_KERNEL);
    if (!hr) {
        n += sprintf(buf + n, "malloc hr buf fail.");
        return n;
    }

    mmc_get_card(card);

#ifdef CONFIG_MTK_EMMC_HW_CQ
    if (mmc_card_cmdq(card)) {
        err = mmc_cmdq_halt_on_empty_queue(card->host);
        if (err) {
            pr_mem_err("%s: halt failed while doing err (%d)\n",
                mmc_hostname(card->host), err);
            goto out;
        }
    }
#endif

    /*run relevant rountion*/
    pr_mem_err("get_hr start\n");
    err = mmc_cb->get_hr(card, hr);
    pr_mem_err("get_hr done\n");
    if (err) {
        n += sprintf(buf + n, "fail to get hr.err:%d\n", err);
        goto out;
    }
#if 0
    for (i = 0; i < 512; i++) {
        pr_err("hr[%d~%d] = 0x%0x 0x%0x 0x%0x 0x%0x\n",
            i, i + 3, hr[i], hr[i + 1], hr[i + 2], hr[i + 3]);
    }
#endif
    for (i = 0; i < 512; i++)
        n += sprintf(buf + n, "%02x", hr[i]);
    n += sprintf(buf + n, "\n");

#ifdef CONFIG_MTK_EMMC_HW_CQ
    if (mmc_card_cmdq(card)) {
        if (mmc_cmdq_halt(card->host, false))
            pr_mem_err("%s: cmdq unhalt failed\n", mmc_hostname(card->host));
    }
#endif

out:
    mmc_put_card(card);
    kfree(hr);
    return n;
}
static DEVICE_ATTR_RO(hr);

static struct attribute *mmc_sysfs[] = {
    &dev_attr_hr.attr,
    NULL,
};

 const struct  attribute_group mmc_sysfs_group = {
    .name = "mmc_info",
    .attrs = mmc_sysfs,
    NULL,
};
