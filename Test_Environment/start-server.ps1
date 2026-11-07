param([int]$Port = 8080)

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $projectRoot

if(Get-Command py -ErrorAction SilentlyContinue) {
    py (Join-Path $projectRoot 'bridge_server.py') --port $Port
}
elseif(Get-Command python -ErrorAction SilentlyContinue) {
    python (Join-Path $projectRoot 'bridge_server.py') --port $Port
}
else {
    throw '未找到 Python。请安装 Python，或使用任意静态 HTTP 服务器打开此目录。'
}
