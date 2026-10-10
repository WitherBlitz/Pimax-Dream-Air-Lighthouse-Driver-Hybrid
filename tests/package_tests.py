"""Run the real packager with isolated upstream fixtures, without installing anything."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--test-root', required=True, type=Path)
args = parser.parse_args()
repo = Path(__file__).resolve().parent.parent
root = args.test_root.resolve()
if root.exists():
    raise SystemExit('Choose a fresh package test directory.')
upstream = root / 'upstream'
driver = upstream / 'CustomHeadsetOpenVR'
(driver / 'bin/win64').mkdir(parents=True)
(driver / 'bin/win64/driver_CustomHeadsetOpenVR.dll').write_bytes(b'isolated-upstream-fixture')
(driver / 'driver.vrdrivermanifest').write_text('{"name":"CustomHeadsetOpenVR","version":"1.3.0"}')
gui = upstream / 'CustomHeadsetGUI'
gui.mkdir()
(gui / 'custom-headset-gui.exe').write_bytes(b'isolated-upstream-gui-fixture')
destination = root / 'package'
subprocess.run([sys.executable, str(repo / 'Tools/Package.py'), '--upstream', str(upstream), '--destination', str(destination)], check=True, cwd=repo)
custom = destination / 'Distortion/Dream Air Maximum Comfort 100pct Overlap.json'
assert custom.is_file(), 'Packager omitted the nested bundled distortion data.'
assert hashlib.sha256(custom.read_bytes()).hexdigest() == '00b17f0d11166c0796f344eb3f414fe113e898cbda04e9c40a94c05b0fc8806e', 'Bundled profile bytes changed.'
provenance = json.loads((destination / 'PROVENANCE.json').read_text())
assert provenance['package'] == 'DreamAir-LH1-0.3.7-experimental', 'Launcher release metadata was not updated.'
assert provenance['forkDriverSha256'] == 'c0d1f0dad2deec398b4f8712e73a39c64c9748a1616ef1fb7bde5a1e3dc8c606', 'Native driver changed.'
manifest = json.loads((destination / 'CustomHeadsetOpenVR/driver.vrdrivermanifest').read_text())
assert manifest['version'] == '1.3.0-dreamair-lh1.6-experimental', 'Launcher release incorrectly bumped the driver.'
with zipfile.ZipFile(destination / 'Source.zip') as archive:
    assert 'Source/Tools/Package/Distortion/Dream Air Maximum Comfort 100pct Overlap.json' in archive.namelist(), 'Corresponding source omitted the bundled data.'
    assert 'Source/Tools/DistortionCatalog.cs' in archive.namelist(), 'Corresponding source omitted the profile catalog.'
print('PASS real package includes exact bundled data and corresponding source, with .7 launcher and unchanged .6 driver')
print(f'Package test artifacts: {root}')
