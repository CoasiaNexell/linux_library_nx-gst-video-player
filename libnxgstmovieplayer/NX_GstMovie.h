#ifndef NX_MEDIAINFO_H
#define NX_MEDIAINFO_H

#include <glib.h>

#define PROGRAM_MAX			16
#define MAX_TRACK_NUM		10

typedef struct MOVIE_TYPE	*MP_HANDLE;

typedef enum {
	MP_EVENT_EOS,
	MP_EVENT_DEMUX_LINK_FAILED,
	MP_EVENT_NOT_SUPPORTED,
	MP_EVENT_GST_ERROR,
	MP_EVENT_STATE_CHANGED,
	MP_EVENT_NUMS
} NX_GST_EVENT;

typedef enum {
	NX_GST_RET_ERROR,
	NX_GST_RET_OK
} NX_GST_RET;

typedef enum
{
	NX_GST_ERROR_NONE,
	NX_GST_ERROR_DISCOVER_FAILED,
	NX_GST_ERROR_NOT_SUPPORTED_CONTENTS,
	NX_GST_ERROR_DEMUX_LINK_FAILED,
	NX_GST_ERROR_NUM_ERRORS	
} NX_GST_ERROR;

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

struct GST_PROGRAM_INFO {
	gint32			iAudioNum;      // total audio number
	gint32			iVideoNum;
	gint32			iSubTitleNum;
	gint32			iDataNum;
	gint64			iDuration;
	GST_TRACK_INFO 	TrackInfo[MAX_TRACK_NUM];
};

typedef enum
{
	URI_TYPE_FILE,
	URI_TYPE_URL
} NX_URI_TYPE;

typedef enum
{
	MP_STATE_VOID_PENDING	= 0,
	MP_STATE_STOPPED		= 1,
	MP_STATE_READY			= 2,
	MP_STATE_PAUSED 		= 3,
	MP_STATE_PLAYING		= 4,
} NX_MEDIA_STATE;

typedef struct GST_MEDIA_INFO {
	gchar*          container_format;
	gchar*          video_mime_type;
	gchar*          audio_mime_type;
	gint32          iWidth;
	gint32          iHeight;
	gint32          iX;
	gint32          iY;
	gint32			video_mpegversion;
	gint32			audio_mpegversion;
	gboolean        isSeekable;
	gint64          iDuration;
	NX_URI_TYPE		uriType;
} GST_MEDIA_INFO;

#define TOPOLOGY_TYPE_CONTAINER		"container"
#define TOPOLOGY_TYPE_VIDEO			"video"
#define TOPOLOGY_TYPE_AUDIO			"audio"

typedef struct DSP_RECT {
    int32_t     iX;
    int32_t     iY;
    int32_t     iWidth;
    int32_t     iHeight;
} DSP_RECT;

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

NX_GST_RET NX_GSTMP_SetUri(MP_HANDLE handle, const char *pUri);
NX_GST_RET NX_GSTMP_Open(MP_HANDLE *handle,
					           void (*cb)(void *owner, unsigned int msg,
				   				          unsigned int param1, unsigned int param2),
				   			   void *cbOwner);
void NX_GSTMP_Close(MP_HANDLE handle);
NX_GST_RET NX_GSTMP_GetMediaInfo(MP_HANDLE handle, GST_MEDIA_INFO *pInfo);
NX_GST_RET NX_GSTMP_SetDisplayInfo(MP_HANDLE handle, int dspWidth, int dspHeight, DSP_RECT rect);
NX_GST_RET NX_GSTMP_Play(MP_HANDLE handle);
NX_GST_RET NX_GSTMP_Pause(MP_HANDLE hande);
NX_GST_RET NX_GSTMP_Stop(MP_HANDLE hande);
NX_GST_RET NX_GSTMP_Seek(MP_HANDLE hande, gint64 seekTime);
gint64 NX_GSTMP_GetDuration(MP_HANDLE handle);
gint64 NX_GSTMP_GetPosition(MP_HANDLE handle);
NX_GST_RET NX_GSTMP_SetVolume(MP_HANDLE handle, int volume);
NX_MEDIA_STATE NX_GSTMP_GetState(MP_HANDLE handle);
NX_GST_RET NX_GSTMP_VideoMute(MP_HANDLE handle, int32_t bOnoff);

const char* get_nx_media_state(NX_MEDIA_STATE state);
const char* get_nx_gst_event(NX_GST_EVENT event);
const char* get_nx_gst_error(NX_GST_ERROR error);
#ifdef __cplusplus
}
#endif


#endif // NX_MEDIAINFO_H

