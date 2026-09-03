/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _IMSBR_SIPC_H
#define _IMSBR_SIPC_H

#include <linux/completion.h>
#include <linux/sipc.h>

#define IMSBR_DATA_BLKSZ	(1500 + 8 + 16) /* MTU + headsize + reserve */
#define IMSBR_DATA_BLKNUM	64

#define IMSBR_CTRL_BLKSZ	128
#define IMSBR_CTRL_BLKNUM	64

#define IMSBR_DESC_NAMESZ	16

struct imsbr_sipc {
	u8		dst;
	u8		channel;
	u16		hdrlen;
	u32		blksize;
	u32		blknum;
	atomic_t	peer_ready;
	struct completion peer_comp;
	char desc[IMSBR_DESC_NAMESZ];
	int (*pre_hook)(struct imsbr_sipc *sipc);
	void (*process)(struct imsbr_sipc *sipc, struct sblock *blk, bool freeit);
	struct task_struct *task;
	struct work_struct initwork;
};

extern struct imsbr_sipc imsbr_data;
extern struct imsbr_sipc imsbr_ctrl;

#ifdef CONFIG_SPRD_IMS_BRIDGE_TEST

struct call_internal_function {
	void (*sipc_handler) (int event, void *data);
	int (*sipc_kthread) (void *arg);
	int (*sipc_create) (struct imsbr_sipc *sipc);
	void (*sipc_destroy) (struct imsbr_sipc *sipc);
};

void call_imsbr_sipc_function(struct call_internal_function *cif);

#endif

int imsbr_notify_ltevideo_apsk(void);
int imsbr_build_cmd(const char *cmd, struct sblock *blk,
		    void *payload, int paylen);

int imsbr_sblock_receive(struct imsbr_sipc *sipc, struct sblock *blk);
int imsbr_sblock_get(struct imsbr_sipc *sipc, struct sblock *blk, int size);
int imsbr_sblock_send(struct imsbr_sipc *sipc, struct sblock *blk, int size);
void imsbr_sblock_release(struct imsbr_sipc *sipc, struct sblock *blk);
void imsbr_sblock_put(struct imsbr_sipc *sipc, struct sblock *blk);

int imsbr_sipc_init(void);
void imsbr_sipc_exit(void);

void imsbr_transit(struct imsbr_sipc *sipc, struct sblock *blk,
		   bool freeit);

#endif
