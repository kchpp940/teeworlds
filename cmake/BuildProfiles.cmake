cmake_minimum_required(VERSION 3.12)

########################################################################
# BUILD PROFILES - Table-driven, declarative build configuration system
#
# Design:
#   1. Profiles are DECLARED once with tw_define_profile()
#   2. Targets are CREATED with tw_add_executable() / tw_add_library()
#      specifying a single PROFILE name
#   3. All compile flags, includes, defines, link options, install rules,
#      output directories are applied AUTOMATICALLY from the profile table
#
# Profile hierarchy:
#   DEP      (3rd-party bundled libraries)
#   OWN      (our own code, inherits base)
#     ├─ CLIENT
#     ├─ SERVER
#     ├─ TOOL
#     └─ TEST
#
# Usage:
#   # Declare a new profile (only once)
#   tw_define_profile(MYTOOL
#     INHERITS OWN
#     COMPILE_DEFINITIONS MY_TOOL=1
#     INCLUDE_DIRECTORIES src/mytool
#     INSTALL_DESTINATION bin
#     INSTALL_COMPONENT mytool
#   )
#
#   # Create a target with that profile (one call does everything)
#   tw_add_executable(mytool PROFILE MYTOOL src/mytool/main.cpp)
########################################################################

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckSymbolExists)

########################################################################
# GLOBAL STATE
########################################################################

set(TW_PROFILES "" CACHE INTERNAL "List of all registered profiles")
set(TW_TARGETS "" CACHE INTERNAL "List of all targets created via tw_add_*")
set(TW_TARGET_PROFILE "" CACHE INTERNAL "Map: target -> profile name")

set(TARGETS_OWN "" CACHE INTERNAL "List of all OWN-code targets (for 'everything' target)")
set(TARGETS_DEP "" CACHE INTERNAL "List of all 3rd-party dep targets")
set(TARGETS_LINK "" CACHE INTERNAL "List of all targets with a linking stage (executables/shared libs)")
set(TW_INSTALL_TARGETS "" CACHE INTERNAL "List of all targets eligible for install/package")

########################################################################
# BUILD OPTIONS (centralized)
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
# BUILD TYPE
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
# COMPILER FLAG DETECTION (done once at module load)
########################################################################

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
# OUTPUT DIRECTORIES (applied globally, once)
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
# PROFILE REGISTRY: tw_define_profile + tw_get_profile_field
########################################################################

function(tw_define_profile NAME)
  cmake_parse_arguments(PROF "" "INHERITS;INSTALL_DESTINATION;INSTALL_COMPONENT;MSVC_RUNTIME"
    "COMPILE_DEFINITIONS;INCLUDE_DIRECTORIES;COMPILE_OPTIONS;LINK_OPTIONS;LINK_LIBRARIES;MSVC_OPTIONS" ${ARGN})

  string(TOUPPER ${NAME} NAME_UPPER)
  list(APPEND TW_PROFILES ${NAME_UPPER})
  set(TW_PROFILES ${TW_PROFILES} CACHE INTERNAL "List of all registered profiles")

  foreach(FIELD INHERITS COMPILE_DEFINITIONS INCLUDE_DIRECTORIES COMPILE_OPTIONS LINK_OPTIONS LINK_LIBRARIES MSVC_OPTIONS INSTALL_DESTINATION INSTALL_COMPONENT MSVC_RUNTIME)
    if(PROF_${FIELD})
      set(TW_PROFILE_${NAME_UPPER}_${FIELD} ${PROF_${FIELD}} CACHE INTERNAL "Profile ${NAME_UPPER} field ${FIELD}")
    endif()
  endforeach()

  set(TW_PROFILE_${NAME_UPPER}_DEFINED TRUE CACHE INTERNAL "Profile ${NAME_UPPER} is registered")
endfunction()

function(tw_get_profile_field OUTPUT_VAR PROFILE_NAME FIELD)
  string(TOUPPER ${PROFILE_NAME} NAME_UPPER)
  if(NOT TW_PROFILE_${NAME_UPPER}_DEFINED)
    message(FATAL_ERROR "tw_get_profile_field: Unknown profile '${PROFILE_NAME}'. Registered: ${TW_PROFILES}")
  endif()

  set(RESULT "")
  set(INHERITS ${TW_PROFILE_${NAME_UPPER}_INHERITS})
  if(INHERITS)
    tw_get_profile_field(PARENT_RESULT ${INHERITS} ${FIELD})
    list(APPEND RESULT ${PARENT_RESULT})
  endif()

  if(TW_PROFILE_${NAME_UPPER}_${FIELD})
    list(APPEND RESULT ${TW_PROFILE_${NAME_UPPER}_${FIELD}})
  endif()

  set(${OUTPUT_VAR} ${RESULT} PARENT_SCOPE)
endfunction()

########################################################################
# PROFILE DEFINITIONS (the single source of truth)
########################################################################

# ---- Base profile: DEP (3rd-party bundled libraries) ----
tw_define_profile(DEP
  COMPILE_OPTIONS ${TW_FLAGS_ALL} ${TW_FLAGS_DEP}
  MSVC_RUNTIME "MultiThreaded$<${DBG}:Debug>"
  MSVC_OPTIONS /MP /EHsc /GS /utf-8 /w /W0
)

# ---- Base profile: OWN (our own code) ----
tw_define_profile(OWN
  INHERITS DEP
  COMPILE_DEFINITIONS
    $<$<CONFIG:Debug>:CONF_DEBUG>
    _GLIBCXX_ASSERTIONS
    $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>
  COMPILE_OPTIONS ${TW_FLAGS_OWN}
  MSVC_OPTIONS /W3 /wd4244 /wd4267 /wd4800 /wd4996
  INCLUDE_DIRECTORIES
    ${PROJECT_BINARY_DIR}/src
    src
)

# ---- CLIENT profile ----
tw_define_profile(CLIENT
  INHERITS OWN
  INSTALL_DESTINATION bin
  INSTALL_COMPONENT client
)

# ---- SERVER profile ----
tw_define_profile(SERVER
  INHERITS OWN
  INSTALL_DESTINATION bin
  INSTALL_COMPONENT server
)

# ---- TOOL profile (mastersrv, versionsrv, map tools) ----
tw_define_profile(TOOL
  INHERITS OWN
  INSTALL_DESTINATION bin
  INSTALL_COMPONENT tools
)

# ---- TEST profile ----
tw_define_profile(TEST
  INHERITS OWN
)

########################################################################
# DYNAMIC DEFINES (resolved after find_package)
########################################################################

function(tw_apply_dynamic_defines)
  if(CRYPTO_FOUND)
    tw_get_profile_field(EXISTING_DEFS OWN COMPILE_DEFINITIONS)
    list(APPEND EXISTING_DEFS CONF_OPENSSL)
    set(TW_PROFILE_OWN_COMPILE_DEFINITIONS ${EXISTING_DEFS} CACHE INTERNAL "" FORCE)

    tw_get_profile_field(EXISTING_INCS OWN INCLUDE_DIRECTORIES)
    list(APPEND EXISTING_INCS ${CRYPTO_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})
    set(TW_PROFILE_OWN_INCLUDE_DIRECTORIES ${EXISTING_INCS} CACHE INTERNAL "" FORCE)

    tw_get_profile_field(EXISTING_LIBS OWN LINK_LIBRARIES)
    list(APPEND EXISTING_LIBS ${CMAKE_THREAD_LIBS_INIT} ${ZLIB_LIBRARIES} ${CRYPTO_LIBRARIES} ${CURL_LIBRARIES} ${PLATFORM_LIBS})
    set(TW_PROFILE_OWN_LINK_LIBRARIES ${EXISTING_LIBS} CACHE INTERNAL "" FORCE)
  else()
    tw_get_profile_field(EXISTING_INCS OWN INCLUDE_DIRECTORIES)
    list(APPEND EXISTING_INCS ${ZLIB_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})
    set(TW_PROFILE_OWN_INCLUDE_DIRECTORIES ${EXISTING_INCS} CACHE INTERNAL "" FORCE)

    tw_get_profile_field(EXISTING_LIBS OWN LINK_LIBRARIES)
    list(APPEND EXISTING_LIBS ${CMAKE_THREAD_LIBS_INIT} ${ZLIB_LIBRARIES} ${CURL_LIBRARIES} ${PLATFORM_LIBS})
    set(TW_PROFILE_OWN_LINK_LIBRARIES ${EXISTING_LIBS} CACHE INTERNAL "" FORCE)
  endif()

  if(HEADLESS_CLIENT)
    tw_get_profile_field(EXISTING_DEFS OWN COMPILE_DEFINITIONS)
    list(APPEND EXISTING_DEFS CONF_HEADLESS_CLIENT)
    set(TW_PROFILE_OWN_COMPILE_DEFINITIONS ${EXISTING_DEFS} CACHE INTERNAL "" FORCE)
  endif()

  if(TARGET_OS STREQUAL "windows")
    tw_get_profile_field(EXISTING_DEFS OWN COMPILE_DEFINITIONS)
    list(APPEND EXISTING_DEFS _WIN32_WINNT=0x0501 UNICODE _UNICODE)
    set(TW_PROFILE_OWN_COMPILE_DEFINITIONS ${EXISTING_DEFS} CACHE INTERNAL "" FORCE)
  endif()

  if(NOT TW_DEFINE_FORTIFY_SOURCE)
    tw_get_profile_field(EXISTING_DEFS OWN COMPILE_DEFINITIONS)
    list(REMOVE_ITEM EXISTING_DEFS "$<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>")
    set(TW_PROFILE_OWN_COMPILE_DEFINITIONS ${EXISTING_DEFS} CACHE INTERNAL "" FORCE)
  endif()
endfunction()

function(tw_apply_profile_client_libs)
  tw_get_profile_field(EXISTING_INCS CLIENT INCLUDE_DIRECTORIES)
  if(FREETYPE_INCLUDE_DIRS)
    list(APPEND EXISTING_INCS ${FREETYPE_INCLUDE_DIRS})
  endif()
  if(PNGLITE_INCLUDE_DIRS)
    list(APPEND EXISTING_INCS ${PNGLITE_INCLUDE_DIRS})
  endif()
  if(SDL2_INCLUDE_DIRS)
    list(APPEND EXISTING_INCS ${SDL2_INCLUDE_DIRS})
  endif()
  if(WAVPACK_INCLUDE_DIRS)
    list(APPEND EXISTING_INCS ${WAVPACK_INCLUDE_DIRS})
  endif()
  if(PLATFORM_CLIENT_INCLUDE_DIRS)
    list(APPEND EXISTING_INCS ${PLATFORM_CLIENT_INCLUDE_DIRS})
  endif()
  set(TW_PROFILE_CLIENT_INCLUDE_DIRECTORIES ${EXISTING_INCS} CACHE INTERNAL "" FORCE)

  tw_get_profile_field(EXISTING_LIBS CLIENT LINK_LIBRARIES)
  if(FREETYPE_LIBRARIES)
    list(APPEND EXISTING_LIBS ${FREETYPE_LIBRARIES})
  endif()
  if(PNGLITE_LIBRARIES)
    list(APPEND EXISTING_LIBS ${PNGLITE_LIBRARIES})
  endif()
  if(SDL2_LIBRARIES)
    list(APPEND EXISTING_LIBS ${SDL2_LIBRARIES})
  endif()
  if(WAVPACK_LIBRARIES)
    list(APPEND EXISTING_LIBS ${WAVPACK_LIBRARIES})
  endif()
  if(PLATFORM_CLIENT_LIBS)
    list(APPEND EXISTING_LIBS ${PLATFORM_CLIENT_LIBS})
  endif()
  set(TW_PROFILE_CLIENT_LINK_LIBRARIES ${EXISTING_LIBS} CACHE INTERNAL "" FORCE)
endfunction()

function(tw_apply_profile_test_libs)
  tw_get_profile_field(EXISTING_INCS TEST INCLUDE_DIRECTORIES)
  if(GTEST_INCLUDE_DIRS)
    list(APPEND EXISTING_INCS ${GTEST_INCLUDE_DIRS})
  endif()
  set(TW_PROFILE_TEST_INCLUDE_DIRECTORIES ${EXISTING_INCS} CACHE INTERNAL "" FORCE)

  tw_get_profile_field(EXISTING_LIBS TEST LINK_LIBRARIES)
  if(GTEST_LIBRARIES)
    list(APPEND EXISTING_LIBS ${GTEST_LIBRARIES})
  endif()
  set(TW_PROFILE_TEST_LINK_LIBRARIES ${EXISTING_LIBS} CACHE INTERNAL "" FORCE)
endfunction()

########################################################################
# TARGET CREATION: tw_add_executable / tw_add_library
########################################################################

function(tw_apply_profile TARGET)
  cmake_parse_arguments(ARG "" "PROFILE" "" ${ARGN})
  if(NOT ARG_PROFILE)
    message(FATAL_ERROR "tw_apply_profile: PROFILE is required. Registered: ${TW_PROFILES}")
  endif()
  string(TOUPPER ${ARG_PROFILE} PROFILE_UPPER)
  if(NOT TW_PROFILE_${PROFILE_UPPER}_DEFINED)
    message(FATAL_ERROR "tw_apply_profile: Unknown profile '${ARG_PROFILE}'. Registered: ${TW_PROFILES}")
  endif()

  set_property(GLOBAL APPEND PROPERTY TW_TARGETS ${TARGET})
  set_property(TARGET ${TARGET} PROPERTY TW_PROFILE ${PROFILE_UPPER})

  tw_get_profile_field(DEFS ${PROFILE_UPPER} COMPILE_DEFINITIONS)
  tw_get_profile_field(INCS ${PROFILE_UPPER} INCLUDE_DIRECTORIES)
  tw_get_profile_field(OPTS ${PROFILE_UPPER} COMPILE_OPTIONS)
  tw_get_profile_field(MSVC_OPTS ${PROFILE_UPPER} MSVC_OPTIONS)
  tw_get_profile_field(MSVC_RT ${PROFILE_UPPER} MSVC_RUNTIME)
  tw_get_profile_field(LINK_OPTS ${PROFILE_UPPER} LINK_OPTIONS)
  tw_get_profile_field(LINK_LIBS ${PROFILE_UPPER} LINK_LIBRARIES)
  tw_get_profile_field(INSTALL_DEST ${PROFILE_UPPER} INSTALL_DESTINATION)

  if(MSVC)
    if(MSVC_RT)
      if(POLICY CMP0091)
        set_property(TARGET ${TARGET} PROPERTY MSVC_RUNTIME_LIBRARY "${MSVC_RT}")
      else()
        target_compile_options(${TARGET} PRIVATE $<$<NOT:${DBG}>:/MT> $<${DBG}:/MTd>)
      endif()
    endif()
    if(MSVC_OPTS)
      target_compile_options(${TARGET} PRIVATE ${MSVC_OPTS})
    endif()
  else()
    if(OPTS)
      target_compile_options(${TARGET} PRIVATE ${OPTS})
    endif()
  endif()

  if(NOT MSVC)
    if(PROFILE_UPPER STREQUAL "DEP")
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
      if(DEFS)
        target_compile_definitions(${TARGET} PRIVATE ${DEFS})
      endif()
      if(INCS)
        target_include_directories(${TARGET} PRIVATE ${INCS})
      endif()
    endif()
  else()
    if(NOT PROFILE_UPPER STREQUAL "DEP")
      if(DEFS)
        target_compile_definitions(${TARGET} PRIVATE ${DEFS})
      endif()
      if(INCS)
        target_include_directories(${TARGET} PRIVATE ${INCS})
      endif()
    endif()
  endif()

  if(LINK_LIBS)
    target_link_libraries(${TARGET} PRIVATE ${LINK_LIBS})
  endif()
  if(LINK_OPTS)
    target_link_options(${TARGET} PRIVATE ${LINK_OPTS})
  endif()

  get_target_property(TARGET_TYPE ${TARGET} TYPE)
  if(TARGET_TYPE STREQUAL "EXECUTABLE" OR TARGET_TYPE STREQUAL "SHARED_LIBRARY" OR TARGET_TYPE STREQUAL "MODULE_LIBRARY")
    if(MSVC)
      set_property(TARGET ${TARGET} APPEND PROPERTY LINK_FLAGS /SAFESEH:NO)
    endif()
    if(TARGET_OS STREQUAL "mac")
      target_link_libraries(${TARGET} PRIVATE -stdlib=libc++)
      target_link_libraries(${TARGET} PRIVATE -mmacosx-version-min=10.7)
    endif()
    if((MINGW OR TARGET_OS STREQUAL "linux") AND PREFER_BUNDLED_LIBS)
      target_link_libraries(${TARGET} PRIVATE -static-libgcc)
      target_link_libraries(${TARGET} PRIVATE -static-libstdc++)
      if(MINGW)
        target_link_libraries(${TARGET} PRIVATE -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic)
      endif()
    endif()
  endif()

  if(PROFILE_UPPER STREQUAL "DEP")
    list(APPEND TARGETS_DEP ${TARGET})
    set(TARGETS_DEP ${TARGETS_DEP} CACHE INTERNAL "" FORCE)
  else()
    list(APPEND TARGETS_OWN ${TARGET})
    set(TARGETS_OWN ${TARGETS_OWN} CACHE INTERNAL "" FORCE)
  endif()

  if(TARGET_TYPE STREQUAL "EXECUTABLE" OR TARGET_TYPE STREQUAL "SHARED_LIBRARY" OR TARGET_TYPE STREQUAL "MODULE_LIBRARY")
    list(APPEND TARGETS_LINK ${TARGET})
    set(TARGETS_LINK ${TARGETS_LINK} CACHE INTERNAL "" FORCE)
    if(INSTALL_DEST)
      list(APPEND TW_INSTALL_TARGETS ${TARGET})
      set(TW_INSTALL_TARGETS ${TW_INSTALL_TARGETS} CACHE INTERNAL "" FORCE)
    endif()
  endif()
endfunction()

function(tw_add_executable NAME)
  cmake_parse_arguments(ARG "EXCLUDE_FROM_ALL" "PROFILE" "" ${ARGN})
  if(NOT ARG_PROFILE)
    message(FATAL_ERROR "tw_add_executable(${NAME}): PROFILE is required")
  endif()

  set(EXTRA_ARGS)
  if(ARG_EXCLUDE_FROM_ALL)
    list(APPEND EXTRA_ARGS EXCLUDE_FROM_ALL)
  endif()

  add_executable(${NAME} ${EXTRA_ARGS} ${ARG_UNPARSED_ARGUMENTS})
  tw_apply_profile(${NAME} PROFILE ${ARG_PROFILE})
endfunction()

function(tw_add_library NAME)
  cmake_parse_arguments(ARG "EXCLUDE_FROM_ALL" "PROFILE;TYPE" "" ${ARGN})
  if(NOT ARG_PROFILE)
    message(FATAL_ERROR "tw_add_library(${NAME}): PROFILE is required")
  endif()
  if(NOT ARG_TYPE)
    set(ARG_TYPE OBJECT)
  endif()

  set(EXTRA_ARGS)
  if(ARG_EXCLUDE_FROM_ALL)
    list(APPEND EXTRA_ARGS EXCLUDE_FROM_ALL)
  endif()

  add_library(${NAME} ${ARG_TYPE} ${EXTRA_ARGS} ${ARG_UNPARSED_ARGUMENTS})
  tw_apply_profile(${NAME} PROFILE ${ARG_PROFILE})
endfunction()

########################################################################
# INSTALL / PACKAGE (applied automatically for targets with install info)
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

  if(INST_TARGETS)
    set(INSTALL_TARGETS ${INST_TARGETS})
  else()
    set(INSTALL_TARGETS ${TW_INSTALL_TARGETS})
  endif()

  set(CPACK_TARGETS ${INSTALL_TARGETS} CACHE INTERNAL "Targets to include in CPack packages" FORCE)

  if(INST_DATA_DIRS)
    install(DIRECTORY ${INST_DATA_DIRS}
      DESTINATION share/${PROJECT_NAME}
      COMPONENT data
    )
  endif()

  foreach(T ${INSTALL_TARGETS})
    if(TARGET ${T})
      get_target_property(_TYPE ${T} TYPE)
      get_target_property(_PROF ${T} TW_PROFILE)
      if(_TYPE STREQUAL "EXECUTABLE")
        if(_PROF)
          tw_get_profile_field(_DEST ${_PROF} INSTALL_DESTINATION)
          tw_get_profile_field(_COMP ${_PROF} INSTALL_COMPONENT)
          if(_DEST AND _COMP)
            install(TARGETS ${T} DESTINATION ${_DEST} COMPONENT ${_COMP})
          elseif(_DEST)
            install(TARGETS ${T} DESTINATION ${_DEST})
          else()
            install(TARGETS ${T} DESTINATION bin COMPONENT ${T})
          endif()
        else()
          install(TARGETS ${T} DESTINATION bin COMPONENT ${T})
        endif()
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

  foreach(T ${INSTALL_TARGETS})
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
# CLIENT-SPECIFIC DYNAMIC CHECKS (Wavpack API detection)
########################################################################

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
# STANDALONE MACRO HELPERS (for targets not using tw_add_*)
########################################################################

function(tw_apply_assert_config TARGET)
  target_compile_definitions(${TARGET} PRIVATE _GLIBCXX_ASSERTIONS)
endfunction()

function(tw_apply_debug_config TARGET)
  target_compile_definitions(${TARGET} PRIVATE $<$<CONFIG:Debug>:CONF_DEBUG>)
  if(TW_DEFINE_FORTIFY_SOURCE)
    target_compile_definitions(${TARGET} PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>)
  endif()
endfunction()

function(tw_apply_platform_defines TARGET)
  if(TARGET_OS STREQUAL "windows")
    target_compile_definitions(${TARGET} PRIVATE _WIN32_WINNT=0x0501 UNICODE _UNICODE)
  endif()
  if(HEADLESS_CLIENT)
    target_compile_definitions(${TARGET} PRIVATE CONF_HEADLESS_CLIENT)
  endif()
endfunction()
