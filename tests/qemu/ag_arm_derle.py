"""İkinci ARM çekirdeğini aynı CAT kaynağından ve EL0 bölüm sözleşmesiyle üretir."""
from pathlib import Path
import shutil
import subprocess
import sys

from ag_arm_kahin import kos



def arac(ad):
    """[D-771] Araç yolunu PATH(EXT) ile çöz. Windows CreateProcess adında nokta olan programa
    `.exe` eklemez (`ld.lld` → WinError 2, CI); shutil.which PATHEXT'i uygular."""
    return shutil.which(ad) or ad

def derle(w,derleyici,kaynak):
    w.mkdir(exist_ok=True,parents=True)
    ham=w/'ham.ll'
    ir=w/'cekirdek.ll'
    nesne=w/'cekirdek.o'
    hedef=w/'yonlendirilmis.o'
    elf=w/'kem_os.elf'
    imaj=w/'kem_os.img'
    for p in (ham,ir,nesne,hedef,elf,imaj):
        p.unlink(missing_ok=True)
    with ham.open('wb') as f:
        subprocess.run([derleyici,'--llvm','--mimari','arm64',kaynak],stdout=f,check=True)
    with ir.open('wb') as f:
        subprocess.run([arac('awk'),'-f','test/strip_defined_declares.awk',ham],stdout=f,check=True)
    subprocess.run([arac('clang'),'-target','aarch64-unknown-none','-ffreestanding','-nostdlib','-O2',
                    '-Wno-override-module','-ffunction-sections','-fdata-sections','-x','ir',ir,'-c','-o',nesne],check=True)
    # KEMGU-OS'un kullanıcı bölümlerini özgün bağlayıcı betiğinin karşıladığı adlara taşı.
    semboller=subprocess.check_output([arac('llvm-nm'),nesne],text=True)
    bayrak=[]
    for ad in sorted({l.split()[-1] for l in semboller.splitlines() if l.split() and l.split()[-1].startswith('kul_')}):
        for src,dst in (('text','user'),('data','user_data'),('bss','user_data'),('rodata','user_data')):
            bayrak.append(f'--rename-section=.{src}.{ad}=.{dst}.{ad}')
    subprocess.run([arac('llvm-objcopy'),*bayrak,nesne,hedef],check=True)
    subprocess.run([arac('ld.lld'),'-m','aarch64linux','-T','linker/bare-metal-aarch64.ld','--defsym=__kem_yukleme_tabani=0x40080000',
                    '--gc-sections','-o',elf,hedef,'build/bm_a64_start.o'],check=True)
    subprocess.run([arac('llvm-objcopy'),'-O','binary',elf,imaj],check=True)
    return imaj


def takim(w):
    imaj=derle(w/'self',Path('build/codegen'),Path('build/kem_os_comb.kem'))
    if not shutil.which('qemu-system-aarch64'):
        print('ATLANDI (qemu-system-aarch64 yok): ARM TCP/HTTP çalışma zamanı')
        return
    kos(w/'c',Path('build/kem_os.img').resolve())
    kos(w/'self',imaj)


if __name__=='__main__':
    takim(Path(sys.argv[1]).resolve())
