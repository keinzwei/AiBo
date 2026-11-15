#!/usr/bin/env python3
"""Build AiBo's native Windows installer after verifying its actual payload."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shlex
import shutil
import struct
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
DLL_PATH = 'addons/AiBo/bin/AiBo.dll'
UPLOADED_CONFIG_SHA256 = {
    'autobuy.txt': '567b58586f6969da91d5d272d3217ff5b66ddede05c70773970ca80a9c0e6160',
    'listenserver.cfg': 'de3a309511aa8b0a26cbdc1b9b9041e9d7216da7e1b97c396b097e4359254cb6',
}


def preflight(payload):
    dll = payload / DLL_PATH
    b = dll.read_bytes()
    pe = struct.unpack_from('<I', b, 60)[0]
    assert b[:2] == b'MZ' and b[pe:pe+4] == b'PE\0\0'
    assert struct.unpack_from('<H', b, pe+4)[0] == 0x14c
    assert struct.unpack_from('<H', b, pe+22)[0] & 0x2000
    assert 'AiBo 4.3.3'.encode('utf-16le') in b
    ini = payload / 'addons/metamod/plugins.ini'
    active = []
    for line in ini.read_text(encoding='utf-8-sig').splitlines():
        tokens = shlex.split(line.split(';', 1)[0].replace('\\', '/'))
        if tokens and tokens[0].lower() == 'win32':
            assert len(tokens) >= 2
            rel = Path(tokens[1]); assert not rel.is_absolute() and '..' not in rel.parts
            assert (payload / rel).is_file(), f'plugins.ini names a missing DLL: {rel}'
            active.append(tokens[1])
    assert active.count(DLL_PATH) == 1, active
    assert not any('yapb' in x.lower() or 'realbot' in x.lower() for x in active)
    values = {}
    for line in (payload / 'addons/AiBo/conf/AiBo.cfg').read_text().splitlines():
        t = shlex.split(line.split('//', 1)[0])
        if len(t) >= 2: values[t[0]] = t[1]
    for key in ['human_aim_level', 'human_aim_skill', 'human_reaction_skill']:
        assert values['aibo_' + key] == '100', key
    for key in ['human_copycat', 'human_aim_adaptive', 'human_aim_techniques', 'human_aim_wallbang', 'human_aim_preaim', 'human_aim_quickswitch', 'human_aim_crouchhop', 'human_movement', 'human_pro_profiles', 'human_hltv_skill', 'human_ct_patrol']:
        assert values['aibo_' + key] == '1', key
    for name, expected in UPLOADED_CONFIG_SHA256.items():
        assert hashlib.sha256((payload / name).read_bytes()).hexdigest() == expected, name
    return {'dll': DLL_PATH, 'dll_sha256': hashlib.sha256(b).hexdigest(),
            'plugins_ini': str(ini.resolve()), 'active_win32_modules': active,
            'all_referenced_dlls_exist': True, 'pro_defaults': True,
            'uploaded_config_sha256': UPLOADED_CONFIG_SHA256,
            'ct_enemy_hunt_and_pro_profiles_enabled': True,
            'checked_before_makensis': True, 'checked_at_unix': time.time()}


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--worker', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--payload-base', type=Path, default=ROOT.parent / 'install/cstrike',
                   help='Base cstrike runtime with supplied Metamod/AMXX components')
    p.add_argument('--makensis', default='makensis')
    a = p.parse_args()
    stage = ROOT / 'build/installer'
    if stage.exists(): shutil.rmtree(stage)
    stage.mkdir(parents=True)
    payload = stage / 'payload'
    shutil.copytree(a.payload_base, payload)
    own = payload / 'addons/AiBo'
    # Runtime DLLs from the old fork must never accompany the new active module.
    for path in (own / 'bin').glob('*'):
        if path.is_file(): path.unlink()
    shutil.copy2(a.binary, own / 'bin/AiBo.dll')
    shutil.copytree(ROOT / 'cfg/addons/AiBo', own, dirs_exist_ok=True)
    for path in list(payload.rglob('logs')):
        if path.is_dir(): shutil.rmtree(path)  # original logs stay in the source bundle
    shutil.copytree(ROOT / 'cfg/addons/amxmodx', payload / 'addons/amxmodx', dirs_exist_ok=True)
    # Keep original notices accessible from both the EXE and its installed files.
    licenses = own / 'licenses'; licenses.mkdir(exist_ok=True)
    for name in ['COPYING.GPL-2.0', 'UNLICENSE', 'NOTICE-AiBo.md']:
        shutil.copy2(ROOT / name, licenses / name)
    source_license = ROOT.parent / 'licenses/GCC-RUNTIME-COPYRIGHT.txt'
    shutil.copy2(source_license, licenses / source_license.name)
    (stage / 'LICENSE.txt').write_bytes((ROOT / 'NOTICE-AiBo.md').read_bytes() + b'\r\n\r\n' + (ROOT / 'COPYING.GPL-2.0').read_bytes())
    shutil.copy2(a.worker, stage / 'AiBoInstall.exe')
    evidence = preflight(payload)
    a.output = a.output.resolve(); a.output.parent.mkdir(parents=True, exist_ok=True)
    command = [a.makensis, '-V3', '-DSTAGE=' + str(stage), '-DOUTPUT=' + str(a.output), str(ROOT / 'installer/AiBo-Setup.nsi')]
    evidence['makensis_started_at_unix'] = time.time()
    assert evidence['checked_at_unix'] <= evidence['makensis_started_at_unix']
    r = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (stage / 'nsis-build.log').write_text(r.stdout)
    if r.returncode: raise SystemExit(r.stdout)
    evidence.update({'installer': a.output.name, 'installer_bytes': a.output.stat().st_size,
                     'installer_sha256': hashlib.sha256(a.output.read_bytes()).hexdigest()})
    (stage / 'preflight.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__': main()
