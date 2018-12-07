#!/bin/bash

INSTDIR=napd

if [ ! -d $INSTDIR ]; then
	mkdir -p $INSTDIR
fi

cp $1/$RTE_TARGET/napd $INSTDIR/
cp script/* $INSTDIR/

date > $INSTDIR/build_time

SHA_GIT=`git rev-list HEAD --abbrev-commit --max-count=1`
VERSION="2.5.0.${SHA_GIT}"
TAR_FILE=napd-v$VERSION.tar.gz

tar -zcvf $TAR_FILE $INSTDIR 2>&1 >/dev/null

rm -rf $INSTDIR
