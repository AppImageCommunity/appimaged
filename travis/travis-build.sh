#! /bin/bash

set -x
set -e

if [ "$ARCH" == "" ]; then
    echo "Error: \$ARCH not set"
    exit 1
fi

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

if [ "$CI" != "" ] && [ "$KEY" != "" ]; then
    # clean up and download data from GitHub
    # don't worry about removing those data -- they'll be removed by the exit hook
    wget https://github.com/AppImage/AppImageKit/files/584665/data.zip -O data.tar.gz.gpg
    set +x ; echo "$KEY" | gpg2 --batch --passphrase-fd 0 --no-tty --skip-verify --output data.tar.gz --decrypt data.tar.gz.gpg || true
    tar xf data.tar.gz
    chown -R "$USER" .gnu*/
    chmod 0700 .gnu*/
    export GNUPGHOME=$(readlink -f .gnu*/)
fi

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

# make .rpm
cpack -V -G RPM

# make AppImages
mkdir -p appdir
make install DESTDIR=appdir

wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-"$ARCH".AppImage
chmod +x linuxdeploy-"$ARCH".AppImage
./linuxdeploy-"$ARCH".AppImage --appimage-extract
mv squashfs-root/ linuxdeploy/

linuxdeploy/AppRun --list-plugins

export UPDATE_INFORMATION="gh-releases-zsync|AppImage|appimaged|continuous|appimaged*$ARCH*.AppImage.zsync"
export SIGN=1
export VERBOSE=1
# Add "hidden" dependencies; https://github.com/AppImage/libappimage/issues/104
linuxdeploy/AppRun \
    --appdir appdir \
    --library=/usr/lib/x86_64-linux-gnu/librsvg-2.so.2 \
    --library=/usr/lib/x86_64-linux-gnu/libcairo.so.2 \
    --output appimage

mv appimaged*.{AppImage,deb,rpm}* "$OLD_CWD/"
