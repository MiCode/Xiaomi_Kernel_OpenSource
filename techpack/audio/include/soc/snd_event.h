/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _SND_EVENT_H_
#define _SND_EVENT_H_

enum {
	SND_EVENT_DOWN = 0,
	SND_EVENT_UP,
};

struct snd_event_clients;

struct snd_event_ops {
	int (*enable)(struct device *dev, void *data);
	void (*disable)(struct device *dev, void *data);
};

#ifdef CONFIG_SND_EVENT
int snd_event_client_register(struct device *dev,
			      const struct snd_event_ops *snd_ev_ops,
			      void *data);
int snd_event_client_deregister(struct device *dev);
int snd_event_master_register(struct device *dev,
			      const struct snd_event_ops *ops,
			      struct snd_event_clients *clients,
			      void *data);
int snd_event_master_deregister(struct device *dev);
int snd_event_notify(struct device *dev, unsigned int state);

void snd_event_mstr_add_client(struct snd_event_clients **snd_clients,
			    int (*compare)(struct device *, void *),
			    void *data);
static inline bool is_snd_event_fwk_enabled(void)
{
	return 1;
}
#else
static inline int snd_event_client_register(struct device *dev,
			      const struct snd_event_ops *snd_ev_ops,
			      void *data)
{
	return 0;
}
static inline int snd_event_client_deregister(struct device *dev)
{
	return 0;
}
static inline int snd_event_master_register(struct device *dev,
			      const struct snd_event_ops *ops,
			      struct snd_event_clients *clients,
			      void *data)
{
	return 0;
}
static inline int snd_event_master_deregister(struct device *dev)
{
	return 0;
}
static inline int snd_event_notify(struct device *dev, unsigned int state)
{
	return 0;
}

static inline void snd_event_mstr_add_client(struct snd_event_clients **snd_clients,
			    int (*compare)(struct device *, void *),
			    void *data)
{
	return;
}
static inline bool is_snd_event_fwk_enabled(void)
{
	return 0;
}

#endif /* CONFIG_SND_EVENT */
#endif /* _SND_EVENT_H_ */
