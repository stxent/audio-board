Installation
------------

Audio Board project implements controls over TLV320AIC310x codec and TPA6017A2 power amplifier. It requires GNU toolchain for ARM Cortex-M processors and CMake version 3.21.

Quickstart
----------

Clone git repository:

```sh
git clone https://github.com/stxent/audio-board.git
cd audio-board
git submodule update --init --recursive
```

Build release version:

```sh
mkdir build
cd build
cmake .. -DPLATFORM=LPC11XX -DBOARD=audioboard_v1 -DCMAKE_TOOLCHAIN_FILE=libs/xcore/toolchains/cortex-m0.cmake -DCMAKE_BUILD_TYPE=Release -DUSE_LTO=OFF -DUSE_WDT=ON
make
```

Build release version with fixed board configuration switches:

```sh
mkdir build
cd build
cmake .. -DPLATFORM=LPC11XX -DBOARD=audioboard_v1 -DCMAKE_TOOLCHAIN_FILE=libs/xcore/toolchains/cortex-m0.cmake -DCMAKE_BUILD_TYPE=Release -DUSE_LTO=OFF -DUSE_WDT=ON -DOVERRIDE_SW=32
make
```

All firmwares are placed in a *board* directory inside the *build* directory.

Useful settings
---------------

* CMAKE_BUILD_TYPE — specifies the build type. Possible values are empty, Debug, Release, RelWithDebInfo and MinSizeRel.
* USE_DBG — enables debug messages and profiling.
* USE_LTO — enables Link Time Optimization.
* USE_WDT — explicitly enables a watchdog timer for all build types.
