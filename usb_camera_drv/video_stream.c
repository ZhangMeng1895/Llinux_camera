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
#include <linux/version.h>
#include <media/videobuf-core.h>
#include <linux/string.h>
#include "video_stream.h"
#include <linux/usb.h>
#include "video.h"

static int uvc_version = 0x0110;

extern struct usb_device *myuvc_udev;
extern int myuvc_control_intf;
extern int myuvc_streaming_intf;
extern struct myuvc_queue uq;

/* 打印输出信息 */
void myuvc_print_streaming_params(struct myuvc_streaming_ctrl *ctrl)
{
    printk("video params:\n");
    printk("bmHint                   = %d\n", ctrl->bmHint);
    printk("bFormatIndex             = %d\n", ctrl->bFormatIndex);
    printk("bFrameIndex              = %d\n", ctrl->bFrameIndex);
    printk("dwFrameInterval          = %d\n", ctrl->dwFrameInterval);
    printk("wKeyFrameRate            = %d\n", ctrl->wKeyFrameRate);
    printk("wPFrameRate              = %d\n", ctrl->wPFrameRate);
    printk("wCompQuality             = %d\n", ctrl->wCompQuality);
    printk("wCompWindowSize          = %d\n", ctrl->wCompWindowSize);
    printk("wDelay                   = %d\n", ctrl->wDelay);
    printk("dwMaxVideoFrameSize      = %d\n", ctrl->dwMaxVideoFrameSize);
    printk("dwMaxPayloadTransferSize = %d\n", ctrl->dwMaxPayloadTransferSize);
    printk("dwClockFrequency         = %d\n", ctrl->dwClockFrequency);
    printk("bmFramingInfo            = %d\n", ctrl->bmFramingInfo);
    printk("bPreferedVersion         = %d\n", ctrl->bPreferedVersion);
    printk("bMinVersion              = %d\n", ctrl->bMinVersion);
}

/* 这个函数获取的信息是什么？ */
int myuvc_get_streaming_params(struct myuvc_streaming_ctrl *ctrl)
{
    __u8 *data;
    __u16 size;
    int ret;
    __u8 type = USB_TYPE_CLASS | USB_RECIP_INTERFACE;
    unsigned int pipe;

    size = (uvc_version >= 0x0110 ? 34 : 26);
    data = kmalloc(size, GFP_KERNEL);
    if (data == NULL)
        return -ENOMEM;

    pipe = (GET_CUR & 0x80) ? usb_rcvctrlpipe(myuvc_udev, 0)
                            : usb_sndctrlpipe(myuvc_udev, 0);
    type |= (GET_CUR & 0x80) ? USB_DIR_IN : USB_DIR_OUT;

    ret = usb_control_msg(myuvc_udev, pipe, GET_CUR, type, VS_PROBE_CONTROL << 8,
                          0 << 8 | myuvc_streaming_intf, data, size, 5000);

    if (ret < 0)
        goto done;

    ctrl->bmHint = le16_to_cpup((__le16 *)&data[0]);
    ctrl->bFormatIndex = data[2];
    ctrl->bFrameIndex = data[3];
    ctrl->dwFrameInterval = le32_to_cpup((__le32 *)&data[4]);
    ctrl->wKeyFrameRate = le16_to_cpup((__le16 *)&data[8]);
    ctrl->wPFrameRate = le16_to_cpup((__le16 *)&data[10]);
    ctrl->wCompQuality = le16_to_cpup((__le16 *)&data[12]);
    ctrl->wCompWindowSize = le16_to_cpup((__le16 *)&data[14]);
    ctrl->wDelay = le16_to_cpup((__le16 *)&data[16]);
    ctrl->dwMaxVideoFrameSize = get_unaligned_le32(&data[18]);
    ctrl->dwMaxPayloadTransferSize = get_unaligned_le32(&data[22]);

    if (size == 34)
    {
        ctrl->dwClockFrequency = get_unaligned_le32(&data[26]);
        ctrl->bmFramingInfo = data[30];
        ctrl->bPreferedVersion = data[31];
        ctrl->bMinVersion = data[32];
        ctrl->bMaxVersion = data[33];
    }
    else
    {
        // ctrl->dwClockFrequency = video->dev->clock_frequency;
        ctrl->bmFramingInfo = 0;
        ctrl->bPreferedVersion = 0;
        ctrl->bMinVersion = 0;
        ctrl->bMaxVersion = 0;
    }

done:
    kfree(data);

    return (ret < 0) ? ret : 0;
}

/* 测试参数是否可以获得摄像头的支持 */
int myuvc_try_streaming_params(struct myuvc_streaming_ctrl *ctrl)
{
    __u8 *data;
    __u16 size;
    int ret;
    __u8 type = USB_TYPE_CLASS | USB_RECIP_INTERFACE;
    unsigned int pipe;

    memset(ctrl, 0, sizeof *ctrl);

    // ctrl->bmHint = 1;
    // ctrl->bFormatIndex = 2;
    // ctrl->bFrameIndex = 1;          /* 这里设置的是使用哪种分辨率 该参数是分辨率数组的索引 */
    ctrl->bmHint = 1;
    ctrl->bFormatIndex = 1;
    ctrl->bFrameIndex = 2;          /* 这里设置的是使用哪种分辨率 该参数是分辨率数组的索引 */
    ctrl->dwFrameInterval = 333333; /* 这个参数设置的是摄像头两个帧之间的时间间隔单位ns */
    /* 该数字应该来自于VideoStreaming Interface Descriptor描述符 */

    size = uvc_version >= 0x0110 ? 34 : 26;
    data = kzalloc(size, GFP_KERNEL);
    if (data == NULL)
        return -ENOMEM;

    *(__le16 *)&data[0] = cpu_to_le16(ctrl->bmHint);
    data[2] = ctrl->bFormatIndex;
    data[3] = ctrl->bFrameIndex;
    *(__le32 *)&data[4] = cpu_to_le32(ctrl->dwFrameInterval);
    *(__le16 *)&data[8] = cpu_to_le16(ctrl->wKeyFrameRate);
    *(__le16 *)&data[10] = cpu_to_le16(ctrl->wPFrameRate);
    *(__le16 *)&data[12] = cpu_to_le16(ctrl->wCompQuality);
    *(__le16 *)&data[14] = cpu_to_le16(ctrl->wCompWindowSize);
    *(__le16 *)&data[16] = cpu_to_le16(ctrl->wDelay);
    put_unaligned_le32(ctrl->dwMaxVideoFrameSize, &data[18]);
    put_unaligned_le32(ctrl->dwMaxPayloadTransferSize, &data[22]);

    if (size == 34)
    {
        put_unaligned_le32(ctrl->dwClockFrequency, &data[26]);
        data[30] = ctrl->bmFramingInfo;
        data[31] = ctrl->bPreferedVersion;
        data[32] = ctrl->bMinVersion;
        data[33] = ctrl->bMaxVersion;
    }

    pipe = (SET_CUR & 0x80) ? usb_rcvctrlpipe(myuvc_udev, 0)
                            : usb_sndctrlpipe(myuvc_udev, 0);
    type |= (SET_CUR & 0x80) ? USB_DIR_IN : USB_DIR_OUT;

    ret = usb_control_msg(myuvc_udev, pipe, SET_CUR, type, VS_PROBE_CONTROL << 8,
                          0 << 8 | myuvc_streaming_intf, data, size, 5000);

    kfree(data);

    return (ret < 0) ? ret : 0;
}

/* 参考: uvc_v4l2_try_format ∕uvc_probe_video
 *       uvc_set_video_ctrl(video, probe, 1)
 设置参数
 */
int myuvc_set_streaming_params(struct myuvc_streaming_ctrl *ctrl)
{
    __u8 *data;
    __u16 size;
    int ret;
    __u8 type = USB_TYPE_CLASS | USB_RECIP_INTERFACE;
    unsigned int pipe;

    size = uvc_version >= 0x0110 ? 34 : 26;
    data = kzalloc(size, GFP_KERNEL);
    if (data == NULL)
        return -ENOMEM;

    *(__le16 *)&data[0] = cpu_to_le16(ctrl->bmHint);
    data[2] = ctrl->bFormatIndex;
    data[3] = ctrl->bFrameIndex;
    *(__le32 *)&data[4] = cpu_to_le32(ctrl->dwFrameInterval);
    *(__le16 *)&data[8] = cpu_to_le16(ctrl->wKeyFrameRate);
    *(__le16 *)&data[10] = cpu_to_le16(ctrl->wPFrameRate);
    *(__le16 *)&data[12] = cpu_to_le16(ctrl->wCompQuality);
    *(__le16 *)&data[14] = cpu_to_le16(ctrl->wCompWindowSize);
    *(__le16 *)&data[16] = cpu_to_le16(ctrl->wDelay);
    put_unaligned_le32(ctrl->dwMaxVideoFrameSize, &data[18]);
    put_unaligned_le32(ctrl->dwMaxPayloadTransferSize, &data[22]);

    if (size == 34)
    {
        put_unaligned_le32(ctrl->dwClockFrequency, &data[26]);
        data[30] = ctrl->bmFramingInfo;
        data[31] = ctrl->bPreferedVersion;
        data[32] = ctrl->bMinVersion;
        data[33] = ctrl->bMaxVersion;
    }

    pipe = (SET_CUR & 0x80) ? usb_rcvctrlpipe(myuvc_udev, 0)
                            : usb_sndctrlpipe(myuvc_udev, 0);
    type |= (SET_CUR & 0x80) ? USB_DIR_IN : USB_DIR_OUT;
    // 之前是PROBE，现在是COMMIT
    ret = usb_control_msg(myuvc_udev, pipe, SET_CUR, type, VS_COMMIT_CONTROL << 8,
                          0 << 8 | myuvc_streaming_intf, data, size, 5000);

    kfree(data);

    return (ret < 0) ? ret : 0;
}

void myuvc_uninit_urbs(void)
{
    int i;
    for (i = 0; i < 32; i++)
    {
        if (uq.urb_buffer[i])
        {
            usb_free_coherent(myuvc_udev, uq.urb_size, uq.urb_buffer[i], uq.urb_dma[i]);
            uq.urb_buffer[i] = NULL;
        }
        if (uq.urb[i])
        {
            usb_free_urb(uq.urb[i]);
            uq.urb[i] = NULL;
        }
    }
}

/* urb->complete urb包接收完成处理回调函数 */
void myuvc_video_complete(struct urb *urb)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    u8 *src;
    u8 *dest;
    int i, ret;
    int len;
    int maxlen;
    int nbytes;
    struct myuvc_buffer *buffer;

    switch (urb->status)
    {
    case 0:
        break;

    default:
        printk("Non-zero status (%d) in video "
               "completion handler.\n",
               urb->status);
        return;
    }

    if (!list_empty(&uq.irqqueue))
    {
        /* 从irqqueue中取出第一个队列项 */
        buffer = list_first_entry(&uq.irqqueue, struct myuvc_buffer, irq);

        for (i = 0; i < urb->number_of_packets; i++)
        {
            if (urb->iso_frame_desc[i].status < 0)
            {
                printk("USB isochronous frame "
                       "lost (%d).\n",
                       urb->iso_frame_desc[i].status);
                continue;
            }
            src = urb->transfer_buffer + urb->iso_frame_desc[i].offset;
            dest = uq.mem + buffer->vbuff.m.offset + buffer->vbuff.bytesused;
            len = urb->iso_frame_desc[i].actual_length;

            /* Sanity checks:
             * - packet must be at least 2 bytes long
             * - bHeaderLength value must be at least 2 bytes (see above)    //数据头不能小于2字节
             * - bHeaderLength value can't be larger than the packet size.    //同时数据头不能大于整个子包packet的长度
             */

            /* 判断数据是否有效 */
            /* URB数据含义:
             * src[0] : 头部长度
             * src[1] : 错误状态
             */
            if (len < 2 || src[0] < 2 || src[0] > len)
                continue; // 处理下一个子包

            /* Skip payloads marked with the error bit ("error frames"). */
            // 忽略有错误状态的数据包
            if (src[1] & UVC_STREAM_ERR)
            {
                printk("Dropping payload (error bit set).\n");
                continue;
            }

            len -= src[0];                                           /* 除去头部数据后的数据长度 */
            maxlen = buffer->vbuff.length - buffer->vbuff.bytesused; /* 缓冲区最多还能存多少数据 */
            nbytes = min(len, maxlen);
            memcpy(dest, src + src[0], nbytes); /* 将数据拷贝至缓存 */
            buffer->vbuff.bytesused += nbytes;

            /* 判断一帧数据是否完全接受 */
            if (src[1] & UVC_STREAM_EOF && buffer->vbuff.bytesused != 0)
            { // src[1]的标志状态为EOF（End of Frame） 并且已经接收到的数据不为0，就表示一个urb的数据已经接收完毕！
                printk("Frame complete (EOF found).\n");
                if (len == 0) // 表明接收到了一个空的帧（urb）
                    printk("EOF in empty payload.\n");
                buffer->state = VIDEOBUF_DONE;
            }
        }

        /* 接受完一帧数据则唤醒等待数据的队列 */
        if (buffer->state == VIDEOBUF_DONE || buffer->state == VIDEOBUF_ERROR)
        {
            list_del(&buffer->irq);
            wake_up(&buffer->wait);
        }
    }

    /* 再次提交URB why?*/
    if ((ret = usb_submit_urb(urb, GFP_ATOMIC)) < 0)
    {
        printk("Failed to resubmit video URB (%d).\n", ret);
    }
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
}

/* 分配初始化urb */
int myuvc_alloc_init_urbs(struct myuvc_streaming_ctrl *ctrl)
{

    u16 psize; /* 实时传输端点一次能传输的最大字节数 */
    u32 size;
    int npackets;
    int i;

    psize = 1024;                         /* 应该从描述符中获取  wMaxPacketSize     0x0400  1x 1024 bytes */
    size = ctrl->dwMaxVideoFrameSize;     /* 一帧数据的最大长度 */
    npackets = DIV_ROUND_UP(size, psize); /* 计算一帧数据要分成多少次发送 */
    if (npackets > 32)
        npackets = 32;

    size = uq.urb_size = psize * npackets;

    for (i = 0; i < 32; i++)
    {
        /* 分配usb buffers */
        uq.urb_buffer[i] = usb_alloc_coherent(myuvc_udev, psize * npackets, GFP_KERNEL | __GFP_NOWARN, &uq.urb_dma[i]);
        /* 分配urb结构体 */
        uq.urb[i] = usb_alloc_urb(npackets, GFP_KERNEL);

        if (!uq.urb_buffer[i] || !uq.urb[i])
        {
            myuvc_uninit_urbs();
            return -ENOMEM;
        }
    }

    /* 设置urb */
    for (i = 0; i < 32; i++)
    {
        int j;
        struct urb *urb = uq.urb[i];
        urb->dev = myuvc_udev;
        urb->context = NULL;
        urb->pipe = usb_rcvisocpipe(myuvc_udev, 0x84); /* 0x81应该从描述符中获取 bEndpointAddress */
        urb->transfer_flags = URB_ISO_ASAP | URB_NO_TRANSFER_DMA_MAP;
        urb->interval = 1; /* Endpoint Descriptor:bInterval */
        urb->transfer_buffer = uq.urb_buffer[i];
        urb->transfer_dma = uq.urb_dma[i];
        urb->complete = myuvc_video_complete; /* urb包接收完成处理回调函数 */
        urb->number_of_packets = npackets;
        urb->transfer_buffer_length = size;

        for (j = 0; j < npackets; j++)
        {
            urb->iso_frame_desc[j].offset = j * psize;
            urb->iso_frame_desc[j].length = psize;
        }
    }

    return 0;
}