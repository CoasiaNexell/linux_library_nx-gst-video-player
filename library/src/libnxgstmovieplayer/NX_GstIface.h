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
#include "NX_GstTypes.h"

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