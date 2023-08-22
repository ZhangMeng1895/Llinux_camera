#ifndef __V_STREAM__H
#define __V_STREAM__H

#define RC_UNDEFINED 0x00
#define SET_CUR 0x01
#define GET_CUR 0x81
#define GET_MIN 0x82
#define GET_MAX 0x83
#define GET_RES 0x84
#define GET_LEN 0x85
#define GET_INFO 0x86
#define GET_DEF 0x87

/* A.9.7. VideoStreaming Interface Control Selectors */
#define VS_CONTROL_UNDEFINED 0x00
#define VS_PROBE_CONTROL 0x01
#define VS_COMMIT_CONTROL 0x02
#define VS_STILL_PROBE_CONTROL 0x03
#define VS_STILL_COMMIT_CONTROL 0x04
#define VS_STILL_IMAGE_TRIGGER_CONTROL 0x05
#define VS_STREAM_ERROR_CODE_CONTROL 0x06
#define VS_GENERATE_KEY_FRAME_CONTROL 0x07
#define VS_UPDATE_FRAME_SEGMENT_CONTROL 0x08
#define VS_SYNC_DELAY_CONTROL 0x09

/* Values for bmHeaderInfo (Video and Still Image Payload Headers, 2.4.3.3) */
#define UVC_STREAM_EOH (1 << 7)
#define UVC_STREAM_ERR (1 << 6)
#define UVC_STREAM_STI (1 << 5)
#define UVC_STREAM_RES (1 << 4)
#define UVC_STREAM_SCR (1 << 3)
#define UVC_STREAM_PTS (1 << 2)
#define UVC_STREAM_EOF (1 << 1)
#define UVC_STREAM_FID (1 << 0)

struct myuvc_streaming_ctrl
{
    __u16 bmHint;
    __u8 bFormatIndex;
    __u8 bFrameIndex;
    __u32 dwFrameInterval;
    __u16 wKeyFrameRate;
    __u16 wPFrameRate;
    __u16 wCompQuality;
    __u16 wCompWindowSize;
    __u16 wDelay;
    __u32 dwMaxVideoFrameSize;
    __u32 dwMaxPayloadTransferSize;
    __u32 dwClockFrequency;
    __u8 bmFramingInfo;
    __u8 bPreferedVersion;
    __u8 bMinVersion;
    __u8 bMaxVersion;
};

void myuvc_print_streaming_params(struct myuvc_streaming_ctrl *ctrl);

int myuvc_get_streaming_params(struct myuvc_streaming_ctrl *ctrl);

int myuvc_try_streaming_params(struct myuvc_streaming_ctrl *ctrl);

int myuvc_set_streaming_params(struct myuvc_streaming_ctrl *ctrl);

int myuvc_alloc_init_urbs(struct myuvc_streaming_ctrl *ctrl);

void myuvc_uninit_urbs(void);

#endif