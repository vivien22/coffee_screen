param([string[]]$Configs = @("40:0.1:500"), [double]$Setpoint = 97, [double]$Ff = 1.0, [int]$PumpSeconds = 20, [int]$ObserveSeconds = 150, [string]$Csv = "live2.csv", [string]$Result = "pid_results.txt")
# Drives the live server: for each "kp:ki:kd" configuration, waits for a stable probe at the setpoint, runs the pump and follows
# the probe, then writes a summary (steady state before, drop, recovery, overshoot, state at the end).
$Configs = @($Configs | ForEach-Object { $_ -split "," })
$base = "http://localhost:8765"
$page = (Invoke-WebRequest -UseBasicParsing "$base/").Content
$token = [regex]::Match($page, 'TOKEN="([0-9a-f]+)"').Groups[1].Value
$ci = [Globalization.CultureInfo]::InvariantCulture
function Cmd($c) { [void](Invoke-WebRequest -UseBasicParsing "$base/cmd?t=$token&c=$([uri]::EscapeDataString($c))") }
function Last-Rows($n) {
  Get-Content $Csv -Tail $n | Where-Object { $_ -match "^[0-9]+," } | ForEach-Object {
    $f = $_.Split(","); [pscustomobject]@{ ms = [double]$f[0]; probe = [double]::Parse($f[1], $ci); on = [double]$f[5]; pump = [int]$f[7]; vol = [double]::Parse($f[9], $ci) } }
}
function Wait-Stable($timeoutS) {
  $t0 = Get-Date
  while (((Get-Date) - $t0).TotalSeconds -lt $timeoutS) {
    $r = @(Last-Rows 25)
    if ($r.Count -ge 20) {
      $slope = ($r[-1].probe - $r[-11].probe) / 10
      if ([math]::Abs($r[-1].probe - $Setpoint) -le 0.4 -and [math]::Abs($slope) -le 0.02) { return $true }
    }
    Start-Sleep -Seconds 3
  }
  return $false
}
function F($x) { $x.ToString("0.00", $ci) }
"series started $(Get-Date -Format s), setpoint $Setpoint, ff $Ff, pump $PumpSeconds s" | Add-Content $Result
Cmd "ff $Ff"; Cmd "delay 0"; Cmd "set $Setpoint"
foreach ($cfg in $Configs) {
  $kp, $ki, $kd = $cfg.Split(":")
  Cmd "kp $kp"; Cmd "ki $ki"; Cmd "kd $kd"
  if (-not (Wait-Stable 600)) { "kp $kp ki $ki kd $kd : the probe did not stabilise at $Setpoint, config skipped" | Add-Content $Result; continue }
  $pre = @(Last-Rows 20); $before = ($pre | Measure-Object probe -Average).Average
  $startMs = $pre[-1].ms
  Cmd "valve 1"; Start-Sleep -Seconds 2        # the valve is opened first, then the pump
  Cmd "pump $PumpSeconds"
  # Safety: the water must flow (5 ml/s expected, the valve open). If the flow is under 3.5 ml/s after 5 s of pump: everything off.
  $invalid = $false; $t0 = Get-Date
  while (((Get-Date) - $t0).TotalSeconds -lt ($PumpSeconds + $ObserveSeconds)) {
    Start-Sleep -Seconds 1
    $el = ((Get-Date) - $t0).TotalSeconds
    if ($el -gt 5 -and $el -lt $PumpSeconds) {
      $l = (Get-Content $Csv -Tail 1).Split(",")
      if ($l.Count -ge 11 -and [int]$l[7] -eq 1 -and [double]::Parse($l[8], $ci) -lt 3.5) { Cmd "stop"; $invalid = $true; break }
    }
  }
  if ($invalid) { "kp $kp ki $ki kd $kd : INVALID, the flow stayed under 3.5 ml/s (valve closed?): everything stopped, series ended" | Add-Content $Result; return }
  $rows = @(Last-Rows 400 | Where-Object { $_.ms -ge $startMs })
  $vol0 = ($rows | Sort-Object vol -Descending | Select-Object -First 1).vol
  $min = $rows | Sort-Object probe | Select-Object -First 1
  $after = @($rows | Where-Object { $_.ms -ge $min.ms })
  $max = $after | Sort-Object probe -Descending | Select-Object -First 1
  $back = $after | Where-Object { [math]::Abs($_.probe - $before) -le 0.5 } | Select-Object -First 1
  $settled = $after | Where-Object { [math]::Abs($_.probe - $before) -le 0.2 } | Select-Object -First 1
  $tail = @($rows | Select-Object -Last 20); $endMean = ($tail | Measure-Object probe -Average).Average
  $line = "kp {0} ki {1} kd {2}: before {3} (setpoint error {4})  min {5} (drop {6})  back within 0.5 C at +{7}  within 0.2 C at +{8}  max after min {9}  mean of the last 20 s {10}" -f $kp, $ki, $kd, (F $before), (F ($before - $Setpoint)), (F $min.probe), (F ($before - $min.probe)), $(if ($back) { [int](($back.ms - $startMs) / 1000) } else { "never" }), $(if ($settled) { [int](($settled.ms - $startMs) / 1000) } else { "never" }), (F $max.probe), (F $endMean)
  $line | Add-Content $Result
}
Cmd "stop"
"series finished $(Get-Date -Format s)" | Add-Content $Result
