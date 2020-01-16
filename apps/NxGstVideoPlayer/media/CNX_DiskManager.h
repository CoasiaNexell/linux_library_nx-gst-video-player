#ifndef CNX_DISKMANAGER_H
#define CNX_DISKMANAGER_H

#include <QThread>
#include <QMap>

class CNX_DiskManager : public QThread
{
	Q_OBJECT

signals:
	void signalDetectUevent(uint32_t iEventType, uint8_t *pDevice);

public:
	CNX_DiskManager();

	~CNX_DiskManager();

	void Start();

	void Stop();

protected:
	virtual void run();

private:
	bool m_bThreadRun;

	QMap<QString, QString> m_DeviceMap;
};

#endif // CNX_DISKMANAGER_H
