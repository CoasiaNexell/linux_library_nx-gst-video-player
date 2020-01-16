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

#ifndef __CNX_MEDIADATABASE_H__
#define __CNX_MEDIADATABASE_H__

#include <stdio.h>
#include <stdint.h>
#include <sqlite3.h>

typedef enum {
	NX_MEDIA_TYPE_NONE	= 0,
	NX_MEDIA_TYPE_VIDEO = 1,
	NX_MEDIA_TYPE_AUDIO = 2,
} NX_MEDIA_TYPE;

typedef struct NX_MEDIA_DATABASE {
	char*		pFile;
	int32_t		iType;
	int32_t		iChangeDate;
	char*		pChecksum;
} NX_MEDIA_DATABASE;

class CNX_MediaDatabase
{
public:
	CNX_MediaDatabase();
	~CNX_MediaDatabase();

public:
	int32_t Open( const char *pDatabase );
	int32_t Open( const char *pDatabase, const char *pTable );
	int32_t SetTable( const char *pTable );
	void	Close();

	int32_t	Create( int32_t bCheck = true );
	int32_t Drop();
	int32_t Rename( const char *pTable );
	int32_t IsValidTable();
	int32_t IsValidColumn();

	int32_t Insert( NX_MEDIA_DATABASE *pData );
	int32_t Delete( NX_MEDIA_DATABASE *pData );
	int32_t DeleteAll();

	// int32_t cbFunc( void *pObj, int32_t iColumnNum, char **ppColumnValue, char **ppColumnName )
	int32_t Select( NX_MEDIA_DATABASE *pData, int32_t (*cbFunc)(void*, int32_t, char**, char**), void *pObj );
	int32_t IsValidData( NX_MEDIA_DATABASE *pData );

private:
	enum{ MAX_QUERY_SIZE = 1024 };
	sqlite3*		m_pSqlite;
	char*			m_pTable;

private:
	CNX_MediaDatabase (const CNX_MediaDatabase &Ref);
	CNX_MediaDatabase &operator=(const CNX_MediaDatabase &Ref);
};

#endif	// __CNX_MEDIADATABASE_H__
