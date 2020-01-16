#include "MediaScanner.h"
#include "MediaConf.h"
#include <sys/mount.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <QDir>

#define LOG_TAG "[MediaScanner]"
#include <NX_Log.h>

#define MOUNTPOINT_DIR	"/tmp/media"

MediaScanner::MediaScanner()
{
	m_pUeventManager = new CNX_UeventManager();
	connect(m_pUeventManager, SIGNAL(signalDetectUEvent(QString,QString)), this, SLOT(slotDetectUEvent(QString,QString)));

	m_pVolumeManager = new CNX_VolumeManager();
	m_pVolumeManager->Start();

	m_pMediaScanner = new CNX_MediaScanner();
	connect(m_pMediaScanner, SIGNAL(signalScanDone()), this, SLOT(slotScanDone()));

	connect(&m_StartTimer, SIGNAL(timeout()), this, SLOT(slotStartTimer()));

	NX_VOLUME_INFO *pVolumeInfo = NULL;
	int32_t iVolumeNum = 0;

	m_pVolumeManager->GetMount(&pVolumeInfo, &iVolumeNum);
	for( int32_t i = 0; i < iVolumeNum; i++ )
	{
		Umount(pVolumeInfo[i].szVolume);
	}

	QDir dir("/dev/disk/by-uuid");
	foreach (QFileInfo f, dir.entryInfoList(QDir::NoDotAndDotDot|QDir::System)) {
		QStringList tokens = f.symLinkTarget().split("/");
		QString nodeName = tokens[tokens.size()-1];

		if (nodeName.indexOf("mmcblk0") == 0)
			continue;

		Mount(f.symLinkTarget());
	}

	Start(3000);
}

MediaScanner::~MediaScanner()
{
	if (m_pUeventManager)
		delete m_pUeventManager;
	if (m_pVolumeManager)
		delete m_pVolumeManager;
	if (m_pMediaScanner)
		delete m_pMediaScanner;
}

void MediaScanner::slotDetectUEvent(QString action, QString devNode)
{
	if (action == "add")
	{
		Mount(devNode);
	}
	else if (action == "remove")
	{
		if (devNode.indexOf("/dev/mmcblk") == 0)
			emit signalMediaEvent(E_NX_EVENT_SDCARD_REMOVE);
		else
			emit signalMediaEvent(E_NX_EVENT_USB_REMOVE);

		Umount(devNode);
	}

	Start(3000);
}

void MediaScanner::Mount(QString devNode)
{
	QDir dir(MOUNTPOINT_DIR);
	if (!dir.exists())
	{
		dir.mkpath(MOUNTPOINT_DIR);
	}

	if (dir.exists())
	{
		QStringList tokens = devNode.split("/");
		dir.mkdir(tokens[tokens.size()-1]);
		QString mountpoint = QString("%1/%2").arg(MOUNTPOINT_DIR).arg(tokens[tokens.size()-1]);
		dir.mkpath(mountpoint);

		if (0 > ::mount(devNode.toStdString().c_str(),
				mountpoint.toStdString().c_str(),
				"vfat",
				0,
				"utf8=1"))
		{
			dir.rmdir(mountpoint);
		}
	}
}

void MediaScanner::Umount(QString devNode)
{
	QStringList tokens = devNode.split("/");
	QString mountpoint = QString("%1/%2").arg(MOUNTPOINT_DIR).arg(tokens[tokens.size()-1]);

	FILE *fp = popen("lsof | grep /tmp/media/", "r");
	if (fp)
	{
		char line[2048];
		int stx = 0;
		int etx = 0;
		char pid[10] = {0,};

		while (fgets(line, 2048, fp))
		{
			stx = etx = 0;
			memset(pid, 0, 10);
			for (size_t i = 1, k = 0; i < strlen(line); ++i)
			{
				if (!stx && (line[i-1] == ' ' && (line[i] >= '0' && line[i] <= '9')))
				{
					stx = i;
				}
				else if (stx && (line[i-1] >= '0' && line[i-1] <= '9') && line[i] == ' ')
				{
					etx = i-1;
				}

				if (stx && !etx)
				{
					pid[k++] = line[i];
				}
				else if (stx && etx)
				{
					if (atoi(pid) != getpid())
						kill(atoi(pid), SIGTERM);
					break;
				}
			}
		}

		pclose(fp);
	}

	if (!QFile::exists(mountpoint))
	{
		return;
	}

	int ret = ::umount2(mountpoint.toStdString().c_str(), MNT_FORCE);
	for (int i = 0; ret < 0; ++i)
	{
		NXLOGE("[%s] [%d] umount2(): %s", __func__, i, strerror(ret));
		usleep(100000);

		ret = ::umount2(mountpoint.toStdString().c_str(), MNT_FORCE);
	}
}

void MediaScanner::slotScanDone()
{
	emit signalMediaEvent(E_NX_EVENT_MEDIA_SCAN_DONE);
}

void MediaScanner::Start(int msec)
{
	m_StartTimer.stop();
	m_StartTimer.start(msec);
}

void MediaScanner::slotStartTimer()
{
	m_StartTimer.stop();

	start();
}

void MediaScanner::run()
{
	NX_VOLUME_INFO *pVolumeInfo = NULL;
	int32_t iVolumeNum = 0;

	m_pVolumeManager->GetMount( &pVolumeInfo, &iVolumeNum );

	char *pDirectory[iVolumeNum];
	for( int32_t i = 0; i < iVolumeNum; i++ )
	{
		pDirectory[i] = (char*)malloc( sizeof(char) * 256 );
		sprintf( pDirectory[i], "%s", pVolumeInfo[i].szVolume );
	}

	m_pMediaScanner->Scan( pDirectory, iVolumeNum);

	for( int32_t i = 0; i < iVolumeNum; i++ )
	{
		free( pDirectory[i] );
	}
}
