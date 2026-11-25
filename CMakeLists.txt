cmake_minimum_required(VERSION 3.5)
if(DEFINED ENV{VITASDK})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "")
else()
  message(FATAL_ERROR "VITASDK is not set")
endif()
project(ngu C CXX)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "NGU Idle")
set(VITA_TITLEID "NGUI00001")
set(VITA_VERSION "01.01")

set(NGU_RUST_LIB "" CACHE FILEPATH "Rust game archive")
option(NGU_VITA_BENCHMARK "Benchmark with an isolated save directory" OFF)
option(NGU_VITA_PROFILE "Log individual slow game callbacks in benchmark builds" OFF)
option(NGU_VITA_FAST_CLOCKS "Use 444 MHz CPU and 222 MHz GPU clocks" ON)
option(NGU_VITA_VERTEX_BUFFER "Upload one streamed vertex buffer per frame" ON)
if(NOT EXISTS "${NGU_RUST_LIB}")
  message(FATAL_ERROR "Set NGU_RUST_LIB to a built Rust game archive")
endif()
set(NGU_VGL_LIB "" CACHE FILEPATH "Patched vitaGL archive built by setup-tools.sh")
if(NOT EXISTS "${NGU_VGL_LIB}")
  message(FATAL_ERROR "Run the Vita setup and provide NGU_VGL_LIB")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O2 -g -std=gnu11")
add_executable(ngu main.c stubs.c host_ime.c host_log.c)
target_compile_options(ngu PRIVATE -Wall -Wextra)
if(NGU_VITA_BENCHMARK)
  target_compile_definitions(ngu PRIVATE NGU_VITA_BENCHMARK)
endif()
if(NGU_VITA_PROFILE)
  target_compile_definitions(ngu PRIVATE NGU_VITA_PROFILE)
endif()
if(NGU_VITA_FAST_CLOCKS)
  target_compile_definitions(ngu PRIVATE NGU_VITA_FAST_CLOCKS)
endif()
if(NGU_VITA_VERTEX_BUFFER)
  target_compile_definitions(ngu PRIVATE NGU_VITA_VERTEX_BUFFER)
endif()
target_link_libraries(ngu
  ${NGU_RUST_LIB}
  ${NGU_VGL_LIB} vitashark SceShaccCgExt mathneon taihen_stub
  SceDisplay_stub SceCtrl_stub SceTouch_stub SceGxm_stub SceSysmodule_stub SceCommonDialog_stub
  SceAppMgr_stub SceKernelThreadMgr_stub SceLibKernel_stub SceIofilemgr_stub SceProcessmgr_stub
  SceRtc_stub ScePower_stub SceShaccCg_stub SceIme_stub SceKernelDmacMgr_stub
  pthread stdc++ m z)
# Leave space for the SCE module metadata appended by vita-elf-create.
target_link_options(ngu PRIVATE -Wl,--wrap=invoke_splashscreen -Wl,--defsym=__sce_headroom=0x4000)

vita_create_self(eboot.bin ngu)
vita_create_vpk(ngu.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE livearea/icon0.png sce_sys/icon0.png
  FILE livearea/bg.png sce_sys/livearea/contents/bg.png
  FILE livearea/startup.png sce_sys/livearea/contents/startup.png
  FILE livearea/template.xml sce_sys/livearea/contents/template.xml)
