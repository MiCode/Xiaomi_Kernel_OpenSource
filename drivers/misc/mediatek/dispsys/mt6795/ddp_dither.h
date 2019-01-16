#ifndef __DDP_DITHER_H__
#define __DDP_DITHER_H__

typedef enum {
	DISP_DITHER0,
	DISP_DITHER1
} disp_dither_id_t;

void disp_dither_init(disp_dither_id_t id, unsigned int dither_bpp, void *cmdq);

#endif
