// 3D 仿真安装程序：把内嵌的 zip 解压到当前用户目录，建快捷方式并启动。
// 不需要管理员权限，不改系统设置，也不需要联网。写不进默认位置时换一个位置。
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

    // 依次尝试的安装位置。安全软件可能禁止未签名程序写 AppData，这时换一个用户可写的位置，不去绕过它。
    static string[] Candidates()
    {
        string exeDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
        return new string[] {
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Sim3D"),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Sim3D"),
            Path.Combine(exeDir, "Sim3D")
        };
    }

    static string Install(out string detail)
    {
        string one = Environment.GetEnvironmentVariable("SIM3D_DEST");
        string[] list = string.IsNullOrEmpty(one) ? Candidates() : new string[] { one };
        detail = "";
        foreach (string dest in list)
        {
            try { Extract(dest); return dest; }
            catch (UnauthorizedAccessException ex) { detail += dest + "：" + ex.Message + "\n"; }
            catch (IOException ex) { detail += dest + "：" + ex.Message + "\n"; }
        }
        return null;
    }

    [STAThread]
    static int Main()
    {
        bool quiet = Environment.GetEnvironmentVariable("SIM3D_QUIET") == "1";
        string lnkDir = Environment.GetEnvironmentVariable("SIM3D_LNK_DIR");
        bool testLinks = !string.IsNullOrEmpty(lnkDir);
        bool silent = quiet || testLinks;
        try
        {
            string detail;
            string dest = Install(out detail);
            if (dest == null)
            {
                string msg = "没有可写入的安装位置，可能被安全软件拦截：\n\n" + detail +
                    "\n可以在安全软件中信任本程序后重试，或改用免安装版 Sim3D-Portable.zip：解压后双击“启动3D仿真.bat”。";
                if (silent) Console.Error.WriteLine(msg); else MessageBox.Show(msg, Title, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return 1;
            }
            if (silent) Console.Out.WriteLine(dest);
            if (quiet) return 0;

            // 快捷方式只是方便，建不了不算安装失败
            bool linksOk = true;
            try
            {
                string desktop = testLinks ? lnkDir : Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
                string menu = testLinks ? Path.Combine(lnkDir, "menu") : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), "多目标跟踪 3D 仿真");
                Directory.CreateDirectory(desktop);
                Directory.CreateDirectory(menu);
                Shortcut(Path.Combine(desktop, "3D 仿真.lnk"), dest, "launch.ps1", "打开 3D 仿真");
                Shortcut(Path.Combine(menu, "3D 仿真.lnk"), dest, "launch.ps1", "打开 3D 仿真");
                Shortcut(Path.Combine(menu, "卸载.lnk"), dest, "uninstall.ps1", "卸载 3D 仿真");
            }
            catch (Exception) { linksOk = false; }
            if (testLinks) return linksOk ? 0 : 1;

            bool started = false;
            try
            {
                ProcessStartInfo psi = new ProcessStartInfo(PowerShell(), "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + Path.Combine(dest, "launch.ps1") + "\"");
                psi.UseShellExecute = false;
                psi.CreateNoWindow = true;
                Process.Start(psi);
                started = true;
            }
            catch (Exception) { }

            if (!linksOk || !started)
            {
                MessageBox.Show("已安装到：\n" + dest + "\n\n" +
                    (linksOk ? "" : "未能创建桌面快捷方式。") + (started ? "" : "未能自动打开软件。") +
                    "\n以后请打开这个文件夹，双击“启动3D仿真.bat”。", Title, MessageBoxButtons.OK, MessageBoxIcon.Information);
                Process.Start("explorer.exe", "\"" + dest + "\"");
            }
            return 0;
        }
        catch (Exception ex)
        {
            if (silent) { Console.Error.WriteLine(ex); return 1; }
            MessageBox.Show("安装失败：" + ex.Message, Title, MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }
}
