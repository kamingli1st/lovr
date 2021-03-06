cmake_minimum_required(VERSION 3.1.0)
cmake_policy(SET CMP0063 NEW)
project(lovr)

# Options
option(LOVR_ENABLE_AUDIO "Enable the audio module" ON)
option(LOVR_ENABLE_DATA "Enable the data module" ON)
option(LOVR_ENABLE_EVENT "Enable the event module" ON)
option(LOVR_ENABLE_FILESYSTEM "Enable the filesystem module" ON)
option(LOVR_ENABLE_GRAPHICS "Enable the graphics module" ON)
option(LOVR_ENABLE_HEADSET "Enable the headset module" ON)
option(LOVR_ENABLE_MATH "Enable the math module" ON)
option(LOVR_ENABLE_PHYSICS "Enable the physics module" ON)
option(LOVR_ENABLE_THREAD "Enable the thread module" ON)
option(LOVR_ENABLE_TIMER "Enable the timer module" ON)

option(LOVR_ENABLE_ENET "Bundle with lua-enet" ON)
option(LOVR_ENABLE_JSON "Bundle with lua-cjson" ON)

option(LOVR_USE_LUAJIT "Use LuaJIT instead of Lua" ON)
option(LOVR_USE_OPENVR "Enable the OpenVR backend for the headset module" ON)
option(LOVR_USE_WEBVR "Enable the WebVR backend for the headset module" OFF)
option(LOVR_USE_OCULUS "Enable the LibOVR backend for the headset module (be sure to also set LOVR_OCULUS_PATH to point to the Oculus SDK)" OFF)
option(LOVR_USE_OCULUS_MOBILE "Enable the Oculus Mobile (Android) backend for the headset module" OFF)
option(LOVR_USE_DESKTOP_HEADSET "Enable the keyboard/mouse backend for the headset module" ON)
option(LOVR_USE_LEAP "Enable the Leap Motion backend for the headset module" OFF)
option(LOVR_USE_SSE "Enable SIMD use of intrinsics" ON)

option(LOVR_SYSTEM_PHYSFS "Use the system-provided PhysFS" OFF)
option(LOVR_SYSTEM_ENET "Use the system-provided enet" OFF)
option(LOVR_SYSTEM_GLFW "Use the system-provided glfw" OFF)
option(LOVR_SYSTEM_LUA "Use the system-provided Lua" OFF)
option(LOVR_SYSTEM_ODE "Use the system-provided ODE" OFF)
option(LOVR_SYSTEM_OPENAL "Use the system-provided OpenAL" OFF)

option(LOVR_BUILD_SHARED "Build as a shared library instead of an executable" OFF)

# Setup
if(EMSCRIPTEN)
  string(CONCAT LOVR_EMSCRIPTEN_FLAGS
    "-O3 "
    "-s WASM=1 "
    "-s USE_GLFW=3 "
    "-s USE_WEBGL2=1 "
    "-s GL_PREINITIALIZED_CONTEXT=1 "
    "-s USE_ZLIB=1 "
    "-s USE_PTHREADS=0 "
    "-s FULL_ES2=1 "
    "-s FULL_ES3=1 "
    "-s FORCE_FILESYSTEM=1 "
    "-s ALLOW_MEMORY_GROWTH=1 "
    "-s \"EXPORTED_FUNCTIONS=[ "
      "'_main','_lovrDestroy',"
      "'_mat4_identity','_mat4_invert','_mat4_multiply','_mat4_rotateQuat','_mat4_transform','_mat4_transformDirection','_mat4_translate',"
      "'_quat_fromMat4','_quat_getAngleAxis'"
    "]\" "
    "-s \"EXTRA_EXPORTED_RUNTIME_METHODS=['getValue','setValue']\" "
    "--js-library \"${CMAKE_CURRENT_SOURCE_DIR}/src/resources/webvr.js\" "
    "--shell-file \"${CMAKE_CURRENT_SOURCE_DIR}/src/resources/lovr.html\""
  )
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${LOVR_EMSCRIPTEN_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LOVR_EMSCRIPTEN_FLAGS}")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  set(LOVR_USE_WEBVR ON)
  set(LOVR_USE_OPENVR OFF)
  set(LOVR_USE_OCULUS OFF)
  set(LOVR_ENABLE_THREAD OFF)
elseif(ANDROID)
  set(LOVR_USE_OPENVR OFF)
  set(LOVR_USE_OCULUS OFF)
  set(LOVR_USE_DESKTOP_HEADSET OFF)
  set(LOVR_USE_OCULUS_MOBILE ON)
  set(LOVR_BUILD_SHARED ON) # Android has only "activities"
elseif(UNIX)
  if(APPLE)
    set(CMAKE_MACOSX_RPATH 1)
  endif()
  find_package(PkgConfig)
endif()

if(EMSCRIPTEN OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm" OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch64")
  set(LOVR_USE_SSE OFF)
endif()

# PhysicsFS
if(LOVR_ENABLE_FILESYSTEM)
  if(LOVR_SYSTEM_PHYSFS)
    find_package(PhysFS REQUIRED)
    include_directories(${PHYSFS_INCLUDE_DIR})
    set(LOVR_PHYSFS ${PHYSFS_LIBRARY})
  else()
    set(PHYSFS_BUILD_STATIC OFF CACHE BOOL "")
    set(PHYSFS_ARCHIVE_7Z OFF CACHE BOOL "")
    set(PHYSFS_ARCHIVE_GRP OFF CACHE BOOL "")
    set(PHYSFS_ARCHIVE_WAD OFF CACHE BOOL "")
    set(PHYSFS_ARCHIVE_HOG OFF CACHE BOOL "")
    set(PHYSFS_ARCHIVE_MVL OFF CACHE BOOL "")
    set(PHYSFS_ARCHIVE_QPAK OFF CACHE BOOL "")
    set(PHYSFS_ARCHIVE_SLB OFF CACHE BOOL "")
    set(PHYSFS_ARCHIVE_ISO9660 OFF CACHE BOOL "")
    set(PHYSFS_ARCHIVE_VDF OFF CACHE BOOL "")
    set(PHYSFS_BUILD_TEST OFF CACHE BOOL "")
    set(PHYSFS_BUILD_WX_TEST FALSE CACHE BOOL "")
    add_subdirectory(deps/physfs physfs)
    include_directories(deps/physfs/src)
    set(LOVR_PHYSFS physfs)
  endif()
endif()

# enet
if(LOVR_ENABLE_ENET)
  if(LOVR_SYSTEM_ENET)
    pkg_search_module(ENET REQUIRED enet)
    include_directories(${ENET_INCLUDE_DIRS})
    set(LOVR_ENET ${ENET_LIBRARIES})
  else()
    if(EMSCRIPTEN)
      set(HAVE_HAS_SOCKLEN_T TRUE CACHE BOOL "")
    endif()
    add_subdirectory(deps/enet enet)
    include_directories(deps/enet/include)
    set(LOVR_ENET enet)
    if(WIN32)
      set(LOVR_ENET ${LOVR_ENET} ws2_32 winmm)
    endif()
    if(EMSCRIPTEN)
      target_compile_definitions(enet PRIVATE __APPLE__)
    endif()
  endif()
endif()

# GLFW
if(NOT (EMSCRIPTEN OR ANDROID))
  if(LOVR_SYSTEM_GLFW)
    pkg_search_module(GLFW REQUIRED glfw3)
    include_directories(${GLFW_INCLUDE_DIRS})
    set(LOVR_GLFW ${GLFW_LIBRARIES})
  else()
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
    set(BUILD_SHARED_LIBS ON CACHE BOOL "")
    add_subdirectory(deps/glfw glfw)
    include_directories(deps/glfw/include)
    set(LOVR_GLFW glfw ${GLFW_LIBRARIES})
  endif()
  unset(LIB_SUFFIX CACHE)
endif()

# Lua
if(LOVR_USE_LUAJIT AND NOT EMSCRIPTEN)
  if (APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pagezero_size 10000 -image_base 100000000")
  endif()
  if(LOVR_SYSTEM_LUA)
    pkg_search_module(LUAJIT REQUIRED luajit)
    include_directories(${LUAJIT_INCLUDE_DIRS})
    set(LOVR_LUA ${LUAJIT_LIBRARIES})
  else()
    add_subdirectory(deps/luajit luajit)
    include_directories(deps/luajit/src ${CMAKE_BINARY_DIR}/luajit)
    set(LOVR_LUA libluajit)
  endif()
else()
  if(EMSCRIPTEN)
    option(LUA_USE_RELATIVE_LOADLIB OFF)
    option(LUA_USE_ULONGJMP OFF)
    option(LUA_USE_POPEN OFF)
  endif()
  if(LOVR_SYSTEM_LUA)
    pkg_search_module(LUA REQUIRED lua)
    include_directories(${LUA_INCLUDE_DIRS})
    set(LOVR_LUA ${LUA_LIBRARIES})
  else()
    add_subdirectory(deps/lua lua)
    include_directories(deps/lua/src ${CMAKE_BINARY_DIR}/lua)
    set(LOVR_LUA liblua_static)
    set_target_properties(lua luac liblua liblua_static PROPERTIES EXCLUDE_FROM_ALL 1)
  endif()
endif()

# MSDF
if(LOVR_ENABLE_DATA)
  set(BUILD_SHARED_LIBS OFF)
  add_subdirectory(deps/msdfgen lib_msdfgen)
  set(BUILD_SHARED_LIBS ON)
  include_directories(deps/msdfgen)
  set(LOVR_MSDF lib_msdfgen)
endif()

# ODE
if(LOVR_ENABLE_PHYSICS)
  if(LOVR_SYSTEM_ODE)
    pkg_search_module(ODE REQUIRED ode)
    pkg_search_module(CCD REQUIRED ccd)
    include_directories(${ODE_INCLUDE_DIRS} ${CCD_INCLUDE_DIRS})
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lstdc++")
    set(LOVR_ODE ode ccd)
  else()
    if(EMSCRIPTEN)
      set(ODE_BUILD_SHARED OFF CACHE BOOL "")
    else()
      set(ODE_BUILD_SHARED ON CACHE BOOL "")
    endif()
    add_subdirectory(deps/ode ode)
    if(NOT WIN32)
      set_target_properties(ode PROPERTIES COMPILE_FLAGS "-Wno-unused-volatile-lvalue -Wno-array-bounds -Wno-undefined-var-template")
    endif()
    include_directories(deps/ode/include "${CMAKE_CURRENT_BINARY_DIR}/ode/include")
    set(LOVR_ODE ode)
  endif()
endif()

# OpenAL
if(LOVR_ENABLE_AUDIO)
  if(LOVR_SYSTEM_OPENAL)
    pkg_search_module(OPENAL openal-soft)
    if (NOT OPENAL_FOUND)
      pkg_search_module(OPENAL openal)
      if (NOT OPENAL_FOUND)
        message(FATAL_ERROR "OpenAL not found.")
      endif()
    endif()
    include_directories(${OPENAL_INCLUDE_DIRS})
    string(REPLACE ";" " " OPENAL_LDFLAGS_STR "${OPENAL_LDFLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OPENAL_LDFLAGS_STR}")
    set(LOVR_OPENAL ${OPENAL_LIBRARIES})
  else()
    set(ALSOFT_UTILS OFF CACHE BOOL "")
    set(ALSOFT_EXAMPLES OFF CACHE BOOL "")
    set(ALSOFT_TESTS OFF CACHE BOOL "")
    if(NOT EMSCRIPTEN)
      add_subdirectory(deps/openal-soft openal)
      set(LOVR_OPENAL OpenAL)
      if(WIN32)
        set_target_properties(OpenAL PROPERTIES COMPILE_FLAGS "/wd4005 /wd4098")
      endif()
    endif()
    include_directories(deps/openal-soft/include)
  endif()
endif()

# OpenGL
if(NOT (WIN32 OR EMSCRIPTEN OR ANDROID))
  find_package(OpenGL REQUIRED)
  include_directories(${OPENGL_INCLUDE_DIRS})
  set(LOVR_OPENGL ${OPENGL_LIBRARIES})
endif()

# OpenVR
if(LOVR_ENABLE_HEADSET AND LOVR_USE_OPENVR)
  set(BUILD_SHARED ON CACHE BOOL "")
  set(BUILD_UNIVERSAL OFF CACHE BOOL "")
  include_directories(deps/openvr/headers)
  include_directories(deps/openvr/src)
  include_directories(deps/openvr/src/vrcommon)
  if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(LOVR_OPENVR openvr_api64)
  else()
    set(LOVR_OPENVR openvr_api)
  endif()
  add_subdirectory(deps/openvr)
  set_target_properties(${LOVR_OPENVR} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/openvr_api"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/openvr_api"
  )
endif()

# Leap Motion
if(LOVR_ENABLE_HEADSET AND LOVR_USE_LEAP)
  if(NOT LOVR_LEAP_PATH)
    message(FATAL_ERROR "LOVR_USE_LEAP requires the LOVR_LEAP_PATH to be set to the location of the Leap Motion SDK (LeapSDK) folder")
  endif()
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(LEAP_ARCH "x64")
  else()
    set(LEAP_ARCH "x86")
  endif()
  add_library(LeapC SHARED IMPORTED)
  include_directories("${LOVR_LEAP_PATH}/include")
  set_target_properties(LeapC PROPERTIES IMPORTED_IMPLIB "${LOVR_LEAP_PATH}/lib/${LEAP_ARCH}/LeapC.lib")
  set_target_properties(LeapC PROPERTIES IMPORTED_LOCATION "${LOVR_LEAP_PATH}/lib/${LEAP_ARCH}/LeapC.dll")
  set(LOVR_LEAP LeapC)
endif()

# Oculus SDK -- expects Oculus SDK 1.26.0 or later
if(LOVR_ENABLE_HEADSET AND LOVR_USE_OCULUS)
  if(NOT LOVR_OCULUS_PATH)
    message(FATAL_ERROR "LOVR_USE_OCULUS requires the LOVR_OCULUS_PATH to be set to the location of the Oculus Desktop SDK (LibOVR) folder")
  endif()
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(OCULUS_BUILD_TYPE "Release")
  else()
    set(OCULUS_BUILD_TYPE "Debug")
  endif()
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(OCULUS_ARCH "x64")
  else()
    set(OCULUS_ARCH "Win32")
  endif()
  include_directories("${LOVR_OCULUS_PATH}/LibOVR/Include")
  link_directories("${LOVR_OCULUS_PATH}/LibOVR/Lib/Windows/${OCULUS_ARCH}/${OCULUS_BUILD_TYPE}/VS2017")
  set(LOVR_OCULUS LibOVR)
endif()

# pthreads
if(LOVR_ENABLE_THREAD)
  if(NOT WIN32 AND NOT EMSCRIPTEN)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    set(LOVR_PTHREADS Threads::Threads)
  endif()
endif()

# LÖVR

set(LOVR_SRC
  src/main.c
  src/core/luax.c
  src/core/maf.c
  src/core/platform.c
  src/core/types.c
  src/core/utf.c
  src/core/util.c
  src/api/l_lovr.c
  src/lib/map/map.c
  src/lib/vec/vec.c
  src/lib/sds/sds.c
)

if (LOVR_BUILD_SHARED)
  add_library(lovr SHARED ${LOVR_SRC})
else()
  add_executable(lovr ${LOVR_SRC})
endif()

set_target_properties(lovr PROPERTIES C_VISIBILITY_PRESET default)
set_target_properties(lovr PROPERTIES C_STANDARD 99)
target_include_directories(lovr PRIVATE src src/core src/modules)
target_link_libraries(lovr
  ${LOVR_ENET}
  ${LOVR_GLFW}
  ${LOVR_LUA}
  ${LOVR_MSDF}
  ${LOVR_ODE}
  ${LOVR_OPENAL}
  ${LOVR_OPENGL}
  ${LOVR_OPENVR}
  ${LOVR_OCULUS}
  ${LOVR_LEAP}
  ${LOVR_PHYSFS}
  ${LOVR_PTHREADS}
  ${LOVR_EMSCRIPTEN_FLAGS}
)

if(LOVR_USE_LUAJIT)
  target_compile_definitions(lovr PUBLIC -DLOVR_USE_LUAJIT)
endif()

if(LOVR_ENABLE_AUDIO)
  add_definitions(-DLOVR_ENABLE_AUDIO)
  target_sources(lovr PRIVATE
    src/modules/audio/audio.c
    src/modules/audio/source.c
    src/modules/audio/microphone.c
    src/api/l_audio.c
    src/api/l_source.c
    src/api/l_microphone.c
  )
endif()

if(LOVR_ENABLE_DATA)
  add_definitions(-DLOVR_ENABLE_DATA)
  target_sources(lovr PRIVATE
    src/modules/data/audioStream.c
    src/modules/data/blob.c
    src/modules/data/modelData.c
    src/modules/data/modelData_gltf.c
    src/modules/data/modelData_obj.c
    src/modules/data/rasterizer.c
    src/modules/data/soundData.c
    src/modules/data/textureData.c
    src/api/l_data.c
    src/api/l_audioStream.c
    src/api/l_blob.c
    src/api/l_modelData.c
    src/api/l_rasterizer.c
    src/api/l_soundData.c
    src/api/l_textureData.c
    src/lib/stb/stb_image.c
    src/lib/stb/stb_image_write.c
    src/lib/stb/stb_truetype.c
    src/lib/stb/stb_vorbis.c
    src/lib/jsmn/jsmn.c
  )
endif()

if(LOVR_ENABLE_EVENT)
  add_definitions(-DLOVR_ENABLE_EVENT)
  target_sources(lovr PRIVATE
    src/modules/event/event.c
    src/api/l_event.c
  )
endif()

if(LOVR_ENABLE_FILESYSTEM)
  add_definitions(-DLOVR_ENABLE_FILESYSTEM)
  target_sources(lovr PRIVATE
    src/modules/filesystem/file.c
    src/modules/filesystem/filesystem.c
    src/api/l_filesystem.c
  )
endif()

if(LOVR_ENABLE_GRAPHICS)
  add_definitions(-DLOVR_ENABLE_GRAPHICS)
  target_sources(lovr PRIVATE
    src/modules/graphics/animator.c
    src/modules/graphics/buffer.c
    src/modules/graphics/canvas.c
    src/modules/graphics/font.c
    src/modules/graphics/graphics.c
    src/modules/graphics/material.c
    src/modules/graphics/mesh.c
    src/modules/graphics/model.c
    src/modules/graphics/opengl.c
    src/modules/graphics/shader.c
    src/modules/graphics/texture.c
    src/api/l_graphics.c
    src/api/l_animator.c
    src/api/l_canvas.c
    src/api/l_font.c
    src/api/l_material.c
    src/api/l_mesh.c
    src/api/l_model.c
    src/api/l_shader.c
    src/api/l_shaderBlock.c
    src/api/l_texture.c
    src/resources/shaders.c
    src/lib/glad/glad.c
  )
endif()

if(LOVR_ENABLE_HEADSET)
  add_definitions(-DLOVR_ENABLE_HEADSET)
  target_sources(lovr PRIVATE
    src/api/l_headset.c
    src/modules/headset/headset.c
  )

  if(LOVR_USE_OPENVR)
    add_definitions(-DLOVR_USE_OPENVR)
    target_sources(lovr PRIVATE src/modules/headset/openvr.c)
  endif()
  if(LOVR_USE_OCULUS)
    add_definitions(-DLOVR_USE_OCULUS)
    target_sources(lovr PRIVATE src/modules/headset/oculus.c)
  endif()
  if(LOVR_USE_OCULUS_MOBILE)
    add_definitions(-DLOVR_USE_OCULUS_MOBILE)
    target_sources(lovr PRIVATE src/modules/headset/oculus_mobile.c)
  endif()
  if(LOVR_USE_WEBVR)
    add_definitions(-DLOVR_USE_WEBVR)
    target_sources(lovr PRIVATE src/modules/headset/webvr.c)
  endif()
  if(LOVR_USE_DESKTOP_HEADSET)
    add_definitions(-DLOVR_USE_DESKTOP_HEADSET)
    target_sources(lovr PRIVATE src/modules/headset/desktop.c)
  endif()
  if(LOVR_USE_LEAP)
    add_definitions(-DLOVR_USE_LEAP)
    target_sources(lovr PRIVATE src/modules/headset/leap.c)
  endif()
endif()

if(LOVR_ENABLE_MATH)
  add_definitions(-DLOVR_ENABLE_MATH)
  target_sources(lovr PRIVATE
    src/modules/math/math.c
    src/modules/math/curve.c
    src/modules/math/pool.c
    src/modules/math/randomGenerator.c
    src/api/l_math.c
    src/api/l_curve.c
    src/api/l_pool.c
    src/api/l_randomGenerator.c
    src/api/l_vec3.c
    src/api/l_quat.c
    src/api/l_mat4.c
    src/lib/noise1234/noise1234.c
  )

  if(LOVR_USE_SSE)
    add_definitions(-DLOVR_USE_SSE)
  endif()
endif()

if(LOVR_ENABLE_PHYSICS)
  add_definitions(-DLOVR_ENABLE_PHYSICS)
  target_sources(lovr PRIVATE
    src/modules/physics/physics.c
    src/api/l_physics.c
    src/api/l_collider.c
    src/api/l_joints.c
    src/api/l_shapes.c
    src/api/l_world.c
  )
endif()

if(LOVR_ENABLE_THREAD)
  add_definitions(-DLOVR_ENABLE_THREAD)
  target_sources(lovr PRIVATE
    src/modules/thread/channel.c
    src/modules/thread/thread.c
    src/api/l_thread_module.c
    src/api/l_channel.c
    src/api/l_thread.c
    src/lib/tinycthread/tinycthread.c
  )
endif()

if(LOVR_ENABLE_TIMER)
  add_definitions(-DLOVR_ENABLE_TIMER)
  target_sources(lovr PRIVATE src/modules/timer/timer.c src/api/l_timer.c)
endif()

if(LOVR_ENABLE_ENET)
  add_definitions(-DLOVR_ENABLE_ENET)
  target_sources(lovr PRIVATE src/lib/lua-enet/enet.c)
endif()

if(LOVR_ENABLE_JSON)
  add_definitions(-DLOVR_ENABLE_JSON)
  target_sources(lovr PRIVATE
    src/lib/lua-cjson/fpconv.c
    src/lib/lua-cjson/lua_cjson.c
    src/lib/lua-cjson/strbuf.c
  )
endif()

# Platforms
if(WIN32)
  set_target_properties(lovr PROPERTIES COMPILE_FLAGS "/wd4244")
  set_target_properties(lovr PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
  set_target_properties(lovr PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
  target_compile_definitions(lovr PUBLIC -D_CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(lovr PUBLIC -D_CRT_NONSTDC_NO_WARNINGS)
  target_compile_definitions(lovr PUBLIC -D_USE_MATH_DEFINES)

  if(MSVC_VERSION VERSION_LESS 1900)
    target_compile_definitions(lovr PUBLIC -Dinline=__inline -Dsnprintf=_snprintf)
  endif()

  function(move_dll)
    if(TARGET ${ARGV0})
      add_custom_command(TARGET lovr POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${ARGV0}>
        ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/$<TARGET_FILE_NAME:${ARGV0}>
      )
    endif()
  endfunction()

  move_dll(${LOVR_GLFW})
  move_dll(${LOVR_LUA})
  move_dll(${LOVR_ODE})
  move_dll(${LOVR_OPENAL})
  move_dll(${LOVR_OPENVR})
  move_dll(${LOVR_PHYSFS})
  move_dll(${LOVR_LEAP})
  target_compile_definitions(lovr PRIVATE -DLOVR_GL)
elseif(APPLE)
  target_compile_definitions(lovr PRIVATE -DLOVR_GL)
elseif(EMSCRIPTEN)
  target_compile_definitions(lovr PRIVATE -DLOVR_WEBGL)
elseif(ANDROID)
  target_link_libraries(lovr log EGL GLESv3)
  target_compile_definitions(lovr PRIVATE -DLOVR_GLES)
elseif(UNIX)
  target_compile_definitions(lovr PRIVATE -DLOVR_GL)
endif()
