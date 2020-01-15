#!/bin/bash

TOP=`pwd`
RESULT_DIR="result"
QT_APP_ROOT="nexell/daudio"
PACKAGE_DIR="Package"
USR_BIN="usr/bin"
USR_LIB="usr/lib"
QT_BUILD_TOP="qt_build"
BUILT_LIB="library/lib"
BUILT_BIN="bin"
PREBUILT_LIB="library/prebuilt/lib"

function package_sdk_prebuilt_all()
{
	package_sdk_prebuilt_libaries
}

function package_sdk_built_all()
{
	package_sdk_binaries
	package_sdk_libraries
	package_sdk_qtapplications
}

function package_sdk_prebuilt_libaries()
{
	echo "<< Package SDK prebuilt libraries >>"

	mkdir -p ${RESULT_DIR}/${USR_LIB}
	cp -apvR ${TOP}/${PREBUILT_LIB}/* ${RESULT_DIR}/${USR_LIB}/
}

function package_sdk_binaries()
{
	echo "<< Package SDK binaries >>"

	mkdir -p ${RESULT_DIR}/${USR_BIN}
	cp -apvR ${TOP}/${BUILT_BIN}/* ${RESULT_DIR}/${USR_BIN}/
}

function package_sdk_libraries()
{
	echo "<< Package SDK libraries >>"

	mkdir -p ${RESULT_DIR}/${USR_LIB}
	cp -apvR ${TOP}/${BUILT_LIB}/* ${RESULT_DIR}/${USR_LIB}/
}

function package_sdk_qtapplications()
{
	echo "<< Package SDK Qt applications >>"

	package_sdk_qtapplication NxGstVideoPlayer
}

function package_luncher_application()
{
	local app_name=NxLauncher
	echo "<< Package ${app_name} >>"

	cp -apvR ${TOP}/${QT_BUILD_TOP}/build-${app_name}/${app_name} ${RESULT_DIR}/${USR_BIN}/

	if [ ! -d ${RESULT_DIR}/${QT_APP_ROOT} ]; then
		mkdir -p ${RESULT_DIR}/${QT_APP_ROOT}
	fi

	if [ ${TARGET_MACHINE} == "s5p4418-convergence-daudio" ]; then
		cp -apvR ${TOP}/apps/${app_name}/configs/s5p4418_convergence_daudio.xml ${RESULT_DIR}/${QT_APP_ROOT}/daudio.xml
	elif [ ${TARGET_MACHINE} == "nxp3220-daudio" ]; then
		cp -apvR ${TOP}/apps/${app_name}/configs/nxp3220_daudio.xml ${RESULT_DIR}/${QT_APP_ROOT}/daudio.xml
	elif [ ${TARGET_MACHINE} == "nxp3220-daudio2" ]; then
		cp -apvR ${TOP}/apps/${app_name}/configs/nxp3220_daudio2.xml ${RESULT_DIR}/${QT_APP_ROOT}/daudio.xml
	elif [ ${TARGET_MACHINE} == "nxp3220-evb2" ]; then
		cp -apvR ${TOP}/apps/${app_name}/configs/nxp3220_evb2.xml ${RESULT_DIR}/${QT_APP_ROOT}/daudio.xml
	else
		cp -apvR ${TOP}/apps/${app_name}/configs/s5p4418_daudio_ref.xml ${RESULT_DIR}/${QT_APP_ROOT}/daudio.xml
	fi
}

function package_bt_service_application()
{
	local app_name=NxBTService
	echo "<< Package ${app_name} >>"

	mkdir -p ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}
	cp -apvR ${TOP}/apps/${app_name}/*.so ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/
	cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/${app_name}.desktop ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/
	if [ ${TARGET_MACHINE} == "s5p4418-convergence-daudio" ]; then
		cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxbtservice_config_convergence_daudio.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxbtservice_config.xml
	elif [ ${TARGET_MACHINE} == "nxp3220-daudio" ]; then
		cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxbtservice_config_nxp3220_daudio.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxbtservice_config.xml
	elif [ ${TARGET_MACHINE} == "nxp3220-daudio2" ]; then
		cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxbtservice_config_nxp3220_daudio2.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxbtservice_config.xml
	elif [ ${TARGET_MACHINE} == "nxp3220-evb2" ]; then
		cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxbtservice_config_nxp3220_evb2.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxbtservice_config.xml
	else
		cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxbtservice_config_daudio_ref.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxbtservice_config.xml
	fi
}

function unpackage_bt_service_application()
{
	local app_name=NxBTService
	if [ -d ${RESULT_DIR}/${QT_APP_ROOT}/${app_name} ]; then
		rm -rfvR ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}
	fi
}

function package_sdk_qtapplication()
{
	local app_name=${1}
	echo "<< Package ${app_name} >>"

	mkdir -p ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}
	cp -apvR ${TOP}/${QT_BUILD_TOP}/build-${app_name}/*.so ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/
	cp -apvR ${TOP}/${QT_BUILD_TOP}/build-${app_name}/${app_name} ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/
	cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/*.png ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/
	cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/${app_name}.desktop ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/
	if [ ${app_name} == "NxRearCam" ]; then
		if [ ${TARGET_MACHINE} == "s5p4418-convergence-daudio" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/rearcam_config_convergence_daudio.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/rearcam_config.xml
		elif [ ${TARGET_MACHINE} == "nxp3220-daudio" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/rearcam_config_nxp3220_daudio.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/rearcam_config.xml
		elif [ ${TARGET_MACHINE} == "nxp3220-daudio2" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/rearcam_config_nxp3220_daudio2.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/rearcam_config.xml
		elif [ ${TARGET_MACHINE} == "nxp3220-evb2" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/rearcam_config_nxp3220_evb2.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/rearcam_config.xml
		else
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/rearcam_config_daudio_ref.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/rearcam_config.xml
		fi
	fi
	if [ ${app_name} == "NxAudioPlayer" ]; then
		if [ ${TARGET_MACHINE} == "s5p4418-convergence-daudio" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxaudioplayer_config_convergence_daudio.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxaudioplayer_config.xml
		elif [ ${TARGET_MACHINE} == "nxp3220-daudio" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxaudioplayer_config_nxp3220_daudio.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxaudioplayer_config.xml
		elif [ ${TARGET_MACHINE} == "nxp3220-daudio2" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxaudioplayer_config_nxp3220_daudio2.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxaudioplayer_config.xml
		elif [ ${TARGET_MACHINE} == "nxp3220-evb2" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxaudioplayer_config_nxp3220_evb2.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxaudioplayer_config.xml
		else
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxaudioplayer_config_daudio_ref.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxaudioplayer_config.xml
		fi
	fi
	if [ ${app_name} == "NxVideoPlayer" ]; then
		if [ ${TARGET_MACHINE} == "s5p4418-convergence-daudio" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxvideoplayer_config_convergence_daudio.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxvideoplayer_config.xml
		elif [ ${TARGET_MACHINE} == "nxp3220-daudio" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxvideoplayer_config_nxp3220_daudio.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxvideoplayer_config.xml
		elif [ ${TARGET_MACHINE} == "nxp3220-daudio2" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxvideoplayer_config_nxp3220_daudio2.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxvideoplayer_config.xml
		elif [ ${TARGET_MACHINE} == "nxp3220-evb2" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxvideoplayer_config_nxp3220_evb2.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxvideoplayer_config.xml
		else
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxvideoplayer_config_daudio_ref.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxvideoplayer_config.xml
		fi
	fi

	if [ ${app_name} == "NxGstVideoPlayer" ]; then
		if [ ${TARGET_MACHINE} == "s5p4418-convergence-daudio" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxgstvideoplayer_config_convergence_daudio.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxgstvideoplayer_config.xml
		elif [ ${TARGET_MACHINE} == "nxp3220-daudio" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxgstvideoplayer_config_nxp3220_daudio.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxgstvideoplayer_config.xml
		elif [ ${TARGET_MACHINE} == "nxp3220-daudio2" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxgstvideoplayer_config_nxp3220_daudio2.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxgstvideoplayer_config.xml
		elif [ ${TARGET_MACHINE} == "nxp3220-evb2" ]; then
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxgstvideoplayer_config_nxp3220_evb2.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxgstvideoplayer_config.xml
		else
			cp -apvR ${TOP}/apps/${app_name}/${PACKAGE_DIR}/nxgstvideoplayer_config_daudio_ref.xml ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}/nxgstvideoplayer_config.xml
		fi
	fi
}

function unpackage_sdk_qtapplication()
{
	local app_name=${1}
	if [ -d ${RESULT_DIR}/${QT_APP_ROOT}/${app_name} ]; then
		rm -rfvR ${RESULT_DIR}/${QT_APP_ROOT}/${app_name}
	fi
}

echo ""
echo "===== Displayaudio SDK packaging ====="
echo ""

if [ -d ${RESULT_DIR} ]; then
	rm -rfR ${RESULT_DIR}
fi

mkdir -p ${RESULT_DIR}

package_sdk_prebuilt_all
package_sdk_built_all

echo ""
echo "===== Packaging complete ====="
echo ""
