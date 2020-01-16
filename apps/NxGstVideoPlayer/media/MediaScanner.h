#ifndef MEDIASCANNER_H
#define MEDIASCANNER_H

#include <QThread>
#include <QTimer>
#include "CNX_UeventManager.h"
#include "CNX_VolumeManager.h"
#include "CNX_MediaScanner.h"
#include "CNX_DiskManager.h"
#include "../NxEvent.h"

class MediaScanner : public QThread
{
	Q_OBJECT

signals:
	void signalMediaEvent(NxEventTypes eType);

private slots:
	void slotDetectUEvent(QString action, QString devNode);

	void slotStartTimer();

	void slotScanDone();

public:
	MediaScanner();

	~MediaScanner();

	CNX_MediaScanner *GetMediaScanner();

protected:
	virtual void run();

private:
	void Start(int msec);

	void Mount(QString devNode);

	void Umount(QString devNode);

private:
	CNX_UeventManager *m_pUeventManager;
	CNX_VolumeManager *m_pVolumeManager;
	CNX_MediaScanner *m_pMediaScanner;

	QTimer m_StartTimer;
};

#endif // MEDIASCANNER_H
