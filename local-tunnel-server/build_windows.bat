@echo off
echo ================================
echo  Local Tunnel Server - Windows Build
echo ================================
echo.

REM Проверка существования CMake
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ОШИБКА: CMake не найден в PATH!
    echo Установите CMake: https://cmake.org/download/
    pause
    exit /b 1
)

REM Создание директории сборки
if not exist build (
    echo Создание директории build...
    mkdir build
)

cd build

echo Генерация проекта Visual Studio...
REM Попробуем различные версии Visual Studio
cmake -G "Visual Studio 17 2022" -A x64 .. >nul 2>&1
if %errorlevel% neq 0 (
    echo Пробуем Visual Studio 2019...
    cmake -G "Visual Studio 16 2019" -A x64 .. >nul 2>&1
    if %errorlevel% neq 0 (
        echo Пробуем MinGW Makefiles...
        cmake -G "MinGW Makefiles" ..
        if %errorlevel% neq 0 (
            echo ОШИБКА: Не удалось сгенерировать проект!
            pause
            exit /b 1
        )
        set BUILD_TYPE=MinGW
    ) else (
        set BUILD_TYPE=VS2019
    )
) else (
    set BUILD_TYPE=VS2022
)

echo Найден генератор: %BUILD_TYPE%
echo.
echo Начинаем сборку...

if "%BUILD_TYPE%"=="MinGW" (
    mingw32-make -j%NUMBER_OF_PROCESSORS%
) else (
    cmake --build . --config Release
)

if %errorlevel% neq 0 (
    echo.
    echo ОШИБКА СБОРКИ!
    pause
    exit /b 1
)

echo.
echo ================================
echo  СБОРКА ЗАВЕРШЕНА УСПЕШНО!
echo ================================
echo.
echo Созданные файлы:
if "%BUILD_TYPE%"=="MinGW" (
    if exist local-tunnel-server.exe echo - local-tunnel-server.exe
    if exist test-client.exe echo - test-client.exe
    if exist encryption_plugins\libxor_encryption.dll echo - encryption_plugins\libxor_encryption.dll
    if exist encryption_plugins\libcaesar_encryption.dll echo - encryption_plugins\libcaesar_encryption.dll
) else (
    if exist Release\local-tunnel-server.exe echo - Release\local-tunnel-server.exe
    if exist Release\test-client.exe echo - Release\test-client.exe
    if exist encryption_plugins\Release\xor_encryption.dll echo - encryption_plugins\Release\xor_encryption.dll
    if exist encryption_plugins\Release\caesar_encryption.dll echo - encryption_plugins\Release\caesar_encryption.dll
)
echo.

REM Копирование конфигурации
if not exist ..\config_windows.json (
    echo Создание файла конфигурации...
    copy ..\config.json ..\config_windows.json >nul
)

echo Для запуска используйте:
if "%BUILD_TYPE%"=="MinGW" (
    echo   cd .. ^&^& build\local-tunnel-server.exe
) else (
    echo   cd .. ^&^& build\Release\local-tunnel-server.exe
)
echo.
pause