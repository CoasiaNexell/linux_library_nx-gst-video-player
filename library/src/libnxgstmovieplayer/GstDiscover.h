#ifndef GST_DISCOVER_H
#define GST_DISCOVER_H

#include "NX_GstMovie.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "[libnxgstmovieplayer|GST_Discover]"
#include <NX_Log.h>

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

NX_GST_ERROR StartDiscover(const char* pUri, GST_MEDIA_INFO **pInfo);

#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // GST_DISCOVER_H

