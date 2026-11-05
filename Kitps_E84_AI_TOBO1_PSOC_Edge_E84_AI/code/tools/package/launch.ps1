# 启动 3D 仿真：优先用 Edge / Chrome 的应用窗口打开，没有就用默认浏览器
param([switch]$DryRun)
$ErrorActionPreference = 'Stop'
$page = Join-Path $PSScriptRoot 'sim_3d.html'
$url = ([System.Uri]$page).AbsoluteUri   # 路径里有中文、空格、# 时也能正确转义

function Find-Browser {
    foreach ($exe in 'msedge.exe', 'chrome.exe') {
        foreach ($root in 'HKCU:', 'HKLM:', 'HKLM:\SOFTWARE\WOW6432Node') {
            $key = if ($root -like '*WOW6432Node') { "$root\Microsoft\Windows\CurrentVersion\App Paths\$exe" } else { "$root\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\$exe" }
            try {
                $p = (Get-ItemProperty -Path $key -ErrorAction Stop).'(default)'
                if ($p -and (Test-Path $p)) { return $p }
            } catch { }
        }
    }
    foreach ($p in "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe",
                   "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
                   "$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
                   "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
                   "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe") {
        if ($p -and (Test-Path $p)) { return $p }
    }
    return $null
}

$browser = Find-Browser
if ($DryRun) { "browser=$browser"; "url=$url"; exit 0 }
if ($browser) {
    Start-Process -FilePath $browser -ArgumentList "--app=$url", '--window-size=1500,950'
} else {
    Add-Type -AssemblyName PresentationFramework
    [System.Windows.MessageBox]::Show('没有找到 Edge 或 Chrome。3D 显示和串口连接需要这两种浏览器之一，将尝试用默认浏览器打开。', '3D 仿真') | Out-Null
    Start-Process $url
}
