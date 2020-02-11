#ifndef __GST_DISCOVER_H
#define __GST_DISCOVER_H

#include "NX_GstIface.h"

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

enum NX_GST_ERROR
{
    NX_GST_ERROR_NONE,
    NX_GST_ERROR_DISCOVER_FAILED,
    NX_GST_ERROR_NOT_SUPPORTED_CONTENTS,
    NX_GST_ERROR_DEMUX_LINK_FAILED,
    NX_GST_ERROR_NUM_ERRORS	
};

enum NX_GST_ERROR StartDiscover(const char* pUri, struct GST_MEDIA_INFO **pInfo);

#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // __GST_DISCOVER_H

