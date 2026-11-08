param(
    [int]$Port = 8080,
    [string]$HostAddress = '127.0.0.1'
)

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $projectRoot
$serverScript = Join-Path $projectRoot 'bridge_server.py'
$pythonCommand = Get-Command py -ErrorAction SilentlyContinue

if($null -eq $pythonCommand) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
}

if($null -eq $pythonCommand) {
    throw 'Python was not found. Install Python 3 and try again.'
}

if(-not (Test-Path -LiteralPath $serverScript)) {
    throw "Bridge server script was not found: $serverScript"
}

$statusUrl = "http://${HostAddress}:$Port/api/status"
try {
    $currentStatus = Invoke-RestMethod -Uri $statusUrl -TimeoutSec 2
    if($currentStatus.ok) {
        Write-Host "Phone navigation bridge is already running at http://${HostAddress}:$Port"
        exit 0
    }
}
catch {
    # No active bridge on this port. Start a new instance below.
}

Write-Host "Starting phone navigation bridge at http://${HostAddress}:$Port"
& $pythonCommand.Source $serverScript --host $HostAddress --port $Port
exit $LASTEXITCODE
