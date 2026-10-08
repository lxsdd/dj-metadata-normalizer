#requires -Version 5.1
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Join-Path $env:TEMP ('djmeta-script-selftest-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
try {
    $source = Join-Path $root 'original.mp3'
    [IO.File]::WriteAllBytes($source, [byte[]](0x49, 0x44, 0x33, 0, 0, 0, 0, 0))
    $sourceBefore = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    $sourceTime = (Get-Item -LiteralPath $source).LastWriteTimeUtc.Ticks
    $samples = Join-Path $root 'disposable'
    & "$PSScriptRoot/../scripts/host-acceptance-noop.ps1" -Mode Prepare -SampleFile $source -TestFolder $samples
    if (-not (Test-Path -LiteralPath (Join-Path $samples 'BASELINE.json'))) {
        throw 'Prepare failed to create a baseline.'
    }
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne $sourceBefore -or
        (Get-Item -LiteralPath $source).LastWriteTimeUtc.Ticks -ne $sourceTime) {
        throw 'Prepare modified its original sample MP3.'
    }
    & "$PSScriptRoot/../scripts/host-acceptance-noop.ps1" -Mode Verify -TestFolder $samples
    if ((Get-Content -LiteralPath (Join-Path $samples 'HOST-RESULT.txt') -Raw) -notmatch 'FILE_INTEGRITY=PASS') {
        throw 'Unchanged test samples did not pass verification.'
    }
    Add-Content -LiteralPath (Join-Path $samples 'sample-a.mp3') -Value 'deliberate mutation'
    $failedAsExpected = $false
    try {
        & "$PSScriptRoot/../scripts/host-acceptance-noop.ps1" -Mode Verify -TestFolder $samples
    } catch {
        if ($_.Exception.Message -like '*File integrity verification failed*') {
            $failedAsExpected = $true
        } else {
            throw
        }
    }
    if (-not $failedAsExpected) {
        throw 'A modified media file was not rejected by verification.'
    }
    if ((Get-Content -LiteralPath (Join-Path $samples 'HOST-RESULT.txt') -Raw) -notmatch 'FILE_INTEGRITY=FAIL') {
        throw 'Failure report did not record changed media.'
    }
    Write-Host 'PASS: isolated host gate prepares copies, preserves original and detects media writes.'
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
