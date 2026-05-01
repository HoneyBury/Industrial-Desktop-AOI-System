param(
    [string]$BuildDir = (Join-Path $PSScriptRoot "..\build-msvc-qt5"),
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Debug",
    [string]$LocalHost = "127.0.0.1",
    [int]$LocalPort = 3306,
    [string]$LocalRootUser = "root",
    [string]$LocalRootPassword = "zjh123456",
    [string]$MySql55Cli = "C:\Program Files\MySQL\MySQL Server 5.5\bin\mysql.exe",
    [switch]$BuildFirst,
    [switch]$CleanupAfter
)

$ErrorActionPreference = "Stop"

function Write-Section {
    param([string]$Text)
    Write-Host ""
    Write-Host ("=== " + $Text + " ===")
}

function Require-Path {
    param(
        [string]$PathValue,
        [string]$Message
    )

    if (-not (Test-Path $PathValue)) {
        throw ($Message + ": " + $PathValue)
    }
}

function Invoke-Checked {
    param(
        [scriptblock]$Action,
        [string]$FailureMessage
    )

    & $Action
    if ($LASTEXITCODE -ne 0) {
        throw ($FailureMessage + " (exit=" + $LASTEXITCODE + ")")
    }
}

function Invoke-ProcessCapture {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [hashtable]$EnvironmentVariables = @{},
        [string]$WorkingDirectory = ""
    )

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    if (-not [string]::IsNullOrWhiteSpace($WorkingDirectory)) {
        $psi.WorkingDirectory = $WorkingDirectory
    }
    $escapedArguments = foreach ($argument in $Arguments) {
        if ($argument -match '[\s"]') {
            '"' + ($argument -replace '"', '\"') + '"'
        } else {
            $argument
        }
    }
    $psi.Arguments = ($escapedArguments -join " ")
    foreach ($key in $EnvironmentVariables.Keys) {
        $psi.Environment[$key] = [string]$EnvironmentVariables[$key]
    }

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()
    $stdOut = $process.StandardOutput.ReadToEnd()
    $stdErr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()

    $outputLines = New-Object System.Collections.Generic.List[string]
    foreach ($chunk in @($stdOut, $stdErr)) {
        if ([string]::IsNullOrWhiteSpace($chunk)) {
            continue
        }
        foreach ($line in ($chunk -split "`r?`n")) {
            if (-not [string]::IsNullOrWhiteSpace($line)) {
                $outputLines.Add($line)
            }
        }
    }

    [PSCustomObject]@{
        ExitCode = $process.ExitCode
        Lines = $outputLines
    }
}

function Invoke-DbInit {
    param(
        [string]$ExePath,
        [string]$TargetHost,
        [int]$Port,
        [string]$AdminUser,
        [string]$AdminPassword,
        [string]$DbName,
        [string]$AppUser,
        [string]$AppPassword,
        [bool]$SeedEnabled
    )

    $result = Invoke-ProcessCapture -FilePath $ExePath `
                                    -Arguments @() `
                                    -WorkingDirectory (Split-Path $ExePath -Parent) `
                                    -EnvironmentVariables @{
                                        LASERSPC_DB_HOST = $TargetHost
                                        LASERSPC_DB_PORT = $Port.ToString()
                                        LASERSPC_ADMIN_DB = "mysql"
                                        LASERSPC_ADMIN_DB_USER = $AdminUser
                                        LASERSPC_ADMIN_DB_PASSWORD = $AdminPassword
                                        LASERSPC_DB_NAME = $DbName
                                        LASERSPC_DB_USER = $AppUser
                                        LASERSPC_DB_PASSWORD = $AppPassword
                                        LASERSPC_DB_SEED = $(if ($SeedEnabled) { "1" } else { "0" })
                                        LASERSPC_DBINIT_NO_DIALOG = "1"
                                        QT_QPA_PLATFORM = "offscreen"
                                    }
    $result.Lines | ForEach-Object { Write-Host $_ }
    if ($result.ExitCode -ne 0) {
        throw ("LaserSpcDbInit failed for " + $TargetHost + ":" + $Port + "/" + $DbName)
    }
}

function Invoke-MySql55 {
    param(
        [string]$CliPath,
        [string]$TargetHost,
        [int]$Port,
        [string]$User,
        [string]$Password,
        [string]$Database,
        [string]$Sql
    )

    $args = @(
        "--protocol=tcp",
        "--host=$TargetHost",
        "--port=$Port",
        "--user=$User",
        "--password=$Password",
        "--batch",
        "--raw"
    )
    if (-not [string]::IsNullOrWhiteSpace($Database)) {
        $args += "--database=$Database"
    }
    $args += "-e"
    $args += $Sql

    $result = Invoke-ProcessCapture -FilePath $CliPath -Arguments $args
    $result.Lines | ForEach-Object { Write-Host $_ }
    if ($result.ExitCode -ne 0) {
        throw ("mysql.exe failed for " + $TargetHost + ":" + $Port + " user=" + $User)
    }
}

function Remove-VerificationDatabases {
    param(
        [string]$CliPath,
        [string]$TargetHost,
        [int]$Port,
        [string]$RootUser,
        [string]$RootPassword,
        [string[]]$Databases,
        [string[]]$Users
    )

    $statements = New-Object System.Collections.Generic.List[string]
    foreach ($db in $Databases) {
        $statements.Add("DROP DATABASE IF EXISTS ``$db``")
    }
    foreach ($user in $Users) {
        $statements.Add("DROP USER '$user'@'localhost'")
        $statements.Add("DROP USER '$user'@'%'")
    }
    $statements.Add("FLUSH PRIVILEGES")
    Invoke-MySql55 -CliPath $CliPath -TargetHost $TargetHost -Port $Port -User $RootUser -Password $RootPassword -Database "" -Sql ($statements -join "; ")
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildDir = (Resolve-Path $BuildDir).Path
$dbInitExe = Join-Path $BuildDir "$Config\LaserSpcDbInit.exe"
$reportPath = Join-Path $BuildDir ("dbinit-regression-" + (Get-Date -Format "yyyyMMdd-HHmmss") + ".txt")

$localCases = @(
    @{ Db = "laser_spc55_a"; User = "lspc55a"; Password = "Verify55A#2026"; Seed = $true },
    @{ Db = "laser_spc55_b"; User = "lspc55b"; Password = "Verify55B#2026"; Seed = $false }
)
$report = New-Object System.Collections.Generic.List[string]
$report.Add("[INFO] RepoRoot=$repoRoot")
$report.Add("[INFO] BuildDir=$BuildDir")
$report.Add("[INFO] Config=$Config")
$report.Add("[INFO] LocalTarget=$LocalHost`:$LocalPort")

Require-Path -PathValue $dbInitExe -Message "LaserSpcDbInit.exe not found"
Require-Path -PathValue $MySql55Cli -Message "MySQL 5.5 CLI not found"

if ($BuildFirst) {
    Write-Section "Build"
    Invoke-Checked -FailureMessage "Qt5 build failed" -Action {
        cmake --build $BuildDir --config $Config --target LaserSpcDbInit
    }
    $report.Add("[OK] Build completed")
}

Write-Section "Local MySQL 5.5"
foreach ($case in $localCases) {
    $report.Add("[INFO] Init local case db=$($case.Db) user=$($case.User) seed=$($case.Seed)")
    Invoke-DbInit -ExePath $dbInitExe `
                  -TargetHost $LocalHost `
                  -Port $LocalPort `
                  -AdminUser $LocalRootUser `
                  -AdminPassword $LocalRootPassword `
                  -DbName $case.Db `
                  -AppUser $case.User `
                  -AppPassword $case.Password `
                  -SeedEnabled $case.Seed
}
Invoke-MySql55 -CliPath $MySql55Cli `
               -TargetHost $LocalHost `
               -Port $LocalPort `
               -User $LocalRootUser `
               -Password $LocalRootPassword `
               -Database "" `
               -Sql "SELECT User,Host FROM mysql.user WHERE User IN ('lspc55a','lspc55b'); SELECT table_schema, COUNT(*) AS table_count FROM information_schema.tables WHERE table_schema IN ('laser_spc55_a','laser_spc55_b') GROUP BY table_schema ORDER BY table_schema;"
Invoke-MySql55 -CliPath $MySql55Cli `
               -TargetHost $LocalHost `
               -Port $LocalPort `
               -User "lspc55a" `
               -Password "Verify55A#2026" `
               -Database "laser_spc55_a" `
               -Sql "SELECT COUNT(*) AS board_count FROM board_records;"
Invoke-MySql55 -CliPath $MySql55Cli `
               -TargetHost $LocalHost `
               -Port $LocalPort `
               -User "lspc55b" `
               -Password "Verify55B#2026" `
               -Database "laser_spc55_b" `
               -Sql "SHOW TABLES;"
$report.Add("[OK] Local MySQL 5.5 regression checks passed")

if ($CleanupAfter) {
    Write-Section "Cleanup"
    Remove-VerificationDatabases -CliPath $MySql55Cli `
                                 -TargetHost $LocalHost `
                                 -Port $LocalPort `
                                 -RootUser $LocalRootUser `
                                 -RootPassword $LocalRootPassword `
                                 -Databases @("laser_spc55_a", "laser_spc55_b") `
                                 -Users @("lspc55a", "lspc55b")
    $report.Add("[OK] Cleanup completed")
}

$report | Set-Content -Path $reportPath -Encoding UTF8
Write-Section "Done"
Write-Host ("Report saved to " + $reportPath)
