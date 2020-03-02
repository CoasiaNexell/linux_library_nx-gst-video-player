#ifndef __NX_GSTMEDIAINFO_H
#define __NX_GSTMEDIAINFO_H

#include "NX_GstTypes.h"

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

#ifndef LOG_TAG
#define LOG_TAG "[NxGstMediaInfo]"
#endif

NX_GST_RET      OpenMediaInfo(GST_MEDIA_INFO **media_handle);
NX_GST_ERROR    ParseMediaInfo(GST_MEDIA_INFO *media_handle, const char *filePath);
NX_GST_RET      CloseMediaInfo(GST_MEDIA_INFO *media_handle);
void            MediaInfoToStr(GST_MEDIA_INFO *media_info, const char *filePath);

#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // __NX_GSTMEDIAINFO_H
