#! /bin/bash

set -x
set -e

# use RAM disk if possible
if [ -d /dev/shm ]; then
    TEMP_BASE=/dev/shm
else
    TEMP_BASE=/tmp
fi

BUILD_DIR=$(mktemp -d -p "$TEMP_BASE" AppImageUpdate-build-XXXXXX)

cleanup () {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
}

trap cleanup EXIT

# store repo root as variable
REPO_ROOT=$(readlink -f $(dirname $(dirname $0)))
OLD_CWD=$(readlink -f .)

pushd "$BUILD_DIR"

cmake "$REPO_ROOT" -DCMAKE_INSTALL_PREFIX=/usr

make -j$(nproc)

# make .deb
cpack -V -G DEB

# make AppImages
mkdir -p appdir
make install DESTDIR=appdir

wget https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage
chmod +x linuxdeployqt-continuous-x86_64.AppImage
./linuxdeployqt-continuous-x86_64.AppImage --appimage-extract
mv squashfs-root/ linuxdeployqt/

linuxdeployqt/AppRun appdir/usr/share/applications/appimaged.desktop -bundle-non-qt-libs

linuxdeployqt/usr/bin/appimagetool appdir/ -u "gh-releases-zsync|AppImage|appimaged|appimaged*x86_64*.AppImage.zsync"

mv appimaged*.{AppImage,deb}* "$OLD_CWD/"
