param(
    [Parameter(Mandatory = $true)][string]$OriginalMap,
    [Parameter(Mandatory = $true)][string]$RoughCleanMap,
    [Parameter(Mandatory = $true)][string]$GeneratedCleanMap,
    [Parameter(Mandatory = $true)][string]$OutputPath
)

Add-Type -AssemblyName System.Drawing

$width = 600
$height = 465
$original = [Drawing.Bitmap]::new($OriginalMap)
$rough = [Drawing.Bitmap]::new($RoughCleanMap)
$generatedSource = [Drawing.Image]::FromFile($GeneratedCleanMap)
$generated = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [Drawing.Graphics]::FromImage($generated)
$graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graphics.DrawImage($generatedSource, [Drawing.Rectangle]::new(0, 0, $width, $height))
$graphics.Dispose()
$generatedSource.Dispose()

for($y = 0; $y -lt $height; $y++) {
    for($x = 0; $x -lt $width; $x++) {
        $a = $original.GetPixel($x, $y)
        $b = $rough.GetPixel($x, $y)
        $difference = [Math]::Abs([int]$a.R - [int]$b.R) +
                      [Math]::Abs([int]$a.G - [int]$b.G) +
                      [Math]::Abs([int]$a.B - [int]$b.B)
        if($difference -gt 28) {
            $rough.SetPixel($x, $y, $generated.GetPixel($x, $y))
        }
    }
}

$original.Dispose()
$generated.Dispose()
$rough.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
$rough.Dispose()
