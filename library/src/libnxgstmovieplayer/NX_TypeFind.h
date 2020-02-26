#ifndef __NX_TYPEFIND_H
#define __NX_TYPEFIND_H

#include "NX_GstTypes.h"

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

gint start_typefind (const char* filePath, CONTAINER_TYPE *type);
gint start_ts(const char* filePath, struct GST_MEDIA_INFO *media_info);

#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // __NX_TYPEFIND_H

