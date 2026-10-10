using System;
using System.Collections.Generic;
using System.IO;
using System.Globalization;
using System.Text;
namespace DreamAirLauncher {
    public enum Operation { InstallPlay, InstallOpenPort, Rollback, Profile, FloorPreset, FloorCustom, FloorRead, FloorRestore, Diagnostics, HeadsetSettings }
    public enum HeightUnit { FeetInches, Centimeters, Meters }
    public sealed class Command {
        public string FileName, Arguments, WorkingDirectory, LogPath;
        public bool Elevate, DirectGui;
    }
    public sealed class LauncherModel {
        public readonly string Root;
        public LauncherModel(string root) { Root=Path.GetFullPath(root); }
        public static string Quote(string value) {
            if(value==null || value.IndexOf('\0')>=0)throw new ArgumentException("Invalid command argument.");
            var text=new StringBuilder("\"");int slashes=0;
            foreach(char c in value) {
                if(c=='\\'){slashes++;continue;}
                if(c=='\"')text.Append('\\',slashes*2+1);else text.Append('\\',slashes);
                text.Append(c);slashes=0;
            }
            text.Append('\\',slashes*2);text.Append('"');return text.ToString();
        }
        public static string Join(IEnumerable<string> values) { var quoted=new List<string>();foreach(var v in values)quoted.Add(Quote(v));return String.Join(" ",quoted); }
        public static double Height(HeightUnit unit,decimal first,decimal second) {
            decimal meters;
            switch(unit) {
                case HeightUnit.FeetInches:
                    if(first<0 || first!=Decimal.Truncate(first) || second<0 || second>=12)throw new ArgumentException("Use whole feet and inches from 0 to less than 12.");
                    meters=(first*12+second)*0.0254m;break;
                case HeightUnit.Centimeters:meters=first/100;break;
                case HeightUnit.Meters:meters=first;break;
                default:throw new ArgumentException("Choose a supported height unit.");
            }
            if(meters<0 || meters>10)throw new ArgumentException("Height must be between 0 and 10 metres.");return (double)meters;
        }
        private string Required(string name) { string path=Path.Combine(Root,name);if(!File.Exists(path))throw new FileNotFoundException("Release file is missing. Extract the complete ZIP and keep the launcher with it.",path);return path; }
        public Command Build(Operation operation,string logPath,string profile,HeightUnit unit,decimal first,decimal second) {
            if(operation==Operation.InstallPlay && profile=="Pimax")throw new ArgumentException("The Pimax native optics profile requires EVO OpenPort.");
            var command=new Command { WorkingDirectory=Root,LogPath=logPath };
            if(operation==Operation.Profile || operation==Operation.InstallPlay || operation==Operation.InstallOpenPort)
                if(profile!="Guard" && profile!="Jitter" && profile!="Smooth" && profile!="Baseline" && profile!="Pimax")throw new ArgumentException("Unknown tracking profile.");
            if(operation==Operation.HeadsetSettings) {
                command.FileName=Required(Path.Combine("CustomHeadsetGUI","custom-headset-gui.exe"));command.Arguments="";command.DirectGui=true;command.WorkingDirectory=Path.GetDirectoryName(command.FileName);return command;
            }
            string script;var extra=new List<string>();
            switch(operation) {
                case Operation.InstallPlay:case Operation.InstallOpenPort:case Operation.Rollback:
                    Required(operation==Operation.Rollback?"Rollback.ps1":"Install.ps1");script="Launcher-Elevated.ps1";command.Elevate=true;
                    extra.AddRange(new[]{"-Operation",operation==Operation.InstallPlay?"Install-Play":(operation==Operation.InstallOpenPort?"Install-OpenPort":"Rollback"),"-LogPath",Path.GetFullPath(logPath)});break;
                case Operation.Profile:
                    script="Set-Profile.ps1";extra.AddRange(new[]{"-Profile",profile});break;
                case Operation.FloorPreset:script="Floor-Height.ps1";extra.Add("-Preset6ft3");break;
                case Operation.FloorRead:script="Floor-Height.ps1";extra.Add("-Read");break;
                case Operation.FloorRestore:script="Floor-Height.ps1";extra.Add("-Restore");break;
                case Operation.FloorCustom:
                    Height(unit,first,second);script="Floor-Height.ps1";
                    if(unit==HeightUnit.FeetInches)extra.AddRange(new[]{"-Feet",first.ToString(CultureInfo.InvariantCulture),"-Inches",second.ToString(CultureInfo.InvariantCulture)});
                    else extra.AddRange(new[]{unit==HeightUnit.Centimeters?"-Centimeters":"-Meters",first.ToString(CultureInfo.InvariantCulture)});break;
                case Operation.Diagnostics:script="Collect-Diagnostics.ps1";break;
                default:throw new ArgumentException("Unknown launcher operation.");
            }
            if(operation==Operation.InstallPlay || operation==Operation.InstallOpenPort)extra.AddRange(new[]{"-Profile",profile});
            command.FileName=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),"System32","WindowsPowerShell","v1.0","powershell.exe");
            var arguments=new List<string>{"-NoLogo","-NoProfile","-ExecutionPolicy","Bypass","-File",Required(script)};arguments.AddRange(extra);command.Arguments=Join(arguments);return command;
        }
    }
}
