# GNU MinGW-w64 cross-build (run on Linux with i686 tools installed).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)
find_program(CMAKE_C_COMPILER NAMES i686-w64-mingw32-gcc-posix i686-w64-mingw32-gcc REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES i686-w64-mingw32-g++-posix i686-w64-mingw32-g++ REQUIRED)
find_program(CMAKE_RC_COMPILER NAMES i686-w64-mingw32-windres REQUIRED)
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static -static-libstdc++ -static-libgcc")
