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

#ifndef __NX_GSTIFACE_H
#define __NX_GSTIFACE_H

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

/*! \def MAX_STREAM_INFO
 * \brief Maximum number of stream information */
#define	MAX_STREAM_INFO		20

/*! \struct _GST_STREAM_INFO
 * \brief Describes the stream information */
struct _GST_STREAM_INFO {
    /*! \brief Total number of audio */
    gint32			iAudioNum;
    /*! \brief Total number of videos */
    gint32			iVideoNum;
    /*! \brief Total number of subtitles */
    gint32			iSubTitleNum;
    /*! \brief Total stream duration  */
    gint64			iDuration;
};
typedef struct _GST_STREAM_INFO	GST_STREAM_INFO;/*! \typedef GST_STREAM_INFO */

/*! \struct GST_MEDIA_INFO
 * \brief Describes the media information */
struct GST_MEDIA_INFO {
    /*! \brief Container type */
    gchar*          container_format;
    /*! \brief Total number of container */
    gint32			n_container;
    /*! \brief Total number of videos */
    gint32			n_video;
    /*! \brief Total number of audio */            
    gint32			n_audio;
    /*! \brief Total number of subtitles */
    gint32			n_subtitle;
    /*! \brief If the content is seekable */
    gboolean        isSeekable;
    /*! \brief Total stream duration */
    gint64          iDuration;
    /*! \brief The information for each stream */
    GST_STREAM_INFO	StreamInfo[MAX_STREAM_INFO];
    /*! \brief Video codec type */
    gchar*          video_mime_type;
    /*! \brief Video mpeg version */
    gint32			video_mpegversion;
    /*! \brief Display X position */
    gint32          iX;
    /*! \brief Display Y position */
    gint32          iY;
    /*! \brief Display Width */
    gint32          iWidth;
    /*! \brief Display Height */
    gint32          iHeight;
    /*! \brief Audio codec type */
    gchar*          audio_mime_type;
    /*! \brief Audio mpeg version */
    gint32			audio_mpegversion;
    /*! \brief Subtitle codec type */
    gchar*			subtitle_codec;
    /*! \brief URI type */
    NX_URI_TYPE		uriType;
};

/*! \struct DSP_RECT
 * \brief Describes the information of display rectangle */
struct DSP_RECT {
    /*! \brief Display X position */
    int32_t     iX;
    /*! \brief Display Y position */
    int32_t     iY;
    /*! \brief Display Width */
    int32_t     iWidth;
    /*! \brief Display Height */
    int32_t     iHeight;
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
    /*! \brief If both the primary LCD and secondary HDMI display are supported */
    DISPLAY_MODE_LCD_HDMI   = 1,
    /*! \brief Unknown */
    DISPLAY_MODE_UNKNOWN    = 2
};

/*! \enum DISPLAY_TYPE
 * \brief Describes the display type */
enum DISPLAY_TYPE {
    /*! \brief If the display type is primary */
    DISPLAY_TYPE_PRIMARY,
    /*! \brief If the display type is secondary */
    DISPLAY_TYPE_SECONDARY
};

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

/*!
 * \fn NX_GST_RET NX_GSTMP_SetDisplayMode(MP_HANDLE handle, enum DISPLAY_MODE in_mode);
 *
 * \brief This function is to set display mode.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  in_mode   Display mode
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_SetDisplayMode(MP_HANDLE handle, enum DISPLAY_MODE in_mode);

/*!
 * \fn NX_GST_RET NX_GSTMP_SetUri(MP_HANDLE handle, const char *pUri);
 *
 * \brief This function is to discover the URI and to get the basic media information
 * like container format, video codec, audio codec, video size, etc.
 * If the discovering process is done without any error, after configuring GStreamer pipeline,
 * it sets the state to ‘READY’.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  pUri      URI
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_SetUri(MP_HANDLE handle, const char *pUri);

/*!
 * \fn NX_GST_RET NX_GSTMP_Open(MP_HANDLE *handle,
 * void (*cb)(void *owner, unsigned int msg, unsigned int param1, void* param),
 * void *cbOwner);
 *
 * \brief This function is to allocate the required resources and register a callback.
 * GStreamer library is also initialized in this method.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  pUri      URI
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_Open(MP_HANDLE *handle,
        void (*cb)(void *owner, unsigned int msg, unsigned int param1, void* param),
        void *cbOwner);

/*!
 * \fn void NX_GSTMP_Close(MP_HANDLE handle);
 *
 * \brief This function is to deallocate all the resources allocated.
 *
 * \param [in]  handle    Movie player handle
 *
 */
void NX_GSTMP_Close(MP_HANDLE handle);

/*!
 * \fn NX_GST_RET NX_GSTMP_GetMediaInfo(MP_HANDLE handle, struct GST_MEDIA_INFO *pInfo);
 *
 * \brief This is used to get media information.
 * The application can use the media information to limit the codec types to support. And
 * it can calculate the display rect information according to aspect ratio.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  pInfo     Media information
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_GetMediaInfo(MP_HANDLE handle, struct GST_MEDIA_INFO *pInfo);

/*!
 * \fn NX_GST_RET NX_GSTMP_SetDisplayInfo(MP_HANDLE handle, enum DISPLAY_TYPE type,
 * int dspWidth, int dspHeight, struct DSP_RECT rect);
 *
 * \brief This is used to set the display rectangle information according to the each display type.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  pInfo     Media information
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_SetDisplayInfo(MP_HANDLE handle, enum DISPLAY_TYPE type,
                                    int dspWidth, int dspHeight, struct DSP_RECT rect);

/*!
 * \fn NX_GST_RET NX_GSTMP_Play(MP_HANDLE handle);
 *
 * \brief This is used to start playback.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_Play(MP_HANDLE handle);

/*!
 * \fn NX_GST_RET NX_GSTMP_Pause(MP_HANDLE handle);
 *
 * \brief This is used to pause playback.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_Pause(MP_HANDLE hande);

/*!
 * \fn NX_GST_RET NX_GSTMP_Stop(MP_HANDLE handle);
 *
 * \brief This is used to stop playback.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_Stop(MP_HANDLE hande);

/*!
 * \fn NX_GST_RET NX_GSTMP_Seek(MP_HANDLE hande, gint64 seekTime);
 *
 * \brief This is used to seek(jump) to a certain position(time).
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_Seek(MP_HANDLE hande, gint64 seekTime);

/*!
 * \fn gint64 NX_GSTMP_GetDuration(MP_HANDLE handle);
 *
 * \brief This is used to get the total stream duration in nanoseconds.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval The stream duration in nanoseconds
 */
int64_t NX_GSTMP_GetDuration(MP_HANDLE handle);

/*!
 * \fn gint64 NX_GSTMP_GetPosition(MP_HANDLE handle);
 *
 * \brief This is used to get the current stream position in nanoseconds.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval The stream duration in nanoseconds
 */
int64_t NX_GSTMP_GetPosition(MP_HANDLE handle);

/*!
 * \fn NX_GST_RET NX_GSTMP_SetVolume(MP_HANDLE handle, int volume);
 *
 * \brief This is not used.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  volume    volume
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_SetVolume(MP_HANDLE handle, int32_t volume);

/*!
 * \fn enum NX_MEDIA_STATE NX_GSTMP_GetState(MP_HANDLE handle);
 *
 * \brief This is used for getting the current state playback.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval MP_STATE_VOID_PENDING    
 * \retval MP_STATE_STOPPED         STOPPED state
 * \retval MP_STATE_READY           READY state
 * \retval MP_STATE_PAUSED          PAUSED state
 * \retval MP_STATE_PLAYING         PLAYING state
 */
enum NX_MEDIA_STATE NX_GSTMP_GetState(MP_HANDLE handle);

/*!
 * \fn NX_GST_RET NX_GSTMP_VideoMute(MP_HANDLE handle, int32_t bOnoff);
 *
 * \brief This is used to turn on/off the video mute.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_VideoMute(MP_HANDLE handle, int32_t bOnoff);

/*!
 * \fn NX_GST_RET NX_GSTMP_SetVideoSpeed(MP_HANDLE handle, double rate);
 *
 * \brief This is used to control the playback rate. It’s available in PAUSED or PLAYING state.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  rate      Playback speed rate
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_SetVideoSpeed(MP_HANDLE handle, double rate);

/*!
 * \fn double NX_GSTMP_GetVideoSpeed(MP_HANDLE handle);
 *
 * \brief This is used to get video playback speed rate.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval Video playback speed rate.
 */
double NX_GSTMP_GetVideoSpeed(MP_HANDLE handle);

/*!
 * \fn NX_GST_RET NX_GSTMP_GetVideoSpeedSupport(MP_HANDLE handle);
 *
 * \brief This is used to check if the media is seekable.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On success (Seekable media)
 */
NX_GST_RET NX_GSTMP_GetVideoSpeedSupport(MP_HANDLE handle);

/*!
 * \fn const char* NX_GSTMP_MakeThumbnail(const gchar *uri, int64_t pos_msec,
 * int32_t width, const char *outPath);
 *
 * \brief This is used to make thumbnail for a certain position.
 *
 * \param [in]  uri         URI
 * \param [in]  pos_msec    Video position to make thumbnail
 * \param [in]  width       Width of thumbnail to create
 * \param [in]  outPath     File path of thumbnail to create
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_MakeThumbnail(const gchar *uri, gint64 pos_msec, gint width, const char *outPath);

#ifdef __cplusplus
}
#endif

#endif // __NX_GSTIFACE_H