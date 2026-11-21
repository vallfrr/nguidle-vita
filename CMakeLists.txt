cmake_minimum_required(VERSION 3.5)
if(DEFINED ENV{VITASDK})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "")
else()
  message(FATAL_ERROR "VITASDK non defini")
endif()
project(ngu C CXX)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "NGU Idle")
set(VITA_TITLEID "NGUI00001")
set(VITA_VERSION "01.01")

set(NGU_RUST_LIB "" CACHE FILEPATH "Rust game archive")
option(NGU_NATIVE_LOGIC "Use translated game logic" OFF)
if(NOT EXISTS "${NGU_RUST_LIB}")
  message(FATAL_ERROR "Set NGU_RUST_LIB to a built Rust game archive")
endif()
set(NGU_VGL_LIB "$ENV{VITASDK}/arm-vita-eabi/lib/libvitaGL.a.orig" CACHE FILEPATH "vitaGL")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O2 -g -std=gnu11")
add_executable(ngu main.c stubs.c)
if(NGU_NATIVE_LOGIC)
  target_compile_definitions(ngu PRIVATE NGU_NATIVE_LOGIC)
endif()
target_link_libraries(ngu
  ${NGU_RUST_LIB}
  ${NGU_VGL_LIB} vitashark SceShaccCgExt mathneon taihen_stub
  SceDisplay_stub SceCtrl_stub SceTouch_stub SceGxm_stub SceSysmodule_stub SceCommonDialog_stub
  SceAppMgr_stub SceKernelThreadMgr_stub SceLibKernel_stub SceIofilemgr_stub SceProcessmgr_stub
  SceRtc_stub ScePower_stub SceShaccCg_stub SceIme_stub SceKernelDmacMgr_stub
  pthread stdc++ m z)
target_link_options(ngu PRIVATE -Wl,--allow-multiple-definition)

vita_create_self(eboot.bin ngu)
vita_create_vpk(ngu.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE livearea/icon0.png sce_sys/icon0.png
  FILE livearea/bg.png sce_sys/livearea/contents/bg.png
  FILE livearea/startup.png sce_sys/livearea/contents/startup.png
  FILE livearea/template.xml sce_sys/livearea/contents/template.xml)
