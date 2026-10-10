#!/usr/bin/env python3
"""Exercise real Installer 43 with deterministic answers, never user drawers."""
import os, shutil, subprocess, tempfile, time
from pathlib import Path
from installer_icon import installer_icon, readme_icon
from package_release import fresh_save_slots
WORKBENCH=Path(os.environ.get('WORKBENCH_ADF',Path.home()/'.local/share/amiga/Workbenchv2.04rev37.67Workbench.adf'))
INSTALLER=Path(os.environ.get('INSTALLER43',Path.home()/'.local/share/amiga/Installer43/Installer'))
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
    for mode in ('fresh','keep','replace','partial','remove','bad-size'):
        base=Path(tempfile.mkdtemp(prefix='installer-'+mode+'-',dir=ROOT/'tmp'));print(base,flush=True)
        boot=base/'boot';dest=base/'out/RAYPokeri'
        for p in (boot/'s',boot/'env',boot/'devs/Kickstarts',base/'out',base/'state'):p.mkdir(parents=True,exist_ok=True)
        for source,name in ((INSTALLER,'Installer'),
          (Path.home()/'.local/share/amiga/WHDLoad/C/WHDLoad','WHDLoad'),
          (ROOT/'amiga/out/RAYPokeri','RAYPokeri'),(ROOT/'build/whdload/RAYPokeri.slave','RAYPokeri.slave'),(ROOT/'release/ReadMe','ReadMe')):shutil.copyfile(source,boot/name)
        for name,data in fresh_save_slots().items():(boot/name).write_bytes(data)
        (boot/'devs/Kickstarts/kick40063.A600').touch();(boot/'devs/Kickstarts/kick40063.A600.RTB').touch()
        (boot/'Install.info').write_bytes(installer_icon());(boot/'RAYPokeri.inf').write_bytes(installer_icon(game=True));(boot/'ReadMe.info').write_bytes(readme_icon())
        if mode!='fresh':
            (dest/'data').mkdir(parents=True)
            for i,chip in enumerate(CHIPS):
                # Distinct local fixture data proves reuse leaves ROMs untouched
                # and replacement really overwrites them; no ROM bytes in source.
                (dest/'data'/chip).write_bytes(bytes([0xa0+i])*65536)
            if mode=='partial':(dest/'data'/CHIPS[-1]).unlink()
            (dest/'data/nvram.bin').write_bytes(bytes([37])*32768);(dest/'data/accounting.bin').write_bytes(fresh_save_slots()['FreshAccounting'])
            (dest/'data/nvram.bak').write_bytes(bytes([38])*32768) # legacy backup
            if mode=='bad-size':(dest/'data/nvram.bin').write_bytes(b'invalid')
            (dest/'old-marker').touch()
        (base/'out/unrelated').write_text('keep')
        # Exercise real Installer run/fallback behavior with noninteractive viewers.
        fallback=mode in ('fresh','replace')
        for viewer,rc in (('multiview',20 if fallback else 0),('more',0)):
            (boot/viewer).write_text('.key FILE/A\nEcho "<FILE>" >DH2:'+viewer+'-readme\nQuit '+str(rc)+'\n')
        (boot/'env/WHDLInstPath').write_text('DH2:previous parent')
        s=(ROOT/'release/Install').read_text();s='(textfile (dest "DH2:entered") (append "yes"))\n'+s;s=replace_form(s,'(welcome)','(if 0 (welcome))')
        s=replace_form(s,'(set #source','(set #source "DH0:")')
        s=s.replace('SYS:Utilities/MultiView', 'Execute DH0:multiview').replace('SYS:Utilities/More', 'Execute DH0:more')
        s=replace_form(s,'(set #parent','((textfile (dest "DH2:previous-destination") (append @default-dest)) (set #parent "DH2:out"))')
        s=replace_form(s,'(set #remove-existing',f'((textfile (dest "DH2:remove-asked") (append "yes")) (set #remove-existing {int(mode=="remove")}))')
        s=replace_form(s,'(askchoice\n      (prompt "The four RAY Pokeri',f'((textfile (dest "DH2:reuse-asked") (append "yes")) {int(mode=="replace")})')
        s=replace_form(s,'(set #roms','((textfile (dest "DH2:roms-asked") (append "yes")) (set #roms "DH1:rom"))')
        if mode=='bad-size':s=replace_form(s,'(abort "Invalid save size: nvram.bin', '((textfile (dest "DH2:invalid-refused") (append "yes")) (exit (quiet)))')
        s=replace_form(s,'(exit)','(exit (quiet))');(boot/'Install').write_text(s)
        (boot/'s/startup-sequence').write_text('CD DH0:\nStack 16384\nDF0:C/Assign C: DF0:C\nDF0:C/Assign LIBS: DF0:Libs\nDF0:C/Assign DEVS: DH0:devs\nDF0:C/Assign ENV: RAM:\nDF0:C/Assign ENVARC: DH0:env\nCopy ENVARC:WHDLInstPath ENV:WHDLInstPath\nDF0:C/Assign T: RAM:\nPath DH0: ADD\nC:LoadWB\nInstaller SCRIPT DH0:Install APPNAME RAYPokeri MINUSER NOVICE DEFUSER NOVICE LOGFILE DH2:installer.log NOPRETEND >DH2:console.log\nEcho done >DH2:finished\n')
        with (base/'emulator.log').open('w') as log:
            emu=subprocess.Popen(['fs-uae','--amiga_model=A1200','--chip_memory=2048','--fast_memory=8192','--kickstart_file='+os.environ['KICKSTART'],
                '--hard_drive_0='+str(boot),'--hard_drive_0_priority=10','--hard_drive_1='+str(ROOT),'--hard_drive_2='+str(base),
                '--floppy_drive_0='+str(WORKBENCH),
                '--warp_mode=1','--fullscreen=0','--state_dir='+str(base/'state')],stdout=log,stderr=log,env=dict(os.environ,SDL_AUDIODRIVER='dummy'))
            try:
                deadline=time.monotonic()+120
                while time.monotonic()<deadline and not (base/'finished').exists():
                    if emu.poll() is not None:raise RuntimeError('emulator exited')
                    time.sleep(.25)
                assert (base/'finished').exists(),f'Installer did not finish: {base}'
                assert (base/'multiview-readme').read_text().strip()=='DH0:ReadMe'
                assert (base/'more-readme').exists()==fallback
                if fallback:assert (base/'more-readme').read_text().strip()=='DH0:ReadMe'
                assert (base/'previous-destination').read_text()=='DH2:previous parent'
                assert (boot/'env/WHDLInstPath').read_text().strip()=='DH2:out'
                if mode=='bad-size':
                    assert (base/'invalid-refused').exists(),'malformed save was not rejected'
                    assert (dest/'data/nvram.bin').read_bytes()==b'invalid'
                    assert not (dest/'data/RAYPokeri').exists(),'program updated despite invalid saves'
                    print('PASS: Installer refuses malformed save size without overwriting it',flush=True)
                    continue
                for i,chip in enumerate(CHIPS):
                    expected=bytes([0xa0+i])*65536 if mode=='keep' else (ROOT/'rom'/chip).read_bytes()
                    assert (dest/'data'/chip).read_bytes()==expected
                assert (dest/'data/RAYPokeri').read_bytes()==(ROOT/'amiga/out/RAYPokeri').read_bytes()
                assert (dest/'RAYPokeri.slave').read_bytes()==(ROOT/'build/whdload/RAYPokeri.slave').read_bytes()
                assert (dest/'RAYPokeri.info').exists() and (dest/'ReadMe.info').exists()
                icon=(dest/'RAYPokeri.info').read_bytes().lower()
                assert b'novbrmove' not in icon and b'nowritecache' not in icon
                assert (base/'roms-asked').exists()==(mode in ('fresh','replace','partial','remove'))
                assert (base/'reuse-asked').exists()==(mode in ('keep','replace'))
                assert (base/'remove-asked').exists()==(mode!='fresh')
                assert (base/'out/unrelated').read_text()=='keep'
                for name in ('nvram','accounting'):
                    expected=fresh_save_slots()['EmptyNVRAM' if name=='nvram' else 'FreshAccounting']
                    if mode in ('keep','replace','partial') and name=='nvram':expected=bytes([37])*32768
                    assert (dest/'data'/f'{name}.bin').read_bytes()==expected
                # An older release's backup is left alone; the installer no longer creates backups.
                assert (dest/'data/nvram.bak').exists()==(mode in ('keep','replace','partial'))
                assert not (dest/'data/accounting.bak').exists()
                print('PASS: Installer '+mode+'; ReadMe viewer/fallback, remembered destination, ROM prompts and saves',flush=True)
            finally:
                emu.terminate()
                try:emu.wait(timeout=5)
                except subprocess.TimeoutExpired:emu.kill();emu.wait()
if __name__=='__main__':main()
