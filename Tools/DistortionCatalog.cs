using System;
using System.Collections.Generic;
using System.IO;
using System.Web.Script.Serialization;

namespace DreamAirLauncher {
    public sealed class DistortionCatalog {
        public const string DefaultName="Dream Air Maximum Comfort 100pct Overlap";
        public string[] Names,Warnings;
        public string SelectedName;
        public static void ValidateName(string name) {
            if(String.IsNullOrWhiteSpace(name) || name!=name.Trim() || name.Length>180 || name=="." || name==".." || name.EndsWith(".") || name.IndexOfAny(Path.GetInvalidFileNameChars())>=0)
                throw new ArgumentException("Choose a valid distortion profile name.");
            foreach(char c in name)if(Char.IsControl(c))throw new ArgumentException("Choose a valid distortion profile name.");
            string stem=name.Split('.')[0].ToUpperInvariant();
            if(stem=="CON" || stem=="PRN" || stem=="AUX" || stem=="NUL" || (stem.Length==4 && (stem.StartsWith("COM") || stem.StartsWith("LPT")) && stem[3]>='1' && stem[3]<='9'))
                throw new ArgumentException("Choose a valid distortion profile name.");
        }
        private static Dictionary<string,object> Read(string path) {
            for(string cursor=Path.GetFullPath(path);!String.IsNullOrEmpty(cursor);cursor=Path.GetDirectoryName(cursor))
                if((File.Exists(cursor) || Directory.Exists(cursor)) && (File.GetAttributes(cursor)&FileAttributes.ReparsePoint)!=0)throw new IOException("Linked profile paths are unsupported.");
            var serializer=new JavaScriptSerializer { MaxJsonLength=4*1024*1024 };
            var value=serializer.DeserializeObject(File.ReadAllText(path)) as Dictionary<string,object>;
            if(value==null)throw new FormatException("Expected a JSON object.");return value;
        }
        private static bool Number(object value) {
            if(!(value is byte || value is sbyte || value is short || value is ushort || value is int || value is uint || value is long || value is ulong || value is float || value is double || value is decimal))return false;
            double number=Convert.ToDouble(value);return !Double.IsNaN(number) && !Double.IsInfinity(number);
        }
        private static bool Curve(object value,int minimum,bool increasing) {
            var points=value as object[];if(points==null || points.Length<minimum || points.Length%2!=0)return false;
            for(int i=0;i<points.Length;i++){if(!Number(points[i]))return false;if(increasing && i%2==0 && (Convert.ToDouble(points[i])<0 || (i>0 && Convert.ToDouble(points[i])<=Convert.ToDouble(points[i-2]))))return false;}return true;
        }
        private static bool Compatible(Dictionary<string,object> data) {
            object value;if(!data.TryGetValue("device",out value))return false;
            bool matches=value as string=="DreamAir";var devices=value as object[];
            if(devices!=null){foreach(var device in devices){if(!(device is string))return false;if((string)device=="DreamAir")matches=true;}}
            if(!matches || !data.TryGetValue("type",out value) || value as string!="RadialBezier" || !data.TryGetValue("distortions",out value) || !Curve(value,4,true))return false;
            foreach(string name in new[]{"distortionsRed","distortionsBlue"})if(data.TryGetValue(name,out value) && !Curve(value,0,false))return false;
            foreach(string name in new[]{"offsetX","offsetY","eyeRotationOffset","smoothAmount"})if(data.TryGetValue(name,out value) && !Number(value))return false;
            if(data.TryGetValue("legacySmoothing",out value) && !(value is bool))return false;return true;
        }
        public static DistortionCatalog Load(string releaseRoot,string configRoot) {
            var profiles=new Dictionary<string,bool>(StringComparer.Ordinal);var warnings=new List<string>();
            string info=Path.Combine(configRoot,"info.json");
            if(File.Exists(info))try {
                object value;var data=Read(info);
                if(data.TryGetValue("builtInDistortionProfiles",out value)) {
                    var builtins=value as Dictionary<string,object>;if(builtins==null)throw new FormatException("Invalid built-in profile metadata.");
                    foreach(var entry in builtins)try{ValidateName(entry.Key);var profile=entry.Value as Dictionary<string,object>;if(profile!=null && Compatible(profile))profiles[entry.Key]=true;}catch(ArgumentException){}
                }
            }catch(Exception e){warnings.Add("Could not read built-in distortion profiles: "+e.Message);}
            foreach(string directory in new[]{Path.Combine(releaseRoot,"Distortion"),Path.Combine(configRoot,"Distortion")}) {
                if(!Directory.Exists(directory))continue;
                try{foreach(string file in Directory.GetFiles(directory,"*.json")) {
                    string name=Path.GetFileNameWithoutExtension(file);bool builtin;
                    try{ValidateName(name);}catch(ArgumentException){continue;}
                    if(profiles.TryGetValue(name,out builtin) && builtin)continue;
                    profiles.Remove(name);
                    try{if(Compatible(Read(file)))profiles[name]=false;}catch(Exception){warnings.Add("Skipped invalid distortion profile: "+Path.GetFileName(file));}
                }}catch(Exception e){warnings.Add("Could not read distortion profiles: "+e.Message);}
            }
            var names=new List<string>(profiles.Keys);names.Sort(StringComparer.OrdinalIgnoreCase);
            if(names.Remove(DefaultName))names.Insert(0,DefaultName);
            string selected=names.Contains(DefaultName)?DefaultName:(names.Count>0?names[0]:"");
            string settings=Path.Combine(configRoot,"settings.json");
            if(File.Exists(settings))try {
                object value;var data=Read(settings);
                if(data.TryGetValue("dreamAir",out value)){
                    var headset=value as Dictionary<string,object>;
                    if(headset!=null && headset.TryGetValue("distortionProfile",out value) && value is string && !String.IsNullOrWhiteSpace((string)value)){
                        if(profiles.ContainsKey((string)value))selected=(string)value;
                        else warnings.Add("Current distortion profile is unavailable: "+(string)value+". Settings were preserved.");
                    }
                }
            }catch(Exception e){warnings.Add("Could not read the current distortion setting: "+e.Message);}
            return new DistortionCatalog { Names=names.ToArray(),SelectedName=selected,Warnings=warnings.ToArray() };
        }
    }
}
