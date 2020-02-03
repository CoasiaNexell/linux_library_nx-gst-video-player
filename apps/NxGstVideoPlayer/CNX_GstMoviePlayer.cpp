#include "CNX_GstMoviePlayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define LOG_TAG "[CNX_GstMoviePlayer]"
#include <NX_Log.h>
#include <math.h>
#include <media/CNX_Base.h>

//------------------------------------------------------------------------------
CNX_GstMoviePlayer::CNX_GstMoviePlayer(QWidget *parent)
    : debug(false)
    , m_hPlayer(NULL)
	, m_pAudioDeviceName(NULL)
	, m_fSpeed(1.0)
	, m_pSubtitleParser(NULL)
	, m_iSubtitleSeekTime(0)
	, m_bIsSecDis(false)
	, m_iSecDspWidth(1920)
	, m_iSecDspHeight(1080)
{
	pthread_mutex_init(&m_hLock, NULL);
	pthread_mutex_init(&m_SubtitleLock, NULL);

    memset(&m_MediaInfo, 0, sizeof(GST_MEDIA_INFO));

	// Subtitle
	m_pSubtitleParser = new CNX_SubtitleParser();
}

CNX_GstMoviePlayer::~CNX_GstMoviePlayer()
{
	pthread_mutex_destroy(&m_hLock);
	pthread_mutex_destroy(&m_SubtitleLock);
	if(m_pSubtitleParser) {
		delete m_pSubtitleParser;
		m_pSubtitleParser = NULL;
	}
}

//================================================================================================================
//public methods	commomn Initialize , close
int CNX_GstMoviePlayer::InitMediaPlayer(void (*pCbEventCallback)(void *privateDesc,
                                                                 unsigned int EventType,
                                                                 unsigned int EventData,
                                                                 void* param),
                                     void *pCbPrivate,
                                     const char *pUri,
									 DISPLAY_INFO dspInfo,
                                     char *pAudioDeviceName)
{
	CNX_AutoLock lock( &m_hLock );

	NXLOGI("%s", __FUNCTION__);

	m_pAudioDeviceName = pAudioDeviceName;

	if(0 > OpenHandle(pCbEventCallback, pCbPrivate))		return -1;
	if(0 > SetDisplayMode(dspInfo.dspMode))					return -1;
	if(0 > SetUri(pUri))									return -1;
	if(0 > GetMediaInfo())									return -1;
	if(0 > SetAspectRatio(dspInfo))							return -1;

	return 0;
}

int CNX_GstMoviePlayer::SetAspectRatio(DISPLAY_INFO dspInfo)
{
	DSP_RECT m_dstDspRect;
	DSP_RECT m_dstSubDspRect;

	NXLOGI("%s() dspInfo(%d, %d, %d, %d, %d)",
			__FUNCTION__, dspInfo.dspWidth, dspInfo.dspHeight,
			dspInfo.dspMode, dspInfo.subDspWidth, dspInfo.subDspHeight);

	// Set aspect ratio for the primary display
	memset(&m_dstDspRect, 0, sizeof(DSP_RECT));
	GetAspectRatio(m_MediaInfo.iWidth, m_MediaInfo.iHeight,
				   dspInfo.dspWidth, dspInfo.dspHeight,
				   &m_dstDspRect);
	if (0 > SetDisplayInfo(DISPLAY_TYPE_PRIMARY, dspInfo.dspWidth, dspInfo.dspHeight, m_dstDspRect)) {
		NXLOGE("%s() Failed to set aspect ratio rect for primary", __FUNCTION__);
		return -1;
	}
	NXLOGI("%s() m_dstDspRect(%d, %d, %d, %d)",
			__FUNCTION__, m_dstDspRect.iX, m_dstDspRect.iY,
			m_dstDspRect.iWidth, m_dstDspRect.iHeight);

	// Set aspect ratio for the secondary display
	if (dspInfo.dspMode == DISPLAY_MODE_LCD_HDMI)
	{
		memset(&m_dstSubDspRect, 0, sizeof(DSP_RECT));
		GetAspectRatio(m_MediaInfo.iWidth, m_MediaInfo.iHeight,
						dspInfo.subDspWidth, dspInfo.subDspHeight,
						&m_dstSubDspRect);
		if(0 > SetDisplayInfo(DISPLAY_TYPE_SECONDARY, dspInfo.subDspWidth, dspInfo.subDspHeight, m_dstSubDspRect)) {
			NXLOGE("%s() Failed to set aspect ratio rect for secondary", __FUNCTION__);
			return -1;
		}
	}
	NXLOGI("%s() m_dstSubDspRect(%d, %d, %d, %d)",
			__FUNCTION__, m_dstSubDspRect.iX, m_dstSubDspRect.iY,
			m_dstSubDspRect.iWidth, m_dstSubDspRect.iHeight);
	return 0;
}

int CNX_GstMoviePlayer::CloseHandle()
{
	CNX_AutoLock lock( &m_hLock );
	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return -1;
	}

	NX_GSTMP_Close(m_hPlayer);

	m_hPlayer = NULL;

	return 0;
}

//================================================================================================================
//public methods	common Control
int CNX_GstMoviePlayer::SetVolume(int volume)
{
	CNX_AutoLock lock( &m_hLock );
	if(NULL == m_hPlayer)
	{
		NXLOGE( "%s: Error! Handle is not initialized!", __FUNCTION__ );
		return -1;
	}

	NX_GST_RET iResult = NX_GSTMP_SetVolume(m_hPlayer, volume);
	if(NX_GST_RET_OK != iResult)
	{
		NXLOGE( "%s(): Error! NX_MPSetVolume() Failed! (ret = %d)", __FUNCTION__, iResult);
		return -1;
	}
	return 0;
}

int CNX_GstMoviePlayer::Play()
{
	CNX_AutoLock lock( &m_hLock );
	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return -1;
	}

	NX_GST_RET iResult = NX_GSTMP_Play(m_hPlayer);
	if(NX_GST_RET_OK != iResult)
	{
		NXLOGE("%s(): Error! NX_MPPlay() Failed! (ret = %d)", __FUNCTION__, iResult);
		return -1;
	}

	return 0;
}

int CNX_GstMoviePlayer::Seek(qint64 position)
{
	CNX_AutoLock lock(&m_hLock);
	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return -1;
	}

	NX_GST_RET iResult = NX_GSTMP_Seek(m_hPlayer, (gint64)position);
	if(NX_GST_RET_OK != iResult)
	{
		NXLOGE( "%s(): Error! NX_MPSeek() Failed! (ret = %d)", __FUNCTION__, iResult);
		return -1;
	}
	return 0;
}

int CNX_GstMoviePlayer::Pause()
{
	CNX_AutoLock lock(&m_hLock);
	if(NULL == m_hPlayer)
	{
		NXLOGE( "%s: Error! Handle is not initialized!", __FUNCTION__);
		return -1;
	}

	NX_GST_RET iResult = NX_GSTMP_Pause(m_hPlayer);
	if(NX_GST_RET_OK != iResult)
	{
		NXLOGE("%s(): Error! NX_MPPause() Failed! (ret = %d)", __FUNCTION__, iResult);
		return -1;
	}

	return 0;
}

int CNX_GstMoviePlayer::Stop()
{
	CNX_AutoLock lock(&m_hLock);
	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return -1;
	}

	NX_GST_RET iResult = NX_GSTMP_Stop(m_hPlayer);
	if(NX_GST_RET_OK != iResult)
	{
		NXLOGE( "%s(): Error! NX_MPStop() Failed! (ret = %d)", __FUNCTION__, iResult);
		return -1;
	}

	return 0;
}

//================================================================================================================
//public methods	common information
gint64 CNX_GstMoviePlayer::GetMediaPosition()
{
	CNX_AutoLock lock(&m_hLock);
	if(NULL == m_hPlayer)
	{
		NXLOGE( "%s: Error! Handle is not initialized!", __FUNCTION__ );
		return -1;
	}

	gint64 position = NX_GSTMP_GetPosition(m_hPlayer);
	if(-1 == position)
	{
		NXLOGE( "%s(): Error! NX_MPGetPosition() Failed!", __FUNCTION__);
		return -1;
	}

	return position;
}

gint64 CNX_GstMoviePlayer::GetMediaDuration()
{
	CNX_AutoLock lock(&m_hLock);
	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return -1;
	}

	gint64 duration = NX_GSTMP_GetDuration(m_hPlayer);
	if(-1 == duration)
	{
		NXLOGE( "%s(): Error! NX_MPGetDuration() Failed!", __FUNCTION__);
		return -1;
	}

	return duration;
}

NX_MEDIA_STATE CNX_GstMoviePlayer::GetState()
{
	CNX_AutoLock lock(&m_hLock);
	if(NULL == m_hPlayer)
	{
		NXLOGI("%s() handle is NULL, state is considered as STOPPED!", __FUNCTION__);
		return MP_STATE_STOPPED;
	}
	return (NX_MEDIA_STATE)NX_GSTMP_GetState(m_hPlayer);
}

//================================================================================================================
//public methods	video information
void CNX_GstMoviePlayer::PrintMediaInfo( const char *pUri )
{
	NXLOGD( "FileName : %s\n", pUri );
	NXLOGI("%s() container(%s), video codec(%s)"
		   ", audio codec(%s), seekable(%s), width(%d), height(%d)"
		   ", duration: (%" GST_TIME_FORMAT ")\r"
		   , __FUNCTION__
		   , m_MediaInfo.container_format
		   , m_MediaInfo.video_mime_type
		   , m_MediaInfo.audio_mime_type
		   , m_MediaInfo.isSeekable ? "yes":"no"
		   , m_MediaInfo.iWidth
		   , m_MediaInfo.iHeight
		   , GST_TIME_ARGS (m_MediaInfo.iDuration));
}

//================================================================================================================
int CNX_GstMoviePlayer::OpenHandle(void (*pCbEventCallback)(void *privateDesc, unsigned int EventType,
															unsigned int EventData, void* param),
								 void *cbPrivate)
{
	NXLOGI("%s", __FUNCTION__);

	NX_GST_RET iResult = NX_GSTMP_Open(&m_hPlayer, pCbEventCallback, cbPrivate);
	if(NX_GST_RET_OK != iResult)
	{
		NXLOGE( "%s: Error! Handle is not initialized!\n", __FUNCTION__);
		return -1;
	}
	return 0;
}

int CNX_GstMoviePlayer::SetDisplayMode(DISPLAY_MODE mode)
{
	NXLOGI("%s mode(%d)", __FUNCTION__, mode);

	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return -1;
	}

	NX_GST_RET iResult = NX_GSTMP_SetDisplayMode(m_hPlayer, mode);
	if(NX_GST_RET_OK != iResult)
	{
		NXLOGE("%s(): Error! NX_GSTMP_SetDisplayMode() Failed! (ret = %d, mode = %d)\n", __FUNCTION__, iResult, mode);
		return -1;
	}
	return 0;
}

int CNX_GstMoviePlayer::SetUri(const char *pUri)
{
	NXLOGI("%s", __FUNCTION__);

	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return -1;
	}

	NX_GST_RET iResult = NX_GSTMP_SetUri(m_hPlayer, pUri);
	if(NX_GST_RET_OK != iResult)
	{
		NXLOGE("%s(): Error! NX_MPSetUri() Failed! (ret = %d, uri = %s)\n", __FUNCTION__, iResult, pUri);
		return -1;
	}
	return 0;
}

int CNX_GstMoviePlayer::GetMediaInfo()
{
	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return -1;
	}
	NX_GST_RET iResult = NX_GSTMP_GetMediaInfo(m_hPlayer, &m_MediaInfo);
	if(NX_GST_RET_OK != iResult)
	{
		NXLOGE("%s(): Error! NX_MPGetMediaInfo() Failed! (ret = %d)\n", __FUNCTION__, iResult);
		return -1;
	}

	return 0;
}

int CNX_GstMoviePlayer::SetDisplayInfo(DISPLAY_TYPE type, int dspWidth, int dspHeight, DSP_RECT rect)
{
	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return -1;
	}
	NX_GST_RET iResult = NX_GSTMP_SetDisplayInfo(m_hPlayer, type, dspWidth, dspHeight, rect);
	if(NX_GST_RET_OK != iResult)
	{
		NXLOGE("%s(): Error! NX_GSTMP_SetDisplayInfo() Failed! (ret = %d)\n", __FUNCTION__, iResult);
		return -1;
	}

	return 0;
}

int CNX_GstMoviePlayer::DrmVideoMute(int bOnOff)
{
	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return -1;
	}

	m_bVideoMute = bOnOff;
	NX_GST_RET iResult = NX_GSTMP_VideoMute(m_hPlayer, m_bVideoMute);
	if(NX_GST_RET_OK != iResult)
	{
		NXLOGE("%s(): Error! NX_GSTMP_VideoMute() Failed! (ret = %d)\n", __FUNCTION__, iResult);
		return -1;
	}

	return 0;
}

//================================================================================================================
// subtitle routine
void CNX_GstMoviePlayer::CloseSubtitle()
{
	CNX_AutoLock lock( &m_SubtitleLock );
	if(m_pSubtitleParser)
	{
		if(m_pSubtitleParser->NX_SPIsParsed())
		{
			m_pSubtitleParser->NX_SPClose();
		}
	}
}

int CNX_GstMoviePlayer::OpenSubtitle(char * subtitlePath)
{
	CNX_AutoLock lock( &m_SubtitleLock );
	if(m_pSubtitleParser)
	{
		return m_pSubtitleParser->NX_SPOpen(subtitlePath);
	}
	else
	{
		NXLOGW("in OpenSubtitle no parser instance\n");
		return 0;
	}
}

int CNX_GstMoviePlayer::GetSubtitleStartTime()
{
	CNX_AutoLock lock( &m_SubtitleLock );
	if(m_pSubtitleParser->NX_SPIsParsed())
	{
		return m_pSubtitleParser->NX_SPGetStartTime();
	}
	else
	{
		return 0;
	}
}

void CNX_GstMoviePlayer::SetSubtitleIndex(int idx)
{
	CNX_AutoLock lock( &m_SubtitleLock );
	if(m_pSubtitleParser->NX_SPIsParsed())
	{
		m_pSubtitleParser->NX_SPSetIndex(idx);
	}
}

int CNX_GstMoviePlayer::GetSubtitleIndex()
{
	CNX_AutoLock lock( &m_SubtitleLock );
	if(m_pSubtitleParser->NX_SPIsParsed())
	{
		return m_pSubtitleParser->NX_SPGetIndex();
	}
	else
	{
		return 0;
	}
}

int CNX_GstMoviePlayer::GetSubtitleMaxIndex()
{
	CNX_AutoLock lock( &m_SubtitleLock );
	if(m_pSubtitleParser->NX_SPIsParsed())
	{
		return m_pSubtitleParser->NX_SPGetMaxIndex();
	}
	else
	{
		return 0;
	}
}

void CNX_GstMoviePlayer::IncreaseSubtitleIndex()
{
	CNX_AutoLock lock( &m_SubtitleLock );
	if(m_pSubtitleParser->NX_SPIsParsed())
	{
		m_pSubtitleParser->NX_SPIncreaseIndex();
	}
}

char* CNX_GstMoviePlayer::GetSubtitleText()
{
	CNX_AutoLock lock( &m_SubtitleLock );
	if(m_pSubtitleParser->NX_SPIsParsed())
	{
		return m_pSubtitleParser->NX_SPGetSubtitle();
	}
	else
	{
		return NULL;
	}
}

bool CNX_GstMoviePlayer::IsSubtitleAvailable()
{
	return m_pSubtitleParser->NX_SPIsParsed();
}

const char *CNX_GstMoviePlayer::GetBestSubtitleEncode()
{
	CNX_AutoLock lock( &m_SubtitleLock );
	if(m_pSubtitleParser->NX_SPIsParsed())
	{
		return m_pSubtitleParser->NX_SPGetBestTextEncode();
	}
	else
	{
		return NULL;
	}
}

const char *CNX_GstMoviePlayer::GetBestStringEncode(const char *str)
{
	if(!m_pSubtitleParser)
	{
		NXLOGW("GetBestStringEncode no parser instance\n");
		return "EUC-KR";
	}
	else
	{
		return m_pSubtitleParser->NX_SPFindStringEncode(str);
	}
}

void CNX_GstMoviePlayer::SeekSubtitle(int milliseconds)
{
	if (0 > pthread_create(&m_subtitleThread, NULL, ThreadWrapForSubtitleSeek, this) )
	{
		NXLOGE("SeekSubtitle creating Thread err\n");
		m_pSubtitleParser->NX_SPSetIndex(0);
		return;
	}

	m_iSubtitleSeekTime = milliseconds;
	NXLOGD("seek input  : %d\n",milliseconds);

	pthread_join(m_subtitleThread, NULL);
}

void* CNX_GstMoviePlayer::ThreadWrapForSubtitleSeek(void *Obj)
{
	if( NULL != Obj )
	{
		NXLOGD("ThreadWrapForSubtitleSeek ok");
		( (CNX_GstMoviePlayer*)Obj )->SeekSubtitleThread();
	}
	else
	{
		NXLOGE("ThreadWrapForSubtitleSeek err");
		return (void*)0xDEADDEAD;
	}
	return (void*)1;
}

void CNX_GstMoviePlayer::SeekSubtitleThread(void)
{
	CNX_AutoLock lock( &m_SubtitleLock );
	m_pSubtitleParser->NX_SPSetIndex(m_pSubtitleParser->NX_SPSeekSubtitleIndex(m_iSubtitleSeekTime));
}

void CNX_GstMoviePlayer::GetAspectRatio(int srcWidth, int srcHeight,
									 int dspWidth, int dspHeight,
									 DSP_RECT *pDspDstRect)
{
	// Calculate Video Aspect Ratio
	double xRatio = (double)dspWidth / (double)srcWidth;
	double yRatio = (double)dspHeight / (double)srcHeight;

	if( xRatio > yRatio )
	{
		pDspDstRect->iWidth    = (int)((double)srcWidth * yRatio);
		pDspDstRect->iHeight   = dspHeight;
	}
	else
	{
		pDspDstRect->iWidth    = dspWidth;
		pDspDstRect->iHeight   = (int)((double)srcHeight * xRatio);
	}

	if(dspWidth != pDspDstRect->iWidth)
	{
		if(dspWidth > pDspDstRect->iWidth)
		{
			pDspDstRect->iX = (dspWidth - pDspDstRect->iWidth)/2;
		}
	}

	if(dspHeight != pDspDstRect->iHeight)
	{
		if(dspHeight > pDspDstRect->iHeight)
		{
			pDspDstRect->iY = (dspHeight - pDspDstRect->iHeight)/2;
		}
	}
}

gdouble CNX_GstMoviePlayer::GetVideoSpeed()
{
	gdouble speed = 1.0;

	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return speed;
	}

	speed = NX_GSTMP_GetVideoSpeed(m_hPlayer);
	NXLOGI("%s() GetVideoSpeed(%f)", __FUNCTION__, speed);
	return speed;
}

int CNX_GstMoviePlayer::SetVideoSpeed(gdouble speed)
{
	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return -1;
	}

	NXLOGI("%s() SetVideoSpeed(%f)", __FUNCTION__, speed);
	NX_GST_RET iResult = NX_GSTMP_SetVideoSpeed(m_hPlayer, speed);
	if(NX_GST_RET_OK != iResult)
	{
		NXLOGE("%s(): Error! NX_GSTMP_SetVideoSpeed() Failed! (ret = %d)\n", __FUNCTION__, iResult);
		return -1;
	}

	return 0;
}

int	CNX_GstMoviePlayer::GetVideoSpeedSupport()
{
	int ret = 0;

	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return -1;
	}

	bool iResult = NX_GSTMP_GetVideoSpeedSupport(m_hPlayer);
	if(true != iResult)
	{
		NXLOGE( "%s: Error! This file doesn't support changing playback speed!", __FUNCTION__);
		return -1;
	}

	return ret;
}

bool CNX_GstMoviePlayer::HasSubTitleStream()
{
	if(NULL == m_hPlayer)
	{
		NXLOGE("%s: Error! Handle is not initialized!", __FUNCTION__);
		return false;
	}
	NXLOGI("%s() %s", __FUNCTION__, ((m_MediaInfo.n_subtitle > 0)?"true":"false"));
	return (m_MediaInfo.n_subtitle > 0) ? true:false;
}

const char* CNX_GstMoviePlayer::GetThumbnail(const char *pUri, gint64 pos_msec, gint width)
{
	NXLOGI("%s", __FUNCTION__);

	const char* filepath = NX_GSTMP_GetThumbnail(pUri, pos_msec, width);
	if (strlen(filepath) != 0)
	{
		NXLOGI("%s filepath:%s", __FUNCTION__, filepath);
	}

	return filepath;
}