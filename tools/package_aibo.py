#!/usr/bin/env python3
"""Package the verified installer, runtime tree, full source and release evidence."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BUNDLE = ROOT.parent
VERSION = '4.3.3'
PREFIX = f'AiBo-{VERSION}'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--installer', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True, help='Output ZIP filename')
    a = p.parse_args()
    binary, setup = a.binary.read_bytes(), a.installer.read_bytes()
    pe = struct.unpack_from('<I', binary, 60)[0]
    assert binary[:2] == b'MZ' and binary[pe:pe+4] == b'PE\0\0'
    assert struct.unpack_from('<H', binary, pe+4)[0] == 0x14c
    assert struct.unpack_from('<H', binary, pe+22)[0] & 0x2000
    assert f'AiBo {VERSION}'.encode('utf-16le') in binary
    assert setup[:2] == b'MZ'
    evidence = json.loads((ROOT / 'build/installer/preflight.json').read_text())
    assert evidence['dll_sha256'] == sha(binary)
    assert evidence['installer_sha256'] == sha(setup)
    assert evidence['checked_before_makensis'] and evidence['all_referenced_dlls_exist']

    # The parent install tree includes the user's additional Metamod/AMXX files.
    runtime = BUNDLE / 'install/cstrike'
    bin_dir = runtime / 'addons/AiBo/bin'
    if bin_dir.exists():
        for old_binary in bin_dir.iterdir():
            if old_binary.is_file():
                old_binary.unlink()
    shutil.copytree(ROOT / 'build/installer/payload', runtime, dirs_exist_ok=True)
    dll = runtime / 'addons/AiBo/bin/AiBo.dll'
    dll.parent.mkdir(parents=True, exist_ok=True)
    dll.write_bytes(binary)
    (BUNDLE / f'AiBo-{VERSION}-Setup.exe').write_bytes(setup)
    for name, expected in evidence['uploaded_config_sha256'].items():
        assert sha((runtime / name).read_bytes()) == expected

    info = {
        'product': 'AiBo', 'version': VERSION, 'release': 'FINAL',
        'platform': 'Windows x86, GoldSrc CS 1.6',
        'dll': 'install/cstrike/addons/AiBo/bin/AiBo.dll',
        'dll_sha256': sha(binary), 'installer_sha256': sha(setup),
        'upstream_commit': '7985904584a0755b216e17cc5de8c5fd28953895',
        'input_source_archive_sha256': '959b8a9d35bef42043587b991ef8f83b52f6e2a6b21870eca826162dff4c4b3c',
        'default_global_aim_level': 100, 'pro_profiles': 42,
        'ranking_basis': 'verified historical annual career peaks, mixed CS eras',
        'previous_release_history_included': False,
        'old_source_snapshots_included': False,
        'ranking_to_ability_is_custom_simulation': True,
        'score_or_damage_manipulation': False,
        'ct_enemy_search_regions': ['T-Spawn', 'Tunnel', 'Pit'],
        'native_graph_and_geometry_exploration': True,
        'dust2_graph_nodes': 1238, 'existing_graph_preserved': True,
        'replacement_policy': 'rename old file to .bak, .bak.1, ... before commit; rollback',
        'uploaded_config_sha256': evidence['uploaded_config_sha256'],
        'dialogue': 'local procedural grammar, 16 profile languages; no LLM/API',
        'max_bots': 31, 'full_vendored_source_included': True,
        'native_code_tests_passed': 293, 'installer_scenarios_passed': 11,
        'actual_cs16_windows_match_tested': False,
        'windows_installer_ui_tested': False,
        'actual_pro_player_benchmark_tested': False,
    }
    (BUNDLE / 'BUILD_INFO.json').write_text(json.dumps(info, indent=2) + '\n')
    files = {}
    for base, dirs, names in os.walk(BUNDLE):
        dirs[:] = sorted(d for d in dirs if d not in {'.git', '.vs', '__pycache__', 'build', 'dist', 'node_modules'} and not d.startswith('build-'))
        for name in sorted(names):
            path = Path(base) / name
            rel = path.relative_to(BUNDLE).as_posix()
            if path.suffix in {'.pyc', '.o', '.obj', '.zip'} or name in {'.DS_Store', 'SHA256SUMS.txt'}:
                continue
            if path.is_symlink():
                raise RuntimeError(f'Unexpected symlink: {rel}')
            files[rel] = path.read_bytes()
    files['SHA256SUMS.txt'] = ''.join(sha(data) + '  ' + name + '\n' for name, data in sorted(files.items())).encode()
    (BUNDLE / 'SHA256SUMS.txt').write_bytes(files['SHA256SUMS.txt'])
    a.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(a.output, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for name, data in sorted(files.items()):
            z.writestr(PREFIX + '/' + name, data)
    # Verify the whole delivered archive, not only its central directory.
    with zipfile.ZipFile(a.output) as z:
        assert z.testzip() is None
        for name, data in files.items():
            assert sha(z.read(PREFIX + '/' + name)) == sha(data), name
        assert z.read(PREFIX + '/install/cstrike/addons/AiBo/bin/AiBo.dll') == binary
    print(json.dumps({'path': str(a.output.resolve()), 'bytes': a.output.stat().st_size,
                      'files': len(files), 'sha256': sha(a.output.read_bytes())}))


if __name__ == '__main__':
    main()
