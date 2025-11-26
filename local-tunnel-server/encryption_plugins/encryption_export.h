#ifndef ENCRYPTION_EXPORT_H
#define ENCRYPTION_EXPORT_H

/**
 * Макросы для экспорта символов в динамических библиотеках
 * Поддерживает как Windows DLL, так и Unix/Linux .so
 */

#ifdef _WIN32
    // Windows DLL export/import
    #ifdef BUILDING_DLL
        #define ENCRYPTION_API __declspec(dllexport)
    #else
        #define ENCRYPTION_API __declspec(dllimport)
    #endif
    #define ENCRYPTION_CALL __cdecl
#else
    // Unix/Linux shared library
    #define ENCRYPTION_API __attribute__((visibility("default")))
    #define ENCRYPTION_CALL
#endif

#endif // ENCRYPTION_EXPORT_H