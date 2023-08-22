#ifndef __VIDEO_H
#define __VIDEO_H

struct frame_desc
{
    int width;
    int height;
};

/* 参考uvc_video_queue定义结构体*/
struct myuvc_buffer
{
    struct v4l2_buffer vbuff;
    enum videobuf_state state; /* buffer状态 */
    int vma_use_count;         /* 表示Bufffer被mmap的次数 */
    wait_queue_head_t wait;    /* 等待队列头 若app在缓冲区空时读取则休眠*/
    struct list_head irq;      /* 链表头 */
    struct list_head stream;   /* 两个链表 */
};

struct myuvc_queue
{
    void *mem;     /* buffer内存的起始地址 */
    int count;     /* 这个队列里的buffer数目 应该<=32 */
    int buff_size; /* 缓冲区的大小 */
    struct myuvc_buffer buffer[32];

    struct urb *urb[32];
    char *urb_buffer[32];
    dma_addr_t urb_dma[32];
    unsigned int urb_size;

    struct list_head mainqueue; /* 链表头 */
    struct list_head irqqueue;  /* 两个链表 */
};

enum uvc_buffer_state
{
    UVC_BUF_STATE_IDLE = 0,
    UVC_BUF_STATE_QUEUED = 1,
    UVC_BUF_STATE_ACTIVE = 2,
    UVC_BUF_STATE_READY = 3,
    UVC_BUF_STATE_DONE = 4,
    UVC_BUF_STATE_ERROR = 5,
};

#endif