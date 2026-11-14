# Keeps a free Cloudflare quick tunnel to the gate alive: restarts cloudflared when it exits or when the public URL stops
# answering /healthz three times in a row, and writes the current https base URL to -OutFile (it changes on each restart).
param([string]$Cloudflared, [int]$Port, [string]$OutFile, [string]$LogDir)
$log = Join-Path $LogDir 'cloudflared.log'
while ($true) {
  Remove-Item "$log.cur" -ErrorAction SilentlyContinue
  $p = Start-Process $Cloudflared -ArgumentList @('tunnel', '--no-autoupdate', '--url', "http://127.0.0.1:$Port") -WindowStyle Hidden -PassThru -RedirectStandardError "$log.cur" -RedirectStandardOutput "$log.out"
  $url = $null
  for ($i = 0; $i -lt 90 -and -not $url -and -not $p.HasExited; $i++) {
    Start-Sleep 1
    $m = Select-String -Path "$log.cur" -Pattern 'https://[a-z0-9-]+\.trycloudflare\.com' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($m) { $url = $m.Matches[0].Value }
  }
  if ($url) {
    Add-Content $log "$(Get-Date -Format s) tunnel up: $url"
    # wait for DNS / edge, then publish
    for ($i = 0; $i -lt 30; $i++) { try { if ((Invoke-WebRequest "$url/healthz" -UseBasicParsing -TimeoutSec 10).Content -eq 'ok') { break } } catch {}; Start-Sleep 2 }
    Set-Content -Encoding UTF8 $OutFile $url
    $bad = 0
    while (-not $p.HasExited -and $bad -lt 3) {
      Start-Sleep 60
      try { if ((Invoke-WebRequest "$url/healthz" -UseBasicParsing -TimeoutSec 15).Content -eq 'ok') { $bad = 0 } else { $bad++ } } catch { $bad++ }
    }
    Add-Content $log "$(Get-Date -Format s) tunnel lost (exited=$($p.HasExited), failed checks=$bad) - restarting"
  } else { Add-Content $log "$(Get-Date -Format s) tunnel failed to start - retrying in 10 s" }
  Get-Content "$log.cur" -ErrorAction SilentlyContinue | Select-Object -Last 20 | Add-Content $log
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
  Start-Sleep 10
}
