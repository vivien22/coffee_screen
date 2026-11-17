param([string]$Port = "COM8", [int]$HttpPort = 8765, [double]$AbortAbove = 104, [string]$LogFile = "live.csv", [int]$MaxSeconds = 3600)
# Live view of the pid-tuning telemetry: reads the serial port, serves a page with charts on http://localhost:$HttpPort/
# (only this PC), and lets the page send a few console commands. The heater is cut (stop) if the probe goes above $AbortAbove.
$token = [guid]::NewGuid().ToString("N")
$allowed = "^(set|kp|ki|kd|ff|delay|power|inlet|heater|valve|pump|stream|status|stop|help)( [0-9.\-]+)?$"

$html = @'
<!doctype html><html><head><meta charset="utf-8"><title>PID live</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
:root{--bg:#fff;--fg:#1c1c1e;--mut:#6b6b70;--grid:#e3e3e6;--c1:#d9480f;--c2:#1c7ed6;--c3:#2f9e44;--c4:#9c36b5;--card:#f6f6f8}
@media(prefers-color-scheme:dark){:root{--bg:#161618;--fg:#ececee;--mut:#9a9aa2;--grid:#2c2c30;--c1:#ff8a5c;--c2:#5aa9f0;--c3:#5fd17a;--c4:#d58bea;--card:#202024}}
body{margin:0;padding:16px;background:var(--bg);color:var(--fg);font:14px system-ui,sans-serif}
h1{font-size:18px;margin:0 0 4px}.sub{color:var(--mut);margin-bottom:12px}
.row{display:flex;flex-wrap:wrap;gap:8px;margin:8px 0}
.stat{background:var(--card);border-radius:8px;padding:8px 12px;min-width:90px}.stat b{display:block;font-size:20px}.stat span{color:var(--mut);font-size:12px}
.card{background:var(--card);border-radius:10px;padding:10px;margin:10px 0}
.card h2{font-size:13px;margin:0 0 4px;color:var(--mut);font-weight:600}
canvas{width:100%;height:210px;display:block}
.leg span{margin-right:14px;font-size:12px}.leg i{display:inline-block;width:10px;height:10px;border-radius:2px;margin-right:4px}
input,button{font:inherit;padding:6px 10px;border-radius:6px;border:1px solid var(--grid);background:var(--bg);color:var(--fg)}
button{cursor:pointer}button.stop{background:#c92a2a;color:#fff;border-color:#c92a2a}
#log{white-space:pre;font:12px ui-monospace,monospace;color:var(--mut);max-height:90px;overflow:auto}
</style></head><body>
<h1>Boiler PID, live</h1><div class="sub" id="sub">waiting for data...</div>
<div class="row">
<div class="stat"><b id="sProbe">-</b><span>probe °C</span></div>
<div class="stat"><b id="sSet">-</b><span>setpoint °C</span></div>
<div class="stat"><b id="sOn">-</b><span>heater ms / s</span></div>
<div class="stat"><b id="sFlow">-</b><span>flow In ml/s</span></div>
<div class="stat"><b id="sVol">-</b><span>volume In ml</span></div>
</div>
<div class="row">
<input id="cmd" size="18" placeholder="command (set 97, ff 0.3, pump 20...)"><button onclick="send(document.getElementById('cmd').value)">Send</button>
<button onclick="send('pump 20')">pump 20 s</button><button onclick="send('pump 0')">pump off</button><button onclick="send('valve 1')">valve open</button><button onclick="send('valve 0')">valve closed</button>
<button class="stop" onclick="send('stop')">STOP (pump off, heater off)</button>
<label>window <select id="win"><option value="120">2 min</option><option value="300" selected>5 min</option><option value="900">15 min</option><option value="0">all</option></select></label>
</div>
<div class="card"><h2>Probe and setpoint (°C)</h2><canvas id="c1"></canvas><div class="leg" id="l1"></div></div>
<div class="card"><h2>Heating in the 1 s window (ms): total, from the PID, from the feedforward</h2><canvas id="c2"></canvas><div class="leg" id="l2"></div></div>
<div class="card"><h2>Flow In (ml/s) and volume In (ml)</h2><canvas id="c3"></canvas><div class="leg" id="l3"></div></div>
<div class="card"><h2>Answers</h2><div id="log"></div></div>
<script>
const TOKEN="__TOKEN__";const rows=[];let next=0;
const col=n=>getComputedStyle(document.documentElement).getPropertyValue(n).trim();
function legend(id,items){document.getElementById(id).innerHTML=items.map(([n,c])=>`<span><i style="background:${col(c)}"></i>${n}</span>`).join('')}
legend('l1',[['probe','--c1'],['setpoint','--c2']]);legend('l2',[['on_ms','--c1'],['pid_ms','--c2'],['ff_ms','--c3']]);legend('l3',[['flow In','--c2'],['volume In','--c3']]);
function chart(id,xs,series,opts){
 const cv=document.getElementById(id),dpr=devicePixelRatio||1,W=cv.clientWidth,H=cv.clientHeight;cv.width=W*dpr;cv.height=H*dpr;
 const g=cv.getContext('2d');g.scale(dpr,dpr);g.clearRect(0,0,W,H);
 const L=44,R=44,T=8,B=20;let x0=xs[0],x1=xs[xs.length-1];if(!(x1>x0)){return}
 const sides=opts.sides||series.map(()=>0);
 const rng=[0,1].map(s=>{let lo=1e9,hi=-1e9;series.forEach((se,i)=>{if(sides[i]!==s)return;se.v.forEach(v=>{if(v==null)return;lo=Math.min(lo,v);hi=Math.max(hi,v)})});if(lo>hi)return null;if(hi-lo<(opts.minSpan?opts.minSpan[s]:1)){const m=(hi+lo)/2,h=(opts.minSpan?opts.minSpan[s]:1)/2;lo=m-h;hi=m+h}const p=(hi-lo)*.05;return[lo-p,hi+p]});
 const X=t=>L+(t-x0)/(x1-x0)*(W-L-R),Y=(v,s)=>{const r=rng[s];return T+(1-(v-r[0])/(r[1]-r[0]))*(H-T-B)};
 g.strokeStyle=col('--grid');g.fillStyle=col('--mut');g.font='11px system-ui';g.lineWidth=1;
 for(let k=0;k<=4;k++){const y=T+k*(H-T-B)/4;g.beginPath();g.moveTo(L,y);g.lineTo(W-R,y);g.stroke();
  [0,1].forEach(s=>{if(!rng[s])return;const v=rng[s][1]-k*(rng[s][1]-rng[s][0])/4;g.textAlign=s?'left':'right';g.fillText(v.toFixed(opts.dec?opts.dec[s]:1),s?W-R+4:L-4,y+4)})}
 g.textAlign='center';for(let k=0;k<=4;k++){const t=x0+k*(x1-x0)/4;g.fillText(Math.round(t-x1)+' s',X(t),H-4)}
 series.forEach((se,i)=>{const s=sides[i];if(!rng[s])return;g.strokeStyle=col(se.c);g.lineWidth=1.8;g.beginPath();let pen=false;
  xs.forEach((t,j)=>{const v=se.v[j];if(v==null){pen=false;return}const px=X(t),py=Y(v,s);pen?g.lineTo(px,py):g.moveTo(px,py);pen=true});g.stroke()});
}
function draw(){
 if(!rows.length)return;const w=+document.getElementById('win').value;const last=rows[rows.length-1][0]/1000;
 const r=w?rows.filter(x=>x[0]/1000>=last-w):rows;if(r.length<2)return;const xs=r.map(x=>x[0]/1000);
 const n=i=>r.map(x=>isNaN(x[i])?null:x[i]);
 chart('c1',xs,[{v:n(1),c:'--c1'},{v:n(2).map(v=>v>0?v:null),c:'--c2'}],{dec:[1]});
 chart('c2',xs,[{v:n(5),c:'--c1'},{v:n(3),c:'--c2'},{v:n(4),c:'--c3'}],{dec:[0],minSpan:[200]});
 chart('c3',xs,[{v:n(8),c:'--c2'},{v:n(9),c:'--c3'}],{sides:[0,1],dec:[1,0],minSpan:[2,20]});
 const q=rows[rows.length-1];sProbe.textContent=isNaN(q[1])?'nan':q[1].toFixed(2);sSet.textContent=q[2];sOn.textContent=q[5];sFlow.textContent=q[8].toFixed(1);sVol.textContent=q[9].toFixed(1);
 sub.textContent=`${rows.length} samples, last at ${(q[0]/1000).toFixed(0)} s, pump ${q[7]?'ON':'off'}, heater relay ${q[6]?'on':'off'}`;
}
async function poll(){try{const r=await fetch(`/data?from=${next}&t=${TOKEN}`);const j=await r.json();j.rows.forEach(x=>rows.push(x));next=j.next;
 if(j.log.length){const l=document.getElementById('log');l.textContent+=j.log.join('\n')+'\n';l.scrollTop=l.scrollHeight}
 if(j.rows.length)draw()}catch(e){sub.textContent='no connection to the server'}setTimeout(poll,1000)}
async function send(c){c=c.trim();if(!c)return;await fetch(`/cmd?t=${TOKEN}&c=${encodeURIComponent(c)}`)}
win.onchange=draw;addEventListener('resize',draw);poll();
</script></body></html>
'@
$html = $html.Replace("__TOKEN__", $token)

$sp = New-Object System.IO.Ports.SerialPort $Port, 115200, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
$sp.NewLine = "`n"; $sp.DtrEnable = $true; $sp.ReadTimeout = 100
$sp.Open()
Start-Sleep -Seconds 3
$null = $sp.ReadExisting()
$sp.WriteLine("stream 1000")

$rows = New-Object System.Collections.ArrayList
$answers = New-Object System.Collections.ArrayList
$logSent = 0
$buffer = ""
$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://localhost:$HttpPort/")
$listener.Start()
"listening on http://localhost:$HttpPort/ (serial $Port)"
$start = Get-Date
$pendingCtx = $listener.GetContextAsync()
function Send-Text($ctx, $text, $type) {
  $b = [Text.Encoding]::UTF8.GetBytes($text); $ctx.Response.ContentType = $type
  $ctx.Response.ContentLength64 = $b.Length; $ctx.Response.OutputStream.Write($b, 0, $b.Length); $ctx.Response.Close()
}
try {
  while (((Get-Date) - $start).TotalSeconds -lt $MaxSeconds) {
    try { $buffer += $sp.ReadExisting() } catch {}
    while (($nl = $buffer.IndexOf("`n")) -ge 0) {
      $line = $buffer.Substring(0, $nl).Trim(); $buffer = $buffer.Substring($nl + 1)
      if ($line -match "^[0-9]+,") {
        $f = $line.Split(",")
        if ($f.Count -ge 11) {
          $v = $f | ForEach-Object { $d = 0.0; if ([double]::TryParse($_, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$d)) { $d } else { [double]::NaN } }
          [void]$rows.Add($v); Add-Content -Path $LogFile -Value $line
          if (-not [double]::IsNaN($v[1]) -and $v[1] -gt $AbortAbove) { $sp.WriteLine("stop"); [void]$answers.Add("# ABORT: probe $($v[1]) above $AbortAbove, stop sent") }
        }
      } elseif ($line.Length -gt 0 -and $line -notmatch "^ms,") { [void]$answers.Add($line) }
    }
    if ($pendingCtx.IsCompleted) {
      $ctx = $pendingCtx.Result; $pendingCtx = $listener.GetContextAsync()
      try {
        $path = $ctx.Request.Url.AbsolutePath; $q = $ctx.Request.QueryString
        if ($path -eq "/") { Send-Text $ctx $html "text/html; charset=utf-8" }
        elseif ($q["t"] -ne $token) { $ctx.Response.StatusCode = 403; $ctx.Response.Close() }
        elseif ($path -eq "/data") {
          $from = [int]$q["from"]; $out = New-Object System.Collections.ArrayList
          for ($i = $from; $i -lt $rows.Count; $i++) { [void]$out.Add(($rows[$i] | ForEach-Object { if ([double]::IsNaN($_)) { "null" } else { $_.ToString("R", [Globalization.CultureInfo]::InvariantCulture) } }) -join ",") }
          $rowsJson = "[" + (($out | ForEach-Object { "[" + ($_ -replace "null", "NaN") + "]" }) -join ",") + "]"
          $logLines = @(); for ($i = $logSent; $i -lt $answers.Count; $i++) { $logLines += ('"' + ($answers[$i] -replace '\\', '\\' -replace '"', '\"') + '"') }; $logSent = $answers.Count
          # NaN is not JSON: the page uses eval-free parsing, so send null and let the page treat it (rows are numbers)
          $rowsJson = $rowsJson -replace "NaN", "null"
          Send-Text $ctx ('{"next":' + $rows.Count + ',"rows":' + $rowsJson + ',"log":[' + ($logLines -join ",") + ']}') "application/json"
        }
        elseif ($path -eq "/cmd") {
          $c = [string]$q["c"]
          if ($c -match $allowed) { $sp.WriteLine($c); [void]$answers.Add("> $c"); Send-Text $ctx "ok" "text/plain" }
          else { [void]$answers.Add("# refused: $c"); $ctx.Response.StatusCode = 400; $ctx.Response.Close() }
        }
        else { $ctx.Response.StatusCode = 404; $ctx.Response.Close() }
      } catch { try { $ctx.Response.Close() } catch {} }
    }
    Start-Sleep -Milliseconds 50
  }
} finally {
  try { $sp.WriteLine("stop"); Start-Sleep -Milliseconds 300 } catch {}
  $listener.Stop(); $sp.Close()
}
