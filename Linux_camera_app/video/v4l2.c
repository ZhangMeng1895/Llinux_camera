#include "video_manager.h"
#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <poll.h>

static struct video_operations my_vops;

static int g_aiSupportedFormats[] = {V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_RGB565};

static int isSupportThisFormat(int iPixelFormat)
{
    int i;
    for (i = 0; i < sizeof(g_aiSupportedFormats) / sizeof(g_aiSupportedFormats[0]); i++)
    {
        if (g_aiSupportedFormats[i] == iPixelFormat)
            return 1;
    }
    return 0;
}

int my_init_device(char *device_name, struct camera_device *c_dev)
{
    int fd_video, ret;
    int fmt_idx = 0;
    struct v4l2_fmtdesc fmtdesc;
    /* 打开设备 */
    fd_video = open(device_name, O_RDWR);
    if (fd_video < 0)
    {
        printf("can not open %s\n", device_name);
        return -1;
    }
    c_dev->fd_video = fd_video;

    while (1)
    {
        /* 枚举格式 */
        memset(&fmtdesc, 0, sizeof(struct v4l2_fmtdesc));
        fmtdesc.index = fmt_idx++;
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret = ioctl(fd_video, VIDIOC_ENUM_FMT, &fmtdesc);
        if (ret != 0)
            break;
        if (isSupportThisFormat(fmtdesc.pixelformat))
        {
            c_dev->pixelformat = fmtdesc.pixelformat;
            break;
        }
    }

    if (!c_dev->pixelformat)
    {
        printf("can not support this device!\r\n");
        ret = -1;
        goto done;
    }

    /* 设置格式 */
    struct v4l2_format fmt;
    memset(&fmt, 9, sizeof(struct v4l2_format));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 1024;
    fmt.fmt.pix.height = 768;
    fmt.fmt.pix.pixelformat = c_dev->pixelformat;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    ret = ioctl(fd_video, VIDIOC_S_FMT, &fmt); /* 设置格式 如果传入分辨率不支持则驱动会设置成某种分辨率并保存在传入的结构体中 */
    if (ret == 0)
    {
        printf("set format : %d * %d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
        c_dev->width = fmt.fmt.pix.width;
        c_dev->height = fmt.fmt.pix.height;
    }
    else
    {
        printf("can not set format!\r\n");
        goto done;
    }

    /* 获取数据 ：申请buffer*/
    struct v4l2_requestbuffers reqbuf;
    memset(&reqbuf, 0, sizeof(struct v4l2_requestbuffers));
    reqbuf.count = BUF_NUM;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd_video, VIDIOC_REQBUFS, &reqbuf); /* 申请buffer */
    if (ret < 0)
    {
        printf("can not alloc buffers\r\n");
        goto done;
    }
    c_dev->buffer_cnt = reqbuf.count;

    /* 申请成功之后查询buffer信息 */
    struct v4l2_buffer buf; /* 该结构体存储Buffer信息 */
    memset(&buf, 0, sizeof(struct v4l2_buffer));
    for (int i = 0; i < c_dev->buffer_cnt; i++)
    {
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd_video, VIDIOC_QUERYBUF, &buf);
        if (ret == 0) /* 查询缓冲区信息成功则进行mmap映射 */
        {
            c_dev->addr_buf[i] = mmap(0, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_video, buf.m.offset);
            c_dev->videobuf_maxlen = buf.length;
            if (c_dev->addr_buf[i] == MAP_FAILED)
            {
                perror("Unable to map buffer");
                ret = -1;
                goto done;
            }
        }
        else
        {
            printf("can not query buffer[%d]", i);
            goto done;
        }
    }
    printf("map %d buffers success\n", reqbuf.count);

    /* 将Buffer放入空闲链表 所有Buffer此时都空闲*/
    for (int i = 0; i < c_dev->buffer_cnt; i++)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd_video, VIDIOC_QBUF, &buf);
        if (ret != 0)
        {
            perror("Unable to queue buffer");
            goto done;
        }
    }
    printf("queue buffer success!\r\n");
    c_dev->v_ops = &my_vops;
    return 0;

done:
    close(fd_video);
    return ret;
}

int my_exit_device(struct camera_device *c_dev)
{
    int i;
    for (i = 0; i < c_dev->buffer_cnt; i++)
    {
        if (c_dev->addr_buf[i])
        {
            munmap(c_dev->addr_buf[i], c_dev->videobuf_maxlen);
            c_dev->addr_buf[i] = NULL;
        }
    }
    close(c_dev->fd_video);
    return 0;
}

int my_get_frame(struct camera_device *c_dev, struct videobuf *vbuf)
{
    int ret;
    struct pollfd fds[1];
    fds[0].fd = c_dev->fd_video;
    fds[0].events = POLLIN;

    ret = poll(fds, 1, -1);
    if (ret == 1)
    {
        /* 把buffer取出队列 */
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        // buf.index = 1;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(c_dev->fd_video, VIDIOC_DQBUF, &buf);
        if (ret < 0)
        {
            perror("Unable to dequeue buffer");
            return ret;
        }
        c_dev->buf_idx_cur = buf.index;

        /* 设置videobuf */
        vbuf->pixelformat = c_dev->pixelformat;
        vbuf->pdata.iWidth = c_dev->width;
        vbuf->pdata.iHeight = c_dev->height;
        vbuf->pdata.iBpp = (c_dev->pixelformat == V4L2_PIX_FMT_YUYV) ? 16 : (c_dev->pixelformat == V4L2_PIX_FMT_MJPEG) ? 0
                                                                        : (c_dev->pixelformat == V4L2_PIX_FMT_RGB565)  ? 16
                                                                                                                       : 0;
        vbuf->pdata.iLineBytes = c_dev->width * vbuf->pdata.iBpp / 8;
        vbuf->pdata.iTotalBytes = buf.bytesused;
        vbuf->pdata.aucPixelDatas = c_dev->addr_buf[c_dev->buf_idx_cur];

        /* 把buffer存到空闲链表 */
    }

    return 0;
}

int my_put_frame(struct camera_device *c_dev, struct videobuf *vbuf)
{
    int ret;
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.index = c_dev->buf_idx_cur;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(c_dev->fd_video, VIDIOC_QBUF, &buf);
    if (ret != 0)
    {
        perror("Unable to queue buffer");
        return -1;
    }
    return 0;
}

int my_start_device(struct camera_device *c_dev)
{
    int ret;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(c_dev->fd_video, VIDIOC_STREAMON, &type);
    if (ret < 0)
    {
        printf("start camera error!\r\n");
        return ret;
    }
    printf("start camera success!\r\n");
    return 0;
}

int my_stop_device(struct camera_device *c_dev)
{
    int ret;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(c_dev->fd_video, VIDIOC_STREAMOFF, &type);
    if (ret < 0)
    {
        printf("stop camera error!\r\n");
        return ret;
    }
    printf("stop camera success!\r\n");
    return 0;
}

static struct video_operations my_vops = {
    .name = "v4l2",
    .init_device = my_init_device,
    .exit_device = my_exit_device,
    .get_frame = my_get_frame,
    .put_frame = my_put_frame,
    .start_device = my_start_device,
    .stop_device = my_stop_device,
};

int v4l2_init(void)
{
    int ret = register_video_ops(&my_vops);
    return ret;
}
