"""Kullanıcı uygulamalarının görünür ekranı, klavyesi ve disk kalıcılığı."""
import time
from dalga12_fat import Fat
from dalga12_kahin import fsck, wait_for
from psf_kahin import font_yukle, glif_ciz, karsilastir, screendump
from uefi_kabuk_kahin import keys

ARKA = 0x102040

def tus(c,ad,ctrl=False,shift=False):
    if ctrl: c.key("ctrl",True)
    if shift: c.key("shift",True)
    c.key(ad,True)
    c.key(ad,False)
    if shift: c.key("shift",False)
    if ctrl: c.key("ctrl",False)

def yaz(c,s):
    for ch in s:
        ad,shift=keys(ch)
        tus(c,ad,shift=shift)

def piksel(c,satirlar,imlec=None):
    font=font_yukle()
    def denetle():
        w,h,px=screendump(c.q.f,c.w/'uygulama.ppm',c.q.response)
        bek=[ARKA]*(w*h)
        for y,x,s,on,arka in satirlar:
            for i,ch in enumerate(s):
                glif_ciz(bek,w,h,font,(x+i)*8,(y%(h//16))*16,ord(ch),on,arka)
        if imlec:
            y,x=imlec
            for yy in (y*16+14,y*16+15):
                for xx in range(x*8,x*8+8):
                    bek[yy*w+xx]^=0xffffff
        fark=karsilastir(px,bek,w)
        denetle.fark=fark
        return fark[0]==0
    try: wait_for(c.p,denetle,7)
    except AssertionError as e: raise AssertionError(("ekran farklı",denetle.fark,c.output()[-240:])) from e

def du_ekran(c,yol,lines,row,col,dirty=False,msg=""):
    durum=f"{yol}  {row+1}:{col+1}"+(" *" if dirty else "")+msg
    piksel(c,[(i,0,s,0xffffff,ARKA) for i,s in enumerate(lines)]+[(-1,0,durum,0,0xffffff)],(row,col))

def tamam(c,exe,pid):
    s=f"[{exe}.elf pid={pid} cikis=0]\nKABUK_HAZIR\n"
    c.expected+=s.encode(); c.akis+=s
    wait_for(c.p,lambda:c.output()==c.expected)

def duzenle_kos(c,part,yalniz_icerik=False):
    yol="/diskler/usb0p2/notlar/deneme.txt"
    c.run("bagla --yazilabilir usb0p2")
    cmd=f"calistir /diskler/usb0p2/bin/duzenle.elf {yol}"
    c.input(cmd); c.akis+="> "+cmd+"\n"
    du_ekran(c,yol,[],0,0)
    once=c.heap(); time.sleep(8); assert c.heap()==once
    yaz(c,"Ağaç"); tus(c,"backspace"); yaz(c,"ç\nİş")
    tus(c,"home"); tus(c,"backspace")
    if not yalniz_icerik: du_ekran(c,yol,["Ağaçİş"],0,4,True)
    tus(c,"ret"); tus(c,"end"); tus(c,"q",ctrl=True)
    if not yalniz_icerik: du_ekran(c,yol,["Ağaç","İş"],1,2,True,"  Kaydedilmedi. Çıkmak için Ctrl+Q")
    tus(c,"s",ctrl=True)
    if not yalniz_icerik: du_ekran(c,yol,["Ağaç","İş"],1,2,False,"  Kaydedildi")
    if yalniz_icerik: time.sleep(.5)
    c.q.call("stop")
    try:
        fsck(c.w,part,"duzenle-kaydet")
        assert Fat(part.read()).get("/NOTLAR/deneme.txt")=="Ağaç\nİş".encode(), "kaydedilen UTF-8 içerik farklı"
    finally: c.q.call("cont")
    tus(c,"q",ctrl=True); tamam(c,"duzenle",2)
