param(
    [Parameter(Mandatory = $true)][string]$OriginalMap,
    [Parameter(Mandatory = $true)][string]$GeneratedCleanMap,
    [Parameter(Mandatory = $true)][string]$RouteOverlay,
    [Parameter(Mandatory = $true)][string]$TurnOverlay,
    [Parameter(Mandatory = $true)][string]$PinOverlay,
    [Parameter(Mandatory = $true)][string]$ArrowOverlay,
    [Parameter(Mandatory = $true)][string]$OutputPath
)

Add-Type -AssemblyName System.Drawing

$width = 600
$height = 465
$original = [Drawing.Bitmap]::new($OriginalMap)
$generatedSource = [Drawing.Image]::FromFile($GeneratedCleanMap)
$generated = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [Drawing.Graphics]::FromImage($generated)
$graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graphics.DrawImage($generatedSource, [Drawing.Rectangle]::new(0, 0, $width, $height))
$graphics.Dispose()
$generatedSource.Dispose()

$mask = New-Object bool[] ($width * $height)

function Add-OverlayMask {
    param([string]$Path, [int]$OffsetX, [int]$OffsetY, [int]$DrawWidth, [int]$DrawHeight)

    $source = [Drawing.Image]::FromFile($Path)
    $overlay = [Drawing.Bitmap]::new($DrawWidth, $DrawHeight, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($overlay)
    $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.DrawImage($source, [Drawing.Rectangle]::new(0, 0, $DrawWidth, $DrawHeight))
    $g.Dispose()
    $source.Dispose()

    for($y = 0; $y -lt $DrawHeight; $y++) {
        $targetY = $OffsetY + $y
        if($targetY -lt 0 -or $targetY -ge $height) { continue }
        for($x = 0; $x -lt $DrawWidth; $x++) {
            $targetX = $OffsetX + $x
            if($targetX -lt 0 -or $targetX -ge $width) { continue }
            if($overlay.GetPixel($x, $y).A -gt 8) {
                $mask[$targetY * $width + $targetX] = $true
            }
        }
    }
    $overlay.Dispose()
}

Add-OverlayMask -Path $RouteOverlay -OffsetX 0 -OffsetY 0 -DrawWidth 600 -DrawHeight 465
Add-OverlayMask -Path $TurnOverlay -OffsetX 20 -OffsetY 6 -DrawWidth 397 -DrawHeight 124
Add-OverlayMask -Path $PinOverlay -OffsetX 506 -OffsetY 58 -DrawWidth 40 -DrawHeight 52
Add-OverlayMask -Path $ArrowOverlay -OffsetX 286 -OffsetY 435 -DrawWidth 56 -DrawHeight 56

# Expand the replacement area to remove antialiased glow pixels at overlay edges.
$expanded = New-Object bool[] ($width * $height)
$radius = 3
for($y = 0; $y -lt $height; $y++) {
    for($x = 0; $x -lt $width; $x++) {
        if(-not $mask[$y * $width + $x]) { continue }
        for($dy = -$radius; $dy -le $radius; $dy++) {
            $ny = $y + $dy
            if($ny -lt 0 -or $ny -ge $height) { continue }
            for($dx = -$radius; $dx -le $radius; $dx++) {
                $nx = $x + $dx
                if($nx -ge 0 -and $nx -lt $width) {
                    $expanded[$ny * $width + $nx] = $true
                }
            }
        }
    }
}

for($y = 0; $y -lt $height; $y++) {
    for($x = 0; $x -lt $width; $x++) {
        if($expanded[$y * $width + $x]) {
            $original.SetPixel($x, $y, $generated.GetPixel($x, $y))
        }
    }
}

$generated.Dispose()
$original.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
$original.Dispose()
