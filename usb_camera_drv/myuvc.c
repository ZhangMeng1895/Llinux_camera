#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <asm/atomic.h>
#include <asm/unaligned.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include "descriptor.h"

static struct video_device *myuvc_vdev = NULL;
struct usb_device *myuvc_udev = NULL;
int myuvc_control_intf;
int myuvc_streaming_intf;

/* video_device.release */
static void myuvc_release(struct video_device *vdev)
{
}

extern struct v4l2_file_operations myuvc_fops;
extern struct v4l2_ioctl_ops myuvc_ioctl_ops;

/* 所支持的usb设备类 */
static struct usb_device_id myuvc_ids[] = {
    /* Generic USB Video Class */
    {USB_INTERFACE_INFO(USB_CLASS_VIDEO, 1, 0)}, /**< VideoControl interface */
    {USB_INTERFACE_INFO(USB_CLASS_VIDEO, 2, 0)}, /**< VideoStreaming interface */
    {},
};

static int myuvc_probe(struct usb_interface *intf,
                       const struct usb_device_id *id)
{
    struct v4l2_device *v4l2dev;
    static int cnt = 0;

    myuvc_udev = interface_to_usbdev(intf);

    v4l2dev = kmalloc(sizeof(struct v4l2_device), GFP_KERNEL);
    printk("myuvc_probe : cnt = %d\n", cnt++);

    if (cnt == 1)
    {
        myuvc_control_intf = intf->cur_altsetting->desc.bInterfaceNumber;
    }
    else if (cnt == 2)
    {
        myuvc_streaming_intf = intf->cur_altsetting->desc.bInterfaceNumber;
    }

    if (cnt == 2)
    {
        /* 分配video_device结构体 */
        myuvc_vdev = video_device_alloc();
        if (NULL == myuvc_vdev)
        {
            printk("can not malloc myuvc_vdev!\r\n");
            return -1;
        }

        /* 设置该结构体 下面这些设置都是必须的 */
        myuvc_vdev->v4l2_dev = v4l2dev;
        myuvc_vdev->release = myuvc_release;
        myuvc_vdev->fops = &myuvc_fops;
        myuvc_vdev->ioctl_ops = &myuvc_ioctl_ops;
        myuvc_vdev->device_caps = V4L2_CAP_DEVICE_CAPS;

        /* 注册该结构体 */
        video_register_device(myuvc_vdev, VFL_TYPE_GRABBER, -1);
    }

    return 0;
}

static void myuvc_disconnect(struct usb_interface *intf)
{
    static int cnt = 0;
    printk("myuvc_disconnect : cnt = %d\n", cnt++);

    if (cnt == 2)
    {
        video_unregister_device(myuvc_vdev);
        video_device_release(myuvc_vdev);
    }
}

struct usb_driver myuvc_driver = {
    .name = "myuvcvideo",
    .probe = myuvc_probe,
    .disconnect = myuvc_disconnect,
    .id_table = myuvc_ids,
};

static int myuvc_init(void)
{
    int ret;

    ret = usb_register(&myuvc_driver);
    if (ret < 0)
        printk("USB register error!\n");
    return ret;
}

static void myuvc_cleanup(void)
{
    usb_deregister(&myuvc_driver);
}

module_init(myuvc_init);
module_exit(myuvc_cleanup);
MODULE_LICENSE("GPL");
