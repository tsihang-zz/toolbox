#!/bin/bash

#TARGET=x86_64-native-linuxapp-gcc

chmod 755 ./scripts/*.sh
#make config T=$RTE_TARGET O=$RTE_TARGET
#make O=$RTE_TARGET

#export RTE_SDK=`pwd`
#export RTE_TARGET=$TARGET

make clean
make

if [ ! -d "release" ]; then
	mkdir release
fi

#cp ./ncapdc/build/ncapdc ./release
#cp ./ncapds/build/ncapds ./release
#cp ./script/*.sh ./release
cp ./scripts/*.sh ./release
cp ./ncapds/ncapds/$RTE_TARGET/ncapds ./release
cp ./ncapdc/ncapdc/$RTE_TARGET/ncapdc ./release

chmod 755 ./release/*
cp $RTE_SDK/$RTE_TARGET/build/lib/librte_eal/linuxapp/igb_uio/igb_uio.ko ./release

date > ./release/build_time
SHA_GIT=`git rev-list HEAD --abbrev-commit --max-count=1`
VERSION="2.5.0.${SHA_GIT}"
tar -zcvf ncapd-v$VERSION.tar.gz ./release

