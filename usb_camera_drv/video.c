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
#include "video.h"
#include "video_stream.h"

struct myuvc_streaming_ctrl myuvc_params;

struct myuvc_queue uq;

/* 用于描述摄像头设备支持的分辨率 */
static struct frame_desc frm_desc[] = {
    {640, 480},
    {640, 360},
    {1920, 1080},
    {1280, 720},
};

static int bBitsPerPixel = 16; /* 每个像素占用多少字节 该信息来自于描述符 */
                               /* 这几个参数（分辨率 bitperpixel uvc_verison）应该从描述符中获取 */

struct v4l2_format myuvc_fmt;

extern struct usb_device *myuvc_udev;
extern int myuvc_control_intf;
extern int myuvc_streaming_intf;

int myuvc_open(struct file *filp)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}

int myuvc_close(struct file *filp)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}

void myuvc_vm_open(struct vm_area_struct *area)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    struct myuvc_buffer *buffer = area->vm_private_data;
    buffer->vma_use_count++;
}

void myuvc_vm_clsoe(struct vm_area_struct *area)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    struct myuvc_buffer *buffer = area->vm_private_data;
    buffer->vma_use_count--;
}

static struct vm_operations_struct myuvc_vm_ops =
    {
        .open = myuvc_vm_open,
        .close = myuvc_vm_clsoe,
};

/* 将缓存映射到应用层的地址空间 应用程序可以直接操作一块缓存 */
int myuvc_mmap(struct file *file, struct vm_area_struct *vma)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    int i, ret;
    struct myuvc_buffer *buffer;
    unsigned long addr;
    unsigned long start = vma->vm_start;
    unsigned long size = vma->vm_end - vma->vm_start;

    /* 应用程序调用mmap函数时会传入offset参数
        需要根据这个offset找出指定的缓冲区
    */
    for (i = 0; i < uq.count; i++)
    {
        buffer = &uq.buffer[i];
        if ((buffer->vbuff.m.offset >> PAGE_SHIFT) == vma->vm_pgoff)
        {
            break;
        }
    }

    if (i == uq.count || size != uq.buff_size)
    {
        ret = -EINVAL;
        goto done;
    }

    vma->vm_flags |= VM_IO;

    /* 根据虚拟地址找到缓冲区对应的page结构体 */
    addr = (unsigned long)uq.mem + buffer->vbuff.m.offset;
    while (size > 0)
    {
        struct page *page = vmalloc_to_page((void *)addr); /* 用于找到虚拟地址addr对应的物理page */

        /* 把page和APP传入的虚拟地址（vma）挂钩：就是把这块page映射到这个虚拟地址上面去 */
        if ((ret = vm_insert_page(vma, start, page)) < 0)
            goto done;

        start += PAGE_SIZE;
        addr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }

    vma->vm_private_data = buffer;
    vma->vm_ops = &myuvc_vm_ops;
    myuvc_vm_open(vma);
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

done:
    return ret;
}

/* app调用poll函数来确定缓存是否有数据 */
__poll_t myuvc_poll(struct file *file, struct poll_table_struct *wait)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    struct myuvc_buffer *buffer;
    unsigned int mask = 0;

    /* 从mainqueue中取出第一个buffer 判断其状态 如果未就绪则休眠 */
    if (list_empty(&uq.mainqueue))
    {
        mask |= POLLERR;
        goto done;
    }

    buffer = list_first_entry(&uq.mainqueue, struct myuvc_buffer, stream); // 取出第一个Buffer

    poll_wait(file, &(buffer->wait), wait);
    if (buffer->state == VIDEOBUF_DONE || buffer->state == VIDEOBUF_ERROR) /* 为什么是这两个状态 */
    {
        mask |= POLLIN | POLLRDNORM;
    }
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

done:
    return mask;
}

struct v4l2_file_operations myuvc_fops = {
    .owner = THIS_MODULE,
    .open = myuvc_open,
    .release = myuvc_close,
    .mmap = myuvc_mmap,
    .unlocked_ioctl = video_ioctl2,
    .poll = myuvc_poll,
};

/* VIDIOC_QUERYCAP handler */
/* 应用程序通过IOCTL调用可以获得设备属性 */
int myuvc_vidioc_querycap(struct file *file, void *fh,
                          struct v4l2_capability *cap)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    strlcpy(cap->driver, "myuvc", sizeof(cap->driver));
    strlcpy(cap->bus_info, "USB", sizeof(cap->bus_info));
    strlcpy(cap->card, "myuvc", sizeof(cap->card));
    cap->capabilities = V4L2_CAP_DEVICE_CAPS | V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    return 0;
}

/* 应用程序通过该ioctl调用获取摄像头数据格式
参考 uvc_fmts数组，该数组内核提供
这个地方应该是从描述符中解析出格式
记得修改完善该部分 */
int myuvc_vidioc_enum_fmt_vid_cap(struct file *file, void *fh,
                                  struct v4l2_fmtdesc *f)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    if (f->index > 0)
        return -EINVAL;
    strcpy(f->description, "4:2:2,packed,YUYV");
    f->pixelformat = V4L2_PIX_FMT_YUYV;
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    return 0;
}

/* 用于测试驱动程序是否支持应用程序想要设置的某个数据格式
支持就返回0 函数参数中的f即来自应用程序*/
int myuvc_try_fmt_vid_cap(struct file *file, void *fh,
                          struct v4l2_format *f)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
    {
        return -EINVAL;
    }
    if (f->fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
    {
        return -EINVAL;
    }

    /* 调整分辨率 */
    f->fmt.pix.width = frm_desc[0].width;
    f->fmt.pix.height = frm_desc[0].height;
    f->fmt.pix.bytesperline = (f->fmt.pix.width * bBitsPerPixel) / 8;
    f->fmt.pix.sizeimage = (f->fmt.pix.bytesperline) * (f->fmt.pix.height);
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    return 0;
}

/* 设置格式
struct v4l2_format *f 来自用户层
*/
int myuvc_vidioc_s_fmt_vid_cap(struct file *file, void *fh,
                               struct v4l2_format *f)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    int ret = myuvc_try_fmt_vid_cap(file, NULL, f);
    if (ret < 0)
        return ret;

    memcpy(&myuvc_fmt, f, sizeof(struct v4l2_format)); /* 将用户层传入的格式赋给myuvc_fmt */
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    return 0;
}

/* 获取当前的格式 返回给应用程序 */
int myuvc_vidioc_g_fmt_vid_cap(struct file *file, void *fh,
                               struct v4l2_format *f)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    memcpy(f, &myuvc_fmt, sizeof(struct v4l2_format));
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    return 0;
}

static int myuvc_free_buffers(void)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    vfree(uq.mem);
    memset(&uq, 0, sizeof(uq));
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    return 0;
}

/* 应用程序申请缓存时该函数被调用
struct v4l2_requestbuffers *b 来自应用层
 */
int myuvc_vidioc_reqbufs(struct file *file, void *fh,
                         struct v4l2_requestbuffers *b)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    int n = b->count;                                       /* 申请buffer的数量 */
    int buffsize = PAGE_ALIGN(myuvc_fmt.fmt.pix.sizeimage); /* 一个buff的大小为一个图像的大小 按页对齐 */

    int i;
    void *mem = NULL;
    int ret;

    if ((ret = myuvc_free_buffers()) < 0)
        goto done;

    if (n == 0)
        goto done;

    /* 分配缓冲区内存 若不成功则减少需要分配的缓冲区数目直至成功 */
    for (; n > 0; n--)
    {
        mem = vmalloc_32(n * buffsize);
        if (mem != NULL)
            break;
    }

    if (mem == NULL)
    {
        ret = -ENOMEM;
        goto done;
    }

    /* 这些缓存是一次性作为一个整体分配的
    设置申请的buffer
    */
    memset(&uq, 0, sizeof(struct myuvc_queue));

    /* 初始化链表头 */
    INIT_LIST_HEAD(&(uq.mainqueue));
    INIT_LIST_HEAD(&(uq.irqqueue));

    for (i = 0; i < n; i++)
    {
        uq.buffer[i].vbuff.index = i;                            /* buffer索引 */
        uq.buffer[i].vbuff.m.offset = i * buffsize;              /* buffer偏移 */
        uq.buffer[i].vbuff.length = myuvc_fmt.fmt.pix.sizeimage; /* buffer大小 */
        uq.buffer[i].vbuff.type = b->type;
        uq.buffer[i].vbuff.sequence = 0;
        uq.buffer[i].vbuff.field = V4L2_FIELD_NONE;
        uq.buffer[i].vbuff.memory = V4L2_MEMORY_MMAP;
        uq.buffer[i].vbuff.flags = 0;
        uq.buffer[i].state = VIDEOBUF_IDLE;      /* 空闲 */
        init_waitqueue_head(&uq.buffer[i].wait); /* 初始化等待队列 */
    }

    uq.mem = mem; /* 记录申请成功的缓冲区起始地址 */
    uq.count = n;
    uq.buff_size = buffsize;
    ret = n;
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

done:
    return ret;
}

/* 查询缓冲区的信息 */
int myuvc_vidioc_querbuf(struct file *file, void *fh,
                         struct v4l2_buffer *b)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    int ret = 0;
    if (b->index >= uq.count) /* 如果想要查询的buffer的索引超过了申请的buffer总数 */
    {
        ret = -EINVAL;
        goto done;
    }

    memcpy(b, &(uq.buffer[b->index].vbuff), sizeof(struct v4l2_buffer)); /* 将队列中的Buff拷贝给应用层传入的buff指针 返回给应用层 */

    /* 更新v4l2_buffer的flag */
    if (uq.buffer[b->index].vma_use_count)
    {
        b->flags |= V4L2_BUF_FLAG_MAPPED; /*  该buffer被mmap映射过*/
    }

    switch (uq.buffer[b->index].state) /* 根据buffer的状态 */
    {
    case VIDEOBUF_ERROR:
    case VIDEOBUF_DONE:
        b->flags |= V4L2_BUF_FLAG_DONE;
        break;
    case VIDEOBUF_QUEUED: /* 表示buffer在队列中 */
    case VIDEOBUF_ACTIVE:
        b->flags |= V4L2_BUF_FLAG_QUEUED;
        break;
    case VIDEOBUF_IDLE:
    default:
        break;
    }
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

done:
    return ret;
}

/* 将缓存放入队列中 */
int myuvc_vidioc_qbuf(struct file *file, void *fh,
                      struct v4l2_buffer *b)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    struct myuvc_buffer *p = &(uq.buffer[b->index]);
    /* 修改状态 */
    p->state = VIDEOBUF_QUEUED;
    b->bytesused = 0;

    /* 放入两个队列
    队列1：供APp使用
    当缓冲区没有数据时，放入mainqueue队列
    当缓冲区有数据时，app从mainqueue队列取出*/
    list_add_tail(&(p->stream), &(uq.mainqueue));

    /* 队列2：供底层产生数据的函数使用 当采集到数据时
    从irqqueeu中取出第一个Buffer存入数据 */
    list_add_tail(&(p->irq), &(uq.irqqueue));
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    return 0;
}

/* 将缓冲区从链表中取出 */
int myuvc_vidioc_dqbuf(struct file *file, void *fh,
                       struct v4l2_buffer *b)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    /* app发现数据就绪后从mainqueue队列取出buffer */
    // struct myuvc_buffer *buf = &(uq.buffer[b->index]);

    // list_del(&(p->stream));
    int ret;
    struct myuvc_buffer *buf;

    if (list_empty(&uq.mainqueue))
    {
        ret = -EINVAL;
        goto done;
    }

    buf = list_first_entry(&uq.mainqueue, struct myuvc_buffer, stream);

    switch (buf->state)
    {
    case VIDEOBUF_ERROR:
        ret = -EIO;
        break;
    case VIDEOBUF_DONE:
        buf->state = VIDEOBUF_IDLE;
        break;
    case VIDEOBUF_IDLE:
    case VIDEOBUF_QUEUED:
    case VIDEOBUF_ACTIVE:
    default:
        ret = -EINVAL;
        goto done;
    }

    list_del(&buf->stream);
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

done:
    return ret;
}

/* 启动数据传输 */
int myuvc_vidioc_streamon(struct file *file, void *fh,
                          enum v4l2_buf_type idx)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    int i, ret;
    /* 向USB摄像头设置参数
        根据一个结构体设置数据包 调用usb_control_msg发出数据包
    */
    /* 先读出参数 */

    // myuvc_print_streaming_params(&myuvc_params);

    /* 修改、测试参数 */
    myuvc_try_streaming_params(&myuvc_params); /* 测试想要修改的参数是否能获得摄像头的支持 */
    myuvc_get_streaming_params(&myuvc_params); /* 不可缺少 需要先读取后面才能成功设置*/
    myuvc_set_streaming_params(&myuvc_params);
    /* 设置videostreaming interface使用的setting
        从myuvc_params中确定带宽
        根据settting的endpoint能传输的最大包大小，选择setting
    */
    /* 手工确定:
     * bandwidth = myuvc_params.dwMaxPayloadTransferSize = 1024
     *
     * 观察lsusb -v -d 0x1e4e:的结果:
     *                wMaxPacketSize     0x0400  1x 1024 bytes
     *
     * bAlternateSetting       1  这些应该从描述符获取
     */
    usb_set_interface(myuvc_udev, myuvc_streaming_intf, 1);

    /* 分配设置URB */
    ret = myuvc_alloc_init_urbs(&myuvc_params);
    if (ret)
    {
        printk("myuvc_alloc_init_urbs error: ret = %d", ret);
    }

    /* 提交URB以接收数据 */
    for (i = 0; i < 32; ++i)
    {
        if ((ret = usb_submit_urb(uq.urb[i], GFP_KERNEL)) < 0)
        {
            printk("Failed to submit URB %u (%d).\n", i, ret);
            myuvc_uninit_urbs();
            return ret;
        }
    }
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    return 0;
}

int myuvc_vidioc_streamoff(struct file *file, void *fh,
                           enum v4l2_buf_type idx)
{
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    /* kill urb */
    struct urb *urb;
    int i;

    for (i = 0; i < 32; i++)
    {
        if (uq.urb[i] == NULL)
            continue;
        usb_kill_urb(urb);
        usb_free_urb(urb);
        uq.urb[i] = NULL;
    }

    /* free urb */
    myuvc_uninit_urbs();

    /* 设置ivdeostream interface 为setting 0 */
    usb_set_interface(myuvc_udev, myuvc_streaming_intf, 0);
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

    return 0;
}

struct v4l2_ioctl_ops myuvc_ioctl_ops = {

    .vidioc_querycap = myuvc_vidioc_querycap,

    /* 格式操作 */
    .vidioc_enum_fmt_vid_cap = myuvc_vidioc_enum_fmt_vid_cap,
    .vidioc_g_fmt_vid_cap = myuvc_vidioc_g_fmt_vid_cap,
    .vidioc_try_fmt_vid_cap = myuvc_try_fmt_vid_cap,
    .vidioc_s_fmt_vid_cap = myuvc_vidioc_s_fmt_vid_cap,

    /* 缓冲区操作 */
    .vidioc_reqbufs = myuvc_vidioc_reqbufs,
    .vidioc_querybuf = myuvc_vidioc_querbuf,
    .vidioc_qbuf = myuvc_vidioc_qbuf,
    .vidioc_dqbuf = myuvc_vidioc_dqbuf,

    /* 启动/停止 */
    .vidioc_streamon = myuvc_vidioc_streamon,
    .vidioc_streamoff = myuvc_vidioc_streamoff,
};