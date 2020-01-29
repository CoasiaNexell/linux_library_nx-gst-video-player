#ifndef GST_THUMBNAIL_H
#define GST_THUMBNAIL_H

#include "NX_GstMovie.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "[NX_GstThumbnail]"
#include <NX_Log.h>

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

char * makeThumbnail(const gchar *uri, gint64 pos_msec, gint width);

#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // GST_THUMBNAIL_H

