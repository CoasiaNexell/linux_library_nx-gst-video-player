libnxgstvplayer_la_CPPFLAGS = \
	$(WARN_CFLAGS) \
	$(GST_CFLAGS) \
	-O -DDEBUG -g \
	-I./include \
	-I${includedir} \
	-I${includedir}/gdk-pixbuf-2.0

libnxgstvplayer_la_LTLIBRARIES = libnxgstvplayer.la
libnxgstvplayer_ladir = ${libdir}

# library version
libnxgstvplayer_la_LDFLAGS = -version-number 0:9:0

# add dependency libraries
libnxgstvplayer_la_LDFLAGS += \
	$(GST_LIBS) \
	-lgstmpegts-1.0 \
	-lgdk_pixbuf-2.0

libnxgstvplayer_la_SOURCES = \
	NX_GstDiscover.c \
	NX_GstLog.c \
	NX_GstThumbnail.c \
	NX_TypeFind.c \
	NX_TSProgram.c \
	NX_OMXSemaphore.c \
	NX_GstMediaInfo.cpp \
	NX_GstMoviePlay.cpp
