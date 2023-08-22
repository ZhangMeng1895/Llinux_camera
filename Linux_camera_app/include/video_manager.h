#ifndef _VIDEO_MANA_H
#define _VIDEO_MANA_H

#include <pic_operation.h>

#define BUF_NUM 32
// const int BUF_NUM = 32;

struct camera_device;
struct video_operations;

/* 这是一个摄像头设备结构体 */
struct camera_device
{
    int fd_video; /* 文件句柄 */
    int pixelformat;
    int width;
    int height;
    int buffer_cnt;
    int buf_idx_cur;
    unsigned char *addr_buf[BUF_NUM];
    struct video_operations *v_ops;
    int videobuf_maxlen;
};

struct videobuf
{
    struct PixelDatas pdata;
    int pixelformat;
};

/* 对摄像头的各种操作 */
struct video_operations
{
    char *name;
    int (*init_device)(char *device_name, struct camera_device *c_dev);
    int (*exit_device)(struct camera_device *c_dev);
    int (*get_frame)(struct camera_device *c_dev, struct videobuf *vbuf);
    int (*put_frame)(struct camera_device *c_dev, struct videobuf *vbuf);
    int (*start_device)(struct camera_device *c_dev);
    int (*stop_device)(struct camera_device *c_dev);
    struct video_operations *next;
};

/* 注册video_ops结构体 放入链表 */
int register_video_ops(struct video_operations *v_ops);

/* 遍历链表 */
void show_video_ops(void);

/* 通过传入的name找到对应的video_ops结构体 */
struct video_operations *get_video_ops(char *name);

/* 初始化摄像头设备 */
int camera_device_init(char *strDevName, struct camera_device *c_dev);

int video_init(void);

#endif
