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

#include <stdlib.h>
#include <string.h>

#include <NX_Type.h>
#include "CNX_MediaDatabase.h"

#define LOG_TAG	"[CNX_MediaDatabase]"
#include <NX_Log.h>

#define NX_ENABLE_QUERY		0

//------------------------------------------------------------------------------
CNX_MediaDatabase::CNX_MediaDatabase()
	: m_pSqlite( NULL )
	, m_pTable( NULL )
{

}

//------------------------------------------------------------------------------
CNX_MediaDatabase::~CNX_MediaDatabase()
{

}

//------------------------------------------------------------------------------
int32_t CNX_MediaDatabase::Open( const char *pDatabase )
{
	int32_t iRet = 0;
	if( m_pSqlite )
	{
		sqlite3_close( m_pSqlite );
		m_pSqlite = NULL;
	}

	iRet = sqlite3_open( pDatabase, &m_pSqlite );
	if( SQLITE_OK != iRet )
	{
		NXLOGE("Fail, sqlite3_open(). ( %d )\n", iRet );
		return -1;
	}
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_MediaDatabase::Open( const char *pDatabase, const char *pTable )
{
	Close();

	if( 0 > Open( pDatabase ) )
		return -1;

	SetTable( pTable );
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_MediaDatabase::SetTable( const char *pTable )
{
	if( m_pTable )
	{
		free( m_pTable );
		m_pTable = NULL;
	}

	m_pTable = strdup( pTable );
	return 0;
}

//------------------------------------------------------------------------------
void CNX_MediaDatabase::Close()
{
	if( m_pTable )
	{
		free( m_pTable );
		m_pTable = NULL;
	}

	if( m_pSqlite )
	{
		sqlite3_close( m_pSqlite );
		m_pSqlite = NULL;
	}
}

//------------------------------------------------------------------------------
const static char *pstColumn[][3] = {
//	Name			Type			Property
	"_id",			"INTEGER",		"PRIMARY KEY AUTOINCREMENT",
	"_path",		"TEXT",			"NOT NULL",
	"_type",		"TEXT",			"NOT NULL",
	"_changedate",	"TEXT",			"",
	"_checksum",	"TEXT",			"",
	"_date",		"DATETIME",		"DEFAULT CURRENT_TIMESTAMP",
};

//------------------------------------------------------------------------------
int32_t CNX_MediaDatabase::Create( int32_t bCheck )
{
	int32_t iRet = 0;
	char *pErrMsg;
	char query[MAX_QUERY_SIZE] = { 0x00, };

	if( m_pSqlite == NULL )
	{
		NXLOGE("Fail, Invalid Handle.\n" );
		return -1;
	}

	sprintf( query,
		"CREATE TABLE %s%s ( %s, %s, %s, %s, %s, %s );",
		bCheck ? "IF NOT EXISTS " : "",
		m_pTable,
		"_id INTEGER PRIMARY KEY AUTOINCREMENT",
		"_path TEXT NOT NULL",
		"_type TEXT NOT NULL",
		"_changedate TEXT",
		"_checksum TEXT",
		"_date DATETIME DEFAULT CURRENT_TIMESTAMP"
	);

#if NX_ENABLE_QUERY
	printf(">>> %s(): %s\n", __FUNCTION__, query);
#endif

	iRet = sqlite3_exec( m_pSqlite, query, NULL, 0, &pErrMsg );
	if( SQLITE_OK != iRet )
	{
		NXLOGE("Fail, sqlite3_exec(). ( %d, %s )\n", iRet, pErrMsg );
		return -1;
	}

	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_MediaDatabase::Drop()
{
	int32_t iRet = 0;
	char *pErrMsg;
	char query[MAX_QUERY_SIZE] = { 0x00, };

	if( m_pSqlite == NULL )
	{
		NXLOGE("Fail, Invalid Handle.\n" );
		return -1;
	}

	sprintf( query,
		"DROP TABLE %s;",
		m_pTable
	);

#if NX_ENABLE_QUERY
	printf(">>> %s(): %s\n", __FUNCTION__, query);
#endif

	iRet = sqlite3_exec( m_pSqlite, query, NULL, 0, &pErrMsg );
	if( SQLITE_OK != iRet )
	{
		NXLOGE("Fail, sqlite3_exec(). ( %d, %s )\n", iRet, pErrMsg );
		return -1;
	}

	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_MediaDatabase::Rename( const char *pTable )
{
	int32_t iRet = 0;
	char *pErrMsg;
	char query[MAX_QUERY_SIZE] = { 0x00, };

	if( m_pSqlite == NULL )
	{
		NXLOGE("Fail, Invalid Handle.\n" );
		return -1;
	}

	sprintf( query,
		"ALTER TABLE %s RENAME TO %s;",
		m_pTable,
		pTable
	);

#if NX_ENABLE_QUERY
	printf(">>> %s(): %s\n", __FUNCTION__, query);
#endif

	iRet = sqlite3_exec( m_pSqlite, query, NULL, 0, &pErrMsg );
	if( SQLITE_OK != iRet )
	{
		NXLOGE("Fail, sqlite3_exec(). ( %d, %s )\n", iRet, pErrMsg );
		return -1;
	}

	SetTable( pTable );
	return 0;
}

//------------------------------------------------------------------------------
static int32_t cbQuerySuccess( void *pObj, int32_t iColumnNum, char **ppColumnValue, char **ppColumnName )
{
	int32_t *pResult = (int32_t*)pObj;
	*pResult = true;
	(void)iColumnNum;
	(void)ppColumnValue;
	(void)ppColumnName;
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_MediaDatabase::IsValidTable()
{
	int32_t iRet = 0;
	char *pErrMsg;
	char query[MAX_QUERY_SIZE] = { 0x00, };
	int32_t bResult = false;

	if( m_pSqlite == NULL )
	{
		NXLOGE("Fail, Invalid Handle.\n" );
		return false;
	}

	sprintf( query,
		"SELECT name FROM sqlite_master WHERE type='table' AND name ='%s';",
		m_pTable
	);

#if NX_ENABLE_QUERY
	printf(">>> %s(): %s\n", __FUNCTION__, query);
#endif

	iRet = sqlite3_exec( m_pSqlite, query, cbQuerySuccess, &bResult, &pErrMsg );
	if( SQLITE_OK != iRet )
	{
		NXLOGE("Fail, sqlite3_exec(). ( %d, %s )\n", iRet, pErrMsg );
		return false;
	}

	return bResult;
}

//------------------------------------------------------------------------------
static int32_t cbCompareColumn( void *pObj, int32_t iColumnNum, char **ppColumnValue, char **ppColumnName )
{
	int32_t iColumnIndex = -1;
	int32_t bResult = false;

	(void)pObj;

	for( int32_t i = 0; i < iColumnNum; i++ )
	{
		if( !strcmp( ppColumnName[i], "name" ) )
		{
			iColumnIndex = i;
			break;
		}
	}

	if( -1 > iColumnIndex )
	{
		printf("Fail, Invalid Name Index.\n");
		return -1;
	}

	for( int32_t i = 0; i < (int32_t)(sizeof(pstColumn) / sizeof(pstColumn[0])); i++ )
	{
		bResult = false;
		if( !strcmp( pstColumn[i][0], ppColumnValue[iColumnIndex]) )
		{
			bResult = true;
			break;
		}
	}

	if( bResult == false )
	{
		printf("Fail, Invalid Column. ( Column : %s )\n", ppColumnValue[iColumnIndex]);
	}

	return (bResult == true) ? 0 : -1;
}

//------------------------------------------------------------------------------
int32_t CNX_MediaDatabase::IsValidColumn()
{
	int32_t iRet = 0;
	char *pErrMsg;
	char query[MAX_QUERY_SIZE] = { 0x00, };

	if( m_pSqlite == NULL )
	{
		NXLOGE("Fail, Invalid Handle.\n" );
		return false;
	}

	{
		sprintf( query,
			"PRAGMA table_info(%s);",
			m_pTable
		);

#if NX_ENABLE_QUERY
		printf(">>> %s(): %s\n", __FUNCTION__, query);
#endif

		iRet = sqlite3_exec( m_pSqlite, query, cbCompareColumn, NULL, &pErrMsg );
		if( SQLITE_OK != iRet )
		{
			NXLOGE("Fail, sqlite3_exec(). ( %d, %s )\n", iRet, pErrMsg );
			return false;
		}
	}

	{
		for( int32_t i = 0; i < (int32_t)(sizeof(pstColumn) / sizeof(pstColumn[0])); i++ )
		{
			sprintf( query,
				"SELECT %s FROM %s",
				pstColumn[i][0],
				m_pTable
			);

#if NX_ENABLE_QUERY
			printf(">>> %s(): %s\n", __FUNCTION__, query);
#endif

			iRet = sqlite3_exec( m_pSqlite, query, NULL, 0, &pErrMsg );
			if( SQLITE_OK != iRet )
			{
				printf("Fail, Invalid Column. ( Column : %s )\n", pstColumn[i][0]);
				NXLOGE("Fail, sqlite3_exec(). ( %d, %s )\n", iRet, pErrMsg );
				return false;
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------------
int32_t CNX_MediaDatabase::Insert( NX_MEDIA_DATABASE *pData )
{
	int32_t iRet = 0;
	char *pErrMsg;
	char query[MAX_QUERY_SIZE] = { 0x00, };

	if( m_pSqlite == NULL )
	{
		NXLOGE("Fail, Invalid Handle.\n" );
		return -1;
	}

	sprintf( query,
		"INSERT INTO %s (%s, %s, %s, %s) VALUES(\"%s\", \"%s\", \"%d\", \"%s\");",
		m_pTable,
		"_path", "_type", "_changedate", "_checksum",
		pData->pFile,
		(pData->iType == NX_MEDIA_TYPE_VIDEO) ? "video" : "audio",
		pData->iChangeDate ? pData->iChangeDate : 0,
		pData->pChecksum ? pData->pChecksum : ""
	);

#if NX_ENABLE_QUERY
	printf(">>> %s(): %s\n", __FUNCTION__, query);
#endif

	iRet = sqlite3_exec( m_pSqlite, query, NULL, 0, &pErrMsg );
	if( SQLITE_OK != iRet )
	{
		NXLOGE("Fail, sqlite3_exec(). ( %d, %s )\n", iRet, pErrMsg );
		return -1;
	}
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_MediaDatabase::Delete( NX_MEDIA_DATABASE *pData )
{
	int32_t iRet = 0;
	char *pErrMsg;
	char query[MAX_QUERY_SIZE] = { 0x00, };

	if( m_pSqlite == NULL )
	{
		NXLOGE("Fail, Invalid Handle.\n" );
		return -1;
	}

	int32_t iWrite = 0, iWritten = 0;
	char *pQuery = query;

	iWrite = sprintf( pQuery, "DELETE FROM %s", m_pTable );
	pQuery += iWrite;

	if( pData != NULL && pData->pFile != NULL )
	{
		iWrite = sprintf( pQuery,
			"%s_path=\"%s\"",
			(iWritten != 0) ? " AND " : " WHERE ",
			pData->pFile
		);
		pQuery   += iWrite;
		iWritten += iWrite;
	}

	if( pData != NULL && pData->iType != NX_MEDIA_TYPE_NONE )
	{
		iWrite = sprintf( pQuery,
			"%s_type=\"%s\"",
			(iWritten != 0) ? " AND " : " WHERE ",
			(pData->iType == NX_MEDIA_TYPE_VIDEO) ? "video" : "audio"
		);
		pQuery   += iWrite;
		iWritten += iWrite;
	}

	if( pData != NULL && pData->iChangeDate != 0 )
	{
		iWrite = sprintf( pQuery,
			"%s_changedate=\"%d\"",
			(iWritten != 0) ? " AND " : " WHERE ",
			pData->iChangeDate
		);
		pQuery   += iWrite;
		iWritten += iWrite;
	}

	if( pData != NULL && pData->pChecksum != NULL )
	{
		iWrite = sprintf( pQuery,
			"%s_checksum=\"%s\"",
			(iWritten != 0) ? " AND " : " WHERE ",
			pData->pChecksum
		);
		pQuery   += iWrite;
		iWritten += iWrite;
	}

#if NX_ENABLE_QUERY
	printf(">>> %s(): %s\n", __FUNCTION__, query);
#endif

	iRet = sqlite3_exec( m_pSqlite, query, NULL, 0, &pErrMsg );
	if( SQLITE_OK != iRet )
	{
		NXLOGE("Fail, sqlite3_exec(). ( %d, %s )\n", iRet, pErrMsg );
		return -1;
	}
	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_MediaDatabase::DeleteAll()
{
	return Delete( NULL );
}

//------------------------------------------------------------------------------
int32_t CNX_MediaDatabase::Select( NX_MEDIA_DATABASE *pData, int32_t (*cbFunc)(void*, int32_t, char**, char**), void *pObj )
{
	int32_t iRet = 0;
	char *pErrMsg;
	char query[MAX_QUERY_SIZE] = { 0x00, };

	if( m_pSqlite == NULL )
	{
		NXLOGE("Fail, Invalid Handle.\n" );
		return -1;
	}

	int32_t iWrite = 0, iWritten = 0;
	char *pQuery = query;

	iWrite = sprintf( pQuery, "SELECT * FROM %s", m_pTable );
	pQuery += iWrite;

	if( pData != NULL && pData->pFile != NULL )
	{
		iWrite = sprintf( pQuery,
			"%s_path=\"%s\"",
			(iWritten != 0) ? " AND " : " WHERE ",
			pData->pFile
		);
		pQuery   += iWrite;
		iWritten += iWrite;
	}

	if( pData != NULL && pData->iType != NX_MEDIA_TYPE_NONE )
	{
		iWrite = sprintf( pQuery,
			"%s_type=\"%s\"",
			(iWritten != 0) ? " AND " : " WHERE ",
			(pData->iType == NX_MEDIA_TYPE_VIDEO) ? "video" : "audio"
		);
		pQuery   += iWrite;
		iWritten += iWrite;
	}

	if( pData != NULL && pData->iChangeDate != 0 )
	{
		iWrite = sprintf( pQuery,
			"%s_changedate=\"%d\"",
			(iWritten != 0) ? " AND " : " WHERE ",
			pData->iChangeDate
		);
		pQuery   += iWrite;
		iWritten += iWrite;
	}

	if( pData != NULL && pData->pChecksum != NULL )
	{
		iWrite = sprintf( pQuery,
			"%s_checksum=\"%s\"",
			(iWritten != 0) ? " AND " : " WHERE ",
			pData->pChecksum
		);
		pQuery   += iWrite;
		iWritten += iWrite;
	}

#if NX_ENABLE_QUERY
	printf(">>> %s(): %s\n", __FUNCTION__, query);
#endif

	iRet = sqlite3_exec( m_pSqlite, query, cbFunc, pObj, &pErrMsg );
	if( SQLITE_OK != iRet )
	{
		NXLOGE("Fail, sqlite3_exec(). ( %d, %s )\n", iRet, pErrMsg );
		return -1;
	}

	return 0;
}

//------------------------------------------------------------------------------
int32_t CNX_MediaDatabase::IsValidData( NX_MEDIA_DATABASE *pData )
{
	int32_t iRet = 0;
	char *pErrMsg;
	char query[MAX_QUERY_SIZE] = { 0x00, };
	int32_t bResult = false;

	if( m_pSqlite == NULL )
	{
		NXLOGE("Fail, Invalid Handle.\n" );
		return false;
	}

	int32_t iWrite = 0, iWritten = 0;
	char *pQuery = query;

	iWrite = sprintf( pQuery, "SELECT * FROM %s", m_pTable );
	pQuery += iWrite;

	if( pData != NULL && pData->pFile != NULL )
	{
		iWrite = sprintf( pQuery,
			"%s_path=\"%s\"",
			(iWritten != 0) ? " AND " : " WHERE ",
			pData->pFile
		);
		pQuery   += iWrite;
		iWritten += iWrite;
	}

	if( pData != NULL && pData->iType != NX_MEDIA_TYPE_NONE )
	{
		iWrite = sprintf( pQuery,
			"%s_type=\"%s\"",
			(iWritten != 0) ? " AND " : " WHERE ",
			(pData->iType == NX_MEDIA_TYPE_VIDEO) ? "video" : "audio"
		);
		pQuery   += iWrite;
		iWritten += iWrite;
	}

	if( pData != NULL && pData->iChangeDate != 0 )
	{
		iWrite = sprintf( pQuery,
			"%s_changedate=\"%d\"",
			(iWritten != 0) ? " AND " : " WHERE ",
			pData->iChangeDate
		);
		pQuery   += iWrite;
		iWritten += iWrite;
	}

	if( pData != NULL && pData->pChecksum != NULL )
	{
		iWrite = sprintf( pQuery,
			"%s_checksum=\"%s\"",
			(iWritten != 0) ? " AND " : " WHERE ",
			pData->pChecksum
		);
		pQuery   += iWrite;
		iWritten += iWrite;
	}

#if NX_ENABLE_QUERY
	printf(">>> %s(): %s\n", __FUNCTION__, query);
#endif

	iRet = sqlite3_exec( m_pSqlite, query, cbQuerySuccess, &bResult, &pErrMsg );
	if( SQLITE_OK != iRet )
	{
		NXLOGE("Fail, sqlite3_exec(). ( %d, %s )\n", iRet, pErrMsg );
		return false;
	}

	return bResult;
}
