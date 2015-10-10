/*
** =========================================================================
** File:
**     tspdrv.c
**
** Description: 
**     TouchSense Kernel Module main entry-point.
**
** Portions Copyright (c) 2008-2014 Immersion Corporation. All Rights Reserved.
** Copyright (C) 2015 XiaoMi, Inc.
**
** This file contains Original Code and/or Modifications of Original Code
** as defined in and that are subject to the GNU Public License v2 -
** (the 'License'). You may not use this file except in compliance with the
** License. You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
** TouchSenseSales@immersion.com.
**
** The Original Code and all software distributed under the License are
** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
** the License for the specific language governing rights and limitations
** under the License.
** =========================================================================
*/

#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <tspdrv.h>

static int g_nTimerPeriodMs = 5; /* 5ms timer by default. This variable could be used by the SPI.*/

#ifdef VIBE_RUNTIME_RECORD
/* Flag indicating whether runtime recorder on or off */
static atomic_t g_bRuntimeRecord;
#endif

#include <ImmVibeSPI.c>
#if (defined(VIBE_DEBUG) && defined(VIBE_RECORD)) || defined(VIBE_RUNTIME_RECORD)
#include <tspdrvRecorder.c>
#endif

/* Device name and version information */
#define VERSION_STR " v5.0.36.6\n"                  /* DO NOT CHANGE - this is auto-generated */
#define VERSION_STR_LEN 16                          /* account extra space for future extra digits in version number */
static char g_szDeviceName[  (VIBE_MAX_DEVICE_NAME_LENGTH
                            + VERSION_STR_LEN)
                            * NUM_ACTUATORS];       /* initialized in init_module */
static size_t g_cchDeviceName;                      /* initialized in init_module */

/* Flag indicating whether the driver is in use */
static char g_bIsPlaying = false;

/* Flag indicating the debug level*/
static atomic_t g_nDebugLevel;


/* Buffer to store data sent to SPI */
#define MAX_SPI_BUFFER_SIZE (NUM_ACTUATORS * (VIBE_OUTPUT_SAMPLE_SIZE + SPI_HEADER_SIZE))

static char g_cWriteBuffer[MAX_SPI_BUFFER_SIZE];


#if ((LINUX_VERSION_CODE & 0xFFFF00) < KERNEL_VERSION(2,6,0))
#error Unsupported Kernel version
#endif

#ifndef HAVE_UNLOCKED_IOCTL
#define HAVE_UNLOCKED_IOCTL 0
#endif

#ifdef IMPLEMENT_AS_CHAR_DRIVER
static int g_nMajor = 0;
#endif



/* Needs to be included after the global variables because they use them */
#include <tspdrvOutputDataHandler.c>
#ifdef CONFIG_HIGH_RES_TIMERS
    #include <VibeOSKernelLinuxHRTime.c>
#else
    #include <VibeOSKernelLinuxTime.c>
#endif

asmlinkage void _DbgOut(int level, const char *fmt,...)
{
    static char printk_buf[MAX_DEBUG_BUFFER_LENGTH];
    static char prefix[6][4] =
        {" * ", " ! ", " ? ", " I ", " V", " O "};

    int nDbgLevel = atomic_read(&g_nDebugLevel);

    if (0 <= level && level <= nDbgLevel) {
        va_list args;
        int ret;
        size_t size = sizeof(printk_buf);

        va_start(args, fmt);

        ret = scnprintf(printk_buf, size, KERN_EMERG "%s:%s %s",
             MODULE_NAME, prefix[level], fmt);
        if (ret < size)
            vprintk(printk_buf, args);

        va_end(args);
    }
}

asmlinkage static void _DbgOutV(int level, const char *fmt,va_list args)
{
    static char printk_buf[MAX_DEBUG_BUFFER_LENGTH];
    static char prefix[6][4] =
        {" * ", " ! ", " ? ", " I ", " V", " O "};

    int ret;
    size_t size = sizeof(printk_buf);
    ret = scnprintf(printk_buf, size, KERN_EMERG "%s:%s %s",
         MODULE_NAME, prefix[level], fmt);
    if (ret < size)
        vprintk(printk_buf, args);

}

asmlinkage void _DbgOutTemp(const char *fmt,...)
{
    int nDbgLevel = atomic_read(&g_nDebugLevel);
    if (DBL_TEMP <= nDbgLevel)
    {
        va_list args;
        va_start(args, fmt);
        _DbgOutV(DBL_TEMP, fmt, args);
        va_end(args);
    }
}

asmlinkage void _DbgOutFatal(const char *fmt,...)
{
    int nDbgLevel = atomic_read(&g_nDebugLevel);
    if (DBL_FATAL <= nDbgLevel)
    {
        va_list args;
        va_start(args, fmt);
        _DbgOutV(DBL_FATAL, fmt, args);
        va_end(args);
    }
}

asmlinkage void _DbgOutErr(const char *fmt,...)
{
    int nDbgLevel = atomic_read(&g_nDebugLevel);
    if (DBL_ERROR <= nDbgLevel)
    {
        va_list args;
        va_start(args, fmt);
        _DbgOutV(DBL_ERROR, fmt, args);
        va_end(args);
    }
}

asmlinkage void _DbgOutWarn(const char *fmt,...)
{
    int nDbgLevel = atomic_read(&g_nDebugLevel);
    if (DBL_WARNING <= nDbgLevel)
    {
        va_list args;
        va_start(args, fmt);
        _DbgOutV(DBL_WARNING, fmt, args);
        va_end(args);
    }
}

asmlinkage void _DbgOutInfo(const char *fmt,...)
{
    int nDbgLevel = atomic_read(&g_nDebugLevel);
    if (DBL_INFO <= nDbgLevel)
    {
        va_list args;
        va_start(args, fmt);
        _DbgOutV(DBL_INFO, fmt, args);
        va_end(args);
    }
}

asmlinkage void _DbgOutVerbose(const char *fmt,...)
{
    int nDbgLevel = atomic_read(&g_nDebugLevel);
    if (DBL_VERBOSE <= nDbgLevel)
    {
        va_list args;
        va_start(args, fmt);
        _DbgOutV(DBL_VERBOSE, fmt, args);
        va_end(args);
    }
}

asmlinkage void _DbgOutOverkill(const char *fmt,...)
{
    int nDbgLevel = atomic_read(&g_nDebugLevel);
    if (DBL_OVERKILL <= nDbgLevel)
    {
        va_list args;
        va_start(args, fmt);
        _DbgOutV(DBL_OVERKILL, fmt, args);
        va_end(args);
    }
}



/* File IO */
static int open(struct inode *inode, struct file *file);
static int release(struct inode *inode, struct file *file);
static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos);
static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *ppos);
#if HAVE_UNLOCKED_IOCTL
static long unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#else
static int ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
#endif
static struct file_operations fops =
{
    .owner =            THIS_MODULE,
    .read =             read,
    .write =            write,
#if HAVE_UNLOCKED_IOCTL
    .unlocked_ioctl =   unlocked_ioctl,
#if HAVE_COMPAT_IOCTL
    .compat_ioctl   =   unlocked_ioctl,
#endif
#else
    .ioctl =            ioctl,
#endif
    .open =             open,
    .release =          release,
    .llseek =           default_llseek    /* using default implementation as declared in linux/fs.h */
};

#ifndef IMPLEMENT_AS_CHAR_DRIVER
static struct miscdevice miscdev = 
{
	.minor =    MISC_DYNAMIC_MINOR,
	.name =     MODULE_NAME,
	.fops =     &fops
};
#endif

static int suspend(struct platform_device *pdev, pm_message_t state);
static int resume(struct platform_device *pdev);
static struct platform_driver platdrv =
{
    .suspend =  suspend,
    .resume =   resume,
    .driver =
    {
        .name = MODULE_NAME,
    },
};

static void platform_release(struct device *dev);
static struct platform_device platdev =
{
	.name =     MODULE_NAME,
	.id =       -1,                     /* means that there is only one device */
	.dev =
    {
		.platform_data = NULL,
		.release = platform_release,    /* a warning is thrown during rmmod if this is absent */
	},
};

/* Module info */
MODULE_AUTHOR("Immersion Corporation");
MODULE_DESCRIPTION("TouchSense Kernel Module");
MODULE_LICENSE("GPL v2");

static int __init tspdrv_init(void)
{
    int nRet, i;   /* initialized below */

    atomic_set(&g_nDebugLevel, DBL_ERROR);
#ifdef VIBE_RUNTIME_RECORD
    atomic_set(&g_bRuntimeRecord, 0);
    DbgOutErr(("*** tspdrv: runtime recorder feature is ON for debugging which should be OFF in release version.\n"
                        "*** tspdrv: please turn off the feature by removing VIBE_RUNTIME_RECODE macro.\n"));
#endif
    DbgOutInfo(("tspdrv: init_module.\n"));

#ifdef IMPLEMENT_AS_CHAR_DRIVER
    g_nMajor = register_chrdev(0, MODULE_NAME, &fops);
    if (g_nMajor < 0)
    {
        DbgOutErr(("tspdrv: can't get major number.\n"));
        return g_nMajor;
    }
#else
    nRet = misc_register(&miscdev);
	if (nRet)
    {
        DbgOutErr(("tspdrv: misc_register failed.\n"));
		return nRet;
	}
#endif

	nRet = platform_device_register(&platdev);
	if (nRet)
    {
        DbgOutErr(("tspdrv: platform_device_register failed.\n"));
    }

	nRet = platform_driver_register(&platdrv);
	if (nRet)
    {
        DbgOutErr(("tspdrv: platform_driver_register failed.\n"));
    }

    DbgRecorderInit(());

    ImmVibeSPI_ForceOut_Initialize();
    VibeOSKernelLinuxInitTimer();
    ResetOutputData();

    /* Get and concatenate device name and initialize data buffer */
    g_cchDeviceName = 0;
    for (i=0; i<NUM_ACTUATORS; i++)
    {
        char *szName = g_szDeviceName + g_cchDeviceName;
        ImmVibeSPI_Device_GetName(i, szName, VIBE_MAX_DEVICE_NAME_LENGTH);

        /* Append version information and get buffer length */
        strcat(szName, VERSION_STR);
        g_cchDeviceName += strlen(szName);

    }

    return 0;
}

static void __exit tspdrv_exit(void)
{
    DbgOutInfo(("tspdrv: cleanup_module.\n"));

    DbgRecorderTerminate(());

    VibeOSKernelLinuxTerminateTimer();
    ImmVibeSPI_ForceOut_Terminate();

	platform_driver_unregister(&platdrv);
	platform_device_unregister(&platdev);

#ifdef IMPLEMENT_AS_CHAR_DRIVER
    unregister_chrdev(g_nMajor, MODULE_NAME);
#else
    misc_deregister(&miscdev);
#endif
}

static int open(struct inode *inode, struct file *file)
{
    DbgOutInfo(("tspdrv: open.\n"));

    if (!try_module_get(THIS_MODULE)) return -ENODEV;

    return 0;
}

static int release(struct inode *inode, struct file *file)
{
    DbgOutInfo(("tspdrv: release.\n"));

    /*
    ** Reset force and stop timer when the driver is closed, to make sure
    ** no dangling semaphore remains in the system, especially when the
    ** driver is run outside of immvibed for testing purposes.
    */
    VibeOSKernelLinuxStopTimer();

    /*
    ** Clear the variable used to store the magic number to prevent
    ** unauthorized caller to write data. TouchSense service is the only
    ** valid caller.
    */
    file->private_data = (void*)NULL;

    module_put(THIS_MODULE);

    return 0;
}

static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    const size_t nBufSize = (g_cchDeviceName > (size_t)(*ppos)) ? min(count, g_cchDeviceName - (size_t)(*ppos)) : 0;

    /* End of buffer, exit */
    if (0 == nBufSize) return 0;

    if (0 != copy_to_user(buf, g_szDeviceName + (*ppos), nBufSize))
    {
        /* Failed to copy all the data, exit */
        DbgOutErr(("tspdrv: copy_to_user failed.\n"));
        return 0;
    }

    /* Update file position and return copied buffer size */
    *ppos += nBufSize;
    return nBufSize;
}

static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
    *ppos = 0;  /* file position not used, always set to 0 */

    /*
    ** Prevent unauthorized caller to write data.
    ** TouchSense service is the only valid caller.
    */
    if (file->private_data != (void*)TSPDRV_MAGIC_NUMBER)
    {
        DbgOutErr(("tspdrv: unauthorized write.\n"));
        return -EACCES;
    }

    /*
    ** Ignore packets that have size smaller than SPI_HEADER_SIZE or bigger than MAX_SPI_BUFFER_SIZE.
    ** Please note that the daemon may send an empty buffer (count == SPI_HEADER_SIZE)
    ** during quiet time between effects while playing a Timeline effect in order to maintain
    ** correct timing: if "count" is equal to SPI_HEADER_SIZE, the call to VibeOSKernelLinuxStartTimer()
    ** will just wait for the next timer tick.
    */
    if ((count < SPI_HEADER_SIZE) || (count > MAX_SPI_BUFFER_SIZE))
    {
        DbgOutErr(("tspdrv: invalid buffer size.\n"));
        return -EINVAL;
    }

    /* Copy immediately the input buffer */
    if (0 != copy_from_user(g_cWriteBuffer, buf, count))
    {
        /* Failed to copy all the data, exit */
        DbgOutErr(("tspdrv: copy_from_user failed.\n"));
        return -EIO;
    }

    /* Extract force output samples and save them in an internal buffer */
    if (!SaveOutputData(g_cWriteBuffer, count))
    {
        DbgOutErr(("tspdrv: SaveOutputData failed.\n"));
        return -EIO;
    }

    /* Start the timer after receiving new output force */
    g_bIsPlaying = true;

    VibeOSKernelLinuxStartTimer();

    return count;
}

#if HAVE_UNLOCKED_IOCTL
static long unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
#else
static int ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
#endif
{
    switch (cmd)
    {
        case TSPDRV_SET_MAGIC_NUMBER:
            file->private_data = (void*)TSPDRV_MAGIC_NUMBER;
            break;

        case TSPDRV_ENABLE_AMP:
            ImmVibeSPI_ForceOut_AmpEnable(arg);
#ifdef VIBE_RUNTIME_RECORD
            if (atomic_read(&g_bRuntimeRecord)) {
                DbgRecord((arg,";------- TSPDRV_ENABLE_AMP ---------\n"));
            }
#else
            DbgRecorderReset((arg));
            DbgRecord((arg,";------- TSPDRV_ENABLE_AMP ---------\n"));
#endif
            break;

        case TSPDRV_DISABLE_AMP:
            ImmVibeSPI_ForceOut_AmpDisable(arg);
#ifdef VIBE_RUNTIME_RECORD
            if (atomic_read(&g_bRuntimeRecord)) {
                DbgRecord((arg,";------- TSPDRV_DISABLE_AMP ---------\n"));
            }
#endif
            break;

        case TSPDRV_GET_NUM_ACTUATORS:
            return NUM_ACTUATORS;

#ifdef IMMVIBESPI_MULTIPARAM_SUPPORT
        case TSPDRV_GET_PARAM_FILE_ID:
            return ImmVibeSPI_Device_GetParamFileId();
#endif

        case TSPDRV_SET_DBG_LEVEL:
            {
                long nDbgLevel;
                if (0 != copy_from_user((void *)&nDbgLevel, (const void __user *)arg, sizeof(long))) {
                    /* Error copying the data */
                    DbgOutErr(("copy_from_user failed to copy debug level data.\n"));
                    return -1;
                }

                if (DBL_TEMP <= nDbgLevel &&  nDbgLevel <= DBL_OVERKILL) {
                    atomic_set(&g_nDebugLevel, nDbgLevel);
                } else {
                    DbgOutErr(("Invalid debug level requested, ignored."));
                }

                break;
            }

        case TSPDRV_GET_DBG_LEVEL:
            return atomic_read(&g_nDebugLevel);

#ifdef VIBE_RUNTIME_RECORD
        case TSPDRV_SET_RUNTIME_RECORD_FLAG:
            {
                long nRecordFlag;
                if (0 != copy_from_user((void *)&nRecordFlag, (const void __user *)arg, sizeof(long))) {
                    /* Error copying the data */
                    DbgOutErr(("copy_from_user failed to copy runtime record flag.\n"));
                    return -1;
                }

                atomic_set(&g_bRuntimeRecord, nRecordFlag);
                if (nRecordFlag) {
                    int i;
                    for (i=0; i<NUM_ACTUATORS; i++) {
                        DbgRecorderReset((i));
                    }
                }
                break;
            }
        case TSPDRV_GET_RUNTIME_RECORD_FLAG:
            return atomic_read(&g_bRuntimeRecord);
        case TSPDRV_SET_RUNTIME_RECORD_BUF_SIZE:
            {
                long nRecorderBufSize;
                if (0 != copy_from_user((void *)&nRecorderBufSize, (const void __user *)arg, sizeof(long))) {
                    /* Error copying the data */
                    DbgOutErr(("copy_from_user failed to copy recorder buffer size.\n"));
                    return -1;
                }

                if (0 == DbgSetRecordBufferSize(nRecorderBufSize)) {
                    DbgOutErr(("DbgSetRecordBufferSize failed.\n"));
                    return -1;
                }
                break;
            }
        case TSPDRV_GET_RUNTIME_RECORD_BUF_SIZE:
            return DbgGetRecordBufferSize();
#endif

        case TSPDRV_SET_DEVICE_PARAMETER:
            {
                device_parameter deviceParam;

                if (0 != copy_from_user((void *)&deviceParam, (const void __user *)arg, sizeof(deviceParam)))
                {
                    /* Error copying the data */
                    DbgOutErr(("tspdrv: copy_from_user failed to copy kernel parameter data.\n"));
                    return -1;
                }

                switch (deviceParam.nDeviceParamID)
                {
                    case VIBE_KP_CFG_UPDATE_RATE_MS:
                        /* Update the timer period */
                        g_nTimerPeriodMs = deviceParam.nDeviceParamValue;



#ifdef CONFIG_HIGH_RES_TIMERS
                        /* For devices using high resolution timer we need to update the ktime period value */
                        g_ktTimerPeriod = ktime_set(0, g_nTimerPeriodMs * 1000000);
#endif
                        break;

                    case VIBE_KP_CFG_FREQUENCY_PARAM1:
                    case VIBE_KP_CFG_FREQUENCY_PARAM2:
                    case VIBE_KP_CFG_FREQUENCY_PARAM3:
                    case VIBE_KP_CFG_FREQUENCY_PARAM4:
                    case VIBE_KP_CFG_FREQUENCY_PARAM5:
                    case VIBE_KP_CFG_FREQUENCY_PARAM6:
                        if (0 > ImmVibeSPI_ForceOut_SetFrequency(deviceParam.nDeviceIndex, deviceParam.nDeviceParamID, deviceParam.nDeviceParamValue))
                        {
                            DbgOutErr(("tspdrv: cannot set device frequency parameter.\n"));
                            return -1;
                        }
                        break;
                }
            }
        }
    return 0;
}

static int suspend(struct platform_device *pdev, pm_message_t state)
{
    if (g_bIsPlaying)
    {
        DbgOutInfo(("tspdrv: can't suspend, still playing effects.\n"));
        return -EBUSY;
    }
    else
    {
        DbgOutInfo(("tspdrv: suspend.\n"));
        return 0;
    }
}

static int resume(struct platform_device *pdev)
{
    DbgOutErr(("tspdrv: resume.\n"));

	return 0;   /* can resume */
}

static void platform_release(struct device *dev)
{
    DbgOutErr(("tspdrv: platform_release.\n"));
}

module_init(tspdrv_init);
module_exit(tspdrv_exit);
