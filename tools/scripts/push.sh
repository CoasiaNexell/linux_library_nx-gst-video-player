#!/bin/bash

echo ""
echo "===== Displayaudio SDK pushing ====="
echo ""
RESULT_DIR="result"
adb push ${RESULT_DIR} /
adb shell sync
echo ""
echo "===== Pushing complete ====="
echo ""
