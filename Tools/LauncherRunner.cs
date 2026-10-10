using System;
using System.Diagnostics;
using System.IO;
using System.Text;
namespace DreamAirLauncher {
    public static class CommandRunner {
        public static int Run(Command command,Action<string> receive) {
            using(var process=new Process()) {
                var text=new StringBuilder();
                process.StartInfo=new ProcessStartInfo(command.FileName,command.Arguments) { WorkingDirectory=command.WorkingDirectory,UseShellExecute=command.Elevate,CreateNoWindow=!command.Elevate,WindowStyle=ProcessWindowStyle.Hidden };
                if(command.Elevate)process.StartInfo.Verb="runas";
                else {
                    process.StartInfo.RedirectStandardOutput=true;process.StartInfo.RedirectStandardError=true;
                    DataReceivedEventHandler onLine=(s,e)=>{if(e.Data==null)return;lock(text){text.AppendLine(e.Data);}receive(e.Data);};
                    process.OutputDataReceived+=onLine;process.ErrorDataReceived+=onLine;
                }
                process.Start();if(!command.Elevate){process.BeginOutputReadLine();process.BeginErrorReadLine();}
                process.WaitForExit();int result=process.ExitCode;
                if(command.Elevate) {
                    if(File.Exists(command.LogPath))receive(File.ReadAllText(command.LogPath));
                    else receive("No action log was written. Windows may have blocked the elevated process.");
                } else {
                    try{File.WriteAllText(command.LogPath,text.ToString());}catch(Exception e){receive("Could not save action log: "+e.Message);}
                }
                return result;
            }
        }
    }
}
