// 3D 仿真安装程序：把内嵌的 zip 解压到当前用户目录，建快捷方式并启动。
// 不需要管理员权限，不改系统设置，也不需要联网。
// 环境变量：SIM3D_DEST 指定安装位置；SIM3D_QUIET=1 不建快捷方式、不启动、不弹窗；SIM3D_LNK_DIR 把快捷方式建到指定目录且不启动（用于自动测试）。
using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Windows.Forms;

static class Installer
{
    const string Title = "3D 仿真";

    static string PowerShell()
    {
        return Path.Combine(Environment.GetEnvironmentVariable("SystemRoot"), @"System32\WindowsPowerShell\v1.0\powershell.exe");
    }

    static void Extract(string dest)
    {
        // 只清理自己装过的目录（有 sim_3d.html 才算），避免误删别的内容
        if (Directory.Exists(dest) && File.Exists(Path.Combine(dest, "sim_3d.html"))) Directory.Delete(dest, true);
        Directory.CreateDirectory(dest);
        Assembly asm = Assembly.GetExecutingAssembly();
        using (Stream s = asm.GetManifestResourceStream("Sim3D.zip"))
        using (ZipArchive zip = new ZipArchive(s, ZipArchiveMode.Read))
        {
            string root = Path.GetFullPath(dest) + Path.DirectorySeparatorChar;
            foreach (ZipArchiveEntry e in zip.Entries)
            {
                string target = Path.GetFullPath(Path.Combine(dest, e.FullName));
                if (!target.StartsWith(root, StringComparison.OrdinalIgnoreCase)) continue;
                if (e.FullName.EndsWith("/") || e.FullName.EndsWith("\\")) { Directory.CreateDirectory(target); continue; }
                Directory.CreateDirectory(Path.GetDirectoryName(target));
                e.ExtractToFile(target, true);
            }
        }
    }

    static void Shortcut(string lnk, string dest, string script, string desc)
    {
        Type t = Type.GetTypeFromProgID("WScript.Shell");
        object shell = Activator.CreateInstance(t);
        object sc = t.InvokeMember("CreateShortcut", BindingFlags.InvokeMethod, null, shell, new object[] { lnk });
        Type st = sc.GetType();
        Action<string, string> set = delegate (string k, string v) { st.InvokeMember(k, BindingFlags.SetProperty, null, sc, new object[] { v }); };
        set("TargetPath", PowerShell());
        set("Arguments", "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + Path.Combine(dest, script) + "\"");
        set("WorkingDirectory", dest);
        set("Description", desc);
        set("IconLocation", Path.Combine(Environment.GetEnvironmentVariable("SystemRoot"), @"System32\shell32.dll") + ",14");
        st.InvokeMember("Save", BindingFlags.InvokeMethod, null, sc, null);
    }

    [STAThread]
    static int Main()
    {
        bool quiet = Environment.GetEnvironmentVariable("SIM3D_QUIET") == "1";
        try
        {
            string dest = Environment.GetEnvironmentVariable("SIM3D_DEST");
            if (string.IsNullOrEmpty(dest))
                dest = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Sim3D");
            Extract(dest);
            if (quiet) return 0;

            string lnkDir = Environment.GetEnvironmentVariable("SIM3D_LNK_DIR");
            bool testLinks = !string.IsNullOrEmpty(lnkDir);
            string desktop = testLinks ? lnkDir : Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
            string menu = testLinks ? Path.Combine(lnkDir, "menu") : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), "多目标跟踪 3D 仿真");
            Directory.CreateDirectory(desktop);
            Directory.CreateDirectory(menu);
            Shortcut(Path.Combine(desktop, "3D 仿真.lnk"), dest, "launch.ps1", "打开 3D 仿真");
            Shortcut(Path.Combine(menu, "3D 仿真.lnk"), dest, "launch.ps1", "打开 3D 仿真");
            Shortcut(Path.Combine(menu, "卸载.lnk"), dest, "uninstall.ps1", "卸载 3D 仿真");
            if (testLinks) return 0;

            ProcessStartInfo psi = new ProcessStartInfo(PowerShell(), "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + Path.Combine(dest, "launch.ps1") + "\"");
            psi.UseShellExecute = false;
            psi.CreateNoWindow = true;
            Process.Start(psi);
            return 0;
        }
        catch (Exception ex)
        {
            if (quiet || !string.IsNullOrEmpty(Environment.GetEnvironmentVariable("SIM3D_LNK_DIR"))) { Console.Error.WriteLine(ex); return 1; }
            MessageBox.Show("安装失败：" + ex.Message, Title, MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }
}
