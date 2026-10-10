using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using DreamAirLauncher;
class LauncherTests {
    static void Check(bool ok) { if(!ok)throw new Exception("Assertion failed"); }
    static void Reject(Action action) { bool failed=false;try{action();}catch{failed=true;}Check(failed); }
    static int Main(string[] args) {
        if(args.Length>0 && args[0]=="--echo") { for(int i=1;i<args.Length;i++)Console.WriteLine(Convert.ToBase64String(Encoding.UTF8.GetBytes(args[i])));return 0; }
        int failed=0,total=0;
        Action<string,Action> test=(name,run)=>{total++;try{run();Console.WriteLine("PASS "+name);}catch(Exception e){failed++;Console.WriteLine("FAIL "+name+": "+e.Message);}};
        string root=Path.Combine(Path.GetFullPath(".."),"launcher-tests",Guid.NewGuid().ToString(),"folder with spaces & apostrophe's");Directory.CreateDirectory(root);
        foreach(string name in new[]{"Install.ps1","Rollback.ps1","Set-Profile.ps1","Floor-Height.ps1","Collect-Diagnostics.ps1","Launcher-Elevated.ps1"})File.WriteAllText(Path.Combine(root,name),"");
        Directory.CreateDirectory(Path.Combine(root,"CustomHeadsetGUI"));File.WriteAllText(Path.Combine(root,"CustomHeadsetGUI","custom-headset-gui.exe"),"");
        var model=new LauncherModel(root);string log=Path.Combine(root,"log with spaces.txt");
        Func<Operation,Command> build=op=>model.Build(op,log,"Guard",HeightUnit.FeetInches,6,3);
        test("Windows argument roundtrip with spaces quotes and trailing slashes",()=>{
            string[] values={"--echo","","two words","quote\"inside",@"C:\path with spaces\","apostrophe's & $(literal)","\u03bb unicode"};
            var p=new Process();p.StartInfo=new ProcessStartInfo(typeof(LauncherTests).Assembly.Location,LauncherModel.Join(values)){UseShellExecute=false,RedirectStandardOutput=true,CreateNoWindow=true};p.Start();string output=p.StandardOutput.ReadToEnd();p.WaitForExit();Check(p.ExitCode==0);
            string[] lines=output.Split(new[]{"\r\n","\n"},StringSplitOptions.None);Check(lines.Length>=values.Length);for(int i=1;i<values.Length;i++)Check(Encoding.UTF8.GetString(Convert.FromBase64String(lines[i-1]))==values[i]);p.Dispose();
        });
        test("6ft3 and custom units preserve nominal target",()=>{Check(Math.Abs(LauncherModel.Height(HeightUnit.FeetInches,6,3)-1.905)<1e-12);Check(Math.Abs(LauncherModel.Height(HeightUnit.Centimeters,180,0)-1.8)<1e-12);Check(LauncherModel.Height(HeightUnit.Meters,1.8m,0)==1.8);});
        test("invalid custom heights never create launch request",()=>{Reject(()=>LauncherModel.Height(HeightUnit.FeetInches,6,12));Reject(()=>LauncherModel.Height(HeightUnit.FeetInches,6.5m,0));Reject(()=>LauncherModel.Height(HeightUnit.Meters,-1,0));Reject(()=>LauncherModel.Height(HeightUnit.Meters,10.1m,0));});
        test("only driver install and rollback request elevation",()=>{foreach(Operation op in Enum.GetValues(typeof(Operation)))Check(build(op).Elevate==(op==Operation.InstallPlay || op==Operation.InstallOpenPort || op==Operation.Rollback));});
        test("PowerShell uses file arguments instead of command text",()=>{var c=build(Operation.FloorPreset);Check(c.Arguments.Contains("-File") && !c.Arguments.Contains("-Command") && c.WorkingDirectory==root && c.Arguments.Contains("-Preset6ft3"));});
        test("custom unit request uses invariant decimal value",()=>{var c=model.Build(Operation.FloorCustom,log,"Guard",HeightUnit.Meters,1.905m,0);Check(c.Arguments.Contains("-Meters") && c.Arguments.Contains("1.905"));});
        test("profile names are bounded before command execution",()=>{Reject(()=>model.Build(Operation.Profile,log,"Guard; injected",HeightUnit.Meters,1,0));});
        test("missing script prevents launch",()=>{File.Delete(Path.Combine(root,"Floor-Height.ps1"));Reject(()=>build(Operation.FloorPreset));});
        test("headset GUI launches from its own resource folder",()=>{var c=build(Operation.HeadsetSettings);Check(c.DirectGui && !c.Elevate && c.WorkingDirectory==Path.Combine(root,"CustomHeadsetGUI"));});
        test("background script preserves errors exit code and action log",()=>{
            File.WriteAllText(Path.Combine(root,"Floor-Height.ps1"),"param([switch]$Read)\nWrite-Host 'stdout marker'\n[Console]::Error.WriteLine('stderr marker')\nexit 7\n");
            var c=build(Operation.FloorRead);int result=CommandRunner.Run(c,line=>{});Check(result==7);string text=File.ReadAllText(log);Check(text.Contains("stdout marker") && text.Contains("stderr marker"));
        });
        Console.WriteLine((total-failed)+"/"+total+" passed");return failed==0?0:1;
    }
}
