using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Threading.Tasks;
using System.Threading;
using System.Windows.Forms;

namespace DreamAirLauncher {
    public sealed class LauncherForm:Form {
        private readonly LauncherModel model;
        private readonly Panel actions;
        private readonly TextBox output;
        private readonly Label status,preview,profileHelp,firstLabel,secondLabel;
        private readonly ComboBox units,profiles,backend;
        private readonly NumericUpDown first,second;
        private HeightUnit previousUnit=HeightUnit.FeetInches;
        private bool changingUnits,busy,previewMode;
        private readonly Color ink=Color.FromArgb(25,38,56),accent=Color.FromArgb(31,86,184);
        public LauncherForm(string root) {
            model=new LauncherModel(root);Text="Dream Air LH1 | Release launcher";
            Font=new Font("Segoe UI",10);BackColor=Color.FromArgb(243,246,250);ForeColor=ink;
            AutoScaleDimensions=new SizeF(96,96);AutoScaleMode=AutoScaleMode.Dpi;
            ClientSize=new Size(1000,760);MinimumSize=new Size(950,770);StartPosition=FormStartPosition.CenterScreen;
            var layout=new TableLayoutPanel { Dock=DockStyle.Fill,Padding=new Padding(24),ColumnCount=1,RowCount=5 };
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute,78));layout.RowStyles.Add(new RowStyle(SizeType.Absolute,380));
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute,30));layout.RowStyles.Add(new RowStyle(SizeType.Percent,100));layout.RowStyles.Add(new RowStyle(SizeType.Absolute,44));
            Controls.Add(layout);
            var header=new Panel { Dock=DockStyle.Fill };
            header.Controls.Add(TextLabel("Dream Air LH1",0,0,650,38,new Font("Segoe UI",23,FontStyle.Bold)));
            header.Controls.Add(TextLabel("Experimental release 0.3.4  |  Lighthouse 1.0  |  Sync cable A/b",2,44,750,26,null));layout.Controls.Add(header,0,0);
            actions=new Panel { Dock=DockStyle.Fill };
            var cards=new TableLayoutPanel { Dock=DockStyle.Fill,ColumnCount=2,RowCount=1 };
            cards.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,45));cards.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,55));actions.Controls.Add(cards);layout.Controls.Add(actions,0,1);
            var driver=Card("Driver and tracking",new Padding(0,0,12,0));var floor=Card("Floor height",new Padding(0));cards.Controls.Add(driver,0,0);cards.Controls.Add(floor,1,0);
            driver.Controls.Add(TextLabel("Connection",18,32,180,24,null));backend=SelectBox(18,60,180,new[]{"Pimax Play","EVO OpenPort"});driver.Controls.Add(backend);
            backend.SelectedIndex=1;
            driver.Controls.Add(Button("Install driver",215,58,166,34,()=>Run(backend.SelectedIndex==0?Operation.InstallPlay:Operation.InstallOpenPort),true));
            driver.Controls.Add(TextLabel("Close SteamVR and headset settings before installing or restoring the driver.",18,104,364,48,null));
            driver.Controls.Add(Button("Headset settings",18,158,180,34,()=>Run(Operation.HeadsetSettings),false));
            driver.Controls.Add(Button("Restore driver",215,158,166,34,()=>Run(Operation.Rollback),false));
            driver.Controls.Add(TextLabel("Tracking profile",18,211,180,24,null));profiles=SelectBox(18,239,180,new[]{"Pimax","Guard","Jitter","Smooth","Baseline"});driver.Controls.Add(profiles);
            driver.Controls.Add(Button("Apply profile",215,237,166,34,()=>Run(Operation.Profile),false));
            profileHelp=TextLabel("",18,283,364,47,null);driver.Controls.Add(profileHelp);
            profiles.SelectedIndexChanged+=(s,e)=>ProfileDescription();ProfileDescription();
            driver.Controls.Add(Button("Collect diagnostics (30 seconds)",18,331,363,34,()=>Run(Operation.Diagnostics),false));
            floor.Controls.Add(TextLabel("Keep SteamVR running. Hold the headset still at your reference position; capture starts after five seconds.",18,32,463,46,null));
            floor.Controls.Add(Button("Set floor to 6 ft 3 in",18,91,225,38,()=>Run(Operation.FloorPreset),true));
            floor.Controls.Add(Button("Read current height",261,91,220,38,()=>Run(Operation.FloorRead),false));
            floor.Controls.Add(TextLabel("Custom headset-to-floor height",18,149,440,25,null));
            units=SelectBox(18,180,164,new[]{"Feet + inches","Centimetres","Metres"});floor.Controls.Add(units);
            first=Number(200,180,102,32,0,6);second=Number(370,180,80,11.999m,3,3);
            firstLabel=TextLabel("ft",308,183,42,26,null);secondLabel=TextLabel("in",455,183,28,26,null);
            floor.Controls.Add(first);floor.Controls.Add(second);floor.Controls.Add(firstLabel);floor.Controls.Add(secondLabel);
            preview=TextLabel("",18,221,463,30,new Font("Segoe UI",11,FontStyle.Bold));floor.Controls.Add(preview);
            units.SelectedIndexChanged+=(s,e)=>ChangeUnits();first.ValueChanged+=(s,e)=>UpdatePreview();second.ValueChanged+=(s,e)=>UpdatePreview();UpdatePreview();
            floor.Controls.Add(Button("Apply custom height",18,269,225,36,()=>Run(Operation.FloorCustom),false));
            floor.Controls.Add(Button("Restore previous floor",261,269,220,36,()=>Run(Operation.FloorRestore),false));
            floor.Controls.Add(TextLabel("The floor stays fixed after applying. Crouching still works. Floor backups stay with this release.",18,325,463,45,null));
            status=TextLabel("Ready. Opening this launcher does not change your driver or floor.",0,0,920,28,null);status.Dock=DockStyle.Fill;layout.Controls.Add(status,0,2);
            output=new TextBox { Dock=DockStyle.Fill,Multiline=true,ScrollBars=ScrollBars.Vertical,ReadOnly=true,BackColor=Color.White,BorderStyle=BorderStyle.FixedSingle,Font=new Font("Consolas",9) };
            output.Text="Choose an action above.\n\nPimax uses native Pimax tracking in the existing floor space with your selected distortion profile. Use EVO OpenPort. Close SteamVR before changing to or from this profile.\n\nInstall/profile actions preserve your force-tracking settings. Floor writes require force-tracking flags off.";layout.Controls.Add(output,0,3);
            var footer=new FlowLayoutPanel { Dock=DockStyle.Fill,FlowDirection=FlowDirection.LeftToRight,Padding=new Padding(0,7,0,0) };
            var guide=Button("Open guide",0,0,125,30,ShowGuide,false);guide.Margin=new Padding(0,0,10,0);footer.Controls.Add(guide);
            footer.Controls.Add(Button("Release folder",0,0,145,30,()=>OpenFolder(model.Root),false));
            footer.Controls.Add(Button("Action logs",0,0,125,30,()=>OpenFolder(Path.Combine(model.Root,"LauncherLogs")),false));layout.Controls.Add(footer,0,4);
            if(!File.Exists(Path.Combine(model.Root,"Floor-Height.ps1")) || !File.Exists(Path.Combine(model.Root,"Install.ps1"))) {
                actions.Enabled=false;status.ForeColor=Color.Firebrick;status.Text="Release files are missing. Extract the complete ZIP and keep this EXE inside its release folder.";
            }
            FormClosing+=(s,e)=>{if(busy && e.CloseReason==CloseReason.UserClosing){e.Cancel=true;status.Text="An action is running. Wait for it to finish before closing.";}};
        }
        private Label TextLabel(string text,int x,int y,int w,int h,Font font) { return new Label { Text=text,Location=new Point(x,y),Size=new Size(w,h),Font=font??Font,ForeColor=ink }; }
        private GroupBox Card(string title,Padding margin) { return new GroupBox { Text=title,Dock=DockStyle.Fill,Margin=margin,BackColor=Color.White,Font=new Font("Segoe UI",10),ForeColor=ink }; }
        private ComboBox SelectBox(int x,int y,int width,string[] items) { var box=new ComboBox { Location=new Point(x,y),Width=width,DropDownStyle=ComboBoxStyle.DropDownList };box.Items.AddRange(items);box.SelectedIndex=0;return box; }
        private NumericUpDown Number(int x,int y,int width,decimal max,int places,decimal value) { return new NumericUpDown { Location=new Point(x,y),Width=width,Minimum=0,Maximum=max,DecimalPlaces=places,Value=value,Increment=places==0?1:0.1m }; }
        private Button Button(string text,int x,int y,int width,int height,Action action,bool primary) {
            var button=new Button { Text=text,Location=new Point(x,y),Size=new Size(width,height),FlatStyle=FlatStyle.Flat,BackColor=primary?accent:Color.FromArgb(245,247,250),ForeColor=primary?Color.White:ink,UseVisualStyleBackColor=false };
            button.FlatAppearance.BorderColor=primary?accent:Color.FromArgb(209,216,226);button.Click+=(s,e)=>action();return button;
        }
        private void ProfileDescription() {
            switch(profiles.SelectedIndex) {
                case 1:profileHelp.Text="Guard rejects invalid Lighthouse poses. It can gray-screen despite force tracking.";break;
                case 2:profileHelp.Text="Jitter adds position filtering to Guard; it can add delay.";break;
                case 3:profileHelp.Text="Smooth also filters rotation and can add delay during fast movement.";break;
                case 4:profileHelp.Text="Baseline uses Lighthouse poses without the guard. Existing force flags are preserved.";break;
                default:profileHelp.Text="Native Pimax tracking with custom optics. Keeps the existing room and floor. Requires OpenPort and a SteamVR restart.";break;
            }
        }
        private double CurrentHeight() { return LauncherModel.Height((HeightUnit)units.SelectedIndex,first.Value,second.Value); }
        private void UpdatePreview() {
            if(changingUnits)return;
            try{preview.Text="Target: "+CurrentHeight().ToString("0.0000",CultureInfo.InvariantCulture)+" m below headset";preview.ForeColor=ink;}
            catch(Exception e){preview.Text=e.Message;preview.ForeColor=Color.Firebrick;}
        }
        private void ChangeUnits() {
            if(changingUnits)return;decimal height;
            try{height=(decimal)LauncherModel.Height(previousUnit,first.Value,second.Value);}catch{height=1.905m;}
            changingUnits=true;
            first.Maximum=1000;
            var unit=(HeightUnit)units.SelectedIndex;
            if(unit==HeightUnit.FeetInches) {
                decimal total=height/0.0254m;first.DecimalPlaces=0;first.Value=Decimal.Truncate(total/12);first.Maximum=32;
                second.Value=Decimal.Truncate((total-first.Value*12)*1000)/1000;second.Visible=true;secondLabel.Visible=true;firstLabel.Text="ft";
            } else {
                first.DecimalPlaces=unit==HeightUnit.Centimeters?2:4;first.Value=unit==HeightUnit.Centimeters?height*100:height;
                first.Maximum=unit==HeightUnit.Centimeters?1000:10;firstLabel.Text=unit==HeightUnit.Centimeters?"cm":"m";second.Visible=false;secondLabel.Visible=false;
            }
            previousUnit=unit;changingUnits=false;UpdatePreview();
        }
        private void Log(string text) { if(IsDisposed)return;output.AppendText(text+Environment.NewLine);output.SelectionStart=output.TextLength;output.ScrollToCaret(); }
        private void Receive(string text) { if(text==null || IsDisposed)return;try{BeginInvoke((Action)(()=>Log(text)));}catch(InvalidOperationException){} }
        private async void Run(Operation operation) {
            if(busy)return;Command command;string logPath;
            try {
                logPath=Path.Combine(model.Root,"LauncherLogs",DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N")+".txt");
                command=model.Build(operation,logPath,profiles.Text,(HeightUnit)units.SelectedIndex,first.Value,second.Value);
                if(command.DirectGui){Process.Start(new ProcessStartInfo(command.FileName){WorkingDirectory=command.WorkingDirectory,UseShellExecute=true});status.Text="Opened headset settings.";return;}
                Directory.CreateDirectory(Path.GetDirectoryName(logPath));
            } catch(Exception e){status.ForeColor=Color.Firebrick;status.Text=e.Message;Log(e.Message);return;}
            busy=true;actions.Enabled=false;status.ForeColor=ink;
            status.Text=operation==Operation.FloorPreset || operation==Operation.FloorCustom?"Floor capture starts after five seconds. Hold the headset still.":"Running "+operation+"...";
            Log("\n--- "+operation+" ---");
            try {
                int result=await Task.Run(()=>CommandRunner.Run(command,Receive));
                status.ForeColor=result==0?Color.FromArgb(24,112,65):Color.Firebrick;
                status.Text=result==0?"Completed. See the result below.":"Action failed (exit "+result+"). See the result below.";
            } catch(Win32Exception e) {status.ForeColor=Color.Firebrick;status.Text=e.NativeErrorCode==1223?"Windows elevation was cancelled.":e.Message;Log(status.Text);}
            catch(Exception e){status.ForeColor=Color.Firebrick;status.Text=e.Message;Log(e.Message);}
            finally {busy=false;actions.Enabled=true;}
        }
        private void OpenFolder(string path) { try{if(!Directory.Exists(path)){Log("This folder is created after an action runs: "+path);return;}Process.Start(new ProcessStartInfo("explorer.exe",LauncherModel.Quote(path)){UseShellExecute=true});}catch(Exception e){Log(e.Message);} }
        private void ShowGuide() {
            try {
                var form=new Form { Text="Dream Air LH1 | Guide",Size=new Size(920,760),StartPosition=FormStartPosition.CenterParent };
                form.Controls.Add(new RichTextBox { Dock=DockStyle.Fill,ReadOnly=true,Text=File.ReadAllText(Path.Combine(model.Root,"START-HERE.md")),Font=new Font("Segoe UI",10),BackColor=Color.White,DetectUrls=false });form.Show(this);
            } catch(Exception e){Log(e.Message);}
        }
        protected override bool ShowWithoutActivation { get { return previewMode; } }
        public void RenderPreview(string file) {
            previewMode=true;ShowInTaskbar=false;Opacity=0;Show();Application.DoEvents();PerformLayout();
            using(var image=new Bitmap(Width,Height)){DrawToBitmap(image,new Rectangle(0,0,Width,Height));image.Save(file,System.Drawing.Imaging.ImageFormat.Png);}Close();
        }
        public void SmokeTest(string file) {
            if(!File.Exists(Path.Combine(model.Root,".launcher-ui-sandbox")))throw new InvalidOperationException("UI smoke test needs its isolated sandbox marker.");
            previewMode=true;ShowInTaskbar=false;Opacity=0;Show();Application.DoEvents();
            units.SelectedIndex=2;if(Math.Abs(CurrentHeight()-1.905)>1e-9)throw new Exception("Unit conversion lost target height.");
            units.SelectedIndex=1;if(Math.Abs(CurrentHeight()-1.905)>1e-9)throw new Exception("Metric unit conversion lost target height.");units.SelectedIndex=0;
            output.Clear();Run(Operation.FloorRead);var elapsed=Stopwatch.StartNew();
            while(busy && elapsed.ElapsedMilliseconds<15000){Application.DoEvents();Thread.Sleep(10);}
            Application.DoEvents();
            if(busy || !actions.Enabled || !units.Enabled || !first.Enabled || !status.Text.StartsWith("Completed") || !output.Text.Contains("LAUNCHER-SMOKE-READ-ONLY"))throw new Exception("UI result/callback smoke test failed.");
            PerformLayout();Refresh();Application.DoEvents();using(var image=new Bitmap(Width,Height)){DrawToBitmap(image,new Rectangle(0,0,Width,Height));image.Save(file,System.Drawing.Imaging.ImageFormat.Png);}Close();
        }
    }
    static class Program {
        [STAThread]static int Main(string[] args) {
            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
            try {
                if(args.Length==3 && args[0]=="--smoke-test") {using(var form=new LauncherForm(args[1]))form.RenderPreview(args[2]);return 0;}
                if(args.Length==3 && args[0]=="--ui-smoke-test") {using(var form=new LauncherForm(args[1]))form.SmokeTest(args[2]);return 0;}
                if(args.Length!=0)throw new ArgumentException("Open this launcher without arguments.");
                Application.Run(new LauncherForm(AppDomain.CurrentDomain.BaseDirectory));return 0;
            } catch(Exception e){if(args.Length==3 && args[0].Contains("smoke-test")){File.WriteAllText(args[2]+".error.txt",e.ToString());return 1;}MessageBox.Show(e.Message,"Dream Air LH1",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;}
        }
    }
}
