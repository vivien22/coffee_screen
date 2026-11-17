param([string[]]$Configs = @("0.3:0","0.5:0","0.7:0"), [double]$Setpoint = 97, [int]$PumpSeconds = 20, [string]$Csv = "live.csv", [string]$Result = "ff_results.txt")
# Drives the live server (http://localhost:8765) : for each "ff:delay" configuration, waits for a stable probe at the setpoint,
# runs the pump, follows the probe, and writes a summary. Stops (stop command) if anything looks wrong.
$Configs = @($Configs | ForEach-Object { $_ -split "," })
$base = "http://localhost:8765"
$page = (Invoke-WebRequest -UseBasicParsing "$base/").Content
$token = [regex]::Match($page, 'TOKEN="([0-9a-f]+)"').Groups[1].Value
function Cmd($c) { [void](Invoke-WebRequest -UseBasicParsing "$base/cmd?t=$token&c=$([uri]::EscapeDataString($c))") }
function Last-Rows($n) {
  Get-Content $Csv -Tail $n | Where-Object { $_ -match "^[0-9]+," } | ForEach-Object {
    $f = $_.Split(","); [pscustomobject]@{ ms = [double]$f[0]; probe = [double]::Parse($f[1], [Globalization.CultureInfo]::InvariantCulture); on = [double]$f[5]; pump = [int]$f[7]; vol = [double]::Parse($f[9], [Globalization.CultureInfo]::InvariantCulture) } }
}
function Wait-Stable($timeoutS) {
  $t0 = Get-Date
  while (((Get-Date) - $t0).TotalSeconds -lt $timeoutS) {
    $r = @(Last-Rows 15)
    if ($r.Count -ge 12) {
      $p = $r[-1].probe; $slope = ($r[-1].probe - $r[-11].probe) / 10
      if ([math]::Abs($p - $Setpoint) -le 0.5 -and [math]::Abs($slope) -le 0.03) { return $true }
    }
    Start-Sleep -Seconds 3
  }
  return $false
}
"series started $(Get-Date -Format s), setpoint $Setpoint, pump $PumpSeconds s" | Add-Content $Result
Cmd "set $Setpoint"
foreach ($cfg in $Configs) {
  $ff, $delay = $cfg.Split(":")
  Cmd "ff $ff"; Cmd "delay $delay"
  if (-not (Wait-Stable 420)) { "config $cfg : probe did not stabilise at $Setpoint, series stopped" | Add-Content $Result; Cmd "stop"; return }
  $pre = (Last-Rows 5)[-1].probe
  $startMs = (Last-Rows 1)[-1].ms
  Cmd "pump $PumpSeconds"
  $end = (Get-Date).AddSeconds($PumpSeconds + 110)
  while ((Get-Date) -lt $end) { Start-Sleep -Seconds 2 }
  $rows = @(Last-Rows 400 | Where-Object { $_.ms -ge $startMs })
  $min = ($rows | Sort-Object probe | Select-Object -First 1)
  $max = ($rows | Where-Object { $_.ms -ge $min.ms } | Sort-Object probe -Descending | Select-Object -First 1)
  $vol = ($rows | Sort-Object vol -Descending | Select-Object -First 1).vol
  $peak = ($rows | Sort-Object on -Descending | Select-Object -First 1)
  $back = $rows | Where-Object { $_.ms -ge $min.ms -and [math]::Abs($_.probe - $pre) -le 0.5 } | Select-Object -First 1
  $line = "ff {0} delay {1}: before {2:N2}  minimum {3:N2} (drop {4:N2}) at +{5:N0} s  back within 0.5 C after +{6}  max after the minimum {7:N2}  peak heating {8:N0} ms  volume {9:N0} ml" -f $ff, $delay, $pre, $min.probe, ($pre - $min.probe), (($min.ms - $startMs) / 1000), $(if ($back) { "{0:N0} s" -f (($back.ms - $startMs) / 1000) } else { "never (110 s)" }), $max.probe, $peak.on, $vol
  $line | Add-Content $Result
}
Cmd "stop"
"series finished $(Get-Date -Format s)" | Add-Content $Result
