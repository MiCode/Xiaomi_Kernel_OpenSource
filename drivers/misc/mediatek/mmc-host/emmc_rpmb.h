


#ifndef _EMMC_RPMB_H
#define _EMMC_RPMB_H

#include <linux/mmc/ioctl.h>
#include <linux/mmc/card.h>


/************************************************************************
 *
 * RPMB IOCTL interface.
 *
 ***********************************************************************/
#define RPMB_IOCTL_PROGRAM_KEY  1
#define RPMB_IOCTL_WRITE_DATA   3
#define RPMB_IOCTL_READ_DATA    4

struct rpmb_ioc_param {
    unsigned char *key;
    unsigned char *data;
    unsigned int  data_len;
    unsigned short addr;
    unsigned char *hmac;
    unsigned int hmac_len;
};
/***********************************************************************/


#define RPMB_SZ_STUFF 196
#define RPMB_SZ_MAC   32
#define RPMB_SZ_DATA  256
#define RPMB_SZ_NONCE 16

struct s_rpmb {	
    unsigned char stuff[RPMB_SZ_STUFF];	
    unsigned char mac[RPMB_SZ_MAC];	
    unsigned char data[RPMB_SZ_DATA];	
    unsigned char nonce[RPMB_SZ_NONCE];	
    unsigned int write_counter;	
    unsigned short address;	
    unsigned short block_count;	
    unsigned short result;	
    unsigned short request;
};

enum {
    RPMB_SUCCESS = 0,
    RPMB_HMAC_ERROR,
    RPMB_RESULT_ERROR,
    RPMB_WC_ERROR,
    RPMB_NONCE_ERROR,
    RPMB_ALLOC_ERROR,
    RPMB_TRANSFER_NOT_COMPLETE,
};

#define RPMB_PROGRAM_KEY       1       /* Program RPMB Authentication Key */
#define RPMB_GET_WRITE_COUNTER 2       /* Read RPMB write counter */
#define RPMB_WRITE_DATA		   3	   /* Write data to RPMB partition */
#define RPMB_READ_DATA         4       /* Read data from RPMB partition */
#define RPMB_RESULT_READ       5       /* Read result request */
#define RPMB_REQ               1       /* RPMB request mark */
#define RPMB_RESP              (1 << 1)/* RPMB response mark */
#define RPMB_AVALIABLE_SECTORS 8       /* 4K page size */

#define RPMB_TYPE_BEG          510
#define RPMB_RES_BEG           508
#define RPMB_BLKS_BEG          506
#define RPMB_ADDR_BEG          504
#define RPMB_WCOUNTER_BEG      500

#define RPMB_NONCE_BEG         484
#define RPMB_DATA_BEG          228
#define RPMB_MAC_BEG           196

struct emmc_rpmb_req {
	__u16 type;                     /* RPMB request type */
	__u16 *result;                  /* response or request result */
	__u16 blk_cnt;                  /* Number of blocks(half sector 256B) */
	__u16 addr;                     /* data address */
	__u32 *wc;                      /* write counter */
	__u8 *nonce;                    /* Ramdom number */
	__u8 *data;                     /* Buffer of the user data */
	__u8 *mac;                      /* Message Authentication Code */
    __u8 *data_frame;
};


int mmc_rpmb_set_key(struct mmc_card *card, void *key);
int mmc_rpmb_read(struct mmc_card *card, u8 *buf, u16 blk, u16 cnt, void *key);
int mmc_rpmb_write(struct mmc_card *card, u8 *buf, u16 blk, u16 cnt, void *key);

extern void emmc_rpmb_set_host(void *mmc_host);


#endif
