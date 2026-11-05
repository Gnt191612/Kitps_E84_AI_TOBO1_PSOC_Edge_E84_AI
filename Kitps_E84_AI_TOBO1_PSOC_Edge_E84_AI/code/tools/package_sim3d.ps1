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
$bat = "@echo off`r`npowershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"%~dp0launch.ps1`"`r`n"
[IO.File]::WriteAllText((Join-Path $app '启动3D仿真.bat'), $bat, (New-Object Text.ASCIIEncoding))

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = Join-Path $dist 'Sim3D-Portable.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($app, $zip, [IO.Compression.CompressionLevel]::Optimal, $false)

# 用 .NET Framework 自带的 C# 编译器生成安装程序，软件内嵌在 exe 里。
# 文件名不含 setup/install，并用清单声明不需要管理员权限。
$exe = Join-Path $dist 'Sim3D-Windows.exe'
$csc = Join-Path ([Runtime.InteropServices.RuntimeEnvironment]::GetRuntimeDirectory()) 'csc.exe'
$src = Join-Path $tools 'package\installer.cs'
$manifest = Join-Path $tools 'package\installer.manifest'
$cscOut = & $csc /nologo /target:winexe /codepage:65001 /optimize+ "/out:$exe" "/win32manifest:$manifest" "/resource:$zip,Sim3D.zip" /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:System.Windows.Forms.dll "$src"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) { $cscOut; throw '安装程序编译失败' }
Get-ChildItem $dist -File | ForEach-Object { '{0}  {1:N0} bytes' -f $_.Name, $_.Length }
