#ifndef __GST_DISCOVER_H
#define __GST_DISCOVER_H

#include "NX_GstMoviePlay.h"

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

NX_GST_ERROR StartDiscover(const char* pUri, GST_MEDIA_INFO **pInfo);

#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // __GST_DISCOVER_H

