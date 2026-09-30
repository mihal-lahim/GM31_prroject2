param(
    [Parameter(Mandatory = $true)][string]$Baseline,
    [Parameter(Mandatory = $true)][string]$Current,
    [int]$Tolerance = 0,
    # Rectangle to exclude, "x,y,w,h". Used to mask the wall-clock FPS readout.
    [string]$IgnoreRect = ""
)

# Compares render-test PNG output against a baseline directory.
# Hash first (fast path); only hash-mismatched files get a pixel comparison.

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class PixelCompare
{
    // Returns [0]=max channel delta, [1]=differing bytes, [2]=total bytes. Size mismatch -> [0]=-1.
    // Pixels inside the ignore rectangle (ix,iy,iw,ih) are skipped; pass iw<=0 to compare everything.
    public static long[] Run(string pathA, string pathB, int ix, int iy, int iw, int ih)
    {
        using (Bitmap a = new Bitmap(pathA))
        using (Bitmap b = new Bitmap(pathB))
        {
            if (a.Width != b.Width || a.Height != b.Height)
            {
                return new long[] { -1, 0, 0 };
            }

            Rectangle rect = new Rectangle(0, 0, a.Width, a.Height);
            BitmapData da = a.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            BitmapData db = b.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            try
            {
                int count = da.Stride * a.Height;
                byte[] bufA = new byte[count];
                byte[] bufB = new byte[count];
                Marshal.Copy(da.Scan0, bufA, 0, count);
                Marshal.Copy(db.Scan0, bufB, 0, count);

                long maxDiff = 0;
                long differing = 0;
                long compared = 0;
                for (int y = 0; y < a.Height; y++)
                {
                    bool rowIgnored = iw > 0 && y >= iy && y < iy + ih;
                    int rowStart = y * da.Stride;
                    for (int x = 0; x < a.Width; x++)
                    {
                        if (rowIgnored && x >= ix && x < ix + iw)
                        {
                            continue;
                        }
                        int p = rowStart + x * 4;
                        for (int c = 0; c < 4; c++)
                        {
                            compared++;
                            int d = bufA[p + c] - bufB[p + c];
                            if (d < 0) d = -d;
                            if (d > 0)
                            {
                                differing++;
                                if (d > maxDiff) maxDiff = d;
                            }
                        }
                    }
                }
                return new long[] { maxDiff, differing, compared };
            }
            finally
            {
                a.UnlockBits(da);
                b.UnlockBits(db);
            }
        }
    }
}
'@ -ReferencedAssemblies System.Drawing

$ix = 0; $iy = 0; $iw = 0; $ih = 0
if ($IgnoreRect -ne "") {
    $parts = $IgnoreRect.Split(',')
    $ix = [int]$parts[0]; $iy = [int]$parts[1]; $iw = [int]$parts[2]; $ih = [int]$parts[3]
}

$identical = 0
$changed = @()
$missing = @()

Get-ChildItem -Path $Baseline -Filter *.png | ForEach-Object {
    $currentPath = Join-Path $Current $_.Name
    if (-not (Test-Path $currentPath)) {
        $missing += $_.Name
        return
    }

    $hashA = (Get-FileHash -Algorithm SHA256 $_.FullName).Hash
    $hashB = (Get-FileHash -Algorithm SHA256 $currentPath).Hash
    if ($hashA -eq $hashB) {
        $identical++
        return
    }

    $result = [PixelCompare]::Run($_.FullName, $currentPath, $ix, $iy, $iw, $ih)
    if ($result[0] -ge 0 -and $result[0] -le $Tolerance) {
        $identical++
        return
    }

    $changed += [pscustomobject]@{
        Name      = $_.Name
        MaxDiff   = $result[0]
        DiffRatio = $(if ($result[2] -gt 0) { [math]::Round($result[1] / $result[2], 6) } else { 1 })
    }
}

Write-Output "identical: $identical"
Write-Output "changed:   $($changed.Count)"
Write-Output "missing:   $($missing.Count)"
if ($changed.Count -gt 0) {
    $changed | Sort-Object -Property MaxDiff -Descending | Format-Table -AutoSize | Out-String | Write-Output
}
if ($missing.Count -gt 0) {
    Write-Output "missing files: $($missing -join ', ')"
}
if ($changed.Count -eq 0 -and $missing.Count -eq 0) { exit 0 } else { exit 1 }
