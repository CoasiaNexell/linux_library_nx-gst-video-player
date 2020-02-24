#ifndef __NX_GSTMEDIAINFO_H
#define __NX_GSTMEDIAINFO_H

#include "NX_GstTypes.h"

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

#ifndef LOG_TAG
#define LOG_TAG "[NxGstMediaInfo]"
#endif

NX_GST_RET      NX_GST_OpenMediaInfo(GST_MEDIA_INFO **media_handle);
NX_GST_ERROR    NX_GST_GetMediaInfo(GST_MEDIA_INFO *media_handle, const char *uri);
NX_GST_RET      NX_GST_CloseMediaInfo(GST_MEDIA_INFO *media_handle);

#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // __NX_GSTMEDIAINFO_H
