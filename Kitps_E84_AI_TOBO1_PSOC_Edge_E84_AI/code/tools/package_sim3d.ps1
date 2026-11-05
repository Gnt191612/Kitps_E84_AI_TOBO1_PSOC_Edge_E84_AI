# 打包 3D 仿真：生成 dist/Sim3D-Portable.zip、dist/Sim3D-Windows.exe 和可直接打开的文件夹
# 用法：powershell -ExecutionPolicy Bypass -File package_sim3d.ps1 [-Version v1.0.0]
param([string]$Version = 'dev')
$ErrorActionPreference = 'Stop'
$tools = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $tools '..\..\..')).Path
$dist = Join-Path $repo 'dist'
$app = Join-Path $dist 'app'
if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force $app | Out-Null

foreach ($f in 'sim_3d.html', 'demo_monitor.html', 'demo_sim_data.js', 'demo_sim.log') { Copy-Item (Join-Path $tools $f) $app }
foreach ($d in 'sim_3d', 'vendor') { Copy-Item -Recurse (Join-Path $tools $d) (Join-Path $app $d) }
foreach ($f in 'launch.ps1', 'uninstall.ps1') { Copy-Item (Join-Path $tools "package\$f") $app }
$utf8Bom = New-Object Text.UTF8Encoding($true)
$readme = [IO.File]::ReadAllText((Join-Path $tools 'package\readme.txt'), [Text.Encoding]::UTF8)
[IO.File]::WriteAllText((Join-Path $app '使用说明.txt'), $readme.Replace("`r`n", "`n").Replace("`n", "`r`n") + "`r`n版本：$Version`r`n", $utf8Bom)
$bat = "@echo off`r`nchcp 65001 >nul`r`nif not exist `"%~dp0launch.ps1`" (`r`n  echo 请先把压缩包全部解压到一个文件夹，再双击这个文件。不能在压缩包里直接运行。`r`n  pause`r`n  exit /b 1`r`n)`r`npowershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"%~dp0launch.ps1`"`r`nif errorlevel 1 (`r`n  echo 启动失败。请用 Edge 或 Chrome 打开这个文件夹里的 sim_3d.html。`r`n  pause`r`n)`r`n"
[IO.File]::WriteAllText((Join-Path $app '启动3D仿真.bat'), $bat, (New-Object Text.UTF8Encoding($false)))

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = Join-Path $dist 'Sim3D-Portable.zip'
# 逐个写入并统一用 / 作路径分隔：Windows PowerShell 自带的打包方式用 \，macOS / Linux 解压后目录会坏
Add-Type -AssemblyName System.IO.Compression
$stream = [IO.File]::Open($zip, [IO.FileMode]::Create)
$archive = New-Object IO.Compression.ZipArchive($stream, [IO.Compression.ZipArchiveMode]::Create)
$rootLen = (Resolve-Path $app).Path.TrimEnd('\').Length + 1
Get-ChildItem -LiteralPath $app -Recurse -File | ForEach-Object {
    $name = $_.FullName.Substring($rootLen).Replace('\', '/')
    $null = [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $_.FullName, $name, [IO.Compression.CompressionLevel]::Optimal)
}
$archive.Dispose(); $stream.Dispose()

# 用 .NET Framework 自带的 C# 编译器生成安装程序，软件内嵌在 exe 里。
# 文件名不含 setup/install，并用清单声明不需要管理员权限。
$exe = Join-Path $dist 'Sim3D-Windows.exe'
$csc = Join-Path ([Runtime.InteropServices.RuntimeEnvironment]::GetRuntimeDirectory()) 'csc.exe'
$src = Join-Path $tools 'package\installer.cs'
$manifest = Join-Path $tools 'package\installer.manifest'
$cscOut = & $csc /nologo /target:winexe /codepage:65001 /optimize+ "/out:$exe" "/win32manifest:$manifest" "/resource:$zip,Sim3D.zip" /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:System.Windows.Forms.dll "$src"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) { $cscOut; throw '安装程序编译失败' }
Get-ChildItem $dist -File | ForEach-Object { '{0}  {1:N0} bytes' -f $_.Name, $_.Length }
