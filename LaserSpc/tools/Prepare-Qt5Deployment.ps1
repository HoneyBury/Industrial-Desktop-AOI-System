param(
    [string]$BuildDir = (Join-Path $PSScriptRoot "..\build-msvc-qt5"),
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",
    [string]$OutputDir = "",
    [string]$QtBinDir = "C:\Qt\5.15.2\msvc2019_64\bin",
    [string]$MariaDbLibDir = "C:\Program Files\MariaDB\MariaDB Connector C 64-bit\lib",
    [string]$ExampleConfigFile = (Join-Path $PSScriptRoot "..\config\laserspc.windows.qt5.example.ini")
)

$ErrorActionPreference = "Stop"

function Resolve-OutputDir {
    param(
        [string]$BuildDirValue,
        [string]$ConfigValue,
        [string]$OutputDirValue
    )

    if ([string]::IsNullOrWhiteSpace($OutputDirValue)) {
        return (Join-Path $BuildDirValue ("deploy-qt5\" + $ConfigValue))
    }

    return $OutputDirValue
}

function Require-Path {
    param(
        [string]$PathValue,
        [string]$Message
    )

    if (-not (Test-Path $PathValue)) {
        throw $Message + ": " + $PathValue
    }
}

function Copy-IfExists {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (Test-Path $Source) {
        Copy-Item -Path $Source -Destination $Destination -Force
        return $true
    }

    return $false
}

function Add-ReportLine {
    param(
        [System.Collections.Generic.List[string]]$Report,
        [string]$Status,
        [string]$Message
    )

    $Report.Add("[$Status] $Message")
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildDir = (Resolve-Path $BuildDir).Path
$OutputDir = Resolve-OutputDir -BuildDirValue $BuildDir -ConfigValue $Config -OutputDirValue $OutputDir
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)

$appExe = Join-Path $BuildDir "$Config\LaserSpc.exe"
if (-not (Test-Path $appExe)) {
    $appExe = Join-Path $BuildDir "LaserSpc.exe"
}

$dbInitExe = Join-Path $BuildDir "$Config\LaserSpcDbInit.exe"
$perfToolExe = Join-Path $BuildDir "$Config\LaserSpcPerfTool.exe"
$windeployqt = Join-Path $QtBinDir "windeployqt.exe"
$qtPluginsDir = Join-Path (Split-Path $QtBinDir -Parent) "plugins"
$sqldriversDir = Join-Path $OutputDir "sqldrivers"
$configDir = Join-Path $OutputDir "config"
$reportPath = Join-Path $OutputDir "deployment-check.txt"

Require-Path -PathValue $appExe -Message "LaserSpc.exe not found"
Require-Path -PathValue $windeployqt -Message "windeployqt.exe not found"
Require-Path -PathValue $MariaDbLibDir -Message "MariaDB Connector/C lib directory not found"

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path $configDir | Out-Null

Copy-Item -Path $appExe -Destination (Join-Path $OutputDir "LaserSpc.exe") -Force
Copy-IfExists -Source $dbInitExe -Destination (Join-Path $OutputDir "LaserSpcDbInit.exe") | Out-Null
Copy-IfExists -Source $perfToolExe -Destination (Join-Path $OutputDir "LaserSpcPerfTool.exe") | Out-Null

& $windeployqt --dir $OutputDir --compiler-runtime --no-translations (Join-Path $OutputDir "LaserSpc.exe")

$mariaDbDll = Join-Path $MariaDbLibDir "libmariadb.dll"
Require-Path -PathValue $mariaDbDll -Message "libmariadb.dll not found"
Copy-Item -Path $mariaDbDll -Destination (Join-Path $OutputDir "libmariadb.dll") -Force

New-Item -ItemType Directory -Force -Path $sqldriversDir | Out-Null
$pluginCandidates = @(
    (Join-Path $qtPluginsDir "sqldrivers\qsqlmysql.dll"),
    (Join-Path $qtPluginsDir "sqldrivers\qsqlmysqld.dll"),
    (Join-Path $BuildDir "$Config\sqldrivers\qsqlmysql.dll"),
    (Join-Path $BuildDir "$Config\sqldrivers\qsqlmysqld.dll")
)

$pluginCopied = $false
foreach ($candidate in $pluginCandidates) {
    if (Copy-IfExists -Source $candidate -Destination (Join-Path $sqldriversDir ([System.IO.Path]::GetFileName($candidate)))) {
        $pluginCopied = $true
    }
}

$targetConfigFile = Join-Path $configDir "laserspc.ini"
if (-not (Test-Path $targetConfigFile) -and (Test-Path $ExampleConfigFile)) {
    Copy-Item -Path $ExampleConfigFile -Destination $targetConfigFile -Force
}

$report = New-Object 'System.Collections.Generic.List[string]'
Add-ReportLine -Report $report -Status "INFO" -Message ("BuildDir=" + $BuildDir)
Add-ReportLine -Report $report -Status "INFO" -Message ("Config=" + $Config)
Add-ReportLine -Report $report -Status "INFO" -Message ("OutputDir=" + $OutputDir)
Add-ReportLine -Report $report -Status "INFO" -Message ("QtBinDir=" + $QtBinDir)
Add-ReportLine -Report $report -Status "INFO" -Message ("MariaDbLibDir=" + $MariaDbLibDir)

$checks = @(
    @{ Name = "LaserSpc.exe"; Paths = @((Join-Path $OutputDir "LaserSpc.exe")) },
    @{ Name = "Qt core runtime"; Paths = @((Join-Path $OutputDir "Qt5Core.dll"), (Join-Path $OutputDir "Qt5Cored.dll")) },
    @{ Name = "Windows platform plugin"; Paths = @((Join-Path $OutputDir "platforms\qwindows.dll"), (Join-Path $OutputDir "platforms\qwindowsd.dll")) },
    @{ Name = "MySQL plugin release"; Paths = @((Join-Path $sqldriversDir "qsqlmysql.dll")) },
    @{ Name = "MySQL plugin debug"; Paths = @((Join-Path $sqldriversDir "qsqlmysqld.dll")) },
    @{ Name = "MariaDB client dll"; Paths = @((Join-Path $OutputDir "libmariadb.dll")) },
    @{ Name = "Runtime config"; Paths = @($targetConfigFile) }
)

foreach ($check in $checks) {
    $existingPath = $null
    foreach ($candidate in $check.Paths) {
        if (Test-Path $candidate) {
            $existingPath = $candidate
            break
        }
    }
    if ($check.Name -like "MySQL plugin*") {
        continue
    }

    $status = "FAIL"
    if ($null -ne $existingPath) {
        $status = "OK"
    }
    $displayPath = $check.Paths -join " | "
    if ($null -ne $existingPath) {
        $displayPath = $existingPath
    }
    Add-ReportLine -Report $report -Status $status -Message ($check.Name + " => " + $displayPath)
}

$mysqlPluginPresent = (Test-Path (Join-Path $sqldriversDir "qsqlmysql.dll")) -or (Test-Path (Join-Path $sqldriversDir "qsqlmysqld.dll"))
$mysqlPluginStatus = "FAIL"
if ($mysqlPluginPresent) {
    $mysqlPluginStatus = "OK"
}
Add-ReportLine -Report $report -Status $mysqlPluginStatus -Message ("MySQL plugin present => " + $sqldriversDir)

if (-not $pluginCopied) {
    Add-ReportLine -Report $report -Status "WARN" -Message "No qsqlmysql plugin was copied from the known candidate paths."
}

$report | Set-Content -Path $reportPath -Encoding UTF8
$report | ForEach-Object { Write-Host $_ }
Write-Host ("Deployment report saved to " + $reportPath)
