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
    src/proxy_handler.cpp
    src/config.cpp
    src/logger.cpp
    src/utils.cpp
)

# Заголовочные файлы
set(HEADERS
    src/vpn_server.h
    src/proxy_handler.h
    src/config.h
    src/logger.h
    src/utils.h
)

# Создание исполняемого файла
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Создание тестового клиента
add_executable(test-client test_client.cpp)

# Линковка библиотек
target_link_libraries(${PROJECT_NAME} 
    Threads::Threads
)

target_link_libraries(test-client 
    Threads::Threads
)

# Настройки для Linux
if(UNIX AND NOT APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE LINUX_BUILD)
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