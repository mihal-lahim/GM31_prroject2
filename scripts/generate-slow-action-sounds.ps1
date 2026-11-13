# SlowActionの仮効果音(16bit・モノラル・22050Hz)を合成して assets/audio へ書き出します。
#   実行: powershell -ExecutionPolicy Bypass -File scripts/generate-slow-action-sounds.ps1
# 乱数の種は固定なので、何度実行しても同じファイルになります。
$ErrorActionPreference = 'Stop'
$rate = 22050
$out = Join-Path $PSScriptRoot '..\assets\audio'
$random = New-Object System.Random 20261113

function Write-Wave([string]$name, [double]$seconds, [scriptblock]$sample)
{
	$count = [int]($rate * $seconds)
	$data = New-Object byte[] ($count * 2)
	$state = @{ Low = 0.0 }
	for ($i = 0; $i -lt $count; $i++)
	{
		$t = $i / $rate
		$p = $i / $count
		$v = & $sample $t $p $state
		# 端で音が切れてプツッと鳴らないよう、最後の5%を絞ります。
		if ($p -gt 0.95) { $v *= (1.0 - $p) / 0.05 }
		$v = [Math]::Max(-1.0, [Math]::Min(1.0, $v))
		$s = [int16]($v * 30000)
		$data[$i * 2] = [byte]($s -band 0xFF)
		$data[$i * 2 + 1] = [byte](($s -shr 8) -band 0xFF)
	}
	$path = Join-Path $out "$name.wav"
	$stream = [System.IO.File]::Create($path)
	$writer = New-Object System.IO.BinaryWriter $stream
	$writer.Write([Text.Encoding]::ASCII.GetBytes('RIFF'))
	$writer.Write([int](36 + $data.Length))
	$writer.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt '))
	$writer.Write([int]16)
	$writer.Write([int16]1)        # PCM
	$writer.Write([int16]1)        # モノラル
	$writer.Write([int]$rate)
	$writer.Write([int]($rate * 2))
	$writer.Write([int16]2)
	$writer.Write([int16]16)
	$writer.Write([Text.Encoding]::ASCII.GetBytes('data'))
	$writer.Write([int]$data.Length)
	$writer.Write($data)
	$writer.Close()
	Write-Host "wrote $path"
}

function Noise { return $random.NextDouble() * 2.0 - 1.0 }

# 一次のローパスです。aが小さいほどこもった音になります。
function Low($state, $x, $a) { $state.Low += ($x - $state.Low) * $a; return $state.Low }

# 通常攻撃: 短い打撃音
Write-Wave 'slowHit' 0.09 {
	param($t, $p, $s)
	$env = [Math]::Pow(1.0 - $p, 3)
	((Low $s (Noise) 0.35) * 0.9 + [Math]::Sin(2 * [Math]::PI * 520 * $t) * 0.4) * $env
}

# 強攻撃: 低い衝撃音
Write-Wave 'slowStrongHit' 0.26 {
	param($t, $p, $s)
	$freq = 140 - 90 * $p
	$env = [Math]::Pow(1.0 - $p, 2)
	([Math]::Sin(2 * [Math]::PI * $freq * $t) * 0.9 + (Low $s (Noise) 0.25) * 0.6 * [Math]::Pow(1.0 - $p, 6)) * $env
}

# 壁反射: 金属的な響き
Write-Wave 'slowBounce' 0.32 {
	param($t, $p, $s)
	$env = [Math]::Exp(-9.0 * $p)
	([Math]::Sin(2 * [Math]::PI * 880 * $t) * 0.5 + [Math]::Sin(2 * [Math]::PI * 1390 * $t) * 0.35 +
		[Math]::Sin(2 * [Math]::PI * 230 * $t) * 0.5) * $env
}

# 撃破: ノイズと下がる音
Write-Wave 'slowKill' 0.42 {
	param($t, $p, $s)
	$freq = 420 * [Math]::Pow(0.25, $p)
	$env = [Math]::Pow(1.0 - $p, 1.5)
	((Low $s (Noise) 0.5) * 0.55 + [Math]::Sin(2 * [Math]::PI * $freq * $t) * 0.6) * $env
}

# 被弾: 濁った低音
Write-Wave 'slowDamage' 0.22 {
	param($t, $p, $s)
	$freq = 200 - 80 * $p
	$square = if ([Math]::Sin(2 * [Math]::PI * $freq * $t) -ge 0) { 0.5 } else { -0.5 }
	($square + (Noise) * 0.25) * (1.0 - $p)
}

# 時間減速の開始: 下がっていくうなり
Write-Wave 'slowTimeOn' 0.38 {
	param($t, $p, $s)
	$phase = 2 * [Math]::PI * (520 * $t - 380 * $t * $p * 0.5)
	[Math]::Sin($phase) * 0.45 * [Math]::Sin([Math]::PI * $p)
}

# 敵の射撃
Write-Wave 'slowShot' 0.14 {
	param($t, $p, $s)
	$freq = 1300 - 900 * $p
	([Math]::Sin(2 * [Math]::PI * $freq * $t) * 0.6 + (Noise) * 0.2) * (1.0 - $p)
}

# 弾の打ち返し: 上がっていく鋭い音
Write-Wave 'slowDeflect' 0.2 {
	param($t, $p, $s)
	$freq = 1500 + 1200 * $p
	([Math]::Sin(2 * [Math]::PI * $freq * $t) * 0.55 + [Math]::Sin(2 * [Math]::PI * $freq * 0.5 * $t) * 0.3) * [Math]::Pow(1.0 - $p, 1.5)
}
