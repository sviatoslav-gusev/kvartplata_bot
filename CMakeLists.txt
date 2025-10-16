cmake_minimum_required(VERSION 3.25)
project(kvartplata_bot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # <— важно для clangd

# Основная цель
add_executable(kvartplata_bot
    src/main.cpp
    src/Application.cpp
    src/Bot.cpp
    src/Scheduler.cpp
    src/UserConfig.cpp
    src/storages/ConfigStorage.cpp
    src/storages/PathsStorage.cpp
    src/storages/TokenStorage.cpp
)

# --- Общие зависимости ---
find_package(Threads REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS system)  # через CONFIG по возможности
find_package(OpenSSL REQUIRED)

find_package(CURL QUIET)   # опционально
if (CURL_FOUND)
  target_link_libraries(kvartplata_bot PRIVATE CURL::libcurl)
  target_compile_definitions(kvartplata_bot PRIVATE HAVE_CURL)
endif()

# ================== TgBot (tgbot-cpp) ==================
# 0) Попытка через CONFIG-пакеты (если есть где-то в системе/vcpkg)
find_package(TgBot CONFIG QUIET)
if (NOT TARGET TgBot::TgBot)
  find_package(tgbot-cpp CONFIG QUIET)
endif()

# 1) Если нашли цель — готово
if (TARGET TgBot::TgBot)
  target_link_libraries(kvartplata_bot PRIVATE TgBot::TgBot)

# 2) Если НЕ нашли, но есть vcpkg (Windows или Linux с vcpkg) — собираем импортируемую цель из vcpkg
elseif (DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
  set(_VCPKG_PREFIX "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

  find_path(TGBOT_INCLUDE_DIR
    NAMES tgbot/tgbot.h
    PATHS "${_VCPKG_PREFIX}/include"
    NO_DEFAULT_PATH
  )

  # Release/Debug либы
  find_library(TGBOT_LIBRARY_RELEASE
    NAMES TgBot tgbot
    PATHS "${_VCPKG_PREFIX}/lib"
    NO_DEFAULT_PATH
  )
  find_library(TGBOT_LIBRARY_DEBUG
    NAMES TgBotd tgbotd TgBot tgbot
    PATHS "${_VCPKG_PREFIX}/debug/lib" "${_VCPKG_PREFIX}/lib"
    NO_DEFAULT_PATH
  )

  if (TGBOT_INCLUDE_DIR AND (TGBOT_LIBRARY_RELEASE OR TGBOT_LIBRARY_DEBUG))
    add_library(TgBot::TgBot UNKNOWN IMPORTED)
    set_target_properties(TgBot::TgBot PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${TGBOT_INCLUDE_DIR}"
      IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
    )
    if (TGBOT_LIBRARY_RELEASE)
      set_target_properties(TgBot::TgBot PROPERTIES IMPORTED_LOCATION_RELEASE "${TGBOT_LIBRARY_RELEASE}")
    endif()
    if (TGBOT_LIBRARY_DEBUG)
      set_target_properties(TgBot::TgBot PROPERTIES IMPORTED_LOCATION_DEBUG "${TGBOT_LIBRARY_DEBUG}")
    endif()
    target_link_libraries(kvartplata_bot PRIVATE TgBot::TgBot)
  else()
    message(FATAL_ERROR "tgbot-cpp не найден в vcpkg (${_VCPKG_PREFIX}). Установи порт tgbot-cpp для текущего триплета.")
  endif()

# 3) Иначе — системный фолбэк (Linux без vcpkg): ищем в стандартных путях /usr и /usr/local
else()
  # include
  find_path(TGBOT_INCLUDE_DIR
    NAMES tgbot/tgbot.h
    PATHS /usr/include /usr/local/include
  )
  # lib
  find_library(TGBOT_LIBRARY
    NAMES TgBot tgbot TgBotd tgbotd
    PATHS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64
  )

  if (TGBOT_INCLUDE_DIR AND TGBOT_LIBRARY)
    add_library(TgBot::TgBot UNKNOWN IMPORTED)
    set_target_properties(TgBot::TgBot PROPERTIES
      IMPORTED_LOCATION "${TGBOT_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${TGBOT_INCLUDE_DIR}"
    )
    target_link_libraries(kvartplata_bot PRIVATE TgBot::TgBot)
  else()
    message(FATAL_ERROR "tgbot-cpp (TgBot) не найден: ни CONFIG-пакет, ни vcpkg, ни системные пути (/usr, /usr/local).")
  endif()
endif()
# ================== /TgBot ==================

# Общая линковка
target_link_libraries(kvartplata_bot PRIVATE
  Threads::Threads
  Boost::system
  OpenSSL::SSL
  OpenSSL::Crypto
)

# Предупреждения
if (MSVC)
  target_compile_options(kvartplata_bot PRIVATE /W4 /permissive-)
else()
  target_compile_options(kvartplata_bot PRIVATE -Wall -Wextra -Wpedantic)
endif()