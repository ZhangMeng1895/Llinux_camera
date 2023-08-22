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

int BUF_NUM = 32;

int main(int argc, char *argv[])
{
    int fd_video, fd_file;
    struct v4l2_fmtdesc fmtdesc;
    int fmt_idx = 0;
    int frm_idx = 0;
    int ret;
    struct v4l2_frmsizeenum fsenum; /* 帧大小结构体 */
    void *bufs[BUF_NUM];            /* 用于记录映射的Buff地址 */
    struct pollfd fds[1];
    char filename[32];
    int file_count;

    if (argc != 2)
    {
        printf("invaild params!\r\n");
    }

    /* 打开设备 */
    fd_video = open(argv[1], O_RDWR);
    if (fd_video < 0)
    {
        printf("can not open %s\n", argv[1]);
        return -1;
    }

    while (1)
    {
        /* 枚举格式 */
        fmtdesc.index = fmt_idx++;
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret = ioctl(fd_video, VIDIOC_ENUM_FMT, &fmtdesc);
        if (ret != 0)
            break;

        /* 枚举该格式支持的帧大小 */
        while (1)
        {
            memset(&fsenum, 0, sizeof(struct v4l2_frmsizeenum));
            fsenum.pixel_format = fmtdesc.pixelformat;
            fsenum.index = frm_idx++;
            ret = ioctl(fd_video, VIDIOC_ENUM_FRAMESIZES, &fsenum);
            if (ret != 0)
                break;

            printf("format %s,%d,framesize %d x %d\n", fmtdesc.description, fmtdesc.pixelformat, fsenum.discrete.width, fsenum.discrete.height);
        }
    }

    /* 设置格式 */
    struct v4l2_format fmt;
    memset(&fmt, 9, sizeof(struct v4l2_format));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 1024;
    fmt.fmt.pix.height = 768;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    ret = ioctl(fd_video, VIDIOC_S_FMT, &fmt); /* 设置格式 如果传入分辨率不支持则驱动会设置成某种分辨率并保存在传入的结构体中 */
    if (ret == 0)
    {
        printf("set format : %d * %d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
    }

    /* 获取数据 ：申请buffer*/
    struct v4l2_requestbuffers reqbuf;
    memset(&reqbuf, 0, sizeof(struct v4l2_requestbuffers));
    reqbuf.count = BUF_NUM;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd_video, VIDIOC_REQBUFS, &reqbuf); /* 申请buffer */

    if (ret == 0) /* 申请成功 */
    {
        /* 申请成功之后查询buffer信息 */
        struct v4l2_buffer buf; /* 该结构体存储Buffer信息 */
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        for (int i = 0; i < reqbuf.count; i++)
        {
            buf.index = i;
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            ret = ioctl(fd_video, VIDIOC_QUERYBUF, &buf);
            if (ret == 0) /* 查询缓冲区信息成功则进行mmap映射 */
            {
                bufs[i] = mmap(0, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_video, buf.m.offset);
                if (bufs[i] == MAP_FAILED)
                {
                    perror("Unable to map buffer");
                    return -1;
                }
            }
            else
            {
                printf("can not query buffer[%d]", i);
                goto done;
            }
        }
        printf("map %d buffers success\n", reqbuf.count);
    }
    else
    {
        perror("Unable to allocate buffers");
        goto done;
    }

    /* 将Buffer放入空闲链表 所有Buffer此时都空闲*/
    for (int i = 0; i < reqbuf.count; i++)
    {
        struct v4l2_buffer buf;
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd_video, VIDIOC_QBUF, &buf);
        if (ret != 0)
        {
            perror("Unable to queue buffer");
            return -1;
        }
    }
    printf("queue buffer success!\r\n");

    /* 启动摄像头 */
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd_video, VIDIOC_STREAMON, &type);
    if (ret < 0)
    {
        printf("start camera error!\r\n");
        goto done;
    }
    printf("start camera success!\r\n");

    while (1)
    {
        /* poll */
        fds[0].fd = fd_video;
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
            ret = ioctl(fd_video, VIDIOC_DQBUF, &buf);
            if (ret < 0)
            {
                perror("Unable to dequeue buffer");
                goto done;
            }

            /* 把Buufer的数据存为文件 */
            sprintf(filename, "video_raw_data_%04d.jpg", file_count++);
            fd_file = open(filename, O_RDWR | O_CREAT, 0666);
            if (fd_file < 0)
            {
                perror("Unable to open file");
                goto done;
            }
            write(fd_file, bufs[buf.index], buf.bytesused);
            close(fd_file);

            /* 把buffer存到空闲链表 */
            ret = ioctl(fd_video, VIDIOC_QBUF, &buf);
            if (ret != 0)
            {
                perror("Unable to queue buffer");
                return -1;
            }
        }
    }

    ret = ioctl(fd_video, VIDIOC_STREAMOFF, &type);
    if (ret < 0)
    {
        printf("close camera error!\r\n");
        goto done;
    }
    close(fd_video);
done:
    return ret;
}