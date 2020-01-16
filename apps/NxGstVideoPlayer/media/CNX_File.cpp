//------------------------------------------------------------------------------
//
//	Copyright (C) 2018 Nexell Co. All Rights Reserved
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <libgen.h>
#include <sys/stat.h>
#include <openssl/md5.h>

#include "CNX_File.h"

#define NX_ENABLE_CHECKTIME		0

//------------------------------------------------------------------------------
CNX_File::CNX_File()
	: m_pFile( NULL )
	, m_pBasename( NULL )
	, m_pDirname( NULL )
	, m_pExtension( NULL )
	, m_pAccessDate( NULL )
	, m_pModifyDate( NULL )
	, m_pChangeDate( NULL )
	, m_pChecksum( NULL )
{

}

//------------------------------------------------------------------------------
CNX_File::CNX_File( const char *pFile )
	: m_pFile( NULL )
	, m_pBasename( NULL )
	, m_pDirname( NULL )
	, m_pExtension( NULL )
	, m_pAccessDate( NULL )
	, m_pModifyDate( NULL )
	, m_pChangeDate( NULL )
	, m_pChecksum( NULL )
{
	if( !access(pFile, F_OK) )
	{
		m_pFile = strdup( pFile );
	}
}

//------------------------------------------------------------------------------
CNX_File::~CNX_File()
{
	Close();
}

//------------------------------------------------------------------------------
int32_t CNX_File::Open( const char *pFile )
{
	if( 0 > access(pFile, F_OK) )
		return -1;

	Close();

	m_pFile = strdup( pFile );
	return 0;
}

//------------------------------------------------------------------------------
void CNX_File::Close()
{
	if( m_pFile )		free( m_pFile );
	if( m_pBasename )	free( m_pBasename );
	if( m_pDirname )	free( m_pDirname );
	if( m_pExtension )	free( m_pExtension );
	if( m_pAccessDate ) free( m_pAccessDate );
	if( m_pModifyDate ) free( m_pModifyDate );
	if( m_pChangeDate ) free( m_pChangeDate );
	if( m_pChecksum )	free( m_pChecksum );
}

//------------------------------------------------------------------------------
int32_t CNX_File::IsExist()
{
	struct stat statinfo;
	if( 0 > stat( m_pFile, &statinfo) )
		return false;

	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_File::IsRegularFile()
{
	struct stat statinfo;
	if( 0 > stat( m_pFile, &statinfo) )
		return 0;

	return S_ISREG( statinfo.st_mode );
}

//------------------------------------------------------------------------------
int32_t CNX_File::IsDirectory()
{
	struct stat statinfo;
	if( 0 > stat( m_pFile, &statinfo) )
		return 0;

	return S_ISDIR( statinfo.st_mode );
}

//------------------------------------------------------------------------------
int32_t CNX_File::GetSize()
{
	struct stat statinfo;
	if( 0 > stat( m_pFile, &statinfo) )
		return 0;

	return statinfo.st_size;
}

//------------------------------------------------------------------------------
int32_t CNX_File::GetName( char **ppResult )
{
	*ppResult = m_pFile;
	return (*ppResult != NULL) ? 0 : -1;
}

//------------------------------------------------------------------------------
int32_t CNX_File::GetDirname( char **ppResult )
{
	*ppResult = NULL;
	if( m_pDirname ) {
		free( m_pDirname );
		m_pDirname = NULL;
	}

	m_pDirname = strdup( m_pFile );
	if( m_pDirname )
		*ppResult = dirname( m_pDirname );

	return (*ppResult != NULL) ? 0 : -1;
}

//------------------------------------------------------------------------------
int32_t CNX_File::GetBasename( char **ppResult )
{
	*ppResult = NULL;
	if( m_pBasename ) {
		free( m_pBasename );
		m_pBasename = NULL;
	}

	m_pBasename = strdup( m_pFile );
	if( m_pBasename )
		*ppResult = basename( m_pBasename );

	return (*ppResult != NULL) ? 0 : -1;
}

//------------------------------------------------------------------------------
int32_t CNX_File::GetExtension( char **ppResult )
{
	*ppResult = NULL;
	if( m_pExtension ) {
		free( m_pExtension );
		m_pExtension = NULL;
	}

	char *pTemp = m_pFile + strlen(m_pFile) - 1;
	while( pTemp != m_pFile )
	{
		if( *pTemp == '.' ) break;
		pTemp--;
	}

	if( pTemp != m_pFile ) {
		m_pExtension = (char*)malloc( strlen(pTemp+1) + 1 );
		memcpy( m_pExtension, pTemp+1, strlen(pTemp+1) + 1 );
		*ppResult = m_pExtension;
	}

	return (*ppResult != NULL) ? 0 : -1;
}

//------------------------------------------------------------------------------
int32_t CNX_File::GetAccessDate( char **ppResult )
{
	*ppResult = NULL;
	if( m_pAccessDate ) {
		free( m_pAccessDate );
		m_pAccessDate = NULL;
	}

	struct stat statinfo;
	if( 0 > stat(m_pFile, &statinfo) )
		return -1;

	m_pAccessDate = (char*)malloc( MAX_DATE_NUM );
	memset( m_pAccessDate, 0x00, MAX_DATE_NUM );

	struct tm *tminfo = localtime( (time_t*)&statinfo.st_atime );
	sprintf( m_pAccessDate, "%04d-%02d-%02d %02d:%02d:%02d",
		tminfo->tm_year + 1900, tminfo->tm_mon + 1, tminfo->tm_mday,
		tminfo->tm_hour, tminfo->tm_min, tminfo->tm_sec );

	*ppResult = m_pAccessDate;
	return statinfo.st_atime;
}

//------------------------------------------------------------------------------
int32_t CNX_File::GetModifyDate( char **ppResult )
{
	*ppResult = NULL;
	if( m_pModifyDate ) {
		free( m_pModifyDate );
		m_pModifyDate = NULL;
	}

	struct stat statinfo;
	if( 0 > stat(m_pFile, &statinfo) )
		return -1;

	m_pModifyDate = (char*)malloc( MAX_DATE_NUM );
	memset( m_pModifyDate, 0x00, MAX_DATE_NUM );

	struct tm *tminfo = localtime( (time_t*)&statinfo.st_mtime );
	sprintf( m_pModifyDate, "%04d-%02d-%02d %02d:%02d:%02d",
		tminfo->tm_year + 1900, tminfo->tm_mon + 1, tminfo->tm_mday,
		tminfo->tm_hour, tminfo->tm_min, tminfo->tm_sec );

	*ppResult = m_pModifyDate;
	return statinfo.st_mtime;
}

//------------------------------------------------------------------------------
int32_t CNX_File::GetChangeDate( char **ppResult )
{
	*ppResult = NULL;
	if( m_pChangeDate ) {
		free( m_pChangeDate );
		m_pChangeDate = NULL;
	}

	struct stat statinfo;
	if( 0 > stat(m_pFile, &statinfo) )
		return -1;

	m_pChangeDate = (char*)malloc( MAX_DATE_NUM );
	memset( m_pChangeDate, 0x00, MAX_DATE_NUM );

	struct tm *tminfo = localtime( (time_t*)&statinfo.st_ctime );
	sprintf( m_pChangeDate, "%04d-%02d-%02d %02d:%02d:%02d",
		tminfo->tm_year + 1900, tminfo->tm_mon + 1, tminfo->tm_mday,
		tminfo->tm_hour, tminfo->tm_min, tminfo->tm_sec );

	*ppResult = m_pChangeDate;
	return statinfo.st_ctime;
}

//------------------------------------------------------------------------------
#if NX_ENABLE_CHECKTIME
#include <sys/time.h>
static int64_t iCurTime = 0;
static int64_t NX_GetSystemTick( void )
{
	struct timeval	tv;
	struct timezone	zv;

	gettimeofday( &tv, &zv );

	return ((int64_t)tv.tv_sec) * 1000 + (int64_t)(tv.tv_usec / 1000);
}

#define NX_START_CHECKTIME()	do { iCurTime = NX_GetSystemTick(); } while(0)
#define NX_END_CHECKTIME()		NX_GetSystemTick() - iCurTime
#else
#define NX_START_CHECKTIME()
#define NX_END_CHECKTIME()
#endif

int32_t CNX_File::GetChecksum( char **ppResult )
{
	*ppResult = NULL;
	if( m_pChecksum ) {
		free( m_pChecksum );
		m_pChecksum = NULL;
	}

	struct stat statinfo;
	if( 0 > stat( m_pFile, &statinfo) )
		return -1;

	FILE *hFile = fopen( m_pFile, "r" );
	if( NULL == hFile )
		return -1;

	if( hFile )
	{
		size_t iSize = statinfo.st_size;
		char *pData = (char*)malloc( iSize );

		size_t iReadSize = fread( pData, 1, iSize, hFile );
		fclose( hFile );

		if( iSize != iReadSize ) {
			free( pData );
			return -1;
		}

		uint8_t digest[MD5_DIGEST_LENGTH];
		MD5_CTX ctx;
		MD5_Init( &ctx );
		MD5_Update( &ctx, pData, iSize );
		MD5_Final( digest, &ctx );

		m_pChecksum = (char*)malloc( MD5_DIGEST_LENGTH * 2 + 1 );
		memset( m_pChecksum, 0x00, MD5_DIGEST_LENGTH * 2 + 1 );

		for( int32_t i = 0; i < MD5_DIGEST_LENGTH; i++ )
		{
			sprintf( m_pChecksum + (i*2), "%02X", digest[i] );
		}

		*ppResult = m_pChecksum;
		free( pData );
	}
	return 0;
}
