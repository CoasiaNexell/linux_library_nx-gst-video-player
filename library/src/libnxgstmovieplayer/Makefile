########################################################################
# Get Linux Build Enviornment:
include ../../../env.mk

######################################################################
# Includes
INCLUDE		+= -I. -I../../prebuilt/include

######################################################################
# Library
#LIBRARY		+= -lnx_renderer

######################################################################
# Target
COBJS  	:= \
	GstDiscover.o	\
	NX_GstUtils.o	\
	NX_GstThumbnail.o

CPPOBJS	:=
CPPOBJS += \
	NX_GstMoviePlay.o

CFLAGS		+= -g -Wall -Wextra `pkg-config --cflags glib-2.0 gstreamer-1.0 gstreamer-pbutils-1.0 gdk-pixbuf-2.0`
CFLAGS		+= -fPIC

LDFLAGS		=`pkg-config --libs glib-2.0 gstreamer-1.0 gstreamer-pbutils-1.0 gdk-pixbuf-2.0`

CPPFLAGS	+= -fPIC `pkg-config --cflags glib-2.0 gstreamer-1.0 gstreamer-pbutils-1.0 gdk-pixbuf-2.0`

LIBNAME	:= libnxgstmovplayer
TARGET	:= $(LIBNAME).so

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(TARGET)

$(TARGET): depend $(OBJS)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(TARGET) -o $@ $(OBJS) $(LIBRARY)

install:
	mkdir -p ../../lib/
	cp -av $(TARGET) ../../lib/
	cp -av NX_GstMovie.h ../../include

clean:
	rm -rf $(TARGET) *.o .depend

distclean: clean
	rm -rf ../../lib/$(TARGET)
	rm -rf ../../include/NX_GstMoviePlay.h

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp)
INCS := $(INCLUDE)
depend dep:
	$(CC)  -M  $(CFLAGS)   $(INCS) $(SRCS) > .depend
