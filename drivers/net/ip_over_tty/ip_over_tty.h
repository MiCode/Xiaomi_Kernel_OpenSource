#ifndef _IP_OVER_TTY_H
#define _IP_OVER_TTY_H
#include <linux/netdevice.h>
#include <linux/tty.h>

/* used for unreliable data link layer */
/* #define IPOTTY_NET_HEADER */

/* definitions */
#define IPOTTY_TX_TIMEOUT	(HZ)	/* in jiffies */
#define IPOTTY_SKB_HIGH		100	/* tx high watermark */
#define IPOTTY_SKB_LOW		80	/* tx low watermark */

#define IPOTTY_MTU 		(1400)
#define IPOTTY_TX_BACKOFF 	(50)	/* in ms */

/* global */
extern int debug;
extern int tx_backoff;
extern int dumpsize;

#ifdef IPOTTY_NET_HEADER
#define IPOTTY_HLEN	8
struct net_header {
	unsigned char 	marker[2];
	__be16		protocol;
	__be16		size;
	unsigned char 	reserved;
	unsigned char 	checksum; /* must be the last byte */
};
enum net_state {HEADER_LOOKING, HEADER_VALIDATING, PACKET_LOOKING, PACKET_DONE};
#endif

struct ipotty_priv {
	/* common */

	/* networking */
	struct net_device 	*ndev;
	struct sk_buff_head 	txhead;
	struct sk_buff_head 	rxhead;
	struct delayed_work	txwork;
	struct delayed_work	rxwork;

#define IPOTTY_SENDING		(1 << 0)
#define IPOTTY_RECEIVING	(1 << 1)
#define IPOTTY_SHUTDOWN		(1 << 2)
	unsigned long state;

#ifdef IPOTTY_NET_HEADER
	enum net_state 		rxstate;
	struct sk_buff 		*rxskb;
	struct net_header 	rxhdr;
	int 			rxhdr_len;
#endif

	/* line discipline */
	struct tty_struct 	*tty;
	spinlock_t 		rxlock; /* receive lock */
};

/* networking stuff */
struct ipotty_priv *ipotty_net_create_interface(char *intf_name);
void ipotty_net_destroy_interface(struct ipotty_priv *);
int
ipotty_net_receive(struct ipotty_priv *, const unsigned char *data, int count);
void ipotty_net_wake_transmit(struct ipotty_priv *priv);
int ipotty_net_init(void);
void ipotty_net_remove(void);

/* line discipline stuff */
int ip_over_tty_ldisc_init(void);
void ip_over_tty_ldisc_remove(void);

/* debugging stuffs */
void ipotty_print(int level, char *who, const char *fmt, ...);
#define ipotty_net_print(_level, _ndev, fmt, args...) 			\
	do {								\
		struct net_device *__ndev = _ndev;			\
		BUG_ON(__ndev == NULL);					\
		ipotty_print(_level, __ndev->name, fmt, ## args);	\
	} while (0);

#define ipotty_ldisc_print(_level, _tty, fmt, args...) 			\
	do {								\
		struct tty_struct *__tty = _tty;			\
		char who[16];						\
		BUG_ON(__tty == NULL);					\
		sprintf(who, "%s%d", __tty->driver->name, __tty->index);\
		ipotty_print(_level, who, fmt, ## args);		\
	} while (0);

#define DRIVER_NAME "ip_over_tty: "
#define LEVEL_VERBOSE	5
#define LEVEL_DEBUG	4
#define LEVEL_INFO	3
#define LEVEL_WARNING	2
#define LEVEL_ERROR	1

/* use when have neither ndev nor tty */
#ifdef VERBOSE_DEBUG
#define VDBG(fmt, args...) if (debug >= LEVEL_VERBOSE) \
	pr_debug(DRIVER_NAME fmt, ## args);
#else
#define VDBG(fmt, args...)
#endif

#ifdef DEBUG
#define DBG(fmt, args...) if (debug >= LEVEL_DEBUG) \
	pr_debug(DRIVER_NAME fmt, ## args);
#else
#define DBG(fmt, args...)
#endif

#define INFO(fmt, args...) if (debug >= LEVEL_INFO) \
	pr_info(DRIVER_NAME fmt, ## args);

#define WARNING(fmt, args...) if (debug >= LEVEL_WARNING) \
	pr_warn(DRIVER_NAME fmt, ## args);

#define ERROR(fmt, args...) if (debug >= LEVEL_ERROR) \
	pr_err(DRIVER_NAME fmt, ## args);

/* use when have ndev */
#ifdef VERBOSE_DEBUG
#define n_vdbg(_ndev, format, args...) if (debug >= LEVEL_VERBOSE) \
	ipotty_net_print(LEVEL_VERBOSE, _ndev, format, ## args);
#else
#define n_vdbg(_ndev, format, args...)
#endif

#define n_dbg(_ndev, format, args...) if (debug >= LEVEL_DEBUG) \
	ipotty_net_print(LEVEL_DEBUG, _ndev, format, ## args);

#define n_info(_ndev, fmt, args...) if (debug >= LEVEL_INFO) \
	ipotty_net_print(LEVEL_INFO, _ndev, fmt, ## args);

#define n_warn(_ndev, fmt, args...) if (debug >= LEVEL_WARNING) \
	ipotty_net_print(LEVEL_WARNING, _ndev, fmt, ## args);

#define n_err(_ndev, fmt, args...) if (debug >= LEVEL_ERROR) \
	ipotty_net_print(LEVEL_ERROR, _ndev, fmt, ## args);

/* use when have tty */
#ifdef VERBOSE_DEBUG
#define t_vdbg(_tty, fmt, args...) if (debug >= LEVEL_VERBOSE) \
	ipotty_ldisc_print(LEVEL_VERBOSE, _tty, fmt, ## args);
#else
#define t_vdbg(_tty, fmt, args...)
#endif

#ifdef DEBUG
#define t_dbg(_tty, fmt, args...) if (debug >= LEVEL_DEBUG) \
	ipotty_ldisc_print(LEVEL_DEBUG, _tty, fmt, ## args);
#else
#define t_dbg(_tty, fmt, args...)
#endif

#define t_info(_tty, fmt, args...) if (debug >= LEVEL_INFO) \
	ipotty_ldisc_print(LEVEL_INFO, _tty, fmt, ## args);

#define t_warn(_tty, fmt, args...) if (debug >= LEVEL_WARNING) \
	ipotty_ldisc_print(LEVEL_WARNING, _tty, fmt, ## args);

#define t_err(_tty, fmt, args...) if (debug >= LEVEL_ERROR) \
	ipotty_ldisc_print(LEVEL_ERROR, _tty, fmt, ## args);

#endif /*_IP_OVER_TTY_H*/
