/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef CNX_MoviePlayer_H
#define CNX_MoviePlayer_H

#include <QTime>
#include <QDebug>
#include "CNX_Util.h"
#include "NX_MediaInfo.h"
#include "CNX_Discover.h"

#include <gst/gst.h>
#include <glib.h>
#include <gst/gstdebugutils.h>
#include <gst/gstpad.h>
#include <gst/app/gstappsink.h>

typedef struct DSP_RECT {
    int32_t     iX;
    int32_t     iY;
    int32_t     iWidth;
    int32_t     iHeight;
} DSP_RECT;

typedef struct _MovieData {
	GstElement *source;

	GstElement *filesrc_typefind;

	GstElement *audio_queue;
	GstElement *video_queue;

	GstElement *demuxer;
	GstElement *video_parser;
	GstElement *nxdecoder;
	GstElement *nxvideosink;

	GstElement *decodebin;
	GstElement *audioconvert;
	GstElement *audioresample;
	GstElement *autoaudiosink;

	GstElement *volume;
} MovieData;

class CNX_MoviePlayer
{

public:
	CNX_MoviePlayer();
	~CNX_MoviePlayer();

public:
	//
	//MediaPlayer commomn Initialize , close
	//mediaType is MP_TRACK_VIDEO or MP_TRACK_AUDIO
	int InitMediaPlayer(	void (*pCbEventCallback)( void *privateDesc, unsigned int EventType, unsigned int /*EventData*/, unsigned int /*param*/ ),
							void *pCbPrivate,
							const char *pUri,
							int mediaType,
							int DspWidth,
							int DspHeight,
							char *pAudioDeviceName,
							void (*pCbQtUpdateImg)(void *pImg)
							);
    int SetupGStreamer(void (*pCbEventCallback)(void *privateDesc, unsigned int EventType, unsigned int EventData, unsigned int param),
                       const char *uri, int DspWidth, int DspHeight);

	int CloseHandle();

	//MediaPlayer common Control
	int SetVolume(int volume);
	int Play();
	int Seek(qint64 position);
	int Pause();
	int Stop();

	//MediaPlayer common information
    void PrintMediaInfo(const char* pUri);
	qint64 GetMediaPosition();
	qint64 GetMediaDuration();

    //NX_MediaStatus GetState();
    GstState GetState();

	//MediaPlayer video information
	void DrmVideoMute(int bOnOff);

    void registerCb(void (*pCbEventCallback)(void *, unsigned int EventType, unsigned int EventData, unsigned int param));

private:
	//
	//MediaPlayer InitMediaPlayer
	int OpenHandle( void (*pCbEventCallback)( void *privateDesc, unsigned int EventType, unsigned int /*EventData*/, unsigned int /*param*/ ),
					void *cbPrivate );
	int GetMediaInfo(const char* uri);

	void GetAspectRatio(int srcWidth, int srcHeight,
						int dspWidth, int dspHeight,
                        DSP_RECT *pDspDstRect);

    int initialize();
    int deinitialize();

	//
	//vars
	bool    debug;
	pthread_mutex_t	m_hLock;

    GMainLoop *m_Loop;
    GstBus *m_Bus;
    guint m_WatchId;
	GstElement *m_Pipeline;

	MovieData m_data;

    int m_X, m_Y, m_Width, m_Height;

	int				m_iMediaType;
	int             m_bVideoMute;

	GST_MEDIA_INFO	m_MediaInfo;
    DSP_RECT m_dstDspRect;

    CNX_Discover    *m_pDiscover;

	char			*m_pAudioDeviceName;

public:
	int IsCbQtUpdateImg();
};

#endif // CNX_MoviePlayer_H
