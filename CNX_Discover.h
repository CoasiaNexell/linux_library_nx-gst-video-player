#ifndef CNX_DISCOVER_H
#define CNX_DISCOVER_H

#include "NX_MediaInfo.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "[NxGstVideoPlayer|CNX_Discover]"
#include <NX_Log.h>

class CNX_Discover
{
public:
    CNX_Discover();
    ~CNX_Discover();

	int StartDiscover(const char* pUri, struct GST_MEDIA_INFO *pInfo);
};

#undef LOG_TAG

#endif // CNX_DISCOVER_H
