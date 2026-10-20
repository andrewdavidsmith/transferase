# SPDX-License-Identifier: MIT; Copyright 2026 Andrew D Smith

find_package(Threads REQUIRED)
find_package(LIBDEFLATE REQUIRED)

if(USE_MBEDTLS)
  find_package(MbedTLS REQUIRED)
else()
  find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)
endif()

if(LIBCURL_ROOT_DIR)
  find_package(CURL PATHS ${LIBCURL_ROOT_DIR} NO_DEFAULT_PATH REQUIRED)
else()
  find_package(CURL REQUIRED)
endif()

message(STATUS "Adding external header-only project 'asio'")
include(FetchContent)
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/andrewdavidsmith/asio.git
  GIT_TAG main
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  SYSTEM
  OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(asio)
FetchContent_GetProperties(asio SOURCE_DIR ASIO_SOURCE_DIR)

file(GLOB cpp_files "*.cpp")

# Gather all the object files that will be put in the static library
# and prepare to compile them.
set(LIBRARY_OBJECTS "")
foreach(cpp_file ${cpp_files})
  get_filename_component(BASE_NAME ${cpp_file} NAME_WE)
  add_library(${BASE_NAME} OBJECT ${cpp_file})
  ## Below is to make sure 'config.h' is visible for includes and any
  ## of the headers for configured libraries
  target_include_directories(${BASE_NAME} PUBLIC
    ${PROJECT_BINARY_DIR}
    ${ZLIB_INCLUDE_DIRS}
    ${ASIO_SOURCE_DIR}
    ${LIBDEFLATE_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
  )
  target_compile_options(${BASE_NAME} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
  list(APPEND LIBRARY_OBJECTS ${BASE_NAME})
endforeach()

# macOS GCC-15 has problems with the a zero-sized array in mount.h
get_target_property(target_options find_path_to_binary COMPILE_OPTIONS)
list(REMOVE_ITEM target_options "-Wpedantic")
set_property(TARGET find_path_to_binary PROPERTY COMPILE_OPTIONS ${target_options})

# Create the initial static library linking the individual objects
add_library(transferase_system STATIC)
target_include_directories(transferase_system PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_options(transferase_system PRIVATE ${GLOBAL_LINKER_OPTIONS})
target_link_libraries(transferase_system PUBLIC
  ${LIBRARY_OBJECTS}
  ZLIB::ZLIB
  Threads::Threads
  LIBDEFLATE::LIBDEFLATE
  CURL::libcurl
)
if(USE_MBEDTLS)
  target_link_libraries(transferase_system PUBLIC
    MbedTLS::mbedtls
    MbedTLS::tfpsacrypto
    MbedTLS::mbedx509
  )
else()
  target_link_libraries(transferase_system PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    zstd
  )
endif()

if(TEST_LIB)
  # The UNIT_TEST preprocessor variable turns off the static and inline
  # on several functions so they can be tested; all sources could need this.
  add_compile_definitions(UNIT_TEST)
  enable_testing()
  include(GoogleTest)
  add_subdirectory(tests)
endif()
