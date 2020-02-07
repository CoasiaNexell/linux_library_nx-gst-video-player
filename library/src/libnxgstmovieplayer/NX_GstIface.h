//------------------------------------------------------------------------------
//
//	Copyright (C) 2015 Nexell Co. All Rights Reserved
//	Nexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		: libnxgstmovieplayer.so
//	File		:
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#ifndef __NX_GSTIFACE_H
#define __NX_GSTIFACE_H

#include <glib.h>

#define MAX_TRACK_NUM		10

typedef struct MOVIE_TYPE	*MP_HANDLE;

enum NX_GST_EVENT {
    MP_EVENT_EOS,
    MP_EVENT_DEMUX_LINK_FAILED,
    MP_EVENT_NOT_SUPPORTED,
    MP_EVENT_GST_ERROR,
    MP_EVENT_STATE_CHANGED,
    MP_EVENT_SUBTITLE_UPDATED,
    MP_EVENT_NUMS
};

typedef enum {
    NX_GST_RET_ERROR,
    NX_GST_RET_OK
} NX_GST_RET;

enum NX_GST_ERROR
{
    NX_GST_ERROR_NONE,
    NX_GST_ERROR_DISCOVER_FAILED,
    NX_GST_ERROR_NOT_SUPPORTED_CONTENTS,
    NX_GST_ERROR_DEMUX_LINK_FAILED,
    NX_GST_ERROR_NUM_ERRORS	
};

typedef enum
{
    URI_TYPE_FILE,
    URI_TYPE_URL
} NX_URI_TYPE;

enum NX_MEDIA_STATE
{
    MP_STATE_VOID_PENDING	= 0,
    MP_STATE_STOPPED		= 1,
    MP_STATE_READY			= 2,
    MP_STATE_PAUSED 		= 3,
    MP_STATE_PLAYING		= 4,
};

#define	MAX_STREAM_INFO		20

struct _GST_TRACK_INFO {
    gint32			iTrackIndex;	// Track Index
    gint32			iTrackType;		// MP_TRACK_VIDEO, ...
    gint32			iCodecId;
    gint64			iDuration;		// Track Duration

    gboolean        bIsSeekable;

    gint32			iWidth;			// Only VideoTrack
    gint32			iHeight;		// Only VideoTrack
    gint32			iFrameRate;		// Only VideoTrack

    gint32			iChannels;		// Only AudioTrack
    gint32			iSampleRate;	// Only AudioTrack
    gint32			iBitrate;		// Only AudioTrack
};

typedef struct _GST_TRACK_INFO	GST_TRACK_INFO;

struct _GST_STREAM_INFO {
    gint32			iAudioNum;      // total audio number
    gint32			iVideoNum;
    gint32			iSubTitleNum;
    gint32			iDataNum;
    gint64			iDuration;
    GST_TRACK_INFO 	TrackInfo[MAX_TRACK_NUM];
};
typedef struct _GST_STREAM_INFO	GST_STREAM_INFO;

struct GST_MEDIA_INFO {
    gchar*          container_format;

    gint32			n_container;
    gint32			n_video;
    gint32			n_audio;
    gint32			n_subtitle;

    gboolean        isSeekable;
    gint64          iDuration;

    GST_STREAM_INFO	StreamInfo[MAX_STREAM_INFO];

    gint32          iX;
    gint32          iY;

    gchar*          video_mime_type;
    gint32			video_mpegversion;
    gint32          iWidth;
    gint32          iHeight;

    gchar*          audio_mime_type;
    gint32			audio_mpegversion;

    gchar*			subtitle_codec;

    NX_URI_TYPE		uriType;
};

struct DSP_RECT {
    int32_t     iX;
    int32_t     iY;
    int32_t     iWidth;
    int32_t     iHeight;
};

struct SUBTITLE_INFO {
    gint64 startTime;
    gint64 endTime;
    gint64 duration;
    char*	subtitleText;
};

enum DISPLAY_MODE {
    DISPLAY_MODE_LCD_ONLY,
    DISPLAY_MODE_LCD_HDMI,
    DISPLAY_MODE_UNKNOWN
};

enum DISPLAY_TYPE {
    DISPLAY_TYPE_PRIMARY,
    DISPLAY_TYPE_SECONDARY
};

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

NX_GST_RET NX_GSTMP_SetDisplayMode(MP_HANDLE handle, enum DISPLAY_MODE in_mode);
NX_GST_RET NX_GSTMP_SetUri(MP_HANDLE handle, const char *pUri);
NX_GST_RET NX_GSTMP_Open(MP_HANDLE *handle,
                               void (*cb)(void *owner, unsigned int msg,
                               unsigned int param1, void* param),
                                  void *cbOwner);
void NX_GSTMP_Close(MP_HANDLE handle);
NX_GST_RET NX_GSTMP_GetMediaInfo(MP_HANDLE handle, struct GST_MEDIA_INFO *pInfo);
NX_GST_RET NX_GSTMP_SetDisplayInfo(MP_HANDLE handle, enum DISPLAY_TYPE type,
                                    int dspWidth, int dspHeight, struct DSP_RECT rect);
NX_GST_RET NX_GSTMP_Play(MP_HANDLE handle);
NX_GST_RET NX_GSTMP_Pause(MP_HANDLE hande);
NX_GST_RET NX_GSTMP_Stop(MP_HANDLE hande);
NX_GST_RET NX_GSTMP_Seek(MP_HANDLE hande, gint64 seekTime);
gint64 NX_GSTMP_GetDuration(MP_HANDLE handle);
gint64 NX_GSTMP_GetPosition(MP_HANDLE handle);
NX_GST_RET NX_GSTMP_SetVolume(MP_HANDLE handle, int volume);
enum NX_MEDIA_STATE NX_GSTMP_GetState(MP_HANDLE handle);
NX_GST_RET NX_GSTMP_VideoMute(MP_HANDLE handle, int32_t bOnoff);
NX_GST_RET NX_GSTMP_SetVideoSpeed(MP_HANDLE handle, gdouble speed);
gdouble NX_GSTMP_GetVideoSpeed(MP_HANDLE handle);
gboolean NX_GSTMP_GetVideoSpeedSupport(MP_HANDLE handle);
const char* NX_GSTMP_GetThumbnail(const gchar *uri, gint64 pos_msec, gint width);

const char* get_nx_media_state(enum NX_MEDIA_STATE state);

#ifdef __cplusplus
}
#endif

#endif // __NX_GSTIFACE_H