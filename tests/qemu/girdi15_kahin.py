"""Gerçek QMP girişini kabuk çıktısı ve uygulamaların GOP görüntüsünde ölçer."""
from bekle import bekle, tik_bekle
import argparse
from pathlib import Path

from dalga12_kahin import boot, wait_for
from duzenle_kahin import du_ekran, ge_ekran, ge_girdiler, tamam, tus
from psf_kahin import font_yukle, glif_ciz, konsol, screendump
from surec_imaj import imajlar
from surec_kahin import SurecConsole


def tekrar(c, sure):
    # QEMU'nun sanal milisaniyesi yük altında işlenen LAPIC tikinden ayrılabilir.
    # Basılı süreyi konuk make kodunu aldıktan sonra donanım tikleriyle ölç.
    def kod():
        return int.from_bytes(c.mem(c.symbols['kt_kod'], 4), 'little')
    def beklenen(tik):
        return 1 if tik < 50 else 2 + (tik-50)//5
    if sure < 50:
        # QEMU basma ve bırakmayı tek istekte kendi sanal saatiyle zamanlar.
        # Host'un iki QMP çağrısı arasında çalıştırılmaması basışı uzatamaz.
        def son_tarih():
            return int.from_bytes(c.mem(c.symbols['kt_sonraki'], 8), 'little')
        eski = son_tarih()
        c.q.call('send-key', {'keys':[{'type':'qcode','data':'a'}], 'hold-time':sure*10})
        bekle('kısa basışın konukta işlenmesi', son_tarih, lambda t: t != eski, surec=c.p)
        bekle('kısa basışın bırakılması', kod, lambda k: k == 0, surec=c.p)
        aralik = f'{sure*10} sanal ms'
        alt = ust = 1
    else:
        once = c.tik()
        c.tus_gonder('a', True)
        bekle('tekrar deneyi: A basma kodu', kod, lambda k: k == 30, aralik=.001, surec=c.p)
        ilk = c.tik()
        birak = bekle(f'tekrar için {sure} LAPIC tiki', c.tik, lambda t: t-ilk >= sure,
                      aralik=.001, surec=c.p)
        c.tus_gonder('a', False)
        bekle('tekrar deneyi: A bırakma kodu', kod, lambda k: k == 0, aralik=.001, surec=c.p)
        son = c.tik()
        alt = max(2, beklenen(birak-ilk)-1)
        ust = beklenen(son-once)+1
        aralik = f'{birak-ilk}..{son-once} konuk tiki'
    # Komut öneki de tekrarlanabilir. Basılı tuşun etkisini doğrudan giriş satırından say.
    font = font_yukle()
    fw, fh = font[1][:2]
    desen = [0]*(fw*fh)
    glif_ciz(desen,fw,fh,font,0,0,ord('a'),0xffffff,0x102040)
    baytlar = b''.join(x.to_bytes(3,'big') for x in desen)
    def harf_say():
        w,h,px = screendump(c.q.f,c.w/'tekrar.ppm',c.q.response)
        sut, sat = (w-16)//fw, (h-16)//fh
        _, (_,y) = konsol(c.akis,sut,sat,True)
        adet = 0
        for i in range(2,sut):
            bas = 8+i*fw
            glif = b''.join(px[3*((8+y*fh+j)*w+bas):3*((8+y*fh+j)*w+bas+fw)] for j in range(fh))
            if glif != baytlar:
                break
            adet += 1
        return adet
    adet = bekle('GOP üzerinde yinelenen A glifleri', harf_say, lambda n: n >= alt, surec=c.p)
    print('TEKRAR',sure,adet,'basılı süre',aralik,'beklenen glif',alt,ust,flush=True)
    assert alt <= adet <= ust, (sure,adet,alt,ust)
    for _ in range(adet):
        tus(c,'backspace')


def teker(c, yon):
    for bas in (True,False):
        c.q.call('input-send-event', {'events':[{'type':'btn','data':{'button':'wheel-'+yon,'down':bas}}]})
        tik_bekle(c.tik,surec=c.p)


def kos(w, tur):
    satirlar = [f'satir {n}' for n in range(8)]
    dosyalar = {p.name:p.read_bytes() for p in (w/'kullanici').glob('*.elf')}
    dosyalar['iz.txt'] = '\n'.join(satirlar).encode()
    usb,nvme,sata = imajlar(w,dosyalar)
    with boot(w,usb[0],nvme,sata,klavye=tur,fare=tur) as (q,p,s):
        c = SurecConsole(w,q,p,s)
        assert b'PS2_FARE 3 4\n' in s.read_bytes()
        tekrar(c,20)
        tekrar(c,68)
        tekrar(c,87)
        c.run('hassasiyet 0', b'HATA hassasiyet 1..4\n')
        c.run('hassasiyet 2')
        c.run('bagla --yazilabilir usb0p2')
        yol = '/diskler/usb0p2'
        belge = yol+'/bin/iz.txt'
        c.input('calistir '+yol+'/bin/duzenle.elf '+belge)
        du_ekran(c,belge,satirlar,0,0)
        teker(c,'down')
        du_ekran(c,belge,satirlar,3,0)
        teker(c,'up')
        du_ekran(c,belge,satirlar,0,0)
        tus(c,'q',ctrl=True)
        tamam(c,'duzenle',1)
        c.input('calistir '+yol+'/bin/dosyalar.elf '+yol)
        adlar = ge_girdiler(usb[1],'/')
        ge_ekran(c,yol,adlar,0)
        for hareket,son in ((12,36),(-100,0)):
            q.call('input-send-event',{'events':[{'type':'rel','data':{'axis':'x','value':hareket}}]})
            wait_for(p,lambda: c.sayi('ksv_fare_x') == son)
        teker(c,'down')
        ge_ekran(c,yol,adlar,3)
        tus(c,'q')
        tamam(c,'dosyalar',2)
        c.shutdown()
    print('GIRDI15 OK',tur,flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('dizin',type=Path)
    p.add_argument('--aygit',choices=('ps2','usb'))
    a = p.parse_args()
    for tur in ([a.aygit] if a.aygit else ('ps2','usb')):
        kos(a.dizin.resolve(),tur)
