"""Package the fork, unchanged official GUI/private helper, and corresponding public source."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

repo = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument('--upstream', required=True, type=Path)
parser.add_argument('--destination', required=True, type=Path)
args = parser.parse_args()
destination = args.destination.resolve()
if destination.exists():
    raise SystemExit('Destination already exists; choose a fresh package directory.')
upstream = args.upstream.resolve()
destination.mkdir(parents=True)
main = destination / 'CustomHeadsetOpenVR'
shutil.copytree(upstream / 'CustomHeadsetOpenVR', main)
shutil.copy2(repo / 'output/CustomHeadsetOpenVR/bin/win64/driver_CustomHeadsetOpenVR.dll', main / 'bin/win64/driver_CustomHeadsetOpenVR.dll')
manifest = json.loads((main / 'driver.vrdrivermanifest').read_text(encoding='utf-8-sig'))
manifest['version'] = '1.3.0-dreamair-lh1.6-experimental'
(main / 'driver.vrdrivermanifest').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
private = destination / 'CustomHeadsetOpenVR_Private'
shutil.copytree(upstream / 'CustomHeadsetOpenVR', private)
old_dll = private / 'bin/win64/driver_CustomHeadsetOpenVR.dll'
old_dll.rename(private / 'bin/win64/driver_CustomHeadsetOpenVR_Private.dll')
manifest = json.loads((private / 'driver.vrdrivermanifest').read_text(encoding='utf-8-sig'))
manifest['name'] = 'CustomHeadsetOpenVR_Private'
(private / 'driver.vrdrivermanifest').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
shutil.copytree(upstream / 'CustomHeadsetGUI', destination / 'CustomHeadsetGUI')
for item in (repo / 'Tools/Package').iterdir():
    if item.is_file(): shutil.copy2(item, destination / item.name)
tools = destination / 'Tools'
tools.mkdir()
for name in ['TrackingRecorder.exe', 'PoseTests.exe', 'NativePoseTests.exe', 'RuntimePoseTests.exe', 'RuntimeHeadsetTests.exe', 'LighthouseAlignmentTests.exe', 'FloorHeight.exe', 'FloorTests.exe', 'LauncherTests.exe', 'openvr_api.dll']:
    shutil.copy2(repo / 'output/Tools' / name, tools / name)
shutil.copy2(repo / 'output/Tools/DreamAir-LH1.exe', destination / 'DreamAir-LH1.exe')
shutil.copy2(repo / 'LICENCE.md', destination / 'LICENCE.md')
launches = {
    'Install-Play.cmd': ('Install.ps1', '-Backend Play'),
    'Install-OpenPort.cmd': ('Install.ps1', '-Backend OpenPort'),
    'Rollback.cmd': ('Rollback.ps1', ''),
    'Collect-Diagnostics.cmd': ('Collect-Diagnostics.ps1', ''),
    'Floor-6ft3.cmd': ('Floor-Height.ps1', '-Preset6ft3'),
    'Floor-Custom.cmd': ('Floor-Height.ps1', ''),
    'Floor-Read.cmd': ('Floor-Height.ps1', '-Read'),
    'Floor-Restore.cmd': ('Floor-Height.ps1', '-Restore'),
}
for profile in ['Guard', 'Jitter', 'Smooth', 'Baseline', 'Pimax']:
    launches[f'Profile-{profile}.cmd'] = ('Set-Profile.ps1', f'-Profile {profile}')
for name, (script, arguments) in launches.items():
    (destination / name).write_text(
        '@echo off\n'
        f'powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0{script}" {arguments} %*\n'
        'set "lh1_result=%errorlevel%"\necho.\npause\nexit /b %lh1_result%\n',
        encoding='ascii', newline='\r\n')
(destination / 'SOURCE-NOTICE.txt').write_text(
    'Unofficial experimental fork of sboys3 CustomHeadsetOpenVR 1.3.0. Public source: GPLv2.\n'
    'Original GUI and private helper DLL are unchanged official release binaries, included for the upstream documented private-only arrangement.\n'
    'Private helper code is not public. OpenPort does not require that helper.\n'
    'Source.zip includes corresponding public driver, GUI, dependencies, tests, build/package scripts, and original notices.\n'
    'https://github.com/sboys3/CustomHeadsetOpenVR/releases/tag/1.3.0\n', encoding='utf-8')
files = subprocess.check_output(['git', 'ls-files', '-z', '--recurse-submodules'], cwd=repo).decode('utf-8').split('\0')
with zipfile.ZipFile(destination / 'Source.zip', 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
    for name in sorted(set(files)):
        path = repo / name
        if name and path.is_file(): archive.write(path, 'Source/' + name)
def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()
provenance = {
    'package': 'DreamAir-LH1-0.3.6-experimental',
    'upstreamTag': '1.3.0',
    'upstreamCommit': '21185da63a9e4307ae876b3702a43a43044d451c',
    'publicForkCommit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip(),
    'releaseUrl': 'https://github.com/sboys3/CustomHeadsetOpenVR/releases/download/1.3.0/CustomHeadset-1.3.0-Windows.zip',
    'forkDriverSha256': sha(main / 'bin/win64/driver_CustomHeadsetOpenVR.dll'),
    'upstreamPrivateDllSha256': sha(private / 'bin/win64/driver_CustomHeadsetOpenVR_Private.dll'),
    'upstreamGuiSha256': sha(destination / 'CustomHeadsetGUI/custom-headset-gui.exe'),
    'floorToolSha256': sha(destination / 'Tools/FloorHeight.exe'),
    'floorPresetMeters': 1.905,
    'launcherSha256': sha(destination / 'DreamAir-LH1.exe'),
    'dependencies': subprocess.check_output(['git', 'submodule', 'status', '--recursive'], cwd=repo, text=True).strip().splitlines(),
    'hardwareValidation': 'User confirmed .5 optics, tracking and moving-head behavior work perfectly; that release/source/settings were saved separately. Read-only capture found Lighthouse controller/station universe differs from Pimax FloorLevel. Primary-station rigid mapping reduces controller position disagreement from about 2 metres to 7-11 mm. The .6 update applies that mapping to Lighthouse world transforms and preserves the .5 HMD route. Visual alignment and extended game/reconnect behavior require live confirmation. Secondary station calibration differs between runtimes; see VALIDATION.md.'
}
(destination / 'PROVENANCE.json').write_text(json.dumps(provenance, indent=2), encoding='utf-8')
checksums = [f'{sha(p)}  {p.relative_to(destination).as_posix()}' for p in sorted(destination.rglob('*')) if p.is_file()]
(destination / 'SHA256SUMS.txt').write_text('\n'.join(checksums) + '\n', encoding='utf-8')
print(f'Packaged {destination}; {len(checksums)} files plus checksum manifest.')
