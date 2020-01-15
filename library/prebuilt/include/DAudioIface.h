#ifndef DAUDIOIFACE_H
#define DAUDIOIFACE_H

#include <NX_Type.h>

extern "C" {
	void Init(void *pObj, const char *pArgs);

	void IsInit(bool *bOk);

	void deInit();

	void Show();

	void Hide();

	void Raise();

	void Lower();

	// Launcher Show
	void RegisterRequestLauncherShow(void (*cbFunc)(bool *bOk));

	// Message
	void SendMessage(const char *pSrc, const char *pMsg, int32_t iMsgSize);

	void RegisterRequestSendMessage(void (*cbFunc)(const char *pDst, const char *pMsg, int32_t iMsgSize));

	// Popup Message
	void RegisterRequestPopupMessage(void (*cbFunc)(PopupMessage *, bool *));

	void RegisterRequestExpirePopupMessage(void (*cbFunc)());

	void PopupMessageResponse(bool bOk);

	// Notification
	void RegisterRequestNotification(void (*cbFunc)(PopupMessage *));

	void RegisterRequestExpireNotification(void (*cbFunc)());

	void NotificationResponse(bool bOk);

	// Audio Focus
	void RequestAudioFocus(FocusType eType, FocusPriority ePriority, bool *bOk);

	void RegisterRequestAudioFocus(void (*cbFunc)(FocusPriority ePriority, bool *bOk));

	void RequestAudioFocusTransient(FocusPriority ePriority, bool *bOk);

	void RegisterRequestAudioFocusTransient(void (*cbFunc)(FocusPriority ePriority, bool *bOk));

	void RegisterRequestAudioFocusLoss(void (*cbFunc)(void));

	// Video Focus
	void RequestVideoFocus(FocusType eType, FocusPriority ePriority, bool *bOk);

	void RegisterRequestVideoFocus(void (*cbFunc)(FocusPriority ePriority, bool *bOk));

	void RequestVideoFocusTransient(FocusPriority ePriority, bool *bOk);

	void RegisterRequestVideoFocusTransient(void (*cbFunc)(FocusPriority ePriority, bool *bOk));

	void RegisterRequestVideoFocusLoss(void (*cbFunc)(void));

	// Plug-in Run
	void RegisterRequestPlugInRun(void (*cbFunc)(const char *pPlugin, const char *pArgs));

	// Plug-in Terminate
	void RegisterRequestPlugInTerminate(void (*cbFunc)(const char *pPlugin));

	// Plug-in IsRunning
	void RegisterRequestPlugInIsRunning(void (*cbFunc)(const char *pPlugin, bool *bOk));

	// Application Terminate
	void RegisterRequestTerminate(void (*cbFunc)());

	// Volume
	void RegisterRequestVolume(void (*cbFunc)());

	// Media - disk insert/remove, scan done
	void MediaEventChanged(NxMediaEvent eEvent);

	void BackButtonClicked();
}

#endif // DAUDIOIFACE_H
