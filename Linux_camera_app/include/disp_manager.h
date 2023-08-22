#ifndef _DISP_MA_H
#define _DISP_MA_H

#include "pic_operation.h"
#include "video_manager.h"

struct layout
{
    int top_left_x;
    int top_left_y;
    int bot_right_x;
    int bot_right_y;
    char *icon_name;
};

/* VideoMem的状态:
 * 空闲/用于预先准备显示内容/用于当前线程
 */
typedef enum
{
    VMS_FREE = 0,
    VMS_USED_FOR_PREPARE,
    VMS_USED_FOR_CUR,
} video_mem_state;

/* VideoMem中内存里图片的状态:
 * 空白/正在生成/已经生成
 */
typedef enum
{
    PS_BLANK = 0,
    PS_GENERATING,
    PS_GENERATED,
} video_pic_state;

struct video_mem
{
    int id;
    int bDevFrameBuffer;     /* 1: 这个VideoMem是显示设备的显存; 0: 只是一个普通缓存 */
    video_mem_state vmstate; /* 这个VideoMem的状态 */
    video_pic_state vpstate; /* VideoMem中内存里图片的状态 */
    struct PixelDatas pdata; /* 内存: 用来存储图像 */
    struct video_mem *next;  /* 链表 */
};

struct disp_operations
{
    char *name;
    int x;
    int y;
    int bpp;
    int line_bytes;
    unsigned char *addr_mem;                             /* 显存地址 */
    int (*device_init)(void);                            /* 显示设备初始化 */
    int (*clean_screen)(unsigned int back_color);        /* 清屏为某种颜色 */
    int (*show_pixel)(int x, int y, unsigned int color); /* 设置某点像素为某个颜色 */
    int (*show_page)(struct PixelDatas *pdata);          /* 显示一页 */
    struct disp_operations *next;                        /* 链表7 */
};

int register_disp_ops(struct disp_operations *ptDispOpr);

void show_disp_ops(void);

struct disp_operations *get_disp_ops(char *pcName);

void select_init_disp_dev(char *name);

struct disp_operations *get_default_disp_dev(void);

int get_dispdev_info(int *piXres, int *piYres, int *piBpp);

int GetVideoBufForDisplay(struct videobuf *ptFrameBuf);

void FlushPixelDatasToDev(struct PixelDatas *pdata);

int alloc_video_mem(int iNum);

struct video_mem *get_video_mem(int id, int bCur);

void put_video_mem(struct video_mem *ptVideoMem);

struct video_mem *get_dev_video_mem(void);

void clear_video_mem(struct video_mem *ptVideoMem, unsigned int dwColor);

void clear_video_mem_region(struct video_mem *ptVideoMem, struct layout *ptLayout, unsigned int dwColor);

int DisplayInit(void);

#endif
