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

#ifndef __CNX_MEDIASCANMANAGER_H__
#define __CNX_MEDIASCANMANAGER_H__

#include <QThread>
#include <stdint.h>
#include <sqlite3.h>

#include <NX_Type.h>
#include "CNX_MediaDatabase.h"
#include "CNX_Base.h"

//------------------------------------------------------------------------------
class CNX_MediaScanner : public QThread
{
	Q_OBJECT

signals:
	void signalScanDone();

public:
	CNX_MediaScanner();
	~CNX_MediaScanner();

public:
	void	SetVideoExtension( const char **ppExtension, int32_t iExtensionNum );
	void	SetAudioExtension( const char **ppExtension, int32_t iExtensionNum );

	void Scan( char *pDirectory[], int32_t iDirectoryNum);
	void	Cancel( void );

protected:
	virtual void run();

private:
	int32_t	MakeFileList( const char *pDirectory );
	int32_t RemoveSkipFileList( const char *pDirectory );

	void	MakeDirectory( const char *pDir );
	int32_t IsVideo( char *pFile );
	int32_t IsAudio( char *pFile );

private:
	enum { MAX_PATH_SIZE = 1024 };
	enum { MAX_DIRECTORY_NUM = 16 };

	CNX_Mutex		m_hLock;

	char*			m_pDirectory[MAX_DIRECTORY_NUM];
	const char**	m_ppVideoExtension;
	const char**	m_ppAudioExtension;

	int32_t			m_iDirectoryNum;
	int32_t			m_iVideoExtensionNum;
	int32_t			m_iAudioExtensionNum;

	int32_t			m_bThreadRun;

private:
	sqlite3*		m_pSqlite;
	CNX_MediaDatabase* m_pDatabase;

private:
	CNX_MediaScanner (const CNX_MediaScanner &Ref);
	CNX_MediaScanner &operator=(const CNX_MediaScanner &Ref);
};

#endif	// __CNX_MEDIASCANMANAGER_H__
