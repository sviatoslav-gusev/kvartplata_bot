cmake_minimum_required(VERSION 3.25)
project(kvartplata_bot LANGUAGES CXX)

function(enable_sanitizers tgt)
  if (MSVC)
    message(STATUS "Enabling AddressSanitizer for target: ${tgt} (Windows)")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "" FORCE)
    #target_compile_options(${tgt} PRIVATE /fsanitize=address)
    #target_link_options(${tgt} PRIVATE /fsanitize=address)
    foreach(var CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG)
      if (${var} MATCHES "/RTC1")
        string(REPLACE "/RTC1" "" ${var} "${${var}}")
        set(${var} "${${var}}" CACHE STRING "" FORCE)
      endif()
    endforeach()
    return()
  endif()
  message(STATUS "Enabling sanitizers for target: ${tgt} (Unix)")
  target_compile_options(${tgt} PRIVATE -fno-omit-frame-pointer -fno-optimize-sibling-calls)
  set(_flags "address,undefined")
  target_compile_options(${tgt} PRIVATE -fsanitize=${_flags})
  target_link_options(${tgt} PRIVATE -fsanitize=${_flags})
endfunction()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(kvartplata_bot
    src/main.cpp
    src/Application.cpp
    src/Bot.cpp
    src/Scheduler.cpp
    src/UserConfig.cpp
    src/helpers/Getopt.cpp
    src/storages/ConfigStorage.cpp
    src/storages/PathsStorage.cpp
    src/storages/TokenStorage.cpp
)

if (MSVC)
  if (CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug$")
    set_property(TARGET kvartplata_bot PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

    #target_compile_options(kvartplata_bot PRIVATE /fsanitize=address)
    #target_link_options(kvartplata_bot PRIVATE /fsanitize=address)

    target_link_options(kvartplata_bot PRIVATE /INCREMENTAL:NO)
  endif()
else()
  # Linux/macOS
  if (CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug$")
    target_compile_options(kvartplata_bot PRIVATE -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer)
    target_link_options(kvartplata_bot PRIVATE -fsanitize=address -fsanitize=undefined)
  endif()
endif()

if (CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug$")
  enable_sanitizers(kvartplata_bot)
endif()

find_package(Threads REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

find_package(CURL QUIET)
if (CURL_FOUND)
  target_link_libraries(kvartplata_bot PRIVATE CURL::libcurl)
  target_compile_definitions(kvartplata_bot PRIVATE HAVE_CURL)
endif()

find_package(TgBot CONFIG QUIET)
if (NOT TARGET TgBot::TgBot)
  find_package(tgbot-cpp CONFIG QUIET)
endif()

if (TARGET TgBot::TgBot)
  target_link_libraries(kvartplata_bot PRIVATE TgBot::TgBot)
elseif (DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
  set(_VCPKG_PREFIX "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
  find_path(TGBOT_INCLUDE_DIR NAMES tgbot/tgbot.h PATHS "${_VCPKG_PREFIX}/include" NO_DEFAULT_PATH)
  find_library(TGBOT_LIBRARY_RELEASE NAMES TgBot tgbot PATHS "${_VCPKG_PREFIX}/lib" NO_DEFAULT_PATH)
  find_library(TGBOT_LIBRARY_DEBUG   NAMES TgBotd tgbotd TgBot tgbot PATHS "${_VCPKG_PREFIX}/debug/lib" "${_VCPKG_PREFIX}/lib" NO_DEFAULT_PATH)
  if (TGBOT_INCLUDE_DIR AND (TGBOT_LIBRARY_RELEASE OR TGBOT_LIBRARY_DEBUG))
    add_library(TgBot::TgBot UNKNOWN IMPORTED)
    set_target_properties(TgBot::TgBot PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${TGBOT_INCLUDE_DIR}"
      IMPORTED_CONFIGURATIONS "DEBUG;RELEASE")
    if (TGBOT_LIBRARY_RELEASE)
      set_target_properties(TgBot::TgBot PROPERTIES IMPORTED_LOCATION_RELEASE "${TGBOT_LIBRARY_RELEASE}")
    endif()
    if (TGBOT_LIBRARY_DEBUG)
      set_target_properties(TgBot::TgBot PROPERTIES IMPORTED_LOCATION_DEBUG   "${TGBOT_LIBRARY_DEBUG}")
    endif()
    target_link_libraries(kvartplata_bot PRIVATE TgBot::TgBot)
  else()
    message(FATAL_ERROR "tgbot-cpp not found in vcpkg (${_VCPKG_PREFIX}).")
  endif()
else()
  find_path(TGBOT_INCLUDE_DIR NAMES tgbot/tgbot.h PATHS /usr/include /usr/local/include)
  find_library(TGBOT_LIBRARY NAMES TgBot tgbot TgBotd tgbotd PATHS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64)
  if (TGBOT_INCLUDE_DIR AND TGBOT_LIBRARY)
    add_library(TgBot::TgBot UNKNOWN IMPORTED)
    set_target_properties(TgBot::TgBot PROPERTIES
      IMPORTED_LOCATION "${TGBOT_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${TGBOT_INCLUDE_DIR}")
    target_link_libraries(kvartplata_bot PRIVATE TgBot::TgBot)
  else()
    message(FATAL_ERROR "tgbot-cpp (TgBot) не найден.")
  endif()
endif()

target_link_libraries(kvartplata_bot PRIVATE
  Threads::Threads
  Boost::system
  OpenSSL::SSL
  OpenSSL::Crypto
)

if (MSVC)
  target_compile_options(kvartplata_bot PRIVATE /W4 /permissive-)
else()
  target_compile_options(kvartplata_bot PRIVATE -Wall -Wextra -Wpedantic)
endif()
