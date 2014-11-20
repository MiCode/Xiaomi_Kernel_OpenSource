/*
 *  Common defnition for for HD Audio bus struct used in ASoC and ALSA driver
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 */

#ifndef _SOUND_HDA_BUS_H_
#define _SOUND_HDA_BUS_H_

/*FIXME move to common header file
#define HDA_RW_NO_RESPONSE_FALLBACK (1 << 0)
*/
struct hda_bus;

/* template to pass to the bus constructor */
struct hda_bus_template {
	void *private_data;
	struct pci_dev *pci;
	const char *modelname;
	int *power_save;
};

/*
 * codec bus
 *
 * each controller needs to creata a hda_bus to assign the accessor.
 * A hda_bus contains several codecs in the list codec_list.
 */
struct hda_bus {
	void *private_data;
	struct pci_dev *pci;
	const char *modelname;
	int *power_save;

	struct mutex cmd_mutex;
	struct mutex prepare_mutex;

	 /* unsolicited event queue */
	struct hda_bus_unsolicited *unsol;
	char workq_name[16];
	struct workqueue_struct *workq; /* common workqueue for codecs */

	/* misc op flags */
	unsigned int needs_damn_long_delay:1;
	unsigned int allow_bus_reset:1; /* allow bus reset at fatal error */
	unsigned int sync_write:1;      /* sync after verb write */
	/* status for codec/controller */
	unsigned int shutdown:1;        /* being unloaded */
	unsigned int rirb_error:1;      /* error in codec communication */
	unsigned int response_reset:1;  /* controller was reset */
	unsigned int in_reset:1;        /* during reset operation */
	unsigned int no_response_fallback:1; /* don't fallback at RIRB error */

};

/*
 * unsolicited event handler
 */

#define HDA_UNSOL_QUEUE_SIZE    64

struct hda_bus_unsolicited {
	/* ring buffer */
	u32 queue[HDA_UNSOL_QUEUE_SIZE * 2];
	unsigned int rp, wp;

	/* workqueue */
	struct work_struct work;
	struct hda_bus *bus;
};

#endif /* _SOUND_HDA_BUS_H_ */
