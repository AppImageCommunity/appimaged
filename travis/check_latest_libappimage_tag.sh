#!/usr/bin/env bash

pushd lib/libappimage

latesttag=$(git describe --tags)
echo "checking out libappimage: ${latesttag}"
git checkout ${latesttag}

popd
