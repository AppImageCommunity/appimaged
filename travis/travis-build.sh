#! /bin/bash

set -x
set -e

    TEMP_BASE=/tmp

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

wget https://github.com/TheAssassin/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage
./linuxdeploy-x86_64.AppImage --appimage-extract
mv squashfs-root/ linuxdeploy/

export UPDATE_INFORMATION="gh-releases-zsync|AppImage|appimaged|continuous|appimaged*x86_64*.AppImage.zsync"
linuxdeploy/AppRun --appdir appdir --output appimage

mv appimaged*.{AppImage,deb}* "$OLD_CWD/"
