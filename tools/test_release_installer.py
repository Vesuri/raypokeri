#!/usr/bin/env python3
"""Exercise real Installer 43 with deterministic answers, never user drawers."""
import os, shutil, subprocess, tempfile, time
from pathlib import Path
from installer_icon import installer_icon, readme_icon
from package_release import fresh_save_slots
ROOT=Path(__file__).resolve().parents[1]
CHIPS=('77POK30','77POK38','77POK34','PARA200J')
def replace_form(text,start,replacement):
    a=text.index(start);depth=0;quoted=False;i=a
    while i<len(text):
        c=text[i]
        if quoted and c=='\\':i+=2;continue
        if c=='"':quoted=not quoted
        elif not quoted:
            if c=='(':depth+=1
            elif c==')':
                depth-=1
                if depth==0:return text[:a]+replacement+text[i+1:]
        i+=1
    raise ValueError('Unbalanced Installer form')
def main():
    for mode in ('fresh','keep','remove','bad-size'):
        base=Path(tempfile.mkdtemp(prefix='installer-'+mode+'-',dir=ROOT/'tmp'));print(base,flush=True)
        boot=base/'boot';dest=base/'out/Pokeri'
        for p in (boot/'s',boot/'devs/Kickstarts',base/'out',base/'state'):p.mkdir(parents=True,exist_ok=True)
        for source,name in ((Path.home()/'Documents/Stunt Car Racer/data/Installer43_3/Installer','Installer'),
          (Path.home()/'.local/share/amiga/WHDLoad/C/WHDLoad','WHDLoad'),
          (ROOT/'amiga/out/Pokeri','Pokeri'),(ROOT/'build/whdload/Pokeri.slave','Pokeri.slave'),(ROOT/'release/ReadMe','ReadMe')):shutil.copyfile(source,boot/name)
        for name,data in fresh_save_slots().items():(boot/name).write_bytes(data)
        (boot/'devs/Kickstarts/kick40063.A600').touch();(boot/'devs/Kickstarts/kick40063.A600.RTB').touch()
        (boot/'Install.info').write_bytes(installer_icon());(boot/'Pokeri.inf').write_bytes(installer_icon(game=True));(boot/'ReadMe.info').write_bytes(readme_icon())
        if mode!='fresh':
            (dest/'data').mkdir(parents=True)
            for chip in CHIPS:shutil.copyfile(ROOT/'rom'/chip,dest/'data'/chip)
            (dest/'data/nvram.bin').write_bytes(bytes([37])*32768);(dest/'data/accounting.bin').write_bytes(fresh_save_slots()['FreshAccounting'])
            (dest/'data/nvram.bak').write_bytes(bytes([38])*32768)
            if mode=='bad-size':(dest/'data/nvram.bin').write_bytes(b'invalid')
            (dest/'old-marker').touch()
        (base/'out/unrelated').write_text('keep')
        s=(ROOT/'release/Install').read_text();s='(textfile (dest "DH2:entered") (append "yes"))\n'+s;s=replace_form(s,'(welcome)','(if 0 (welcome))')
        s=replace_form(s,'(set #source','(set #source "DH0:")')
        s=replace_form(s,'(set #parent','(set #parent "DH2:out")')
        s=replace_form(s,'(set #remove-existing',f'((textfile (dest "DH2:remove-asked") (append "yes")) (set #remove-existing {int(mode=="remove")}))')
        s=replace_form(s,'(set #roms','((textfile (dest "DH2:roms-asked") (append "yes")) (set #roms "DH1:rom"))')
        if mode=='bad-size':s=replace_form(s,'(abort "Invalid save size: nvram.bin', '((textfile (dest "DH2:invalid-refused") (append "yes")) (exit (quiet)))')
        s=replace_form(s,'(exit)','(exit (quiet))');(boot/'Install').write_text(s)
        (boot/'s/startup-sequence').write_text('CD DH0:\nStack 16384\nDF0:C/Assign C: DF0:C\nDF0:C/Assign LIBS: DF0:Libs\nDF0:C/Assign DEVS: DH0:devs\nDF0:C/Assign ENV: RAM:\nDF0:C/Assign T: RAM:\nPath DH0: ADD\nC:LoadWB\nInstaller SCRIPT DH0:Install APPNAME Pokeri MINUSER NOVICE DEFUSER NOVICE LOGFILE DH2:installer.log NOPRETEND >DH2:console.log\nEcho done >DH2:finished\n')
        with (base/'emulator.log').open('w') as log:
            emu=subprocess.Popen(['fs-uae','--amiga_model=A1200','--chip_memory=2048','--fast_memory=8192','--kickstart_file='+os.environ['KICKSTART'],
                '--hard_drive_0='+str(boot),'--hard_drive_0_priority=10','--hard_drive_1='+str(ROOT),'--hard_drive_2='+str(base),
                '--floppy_drive_0='+str(Path.home()/'Documents/Vette/tmp/Workbenchv2.04rev37.67Workbench.adf'),
                '--warp_mode=1','--fullscreen=0','--state_dir='+str(base/'state')],stdout=log,stderr=log,env=dict(os.environ,SDL_AUDIODRIVER='dummy'))
            try:
                deadline=time.monotonic()+120
                while time.monotonic()<deadline and not (base/'finished').exists():
                    if emu.poll() is not None:raise RuntimeError('emulator exited')
                    time.sleep(.25)
                assert (base/'finished').exists(),f'Installer did not finish: {base}'
                if mode=='bad-size':
                    assert (base/'invalid-refused').exists(),'malformed save was not rejected'
                    assert (dest/'data/nvram.bin').read_bytes()==b'invalid'
                    assert (dest/'data/nvram.bak').read_bytes()==bytes([38])*32768
                    assert not (dest/'data/Pokeri').exists(),'program updated despite invalid saves'
                    print('PASS: Installer refuses malformed save size without overwriting it',flush=True)
                    continue
                for chip in CHIPS:assert (dest/'data'/chip).read_bytes()==(ROOT/'rom'/chip).read_bytes()
                assert (dest/'data/Pokeri').read_bytes()==(ROOT/'amiga/out/Pokeri').read_bytes()
                assert (dest/'Pokeri.slave').read_bytes()==(ROOT/'build/whdload/Pokeri.slave').read_bytes()
                assert (dest/'Pokeri.info').exists() and (dest/'ReadMe.info').exists()
                icon=(dest/'Pokeri.info').read_bytes().lower()
                assert b'novbrmove' not in icon and b'nowritecache' not in icon
                assert (base/'roms-asked').exists()==(mode!='keep')
                assert (base/'remove-asked').exists()==(mode!='fresh')
                assert (base/'out/unrelated').read_text()=='keep'
                for name in ('nvram','accounting'):
                    expected=fresh_save_slots()['EmptyNVRAM' if name=='nvram' else 'FreshAccounting']
                    if mode=='keep' and name=='nvram':expected=bytes([37])*32768
                    for suffix in ('bin','bak'):
                        want=bytes([38])*32768 if mode=='keep' and name=='nvram' and suffix=='bak' else expected
                        assert (dest/'data'/f'{name}.{suffix}').read_bytes()==want
                print('PASS: Installer '+mode+'; ROM prompts, contents, save preservation and deletion scope',flush=True)
            finally:
                emu.terminate()
                try:emu.wait(timeout=5)
                except subprocess.TimeoutExpired:emu.kill();emu.wait()
if __name__=='__main__':main()
