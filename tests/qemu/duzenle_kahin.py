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

def ge_ekran(c,yol,adlar,sec):
    rows=[(0,0,yol,0xffffff,ARKA)]
    for i,ad in enumerate(adlar):
        rows.append((i+1,0,ad,0 if i==sec else 0xffffff,0xffffff if i==sec else ARKA))
    piksel(c,rows)

def ge_girdiler(part,dizin):
    fat=Fat(part.read())
    records=fat.directory(fat.root) if dizin=="/" else fat.get(dizin)
    return ["../"]+[ad+("/" if d else f"  {n}") for ad,(_,n,d) in records.items() if ad not in (".","..")]

def ge_cift_tik(c,dy):
    # QEMU göreli hareketi ile iki ayrı düğme bas/bırak çevrimi.
    if dy: c.q.call("input-send-event",{"events":[{"type":"rel","data":{"axis":"y","value":dy}}]})
    time.sleep(.08)
    for down in (True,False,True,False):
        c.q.call("input-send-event",{"events":[{"type":"btn","data":{"button":"left","down":down}}]})
        time.sleep(.05)

def gezgin_kos(c,part):
    yol="/diskler/usb0p2"
    c.run("bagla --yazilabilir usb0p2")
    cmd="calistir "+yol+"/bin/dosyalar.elf "+yol
    c.input(cmd); c.akis+="> "+cmd+"\n"
    root=ge_girdiler(part,"/")
    ge_ekran(c,yol,root,0)
    n=next(i for i,ad in enumerate(root) if ad.lower()=="notlar/")
    adi=root[n][:-1]
    hedef=yol+"/"+adi
    ge_cift_tik(c,(n+1)*16+3)
    rows=ge_girdiler(part,"/NOTLAR")
    ge_ekran(c,hedef,rows,0)
    tus(c,"ret"); ge_ekran(c,yol,root,0)
    ge_cift_tik(c,0)
    ge_ekran(c,hedef,rows,0)
    k=next(i for i,ad in enumerate(rows) if ad.startswith("günlük.txt "))
    ge_cift_tik(c,(k-n)*16)
    dosya=hedef+"/günlük.txt"
    du_ekran(c,dosya,["Ağaç ve İş",""],0,0)
    h0=c.heap(); time.sleep(8); assert c.heap()==h0
    tus(c,"end"); tus(c,"ret"); yaz(c,"İş")
    tus(c,"s",ctrl=True)
    du_ekran(c,dosya,["Ağaç ve İş","İş",""],1,2,False,"  Kaydedildi")
    tus(c,"q",ctrl=True)
    yeni=ge_girdiler(part,"/NOTLAR")
    ge_ekran(c,hedef,yeni,k)
    tus(c,"q"); tamam(c,"dosyalar",2)
    assert Fat(part.read()).get("/NOTLAR/günlük.txt")=="Ağaç ve İş\nİş\n".encode()
    fsck(c.w,part,"gezgin")

def salt_gezgin(c,nvme):
    yol="/diskler";cmd="calistir /diskler/usb0p2/bin/dosyalar.elf "+yol
    c.input(cmd);c.akis+="> "+cmd+"\n"
    ge_ekran(c,yol,["../","nvme0p1/","sata0p1/","usb0p1/","usb0p2/"],0)
    tus(c,"down");tus(c,"ret")
    rows=ge_girdiler(nvme,"/")
    ge_ekran(c,yol+"/nvme0p1",rows,0)
    i=next(i for i,s in enumerate(rows) if s.startswith("merhaba.txt "))
    ge_cift_tik(c,(i+1)*16+3)
    dosya=yol+"/nvme0p1/merhaba.txt"
    metin=Fat(nvme.read()).get("/merhaba.txt").decode()
    du_ekran(c,dosya,[metin],0,0,False,"  Salt okunur")
    yaz(c,"a");tus(c,"s",ctrl=True)
    du_ekran(c,dosya,["a"+metin],0,1,True,"  Salt okunur")
    tus(c,"q",ctrl=True)
    du_ekran(c,dosya,["a"+metin],0,1,True,"  Kaydedilmedi. Çıkmak için Ctrl+Q")
    tus(c,"q",ctrl=True)
    ge_ekran(c,yol+"/nvme0p1",rows,i)
    tus(c,"q");tamam(c,"dosyalar",2)
