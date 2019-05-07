cmake_minimum_required(VERSION 2.8)

project(ZenekGienek C)

SET(LIBSUPERDERPY_GAMENAME "zenekgienek" CACHE INTERNAL "")
SET(LIBSUPERDERPY_GAMENAME_PRETTY "Zenek i Gienek przejmują władzę nad światem" CACHE INTERNAL "")

SET(LIBSUPERDERPY_APPID "net.dosowisko.ZenekGienek")
SET(LIBSUPERDERPY_ORIENTATION "LANDSCAPE" CACHE INTERNAL "")
SET(LIBSUPERDERPY_VERSION "1.0" CACHE INTERNAL "")

SET(EMSCRIPTEN_TOTAL_MEMORY "640")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_SOURCE_DIR}/libsuperderpy/cmake")

include(libsuperderpy)

add_subdirectory(libsuperderpy)
add_subdirectory(src)
add_subdirectory(data)
