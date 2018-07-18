#! /bin/bash

set -x
set -e

TEMP_BASE=/tmp

BUILD_DIR=$(mktemp -d -p "$TEMP_BASE" appimaged-build-XXXXXX)

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

## build up-to-date version of CMake
#wget https://cmake.org/files/v3.12/cmake-3.12.0.tar.gz -O- | tar xz
#pushd cmake-*/
#./configure --prefix="$BUILD_DIR"/bin
#make install -j$(nproc)
#popd
#
#export PATH="$BUILD_DIR"/bin:"$PATH"

if [ "$ARCH" == "i386" ]; then
    export EXTRA_CMAKE_ARGS=("-DCMAKE_TOOLCHAIN_FILE=$REPO_ROOT/cmake/toolchains/i386-linux-gnu.cmake")
fi

cmake "$REPO_ROOT" -DCMAKE_INSTALL_PREFIX=/usr "${EXTRA_CMAKE_ARGS[@]}"

make -j$(nproc)

# make .deb
cpack -V -G DEB

# make AppImages
mkdir -p appdir
make install DESTDIR=appdir

wget https://github.com/TheAssassin/linuxdeploy/releases/download/continuous/linuxdeploy-"$ARCH".AppImage
chmod +x linuxdeploy-"$ARCH".AppImage
./linuxdeploy-"$ARCH".AppImage --appimage-extract
mv squashfs-root/ linuxdeploy/

export UPDATE_INFORMATION="gh-releases-zsync|AppImage|appimaged|continuous|appimaged*$ARCH*.AppImage.zsync"
linuxdeploy/AppRun --appdir appdir --output appimage

mv appimaged*.{AppImage,deb}* "$OLD_CWD/"
