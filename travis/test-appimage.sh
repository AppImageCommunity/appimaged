#! /bin/bash

# now check appimaged
timeout "$TIMEOUT" out/appimaged-"$ARCH".AppImage --no-install

if [ $? -ne 124 ]; then
    echo "Error: appimaged was not terminated by timeout as expected" >&2
    exit 1
fi
