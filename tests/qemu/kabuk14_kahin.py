"""Geçmiş, imleç ve tamamlama için bağımsız kabuk ekranı."""
import time
from dalga12_kahin import wait_for
from duzenle_kahin import tus, yaz
from psf_kahin import kabuk_goruntu, screendump, karsilastir


def girdi(c,metin,imlec=None):
    def denetle():
        w,h,px=screendump(c.q.f,c.w/'kabuk14.ppm',c.q.response)
        bek=kabuk_goruntu(c.akis,metin,imlec,gen=w,yuk=h)
        fark=karsilastir(px,bek,w)
        denetle.fark=fark
        return fark[0]==0
    try: wait_for(c.p,denetle,6)
    except AssertionError as e: raise AssertionError(("kabuk girdi pikselleri",metin,denetle.fark)) from e

def enter(c,s,out):
    tus(c,"ret"); sonuc=out+"KABUK_HAZIR\n"; c.expected+=sonuc.encode();c.akis+="> "+s+"\n"+sonuc
    wait_for(c.p,lambda:c.output()==c.expected)

def kabuk14_kos(c):
    c.run("yardim",b"ls cd pwd cat echo yaz cp mv sil mkdir rmdir diskler bagla sync saat\ncalistir baslat bekle ps sonlandir sonuc temizle yardim kapat yenidenbaslat\ndonanim log hassasiyet goster bul hesapla duzenle dosyalar\n")
    c.run("merhaba",b"/diskler/usb0p2/bin/merhaba.elf\n[merhaba.elf pid=2 cikis=1]\n")
    c.run("sonuc",b"1\n")
    c.run("kedi /diskler/usb0p2/notlar/günlük.txt","Ağaç ve İş\n[kedi.elf pid=3 cikis=0]\n".encode())
    c.run("sonuc",b"0\n")
    c.run("temizle");c.akis="KABUK_HAZIR\n";c.screen()
    yaz(c,"echo ac");tus(c,"left");yaz(c,"b");girdi(c,"echo abc",7);tus(c,"right");enter(c,"echo abc","abc\n")
    yaz(c,"echo /diskler/usb0p");tus(c,"tab")
    aday="usb0p1/\nusb0p2/\n";c.expected+=aday.encode();c.akis+=aday
    wait_for(c.p,lambda:c.output()==c.expected)
    girdi(c,"echo /diskler/usb0p")
    yaz(c,"2/notlar/g");tus(c,"tab");s="echo /diskler/usb0p2/notlar/günlük.txt"
    girdi(c,s);enter(c,s,s[5:]+"\n")
    for i in range(33):
        c.run(f"echo {i}",f"{i}\n".encode())
    yaz(c,"taslak")
    for i in range(32):
        tus(c,"up"); girdi(c,f"echo {32-i}")
    girdi(c,"echo 1")
    for i in range(32):
        tus(c,"down"); girdi(c,f"echo {2+i}" if i<31 else "taslak")
    girdi(c,"taslak")
    for _ in "taslak":tus(c,"backspace")
    girdi(c,"")
    once=c.heap();time.sleep(8);assert c.heap()==once
