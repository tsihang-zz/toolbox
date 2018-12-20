
HOME=`pwd`

cd $RTE_SDK
make config T=$RTE_TARGET O=$RTE_TARGET
make O=$RTE_TARGET
cd -

echo "(1) Cleanup ..."
make clean
echo "(2) Compile ..."
make
echo "(3) Packaging ..."
make install

