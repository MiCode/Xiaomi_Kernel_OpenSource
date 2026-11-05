#ifndef	_rsmc_stdlib_s_h_
#define	_rsmc_stdlib_s_h_
#include <linux/init.h>

extern int memcpy_s(void *dest, size_t destsz, const void *src, size_t n);
extern int memset_s(void *dest, size_t destsz, int c, size_t n);

#define EOK				0	/* Success */
#define EERROR			-1	/* Error generic */
#define EBADARG			-2	/* Bad Argument */
#define EBADOPTION			-3	/* Bad option */
#define ENOTUP			-4	/* Not up */
#define ENOTDOWN			-5	/* Not down */
#define ENOTAP			-6	/* Not AP */
#define ENOTSTA			-7	/* Not STA  */
#define EBADKEYIDX			-8	/* BAD Key Index */
#define ERADIOOFF 			-9	/* Radio Off */
#define ENOTBANDLOCKED		-10	/* Not  band locked */
#define ENOCLK			-11	/* No Clock */
#define EBADRATESET			-12	/* BAD Rate valueset */
#define EBADBAND			-13	/* BAD Band */
#define EBUFTOOSHORT		-14	/* Buffer too short */
#define EBUFTOOLONG			-15	/* Buffer too long */
#define ENOTASSOCIATED		-17	/* Not Associated */
#define EBADSSIDLEN			-18	/* Bad SSID len */
#define EOUTOFRANGECHAN		-19	/* Out of Range Channel */
#define EBADCHAN			-20	/* Bad Channel */
#define EBADADDR			-21	/* Bad Address */
#define ENORESOURCE			-22	/* Not Enough Resources */
#define EUNSUPPORTED		-23	/* Unsupported */
#define EBADLEN			-24	/* Bad length */
#define ENOTREADY			-25	/* Not Ready */
#define EEPERM			-26	/* Not Permitted */
#define EASSOCIATED			-28	/* Associated */
#define ENOTFOUND			-30	/* Not Found */
#define EWME_NOT_ENABLED		-31	/* WME Not Enabled */
#define ETSPEC_NOTFOUND		-32	/* TSPEC Not Found */
#define EACM_NOTSUPPORTED		-33	/* ACM Not Supported */
#define ENOT_WME_ASSOCIATION	-34	/* Not WME Association */
#define ESDIO_ERROR			-35	/* SDIO Bus Error */
#define EDONGLE_DOWN		-36	/* Dongle Not Accessible */
#define EVERSION			-37 	/* Incorrect version */
#define ETXFAIL			-38 	/* TX failure */
#define ERXFAIL			-39	/* RX failure */
#define ENODEVICE			-40 	/* Device not present */
#define ENMODE_DISABLED		-41 	/* NMODE disabled */
#define EHOFFLOAD_RESIDENT		-42	/* offload resident */
#define ESCANREJECT			-43 	/* reject scan request */
#define EUSAGE_ERROR                -44     /* WLCMD usage error */
#define EIOCTL_ERROR                -45     /* WLCMD ioctl error */
#define ESERIAL_PORT_ERR            -46     /* RWL serial port error */
#define EDISABLED			-47     /* Disabled in this build */
#define EDECERR				-48		/* Decrypt error */
#define EENCERR				-49		/* Encrypt error */
#define EMICERR				-50		/* Integrity/MIC error */
#define EREPLAY				-51		/* Replay */
#define EIE_NOTFOUND		-52		/* IE not found */
#define EDATA_NOTFOUND		-53		/* Complete data not found in buffer */
#define ENOT_GC			-54     /* expecting a group client */
#define EPRS_REQ_FAILED		-55     /* GC presence req failed to sent */
#define ENO_P2P_SE			-56      /* Could not find P2P-Subelement */
#define ENOA_PND			-57      /* NoA pending, CB shuld be NULL */
#define EFRAG_Q_FAILED		-58      /* queueing 80211 frag failedi */
#define EGET_AF_FAILED		-59      /* Get p2p AF pkt failed */
#define EMSCH_NOTREADY		-60		/* scheduler not ready */
#define EIOV_LAST_CMD		-61		/* last batched iov sub-command */
#define EMINIPMU_CAL_FAIL		-62		/* MiniPMU cal failed */
#define ERCAL_FAIL			-63		/* Rcal failed */
#define ELPF_RCCAL_FAIL		-64		/* RCCAL failed */
#define EDACBUF_RCCAL_FAIL		-65		/* RCCAL failed */
#define EVCOCAL_FAIL		-66		/* VCOCAL failed */
#define EBANDLOCKED			-67	/* interface is restricted to a band */
#define EDNGL_DEVRESET		-68	/* dongle re-attach during DEVRESET */

#endif