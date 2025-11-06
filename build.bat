@echo off
REM Build script for Remote Console Application
REM This script builds the project using MinGW-w64 or MSVC

echo ========================================
echo Remote Console Application Build Script
echo ========================================
echo.

REM Check if MinGW is available
where gcc >nul 2>&1
if %errorlevel% == 0 (
    echo Found GCC compiler
    goto :build_mingw
)

REM Check if MSVC is available
where cl >nul 2>&1
if %errorlevel% == 0 (
    echo Found MSVC compiler
    goto :build_msvc
)

echo ERROR: No compiler found!
echo Please install MinGW-w64 or Visual Studio
exit /b 1

:build_mingw
echo.
echo Building with MinGW-w64...
echo.

REM Build main C application
echo Compiling my.c...
gcc -Wall -O2 -c my.c -o my.o
if %errorlevel% neq 0 (
    echo Compilation failed!
    exit /b 1
)

echo Linking my.exe...
gcc -o my.exe my.o -lws2_32 -ladvapi32
if %errorlevel% neq 0 (
    echo Linking failed!
    exit /b 1
)

echo.
echo Build successful: my.exe
echo.

REM Build C++ wrapper example
echo Building C++ wrapper example...
g++ -Wall -O2 -std=c++11 -c process_wrapper.cpp -o process_wrapper.o
g++ -Wall -O2 -std=c++11 -c process_wrapper_example.cpp -o process_wrapper_example.o
g++ -o process_wrapper_example.exe process_wrapper_example.o process_wrapper.o -lws2_32
echo C++ example built: process_wrapper_example.exe
echo.

goto :end

:build_msvc
echo.
echo Building with MSVC...
echo.

REM Build main C application
echo Compiling my.c...
cl /nologo /W3 /O2 /c my.c
if %errorlevel% neq 0 (
    echo Compilation failed!
    exit /b 1
)

echo Linking my.exe...
link /nologo /OUT:my.exe my.obj ws2_32.lib advapi32.lib
if %errorlevel% neq 0 (
    echo Linking failed!
    exit /b 1
)

echo.
echo Build successful: my.exe
echo.

REM Build C++ wrapper example
echo Building C++ wrapper example...
cl /nologo /W3 /O2 /EHsc /c process_wrapper.cpp
cl /nologo /W3 /O2 /EHsc /c process_wrapper_example.cpp
link /nologo /OUT:process_wrapper_example.exe process_wrapper_example.obj process_wrapper.obj ws2_32.lib
echo C++ example built: process_wrapper_example.exe
echo.

:end
echo ========================================
echo Build complete!
echo ========================================
exit /b 0
