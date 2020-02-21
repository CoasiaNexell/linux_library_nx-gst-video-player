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

#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "CNX_MediaScanner.h"
#include "CNX_File.h"

#define LOG_TAG	"[CNX_MediaScanner]"
#include <NX_Log.h>

#define NX_ENABLE_CHANGEDATE	1
#define NX_ENABLE_CHECKSUM		0

//------------------------------------------------------------------------------
#if 1
#ifdef CONFIG_NXP4330
const static char *pstVideoDefaultExtension[] = {
	"mp4",	"avi",	"mkv",	"divx",	"rmvb",	"flv",	"ts",	"m2ts",	"ps", "vob"
};
#elif CONFIG_NXP3220
const static char *pstVideoDefaultExtension[] = {
	"mp4",	"avi",	"mkv",	"divx",	"flv", "ts", "m2ts",	"ps",
};
#endif

const static char *pstAudioDefaultExtension[] = {
	"mp3",	"ogg",	"flac",	"wma",	"aac",	"wav",	"mp4a",
};
#else
const static char *pstVideoDefaultExtension[] = {
	"avi",	"wmv",	"asf",	"mpg",	"mpeg",	"mpv2",	"mp2v",	"ts",	"tp",	"vob",
	"ogm",	"ogv",	"mp4",	"m4v",	"m4p",	"3gp",	"3gpp",	"mkv",	"rm",	"rmvb",
	"flv",	"m2ts",	"m2t",	"divx",	"webm",
};

const static char *pstAudioDefaultExtension[] = {
	"wav",	"wma",	"mpa",	"mp2",	"m1a",	"m2a",	"mp3",	"ogg",	"m4a",	"aac",
	"mka",	"ra",	"flac",	"ape",	"mpc",	"mod",	"ac3",	"eac3",	"dts",	"dtshd",
	"wv",	"tak",
};
#endif

//------------------------------------------------------------------------------
CNX_MediaScanner::CNX_MediaScanner()
	: m_ppVideoExtension( NULL )
	, m_ppAudioExtension( NULL )
	, m_iDirectoryNum( 0 )
	, m_iVideoExtensionNum( 0 )
	, m_iAudioExtensionNum( 0 )
	, m_bThreadRun( false )
	, m_pSqlite( NULL )
	, m_pDatabase( NULL )
{
	for( int32_t i = 0; i < MAX_DIRECTORY_NUM; i++ )
	{
		m_pDirectory[i] = NULL;
	}

	m_ppVideoExtension = pstVideoDefaultExtension;
	m_iVideoExtensionNum = sizeof(pstVideoDefaultExtension) / sizeof(pstVideoDefaultExtension[0]);

	m_ppAudioExtension = pstAudioDefaultExtension;
	m_iAudioExtensionNum = sizeof(pstAudioDefaultExtension) / sizeof(pstAudioDefaultExtension[0]);

	m_pDatabase = new CNX_MediaDatabase();
}

//------------------------------------------------------------------------------
CNX_MediaScanner::~CNX_MediaScanner()
{
	Cancel();

	if( m_pDatabase )
	{
		delete m_pDatabase;
		m_pDatabase = NULL;
	}

	for( int32_t i = 0; i < MAX_DIRECTORY_NUM; i++ )
	{
		if( m_pDirectory[i] )
		{
			free( m_pDirectory[i] );
			m_pDirectory[i] = NULL;
		}
	}
}

//------------------------------------------------------------------------------
void CNX_MediaScanner::SetVideoExtension( const char **ppExtension, int32_t iExtensionNum )
{
	m_ppVideoExtension   = ppExtension;
	m_iVideoExtensionNum = iExtensionNum;
}

//------------------------------------------------------------------------------
void CNX_MediaScanner::SetAudioExtension( const char **ppExtension, int32_t iExtensionNum )
{
	m_ppAudioExtension   = ppExtension;
	m_iAudioExtensionNum = iExtensionNum;
}

//------------------------------------------------------------------------------
void CNX_MediaScanner::Scan( char *pDirectory[], int32_t iDirectoryNum)
{
	CNX_BaseAutoLock lock( &m_hLock );
	if (m_bThreadRun)
	{
		m_bThreadRun = false;

		if (isRunning())
		{
			wait();
			quit();
		}
	}

	if( MAX_DIRECTORY_NUM < iDirectoryNum )
	{
		return;
	}

	for( int32_t i = 0; i < MAX_DIRECTORY_NUM; i++ )
	{
		if( m_pDirectory[i] )
		{
			free( m_pDirectory[i] );
			m_pDirectory[i] = NULL;
		}
	}

	for( int32_t i = 0; i < iDirectoryNum; i++ )
	{
		m_pDirectory[i] = realpath( pDirectory[i], NULL );
	}

	m_iDirectoryNum    = iDirectoryNum;

	start();
}

//------------------------------------------------------------------------------
void CNX_MediaScanner::Cancel( void )
{
	CNX_BaseAutoLock lock( &m_hLock );

	if( true == m_bThreadRun )
	{
		m_bThreadRun = false;

		if (isRunning())
		{
			wait();
			quit();
		}
	}

	for( int32_t i = 0; i < MAX_DIRECTORY_NUM; i++ )
	{
		if( m_pDirectory[i] )
		{
			free( m_pDirectory[i] );
			m_pDirectory[i] = NULL;
		}
	}
}

//------------------------------------------------------------------------------
static int32_t cbCompare( void *pObj, int32_t iColumnNum, char **ppColumnValue, char **ppColumnName )
{
	CNX_MediaDatabase *pDatabase = (CNX_MediaDatabase*)pObj;
	NX_MEDIA_DATABASE data;
	memset( &data, 0x00, sizeof(NX_MEDIA_DATABASE) );

	for(int32_t i = 0; i < iColumnNum; i++)
	{
		if( !strcmp(ppColumnName[i], "_path") ) 		data.pFile = ppColumnValue[i];
		if( !strcmp(ppColumnName[i], "_changedate"))	data.iChangeDate = atoi(ppColumnValue[i]);
		if( !strcmp(ppColumnName[i], "_checksum") ) 	data.pChecksum = ppColumnValue[i];
	}

	CNX_File file( data.pFile );
	if( !file.IsExist() )
	{
		printf("Remove Data. ( Not Exist ) ( %s )\n", data.pFile );

		data.iChangeDate = 0;
		data.pChecksum   = NULL;
		pDatabase->Delete( &data );
	}
	else
	{
		int32_t bDelete = false;
#if NX_ENABLE_CHANGEDATE
		char *pChangeDate = NULL;
		int32_t iChangeDate = file.GetChangeDate( &pChangeDate );

		if( iChangeDate != data.iChangeDate )
			bDelete = true;
#endif
#if NX_ENABLE_CHECKSUM
		char *pChecksum = NULL;
		file.GetChecksum( &pChecksum );

		if( strcmp( pChecksum, data.pChecksum ) )
			bDelete = true;
#endif
		if( bDelete )
		{
			printf("Remove Data. ( Changed File ) ( %s )\n", data.pFile );
			data.iChangeDate = 0;
			data.pChecksum   = NULL;
			pDatabase->Delete( &data );
		}
	}

	return 0;
}

//------------------------------------------------------------------------------

void CNX_MediaScanner::run()
{
	char szPath[MAX_PATH_SIZE];
	m_bThreadRun = true;
	MakeDirectory(NX_MEDIA_DATABASE_PATH);
	sprintf(szPath, "%s/%s", NX_MEDIA_DATABASE_PATH, NX_MEDIA_DATABASE_NAME);

	m_pDatabase->Open(szPath, NX_MEDIA_DATABASE_TABLE);

	if (!m_pDatabase->IsValidTable())
	{
		m_pDatabase->Create();
	}

	if (!m_pDatabase->IsValidColumn())
	{
		m_pDatabase->Drop();
		m_pDatabase->Create();
	}

	m_pDatabase->Select(NULL, cbCompare, m_pDatabase);

	for (int32_t i = 0; i < m_iDirectoryNum; i++)
	{
		MakeFileList(m_pDirectory[i]);
	}

	emit signalScanDone();

	m_pDatabase->Close();
}

//------------------------------------------------------------------------------
int32_t CNX_MediaScanner::MakeFileList( const char *pDirectory )
{
	DIR *pDir = NULL;
	struct dirent *pDirent;

	{
		char szSkip[MAX_PATH_SIZE];
		sprintf( szSkip, "%s/.nomedia", pDirectory );

		CNX_File skip(szSkip);
		if( skip.IsExist() )
		{
			printf("Skip. Directory Search. ( %s )\n", pDirectory);
			RemoveSkipFileList( pDirectory );
			return 0;
		}
	}

	if( NULL == (pDir = opendir( pDirectory )) )
	{
		return -1;
	}

	while( NULL != (pDirent = readdir(pDir)) )
	{
		if( !strcmp( pDirent->d_name, "." ) ||
			!strcmp( pDirent->d_name, ".." ) )
			continue;

		if( false == m_bThreadRun )
		{
			closedir( pDir );
			return -1;
		}

		char szFile[MAX_PATH_SIZE];
		sprintf( szFile, "%s/%s", pDirectory, pDirent->d_name );

		CNX_File file(szFile);
		if( file.IsDirectory() )
		{
			char szSkip[MAX_PATH_SIZE];
			sprintf( szSkip, "%s/.nomedia", szFile );

			CNX_File skip(szSkip);
			if( skip.IsExist() )
			{
				printf("Skip. Directory Search. ( %s )\n", szFile);
				RemoveSkipFileList( szFile );
				continue;
			}

			MakeFileList(szFile);
		}
		else if( file.IsRegularFile() )
		{
			// if( IsVideo(szFile) || IsAudio(szFile) )
			// {
			// 	printf("Insert [ %s ] : %s\n", IsVideo(szFile) ? "VIDEO" : "AUDIO", szFile );
			// }

			if( IsVideo(szFile) || IsAudio(szFile) )
			{
				char *pChangeDate   = NULL;
				char *pChecksum     = NULL;
				int32_t iChangeDate = 0;

#if NX_ENABLE_CHANGEDATE
				iChangeDate = file.GetChangeDate( &pChangeDate );
#endif
#if NX_ENABLE_CHECKSUM
				file.GetChecksum( &pChecksum );
#endif
				NX_MEDIA_DATABASE data;
				memset( &data, 0x00, sizeof(NX_MEDIA_DATABASE) );
				data.pFile       = szFile;
				data.iType       = IsVideo(szFile) ? NX_MEDIA_TYPE_VIDEO : NX_MEDIA_TYPE_AUDIO;
				data.iChangeDate = iChangeDate;
				data.pChecksum   = pChecksum;

				if( !m_pDatabase->IsValidData( &data ) )
				{
					printf("Insert Data. ( %s )\n", data.pFile );
					m_pDatabase->Insert( &data );
				}
			}
		}
	}

	closedir( pDir );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_MediaScanner::RemoveSkipFileList( const char *pDirectory )
{
	DIR *pDir;
	struct dirent *pDirent;

	if( NULL == (pDir = opendir( pDirectory )) )
	{
		return -1;
	}

	while( NULL != (pDirent = readdir(pDir)) )
	{
		if( !strcmp( pDirent->d_name, "." ) ||
			!strcmp( pDirent->d_name, ".." ) )
			continue;

		if( false == m_bThreadRun )
		{
			closedir( pDir );
			return -1;
		}

		char szFile[MAX_PATH_SIZE];
		sprintf( szFile, "%s/%s", pDirectory, pDirent->d_name );

		CNX_File file(szFile);
		if( file.IsDirectory() )
		{
			RemoveSkipFileList( szFile );
		}
		else if( file.IsRegularFile() )
		{
			if( IsVideo(szFile) || IsAudio(szFile) )
			{
				NX_MEDIA_DATABASE data;
				memset( &data, 0x00, sizeof(NX_MEDIA_DATABASE) );
				data.pFile = szFile;

				if( m_pDatabase->IsValidData( &data ) )
				{
					printf("Delete Skip Data. ( %s )\n", data.pFile );
					m_pDatabase->Delete( &data );
				}
			}
		}
	}

	closedir( pDir );
	return 0;
}

//------------------------------------------------------------------------------
void CNX_MediaScanner::MakeDirectory( const char *pDir )
{
	char buf[1024];
	char *pBuf = buf;

	memcpy( buf, pDir, sizeof(buf) -1 );
	buf[sizeof(buf)-1] = 0x00;

	while( *pBuf )
	{
		if( '/' == *pBuf )
		{
			*pBuf = 0x00;
			if( 0 != access( buf, F_OK ) && (pBuf != buf) )
			{
				mkdir( buf, 0777 );
			}
			*pBuf = '/';
		}
		pBuf++;
	}

	if( 0 != access( buf, F_OK) )
	{
		mkdir( buf, 0777 );
	}
}

//------------------------------------------------------------------------------
int32_t CNX_MediaScanner::IsVideo( char *pFile )
{
	for( int32_t i = 0; i < m_iVideoExtensionNum; i++ )
	{
		CNX_File file( pFile );
		char *pExtension = NULL;

		if( 0 > file.GetExtension( &pExtension) )
			return false;

		if( !strcmp( pExtension, m_ppVideoExtension[i] ) )
			return true;
	}

	return false;
}

//------------------------------------------------------------------------------
int32_t CNX_MediaScanner::IsAudio( char *pFile )
{
	for( int32_t i = 0; i < m_iAudioExtensionNum; i++ )
	{
		CNX_File file( pFile );
		char *pExtension = NULL;

		if( 0 > file.GetExtension( &pExtension) )
			return false;

		if( !strcmp( pExtension, m_ppAudioExtension[i] ) )
			return true;
	}

	return false;
}
