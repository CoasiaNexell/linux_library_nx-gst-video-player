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

#ifndef __CNX_FILE_H__
#define __CNX_FILE_H__

#include <stdint.h>

class CNX_File
{
public:
	CNX_File();
	CNX_File( const char *pFile );
	~CNX_File();

public:
	int32_t Open( const char *pFile );
	void	Close();

	int32_t IsExist();
	int32_t IsRegularFile();
	int32_t IsDirectory();

	int32_t GetSize();
	int32_t GetName( char **ppResult );
	int32_t GetDirname( char **ppResult );
	int32_t GetBasename( char **ppResult );
	int32_t GetExtension( char **ppResult );

	int32_t GetAccessDate( char **ppResult );
	int32_t GetModifyDate( char **ppResult );
	int32_t GetChangeDate( char **ppResult );
	int32_t GetChecksum( char **ppResult );

private:
	enum { MAX_DATE_NUM = 19+1 };

	char	*m_pFile;
	char	*m_pBasename;
	char	*m_pDirname;
	char	*m_pExtension;
	char	*m_pAccessDate;
	char	*m_pModifyDate;
	char	*m_pChangeDate;
	char	*m_pChecksum;

private:
	CNX_File (const CNX_File &Ref);
	CNX_File &operator=(const CNX_File &Ref);
};

#endif	// __CNX_FILE_H__
