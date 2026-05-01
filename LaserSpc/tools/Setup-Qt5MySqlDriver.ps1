param(
    [switch]$BuildAndInstall
)

$ErrorActionPreference = "Stop"

$qtRoot = "C:\Qt\5.15.2\msvc2019_64"
$qtSourceRoot = "C:\Qt\5.15.2\Src\qtbase"
$mysqlPluginSource = Join-Path $qtSourceRoot "src\plugins\sqldrivers\mysql\qsql_mysql.cpp"
$qmodulePri = Join-Path $qtRoot "mkspecs\qmodule.pri"
$mariaDbShort = "C:\PROGRA~1\MariaDB\MARIAD~1"
$backupSuffix = ".bak-laserspc"

function Backup-FileIfNeeded {
    param([string]$Path)
    $backupPath = $Path + $backupSuffix
    if (-not (Test-Path $backupPath)) {
        Copy-Item $Path $backupPath
    }
}

function Ensure-Contains {
    param(
        [string]$Content,
        [string]$Needle,
        [string]$Replacement
    )
    if ($Content.Contains($Needle)) {
        return $Content
    }
    return $Content.Replace($Replacement, $Replacement + $Needle)
}

if (-not (Test-Path $mysqlPluginSource)) {
    throw "Qt5 qsql_mysql.cpp not found: $mysqlPluginSource"
}

if (-not (Test-Path $qmodulePri)) {
    throw "Qt5 qmodule.pri not found: $qmodulePri"
}

Backup-FileIfNeeded -Path $mysqlPluginSource
Backup-FileIfNeeded -Path $qmodulePri

$mysqlContent = Get-Content $mysqlPluginSource -Raw

if (-not $mysqlContent.Contains('QString pluginDir;')) {
    $mysqlContent = $mysqlContent.Replace(
        "    QString unixSocket;`r`n",
        "    QString unixSocket;`r`n    QString pluginDir;`r`n"
    )
}

if (-not $mysqlContent.Contains('bool sslEnforceOptionSet = false;')) {
    $mysqlContent = $mysqlContent.Replace(
        "    QString sslCipher;`r`n    my_bool reconnect=false;`r`n",
        "    QString sslCipher;`r`n    bool sslEnforceOptionSet = false;`r`n    my_bool sslEnforce = true;`r`n    bool sslVerifyServerCertOptionSet = false;`r`n    my_bool sslVerifyServerCert = true;`r`n    my_bool reconnect=false;`r`n"
    )
}

if (-not $mysqlContent.Contains('else if (opt == QLatin1String("MYSQL_PLUGIN_DIR"))')) {
    $mysqlContent = $mysqlContent.Replace(
        "            if (opt == QLatin1String(""UNIX_SOCKET""))`r`n                unixSocket = val;`r`n",
        "            if (opt == QLatin1String(""UNIX_SOCKET""))`r`n                unixSocket = val;`r`n            else if (opt == QLatin1String(""MYSQL_PLUGIN_DIR""))`r`n                pluginDir = val;`r`n"
    )
}

if (-not $mysqlContent.Contains('else if (opt == QLatin1String("MYSQL_OPT_SSL_ENFORCE"))')) {
    $mysqlContent = $mysqlContent.Replace(
        "            else if (opt == QLatin1String(""SSL_CIPHER""))`r`n                sslCipher = val;`r`n",
        "            else if (opt == QLatin1String(""SSL_CIPHER""))`r`n                sslCipher = val;`r`n            else if (opt == QLatin1String(""MYSQL_OPT_SSL_ENFORCE"")) {`r`n                sslEnforceOptionSet = true;`r`n                sslEnforce = (val == QLatin1String(""TRUE"") || val == QLatin1String(""1"")) ? 1 : 0;`r`n            } else if (opt == QLatin1String(""MYSQL_OPT_SSL_VERIFY_SERVER_CERT"")) {`r`n                sslVerifyServerCertOptionSet = true;`r`n                sslVerifyServerCert = (val == QLatin1String(""TRUE"") || val == QLatin1String(""1"")) ? 1 : 0;`r`n            }`r`n"
    )
}

if (-not $mysqlContent.Contains('if (!pluginDir.isEmpty()) {')) {
    $mysqlContent = $mysqlContent.Replace(
        "#if MYSQL_VERSION_ID >= 50100`r`n    if (connectTimeout != 0)`r`n",
        "#if MYSQL_VERSION_ID >= 50100`r`n    if (!pluginDir.isEmpty()) {`r`n        const QByteArray pluginDirBytes = QFile::encodeName(pluginDir);`r`n        mysql_options(d->mysql, MYSQL_PLUGIN_DIR, pluginDirBytes.constData());`r`n    }`r`n    if (connectTimeout != 0)`r`n"
    )
}

if (-not $mysqlContent.Contains('if (sslEnforceOptionSet)')) {
    $mysqlContent = $mysqlContent.Replace(
        "    if (writeTimeout != 0)`r`n        mysql_options(d->mysql, MYSQL_OPT_WRITE_TIMEOUT, &writeTimeout);`r`n",
        "    if (writeTimeout != 0)`r`n        mysql_options(d->mysql, MYSQL_OPT_WRITE_TIMEOUT, &writeTimeout);`r`n    if (sslEnforceOptionSet)`r`n        mysql_options(d->mysql, MYSQL_OPT_SSL_ENFORCE, &sslEnforce);`r`n    if (sslVerifyServerCertOptionSet)`r`n        mysql_options(d->mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &sslVerifyServerCert);`r`n"
    )
}

Set-Content -Path $mysqlPluginSource -Value $mysqlContent -Encoding UTF8

$qmoduleContent = Get-Content $qmodulePri -Raw
$includeLine = "EXTRA_INCLUDEPATH += $mariaDbShort\include"
$libLine = "EXTRA_LIBDIR += $mariaDbShort\lib"

if (-not $qmoduleContent.Contains($includeLine)) {
    $qmoduleContent = $qmoduleContent.TrimEnd("`r", "`n") + "`r`n" + $includeLine + "`r`n"
}

if (-not $qmoduleContent.Contains($libLine)) {
    $qmoduleContent = $qmoduleContent.TrimEnd("`r", "`n") + "`r`n" + $libLine + "`r`n"
}

Set-Content -Path $qmodulePri -Value $qmoduleContent -Encoding ASCII

Write-Host "Qt5 MySQL driver source patch applied."
Write-Host "Patched file: $mysqlPluginSource"
Write-Host "Patched file: $qmodulePri"
Write-Host "Backup suffix: $backupSuffix"

if ($BuildAndInstall) {
    & "D:\Program\spc\LaserSpc\tools\build-qmysql-qt5.cmd" install
}
