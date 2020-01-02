#include "PlayerVideoFrame.h"
#include "ui_PlayerVideoFrame.h"
#include <QTextCodec>
#include <QDesktopWidget>

//Display Mode
#define DSP_FULL   0
#define DSP_HALF   1

//Display Info
#define DSP_FULL_WIDTH  1024
#define DSP_FULL_HEIGHT 600
#define DSP_HALF_WIDTH  DSP_FULL_WIDTH/2
#define DSP_HALF_HEIGHT 600

//Button Width,Height
#define BUTTON_Y            520
#define BUTTON_FULL_X       20
#define BUTTON_FULL_WIDTH   90
#define BUTTON_FULL_HEIGHT  60
#define BUTTON_HALF_X       10
#define BUTTON_HALF_WIDTH   40
#define BUTTON_HALF_HEIGHT  60

#define LOG_TAG "[VideoPlayer|Frame]"
#include <NX_Log.h>

#define DEFAULT_DSP_WIDTH	1024
#define DEFAULT_DSP_HEIGHT	600

static int lock_cnt = 0;

//------------------------------------------
#define NX_CUSTOM_BASE QEvent::User
enum
{
	NX_CUSTOM_BASE_ACCEPT = NX_CUSTOM_BASE+1,
	NX_CUSTOM_BASE_REJECT
};

class AcceptEvent : public QEvent
{
public:
	AcceptEvent() :
		QEvent((QEvent::Type)NX_CUSTOM_BASE_ACCEPT)
	{

	}
};

class RejectEvent : public QEvent
{
public:
	RejectEvent() :
		QEvent((QEvent::Type)NX_CUSTOM_BASE_REJECT)
	{

	}
};

////////////////////////////////////////////////////////////////////////////////
//
//	Event Callback
//
static	CallBackSignal mediaStateCb;
static	PlayerVideoFrame *pPlayFrame = NULL;

//CallBack Eos, Error
static void cbEventCallback( void */*privateDesc*/, unsigned int EventType, unsigned int /*EventData*/, unsigned int /*param*/ )
{
	mediaStateCb.statusChanged(EventType);
}

//CallBack Qt
static void cbStatusHome( void *pObj )
{
	(void)pObj;
	PlayerVideoFrame *p = (PlayerVideoFrame *)pObj;
	QApplication::postEvent(p, new NxStatusHomeEvent());
}

static void cbStatusBack( void *pObj )
{
	PlayerVideoFrame *pW = (PlayerVideoFrame *)pObj;
	pW->SaveInfo();
	pW->StopVideo();
	pW->close();
	QApplication::postEvent(pW, new NxStatusBackEvent());
}

static void cbStatusVolume( void *pObj )
{
	PlayerVideoFrame *pW = (PlayerVideoFrame *)pObj;
	QApplication::postEvent(pW, new NxStatusVolumeEvent());
}

static int32_t cbSqliteRowCallback( void *pObj, int32_t iColumnNum, char **ppColumnValue, char **ppColumnName )
{
	char* cFileType = NULL;
	char* cFilePath = NULL;
	for( int32_t i = 0; i < iColumnNum; i++ )
	{
		if( !strcmp("_type", ppColumnName[i] ))
		{
			cFileType  = ppColumnValue[i];
		}

		if( !strcmp("_path", ppColumnName[i] ))
		{
			cFilePath = ppColumnValue[i];
		}
	}

	//if(type == video)-->add
	if( !strcmp("video", cFileType ))
	{
		(((PlayerVideoFrame*)pObj)->GetFileList())->AddItem( QString::fromUtf8(cFilePath) );
	}
	return 0;
}

PlayerVideoFrame::PlayerVideoFrame(QWidget *parent)
	: QFrame(parent)
    , dbg(false)
	, m_pNxPlayer(NULL)
	, m_bSubThreadFlag(false)
	, m_iVolValue(30)
	, m_iDuration(0)
	, m_bIsInitialized(false)
	, m_pStatusBar(NULL)
	, m_bSeekReady(false)
	, m_bButtonHide(false)
	, m_iCurFileListIdx (0)
	, m_bTurnOffFlag(false)
	, m_bStopRenderingFlag(false)
	, m_bTryFlag(false)
	, m_pPlayListFrame(NULL)
	, m_bIsVideoFocus(true)
	, m_bIsAudioFocus(true)
	, m_pRequestTerminate(NULL)
	, m_pRequestLauncherShow(NULL)
	, m_pRequestVolume(NULL)
	, m_fSpeed(1.0)
	, m_bNotSupportSpeed (false)
	, m_pMessageFrame(NULL)
	, m_pMessageLabel(NULL)
	, m_pMessageButton(NULL)
	, ui(new Ui::PlayerVideoFrame)
{
	//UI Setting
	ui->setupUi(this);

	const QRect screen = QApplication::desktop()->screenGeometry();
	if ((width() != screen.width()) || (height() != screen.height()))
	{
		setFixedSize(screen.width(), screen.height());
	}


	m_pNxPlayer = new CNX_GstMoviePlayer();

	UpdateFileList();
	m_pIConfig = GetConfigHandle();

	//	Connect Solt Functions
	connect(&mediaStateCb, SIGNAL(mediaStatusChanged(int)), SLOT(statusChanged(int)));
	pPlayFrame = this;

	ui->graphicsView->viewport()->installEventFilter(this);
	ui->progressBar->installEventFilter(this);

	//Update position timer
	connect(&m_PosUpdateTimer, SIGNAL(timeout()), this, SLOT(DoPositionUpdate()));

	setAttribute(Qt::WA_AcceptTouchEvents, true);

	m_PosUpdateTimer.start(300);

	//
	//	Initialize UI Controls
	//
	//	Nexell Status Bar
	m_pStatusBar = new CNX_StatusBar( this );
	m_pStatusBar->move( 0, 0 );
	m_pStatusBar->resize( this->size().width(), this->size().height() * 1 / 10 );
	m_pStatusBar->RegOnClickedHome( cbStatusHome );
	m_pStatusBar->RegOnClickedBack( cbStatusBack );
	m_pStatusBar->RegOnClickedVolume( cbStatusVolume );
	m_pStatusBar->SetTitleName( "Nexell Video Player" );

	ui->durationlabel->setStyleSheet("QLabel { color : white; }");
	ui->appNameLabel->setStyleSheet("QLabel { color : white; }");
	ui->subTitleLabel->setStyleSheet("QLabel { color : white; }");
	ui->subTitleLabel2->setStyleSheet("QLabel { color : white; }");

	//Message
	m_pMessageFrame = new QFrame(this);

	m_pMessageFrame->setGeometry(340, 190, 271, 120);
	m_pMessageFrame->setStyleSheet("background: white;");
	m_pMessageFrame->hide();

	m_pMessageLabel = new QLabel(m_pMessageFrame);
	m_pMessageLabel->setGeometry(0, 0, m_pMessageFrame->width(), 100);
	m_pMessageLabel->setText("text");

	m_pMessageButton = new QPushButton(m_pMessageFrame);
	m_pMessageButton->setGeometry(m_pMessageFrame->width()/2-100/2, m_pMessageFrame->height()-30, 80, 30);
	m_pMessageButton->setText("Ok");
	connect(m_pMessageButton, SIGNAL(clicked(bool)), this, SLOT(slotOk()));

	//Get audioDeviceName
	memset(m_audioDeviceName,0,sizeof(m_audioDeviceName));
	if(0 > m_pIConfig->Open("/nexell/daudio/NxGstVideoPlayer/nxvideoplayer_config.xml"))
	{
		NXLOGE("[%s]nxgstvideooplayer_config.xml open err\n", __FUNCTION__);
	}
	else
	{
		char *pBuf = NULL;
		if(0 > m_pIConfig->Read("alsa_default",&pBuf))
		{
			NXLOGE("[%s]xml alsa_default err\n", __FUNCTION__);
			memcpy(m_audioDeviceName,"plughw:0,0",sizeof("plughw:0,0"));
		}
		else
		{
			strcpy(m_audioDeviceName,pBuf);
		}
	}
	m_pIConfig->Close();
}

PlayerVideoFrame::~PlayerVideoFrame()
{
	if (m_PosUpdateTimer.isActive())
	{
		m_PosUpdateTimer.stop();
	}

	if(m_pNxPlayer)
	{
        NX_MEDIA_STATE state = m_pNxPlayer->GetState();
        if( (MP_STATE_PLAYING == state)||(MP_STATE_PAUSED == state) )
		{
			StopVideo();
		}

		delete m_pNxPlayer;
		m_pNxPlayer = NULL;
	}
	if(m_pPlayListFrame)
	{
		delete m_pPlayListFrame;
		m_pPlayListFrame = NULL;
	}

	if(m_pStatusBar)
	{
		delete m_pStatusBar;
	}

	if(m_pMessageButton)
	{
		delete m_pMessageButton;
	}

	if(m_pMessageLabel)
	{
		delete m_pMessageLabel;
	}

	if(m_pMessageFrame)
	{
		delete m_pMessageFrame;
	}

	delete ui;
}

//
//	xml (save previous state)
int32_t PlayerVideoFrame::SaveInfo()
{
	if( NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return -1;
	}

	if(0 > m_pIConfig->Open("/nexell/daudio/NxGstVideoPlayer/config.xml"))
	{
		NXLOGW("xml open err\n");
		QFile qFile;
		qFile.setFileName("/nexell/daudio/NxGstVideoPlayer/config.xml");
		if(qFile.remove())
		{
			NXLOGW("config.xml is removed because of open err\n");
			if(0 > m_pIConfig->Open("/nexell/daudio/NxGstVideoPlayer/config.xml"))
			{
				NXLOGE("xml open err again!!\n");
				return -1;
			}
		}else
		{
			NXLOGE("Deleting config.xml is failed!\n");
			return -1;
		}
	}

	//save current media path
	lock_cnt++;
	m_listMutex.Lock();
	QString curPath = m_FileList.GetList(m_iCurFileListIdx);
	m_listMutex.Unlock();
	lock_cnt--;
	if( curPath.isEmpty() || curPath.isNull() )
	{
		NXLOGE("current path is not valid\n");
		m_pIConfig->Close();
		return -1;
	}

	// encode pCurPath(QString , unicode) to UTF-8
	QTextCodec* pCodec = QTextCodec::codecForName("UTF-8");		//pCodec  271752
	QTextEncoder* pEncoder = pCodec->makeEncoder();
	QByteArray encodedByteArray = pEncoder->fromUnicode(curPath);
	char* pCurPath = (char*)encodedByteArray.data();

	if(0 > m_pIConfig->Write("path", pCurPath ))
	{
		NXLOGE("xml write path err\n");
		m_pIConfig->Close();
		return -1;
	}

	//save current media position
	NX_MEDIA_STATE state = m_pNxPlayer->GetState();
    if((MP_STATE_PLAYING == state)||(MP_STATE_PAUSED == state))
	{
		char pCurPos[sizeof(long long int)] = {};
		qint64 iCurPos = m_pNxPlayer->GetMediaPosition();
		if(0 > iCurPos)
		{
			NXLOGW("current position is not valid  iCurPos : %lld is set to 0\n",iCurPos);
			iCurPos = 0;
		}
		sprintf(pCurPos, "%lld", NANOSEC_TO_MSEC(iCurPos));
		if(0 > m_pIConfig->Write("pos", pCurPos))
		{
			NXLOGE("xml write pos err\n");
			m_pIConfig->Close();
			return -1;
		}
    }
    else if(MP_STATE_STOPPED == state)
	{
		char pCurPos[sizeof(int)] = {};
		sprintf(pCurPos, "%d", 0);
		if(0 > m_pIConfig->Write("pos", pCurPos))
		{
			NXLOGE("xml write pos err\n");
			m_pIConfig->Close();
			return -1;
		}
	}

	m_pIConfig->Close();
	return 0;
}

bool PlayerVideoFrame::SeekToPrev(int* iSavedPosition, int* iFileIdx)
{
	if( NULL == m_pNxPlayer)
	{
		NXLOGE("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return false;
	}

	if(0 > m_pIConfig->Open("/nexell/daudio/NxGstVideoPlayer/config.xml"))
	{
		NXLOGE("xml open err\n");
		QFile qFile;
		qFile.setFileName("/nexell/daudio/NxGstVideoPlayer/config.xml");
		if(qFile.remove())
		{
			NXLOGE("config.xml is removed because of open err\n");
		}else
		{
			NXLOGE("Deleting config.xml is failed!\n");
		}
		return false;
	}

	//load previous media path
	char* pPrevPath = NULL;
	if(0 > m_pIConfig->Read("path",&pPrevPath))
	{
		NXLOGE("xml read path err\n");
		m_pIConfig->Close();
		return false;
	}

	//load previous media position
	char* pBuf = NULL;
	if(0 > m_pIConfig->Read("pos",&pBuf))
	{
		NXLOGE("xml read pos err\n");
		m_pIConfig->Close();
		return false;
	}
	*iSavedPosition = atoi(pBuf);
	m_pIConfig->Close();

	//find index in file list by path
	lock_cnt++;
	m_listMutex.Lock();
	if(0 < m_FileList.GetSize())
	{
		//media file list is valid
		//find pPrevPath in list

		int iIndex = m_FileList.GetPathIndex(QString::fromUtf8(pPrevPath));
		if(0 > iIndex)
		{
			NXLOGE("saved path does not exist in FileList\n");
			m_listMutex.Unlock();
			lock_cnt--;
			return false;
		}
		*iFileIdx = iIndex;
		m_listMutex.Unlock();
		lock_cnt--;
		return true;
	}else
	{
		NXLOGD("File List is not valid.. no media file or media scan is not done\n");
		NXLOGD("just try last path : %s\n\n",pPrevPath);
		m_bTryFlag = true;
		m_FileList.AddItem(QString::fromUtf8(pPrevPath));
		*iFileIdx = 0;
		m_listMutex.Unlock();
		lock_cnt--;
		return true;
	}

	return false;
}

//
//	Storage Event
void PlayerVideoFrame::StorageRemoved()
{
	m_bTurnOffFlag = true;

	if(NULL != m_pNxPlayer)
	{
		SaveInfo();
		StopVideo();
	}else
	{
		NXLOGE("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
	}

	if(NULL != m_pPlayListFrame)
	{
		m_pPlayListFrame->close();
	}

	qDebug("########## StorageRemoved()\n");
	//    this->close();
	QApplication::postEvent(this, new NxTerminateEvent());
}

void PlayerVideoFrame::StorageScanDone()
{
	if(NULL == m_pNxPlayer)
	{
		NXLOGE("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return;
	}
	lock_cnt++;
	m_listMutex.Lock();
	bool bPlayFlag = false;
	if(0 == m_FileList.GetSize())
	{
		//videoplayer is obviously not playing..
		UpdateFileList();
		if(0 != m_FileList.GetSize())
		{
			bPlayFlag = true;
		}
    }
    else
	{
		//videoplayer could be playing some file...
		//what if file list is accessed some where...
		//m_listMutex.....prev next play seekToPrev SaveInfo StorageRemoved StorageScanDone
		QString curPath = m_FileList.GetList(m_iCurFileListIdx);
		m_FileList.ClearList();
		m_iCurFileListIdx = 0;
		UpdateFileList();

		if( NULL != curPath)
		{
			int iIndex = m_FileList.GetPathIndex(curPath);
			if(0 > iIndex)
			{
				NXLOGD("line : %d , path does not exist in FileList\n",__LINE__);
				m_iCurFileListIdx = 0;
			}else
			{
				m_iCurFileListIdx = iIndex;
			}
		}
	}
	m_listMutex.Unlock();
	lock_cnt--;

	if(NULL != m_pPlayListFrame)
	{
		lock_cnt++;
		m_listMutex.Lock();
		m_pPlayListFrame->setList(&m_FileList);
		m_listMutex.Unlock();
		lock_cnt--;
	}

	if(bPlayFlag)
	{
		PlaySeek();
	}
}

void PlayerVideoFrame::UpdateFileList()
{
	//	read data base that Media Scaning made.
	char szPath[256];
	snprintf( szPath, sizeof(szPath), "%s/%s", NX_MEDIA_DATABASE_PATH, NX_MEDIA_DATABASE_NAME );
	NX_SQLiteGetData( szPath, NX_MEDIA_DATABASE_TABLE, cbSqliteRowCallback, (void*)this);
	NXLOGD("<<< Total file list = %d\n", m_FileList.GetSize());
}

CNX_FileList *PlayerVideoFrame::GetFileList()
{
	return &m_FileList;
}

bool PlayerVideoFrame::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->graphicsView->viewport())
	{
		if (event->type() == QEvent::MouseButtonPress)
		{
		}
		else if (event->type() == QEvent::MouseButtonRelease)
		{
			displayTouchEvent();
		}
	}
	else if (watched == ui->progressBar)
	{
		if (event->type() == QEvent::MouseButtonPress)
		{
			QMouseEvent *pMouseEvent = static_cast<QMouseEvent *>(event);
			updateProgressBar(pMouseEvent, false);
		}
		else if (event->type() == QEvent::MouseButtonRelease)
		{
			QMouseEvent *pMouseEvent = static_cast<QMouseEvent *>(event);
			updateProgressBar(pMouseEvent, true);
		}
	}

	return QFrame::eventFilter(watched, event);
}

////////////////////////////////////////////////////////////////////
//
//      Update Player Progress Bar & Volume Progress Bar
//
////////////////////////////////////////////////////////////////////
void PlayerVideoFrame::updateProgressBar(QMouseEvent *event, bool bReleased)
{
    if(bReleased)
	{
		//	 Do Seek
        if(m_bSeekReady)
		{
            if(MP_STATE_STOPPED != m_pNxPlayer->GetState())
			{
				double ratio = (double)event->x()/ui->progressBar->width();
                m_iDuration = m_pNxPlayer->GetMediaDuration();
                qint64 position = ratio * NANOSEC_TO_MSEC(m_iDuration);
                NXLOGI("%s() ratio: %lf, m_iDuration: %lld, conv_msec:%lld",
                       __FUNCTION__, ratio, m_iDuration, NANOSEC_TO_SEC(m_iDuration));
                SeekVideo(position);

				//seek subtitle
				ui->subTitleLabel->setText("");
				ui->subTitleLabel2->setText("");

				NXLOGD("Position = %lld", position);
			}
			NXLOGD("Do Seek !!!");
			m_PosUpdateTimer.start(300);
		}
		m_bSeekReady = false;
	}
	else
	{
		m_bSeekReady = true;
		NXLOGD("Ready to Seek");
	}
}

void PlayerVideoFrame::setVideoFocus(bool bVideoFocus)
{
	m_bIsVideoFocus = bVideoFocus;
}

void PlayerVideoFrame::setAudioFocus(bool bAudioFocus)
{
	m_bIsAudioFocus = bAudioFocus;
}

/* Prev button */
void PlayerVideoFrame::on_prevButton_released()
{
	if(m_bIsAudioFocus == false)
	{
		return;
	}

	PlayPreviousVideo();
}

/* Play button */
void PlayerVideoFrame::on_playButton_released()
{
	if(m_bIsAudioFocus == false)
	{
		return;
	}

	PlayVideo();
}

/* Pause button */
void PlayerVideoFrame::on_pauseButton_released()
{
	if(m_bIsAudioFocus == false)
	{
		return;
	}

	PauseVideo();
}

/* Next button */
void PlayerVideoFrame::on_nextButton_released()
{
	if(m_bIsAudioFocus == false)
	{
		return;
	}

	PlayNextVideo();
}

/* Stop button */
void PlayerVideoFrame::on_stopButton_released()
{
	if(m_bIsAudioFocus == false)
	{
		return;
	}

	StopVideo();
}

void PlayerVideoFrame::DoPositionUpdate()
{
	if(NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		ui->progressBar->setRange(0, 100);
		ui->progressBar->setValue(0);
		UpdateDurationInfo( 0, 0 );
		return;
	}

	if(m_bIsInitialized)
	{
		int64_t iDuration = m_pNxPlayer->GetMediaDuration();
		int64_t iPosition = m_pNxPlayer->GetMediaPosition();

		if( (0 > iDuration) || (0 > iPosition) )
		{
			iPosition = 0;
			iDuration = 0;
		}
		//	ProgressBar
		ui->progressBar->setValue(NANOSEC_TO_SEC(iPosition));
        UpdateDurationInfo(iPosition, iDuration);
    }
	else
	{
		ui->progressBar->setRange(0, 100);
		ui->progressBar->setValue(0);
		UpdateDurationInfo( 0, 0 );
	}
}

void PlayerVideoFrame::UpdateDurationInfo(int64_t position, int64_t duration)
{
    char pos[256], dur[256];
    sprintf(pos, TIME_FORMAT, GST_TIME_ARGS (position));
	//NXLOGD("%s(): position: %s", __FUNCTION__, pos);

    sprintf(dur, TIME_FORMAT, GST_TIME_ARGS (duration));
	//NXLOGD("%s(): duration: %s", __FUNCTION__, dur);

    QString tStr;
    tStr = QString(pos) + " / " + QString(dur);

	ui->durationlabel->setText(tStr);
	//NXLOGD("%s() %s", __FUNCTION__, tStr.toStdString().c_str());
}

void PlayerVideoFrame::statusChanged(int eventType)
{
	if(m_bTurnOffFlag)
	{
		NXLOGW("%s  , line : %d close app soon.. bypass \n", __FUNCTION__, __LINE__);
		return;
	}

    NXLOGI("%s() eventType(%d) ", __FUNCTION__, eventType);
	switch (eventType)
	{
    case MP_EVENT_EOS:
	{
		NXLOGI("******** End-Of-Stream !!!");
		PlayNextVideo();
		break;
	}
	case MP_EVENT_DEMUX_LINK_FAILED:
	{
		NXLOGE("******** MP_EVENT_DEMUX_LINK_FAILED !!!");
		PlayNextVideo();
		break;
	}
	case MP_EVENT_GST_ERROR:
	{
		NXLOGE("******** MP_EVENT_GST_ERROR !!!");
		break;
	}
	case MP_EVENT_STATE_CHANGED:
	{
		NXLOGE("******** MP_EVENT_STATE_CHANGED !!!");
		break;
	}
	default:
		break;
	}
}

bool PlayerVideoFrame::StopVideo()
{
	if(NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return false;
	}

	if(0 == m_FileList.GetSize())
	{
		NXLOGW("%s(), line: %d, m_FileList is 0 \n", __FUNCTION__, __LINE__);
		return false;
	}

	m_statusMutex.Lock();
	m_bStopRenderingFlag = true;
	m_statusMutex.Unlock();

	m_pNxPlayer->Stop();
	CloseVideo();

	m_fSpeed = 1.0;
	ui->speedButton->setText("x 1");
	QString style;
	style += "QProgressBar {";
	style += "  border: 2px solid grey;";
	style += "  border-radius: 5px;";
	style += "  background: white;";
	style += "}";

	style += "QProgressBar::chunk {";
	style += "  background-color: rgb(37, 86, 201);";
	style += "width: 20px;";
	style += "}";
	ui->progressBar->setStyleSheet(style);

	if(m_bNotSupportSpeed)
	{
		m_bNotSupportSpeed = false;
		m_pMessageFrame->hide();
	}

	return true;
}

bool PlayerVideoFrame::CloseVideo()
{
	NXLOGI("%s()", __FUNCTION__);

	if(NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return false;
	}
	m_bIsInitialized = false;

	if(0 > m_pNxPlayer->CloseHandle())
	{
		NXLOGE("%s(), line: %d, CloseHandle failed \n", __FUNCTION__, __LINE__);
		return false;
	}

	return true;
}

bool PlayerVideoFrame::PlayNextVideo()
{
	NXLOGI("%s()", __FUNCTION__);
	if(NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return false;
	}

	StopVideo();

	//	find next index
	if(0 != m_FileList.GetSize())
	{
		m_iCurFileListIdx = (m_iCurFileListIdx+1) % m_FileList.GetSize();
	}

	return PlayVideo();
}

bool PlayerVideoFrame::PlayPreviousVideo()
{
	NXLOGI("%s()", __FUNCTION__);
	if(NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return false;
	}

	StopVideo();

	//	Find previous index
	if(0 != m_FileList.GetSize())
	{
		m_iCurFileListIdx --;
		if( 0 > m_iCurFileListIdx )
			m_iCurFileListIdx = m_FileList.GetSize() -1;
	}

	return PlayVideo();
}

void PlayerVideoFrame::PlaySeek()
{
	bool seekflag = false;
	int iSavedPosition = 0;

	NXLOGI("%s()", __FUNCTION__);
	if(NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return;
	}

	seekflag = SeekToPrev(&iSavedPosition, &m_iCurFileListIdx);

	PlayVideo();

	if(seekflag)
	{
		//seek video
		SeekVideo( iSavedPosition );

		//seek subtitle
		ui->subTitleLabel->setText("");
		ui->subTitleLabel2->setText("");
	}
}

bool PlayerVideoFrame::PlayVideo()
{
	NXLOGI("%s()", __FUNCTION__);
	if(NULL == m_pNxPlayer)
	{
        NXLOGW("%s(), line: %d, m_pNxPlayer is NULL", __FUNCTION__, __LINE__);
		return false;
	}

	NX_MEDIA_STATE state = m_pNxPlayer->GetState();
	NXLOGI("%s() state: %s", __FUNCTION__, get_nx_media_state(state));

	if((MP_STATE_PAUSED == state) || (MP_STATE_READY == state))
	{
		m_pNxPlayer->Play();

		//if(1.0 == m_pNxPlayer->GetVideoSpeed())
		{
			m_fSpeed = 1.0;
			ui->speedButton->setText("x 1");
			QString style;
			style += "QProgressBar {";
			style += "  border: 2px solid grey;";
			style += "  border-radius: 5px;";
			style += "  background: white;";
			style += "}";

			style += "QProgressBar::chunk {";
			style += "  background-color: rgb(37, 86, 201);";
			style += "width: 20px;";
			style += "}";

			ui->progressBar->setStyleSheet(style);
		}
		return true;
	}
	else if(MP_STATE_STOPPED == state)
	{
		lock_cnt++;
		m_listMutex.Lock();
		if(0 < m_FileList.GetSize())
		{
			int iResult = -1;
			int iTryCount = 0;
			int iMaxCount = m_FileList.GetSize();
			while(0 > iResult)
			{
				NXLOGI("%s retry!!!!", __FUNCTION__);
				if(m_bIsVideoFocus)
				{
					m_statusMutex.Lock();
					m_bStopRenderingFlag = false;
					m_statusMutex.Unlock();
				}

				m_fSpeed = 1.0;
				ui->speedButton->setText("x 1");

				iResult = m_pNxPlayer->InitMediaPlayer(cbEventCallback, NULL,
													   m_FileList.GetList(m_iCurFileListIdx).toStdString().c_str(),
													   width(), height(), m_audioDeviceName);

                NXLOGI("%s() filepath:%s", __FUNCTION__, m_FileList.GetList(m_iCurFileListIdx).toStdString().c_str());
                if(iResult != 0) {
					NXLOGE("%s() Error! Failed to setup GStreamer(ret:%d)", __FUNCTION__, iResult);
                }

				iTryCount++;
				if(0 == iResult)
				{
                    NXLOGI("%s() *********** media init done! *********** ", __FUNCTION__);
					m_bIsInitialized = true;

					if(0 > m_pNxPlayer->Play())
					{
						NXLOGE("NX_MPPlay() failed !!!");
                        iResult = -1; //retry with next file..
                    }
                    else
                    {
						m_iDuration = m_pNxPlayer->GetMediaDuration();
						if (-1 == m_iDuration) {
							ui->progressBar->setMaximum(0);
						}
						else
						{
							ui->progressBar->setMaximum(NANOSEC_TO_SEC(m_iDuration));
						}
						ui->appNameLabel->setText(m_FileList.GetList(m_iCurFileListIdx));

						//if(1.0 == m_pNxPlayer->GetVideoSpeed())
						{
							ui->speedButton->setText("x 1");
							QString style;
							style += "QProgressBar {";
							style += "  border: 2px solid grey;";
							style += "  border-radius: 5px;";
							style += "  background: white;";
							style += "}";

							style += "QProgressBar::chunk {";
							style += "  background-color: rgb(37, 86, 201);";
							style += "width: 20px;";
							style += "}";

							ui->progressBar->setStyleSheet(style);
						}

						NXLOGI("%s() *********** media play done! *********** ", __FUNCTION__);
						m_listMutex.Unlock();
						lock_cnt--;
						return true;
					}
				}

				if(m_bTryFlag)
				{
					//This case is for playing back to last file
					//When there is no available file list but last played file path exists in config.xml,
					//videoplayer tries playing path that saved in config.xml .
					//If trying playing is failed, videoplayer should stop trying next file.
                    NXLOGI("%s(): Have no available contents!!", __FUNCTION__);
					m_bTryFlag = false;
					m_FileList.ClearList();
					m_listMutex.Unlock();
					lock_cnt--;
					return false;
				}

				if( iTryCount == iMaxCount )
				{
					//all list is tried, but nothing succeed.
                    NXLOGI("%s(): Have no available contents!!", __FUNCTION__);
					m_listMutex.Unlock();
					lock_cnt--;
					return false;
				}

                NXLOGW("%s(): MediaPlayer Initialization fail.... retry with next file(lock_cnt=%d)", __FUNCTION__, lock_cnt);
				m_iCurFileListIdx = (m_iCurFileListIdx+1) % m_FileList.GetSize();
				CloseVideo();
				NXLOGW("%s() Closed video and try to play next video '%s'"
						, __FUNCTION__, m_FileList.GetList(m_iCurFileListIdx).toStdString().c_str());
			}	// end of while(0 > iResult)
		}		// end of if(0 < m_FileList.GetSize())
		else
		{
            NXLOGW("%s(): Have no available contents!! InitMediaPlayer is not tried", __FUNCTION__);
			m_listMutex.Unlock();
			lock_cnt--;
			return false;
		}
	}			// end of else if(MP_STATE_STOPPED == state)
	NXLOGW("%s() ------------------------------------------", __FUNCTION__);
	return true;
}

bool PlayerVideoFrame::PauseVideo()
{
	NXLOGI("%s()", __FUNCTION__);
	if(NULL == m_pNxPlayer)
	{
		NXLOGE("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return false;
	}

    if(0 > m_pNxPlayer->Pause()) {
        NXLOGE( "%s(): Error! Failed to pause", __FUNCTION__);
		return false;
    }

	return true;
}

bool PlayerVideoFrame::SeekVideo(int32_t mSec)
{
	NXLOGI("%s()", __FUNCTION__);
	if(NULL == m_pNxPlayer)
	{
		NXLOGE("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return false;
	}

	NX_MEDIA_STATE state = m_pNxPlayer->GetState();
    if(MP_STATE_PLAYING == state || MP_STATE_PAUSED == state)
	{
		NXLOGI("%s() seek to %d mSec", __FUNCTION__, mSec);
        if(0 > m_pNxPlayer->Seek(mSec))
		{
			return false;
		}
	}
	return true;
}

bool PlayerVideoFrame::SetVideoMute(bool iStopRendering)
{
	NXLOGI("%s()", __FUNCTION__);
	if(NULL == m_pNxPlayer)
	{
		NXLOGE("%s(), line: %d, m_pNxPlayer is NULL ", __FUNCTION__, __LINE__);
		return false;
	}

	m_statusMutex.Lock();
	m_bStopRenderingFlag = iStopRendering;
	m_statusMutex.Unlock();

	NXLOGI("%s(), line: %d, SetVideoMute : %d -------", __FUNCTION__, __LINE__, m_bStopRenderingFlag);

	return true;
}

bool PlayerVideoFrame::GetVideoMuteStatus()
{
	return m_bStopRenderingFlag;
}

bool PlayerVideoFrame::VideoMuteStart()
{
	if(m_fSpeed > 1.0)
	{
		StopVideo();
		return true;
	}

	SetVideoMute(true);
	m_pNxPlayer->DrmVideoMute(true);

	return true;
}

bool PlayerVideoFrame::VideoMuteStop()
{
	if(m_fSpeed > 1.0)
	{
		StopVideo();
		return true;
	}

	m_pNxPlayer->DrmVideoMute(false);
	SetVideoMute(false);

	return true;
}

bool PlayerVideoFrame::VideoMute()
{
	if(GetVideoMuteStatus())
	{
		VideoMuteStop();
	}
	else
	{
		VideoMuteStart();
	}

	return true;
}

void PlayerVideoFrame::displayTouchEvent()
{
	if(false == m_bButtonHide)
	{
		m_bButtonHide = true;
		ui->progressBar->hide();
		ui->prevButton->hide();
		ui->playButton->hide();
		ui->pauseButton->hide();
		ui->nextButton->hide();
		ui->stopButton->hide();
		ui->playListButton->hide();
		ui->durationlabel->hide();
		ui->appNameLabel->hide();
		ui->speedButton->hide();
		m_pStatusBar->hide();
		NXLOGD("**************** MainWindow:: Hide \n ");
	}
	else
	{
		NXLOGD("**************** MainWindow:: Show \n ");
		ui->progressBar->show();
		ui->prevButton->show();
		ui->playButton->show();
		ui->pauseButton->show();
		ui->nextButton->show();
		ui->stopButton->show();
		ui->playListButton->show();
		ui->durationlabel->show();
		ui->appNameLabel->show();
		ui->speedButton->show();
		m_pStatusBar->show();
		m_bButtonHide = false;
	}
}

//
//		Play Util
//
void PlayerVideoFrame::getAspectRatio(int srcWidth, int srcHeight,
									  int scrWidth, int scrHeight,
									  int *pWidth,  int *pHeight)
{
	// Calculate Video Aspect Ratio
	int dspWidth = 0, dspHeight = 0;
	double xRatio = (double)scrWidth / (double)srcWidth;
	double yRatio = (double)scrHeight / (double)srcHeight;

	if( xRatio > yRatio )
	{
		dspWidth    = (int)((double)srcWidth * yRatio);
		dspHeight   = scrHeight;
	}
	else
	{
		dspWidth    = scrWidth;
		dspHeight   = (int)((double)srcHeight * xRatio);
	}

	*pWidth     = dspWidth;
	*pHeight    = dspHeight;
}

//
//		Play List Button
//
void PlayerVideoFrame::on_playListButton_released()
{
	if(m_bIsAudioFocus == false)
	{
		return;
	}

	if(NULL == m_pPlayListFrame)
	{
		m_pPlayListFrame = new PlayListVideoFrame(this);
		m_pPlayListFrame->RegisterRequestLauncherShow(m_pRequestLauncherShow);
		m_pPlayListFrame->RegisterRequestVolume(m_pRequestVolume);
		connect(m_pPlayListFrame, SIGNAL(signalPlayListAccept()), this, SLOT(slotPlayListFrameAccept()));
		connect(m_pPlayListFrame, SIGNAL(signalPlayListReject()), this, SLOT(slotPlayListFrameReject()));
	}
	m_pPlayListFrame->show();

	lock_cnt++;
	m_listMutex.Lock();
	m_pPlayListFrame->setList(&m_FileList);
	m_listMutex.Unlock();

	lock_cnt--;

	m_pPlayListFrame->setCurrentIndex(m_iCurFileListIdx);
}

void PlayerVideoFrame::slotPlayListFrameAccept()
{
	QApplication::postEvent(this, new AcceptEvent());
}

void PlayerVideoFrame::slotPlayListFrameReject()
{
	QApplication::postEvent(this, new RejectEvent());
}

bool PlayerVideoFrame::event(QEvent *event)
{
	switch ((int32_t)event->type())
	{
	case NX_CUSTOM_BASE_ACCEPT:
	{
		if(m_pPlayListFrame)
		{
			m_iCurFileListIdx = m_pPlayListFrame->getCurrentIndex();
			StopVideo();
			PlayVideo();

			delete m_pPlayListFrame;
			m_pPlayListFrame = NULL;
		}
		return true;
	}
	case NX_CUSTOM_BASE_REJECT:
	{
		if(m_pPlayListFrame)
		{
			delete m_pPlayListFrame;
			m_pPlayListFrame = NULL;
		}
		return true;
	}
	case E_NX_EVENT_STATUS_HOME:
	{
		NxStatusHomeEvent *e = static_cast<NxStatusHomeEvent *>(event);
		StatusHomeEvent(e);
		return true;
	}
	case E_NX_EVENT_STATUS_BACK:
	{
		NxStatusBackEvent *e = static_cast<NxStatusBackEvent *>(event);
		StatusBackEvent(e);
		return true;
	}
	case E_NX_EVENT_STATUS_VOLUME:
	{
		NxStatusVolumeEvent *e = static_cast<NxStatusVolumeEvent *>(event);
		StatusVolumeEvent(e);
		return true;
	}
	case E_NX_EVENT_TERMINATE:
	{
		NxTerminateEvent *e = static_cast<NxTerminateEvent *>(event);
		TerminateEvent(e);
		return true;
	}

	default:
		break;
	}

	return QFrame::event(event);
}

void PlayerVideoFrame::resizeEvent(QResizeEvent *)
{
	if ((width() != DEFAULT_DSP_WIDTH) || (height() != DEFAULT_DSP_HEIGHT))
	{
		SetupUI();
	}
}

void PlayerVideoFrame::SetupUI()
{
	ui->graphicsView->setGeometry(0,0,width(),height());

	float widthRatio = (float)width() / DEFAULT_DSP_WIDTH;
	float heightRatio = (float)height() / DEFAULT_DSP_HEIGHT;
	int rx, ry, rw, rh;

	rx = widthRatio * ui->progressBar->x();
	ry = heightRatio * ui->progressBar->y();
	rw = widthRatio * ui->progressBar->width();
	rh = heightRatio * ui->progressBar->height();
	ui->progressBar->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->prevButton->x();
	ry = heightRatio * ui->prevButton->y();
	rw = widthRatio * ui->prevButton->width();
	rh = heightRatio * ui->prevButton->height();
	ui->prevButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->playButton->x();
	ry = heightRatio * ui->playButton->y();
	rw = widthRatio * ui->playButton->width();
	rh = heightRatio * ui->playButton->height();
	ui->playButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->pauseButton->x();
	ry = heightRatio * ui->pauseButton->y();
	rw = widthRatio * ui->pauseButton->width();
	rh = heightRatio * ui->pauseButton->height();
	ui->pauseButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->nextButton->x();
	ry = heightRatio * ui->nextButton->y();
	rw = widthRatio * ui->nextButton->width();
	rh = heightRatio * ui->nextButton->height();
	ui->nextButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->stopButton->x();
	ry = heightRatio * ui->stopButton->y();
	rw = widthRatio * ui->stopButton->width();
	rh = heightRatio * ui->stopButton->height();
	ui->stopButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->playListButton->x();
	ry = heightRatio * ui->playListButton->y();
	rw = widthRatio * ui->playListButton->width();
	rh = heightRatio * ui->playListButton->height();
	ui->playListButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->durationlabel->x();
	ry = heightRatio * ui->durationlabel->y();
	rw = widthRatio * ui->durationlabel->width();
	rh = heightRatio * ui->durationlabel->height();
	ui->durationlabel->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->subTitleLabel->x();
	ry = heightRatio * ui->subTitleLabel->y();
	rw = widthRatio * ui->subTitleLabel->width();
	rh = heightRatio * ui->subTitleLabel->height();
	ui->subTitleLabel->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->subTitleLabel2->x();
	ry = heightRatio * ui->subTitleLabel2->y();
	rw = widthRatio * ui->subTitleLabel2->width();
	rh = heightRatio * ui->subTitleLabel2->height();
	ui->subTitleLabel2->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->speedButton->x();
	ry = heightRatio * ui->speedButton->y();
	rw = widthRatio * ui->speedButton->width();
	rh = heightRatio * ui->speedButton->height();
	ui->speedButton->setGeometry(rx, ry, rw, rh);
}

void PlayerVideoFrame::StatusHomeEvent(NxStatusHomeEvent *)
{
	if (m_pRequestLauncherShow)
	{
		bool bOk = false;
		m_pRequestLauncherShow(&bOk);
		NXLOGI("[%s] REQUEST LAUNCHER SHOW <%s>", __FUNCTION__, bOk ? "OK" : "NG");
	}
}

void PlayerVideoFrame::StatusBackEvent(NxStatusBackEvent *)
{
	QApplication::postEvent(this, new NxTerminateEvent());
}

void PlayerVideoFrame::StatusVolumeEvent(NxStatusVolumeEvent *)
{
	if (m_pRequestVolume)
	{
		m_pRequestVolume();
	}
}

void PlayerVideoFrame::TerminateEvent(NxTerminateEvent *)
{
	if (m_pRequestTerminate)
	{
		m_pRequestTerminate();
	}
}

void PlayerVideoFrame::RegisterRequestTerminate(void (*cbFunc)(void))
{
	if (cbFunc)
	{
		m_pRequestTerminate = cbFunc;
	}
}

void PlayerVideoFrame::RegisterRequestVolume(void (*cbFunc)(void))
{
	if (cbFunc)
	{
		m_pRequestVolume = cbFunc;
	}
}


void PlayerVideoFrame::RegisterRequestLauncherShow(void (*cbFunc)(bool *bOk))
{
	if (cbFunc)
	{
		m_pRequestLauncherShow = cbFunc;
	}
}

void PlayerVideoFrame::on_speedButton_released()
{
	NX_MEDIA_STATE state = m_pNxPlayer->GetState();
    if((MP_STATE_STOPPED == state) || (MP_STATE_READY == state))
	{
		qDebug("Works when in play state.\n");
		ui->speedButton->setText("x 1");
		return;
	}

	//if ( m_pNxPlayer->GetVideoSpeedSupport() < 0)
	if (1)
	{
		if(m_bNotSupportSpeed)
		{
			return;
		}
        else
		{
			m_pMessageFrame->show();
			m_pMessageLabel->setText("\n  Not Support Speed !!\n  -Support file(.mp4,.mkv,.avi)\n  -Support Codec(h264, mpeg4)\n");
			m_bNotSupportSpeed = true;
			return;
		}
	}

	m_fSpeed = m_fSpeed * 2;

	if(m_fSpeed > 16)
	{
		m_fSpeed = 1.0;
	}

	if(m_fSpeed ==1.0) ui->speedButton->setText("x 1");
	else if(m_fSpeed ==2.0) ui->speedButton->setText("x 2");
	else if(m_fSpeed ==4.0) ui->speedButton->setText("x 4");
	else if(m_fSpeed ==8.0) ui->speedButton->setText("x 8");
	else if(m_fSpeed ==16.0) ui->speedButton->setText("x 16");

	//m_pNxPlayer->SetVideoSpeed( m_fSpeed  );

	if(m_fSpeed > 1.0)
	{
		QString style;
		style += "QProgressBar {";
		style += "  border: 2px solid grey;";
		style += "  border-radius: 5px;";
		style += "  background: grey;";
		style += "}";

		style += "QProgressBar::chunk {";
		style += "  background-color: rgb(37, 86, 201);";
		style += "width: 20px;";
		style += "}";

		ui->progressBar->setStyleSheet(style);
	}
	else
	{
		QString style;
		style += "QProgressBar {";
		style += "  border: 2px solid grey;";
		style += "  border-radius: 5px;";
		style += "  background: white;";
		style += "}";

		style += "QProgressBar::chunk {";
		style += "  background-color: rgb(37, 86, 201);";
		style += "width: 20px;";
		style += "}";

		ui->progressBar->setStyleSheet(style);
	}
}

void PlayerVideoFrame::slotOk()
{
	if(m_bNotSupportSpeed)
	{
		m_bNotSupportSpeed = false;
		m_pMessageFrame->hide();
	}
	else
	{
		ui->progressBar->setValue(0);
		UpdateDurationInfo(0, 0);
		StopVideo();
		m_pMessageFrame->hide();
	}}
