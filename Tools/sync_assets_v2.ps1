# sync_assets.ps1
# Copies downloaded author assets (sounds + textures) into project RawAssets/.
$ErrorActionPreference = "Continue"

$soundsSrc  = "C:\Users\dan22\OneDrive\Desktop\conent\steps"
$textureSrc = "C:\Users\dan22\OneDrive\Desktop\conent\texture"

$rawRoot    = "C:\D.A.R.K\DARK\RawAssets"
$soundsDst  = Join-Path $rawRoot "Sounds"
$loopsDst   = Join-Path $rawRoot "Sounds\Loops"
$texDst     = Join-Path $rawRoot "Textures"
$meshDst    = Join-Path $rawRoot "Meshes"

New-Item -ItemType Directory -Force -Path $soundsDst, $loopsDst, $texDst, $meshDst | Out-Null

Write-Host "=== Sounds ==="
$loopPattern = 'loop|ambient|atmos|roomtone|room-tone|room_tone|room tone|siren|drone|rain|wind-in-trees|cricket'
$copiedSounds = 0
Get-ChildItem $soundsSrc -File | ForEach-Object {
    $dest = if ($_.Name -match $loopPattern) { $loopsDst } else { $soundsDst }
    $target = Join-Path $dest $_.Name
    if (-not (Test-Path $target)) {
        Copy-Item $_.FullName -Destination $dest
        $copiedSounds++
    }
}
Write-Host "Sounds copied: $copiedSounds"

Write-Host "=== Textures/meshes (archives) ==="
$extracted = 0
$skipped = 0
Get-ChildItem $textureSrc -Filter *.zip | ForEach-Object {
    $base = $_.BaseName -replace ' \(1\)$', ''
    $destRoot = if ($base -match 'satellite|radio-telescope|votv') { $meshDst } else { $texDst }
    $target = Join-Path $destRoot $base
    if (Test-Path $target) {
        $skipped++
    } else {
        try {
            Expand-Archive -Path $_.FullName -DestinationPath $target -Force
            $extracted++
        } catch {
            Write-Host "EXTRACT ERROR $($_.Name): $($_.Exception.Message)"
        }
    }
}
Write-Host "Archives extracted: $extracted, skipped (already present): $skipped"
Write-Host "=== DONE ==="
