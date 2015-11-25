libed2k
=======

eDonkey protocol library. Fast cross-platform eDonkey protocol library. Inspired by libtorrent_rasterbar.

Main features:
- high speed
- async IO

Building
--------
Supported platforms
* Linux
* Mac OS 
* Windows
* Android

Common dependencies:
* boost
* cmake

Windows, Linux, Mac OS X:
* git clone https://github.com/qmule/libed2k.git
* mkdir libed2k/build
* cd libed2k/build && cmake ..
* make (on Windows skip this step)


Android cross platform compilation on Linux
--------

Use some dev directory PATH.

Android tools and additional dependencies:
* Android cmake toolchain: https://github.com/taka-no-me/android-cmake/
* Android NDK: android-ndk-r10d-linux-x86_64.bin
* Boost for Android https://github.com/MysticTreeGames/Boost-for-Android

Prepare environment:
* Checkout  and install NDK: android-ndk-r10d-linux-x86_64.bin
* Create stadalone toolchain($NDK/build/tools/make-standalone-toolchain.sh). ${PATH}/arm-linux-androideabi_standalone
* Build boost libraries using Boost for Android manual. For example boost 1.53
* export BOOST_ROOT=${PATH}/Boost-for-Android/build/include/boost-1_53
* export BOOST_LIBRARYDIR=${PATH}/Boost-for-Android/build/lib/
* Possibly you have to rename boost libraries to libboost_xxx.a

Android build:
* git clone https::/github.com/qmule/libed2k.git
* mkdir android && cd android
* cmake -DCMAKE_TOOLCHAIN_FILE=${PATH}/android-cmake/android.toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DANDROID_ABI="armeabi-v7a with NEON" ADANDROID_STANDALONE_TOOLCHAIN=${PATH}/arm-linux-androideabi_standalone  ..
* make
