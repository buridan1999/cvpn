cmake_minimum_required(VERSION 3.16)
project(local-tunnel-server VERSION 1.0.0 LANGUAGES CXX)

# Стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Флаги компиляции
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Настройка типа сборки по умолчанию
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Поиск библиотек
find_package(Threads REQUIRED)

# Включение директорий
include_directories(src)

# Исходные файлы
set(SOURCES
    src/main.cpp
    src/vpn_server.cpp
    src/tunnel_server.cpp
    src/proxy_handler.cpp
    src/tunnel_handler.cpp
    src/encryption_manager.cpp
    src/config.cpp
    src/logger.cpp
    src/utils.cpp
)

# Заголовочные файлы
set(HEADERS
    src/vpn_server.h
    src/tunnel_server.h
    src/proxy_handler.h
    src/tunnel_handler.h
    src/encryption_interface.h
    src/encryption_manager.h
    src/config.h
    src/logger.h
    src/utils.h
)

# Создание исполняемого файла
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Создание тестового клиента
add_executable(test-client test_client.cpp)

# Линковка библиотек
if(WIN32)
    target_link_libraries(${PROJECT_NAME} 
        Threads::Threads
        ws2_32  # Для Windows Sockets
    )
else()
    target_link_libraries(${PROJECT_NAME} 
        Threads::Threads
        dl  # Для dlopen/dlclose на Linux
    )
endif()

target_link_libraries(test-client 
    Threads::Threads
)
if(WIN32)
    target_link_libraries(test-client ws2_32)
endif()

# Создание динамических библиотек шифрования
add_library(xor_encryption SHARED encryption_plugins/xor_encryption.cpp)
add_library(caesar_encryption SHARED encryption_plugins/caesar_encryption.cpp)

# Настройки для различных платформ
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Настройки для Windows (нативная сборка или кросс-компиляция)
    set_target_properties(xor_encryption PROPERTIES
        PREFIX ""
        OUTPUT_NAME "xor_encryption"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/encryption_plugins"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/encryption_plugins"
        SUFFIX ".dll"
    )
    
    set_target_properties(caesar_encryption PROPERTIES
        PREFIX ""
        OUTPUT_NAME "caesar_encryption"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/encryption_plugins"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/encryption_plugins"
        SUFFIX ".dll"
    )
    
    # Экспорт символов для Windows DLL
    target_compile_definitions(xor_encryption PRIVATE BUILDING_DLL)
    target_compile_definitions(caesar_encryption PRIVATE BUILDING_DLL)
    
    # Статическая линковка при кросс-компиляции
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(xor_encryption -static-libgcc -static-libstdc++)
        target_link_libraries(caesar_encryption -static-libgcc -static-libstdc++)
    endif()
else()
    # Настройки для Unix/Linux
    set_target_properties(xor_encryption PROPERTIES
        PREFIX ""
        OUTPUT_NAME "xor_encryption"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/encryption_plugins"
    )
    
    set_target_properties(caesar_encryption PROPERTIES
        PREFIX ""
        OUTPUT_NAME "caesar_encryption"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/encryption_plugins"
    )
endif()

# Настройки для Linux
if(UNIX AND NOT APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE LINUX_BUILD)
endif()

# Специальные настройки для кросс-компиляции Windows под Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Настройка кросс-компиляции Windows под Linux")
    
    # Статическая линковка для портативности
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    
    # Убеждаемся что используется правильный набор библиотек
    target_link_libraries(${PROJECT_NAME} ws2_32 winpthread)
endif()

# Установка
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES config.json DESTINATION etc/${PROJECT_NAME})

# Опции для разработки
option(BUILD_TESTS "Build tests" OFF)
option(ENABLE_DEBUG_LOGGING "Enable debug logging" OFF)

if(ENABLE_DEBUG_LOGGING)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG_LOGGING)
endif()

# Тесты (если включены)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()