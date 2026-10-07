$ErrorActionPreference = 'Stop'
$SdkVersion = '2026-09-17'
$Root = Split-Path -Parent $PSScriptRoot
$External = Join-Path $Root 'external'
$SdkRoot = Join-Path $External 'foobar2000-sdk'
if (Test-Path (Join-Path $SdkRoot 'foobar2000\SDK\foobar2000.h')) {
    Write-Host "foobar2000 SDK $SdkVersion already present."
    exit 0
}
New-Item -ItemType Directory -Force -Path $External | Out-Null
$Archive = Join-Path $External "SDK-$SdkVersion.7z"
Invoke-WebRequest -UseBasicParsing -Uri "https://www.foobar2000.org/downloads/SDK-$SdkVersion.7z" -OutFile $Archive
$SevenZip = $null
$SevenZipCommand = Get-Command '7z.exe' -ErrorAction SilentlyContinue
if ($SevenZipCommand) {
    $SevenZip = $SevenZipCommand.Source
} else {
    $Candidate = Join-Path $env:ProgramFiles '7-Zip\7z.exe'
    if (Test-Path $Candidate) { $SevenZip = $Candidate }
}
if (-not $SevenZip) { throw '7-Zip not found.' }
New-Item -ItemType Directory -Force -Path $SdkRoot | Out-Null
& $SevenZip x $Archive "-o$SdkRoot" -y | Out-Host
if ($LASTEXITCODE -ne 0) { throw "7-Zip extraction failed with exit code $LASTEXITCODE" }
if (-not (Test-Path (Join-Path $SdkRoot 'foobar2000\SDK\foobar2000.h'))) {
    throw 'SDK extraction did not produce the expected foobar2000/SDK/foobar2000.h path.'
}
Write-Host "Installed foobar2000 SDK $SdkVersion at $SdkRoot"
