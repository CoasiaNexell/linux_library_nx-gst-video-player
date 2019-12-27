#ifndef MAINFRAME_H
#define MAINFRAME_H

#include <QFrame>
#include <NX_Type.h>

//	Base UI
#include <CNX_StatusBar.h>
#include "NxEvent.h"

namespace Ui {
class MainFrame;
}

class MainFrame : public QFrame
{
	Q_OBJECT

public:
	static MainFrame *GetInstance(void *pObj);
	static MainFrame *GetInstance();
	static void DestroyInstance();

	bool Initialize();

	// Launcher Show
	static void RegisterRequestLauncherShow(void (*cbFunc)(bool *bOk));

	// Message
	void SendMessage(QString msg);
	static void RegisterRequestSendMessage(void (*cbFunc)(const char *pDst, const char *pMsg, int32_t iMsgSize));

	// Popup Message
	static void RegisterRequestPopupMessage(void (*cbFunc)(PopupMessage *, bool *));
	static void RegisterRequestExpirePopupMessage(void (*cbFunc)());
	void PopupMessageResponse(bool bOk);

	// Audio Focus
	void RequestAudioFocus(FocusType eType, FocusPriority ePriority, bool *bOk);
	static void RegisterRequestAudioFocus(void (*cbFunc)(FocusPriority ePriority, bool *bOk));
	void RequestAudioFocusTransient(FocusPriority ePriority, bool *bOk);
	static void RegisterRequestAudioFocusTransient(void (*cbFunc)(FocusPriority ePriority, bool *bOk));
	static void RegisterRequestAudioFocusLoss(void (*cbFunc)(void));

	// Video Focus
	void RequestVideoFocus(FocusType eType, FocusPriority ePriority, bool *bOk);
	static void RegisterRequestVideoFocus(void (*cbFunc)(FocusPriority ePriority, bool *bOk));
	void RequestVideoFocusTransient(FocusPriority ePriority, bool *bOk);
	static void RegisterRequestVideoFocusTransient(void (*cbFunc)(FocusPriority ePriority, bool *bOk));
	static void RegisterRequestVideoFocusLoss(void (*cbFunc)(void));
	static void RegisterRequestTerminate(void (*cbFunc)(void));
	static void RegisterRequestVolume(void (*cbFunc)(void));

	static void RegisterRequestOpacity(void (*cbFunc)(bool));

public:
	explicit MainFrame(QWidget *parent = 0);
	~MainFrame();

	static MainFrame *m_spInstance;
	void MediaEventChanged(NxMediaEvent eEvent);

protected:
	bool event(QEvent *e);

private:
	void TerminateEvent(NxTerminateEvent *e);

private:
	// Launcher Show
	static void (*m_pRequestLauncherShow)(bool *bOk);

	// Message
	static void (*m_pRequestSendMessage)(const char *pDst, const char *pMsg, int32_t iMsgSize);

	// Popup Message
	static void (*m_pRequestPopupMessage)(PopupMessage *, bool *);
	static void (*m_pReqeustExpirePopupMessage)();

	// Audio
	static void (*m_pRequestAudioFocus)(FocusPriority ePriority, bool *bOk);
	static void (*m_pRequestAudioFocusTransient)(FocusPriority ePriority, bool *bOk);
	static void (*m_pRequestAudioFocusLoss)(void);

	// Video
	static void (*m_pRequestVideoFocus)(FocusPriority ePriority, bool *bOk);
	static void (*m_pRequestVideoFocusTransient)(FocusPriority ePriority, bool *bOk);
	static void (*m_pRequestVideoFocusLoss)(void);

	// Terminate
	static void (*m_pRequestTerminate)(void);
	static void (*m_pRequestVolume)(void);

	// Focus
	bool m_bHasAudioFocus;
	bool m_bHasVideoFocus;

	bool m_bInitialized;

	//	UI Status Bar
	CNX_StatusBar* m_pStatusBar;

private:
	Ui::MainFrame *ui;
};

#endif // MAINFRAME_H
