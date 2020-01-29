#ifndef NXEVENT_H
#define NXEVENT_H

#include <QEvent>
#include <NX_Type.h>
#define NX_BASE_EVENT_TYPE	(QEvent::User+100)

enum NxEventTypes {
	E_NX_EVENT_STATUS_HOME = NX_BASE_EVENT_TYPE,
	E_NX_EVENT_STATUS_BACK,
	E_NX_EVENT_STATUS_VOLUME,
	E_NX_EVENT_SDCARD_MOUNT,
	E_NX_EVENT_SDCARD_UMOUNT,
	E_NX_EVENT_SDCARD_INSERT,
	E_NX_EVENT_SDCARD_REMOVE,
	E_NX_EVENT_USB_INSERT,
	E_NX_EVENT_USB_REMOVE,
	E_NX_EVENT_USB_MOUNT,
	E_NX_EVENT_USB_UMOUNT,
	E_NX_EVENT_HDMI_CONNECTED,
	E_NX_EVENT_HDMI_DISCONNECTED,
	E_NX_EVENT_MEDIA_SCAN_DONE,
	E_NX_EVENT_TERMINATE
};

class NxStatusHomeEvent : public QEvent
{
public:
	NxStatusHomeEvent() :
		QEvent((QEvent::Type)E_NX_EVENT_STATUS_HOME)
	{

	}
};

class NxStatusBackEvent : public QEvent
{
public:
	NxStatusBackEvent() :
		QEvent((QEvent::Type)E_NX_EVENT_STATUS_BACK)
	{

	}
};

class NxStatusVolumeEvent : public QEvent
{
public:
	NxStatusVolumeEvent() :
		QEvent((QEvent::Type)E_NX_EVENT_STATUS_VOLUME)
	{

	}
};

class NxTerminateEvent : public QEvent
{
public:
	NxTerminateEvent() :
		QEvent((QEvent::Type)E_NX_EVENT_TERMINATE)
	{

	}
};

#endif // NXEVENT_H
