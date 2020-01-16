#include "CNX_DiskManager.h"
#include <sys/inotify.h>
//#include <fcntl.h>
//#include <stdio.h>
#include <unistd.h>
#include <QDir>
#include <NxEvent.h>

#define LOG_TAG	"[CNX_DiskManager]"
#include <NX_Log.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUFFER_SIZE (1024*(EVENT_SIZE+16))
#define STORAGE_PATH "/dev/disk/by-uuid"

CNX_DiskManager::CNX_DiskManager()
{
	m_bThreadRun = false;
}

CNX_DiskManager::~CNX_DiskManager()
{
	Stop();
}

void CNX_DiskManager::run()
{
	int fd, wd, iEventType = 0;
	char buffer[BUFFER_SIZE];

	while (0 != access(STORAGE_PATH, F_OK))
	{
		usleep(100000);
	}

	QDir dir;
	dir.setPath("/dev/disk/by-uuid");
	foreach (QFileInfo f, dir.entryInfoList(QDir::Files))
	{
		m_DeviceMap[f.fileName()] = f.symLinkTarget();
	}

	m_bThreadRun = true;

	fd = inotify_init();
	if (fd < 0)
	{
		NXLOGE("[error] inotify_init()");
		return;
	}

	wd = inotify_add_watch(fd, STORAGE_PATH, IN_CREATE|IN_DELETE);
	if (wd < 0)
	{
		NXLOGE("[error] inotify_add_watch(%s)", STORAGE_PATH);
		return;
	}

	while (m_bThreadRun)
	{
		int length = read(fd, buffer, BUFFER_SIZE);
		int i = 0;

		if (length < 0)
		{
			printf("[error] read()\n");
			break;
		}

		while (i < length)
		{
			struct inotify_event *event = (struct inotify_event *)&buffer[i];

			if (event->mask & IN_CREATE)
			{
				char devpath[1024] = {0,};
				char target[1024] = {0,};
				int pos = 0;
				size_t count = 0;
				sprintf(devpath, "/dev/disk/by-uuid/%s", event->name);
				if (0 != access(devpath, F_OK))
				{
					i += EVENT_SIZE + event->len;
					continue;
				}

				count = readlink(devpath, target, sizeof(target));
				if (count <= 0)
					target[count] = '\0';

				for (size_t i = 0; i < count; ++i)
				{
					if (target[i] == '/')
						pos = i+1;
				}

				sprintf(devpath, "/dev/%s", target+pos);
				m_DeviceMap[event->name] = devpath;
				iEventType = strstr(devpath, "mmcblk") ? E_NX_EVENT_SDCARD_INSERT : E_NX_EVENT_USB_INSERT;
				emit signalDetectUevent(iEventType, (uint8_t*)devpath);
			}
			else if (event->mask & IN_DELETE)
			{
				if (m_DeviceMap.find(event->name) != m_DeviceMap.end())
				{
					iEventType = strstr(m_DeviceMap[event->name].toStdString().c_str(), "mmcblk" ) ? E_NX_EVENT_SDCARD_REMOVE : E_NX_EVENT_USB_REMOVE;
					emit signalDetectUevent(iEventType, (uint8_t*)m_DeviceMap[event->name].toStdString().c_str());
				}
			}

			i += EVENT_SIZE + event->len;
		}
	}

	inotify_rm_watch(fd, wd);
	close(fd);
}

void CNX_DiskManager::Start()
{
	start();
}

void CNX_DiskManager::Stop()
{
	m_bThreadRun = false;

	if (isRunning())
	{
		wait();
		quit();
	}
}
