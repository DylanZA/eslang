#!/usr/bin/bash
GEN="Visual Studio 15 2017 Win64"
BOOST_ROOT=$1
CMAKEFLAGS=()
CMAKEFLAGS=("-DBUILD_SHARED=OFF")
CMAKEFLAGS+=("-DPYTHON_EXECUTABLE=c:/cygwin64/bin/python2.7.exe")
CMAKEFLAGS+=("-DBOOST_ROOT=$BOOST_ROOT")
CMAKEFLAGS+=("-DBOOST_INCLUDEDIR=$BOOST_ROOT")
CMAKEFLAGS+=("-DBOOST_LIBRARYDIR=$BOOST_ROOT/lib64-msvc-14.1/")
CMAKEFLAGS+=("-DBoost_USE_STATIC_LIBS=ON")
CMAKEFLAGS+=("-DREGISTER_INSTALL_PREFIX=OFF")


for CONFIG in Release Debug
do
INSTALLDIR=$(cygpath -m `readlink -f "extinstalls/$CONFIG"`)
OUTPATH="-DCMAKE_INSTALL_PREFIX=$INSTALLDIR"
mkdir -p build/$CONFIG
pushd build/$CONFIG
cmake "-G$GEN" ${CMAKEFLAGS[@]} "$OUTPATH" ../../
popd
done
