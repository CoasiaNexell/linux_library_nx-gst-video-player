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

#ifndef __NX_TYPE_H__
#define __NX_TYPE_H__

#include <stdint.h>

enum NxMediaEvent {
	NX_EVENT_MEDIA_UNKNOWN = -1,
	NX_EVENT_MEDIA_SDCARD_INSERT,
	NX_EVENT_MEDIA_SDCARD_REMOVE,
	NX_EVENT_MEDIA_USB_INSERT,
	NX_EVENT_MEDIA_USB_REMOVE,
	NX_EVENT_MEDIA_SCAN_DONE
};
//
// 
//
enum FocusType {
    FocusType_Get,
    FocusType_Set
};

enum FocusPriority {
	FocusPriority_Normal,
	FocusPriority_High,
	FocusPriority_Highest
};

enum ButtonType {
	ButtonType_Ok = 1,
	ButtonType_Cancel,
	ButtonType_Count
};

enum ButtonVisibility {
	ButtonVisibility_Ok = ButtonType_Ok,
	ButtonVisibility_Cencel = ButtonType_Cancel,
	ButtonVisibility_Default = ButtonType_Ok|ButtonType_Cancel,
	ButtonVisibility_Count = ButtonVisibility_Default
};

enum ButtonLocation {
	ButtonLocation_Ok_Cancel,
	ButtonLocation_Cancel_Ok
};

struct PopupMessage {
	char *pMsgTitle;
	char *pMsgBody;

	ButtonVisibility eVisibility;
	ButtonLocation eLocation;
	const char *pStylesheet[ButtonType_Count];

	unsigned int uiTimeout;

	PopupMessage()
	{
		pMsgTitle = 0;
		pMsgBody = 0;

		eVisibility = ButtonVisibility_Default;

		eLocation = ButtonLocation_Cancel_Ok;

		pStylesheet[ButtonType_Ok] = 0;
		pStylesheet[ButtonType_Cancel] = 0;

		uiTimeout = 0;
	}
};

//
//	Volume Information
//
#define NX_MEDIA_DATABASE_PATH	"/home/root"
#define NX_MEDIA_DATABASE_NAME	"mediainfo.db"
#define NX_MEDIA_DATABASE_TABLE	"tbl_media"

enum {
	NX_VOLUME_TYPE_UNKNOWN = -1,
	NX_VOLUME_TYPE_MSDOS,
	NX_VOLUME_TYPE_VFAT,
	NX_VOLUME_TYPE_NTFS,
	NX_VOLUME_TYPE_EXT2,
	NX_VOLUME_TYPE_EXT3,
	NX_VOLUME_TYPE_EXT4,
};

typedef struct NX_VOLUME_INFO {
	char	szDev[64];				//	Storage Device Node
	char	szVolume[64];			//	Mount Position
	int32_t iType;					//	Mount VFS Type
} NX_VOLUME_INFO;


//
//	Version Information
//
#define NX_VERSION_LIBKEYRECEIVER	"1.0.0"
#define NX_VERSION_LIBAVIN			"1.0.0"
#define NX_VERSION_LIBBASEUI		"1.0.0"
#define NX_VERSION_LIBCONFIG		"1.0.0"
#define NX_VERSION_LIBDAUDIOIPC		"1.0.0"
#define NX_VERSION_LIBDAUDIOUTILS	"1.0.0"
#define NX_VERSION_LIBPACPCLIENT	"1.0.0"
#define NX_VERSION_LIBREARCAM		"1.0.0"

#endif	// __NX_TYPE_H__
