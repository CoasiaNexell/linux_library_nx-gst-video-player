//------------------------------------------------------------------------------
//
//	Copyright (C) 2017 Nexell Co. All Rights Reserved
//	Nexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		:
//	File		:
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <QDir>

#include "CNX_VolumeManager.h"

#define LOG_TAG	"[CNX_VolumeManager]"
#include <NX_Log.h>

//------------------------------------------------------------------------------
const char *pstDefaultDeviceReserved[] = {
	"mmcblk0",
};

const char *pstDefaultMountPosition[] = {
	"/tmp/media", "/run/media",
};

//------------------------------------------------------------------------------
CNX_VolumeManager::CNX_VolumeManager()
{
	m_bThreadRun = false;
	m_ppDeviceReserved = NULL;
	m_ppMountPosition = NULL;
	m_iNumDeviceReserved = 0;
	m_iNumMountPosition = 0;

	m_ppDeviceReserved   = pstDefaultDeviceReserved;
	m_iNumDeviceReserved = sizeof(pstDefaultDeviceReserved) / sizeof(pstDefaultDeviceReserved[0]);

	m_ppMountPosition    = pstDefaultMountPosition;
	m_iNumMountPosition  = sizeof(pstDefaultMountPosition) / sizeof(pstDefaultMountPosition[0]);
}

//------------------------------------------------------------------------------
CNX_VolumeManager::~CNX_VolumeManager()
{
	Stop();
}

//------------------------------------------------------------------------------
void CNX_VolumeManager::SetDeviceReserved( const char **ppDevice, int32_t iDeviceNum )
{
	m_ppDeviceReserved   = ppDevice;
	m_iNumDeviceReserved = iDeviceNum;
}

//------------------------------------------------------------------------------
void CNX_VolumeManager::SetMountPosition( const char **ppMount, int32_t iMountNum )
{
	m_ppMountPosition   = ppMount;
	m_iNumMountPosition = iMountNum;
}

//------------------------------------------------------------------------------
int32_t CNX_VolumeManager::GetMount( NX_VOLUME_INFO **ppVolume, int32_t *iVolumeNum )
{
	FILE *pFile = fopen( "/proc/mounts", "r");
	if( NULL == pFile )
		return -1;

	char szTemp[MAX_VOLUME_STR];
	int32_t iCount = 0;

	while( NULL != fgets(szTemp, sizeof(szTemp), pFile) )
	{
		char szDev[MAX_VOLUME_STR], szVolume[MAX_VOLUME_STR], szType[MAX_VOLUME_STR];
		sscanf( szTemp, "%s %s %s", szDev, szVolume, szType );

		if( NULL == strchr( szDev, '/' ) )
			continue;

		if( IsDeviceReserved( szDev ) )
			continue;

		if( !IsMountPosition( szVolume ) )
			continue;

		memset( &m_Volume[iCount], 0x00, sizeof(NX_VOLUME_INFO) );
		sprintf( m_Volume[iCount].szDev, "%s", szDev );
		sprintf( m_Volume[iCount].szVolume, "%s", szVolume );
		m_Volume[iCount].iType = GetMountType( szType );
		iCount++;
	}

	*ppVolume   = m_Volume;
	*iVolumeNum = iCount;

	fclose( pFile );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_VolumeManager::IsMounted( char *pDevice )
{
	FILE *pFile = fopen( "/proc/mounts", "r");
	if( NULL == pFile )
		return false;

	char szTemp[MAX_VOLUME_STR];
	while( NULL != fgets(szTemp, sizeof(szTemp), pFile) )
	{
		char szDev[MAX_VOLUME_STR], szVolume[MAX_VOLUME_STR];
		sscanf( szTemp, "%s %s", szDev, szVolume );

		if( NULL == strchr( szDev, '/' ) )
			continue;

		if( IsDeviceReserved( szDev ) )
			continue;

		if( !IsMountPosition( szVolume ) )
			continue;

		if( !strcmp(szDev, pDevice) )
		{
			fclose( pFile );
			return true;
		}
	}

	fclose( pFile );
	return false;
}

//------------------------------------------------------------------------------
void CNX_VolumeManager::Start()
{
	start();
}

//------------------------------------------------------------------------------
void CNX_VolumeManager::Stop()
{
	if (isRunning())
	{
		m_bThreadRun = false;

		wait();
		quit();
	}
}

//------------------------------------------------------------------------------
void CNX_VolumeManager::run()
{
	m_bThreadRun = true;
	{
		NX_VOLUME_INFO *pVolumeInfo = NULL;
		int32_t iVolumeNum = 0;

		GetMount( &pVolumeInfo, &iVolumeNum );

		//	Insert External Storage
		for( int32_t i = 0; i < iVolumeNum; i++ )
		{
			int32_t iEventType = strstr("mmcblk", pVolumeInfo[i].szDev) ? E_NX_EVENT_SDCARD_MOUNT : E_NX_EVENT_USB_MOUNT;
			emit signalDetectUevent(iEventType, (uint8_t*)pVolumeInfo[i].szDev);
		}

		memcpy( m_CurVolume, m_Volume, sizeof(NX_VOLUME_INFO) * iVolumeNum );
		m_iCurVolumeNum = iVolumeNum;
	}

	int32_t hMount = open( "/proc/mounts", O_RDONLY );
	if( 0 > hMount )
		return;

	while( m_bThreadRun )
	{
		struct pollfd hPoll;
		hPoll.fd      = hMount;
		hPoll.events  = POLLPRI;
		hPoll.revents = 0;

		int32_t iErr = poll( &hPoll, 1, 1000 );
		if( 0 < iErr )
		{
			if( hPoll.revents & POLLPRI )
			{
				NX_VOLUME_INFO *pVolumeInfo = NULL;
				int32_t iVolumeNum = 0;

				GetMount( &pVolumeInfo, &iVolumeNum );

				if( m_iCurVolumeNum < iVolumeNum )		// INSERT
				{
					for( int32_t i = 0; i < iVolumeNum; i++ )
					{
						int32_t bChanged = true;
						for( int32_t j = 0; j < m_iCurVolumeNum; j++ )
						{
							if( !strcmp(pVolumeInfo[i].szDev, m_CurVolume[j].szDev) &&
									!strcmp(pVolumeInfo[i].szVolume, m_CurVolume[j].szVolume) &&
									(pVolumeInfo[i].iType == m_CurVolume[j].iType) )
							{
								bChanged = false;
								break;
							}
						}

						if( bChanged )
						{
							int32_t iEventType = strstr( pVolumeInfo[i].szDev, "mmcblk" ) ? E_NX_EVENT_SDCARD_MOUNT : E_NX_EVENT_USB_MOUNT;
							emit signalDetectUevent(iEventType, (uint8_t*)pVolumeInfo[i].szDev);
						}
					}
				}
				else if( m_iCurVolumeNum > iVolumeNum )	// REMOVE
				{
					for( int32_t i = 0; i < m_iCurVolumeNum; i++ )
					{
						int32_t bChanged = true;
						for( int32_t j = 0; j < iVolumeNum; j++ )
						{
							if( !strcmp(m_CurVolume[i].szDev, pVolumeInfo[j].szDev) &&
									!strcmp(m_CurVolume[i].szVolume, pVolumeInfo[j].szVolume) &&
									(m_CurVolume[i].iType == pVolumeInfo[j].iType) )
							{
								bChanged = false;
								break;
							}
						}

						if( bChanged )
						{
							int32_t iEventType = strstr( m_CurVolume[i].szDev, "mmcblk" ) ? E_NX_EVENT_SDCARD_UMOUNT : E_NX_EVENT_USB_UMOUNT;
							emit signalDetectUevent(iEventType, (uint8_t*)m_CurVolume[i].szDev);
						}
					}
				}

				memcpy( m_CurVolume, m_Volume, sizeof(NX_VOLUME_INFO) * iVolumeNum );
				m_iCurVolumeNum = iVolumeNum;
			}
		}
	}

	if( 0 < hMount ) close( hMount );
}

//------------------------------------------------------------------------------
int32_t CNX_VolumeManager::IsDeviceReserved( char *pDevice )
{
	for( int32_t i = 0; i < m_iNumDeviceReserved; i++ )
	{
		if( NULL != strstr( pDevice, m_ppDeviceReserved[i] ) )
			return true;
	}

	return false;
}

//------------------------------------------------------------------------------
int32_t CNX_VolumeManager::IsMountPosition( char *pMount )
{
	for( int32_t i = 0; i < m_iNumMountPosition; i++ )
	{
		if( NULL != strstr( pMount, m_ppMountPosition[i] ) )
			return true;
	}

	return false;
}

//------------------------------------------------------------------------------
int32_t CNX_VolumeManager::GetMountType( char *pType )
{
	if( !strcmp( pType, "msdos" ) )	return NX_VOLUME_TYPE_MSDOS;
	if( !strcmp( pType, "vfat" ) )	return NX_VOLUME_TYPE_VFAT;
	if( !strcmp( pType, "ntfs" ) )	return NX_VOLUME_TYPE_NTFS;
	if( !strcmp( pType, "ext2" ) )	return NX_VOLUME_TYPE_EXT2;
	if( !strcmp( pType, "ext3" ) )	return NX_VOLUME_TYPE_EXT3;
	if( !strcmp( pType, "ext4" ) )	return NX_VOLUME_TYPE_EXT4;

	return NX_VOLUME_TYPE_UNKNOWN;
}
