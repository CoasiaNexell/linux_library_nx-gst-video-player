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
//	Module		: libnxgstvplayer.so
//	File		:
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#ifndef __NX_GSTTYPES_H
#define __NX_GSTTYPES_H

#include <glib.h>

#define MAX_TRACK_NUM		10

typedef struct MOVIE_TYPE	*MP_HANDLE;

/*! \enum NX_GST_EVENT
 * \brief Describes the event types */
enum NX_GST_EVENT {
    /*! \brief EOS event */
    MP_EVENT_EOS                = 0,
    /*! \brief Demux error */
    MP_EVENT_DEMUX_LINK_FAILED  = 1,
    /*! \brief Not supported contents */
    MP_EVENT_NOT_SUPPORTED      = 2,
    /*! \brief General error from GStreamer */
    MP_EVENT_GST_ERROR          = 3,
    /*! \brief State is changed */
    MP_EVENT_STATE_CHANGED      = 4,
    /*! \brief Subtitle is updated */
    MP_EVENT_SUBTITLE_UPDATED   = 5,
    /*! \brief Unknown error   */
    MP_EVENT_UNKNOWN            = 6
};

/*! \enum NX_GST_RET
 * \brief Describes the return result */
typedef enum {
    /*! \brief On failure */
    NX_GST_RET_ERROR,
    /*! \brief On Success */
    NX_GST_RET_OK
} NX_GST_RET;

/*! \enum NX_URI_TYPE
 * \brief Describes the URI types */
typedef enum
{
    /*! \brief File type */
    URI_TYPE_FILE,
    /*! \brief URL type */
    URI_TYPE_URL
} NX_URI_TYPE;

/*! \enum NX_MEDIA_STATE
 * \brief Describes the media states */
enum NX_MEDIA_STATE
{
    /*! \brief No pending state */
    MP_STATE_VOID_PENDING	= 0,
    /*! \brief Stopped state or initial state */
    MP_STATE_STOPPED		= 1,
    /*! \brief Ready to go to PAUSED state */
    MP_STATE_READY			= 2,
    /*! \brief Paused state */
    MP_STATE_PAUSED 		= 3,
    /*! \brief Playing state */
    MP_STATE_PLAYING		= 4,
};

/*! \struct DSP_RECT
 * \brief Describes the information of display rectangle */
struct DSP_RECT {
    /*! \brief The X-coordinate of the left side of the rectangle */
    int32_t     left;
    /*! \brief The Y-coordinate of the top of the rectangle */
    int32_t     top;
    /*! \brief The X-coordinate of the right side of the rectangle */
    int32_t     right;
    /*! \brief The Y-coordinate of the bottom of the rectangle */
    int32_t     bottom;
};

/*! \enum SUBTITLE_INFO
 * \brief Describes the subtitle information */
struct SUBTITLE_INFO {
    /*! \brief PTS time */
    gint64 startTime;
    /*! \brief startTime + duration */
    gint64 endTime;
    /*! \brief duration time */
    gint64 duration;
    /*! \brief Subtitle texts */
    gchar*	subtitleText;
};

/*! \enum DISPLAY_MODE
 * \brief Describes the display mode */
enum DISPLAY_MODE {
    /*! \brief If only primary LCD is supported */
    DISPLAY_MODE_LCD_ONLY   = 0,
    /*! \brief If only secondary HDMI display is supported */
    DISPLAY_MODE_HDMI_ONLY  = 1,
    /*! \brief If both the primary LCD and the secondary HDMI display are supported */
    DISPLAY_MODE_LCD_HDMI   = 2,
    /*! \brief Unknown */
    DISPLAY_MODE_NONE       = 3
};

/*! \enum DISPLAY_TYPE
 * \brief Describes the display type */
enum DISPLAY_TYPE {
    /*! \brief If the display type is primary */
    DISPLAY_TYPE_PRIMARY,
    /*! \brief If the display type is secondary */
    DISPLAY_TYPE_SECONDARY
};

/*! \enum DEMUX_TYPE
 * \brief Describes demux type */
typedef enum {
    /*! \brief unknown */
    DEMUX_TYPE_UNKNOWN = -1,
    /*! \brief mpegtsdemux */
    DEMUX_TYPE_MPEGTSDEMUX,
    /*! \brief qtdemux */
    DEMUX_TYPE_QTDEMUX,
    /*! \brief oggdemux */
    DEMUX_TYPE_OGGDEMUX,
    /*! \brief rmdemux */
    DEMUX_TYPE_RMDEMUX,
    /*! \brief avidemux */
    DEMUX_TYPE_AVIDEMUX,
    /*! \brief asfdemux */
    DEMUX_TYPE_ASFDEMUX,
    /*! \brief matroskademux */
    DEMUX_TYPE_MATROSKADEMUX,
    /*! \brief flvdemux */
    DEMUX_TYPE_FLVDEMUX,
    /*! \brief mpegdemux */
    DEMUX_TYPE_MPEGDEMUX,
    /*! \brief dvdemux */
    DEMUX_TYPE_DVDEMUX,
    /*! \brief wavparse */
    DEMUX_TYPE_WAVPARSE
} DEMUX_TYPE;

typedef enum {
    CONTAINER_TYPE_MPEGTS,
    CONTAINER_TYPE_QUICKTIME,
    CONTAINER_TYPE_MSVIDEO,
    CONTAINER_TYPE_ASF,
    CONTAINER_TYPE_MATROSKA,
    CONTAINER_TYPE_MPEG,
    CONTAINER_TYPE_3GP,
    CONTAINER_TYPE_FLV,
    CONTAINER_TYPE_OGG,
    CONTAINER_TYPE_REALMEDIA,
    CONTAINER_TYPE_DV,
    CONTAINER_TYPE_ANNODEX,
    CONTAINER_TYPE_WAV,
    CONTAINER_TYPE_UNKNOWN,
} CONTAINER_TYPE;

/*! \enum VIDEO_TYPE
 * \brief Describes a video codec type */
typedef enum {
    /*! \brief H.263 */
    VIDEO_TYPE_H263,
    /*! \brief H.264 */
    VIDEO_TYPE_H264,
    /*! \brief MPEG v4 */
    VIDEO_TYPE_MPEG_V4,
    /*! \brief MPEG v2 */
    VIDEO_TYPE_MPEG_V2,
    /*! \brief Divx */
    VIDEO_TYPE_DIVX,
    /*! \brief XVID */
    VIDEO_TYPE_XVID,
    /*! \brief FLV */
    VIDEO_TYPE_FLV,
    /*! \brief Real Video */
    VIDEO_TYPE_RV,
    /*! \brief ASF */
    VIDEO_TYPE_ASF,
    /*! \brief WMV */
    VIDEO_TYPE_WMV,
    /*! \brief Theora */
    VIDEO_TYPE_THEORA,
    /*! \brief Unknown */
    VIDEO_TYPE_UNKNOWN
} VIDEO_TYPE;

typedef enum {
    AUDIO_TYPE_RAW,
	AUDIO_TYPE_MPEG,
    AUDIO_TYPE_MPEG_V2,
	AUDIO_TYPE_MP3,
	AUDIO_TYPE_AAC,
	AUDIO_TYPE_WMA,
	AUDIO_TYPE_OGG,
	AUDIO_TYPE_AC3,
	AUDIO_TYPE_AC3_PRI,
	AUDIO_TYPE_FLAC,
	AUDIO_TYPE_RA,
	AUDIO_TYPE_DTS,
	AUDIO_TYPE_DTS_PRI,
	AUDIO_TYPE_WAV,
    AUDIO_TYPE_UNKNOWN,
} AUDIO_TYPE;

typedef enum {
    SUBTITLE_TYPE_RAW,
    SUBTITLE_TYPE_SSA,
    SUBTITLE_TYPE_ASS,
    SUBTITLE_TYPE_USF,
    SUBTITLE_TYPE_DVD,
    SUBTITLE_TYPE_DVB,
    SUBTITLE_TYPE_UNKNOWN,
} SUBTITLE_TYPE;

typedef struct {
    int32_t     stream_id;
    SUBTITLE_TYPE   type;
    char*       language_code;
} GST_SUBTITLE_INFO;

typedef struct {
    int32_t     stream_id;
    AUDIO_TYPE  type;
    int32_t     mpegversion;
    int32_t     mpegaudioversion;
    int32_t     n_channels;
    int32_t     samplerate;
    int32_t     bitrate;
} GST_AUDIO_INFO;

typedef struct {
    int32_t         stream_id;
    VIDEO_TYPE      type;
    int32_t         mpegversion;
    int32_t         width;
    int32_t         height;
    int32_t         framerate;
} GST_VIDEO_INFO;

/*! \def MAX_STREAM_INFO
 * \brief Maximum number of stream information */
#define	MAX_STREAM_INFO		20

/*! \struct _GST_STREAM_INFO
 * \brief Describes the stream information */
typedef struct _GST_STREAM_INFO {
    /*! \brief Total number of videos */
    int32_t             n_video;
    /*! \brief Total number of audio */
    int32_t             n_audio;
    /*! \brief Total number of subtitles */
    int32_t             n_subtitle;
    /*! \brief Total duration */
    int32_t             duration;
    /*! \brief If the content is seekable */
    int32_t             seekable;

    /*! \brief Video stream information */
    GST_VIDEO_INFO      VideoInfo[MAX_STREAM_INFO];
    /*! \brief Audio stream information */
    GST_AUDIO_INFO      AudioInfo[MAX_STREAM_INFO];
    /*! \brief Subtitle stream information */
    GST_SUBTITLE_INFO   SubtitleInfo[MAX_STREAM_INFO];
} GST_STREAM_INFO;

/*! \struct GST_MEDIA_INFO
 * \brief Describes the media information */
struct GST_MEDIA_INFO {
    /*! \brief Container format */
    CONTAINER_TYPE      container_type;
    /*! \brief Demux Type */
    DEMUX_TYPE          demux_type;
    /*! \brief Total number of programs */
    int32_t         n_program;
    /*! \brief The selected program number */
    int32_t         program_number;

    /*gint32			n_container;
    gint32			n_video;
    gint32			n_audio;
    gint32			n_subtitle;
    gboolean        isSeekable;
    gint64          iDuration;*/
    
    /*! \brief The information for each stream */
    GST_STREAM_INFO	StreamInfo[MAX_STREAM_INFO];

    /*gchar*          video_mime_type;
    gint32			video_mpegversion;*/

    struct DSP_RECT        dsp_rect;
    
    /*gint32          video_width;
    gint32          video_height;

    gchar*          audio_mime_type;
    gint32			audio_mpegversion;

    gchar*			subtitle_codec;*/

    /*! \brief URI type */
    NX_URI_TYPE		uriType;
};

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

#ifdef __cplusplus
}
#endif

#endif // __NX_GSTTYPES_H