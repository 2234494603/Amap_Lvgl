param(
    [Parameter(Mandatory = $true)][string]$InputPath,
    [Parameter(Mandatory = $true)][string]$OutputPath,
    [Parameter(Mandatory = $true)][string]$Symbol,
    [Parameter(Mandatory = $true)][int]$Width,
    [Parameter(Mandatory = $true)][int]$Height
)

Add-Type -AssemblyName System.Drawing

$source = [Drawing.Image]::FromFile($InputPath)
$bitmap = [Drawing.Bitmap]::new($Width, $Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$graphics.Clear([Drawing.Color]::Transparent)
$graphics.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
$graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
$graphics.DrawImage($source, [Drawing.Rectangle]::new(0, 0, $Width, $Height))
$graphics.Dispose()
$source.Dispose()

$builder = [Text.StringBuilder]::new($Width * $Height * 19)
[void]$builder.AppendLine('/* Generated RGB565 + alpha asset. Do not edit manually. */')
[void]$builder.AppendLine('#include "lvgl.h"')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('#if LV_COLOR_DEPTH != 16 || LV_COLOR_16_SWAP != 0')
[void]$builder.AppendLine('#error "This generated asset requires LV_COLOR_DEPTH=16 and LV_COLOR_16_SWAP=0"')
[void]$builder.AppendLine('#endif')
[void]$builder.AppendLine('')
[void]$builder.AppendLine("const uint8_t ${Symbol}_map[] = {")

for($y = 0; $y -lt $Height; $y++) {
    [void]$builder.Append('    ')
    for($x = 0; $x -lt $Width; $x++) {
        $pixel = $bitmap.GetPixel($x, $y)
        $rgb565 = (($pixel.R -shr 3) -shl 11) -bor (($pixel.G -shr 2) -shl 5) -bor ($pixel.B -shr 3)
        [void]$builder.AppendFormat('0x{0:x2}, 0x{1:x2}, 0x{2:x2}, ',
            ($rgb565 -band 0xff), (($rgb565 -shr 8) -band 0xff), $pixel.A)
    }
    [void]$builder.AppendLine()
}

[void]$builder.AppendLine('};')
[void]$builder.AppendLine('')
[void]$builder.AppendLine("const lv_img_dsc_t $Symbol = {")
[void]$builder.AppendLine('    .header.always_zero = 0,')
[void]$builder.AppendLine("    .header.w = $Width,")
[void]$builder.AppendLine("    .header.h = $Height,")
[void]$builder.AppendLine("    .data_size = $($Width * $Height * 3),")
[void]$builder.AppendLine('    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,')
[void]$builder.AppendLine("    .data = ${Symbol}_map,")
[void]$builder.AppendLine('};')

$bitmap.Dispose()
[IO.File]::WriteAllText($OutputPath, $builder.ToString(), [Text.UTF8Encoding]::new($false))
