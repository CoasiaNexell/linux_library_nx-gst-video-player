#ifndef __NX_TYPEFIND_H
#define __NX_TYPEFIND_H

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

gint start_typefind (const char* filePath);
gint find_avcodec_num_ps(const char* filePath);

#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // __NX_TYPEFIND_H

