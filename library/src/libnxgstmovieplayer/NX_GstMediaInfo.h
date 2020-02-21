#ifndef __NX_GSTMEDIAINFO_H
#define __NX_GSTMEDIAINFO_H

#include "NX_GstTypes.h"

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

#ifndef LOG_TAG
#define LOG_TAG "[NxGstMediaInfo]"
#endif

NX_GST_RET  NX_GST_Open_MediaInfo(GST_MEDIA_INFO **ty_handle);
NX_GST_RET  NX_GST_Get_MediaInfo(GST_MEDIA_INFO *ty_media_handle, const char *uri);
NX_GST_RET  NX_GST_Close_MediaInfo(GST_MEDIA_INFO *ty_handle);

#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // __NX_GSTMEDIAINFO_H
