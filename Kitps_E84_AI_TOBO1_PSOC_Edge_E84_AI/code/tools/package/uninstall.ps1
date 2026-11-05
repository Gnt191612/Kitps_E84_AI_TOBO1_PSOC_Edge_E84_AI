# 卸载：删除快捷方式和安装目录
Add-Type -AssemblyName PresentationFramework
$r = [System.Windows.MessageBox]::Show('确定卸载 3D 仿真吗？', '3D 仿真', 'YesNo')
if ($r -ne 'Yes') { exit 0 }
$desktop = Join-Path ([Environment]::GetFolderPath('Desktop')) '3D 仿真.lnk'
if (Test-Path $desktop) { Remove-Item -Force $desktop }
$menu = Join-Path ([Environment]::GetFolderPath('Programs')) '多目标跟踪 3D 仿真'
if (Test-Path $menu) { Remove-Item -Recurse -Force $menu }
$dir = $PSScriptRoot
# 脚本还在运行，交给一个延时的命令行删除自身目录；只在确认是安装目录时才删
if (Test-Path (Join-Path $dir 'sim_3d.html')) {
    Start-Process -FilePath "$env:SystemRoot\System32\cmd.exe" -ArgumentList "/c timeout /t 2 >nul & rmdir /s /q `"$dir`"" -WindowStyle Hidden
}
