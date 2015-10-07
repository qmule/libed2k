libed2k
=======

eDonkey protocol library. Fast cross-platform eDonkey protocol library. Inspired by libtorrent_rasterbar.

Main features:
- high speed
- async IO

Building
--------

* Linux, Mac OS X - static library by cmake
* Windows - Visual Studio 2008 solution in win32 directory

Android dependencies:
* toolchain: https://github.com/taka-no-me/android-cmake/
* NDK: https://www.crystax.net/ru/android/ndk

Prepare environment:
* checkout toolchain
* create standalone toolchain in crystax:  $NDK/build/tools/make-standalone-toolchain.sh --system=linux-x86_64 --toolchain=arm-linux-androideabi-4.9 --platform=android-14 --install-dir=$HOME/arm-linux-androideabi

Android build:
* mkdir android && cd android
* cmake -DCMAKE_TOOLCHAIN_FILE=som_path/android.toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DANDROID_ABI="armeabi-v7a with NEON" -DANDROID_STANDALONE_TOOLCHAIN=standalone_toolchain_path/arm-linux-androideabi ..
