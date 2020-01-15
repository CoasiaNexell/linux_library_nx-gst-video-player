ifndef	JOBS
JOBS := 8
endif

ifeq ($(OE_QMAKE_AR), )
export OE_QMAKE_AR = $(NX_OE_QMAKE_AR)
endif
ifeq ($(OE_QMAKE_CC), )
export OE_QMAKE_CC = $(NX_OE_QMAKE_CC)
endif
ifeq ($(OE_QMAKE_CFLAGS), )
export OE_QMAKE_CFLAGS = $(NX_OE_QMAKE_CFLAGS)
endif
ifeq ($(OE_QMAKE_CXX), )
export OE_QMAKE_CXX = $(NX_OE_QMAKE_CXX)
endif
ifeq ($(OE_QMAKE_CXXFLAGS), )
export OE_QMAKE_CXXFLAGS = $(NX_OE_QMAKE_CXXFLAGS)
endif
ifeq ($(OE_QMAKE_STRIP), )
export OE_QMAKE_STRIP = $(NX_OE_QMAKE_STRIP)
endif
ifeq ($(OE_QMAKE_INCDIR_QT), )
export OE_QMAKE_INCDIR_QT = $(NX_OE_QMAKE_INCDIR_QT)
endif
ifeq ($(OE_QMAKE_LDFLAGS), )
export OE_QMAKE_LDFLAGS = $(NX_OE_QMAKE_LDFLAGS)
endif
ifeq ($(OE_QMAKE_LIBDIR_QT), )
export OE_QMAKE_LIBDIR_QT = $(NX_OE_QMAKE_LIBDIR_QT)
endif
ifeq ($(OE_QMAKE_LINK), )
export OE_QMAKE_LINK = $(NX_OE_QMAKE_LINK)
endif
ifeq ($(OE_QMAKE_QDBUSCPP2XML), )
export OE_QMAKE_QDBUSCPP2XML = $(NX_OE_QMAKE_QDBUSCPP2XML)
endif
ifeq ($(OE_QMAKE_QDBUSXML2CPP), )
export OE_QMAKE_QDBUSXML2CPP = $(NX_OE_QMAKE_QDBUSXML2CPP)
endif
ifeq ($(OE_QMAKE_QT_CONFIG), )
export OE_QMAKE_QT_CONFIG = $(NX_OE_QMAKE_QT_CONFIG)
endif
ifeq ($(OE_QMAKE_MOC), )
export OE_QMAKE_MOC = $(NX_OE_QMAKE_MOC)
endif
ifeq ($(OE_QMAKE_RCC), )
export OE_QMAKE_RCC = $(NX_OE_QMAKE_RCC)
endif
ifeq ($(OE_QMAKE_UIC), )
export OE_QMAKE_UIC = $(NX_OE_QMAKE_UIC)
endif
ifeq ($(OE_QMAKE_PATH_HOST_BINS), )
export OE_QMAKE_PATH_HOST_BINS = $(NX_OE_QMAKE_PATH_HOST_BINS)
endif
ifeq ($(QMAKESPEC), )
export QMAKESPEC = $(NX_OE_XQMAKESPEC)
endif
ifeq ($(QT_CONF_PATH), )
export QT_CONF_PATH = $(NX_QT_CONF_PATH)
endif
include machine.mk

QMAKE_PATH = $(OE_QMAKE_PATH_HOST_BINS)

TOP_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

LIB_TOP := library/src
APP_TOP := apps
TOOL_TOP := tools
BUILD_TOP := qt_build
BIN_TOP := bin
RESULT_TOP := result
OUT_LIB_TOP := library/lib
OUT_INC_TOP	:= library/include

LIBS :=
LIBS += libnxgstmovieplayer

QT_LIBS :=

APPS :=

QT_APPS :=
QT_APPS += NxGstVideoPlayer

TOOLS :=

######################################################################
# Build
all:
	@echo "===== Displayaudio SDK building ====="
	@for dir in $(LIBS); do							\
	make -C $(LIB_TOP)/$$dir -j$(JOBS) || exit $?;	\
	make -C $(LIB_TOP)/$$dir install || exit $?;	\
	done
	@for dir in $(APPS); do							\
	make -C $(APP_TOP)/$$dir -j$(JOBS) || exit $?;	\
	make -C $(APP_TOP)/$$dir install || exit $?;	\
	done
	@for dir in $(TOOLS); do						\
	make -C $(TOOL_TOP)/$$dir -j$(JOBS) || exit $?;	\
	make -C $(TOOL_TOP)/$$dir install || exit $?;	\
	done
	@for dir in $(QT_LIBS); do						\
	mkdir -p $(BUILD_TOP)/build-$$dir;				\
	cd $(BUILD_TOP)/build-$$dir; 					\
	$(QMAKE_PATH)/qmake $(TOP_DIR)/$(LIB_TOP)/$$dir || exit $?;	\
	make -j$(JOBS) || exit $?;						\
	make install || exit $?;						\
	cd -; 											\
	done
	@for dir in $(QT_APPS); do						\
	mkdir -p $(BUILD_TOP)/build-$$dir;				\
	cd $(BUILD_TOP)/build-$$dir; 					\
	$(QMAKE_PATH)/qmake $(TOP_DIR)/$(APP_TOP)/$$dir || exit $?;	\
	make -j$(JOBS) || exit $?;						\
	cd -; 											\
	done
	@echo ""
	@echo "===== Building complete ====="
	@echo ""
	./tools/scripts/make_package.sh

clean_linux:
	@for dir in $(LIBS); do							\
	make -C $(LIB_TOP)/$$dir clean || exit $?;		\
	done
	@for dir in $(APPS); do							\
	make -C $(APP_TOP)/$$dir clean || exit $?;		\
	done
	@for dir in $(TOOLS); do						\
	make -C $(TOOL_TOP)/$$dir clean || exit $?;		\
	done

distclean_linux:
	@for dir in $(LIBS); do							\
	make -C $(LIB_TOP)/$$dir distclean || exit $?;	\
	done
	@for dir in $(APPS); do							\
	make -C $(APP_TOP)/$$dir distclean || exit $?;	\
	done
	@for dir in $(TOOLS); do						\
	make -C $(TOOL_TOP)/$$dir distclean || exit $?;	\
	done

clean_qt:
	@for dir in $(QT_LIBS); do						\
	if [ -d $(BUILD_TOP)/build-$$dir ] ; then		\
		cd $(BUILD_TOP)/build-$$dir || exit $?;		\
		make clean || exit $?;						\
		cd -; 										\
	fi												\
	done
	@for dir in $(QT_APPS); do						\
	if [ -d $(BUILD_TOP)/build-$$dir ] ; then		\
		cd $(BUILD_TOP)/build-$$dir || exit $?;		\
		make clean || exit $?;						\
		cd -; 										\
	fi												\
	done

clean_libs:
	@for dir in $(OUT_LIB_TOP); do					\
	cd $(OUT_LIB_TOP);								\
	rm -rf *.so *.so.*;								\
	cd -;											\
	done

clean_header:
	@for dir in $(OUT_INC_TOP); do					\
	cd $(OUT_INC_TOP);								\
	rm -rf *.h;										\
	cd -;											\
	done

clean:
	make clean_linux
	@if [ -d ${BUILD_TOP} ] ; then					\
		make clean_qt;								\
	fi

distclean:
	make distclean_linux
	make clean_libs
	make clean_header
	rm -rf $(BUILD_TOP)
	rm -rf $(RESULT_TOP)

package:
	./tools/scripts/make_package.sh

push:
	./tools/scripts/push.sh
