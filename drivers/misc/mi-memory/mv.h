#ifndef __MV_H__
#define __MV_H__
#include <linux/mmc/card.h>
#define MV_NAME                 "mv"

struct mv_emmc
{
    u16 vendor_id;
    u64 density;
    char product_name[8];
    char product_revision[MMC_FIRMWARE_LEN];
};
#endif
