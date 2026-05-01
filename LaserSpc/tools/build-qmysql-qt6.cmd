@echo off
setlocal

set "QT_DIR=C:\Qt\6.9.2\msvc2022_64"
set "QT_SRC_DIR=C:\Qt\6.9.2\Src\qtbase\src\plugins\sqldrivers"
set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set "MARIADB_DIR=C:\PROGRA~1\MariaDB\MARIAD~1"
set "BUILD_DIR=D:\Program\spc\build-qsqlmysql-qt6"

if not exist "%QT_DIR%\bin\qt-cmake.bat" (
    echo qt-cmake not found: %QT_DIR%\bin\qt-cmake.bat
    exit /b 1
)

if not exist "%QT_SRC_DIR%\CMakeLists.txt" (
    echo sqldrivers CMakeLists.txt not found: %QT_SRC_DIR%\CMakeLists.txt
    exit /b 1
)

if not exist "%MARIADB_DIR%\include\mysql.h" (
    echo mysql.h not found: %MARIADB_DIR%\include\mysql.h
    exit /b 1
)

if not exist "%MARIADB_DIR%\lib\libmariadb.lib" (
    echo libmariadb.lib not found: %MARIADB_DIR%\lib\libmariadb.lib
    exit /b 1
)

call "%VS_VCVARS%"
if errorlevel 1 exit /b 1

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if errorlevel 1 exit /b 1

echo Configuring Qt6 MySQL driver with MariaDB Connector/C...
"%QT_DIR%\bin\qt-cmake.bat" -S "%QT_SRC_DIR%" -B "%BUILD_DIR%" ^
    -G "Visual Studio 17 2022" -A x64 ^
    -DMySQL_ROOT="%MARIADB_DIR%" ^
    -DCMAKE_INSTALL_PREFIX="%QT_DIR%"
if errorlevel 1 exit /b 1

echo Building release plugin...
cmake --build "%BUILD_DIR%" --config RelWithDebInfo --target QMYSQLDriverPlugin
if errorlevel 1 exit /b 1

echo Building debug plugin...
cmake --build "%BUILD_DIR%" --config Debug --target QMYSQLDriverPlugin
if errorlevel 1 exit /b 1

echo Build finished.
echo Output folder: %BUILD_DIR%\plugins\sqldrivers
echo Runtime DLL: %MARIADB_DIR%\lib\libmariadb.dll
exit /b 0
