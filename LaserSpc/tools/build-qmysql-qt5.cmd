@echo off
setlocal

set "DO_INSTALL=0"
if /I "%~1"=="install" set "DO_INSTALL=1"

set "QT_DIR=C:\Qt\5.15.2\msvc2019_64"
set "QT_SRC_DIR=C:\Qt\5.15.2\Src\qtbase\src\plugins\sqldrivers"
set "VS_VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "MARIADB_DIR=C:\PROGRA~1\MariaDB\MARIAD~1"
set "BUILD_DIR=D:\Program\spc\build-qsqlmysql-qt5"

if not exist "%QT_DIR%\bin\qmake.exe" (
    echo qmake not found: %QT_DIR%\bin\qmake.exe
    exit /b 1
)

if not exist "%QT_SRC_DIR%\sqldrivers.pro" (
    echo sqldrivers.pro not found: %QT_SRC_DIR%\sqldrivers.pro
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

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"
if errorlevel 1 exit /b 1

cd /d "%BUILD_DIR%"

echo Configuring Qt5 MySQL driver with MariaDB Connector/C...
"%QT_DIR%\bin\qmake.exe" "%QT_SRC_DIR%\sqldrivers.pro" ^
    "MYSQL_PATH=%MARIADB_DIR%" ^
    "MYSQL_LIBS=-L%MARIADB_DIR%\lib -llibmariadb"
if errorlevel 1 exit /b 1

echo Building qsqlmysql plugin...
nmake sub-mysql
if errorlevel 1 exit /b 1

if "%DO_INSTALL%"=="1" (
    echo Installing qsqlmysql plugin into %QT_DIR%\plugins\sqldrivers ...
    nmake install
    if errorlevel 1 exit /b 1
)

echo Build finished.
echo Output folder: %BUILD_DIR%\plugins\sqldrivers
echo MariaDB client DLL: %MARIADB_DIR%\lib\libmariadb.dll
exit /b 0
