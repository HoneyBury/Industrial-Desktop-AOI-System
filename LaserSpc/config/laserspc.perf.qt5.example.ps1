$env:LASERSPC_DB_HOST = "127.0.0.1"
$env:LASERSPC_DB_PORT = "9527"
$env:LASERSPC_DB_CONNECT_OPTIONS = "MYSQL_OPT_SSL_ENFORCE=0;MYSQL_OPT_SSL_VERIFY_SERVER_CERT=0;MYSQL_PLUGIN_DIR=C:/Program Files/MariaDB/MariaDB Connector C 64-bit/lib/plugin"

$env:LASERSPC_ADMIN_DB = "mysql"
$env:LASERSPC_ADMIN_USER = "root"
$env:LASERSPC_ADMIN_PASSWORD = "zjh123456"

$env:LASERSPC_TARGET_USER = "root"
$env:LASERSPC_TARGET_PASSWORD = "zjh123456"
$env:LASERSPC_PERF_DB_NAME = "laser_spc_perf"

$env:PATH = "C:\Qt\5.15.2\msvc2019_64\bin;C:\Program Files\MariaDB\MariaDB Connector C 64-bit\lib;$env:PATH"

# Example:
# . .\config\laserspc.perf.qt5.example.ps1
# .\build-msvc-qt5\Debug\LaserSpcPerfTool.exe --mode all --db-name laser_spc_perf --boards 5000 --points-per-board 4 --rounds 5 --top-n 10
