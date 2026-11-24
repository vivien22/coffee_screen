# png_to_icon.ps1: turns a PNG into an icon of 4 levels of opacity of the icon color (written as 4-bit pixels: 0, 5, 10, 15) for tools/icons_raw.h.
# Windows PowerShell (System.Drawing); the result is text, so the build needs nothing of this.
#
#   .\tools\png_to_icon.ps1 -Path picture.png -Name WELCOME -Size 96 -Palette "8A4B38=0.34,CCD6DE=0.67,F5F9FA=1" -Preview preview.png
#
# The picture is reduced to Size x Size pixels (the whole picture, 6 x 6 samples per pixel: smooth edges). Every color of
# the picture gets an "ink" between 0 (background) and 1 (full icon color): the nearest color of -Palette
# (RRGGBB=ink, comma separated) gives it; without -Palette the ink is the brightness of the color. The transparency of
# the picture is kept. ink x 3 rounded gives the level 0..3 (0 = background, 3 = full color).
#
# Output on stdout: the lines to paste in tools/icons_raw.h (then run ./tools/compress_icons.sh). -Preview writes a
# picture of what the screen will show (icon color on the background color, enlarged 4 times).
param(
  [Parameter(Mandatory = $true)] [string] $Path,
  [string] $Name = "WELCOME",
  [int] $Size = 96,
  [string] $Palette = "",
  [string] $Preview = ""
)

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
public static class IconReducer {
  // argb: Width x Height pixels as read from a 32 bpp bitmap (B, G, R, A bytes). Returns Size x Size levels 0..3.
  public static byte[] Reduce(byte[] argb, int width, int height, int size, double[] paletteRgb, double[] paletteInk) {
    byte[] levels = new byte[size * size];
    const int N = 6;
    for (int y = 0; y < size; y++) {
      for (int x = 0; x < size; x++) {
        double sum = 0;
        for (int sy = 0; sy < N; sy++) {
          for (int sx = 0; sx < N; sx++) {
            int px = (int)((x + (sx + 0.5) / N) * width / size);
            int py = (int)((y + (sy + 0.5) / N) * height / size);
            int i = (py * width + px) * 4;
            double b = argb[i], g = argb[i + 1], r = argb[i + 2], a = argb[i + 3] / 255.0;
            double ink;
            if (paletteInk.Length == 0) {
              ink = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
            } else {
              int best = 0; double bestDistance = double.MaxValue;
              for (int k = 0; k < paletteInk.Length; k++) {
                double dr = r - paletteRgb[k * 3], dg = g - paletteRgb[k * 3 + 1], db = b - paletteRgb[k * 3 + 2];
                double d = dr * dr + dg * dg + db * db;
                if (d < bestDistance) { bestDistance = d; best = k; }
              }
              ink = paletteInk[best];
            }
            sum += a * ink;
          }
        }
        levels[y * size + x] = (byte)Math.Min(3, Math.Max(0, (int)Math.Floor(sum / (N * N) * 3 + 0.5)));
      }
    }
    return levels;
  }
}
"@

# the palette
$rgb = New-Object System.Collections.Generic.List[double]
$ink = New-Object System.Collections.Generic.List[double]
if ($Palette) {
  foreach ($entry in $Palette.Split(",")) {
    $parts = $entry.Trim().Split("=")
    $hex = $parts[0]
    $rgb.Add([Convert]::ToInt32($hex.Substring(0, 2), 16)); $rgb.Add([Convert]::ToInt32($hex.Substring(2, 2), 16)); $rgb.Add([Convert]::ToInt32($hex.Substring(4, 2), 16))
    $ink.Add([double]::Parse($parts[1], [Globalization.CultureInfo]::InvariantCulture))
  }
}

# the pixels of the picture
$source = [System.Drawing.Bitmap]::new((Resolve-Path $Path).Path)
$rect = [System.Drawing.Rectangle]::new(0, 0, $source.Width, $source.Height)
$data = $source.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$bytes = New-Object byte[] ($source.Width * $source.Height * 4)
[Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
$source.UnlockBits($data)
$levels = [IconReducer]::Reduce($bytes, $source.Width, $source.Height, $Size, $rgb.ToArray(), $ink.ToArray())

# preview: the icon color over the background color, as the screen will show it (COLOR_BG 0x1040, COLOR_ICON 0xFDCD)
if ($Preview) {
  $scale = 4
  $bitmap = [System.Drawing.Bitmap]::new($Size * $scale, $Size * $scale)
  $bg = @(16, 8, 0); $fg = @(255, 186, 107)
  for ($y = 0; $y -lt $Size; $y++) { for ($x = 0; $x -lt $Size; $x++) {
    $l = $levels[$y * $Size + $x]
    $c = [System.Drawing.Color]::FromArgb([int]($bg[0] + ($fg[0] - $bg[0]) * $l / 3), [int]($bg[1] + ($fg[1] - $bg[1]) * $l / 3), [int]($bg[2] + ($fg[2] - $bg[2]) * $l / 3))
    for ($dy = 0; $dy -lt $scale; $dy++) { for ($dx = 0; $dx -lt $scale; $dx++) { $bitmap.SetPixel($x * $scale + $dx, $y * $scale + $dy, $c) } }
  } }
  $bitmap.Save($Preview, [System.Drawing.Imaging.ImageFormat]::Png)
}

# the 4-bit data (the format of icons_raw.h), two pixels per byte, the first pixel in the high bits; the levels 0..3 become 0, 5, 10, 15
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("#define ICON_${Name}_WIDTH $Size")
[void]$sb.AppendLine("#define ICON_${Name}_HEIGHT $Size")
[void]$sb.Append("const uint8_t ICON_$Name[] PROGMEM = {")
$count = [int][Math]::Ceiling($Size * $Size / 2.0)
for ($i = 0; $i -lt $count; $i++) {
  $byte = 0
  for ($k = 0; $k -lt 2; $k++) {
    $p = $i * 2 + $k
    $level = if ($p -lt $levels.Length) { $levels[$p] * 5 } else { 0 }
    $byte = $byte -bor ($level -shl (4 - 4 * $k))
  }
  if ($i % 12 -eq 0) { [void]$sb.Append("`n  ") } else { [void]$sb.Append(" ") }
  [void]$sb.Append(("0x{0:X2}," -f $byte))
}
[void]$sb.AppendLine("`n};")
$counts = 0..3 | ForEach-Object { $l = $_; ($levels | Where-Object { $_ -eq $l }).Count }
[Console]::Error.WriteLine("levels 0/1/2/3: " + ($counts -join " / ") + "   ($($levels.Length) pixels)")
$sb.ToString()
