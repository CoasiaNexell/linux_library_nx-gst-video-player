#include "MainFrame.h"
#include "ui_MainFrame.h"
#include <QDebug>
#include <QDesktopWidget>

#define LOG_TAG "[NxVideoMainFrame]"
#include <NX_Log.h>

#define DEFAULT_WIDTH	1024
#define DEFAULT_HEIGHT	600

#define NX_CUSTOM_BASE_MAIN QEvent::User
enum
{
	NX_CUSTOM_BASE_VIDEO_MUTE_START = NX_CUSTOM_BASE_MAIN+1,
	NX_CUSTOM_BASE_VIDEO_MUTE_STOP
};

class VideoMuteEventStart : public QEvent
{
public:
	VideoMuteEventStart() :
		QEvent((QEvent::Type)NX_CUSTOM_BASE_VIDEO_MUTE_START)
	{

	}
};

class VideoMuteEventStop : public QEvent
{
public:
	VideoMuteEventStop() :
		QEvent((QEvent::Type)NX_CUSTOM_BASE_VIDEO_MUTE_STOP)
	{

	}
};


MainFrame* MainFrame::m_spInstance = NULL;

// Launcher Show
void (*MainFrame::m_pRequestLauncherShow)(bool *bOk) = NULL;

// Message
void (*MainFrame::m_pRequestSendMessage)(const char *pDst, const char *pMsg, int32_t iMsgSize) = NULL;

// Popup Message
void (*MainFrame::m_pRequestPopupMessage)(PopupMessage *, bool *) = NULL;
void (*MainFrame::m_pReqeustExpirePopupMessage)() = NULL;

// Audio
void (*MainFrame::m_pRequestAudioFocus)(FocusPriority ePriority, bool *bOk) = NULL;
void (*MainFrame::m_pRequestAudioFocusTransient)(FocusPriority ePriority, bool *bOk) = NULL;
void (*MainFrame::m_pRequestAudioFocusLoss)(void) = NULL;

// Video
void (*MainFrame::m_pRequestVideoFocus)(FocusPriority ePriority, bool *bOk) = NULL;
void (*MainFrame::m_pRequestVideoFocusTransient)(FocusPriority ePriority, bool *bOk) = NULL;
void (*MainFrame::m_pRequestVideoFocusLoss)(void) = NULL;

// Terminate
void (*MainFrame::m_pRequestTerminate)(void) = NULL;
void (*MainFrame::m_pRequestVolume)(void) = NULL;

MainFrame::MainFrame(QWidget *parent) :
	QFrame(parent),
	ui(new Ui::MainFrame)
{
	ui->setupUi(this);

	const QRect screen = QApplication::desktop()->screenGeometry();

	if ((width() != screen.width()) || (height() != screen.height()))
	{
		setFixedSize(screen.width(), screen.height());
	}

	//  Initialize
	m_bInitialized = false;
	//  Focus
	m_bHasAudioFocus = false;
	m_bHasVideoFocus = false;

	// Register Request function
	ui->m_PlayerFrame->RegisterRequestTerminate(m_pRequestTerminate);
	ui->m_PlayerFrame->RegisterRequestLauncherShow(m_pRequestLauncherShow);
	ui->m_PlayerFrame->RegisterRequestVolume(m_pRequestVolume);
}

MainFrame::~MainFrame()
{
	delete ui;
}

bool MainFrame::Initialize()
{
	bool bOk = false;
	if (m_bInitialized)
	{
		return true;
	}

	if (!m_pRequestVideoFocus)
	{
		NXLOGE("[%s] REQUEST VIDEO FOCUS does not exist.", __FUNCTION__);
		return false;
	}

	m_pRequestVideoFocus(FocusPriority_Normal, &bOk);
	if (!bOk)
	{
		NXLOGE("[%s] REQUEST VIDEO FOCUS <FAIL>", __FUNCTION__);
		return false;
	}

	if (!m_pRequestAudioFocus)
	{
		NXLOGE("[%s] REQUEST AUDIO FOCUS does not exist.", __FUNCTION__);
		return false;
	}

	m_pRequestAudioFocus(FocusPriority_Normal, &bOk);
	if (!bOk)
	{
		NXLOGE("[%s] REQUEST AUDIO FOCUS <FAIL>", __FUNCTION__);
		if (m_pRequestVideoFocusLoss)
		{
			m_pRequestVideoFocusLoss();
		}
		return false;
	}

	ui->m_PlayerFrame->PlaySeek();

	if (isHidden())
	{
		show();
	}
	raise();

	m_bInitialized = true;
	return true;
}

bool MainFrame::event(QEvent *event)
{
	switch ((int32_t)event->type())
	{
	case NX_CUSTOM_BASE_VIDEO_MUTE_START:
	{
		// When click home button, back ground is shown.
		// So after running sleep, run videoMute.
		usleep(100*1000);
		NXLOGV("===========VideoMute start ++++ ==========\n");
		ui->m_PlayerFrame->VideoMuteStart();
		NXLOGV("===========VideoMute start ---- ==========\n");
		return true;
	}
	case NX_CUSTOM_BASE_VIDEO_MUTE_STOP:
	{
		// When click back button, back ground is shown.
		// So after running sleep, run videoMute.
		usleep(100*1000);
		NXLOGV("===========VideoMute stop ++++ ==========\n");
		ui->m_PlayerFrame->VideoMuteStop();
		NXLOGV("===========VideoMute stop ---- ==========\n");
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

void MainFrame::TerminateEvent(NxTerminateEvent *)
{
	if (m_pRequestTerminate)
	{
		m_pRequestTerminate();
	}
}

MainFrame* MainFrame::GetInstance(void *pObj)
{
	if (!m_spInstance)
	{
		m_spInstance = new MainFrame((QWidget *)pObj);
	}

	return m_spInstance;
}

MainFrame* MainFrame::GetInstance()
{
	return m_spInstance;
}

void MainFrame::DestroyInstance()
{
	if (m_spInstance)
	{
		delete m_spInstance;
		m_spInstance = NULL;
	}
}

void MainFrame::RegisterRequestLauncherShow(void (*cbFunc)(bool *bOk))
{
	if (cbFunc)
	{
		m_pRequestLauncherShow = cbFunc;
	}
}

// Message
void MainFrame::SendMessage(QString msg)
{
	NXLOGI("[%s] %s", __FUNCTION__, msg.toStdString().c_str());
}

void MainFrame::RegisterRequestSendMessage(void (*cbFunc)(const char *pDst, const char *pMsg, int32_t iMsgSize))
{
	if (cbFunc)
	{
		m_pRequestSendMessage = cbFunc;
	}
}

// Audio Focus
void MainFrame::RequestAudioFocus(FocusType eType, FocusPriority ePriority, bool *bOk)
{
	if (eType == FocusType_Get)
	{
		FocusPriority eCurrPriority = FocusPriority_Normal;
		if (eCurrPriority > ePriority)
		{
			*bOk = false;
		}
		else
		{
			*bOk = true;
		}

		m_bHasAudioFocus = *bOk ? false : true;
		if(m_bHasAudioFocus == false)
		{
			ui->m_PlayerFrame->SaveInfo();
			ui->m_PlayerFrame->StopVideo();
			QApplication::postEvent(this, new NxTerminateEvent());
		}
	}
	else
	{
		ui->m_PlayerFrame->setAudioFocus(true);
		ui->m_PlayerFrame->PlaySeek();
		m_bHasAudioFocus = true;
	}
}

void MainFrame::RegisterRequestAudioFocus(void (*cbFunc)(FocusPriority ePriority, bool *bOk))
{
	if (cbFunc)
	{
		m_pRequestAudioFocus = cbFunc;
	}
}

void MainFrame::RequestAudioFocusTransient(FocusPriority ePriority, bool *bOk)
{
	FocusPriority eCurrPriority = FocusPriority_Normal;

	if (eCurrPriority > ePriority)
	{
		*bOk = false;
	}
	else
	{
		ui->m_PlayerFrame->SaveInfo();
		ui->m_PlayerFrame->StopVideo();
		ui->m_PlayerFrame->setAudioFocus(false);
		*bOk = true;
	}

	m_bHasAudioFocus = *bOk ? false : true;
}

void MainFrame::RegisterRequestAudioFocusTransient(void (*cbFunc)(FocusPriority ePriority, bool *bOk))
{
	if (cbFunc)
	{
		m_pRequestAudioFocusTransient = cbFunc;
	}
}

void MainFrame::RegisterRequestAudioFocusLoss(void (*cbFunc)(void))
{
	if (cbFunc)
	{
		m_pRequestAudioFocusLoss = cbFunc;
	}
}

// Video Focus
void MainFrame::RequestVideoFocus(FocusType eType, FocusPriority ePriority, bool *bOk)
{
	if (eType == FocusType_Get)
	{
		FocusPriority eCurrPriority = FocusPriority_Normal;

		if (eCurrPriority > ePriority)
			*bOk = false;
		else
			*bOk = true;
		m_bHasVideoFocus = *bOk ? false : true;

		if(!m_bHasVideoFocus)
		{
//			QApplication::postEvent(this, new VideoMuteEventStart());
			ui->m_PlayerFrame->VideoMuteStart();
		}
		ui->m_PlayerFrame->setVideoFocus(m_bHasVideoFocus);
	}
	else // FocusType_Set
	{
		*bOk = true;
		m_bHasVideoFocus = true;

		//video mute stop
		if(ui->m_PlayerFrame->GetVideoMuteStatus())
		{
			QApplication::postEvent(this, new VideoMuteEventStop());
		}
		ui->m_PlayerFrame->setVideoFocus(m_bHasVideoFocus);

		if (isHidden())
			show();
		raise();
	}
}

void MainFrame::RegisterRequestVideoFocus(void (*cbFunc)(FocusPriority ePriority, bool *bOk))
{
	if (cbFunc)
	{
		m_pRequestVideoFocus= cbFunc;
	}
}

void MainFrame::RequestVideoFocusTransient(FocusPriority ePriority, bool *bOk)
{
	FocusPriority eCurrPriority  = FocusPriority_Normal;

	if (eCurrPriority > ePriority)
	{
		*bOk = false;
	}
	else
	{
		*bOk = true;
	}

	m_bHasVideoFocus = *bOk ? false : true;
}

void MainFrame::RegisterRequestVideoFocusTransient(void (*cbFunc)(FocusPriority ePriority, bool *bOk))
{
	if (cbFunc)
	{
		m_pRequestVideoFocusTransient = cbFunc;
	}
}

void MainFrame::RegisterRequestVideoFocusLoss(void (*cbFunc)(void))
{
	if (cbFunc)
	{
		m_pRequestVideoFocusLoss = cbFunc;
	}
}

void MainFrame::RegisterRequestTerminate(void (*cbFunc)(void))
{
	if (cbFunc)
	{
		m_pRequestTerminate = cbFunc;
	}
}

void MainFrame::RegisterRequestVolume(void (*cbFunc)(void))
{
	if (cbFunc)
	{
		m_pRequestVolume = cbFunc;
	}
}

void MainFrame::RegisterRequestPopupMessage(void (*cbFunc)(PopupMessage *, bool *))
{
	if (cbFunc)
	{
		m_pRequestPopupMessage = cbFunc;
	}
}

void MainFrame::RegisterRequestExpirePopupMessage(void (*cbFunc)())
{
	if (cbFunc)
	{
		m_pReqeustExpirePopupMessage = cbFunc;
	}
}

void MainFrame::PopupMessageResponse(bool bOk)
{
	NXLOGI("[%s] <%s>", __FUNCTION__, bOk ? "ACCEPT" : "REJECT");
}

void MainFrame::MediaEventChanged(NxMediaEvent eEvent)
{
	switch (eEvent)
	{
#if 0
	case NX_EVENT_MEDIA_SDCARD_INSERT:
	{
		NXLOGD("############### NX_EVENT_MEDIA_SDCARD_INSERT !!!\n");
		break;
	}
#endif
	case NX_EVENT_MEDIA_SDCARD_REMOVE:
	{
		NXLOGD("############### NX_EVENT_MEDIA_SDCARD_REMOVE !!!\n");
		ui->m_PlayerFrame->StorageRemoved();
		break;
	}
#if 0
	case NX_EVENT_MEDIA_USB_INSERT:
	{
		NXLOGD("############### NX_EVENT_MEDIA_USB_INSERT !!!\n");
		break;
	}
#endif
	case NX_EVENT_MEDIA_USB_REMOVE:
	{
		NXLOGD("############### NX_EVENT_MEDIA_USB_REMOVE !!!\n");
		ui->m_PlayerFrame->StorageRemoved();
		break;
	}
	case NX_EVENT_MEDIA_SCAN_DONE:
	{
		NXLOGD("############### NX_EVENT_MEDIA_SCAN_DONE !!!\n");
		ui->m_PlayerFrame->StorageScanDone();
		break;
	}
	default:
		break;
	}
}
