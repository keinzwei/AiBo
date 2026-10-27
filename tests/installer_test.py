"""Behavioral installer integration: real staged files, temp games and rollback."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

WORKER = Path(sys.argv[1]).resolve()
PAYLOAD = Path(sys.argv[2]).resolve()
passed = 0


def run(payload, target, ok=True, fail=None, check=False):
    env = dict(os.environ)
    if fail is not None: env['AIBO_TEST_FAIL_AFTER'] = str(fail)
    report = target.parent / 'report.txt'
    r = subprocess.run([str(WORKER), '--check' if check else '--install', str(payload), str(target), str(report)], env=env, stdout=subprocess.PIPE, text=True)
    assert (r.returncode == 0) == ok, r.stdout
    return r.stdout


def game(base, ini=None):
    c = base / 'Counter Strike üji/cstrike'; c.mkdir(parents=True)
    (c / 'liblist.gam').write_text('game "Counter-Strike"\ngamedll "dlls/mp.dll"\ncustom "keep"\n')
    if ini is not None:
        p = c / 'addons/metamod/plugins.ini'; p.parent.mkdir(parents=True); p.write_text(ini)
    return c


def contents(root):
    return {p.relative_to(root).as_posix(): p.read_bytes() for p in root.rglob('*') if p.is_file() and not any(x.startswith('AiBo-backup-') for x in p.parts)}


def case(name, fn):
    global passed
    with tempfile.TemporaryDirectory(prefix='aibo-installer-') as t: fn(Path(t))
    passed += 1; print('PASS:', name)


def fresh(t):
    c = game(t); before = contents(c); run(PAYLOAD, c.parent, check=True); assert contents(c) == before
    run(PAYLOAD,c.parent)
    assert (c/'addons/AiBo/bin/AiBo.dll').read_bytes() == (PAYLOAD/'addons/AiBo/bin/AiBo.dll').read_bytes()
    ini = (c/'addons/metamod/plugins.ini').read_text()
    assert ini.count('win32 addons/AiBo/bin/AiBo.dll') == 1 and 'amxmodx_mm.dll' in ini
    assert 'custom "keep"' in (c/'liblist.gam').read_text() and 'metamod.dll' in (c/'liblist.gam').read_text()
    assert (c/'addons/amxmodx/plugins/cs_team_semiclip.amxx').is_file()
    before = contents(c); result = run(PAYLOAD,c); assert contents(c) == before and 'Files=0' in result


def upgrade(t):
    original='; keep a comment\r\n  WIN32 "addons/yapb/bin/yapb_human_copycat.dll" ; old bot\r\nwin32 addons/RealBot/dll/realbot.dll\r\nwin32 addons/AiBo/bin/AiBo.dll\r\nwin32 addons/AiBo/bin/AiBo.dll\r\nwin32 addons/other/special.dll\r\n'
    c=game(t,original)
    p=c/'addons/amxmodx/configs/users.ini';p.parent.mkdir(parents=True);p.write_bytes(b'personal admin data\xff')
    p=c/'addons/amxmodx/configs/team_semiclip.cfg';p.write_text('semiclip 0 // keep custom config\n')
    old=c/'addons/yapb/data/copycat/test.haim';old.parent.mkdir(parents=True);old.write_bytes(b'personal-learning\0\xff')
    run(PAYLOAD,c)
    assert (c/'addons/amxmodx/configs/users.ini').read_bytes()==b'personal admin data\xff'
    assert (c/'addons/amxmodx/configs/team_semiclip.cfg').read_text().startswith('semiclip 0')
    assert old.read_bytes()==(c/'addons/AiBo/data/copycat/test.haim').read_bytes()
    ini=(c/'addons/metamod/plugins.ini').read_text();active=[x.strip() for x in ini.splitlines() if x.strip() and not x.strip().startswith(';')]
    assert active.count('win32 addons/AiBo/bin/AiBo.dll')==1 and 'win32 addons/other/special.dll' in active
    assert not any('yapb' in x.lower() or 'realbot' in x.lower() for x in active)
    backups=list((c/'addons').glob('AiBo-backup-*/files/addons/metamod/plugins.ini'))
    assert len(backups)==1 and backups[0].read_bytes()==original.encode()
    assert (c/'addons/metamod/plugins.ini.bak').read_bytes()==original.encode()


def mismatch(t):
    c=game(t); p=t/'payload';shutil.copytree(PAYLOAD,p)
    (p/'addons/metamod/plugins.ini').write_text('win32 addons/AiBo/bin/MISSING.dll\n')
    before=contents(c);run(p,c,ok=False);assert contents(c)==before


def wrong_dll(t):
    c=game(t);p=t/'payload';shutil.copytree(PAYLOAD,p)
    dll=p/'addons/AiBo/bin/AiBo.dll';b=bytearray(dll.read_bytes());off=int.from_bytes(b[60:64],'little');b[off+4:off+6]=(0x8664).to_bytes(2,'little');dll.write_bytes(b)
    before=contents(c);run(p,c,ok=False);assert contents(c)==before


def rollback(t):
    c=game(t,'win32 addons/yapb/bin/yapb.dll\nwin32 addons/custom.dll\n')
    p=c/'addons/AiBo/conf/AiBo.cfg';p.parent.mkdir(parents=True);p.write_text('old custom config\n')
    before=contents(c);run(PAYLOAD,c,ok=False,fail=8);assert contents(c)==before


def symlink(t):
    c=game(t);outside=t/'external';outside.mkdir();(c/'addons').mkdir();(c/'addons/AiBo').symlink_to(outside,target_is_directory=True)
    before=contents(c);run(PAYLOAD,c,ok=False);assert contents(c)==before and not list(outside.iterdir())


def invalid(t):
    c=t/'wrong';c.mkdir();before=contents(c);run(PAYLOAD,c,ok=False);assert contents(c)==before


def bad_pro(t):
    c=game(t);p=t/'payload';shutil.copytree(PAYLOAD,p)
    cfg=p/'addons/AiBo/conf/AiBo.cfg';cfg.write_text(cfg.read_text().replace('aibo_human_aim_level "100"','aibo_human_aim_level "-1"'))
    before=contents(c);run(p,c,ok=False);assert contents(c)==before



def config_replacements(t):
    c=game(t)
    for name in ['autobuy.txt','listenserver.cfg']:
        (c/name).write_bytes(b'old-'+name.encode())
        (c/(name+'.bak')).write_bytes(b'previous-backup')
    run(PAYLOAD,c)
    for name in ['autobuy.txt','listenserver.cfg']:
        assert (c/name).read_bytes()==(PAYLOAD/name).read_bytes()
        assert (c/(name+'.bak')).read_bytes()==b'previous-backup'
        assert (c/(name+'.bak.1')).read_bytes()==b'old-'+name.encode()
    before=contents(c);run(PAYLOAD,c);assert contents(c)==before


def config_rollback(t):
    c=game(t)
    for name in ['autobuy.txt','listenserver.cfg']:(c/name).write_bytes(b'original-'+name.encode())
    before=contents(c)
    report=run(PAYLOAD,c,check=True)
    import re
    files=int(re.search(r'Files=(\d+)',report)[1])
    run(PAYLOAD,c,ok=False,fail=files-2)
    assert contents(c)==before


def existing_graph(t):
    c=game(t);p=c/'addons/AiBo/data/graph/de_dust2.graph';p.parent.mkdir(parents=True);p.write_bytes(b'my-custom-graph')
    run(PAYLOAD,c);assert p.read_bytes()==b'my-custom-graph'

for name,fn in [('uploaded_config_files_replace_after_bak',config_replacements),('uploaded_config_files_restore_on_failure',config_rollback),('existing_custom_graph_is_preserved',existing_graph),('fresh_unicode_path_dryrun_and_reinstall',fresh),('upgrade_merges_and_preserves_personal_data',upgrade),('mismatched_plugins_ini_rejected_before_write',mismatch),('wrong_architecture_rejected_before_write',wrong_dll),('write_failure_rolls_back_existing_and_new_files',rollback),('symlink_target_rejected_before_write',symlink),('invalid_game_rejected_before_write',invalid),('non_pro_payload_rejected_before_write',bad_pro)]: case(name,fn)
print(f'{passed}/{passed} installer integration scenarios passed. Windows UI/process-lock behavior requires Windows execution.')
