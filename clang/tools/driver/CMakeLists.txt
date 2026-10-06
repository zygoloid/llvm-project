set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Analysis
  CodeGen
  Core
  IPO
  AggressiveInstCombine
  InstCombine
  Instrumentation
  MC
  MCParser
  ObjCARCOpts
  Option
  ScalarOpts
  Support
  TargetParser
  TransformUtils
  Vectorize
  )

# Support plugins.
if(CLANG_PLUGIN_SUPPORT)
  set(support_plugins SUPPORT_PLUGINS)
endif()

set(CLANG_BOLT_ALLOWLIST INSTRUMENT PERF LBR)
set(CLANG_BOLT OFF CACHE STRING "Apply BOLT optimization to Clang. \
May be specified as one of ${CLANG_BOLT_ALLOWLIST} to use a particular profiling \
  mechanism.")
string(TOUPPER "${CLANG_BOLT}" CLANG_BOLT)
if (CLANG_BOLT AND NOT CLANG_BOLT IN_LIST CLANG_BOLT_ALLOWLIST)
    message(FATAL_ERROR "Specified CLANG_BOLT value '${CLANG_BOLT}' is not one of ${CLANG_BOLT_ALLOWLIST}.")
endif()

if (CLANG_BOLT AND NOT LLVM_BUILD_INSTRUMENTED)
  set(CLANG_BOLT_DEPS clear-bolt-fdata llvm-bolt llvm-readobj)
  if (NOT CLANG_BOLT STREQUAL "INSTRUMENT")
    list(APPEND CLANG_BOLT_DEPS clear-perf-data)
  endif()
endif()

add_clang_tool(clang
  driver.cpp
  cc1_main.cpp
  cc1as_main.cpp
  cc1edg_main.cpp
  cc1gen_reproducer_main.cpp

  DEPENDS
  intrinsics_gen
  # These generated headers are included transitively.
  target_parser_gen
  ${support_plugins}
  ${CLANG_BOLT_DEPS}
  GENERATE_DRIVER
  )

setup_host_tool(clang CLANG clang_exe clang_target)

clang_target_link_libraries(clang
  PRIVATE
  clangBasic
  clangCodeGen
  clangDriver
  clangFrontend
  clangFrontendTool
  clangOptions
  clangSerialization
  )

if(CLANG_ENABLE_EDG)
  set(EDG_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/edg_gen")
  file(MAKE_DIRECTORY "${EDG_GEN_DIR}")

  # Temporarily point CMAKE_SOURCE_DIR at CLANG_EDG_SOURCE_DIR so EDG's CMake
  # files can locate cmake/macro-conf.cmake and cmake/softfloat.cmake.
  set(EDG_SAVED_CMAKE_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
  set(CMAKE_SOURCE_DIR "${CLANG_EDG_SOURCE_DIR}")
  set(EDG_CPFE_USE_ASAN FALSE CACHE BOOL "" FORCE)
  set(EDG_ENABLE_CONVERSION_WARNINGS FALSE CACHE BOOL "" FORCE)

  include("${CLANG_EDG_SOURCE_DIR}/cmake/macro-conf.cmake")

  math(EXPR CLANG_EDG_VERSION_NUMBER
    "${CLANG_VERSION_MAJOR} * 10000 + ${CLANG_VERSION_MINOR} * 100 + ${CLANG_VERSION_PATCHLEVEL}")

  # Override macro_configuration_to_defines (called by edgcpp/src/CMakeLists.txt)
  # to generate cmake_defines.h using EDG's linux-clang-release configuration
  # plus Clang integration overrides.
  function(macro_configuration_to_defines target)
    set(CMAKE_SOURCE_DIR "${CLANG_EDG_SOURCE_DIR}")
    set(CMAKE_SOURCE_DIR "${CLANG_EDG_SOURCE_DIR}" PARENT_SCOPE)
    configure_definitions_as_defines("${target}" "linux-clang-release")
    file(WRITE "${PROJECT_HEADER_OUTPUT_DIRECTORY}/cmake_defines.h.tmp"
"${configured_cmakedef_definitions}
/* Clang integration overrides */
#undef BACK_END_IS_C_GEN_BE
#define BACK_END_IS_C_GEN_BE 1
#undef DO_IL_LOWERING
#define DO_IL_LOWERING 1
#undef LOWER_FIXED_POINT
#define LOWER_FIXED_POINT 1
#undef LOWER_IFUNC
#define LOWER_IFUNC 1
#undef LOWER_VARIABLE_LENGTH_ARRAYS
#define LOWER_VARIABLE_LENGTH_ARRAYS 0
#undef LOWER_COMPLEX
#define LOWER_COMPLEX 0
#undef RECORD_BACKING_EXPRS_WITH_IL_LOWERING
#define RECORD_BACKING_EXPRS_WITH_IL_LOWERING 1
#undef COMPILE_MULTIPLE_TRANSLATION_UNITS
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#undef SUN_EXTENSIONS_ALLOWED
#define SUN_EXTENSIONS_ALLOWED 1
#undef DEFAULT_SUN_COMPATIBILITY
#define DEFAULT_SUN_COMPATIBILITY 0
#undef MAKE_FRONT_END_CALLABLE
#define MAKE_FRONT_END_CALLABLE 1
#undef USE_EDG_NAMESPACE
#define USE_EDG_NAMESPACE 1
#undef NO_USR_INCLUDE
#define NO_USR_INCLUDE 1
#undef DEFAULT_USE_PREDEFINED_MACRO_FILE
#define DEFAULT_USE_PREDEFINED_MACRO_FILE 0
#undef DEFAULT_GNU_COMPATIBILITY
#define DEFAULT_GNU_COMPATIBILITY 1
#undef DEFAULT_CLANG_COMPATIBILITY
#define DEFAULT_CLANG_COMPATIBILITY 1
#undef DEFAULT_CLANG_VERSION
#define DEFAULT_CLANG_VERSION ${CLANG_EDG_VERSION_NUMBER}
#undef CLANG_IS_GENERATED_CODE_TARGET
#define CLANG_IS_GENERATED_CODE_TARGET 1
#undef GCC_IS_GENERATED_CODE_TARGET
#define GCC_IS_GENERATED_CODE_TARGET 0
#undef CLANG_TARGET_VERSION_NUMBER
#define CLANG_TARGET_VERSION_NUMBER ${CLANG_EDG_VERSION_NUMBER}
#undef DEFAULT_INSTANTIATION_MODE
#define DEFAULT_INSTANTIATION_MODE tim_used
#undef DEFAULT_AUTOMATIC_INSTANTIATION_MODE
#define DEFAULT_AUTOMATIC_INSTANTIATION_MODE 0
")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_HEADER_OUTPUT_DIRECTORY}/cmake_defines.h.tmp"
        "${PROJECT_HEADER_OUTPUT_DIRECTORY}/cmake_defines.h"
    )
    add_compile_definitions("USE_CMAKE_DEFINES")
  endfunction()

  function(macro_configuration_to_compile_definitions target)
  endfunction()

  add_subdirectory("${CLANG_EDG_SOURCE_DIR}/util"
                   "${CMAKE_CURRENT_BINARY_DIR}/edg_util"
                   EXCLUDE_FROM_ALL)
  add_subdirectory("${CLANG_EDG_SOURCE_DIR}/src"
                   "${CMAKE_CURRENT_BINARY_DIR}/edg_src"
                   EXCLUDE_FROM_ALL)

  set(CMAKE_SOURCE_DIR "${EDG_SAVED_CMAKE_SOURCE_DIR}")

  get_directory_property(EDG_CORE_SOURCES
    DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/edg_src"
    DEFINITION CORE_FRONT_END_SOURCE_FILES)
  get_directory_property(EDG_HEADER_DIR
    DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/edg_src"
    DEFINITION PROJECT_HEADER_OUTPUT_DIRECTORY)
  set_source_files_properties(${EDG_CORE_SOURCES} PROPERTIES LANGUAGE CXX)

  add_library(clangEDG STATIC EXCLUDE_FROM_ALL ${EDG_CORE_SOURCES})
  add_dependencies(clangEDG error-info)
  target_include_directories(clangEDG PRIVATE
    "${EDG_HEADER_DIR}"
    "${CLANG_EDG_SOURCE_DIR}/src")
  target_compile_definitions(clangEDG PRIVATE USE_CMAKE_DEFINES=1)
  target_compile_options(clangEDG PRIVATE
    -std=c++14 -fno-exceptions -fno-unwind-tables -fno-rtti -w -x c++)

  target_link_libraries(clang PRIVATE clangEDG)

  # Build the EDG portable EH runtime archive (libclang_rt.edg.a) into Clang's
  # resource directory so C++ programs with exceptions link seamlessly while
  # using c_gen_be (DO_FULL_PORTABLE_EH_LOWERING).
  include(GetClangResourceDir)
  get_clang_resource_dir(EDG_RT_DIR PREFIX ${LLVM_LIBRARY_OUTPUT_INTDIR}/.. SUBDIR lib)
  set(EDG_RT_OBJ_DIR "${EDG_GEN_DIR}/rt")
  file(MAKE_DIRECTORY "${EDG_RT_OBJ_DIR}")

  set(EDG_RT_FILES
    bad_alloc eh_util error exception newhandler rtti set_new throw typeinfo
    vars vec_cctor vec_newdel
  )
  set(EDG_RT_OBJS)
  foreach(rt_file IN LISTS EDG_RT_FILES)
    set(rt_src "${CLANG_EDG_SOURCE_DIR}/lib_src/${rt_file}.c")
    set(rt_obj "${EDG_RT_OBJ_DIR}/${rt_file}.o")
    list(APPEND EDG_RT_OBJS "${rt_obj}")
    add_custom_command(
      OUTPUT "${rt_obj}"
      COMMAND "$<TARGET_FILE:clang>"
        -fedg -x c++ -std=c++23 -fPIC -O2 -w
        -nostdinc++
        -DUSE_STDINT_HEADER=1
        -I "${CLANG_EDG_SOURCE_DIR}/lib_src"
        -isystem "${CLANG_EDG_SOURCE_DIR}/include_c++"
        -isystem "${CLANG_EDG_SOURCE_DIR}/include_c99"
        -Xedg --building_runtime
        -Xedg --diag_suppress=lossy_conversion
        -c "${rt_src}" -o "${rt_obj}"
      DEPENDS clang clang-resource-headers "${rt_src}"
      COMMENT "Building EDG runtime object ${rt_file}.o"
    )
  endforeach()

  set(EDG_RT_ARCHIVE "${EDG_RT_DIR}/libclang_rt.edg.a")
  add_custom_command(
    OUTPUT "${EDG_RT_ARCHIVE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${EDG_RT_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${EDG_RT_ARCHIVE}"
    COMMAND "${CMAKE_AR}" rcs "${EDG_RT_ARCHIVE}" ${EDG_RT_OBJS}
    DEPENDS ${EDG_RT_OBJS}
    COMMENT "Archiving ${EDG_RT_ARCHIVE}"
  )
  add_custom_target(clang-edg-runtime ALL DEPENDS "${EDG_RT_ARCHIVE}")
endif()

if(WIN32 AND NOT CYGWIN)
  # Prevent versioning if the buildhost is targeting for Win32.
else()
  set_target_properties(clang PROPERTIES VERSION ${CLANG_EXECUTABLE_VERSION})
endif()

# Support plugins.
if(CLANG_PLUGIN_SUPPORT)
  export_executable_symbols_for_plugins(clang)
endif()

add_dependencies(clang clang-resource-headers)

if(NOT CLANG_LINKS_TO_CREATE)
  set(CLANG_LINKS_TO_CREATE clang++ clang-cl clang-cpp)
endif()

set(HLSL_LINK clang-dxc)

foreach(link ${CLANG_LINKS_TO_CREATE} ${HLSL_LINK})
  add_clang_symlink(${link} clang)
endforeach()

# Configure plist creation for OS X.
set (TOOL_INFO_PLIST "Info.plist" CACHE STRING "Plist name")
if (APPLE)
  if (CLANG_VENDOR)
    set(TOOL_INFO_NAME "${CLANG_VENDOR} clang")
  else()
    set(TOOL_INFO_NAME "clang")
  endif()

  set(TOOL_INFO_UTI "${CLANG_VENDOR_UTI}")
  set(TOOL_INFO_VERSION "${CLANG_VERSION}")
  set(TOOL_INFO_BUILD_VERSION "${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}")

  set(TOOL_INFO_PLIST_OUT "${CMAKE_CURRENT_BINARY_DIR}/${TOOL_INFO_PLIST}")

  if(LLVM_TOOL_LLVM_DRIVER_BUILD AND clang IN_LIST LLVM_DRIVER_TOOLS)
    set(TARGET_NAME llvm-driver)
  else()
    set(TARGET_NAME clang)
  endif()

  target_link_libraries(${TARGET_NAME}
    PRIVATE
    "-Wl,-sectcreate,__TEXT,__info_plist,\"${TOOL_INFO_PLIST_OUT}\"")
  configure_file("${TOOL_INFO_PLIST}.in" "${TOOL_INFO_PLIST_OUT}" @ONLY)

  set(TOOL_INFO_UTI)
  set(TOOL_INFO_NAME)
  set(TOOL_INFO_VERSION)
  set(TOOL_INFO_BUILD_VERSION)
endif()

if(CLANG_ORDER_FILE AND
    (LLVM_LINKER_IS_APPLE OR LLVM_LINKER_IS_GOLD OR LLVM_LINKER_IS_LLD))
  include(CheckLinkerFlag)

  if (LLVM_LINKER_IS_APPLE OR (LLVM_LINKER_IS_LLD AND APPLE))
    set(LINKER_ORDER_FILE_OPTION "-Wl,-order_file,${CLANG_ORDER_FILE}")
  elseif (LLVM_LINKER_IS_GOLD)
    set(LINKER_ORDER_FILE_OPTION "-Wl,--section-ordering-file,${CLANG_ORDER_FILE}")
  elseif (LLVM_LINKER_IS_LLD)
    set(LINKER_ORDER_FILE_OPTION "-Wl,--symbol-ordering-file,${CLANG_ORDER_FILE}")
  endif()

  # This is a test to ensure the actual order file works with the linker.
  check_linker_flag(CXX ${LINKER_ORDER_FILE_OPTION} LINKER_ORDER_FILE_WORKS)

  # Passing an empty order file disables some linker layout optimizations.
  # To work around this and enable workflows for re-linking when the order file
  # changes we check during configuration if the file is empty, and make it a
  # configuration dependency.
  file(READ ${CLANG_ORDER_FILE} ORDER_FILE LIMIT 20)
  if("${ORDER_FILE}" STREQUAL "\n")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CLANG_ORDER_FILE})
  elseif(LINKER_ORDER_FILE_WORKS)
    target_link_libraries(clang PRIVATE ${LINKER_ORDER_FILE_OPTION})
    set_target_properties(clang PROPERTIES LINK_DEPENDS ${CLANG_ORDER_FILE})
  endif()
endif()

if (CLANG_BOLT AND NOT LLVM_BUILD_INSTRUMENTED)
  # Add a clang-bolt target for backwards compatibility.
  add_custom_target(clang-bolt DEPENDS clang)

  set(CLANG_BOLT_INSTRUMENTED "clang-bolt.inst" CACHE STRING
    "Name of BOLT-instrumented Clang binary")
  set(CLANG_INSTRUMENTED ${LLVM_RUNTIME_OUTPUT_INTDIR}/${CLANG_BOLT_INSTRUMENTED})
  set(PERF_TRAINING_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../utils/perf-training)
  set(BOLT_FDATA ${PERF_TRAINING_BINARY_DIR}/prof.fdata)
  get_llvm_lit_path(
    lit_base_dir
    lit_file_name
    ALLOW_EXTERNAL
  )
  set(LIT_COMMAND "${lit_base_dir}/${lit_file_name}")

  set(CLANG_BOLT_INPUTS $<TARGET_FILE:clang>)
  set(CLANG_INSTRUMENTED_OUTPUTS ${CLANG_INSTRUMENTED})

  # Add in dynamically linked libraries, if needs be. Currently only supported
  # on Linux because it relies on LD_PRELOAD for instrumentation.
  if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (CLANG_LINK_CLANG_DYLIB)
     set(CLANG_CPP_BOLT_INSTRUMENTED "clang-cxx-bolt.inst" CACHE STRING
       "Name of BOLT-instrumented Clang library")
     set(CLANG_CPP_INSTRUMENTED ${LLVM_RUNTIME_OUTPUT_INTDIR}/${CLANG_CPP_BOLT_INSTRUMENTED})
     list(APPEND CLANG_BOLT_INPUTS $<TARGET_FILE:clang-cpp>)
     list(APPEND CLANG_INSTRUMENTED_OUTPUTS ${CLANG_CPP_INSTRUMENTED})
    endif()
    if (LLVM_LINK_LLVM_DYLIB)
      set(LLVM_BOLT_INSTRUMENTED "LLVM-bolt.inst" CACHE STRING
        "Name of BOLT-instrumented LLVM library")
      set(LLVM_INSTRUMENTED ${LLVM_RUNTIME_OUTPUT_INTDIR}/${LLVM_BOLT_INSTRUMENTED})
      list(APPEND CLANG_BOLT_INPUTS $<TARGET_FILE:LLVM>)
      list(APPEND CLANG_INSTRUMENTED_OUTPUTS ${LLVM_INSTRUMENTED})
    endif()
  endif()

  # This POST_BUILD command is executed unconditionally even if the clang target
  # is already built.  We need to wrap the whole bolt optimization process in
  # a single python wrapper, so that we can first check if the binary has
  # already been optimized and then exit early with a 0 status if it has.
  add_custom_command(
    TARGET clang POST_BUILD
    COMMAND  "${Python3_EXECUTABLE}" ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/perf-training/perf-helper.py
             bolt-optimize
             --method ${CLANG_BOLT}
             --input "${CLANG_BOLT_INPUTS}"
             --instrumented-output "${CLANG_INSTRUMENTED_OUTPUTS}"
             --fdata ${BOLT_FDATA}
             --perf-training-binary-dir ${PERF_TRAINING_BINARY_DIR}
             --readelf $<TARGET_FILE:llvm-readobj>
             --bolt $<TARGET_FILE:llvm-bolt>
             --lit "${LIT_COMMAND}"
             --merge-fdata $<TARGET_FILE:merge-fdata>
    COMMENT "Optimizing Clang with BOLT"
    USES_TERMINAL
    VERBATIM
  )
endif()
