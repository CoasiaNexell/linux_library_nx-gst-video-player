#ifndef __NX_TYPEFIND_H
#define __NX_TYPEFIND_H

#include "NX_GstTypes.h"

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

gint typefind_demux (struct GST_MEDIA_INFO *media_handle, const char* filePath);
int find_avcodec_num(struct GST_MEDIA_INFO *media_handle, const char *filePath);
int typefind_codec_info(struct GST_MEDIA_INFO *media_handle, const char *uri,
        gint codec_type, gint program_idx, gint track_num);
gint get_program_info(const char* filePath, struct GST_MEDIA_INFO *media_info);
gint get_stream_info(const char* filePath, int program_number, struct GST_MEDIA_INFO *media_info);

#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // __NX_TYPEFIND_H

