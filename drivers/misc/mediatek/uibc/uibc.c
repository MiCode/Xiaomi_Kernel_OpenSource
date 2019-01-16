#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/slab.h>

#include <linux/device.h>
#include <linux/errno.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/input/mt.h>
#include <linux/version.h>
#include <linux/slab.h>

#define UIBC_TAG	"UIBC:"
#define MAX_POINTERS 5

#define idVal(_x) (_x * 3 + 1)
#define xVal(_x) (_x * 3 + 2)
#define yVal(_x) (_x * 3 + 3)

extern unsigned int DISP_GetScreenHeight(void);
extern unsigned int DISP_GetScreenWidth(void);

static unsigned short uibc_keycode[256] = {
    KEY_RESERVED,
    BTN_LEFT,
    BTN_TOUCH,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_LINEFEED,
    KEY_PAGEDOWN,
    KEY_ENTER,
    KEY_LEFTSHIFT,
    KEY_LEFTCTRL,
    KEY_CANCEL,
    KEY_BACK,
    KEY_DELETE,
    KEY_HOME,
    KEY_VOLUMEDOWN,
    KEY_VOLUMEUP,
};

static unsigned short uibc_keycode_chars[256] = {
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_RESERVED,
    BTN_LEFT,
    BTN_TOUCH,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_LINEFEED,
    KEY_PAGEDOWN,
    KEY_ENTER,
    KEY_LEFTSHIFT,
    KEY_LEFTCTRL,
    KEY_CANCEL,
    KEY_BACK,
    KEY_DELETE,
};


#define UIBC_KBD_NAME  "uibc"
#define UIBC_KEY_PRESS 1
#define UIBC_KEY_RELEASE 0
#define UIBC_KEY_RESERVE	2
#define UIBC_POINTER_X	3
#define UIBC_POINTER_Y	4
#define UIBC_KEYBOARD	5
#define UIBC_MOUSE	6
#define UIBC_TOUCH_DOWN		7
#define UIBC_TOUCH_UP		8
#define UIBC_TOUCH_MOVE		9
#define UIBC_KEYBOARD_MIRACAST		10

#define UIBC_MOUSE_INDEX	0
#define UIBC_GAMEPAD_INDEX	151


static struct input_dev *uibc_input_dev;

struct uibckeyboard {
    struct input_dev *input;
    unsigned short keymap[ARRAY_SIZE(uibc_keycode)];
};

struct uibckeyboard *uibckbd;
int uibc_registered = 0;

#ifdef CONFIG_COMPAT
static long uibc_compat_kbd_dev_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg) {
    long ret = 0;

    if (!pfile->f_op || !pfile->f_op->unlocked_ioctl) {
        pr_debug("uibc_compat_kbd_dev_ioctl null pointer");
        return -ENOTTY;
    }

    switch (cmd) {
    case UIBC_KEYBOARD:
    case UIBC_KEYBOARD_MIRACAST:
    case UIBC_KEY_PRESS:
    case UIBC_KEY_RELEASE:
    case UIBC_POINTER_X:
    case UIBC_POINTER_Y:
    case UIBC_TOUCH_DOWN:
    case UIBC_TOUCH_UP:
    case UIBC_TOUCH_MOVE: {
        ret = pfile->f_op->unlocked_ioctl(pfile, cmd, (unsigned long)compat_ptr(arg));
        break;
    }
    default:
        return -EINVAL;
    }
    return ret;
}
#endif

static long uibc_kbd_dev_ioctl(struct file * file, unsigned int cmd, unsigned long arg) {
    void __user *uarg = (void __user *)arg;
    static short XValue;
    static short YValue;
    short keycode;
    short touchPosition[16];
    int err, i;

    pr_debug("uibc_kbd_dev_ioctl,cmd=%d\n", cmd);
    switch (cmd) {
    case UIBC_KEYBOARD: {
        uibc_input_dev->keycodemax = ARRAY_SIZE(uibc_keycode);
        for (i = 0; i < ARRAY_SIZE(uibckbd->keymap); i++)
            __set_bit(uibckbd->keymap[i], uibc_input_dev->keybit);
        err = input_register_device(uibc_input_dev);
        if (err) {
            pr_err("register input device failed (%d)\n", err);
            input_free_device(uibc_input_dev);
            return err;
        }
        uibc_registered = 1;
        break;
    }
    case UIBC_KEYBOARD_MIRACAST: {
        uibc_input_dev->keycodemax = ARRAY_SIZE(uibc_keycode_chars);
        for (i = 0; i < ARRAY_SIZE(uibc_keycode_chars); i++)
            __set_bit(uibc_keycode_chars[i], uibc_input_dev->keybit);
        err = input_register_device(uibc_input_dev);
        if (err) {
            pr_err("register input device failed (%d)\n", err);
            input_free_device(uibc_input_dev);
            return err;
        }
        uibc_registered = 1;
        break;
    }
    case UIBC_KEY_PRESS: {
        if (copy_from_user(&keycode, uarg, sizeof(keycode)))
            return -EFAULT;
        pr_debug("uibc keycode %d\n", keycode);
        input_report_key(uibc_input_dev, keycode, 1);
        input_sync(uibc_input_dev);
        break;
    }
    case UIBC_KEY_RELEASE: {
        if (copy_from_user(&keycode, uarg, sizeof(keycode)))
            return -EFAULT;
        input_report_key(uibc_input_dev, keycode, 0);
        input_sync(uibc_input_dev);
        break;
    }
    case UIBC_POINTER_X: {
        if (copy_from_user(&XValue, uarg, sizeof(XValue)))
            return -EFAULT;
        pr_debug("uibc pointer X %d\n", XValue);
        break;
    }
    case UIBC_POINTER_Y: {
        if (copy_from_user(&YValue, uarg, sizeof(YValue)))
            return -EFAULT;
        pr_debug("uibc pointer Y %d\n", YValue);
        input_report_rel(uibc_input_dev, REL_X, XValue);
        input_report_rel(uibc_input_dev, REL_Y, YValue);
        input_sync(uibc_input_dev);
        XValue = 0;
        YValue = 0;
        break;
    }
    case UIBC_TOUCH_DOWN: {
        if (copy_from_user(&touchPosition, uarg, sizeof(touchPosition)))
            return -EFAULT;
        pr_debug("uibc UIBC_TOUCH_DOWN id=%d,(%d,%d)\n",
                 touchPosition[idVal(0)],
                 touchPosition[xVal(0)], touchPosition[yVal(0)]);
        input_report_key(uibc_input_dev, BTN_TOUCH, 1);
        input_report_abs(uibc_input_dev,
                         ABS_MT_TRACKING_ID, touchPosition[idVal(0)]);
        input_report_abs(uibc_input_dev, ABS_MT_POSITION_X, touchPosition[xVal(0)]);
        input_report_abs(uibc_input_dev, ABS_MT_POSITION_Y, touchPosition[yVal(0)]);
        input_mt_sync(uibc_input_dev);
        input_sync(uibc_input_dev);
        break;
    }
    case UIBC_TOUCH_UP: {
        if (copy_from_user(&touchPosition, uarg, sizeof(touchPosition)))
            return -EFAULT;
        pr_debug("uibc UIBC_TOUCH_UP");
        input_report_key(uibc_input_dev, BTN_TOUCH, 0);
        input_sync(uibc_input_dev);
        break;
    }
    case UIBC_TOUCH_MOVE: {
        if (copy_from_user(&touchPosition, uarg, sizeof(touchPosition)))
            return -EFAULT;
        for (i = 0; i < MAX_POINTERS; i++) {
            if (touchPosition[xVal(i)] == 0 && touchPosition[yVal(i)] == 0)
                continue;
            input_report_abs(uibc_input_dev,
                             ABS_MT_TRACKING_ID, touchPosition[idVal(i)]);
            input_report_abs(uibc_input_dev,
                             ABS_MT_POSITION_X, touchPosition[xVal(i)]);
            input_report_abs(uibc_input_dev,
                             ABS_MT_POSITION_Y, touchPosition[yVal(i)]);
            input_mt_sync(uibc_input_dev);
        }
        input_sync(uibc_input_dev);
        break;
    }
    default:
        return -EINVAL;
    }
    return 0;
}

static int uibc_kbd_dev_open(struct inode * inode, struct file * file) {
    int TPD_RES_X, TPD_RES_Y;

    pr_debug("*** uibckeyboard uibc_kbd_dev_open ***\n");

#ifdef LCM_ROTATE
    TPD_RES_Y = DISP_GetScreenWidth();
    TPD_RES_X = DISP_GetScreenHeight();
#else
    TPD_RES_X = DISP_GetScreenWidth();
    TPD_RES_Y = DISP_GetScreenHeight();
#endif

    uibckbd = kzalloc(sizeof(struct uibckeyboard), GFP_KERNEL);
    uibc_input_dev = input_allocate_device();
    if (!uibckbd || !uibc_input_dev)
        goto fail;

    memcpy(uibckbd->keymap, uibc_keycode, sizeof(uibc_keycode));
    uibckbd->input = uibc_input_dev;

    set_bit(INPUT_PROP_DIRECT, uibc_input_dev->propbit);

    set_bit(EV_ABS, uibc_input_dev->evbit);
    set_bit(EV_KEY, uibc_input_dev->evbit);
    set_bit(EV_REL, uibc_input_dev->evbit);

    set_bit(REL_X, uibc_input_dev->relbit);
    set_bit(REL_Y, uibc_input_dev->relbit);

    set_bit(ABS_X, uibc_input_dev->absbit);
    set_bit(ABS_Y, uibc_input_dev->absbit);
    set_bit(ABS_MT_TRACKING_ID, uibc_input_dev->absbit);
    set_bit(ABS_MT_POSITION_X, uibc_input_dev->absbit);
    set_bit(ABS_MT_POSITION_Y, uibc_input_dev->absbit);

    input_set_abs_params(uibc_input_dev, ABS_MT_POSITION_X, 0, TPD_RES_X, 0, 0);
    input_set_abs_params(uibc_input_dev, ABS_MT_POSITION_Y, 0, TPD_RES_Y, 0, 0);
    input_set_abs_params(uibc_input_dev, ABS_X, 0, TPD_RES_X, 0, 0);
    input_set_abs_params(uibc_input_dev, ABS_Y, 0, TPD_RES_Y, 0, 0);

    input_abs_set_res(uibc_input_dev, ABS_X, TPD_RES_X);
    input_abs_set_res(uibc_input_dev, ABS_Y, TPD_RES_Y);

    uibc_input_dev->name = UIBC_KBD_NAME;
    uibc_input_dev->keycode = uibckbd->keymap;
    uibc_input_dev->keycodesize = sizeof(unsigned short);
    uibc_input_dev->id.bustype = BUS_HOST;

    return 0;
fail:
    input_free_device(uibc_input_dev);
    kfree(uibckbd);

    return -EINVAL;
}

static int uibc_kbd_dev_release(struct inode * inode, struct file * file) {
    pr_debug("*** uibckeyboard uibc_kbd_dev_release ***\n");
    if (uibc_registered == 1) {
        input_unregister_device(uibc_input_dev);
        uibc_registered = 0;
    }
    return 0;
}


static struct file_operations uibc_kbd_dev_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = uibc_kbd_dev_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = uibc_compat_kbd_dev_ioctl,
#endif
    .open = uibc_kbd_dev_open,
    .release = uibc_kbd_dev_release
};

static struct miscdevice uibc_kbd_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = UIBC_KBD_NAME,
    .fops = &uibc_kbd_dev_fops,
};


static int uibc_keyboard_probe(struct platform_device * pdev) {

    int i, err;

    pr_debug("*** uibckeyboard probe ***\n");

    uibckbd = kzalloc(sizeof(struct uibckeyboard), GFP_KERNEL);
    uibc_input_dev = input_allocate_device();
    if (!uibckbd || !uibc_input_dev)
        goto fail;

    memcpy(uibckbd->keymap, uibc_keycode, sizeof(uibc_keycode));
    uibckbd->input = uibc_input_dev;
    __set_bit(EV_KEY, uibc_input_dev->evbit);
    platform_set_drvdata(pdev, uibckbd);

    uibc_input_dev->name = UIBC_KBD_NAME;
    uibc_input_dev->keycode = uibckbd->keymap;
    uibc_input_dev->keycodesize = sizeof(unsigned short);
    uibc_input_dev->keycodemax = ARRAY_SIZE(uibc_keycode);
    uibc_input_dev->id.bustype = BUS_HOST;
    uibc_input_dev->dev.parent = &pdev->dev;


    for (i = 0; i < ARRAY_SIZE(uibckbd->keymap); i++)
        __set_bit(uibckbd->keymap[i], uibc_input_dev->keybit);

    input_set_capability(uibc_input_dev, EV_MSC, MSC_SCAN);

    uibc_kbd_dev.parent = &pdev->dev;
    err = misc_register(&uibc_kbd_dev);
    if (err) {
        pr_err("register device failed (%d)\n", err);
        return err;
    }

    return 0;

fail:
    platform_set_drvdata(pdev, NULL);
    input_free_device(uibc_input_dev);
    kfree(uibckbd);

    return -EINVAL;
}

static const struct of_device_id uibc_of_ids[] = {
    { .compatible = "mediatek,uibc", },
    {}
};

static struct platform_driver uibc_keyboard_driver = {
    .probe = uibc_keyboard_probe,
    .driver = {
        .name = UIBC_KBD_NAME,
        .of_match_table = uibc_of_ids,
    },
};


static int uibc_keyboard_init(void) {
    pr_debug("uibc_keyboard_init OK\n");

    return platform_driver_register(&uibc_keyboard_driver);
}


static void __exit uibc_keyboard_exit(void) {
}
module_init(uibc_keyboard_init);
module_exit(uibc_keyboard_exit);

MODULE_DESCRIPTION("uibc keyboard Device");
MODULE_LICENSE("GPL");
