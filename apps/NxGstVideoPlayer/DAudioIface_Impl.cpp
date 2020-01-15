#include <DAudioIface.h>
#include "MainFrame.h"


#define LOG_TAG "[NxVideoMainFrame]"
#include <NX_Log.h>

void Init(void *pObj, const char *pArgs)
{
	(void)pArgs;
	MainFrame *p = MainFrame::GetInstance(pObj);
	if (!p)
	{
		NXLOGE("[%s] Instance does not exists.", __FUNCTION__);
		return;
	}

	if (!p->Initialize())
	{
		NXLOGE("[%s] Initialize().", __FUNCTION__);
		deInit();
	}
}

void IsInit(bool *bOk)
{
	*bOk = MainFrame::GetInstance() ? true : false;
}

void deInit()
{
	MainFrame::DestroyInstance();
}

void Show()
{
	NXLOGI("[%s]", __FUNCTION__);
	MainFrame *p = MainFrame::GetInstance();
	if (p)
		p->show();
}

void Hide()
{
	NXLOGI("[%s]", __FUNCTION__);
	MainFrame *p = MainFrame::GetInstance();
	if (p)
		p->hide();
}

void Raise()
{
	NXLOGI("[%s]", __FUNCTION__);
	MainFrame *p = MainFrame::GetInstance();
	if (p)
	{
		if (p->isHidden())
		{
			NXLOGI("[%s] <HIDDEN>", __FUNCTION__);
			p->show();
		}
		p->raise();
	}
}

void Lower()
{
	NXLOGI("[%s]", __FUNCTION__);
	MainFrame *p = MainFrame::GetInstance();
	if (p)
		p->lower();
}

// Launcher Show
void RegisterRequestLauncherShow(void (*cbFunc)(bool *bOk))
{
	MainFrame::RegisterRequestLauncherShow(cbFunc);
}

// Send Message
void SendMessage(const char *pMsg, int32_t iMsgSize)
{
	MainFrame *p = MainFrame::GetInstance();
	if (p)
		p->SendMessage(pMsg);
}

void RegisterRequestSendMessage(void (*cbFunc)(const char *pDst, const char *pMsg, int32_t iMsgSize))
{
	MainFrame::RegisterRequestSendMessage(cbFunc);
}

// Popup Message
void RegisterRequestPopupMessage(void (*cbFunc)(PopupMessage *, bool *))
{
	MainFrame::RegisterRequestPopupMessage(cbFunc);
}

void RegisterRequestExpirePopupMessage(void (*cbFunc)())
{
	MainFrame::RegisterRequestExpirePopupMessage(cbFunc);
}

void PopupMessageResponse(bool bOk)
{
	MainFrame *p = MainFrame::GetInstance();
	if (p)
		p->PopupMessageResponse(bOk);
}

// Audio Focus
void RequestAudioFocus(FocusType eType, FocusPriority ePriority, bool *bOk)
{
	MainFrame *p = MainFrame::GetInstance();
	if (p)
		p->RequestAudioFocus(eType, ePriority, bOk);
}

void RegisterRequestAudioFocus(void (*cbFunc)(FocusPriority ePriority, bool *bOk))
{
	MainFrame::RegisterRequestAudioFocus(cbFunc);
}

void RequestAudioFocusTransient(FocusPriority ePriority, bool *bOk)
{
	MainFrame *p = MainFrame::GetInstance();
	if (p)
		p->RequestAudioFocusTransient(ePriority, bOk);
}

void RegisterRequestAudioFocusTransient(void (*cbFunc)(FocusPriority ePriority, bool *bOk))
{
	MainFrame::RegisterRequestAudioFocusTransient(cbFunc);
}

void RegisterRequestAudioFocusLoss(void (*cbFunc)(void))
{
	MainFrame::RegisterRequestAudioFocusLoss(cbFunc);
}

// Video Focus
void RequestVideoFocus(FocusType eType, FocusPriority ePriority, bool *bOk)
{
	MainFrame *p = MainFrame::GetInstance();
	if (p)
		p->RequestVideoFocus(eType, ePriority,bOk);
}

void RegisterRequestVideoFocus(void (*cbFunc)(FocusPriority ePriority, bool *bOk))
{
	MainFrame::RegisterRequestVideoFocus(cbFunc);
}

void RequestVideoFocusTransient(FocusPriority ePriority, bool *bOk)
{
	MainFrame *p = MainFrame::GetInstance();
	if (p)
		p->RequestVideoFocusTransient(ePriority,bOk);
}

void RegisterRequestVideoFocusTransient(void (*cbFunc)(FocusPriority ePriority, bool *bOk))
{
	MainFrame::RegisterRequestVideoFocusTransient(cbFunc);
}

void RegisterRequestVideoFocusLoss(void (*cbFunc)(void))
{
	MainFrame::RegisterRequestVideoFocusLoss(cbFunc);
}

void RegisterRequestPlugInRun(void (*cbFunc)(const char *pPlugin, const char *pArgs))
{

}

void RegisterRequestPlugInTerminate(void (*cbFunc)(const char *pPlugin))
{

}

void RegisterRequestTerminate(void (*cbFunc)())
{
	MainFrame::RegisterRequestTerminate(cbFunc);
}

void RegisterRequestVolume(void (*cbFunc)())
{
	MainFrame::RegisterRequestVolume(cbFunc);
}

void MediaEventChanged(NxMediaEvent eEvent)
{
	MainFrame *p = MainFrame::GetInstance();
	if (p)
		p->MediaEventChanged(eEvent);
}
