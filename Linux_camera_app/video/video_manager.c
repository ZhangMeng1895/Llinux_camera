
// #include <config.h>
#include "video_manager.h"
#include <string.h>
#include <stdio.h>

struct video_operations *vops_head = NULL;

/* 注册video_ops结构体 放入链表 */
int register_video_ops(struct video_operations *v_ops)
{
    struct video_operations *vops;

    if (!vops_head)
    {
        vops_head = v_ops;
        v_ops->next = NULL;
    }
    else
    {
        vops = vops_head;
        while (vops->next)
        {
            vops = vops->next;
        }
        vops->next = v_ops;
        v_ops->next = NULL;
    }
    return 0;
}

/* 遍历链表 */
void show_video_ops(void)
{
    int i = 0;
    struct video_operations *ptTmp = vops_head;

    while (ptTmp)
    {
        printf("%02d %s\n", i++, ptTmp->name);
        ptTmp = ptTmp->next;
    }
}

/* 通过传入的name找到对应的video_ops结构体 */
struct video_operations *get_video_ops(char *name)
{
    struct video_operations *ptTmp = vops_head;

    while (ptTmp)
    {
        if (strcmp(ptTmp->name, name) == 0)
        {
            return ptTmp;
        }
        ptTmp = ptTmp->next;
    }
    return NULL;
}

/* 初始化摄像头设备 */
int camera_device_init(char *strDevName, struct camera_device *c_dev)
{
    int iError;
    struct video_operations *vops = vops_head;

    while (vops)
    {
        iError = vops->init_device(strDevName, c_dev);
        if (!iError)
        {
            return 0;
        }
        vops = vops->next;
    }
    return -1;
}

/* 模块初始化 */
int video_init(void)
{
    int iError;

    iError = v4l2_init();

    return iError;
}
