cmake_minimum_required(VERSION 3.14)

if (NOT DEFINED ENV{VITASDK} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "Please define VITASDK env var to point to your SDK path or set CMAKE_TOOLCHAIN_FILE.")
endif ()

set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Toolchain file")

project(dqivn C CXX)

include("${VITASDK}/share/vita.cmake" REQUIRED)

# ---- Vita application ------------------------------------------------------------------------------
set(VITA_APP_NAME "Dragon Quest IV")
# DQIVN_SLOT (1-9): parallel work trees build/run side by side in Vita3K with their own TITLEID (DQIVN000<slot>),
# log (dqivn<slot>.log), run cfg, saves and screenshot names. Slot 1 is main.
set(DQIVN_SLOT "1" CACHE STRING "Parallel install slot 1-9")
set(VITA_TITLEID "DQIVN000${DQIVN_SLOT}")   # the Unity+Mono experiment (port/) is DQIVV00001
add_definitions(-DDQIVN_SLOT=${DQIVN_SLOT})
set(VITA_VPKNAME "dqivn")
set(VITA_VERSION "01.00")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")

set(PSVITAIP "192.168.0.25" CACHE STRING "PSVita IP (for FTP access)")

# DATA_PATH: root of everything read/written at run time (see source/config.h). Shared with port/.
set(DATA_PATH "ux0:data/dqiv/" CACHE STRING "Data root on the Vita (trailing /)")
add_definitions(-DDATA_PATH="${DATA_PATH}")

# ---- debug switches ---------------------------------------------------------------------------------
# Debug builds: l_debug/l_info/l_warn/l_success are compiled in (l_error/l_fatal always are), the log file is
# reopened per line (survives crashes). Release: only errors.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG_SOLOADER)
  add_definitions(-DFALSOJNI_DEBUGLEVEL=1)
endif()
# Shortened Boot logo timing (0.35 s fades, 0.6 s hold instead of 1 s / 3 s): on while developing, switch off for releases.
# DQIVN_MEMSTATS: periodic PERF/MEMSTATS log lines (frame-time breakdown, draw counters, heap / vitaGL pool usage), see platform/perf.hpp. Default ON in Debug.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  option(DQIVN_MEMSTATS "Periodic performance/memory statistics" ON)
else()
  option(DQIVN_MEMSTATS "Periodic performance/memory statistics" OFF)
endif()
if (DQIVN_MEMSTATS)
  add_definitions(-DDQIVN_MEMSTATS)
endif()
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(DQIV_FAST_BOOT_DEFAULT ON)
else()
  set(DQIV_FAST_BOOT_DEFAULT OFF)
endif()
option(DQIV_FAST_BOOT "Short boot logos (development)" ${DQIV_FAST_BOOT_DEFAULT})
if (DQIV_FAST_BOOT)
  add_definitions(-DDQIV_FAST_BOOT)
endif()
# Developer tools: test scenes (*test<slot>.on), engine self tests, scripted UI (ui_* cfg keys). ON for Debug builds, OFF for Release.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(DQIV_DEV_TOOLS_DEFAULT ON)
else()
  set(DQIV_DEV_TOOLS_DEFAULT OFF)
endif()
option(DQIV_DEV_TOOLS "Compile the developer test scenes / scripted UI" ${DQIV_DEV_TOOLS_DEFAULT})
if (DQIV_DEV_TOOLS)
  add_definitions(-DDQIV_DEV_TOOLS)
endif()
# Landscape prototype (re/arch/07_LANDSCAPE.md): 960x544 logical surface, UI in a 408x544 panel (source/game/Layout.hpp)
# Landscape layout is a RUNTIME setting (layout=landscape|split|portrait, default landscape); OFF = kill switch, portrait only.
option(DQIVN_LANDSCAPE "Allow the landscape layouts (runtime cfg layout=)" ON)
if (NOT DQIVN_LANDSCAPE)
  add_definitions(-DDQIVN_NO_LANDSCAPE)
endif()
option(DQIV_TRACE_LINK "Debug: log cross-module symbol resolution" OFF)
if (DQIV_TRACE_LINK)
  add_definitions(-DDQIV_TRACE_LINK)
endif()

# ---- vitaGL (submodule lib/vitaGL, built with NO_DEBUG=1, hardfp) ------------------------------------
set(VITAGL_DIR ${CMAKE_SOURCE_DIR}/lib/vitaGL)
set(VITAGL_LIB ${VITAGL_DIR}/libvitaGL.a)
set(VITAGL_FLAGS NO_DEBUG=1)
if (CMAKE_GENERATOR MATCHES "Make")
  set(VITAGL_MAKE $(MAKE))      # share the parent jobserver
else()
  include(ProcessorCount)
  ProcessorCount(NPROC)
  set(VITAGL_MAKE make -j${NPROC})
endif()
add_custom_target(vitaGL_build
                  COMMAND ${VITAGL_MAKE} -C ${VITAGL_DIR} ${VITAGL_FLAGS}
                  BYPRODUCTS ${VITAGL_LIB}
                  COMMENT "Building vitaGL")

add_definitions(-D_GNU_SOURCE -D__POSIX_VISIBLE=999999)

# C++20 (coroutines verified with arm-vita-eabi-g++ 15: -fcoroutines is implied by -std=gnu++20)
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wl,-q -mfloat-abi=hard -std=gnu11 -Wno-deprecated")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-q -mfloat-abi=hard -std=gnu++20 -Wno-write-strings -Wno-psabi")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O1 -g3")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1 -g3")
else ()
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O3 -g0")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g0")
endif()

# ---- sources ----------------------------------------------------------------------------------------
# Globbed on purpose: work packages add files under their own directories without touching this file
# (re-run cmake / `cmake --build` picks them up thanks to CONFIGURE_DEPENDS). See README "File ownership".
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
     ${CMAKE_SOURCE_DIR}/source/*.c
     ${CMAKE_SOURCE_DIR}/source/*.cpp)
list(APPEND SOURCES
     lib/falso_jni/FalsoJNI.c
     lib/falso_jni/FalsoJNI_ImplBridge.c
     lib/falso_jni/FalsoJNI_Logger.c
     lib/falso_jni/converter.c
     lib/sha1/sha1.c
     lib/so_util/so_util.c)

# ---- CG program sources embedded as raw strings (source/shaders/*.cg -> generated/cg_sources.inc, included by engine/backend_vitagl.cpp) --------------------
set(CG_INC "${CMAKE_BINARY_DIR}/generated/cg_sources.inc")
set(CG_TEXT "// generated by CMakeLists.txt from source/shaders/*.cg -- do not edit\n")
foreach(CG dqiv.v dqiv_gray.f dqiv_sepia_fld.f dqiv_sleep.v dqiv_sleep.f)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/source/shaders/${CG}.cg)
  file(READ ${CMAKE_SOURCE_DIR}/source/shaders/${CG}.cg CG_BODY)
  string(MAKE_C_IDENTIFIER ${CG} CG_ID)
  string(APPEND CG_TEXT "static const char* const kCg_${CG_ID} = R\"CGSRC(${CG_BODY})CGSRC\";\n")
endforeach()
file(WRITE ${CG_INC} "${CG_TEXT}")
include_directories(${CMAKE_BINARY_DIR}/generated)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})
add_dependencies(${CMAKE_PROJECT_NAME} vitaGL_build)

target_include_directories(${CMAKE_PROJECT_NAME} BEFORE PUBLIC ${VITAGL_DIR}/source)
target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/lib)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
                           ${CMAKE_SOURCE_DIR}/source
                           ${CMAKE_SOURCE_DIR}/source/pack
                           ${CMAKE_SOURCE_DIR}/include)

# vita-elf-create needs room between the end of the text segment and the 64 KB aligned data segment (see tools/link_pad.ld)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -Wl,-T,${CMAKE_SOURCE_DIR}/tools/link_pad.ld)

target_link_libraries(${CMAKE_PROJECT_NAME}
                      -Wl,--whole-archive pthread -Wl,--no-whole-archive
                      OpenSLES
                      sndfile
                      mp3lame
                      mpg123
                      opus
                      vorbis
                      ogg
                      vorbisenc
                      vorbisfile
                      FLAC
                      stdc++
                      ${VITAGL_LIB}
                      vitashark
                      SceShaccCgExt
                      taihen_stub
                      SceShaccCg_stub
                      z
                      m
                      mathneon
                      kubridge_stub
                      SceAppMgr_stub
                      SceAppUtil_stub
                      SceAudio_stub
                      SceCommonDialog_stub
                      SceCtrl_stub
                      SceDisplay_stub
                      SceKernelDmacMgr_stub
                      SceGxm_stub
                      ScePower_stub
                      SceRtc_stub
                      SceTouch_stub
                      SceVshBridge_stub
                      )

# precompiled CharaSleep program (generated once by a runtime compile in Vita3K, see source/engine/backend_vitagl.cpp LoadCg); optional until it exists
if(EXISTS ${CMAKE_SOURCE_DIR}/extras/cg/dqiv_sleep.bin)
  set(DQIV_SLEEP_BLOB ${CMAKE_SOURCE_DIR}/extras/cg/dqiv_sleep.bin cg/dqiv_sleep.bin)
endif()
vita_create_self(eboot.bin ${CMAKE_PROJECT_NAME} UNSAFE NOASLR)
vita_create_vpk(${VITA_VPKNAME}.vpk ${VITA_TITLEID} eboot.bin
                VERSION ${VITA_VERSION}
                NAME ${VITA_APP_NAME}
                FILE ${CMAKE_SOURCE_DIR}/extras/livearea/icon0.png sce_sys/icon0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/pic0.png sce_sys/pic0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/startup.png sce_sys/livearea/contents/startup.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/bg0.png sce_sys/livearea/contents/bg0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/template.xml sce_sys/livearea/contents/template.xml
                ${CMAKE_SOURCE_DIR}/extras/cg/dqiv_gray.bin cg/dqiv_gray.bin
                ${CMAKE_SOURCE_DIR}/extras/cg/dqiv_sepia_fld.bin cg/dqiv_sepia_fld.bin
                ${DQIV_SLEEP_BLOB}
                )

add_custom_target(send
                  COMMAND echo destroy | nc ${PSVITAIP} 1338
                  COMMAND echo destroy | nc ${PSVITAIP} 1338
                  COMMAND curl -T eboot.bin ftp://${PSVITAIP}:1337/ux0:/app/${VITA_TITLEID}/
                  COMMAND echo launch ${VITA_TITLEID} | nc ${PSVITAIP} 1338
                  DEPENDS ${VITA_VPKNAME}.vpk-vpk
                  )
