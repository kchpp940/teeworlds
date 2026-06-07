cmake_minimum_required(VERSION 3.12)

########################################################################
# BUILD PROFILES - Unified build configuration system
#
# This module centralizes all build configuration:
#   - Build options (client/server/tools/headless)
#   - Build types (debug/release/relwithdebinfo)
#   - Compile definitions (assert/log/debug macros)
#   - Compile options (warnings, optimizations, security flags)
#   - Output directories
#   - Target profiles (dep/own/client/server/tool/test)
#
# Usage:
#   tw_apply_profile(target PROFILE <DEP|OWN|CLIENT|SERVER|TOOL|TEST>)
#
# Note: find_package calls must complete BEFORE tw_apply_profile() is
# called on any target, since the function reads *_FOUND, *_LIBRARIES,
# *_INCLUDE_DIRS variables directly.
########################################################################

########################################################################
# DECLARE BUILD OPTIONS
########################################################################

set(AUTO_DEPENDENCIES_DEFAULT OFF)
if(TARGET_OS STREQUAL "windows")
  set(AUTO_DEPENDENCIES_DEFAULT ON)
endif()

option(CLIENT "Compile client" ON)
option(HEADLESS_CLIENT "Build the client without graphics" OFF)
option(SERVER "Compile server" ON)
option(TOOLS "Compile tools (mastersrv, versionsrv, map tools)" OFF)
option(DOWNLOAD_DEPENDENCIES "Download dependencies (only available on Windows)" ${AUTO_DEPENDENCIES_DEFAULT})
option(DOWNLOAD_GTEST "Download and compile GTest if not found" ${AUTO_DEPENDENCIES_DEFAULT})
option(PREFER_BUNDLED_LIBS "Prefer bundled libraries over system libraries" ${AUTO_DEPENDENCIES_DEFAULT})
option(DEV "Don't generate stuff necessary for packaging" OFF)

########################################################################
# BUILD TYPE SETUP
########################################################################

set(OpenGL_GL_PREFERENCE GLVND)

if(NOT(CMAKE_BUILD_TYPE))
  if(NOT(DEV))
    set(CMAKE_BUILD_TYPE Release)
  else()
    set(CMAKE_BUILD_TYPE Debug)
  endif()
endif()

set(DBG $<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>)

########################################################################
# COMPILER FLAG DETECTION
########################################################################

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

function(add_c_compiler_flag_if_supported VARIABLE FLAG)
  if(ARGC GREATER 2)
    set(CHECKED_FLAG "${ARGV2}")
  else()
    set(CHECKED_FLAG "${FLAG}")
  endif()
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" CONFIG_VARIABLE "FLAG_SUPPORTED${CHECKED_FLAG}")
  check_c_compiler_flag("${CHECKED_FLAG}" ${CONFIG_VARIABLE})
  if(${CONFIG_VARIABLE})
    if(${VARIABLE})
      set("${VARIABLE}" "${${VARIABLE}};${FLAG}" PARENT_SCOPE)
    else()
      set("${VARIABLE}" "${FLAG}" PARENT_SCOPE)
    endif()
  endif()
endfunction()

set(TW_FLAGS_ALL)
set(TW_FLAGS_OWN)
set(TW_FLAGS_DEP)

if(NOT MSVC)
  add_c_compiler_flag_if_supported(TW_FLAGS_ALL -fstack-protector-all)
  add_c_compiler_flag_if_supported(TW_FLAGS_ALL -fcf-protection)

  if(TARGET_ARCH STREQUAL "x86")
    add_c_compiler_flag_if_supported(TW_FLAGS_ALL -ffloat-store)
  endif()

  if(TARGET_ARCH STREQUAL "x86")
    check_c_source_compiles("#include <immintrin.h>\nint main() { _mm_pause(); return 0; }" MM_PAUSE_WORKS_WITHOUT_MSSE2)
    if(NOT MM_PAUSE_WORKS_WITHOUT_MSSE2)
      add_c_compiler_flag_if_supported(TW_FLAGS_ALL -msse2)
    endif()
  endif()

  if(TARGET_OS STREQUAL "mac")
    add_c_compiler_flag_if_supported(TW_FLAGS_ALL -stdlib=libc++)
    add_c_compiler_flag_if_supported(TW_FLAGS_ALL -mmacosx-version-min=10.7)
  endif()

  add_c_compiler_flag_if_supported(TW_FLAGS_OWN -Wall)
  if(CMAKE_VERSION VERSION_GREATER 3.3 OR CMAKE_VERSION VERSION_EQUAL 3.3)
    add_c_compiler_flag_if_supported(TW_FLAGS_OWN
      $<$<COMPILE_LANGUAGE:C>:-Wdeclaration-after-statement>
      -Wdeclaration-after-statement
    )
  endif()
  add_c_compiler_flag_if_supported(TW_FLAGS_OWN -Wextra)
  add_c_compiler_flag_if_supported(TW_FLAGS_OWN -Wno-unused-parameter)
  add_c_compiler_flag_if_supported(TW_FLAGS_OWN -Wno-missing-field-initializers)
  add_c_compiler_flag_if_supported(TW_FLAGS_OWN -Wformat=2)
  add_c_compiler_flag_if_supported(TW_FLAGS_DEP -Wno-implicit-function-declaration)
endif()

if(NOT MSVC)
  check_c_compiler_flag("-O2;-Wp,-Werror;-D_FORTIFY_SOURCE=2" TW_DEFINE_FORTIFY_SOURCE)
endif()

########################################################################
# OUTPUT DIRECTORY CONFIGURATION
########################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER ${CONFIG} CONFIG_UPPER)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${PROJECT_BINARY_DIR})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${PROJECT_BINARY_DIR})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${PROJECT_BINARY_DIR})
endforeach()

########################################################################
# VALID PROFILES
########################################################################

set(TW_PROFILES DEP OWN CLIENT SERVER TOOL TEST)

########################################################################
# DYNAMIC DEFINES (resolved after find_package, via tw_apply_dynamic_defines)
########################################################################

set(TW_EXTRA_OWN_INCLUDES)
set(TW_EXTRA_OWN_DEFINES)

function(tw_apply_dynamic_defines)
  if(CRYPTO_FOUND)
    set(TW_EXTRA_OWN_DEFINES CONF_OPENSSL PARENT_SCOPE)
    set(TW_EXTRA_OWN_INCLUDES ${CRYPTO_INCLUDE_DIRS} PARENT_SCOPE)
  endif()
endfunction()

########################################################################
# MAIN PROFILE APPLICATION FUNCTION
########################################################################

function(tw_apply_profile TARGET)
  cmake_parse_arguments(PROFILE "" "PROFILE" "" ${ARGN})

  if(NOT PROFILE_PROFILE)
    message(FATAL_ERROR "tw_apply_profile: PROFILE argument is required (one of ${TW_PROFILES})")
  endif()

  string(TOUPPER ${PROFILE_PROFILE} PROFILE_UPPER)
  if(NOT PROFILE_UPPER IN_LIST TW_PROFILES)
    message(FATAL_ERROR "tw_apply_profile: Unknown profile '${PROFILE_PROFILE}'. Valid profiles: ${TW_PROFILES}")
  endif()

  set(IS_DEP FALSE)
  if(PROFILE_UPPER STREQUAL "DEP")
    set(IS_DEP TRUE)
  endif()

  set(OWN_BASE_INCLUDES
    ${PROJECT_BINARY_DIR}/src
    src
    ${ZLIB_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${TW_EXTRA_OWN_INCLUDES}
  )

  set(OWN_BASE_DEFINES
    $<$<CONFIG:Debug>:CONF_DEBUG>
    _GLIBCXX_ASSERTIONS
    ${TW_EXTRA_OWN_DEFINES}
  )

  if(TW_DEFINE_FORTIFY_SOURCE)
    list(APPEND OWN_BASE_DEFINES $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>)
  endif()

  if(TARGET_OS STREQUAL "windows")
    list(APPEND OWN_BASE_DEFINES
      _WIN32_WINNT=0x0501
      UNICODE
      _UNICODE
    )
  endif()

  if(HEADLESS_CLIENT)
    list(APPEND OWN_BASE_DEFINES CONF_HEADLESS_CLIENT)
  endif()

  if(NOT MSVC)
    if(IS_DEP)
      if(TW_FLAGS_ALL)
        target_compile_options(${TARGET} PRIVATE ${TW_FLAGS_ALL})
      endif()
      if(TW_FLAGS_DEP)
        target_compile_options(${TARGET} PRIVATE ${TW_FLAGS_DEP})
      endif()
    else()
      if(TW_FLAGS_ALL)
        target_compile_options(${TARGET} PRIVATE ${TW_FLAGS_ALL})
      endif()
      if(TW_FLAGS_OWN)
        target_compile_options(${TARGET} PRIVATE ${TW_FLAGS_OWN})
      endif()
      target_compile_definitions(${TARGET} PRIVATE ${OWN_BASE_DEFINES})
      target_include_directories(${TARGET} PRIVATE ${OWN_BASE_INCLUDES})
    endif()
  else()
    if(POLICY CMP0091)
      set_property(TARGET ${TARGET} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<${DBG}:Debug>")
    else()
      target_compile_options(${TARGET} PRIVATE $<$<NOT:${DBG}>:/MT> $<${DBG}:/MTd>)
    endif()

    target_compile_options(${TARGET} PRIVATE /MP /EHsc /GS /utf-8)

    if(IS_DEP)
      target_compile_options(${TARGET} PRIVATE /w /W0)
      if(TW_FLAGS_ALL)
        target_compile_options(${TARGET} PRIVATE ${TW_FLAGS_ALL})
      endif()
      if(TW_FLAGS_DEP)
        target_compile_options(${TARGET} PRIVATE ${TW_FLAGS_DEP})
      endif()
    else()
      target_compile_options(${TARGET} PRIVATE /W3 /wd4244 /wd4267 /wd4800 /wd4996)
      if(TW_FLAGS_ALL)
        target_compile_options(${TARGET} PRIVATE ${TW_FLAGS_ALL})
      endif()
      if(TW_FLAGS_OWN)
        target_compile_options(${TARGET} PRIVATE ${TW_FLAGS_OWN})
      endif()
      target_compile_definitions(${TARGET} PRIVATE ${OWN_BASE_DEFINES})
      target_include_directories(${TARGET} PRIVATE ${OWN_BASE_INCLUDES})
    endif()
  endif()

  if(NOT IS_DEP)
    if(PROFILE_UPPER STREQUAL "CLIENT")
      if(FREETYPE_INCLUDE_DIRS)
        target_include_directories(${TARGET} PRIVATE ${FREETYPE_INCLUDE_DIRS})
      endif()
      if(PNGLITE_INCLUDE_DIRS)
        target_include_directories(${TARGET} PRIVATE ${PNGLITE_INCLUDE_DIRS})
      endif()
      if(SDL2_INCLUDE_DIRS)
        target_include_directories(${TARGET} PRIVATE ${SDL2_INCLUDE_DIRS})
      endif()
      if(WAVPACK_INCLUDE_DIRS)
        target_include_directories(${TARGET} PRIVATE ${WAVPACK_INCLUDE_DIRS})
      endif()
      if(PLATFORM_CLIENT_INCLUDE_DIRS)
        target_include_directories(${TARGET} PRIVATE ${PLATFORM_CLIENT_INCLUDE_DIRS})
      endif()
    elseif(PROFILE_UPPER STREQUAL "TEST")
      if(GTEST_INCLUDE_DIRS)
        target_include_directories(${TARGET} PRIVATE ${GTEST_INCLUDE_DIRS})
      endif()
    endif()
  endif()

  get_target_property(TARGET_TYPE ${TARGET} TYPE)
  if(TARGET_TYPE STREQUAL "EXECUTABLE" OR TARGET_TYPE STREQUAL "SHARED_LIBRARY" OR TARGET_TYPE STREQUAL "MODULE_LIBRARY")
    if(MSVC)
      set_property(TARGET ${TARGET} APPEND PROPERTY LINK_FLAGS /SAFESEH:NO)
    endif()
    if(TARGET_OS STREQUAL "mac")
      target_link_libraries(${TARGET} -stdlib=libc++)
      target_link_libraries(${TARGET} -mmacosx-version-min=10.7)
    endif()
    if((MINGW OR TARGET_OS STREQUAL "linux") AND PREFER_BUNDLED_LIBS)
      target_link_libraries(${TARGET} -static-libgcc)
      target_link_libraries(${TARGET} -static-libstdc++)
      if(MINGW)
        target_link_libraries(${TARGET} -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic)
      endif()
    endif()
  endif()
endfunction()

########################################################################
# CLIENT-SPECIFIC DYNAMIC CONFIGURATION (post-find)
########################################################################

include(CheckSymbolExists)

function(tw_apply_client_wavpack_config TARGET)
  set(PARAMS "${WAVPACK_INCLUDE_DIRS};${WAVPACK_INCLUDE_DIRS}")
  if(NOT(WAVPACK_OPEN_FILE_INPUT_EX_PARAMS STREQUAL PARAMS))
    unset(WAVPACK_OPEN_FILE_INPUT_EX CACHE)
  endif()
  set(WAVPACK_OPEN_FILE_INPUT_EX_PARAMS "${PARAMS}" CACHE INTERNAL "")

  set(CMAKE_REQUIRED_INCLUDES ${ORIGINAL_CMAKE_REQUIRED_INCLUDES} ${WAVPACK_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_LIBRARIES ${ORIGINAL_CMAKE_REQUIRED_LIBRARIES} ${WAVPACK_LIBRARIES})
  check_symbol_exists(WavpackOpenFileInputEx wavpack.h WAVPACK_OPEN_FILE_INPUT_EX)
  set(CMAKE_REQUIRED_INCLUDES ${ORIGINAL_CMAKE_REQUIRED_INCLUDES})
  set(CMAKE_REQUIRED_LIBRARIES ${ORIGINAL_CMAKE_REQUIRED_LIBRARIES})

  if(WAVPACK_OPEN_FILE_INPUT_EX)
    target_compile_definitions(${TARGET} PRIVATE CONF_WAVPACK_OPEN_FILE_INPUT_EX)
  endif()
endfunction()

########################################################################
# INSTALL / PACKAGE UNIFIED CONFIGURATION
########################################################################

function(tw_setup_install)
  set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME ${PROJECT_NAME} PARENT_SCOPE)

  set(CPACK_PACKAGE_NAME ${PROJECT_NAME} PARENT_SCOPE)
  set(CPACK_GENERATOR TGZ TXZ PARENT_SCOPE)
  set(CPACK_ARCHIVE_COMPONENT_INSTALL ON PARENT_SCOPE)
  set(CPACK_STRIP_FILES TRUE PARENT_SCOPE)
  set(CPACK_COMPONENTS_ALL portable PARENT_SCOPE)
  set(CPACK_SOURCE_GENERATOR ZIP TGZ TBZ2 TXZ PARENT_SCOPE)
  set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR} PARENT_SCOPE)
  set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR} PARENT_SCOPE)
  set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH} PARENT_SCOPE)
  set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} PARENT_SCOPE)
  set(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME} PARENT_SCOPE)

  if(TARGET_OS AND TARGET_BITS)
    if(TARGET_OS STREQUAL "windows")
      set(CPACK_SYSTEM_NAME "win${TARGET_BITS}" PARENT_SCOPE)
      set(CPACK_GENERATOR ZIP PARENT_SCOPE)
    elseif(TARGET_OS STREQUAL "linux")
      if(TARGET_BITS EQUAL 32)
        set(CPACK_SYSTEM_NAME "linux_x86" PARENT_SCOPE)
      elseif(TARGET_BITS EQUAL 64)
        set(CPACK_SYSTEM_NAME "linux_x86_64" PARENT_SCOPE)
      endif()
    elseif(TARGET_OS STREQUAL "mac")
      set(CPACK_SYSTEM_NAME "macos" PARENT_SCOPE)
      set(CPACK_GENERATOR DMG PARENT_SCOPE)
    endif()
  endif()
endfunction()

function(tw_apply_install_rules)
  cmake_parse_arguments(INST "" "" "TARGETS;DATA_DIRS;EXTRA_FILES" ${ARGN})

  if(DEV)
    return()
  endif()

  if(INST_DATA_DIRS)
    install(DIRECTORY ${INST_DATA_DIRS}
      DESTINATION share/${PROJECT_NAME}
      COMPONENT data
    )
  endif()

  foreach(T ${INST_TARGETS})
    if(TARGET ${T})
      get_target_property(_TYPE ${T} TYPE)
      if(_TYPE STREQUAL "EXECUTABLE")
        install(TARGETS ${T} DESTINATION bin COMPONENT ${T})
      endif()
    endif()
  endforeach()

  if(CMAKE_VERSION VERSION_LESS 3.6 OR CMAKE_VERSION VERSION_EQUAL 3.6)
    message(WARNING "Cannot create CPack targets, CMake version too old. Use CMake 3.6 or newer.")
    return()
  endif()

  set(CPACK_PACKAGE_FILE_NAME
    ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME})
  set(CPACK_ARCHIVE_PORTABLE_FILE_NAME ${CPACK_PACKAGE_FILE_NAME})
  set(CPACK_SOURCE_PACKAGE_FILE_NAME
    ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-src)

  set(EXTRA_ARGS DESTINATION ${CPACK_PACKAGE_FILE_NAME} COMPONENT portable EXCLUDE_FROM_ALL)

  foreach(T ${INST_TARGETS})
    if(TARGET ${T})
      get_target_property(_TYPE ${T} TYPE)
      if(_TYPE STREQUAL "EXECUTABLE")
        install(TARGETS ${T} ${EXTRA_ARGS})
      endif()
    endif()
  endforeach()

  if(INST_DATA_DIRS)
    install(DIRECTORY ${INST_DATA_DIRS} ${EXTRA_ARGS})
  endif()

  if(INST_EXTRA_FILES)
    install(FILES ${INST_EXTRA_FILES} ${EXTRA_ARGS})
  endif()
endfunction()

########################################################################
# MACRO HELPERS - unified assert/log/debug configuration entry points
########################################################################

function(tw_apply_assert_config TARGET)
  target_compile_definitions(${TARGET} PRIVATE _GLIBCXX_ASSERTIONS)
endfunction()

function(tw_apply_debug_config TARGET)
  target_compile_definitions(${TARGET} PRIVATE
    $<$<CONFIG:Debug>:CONF_DEBUG>
  )
  if(TW_DEFINE_FORTIFY_SOURCE)
    target_compile_definitions(${TARGET} PRIVATE
      $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>
    )
  endif()
endfunction()

function(tw_apply_platform_defines TARGET)
  if(TARGET_OS STREQUAL "windows")
    target_compile_definitions(${TARGET} PRIVATE
      _WIN32_WINNT=0x0501
      UNICODE
      _UNICODE
    )
  endif()
  if(HEADLESS_CLIENT)
    target_compile_definitions(${TARGET} PRIVATE CONF_HEADLESS_CLIENT)
  endif()
endfunction()
