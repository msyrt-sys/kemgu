"""İki veri diski, iki kullanıcı arayüzü ve kalıcılık aynı OVMF açılışında."""
import re
from dalga12_fat import Fat
from dalga12_kahin import fsck
from duzenle_kahin import du_ekran, ge_cift_tik, ge_ekran, ge_girdiler, tamam, tus, yaz
from fat_yazici import MERHABA

YOL="/diskler/usb0p2/notlar/günlük.txt"
SON="Ağaç ve İş\nİş\nAğaç\nSon".encode()

def disk_durumu(c,part,veri,ad):
    c.q.call("stop")
    try:
        fsck(c.w,part,ad)
        assert Fat(part.read()).get("/NOTLAR/günlük.txt")==veri, "diske kaydedilen metin"
    finally: c.q.call("cont")

def kos(c,part,kesinti=False):
    c.run("bagla --yazilabilir usb0p2")
    cmd="duzenle "+YOL
    c.input(cmd);c.akis+="> "+cmd+"\n"
    du_ekran(c,YOL,["Ağaç ve İş",""],0,0)
    c.bosta_gozle()
    tus(c,"down");yaz(c,"İş\nAğaç");tus(c,"s",ctrl=True)
    ilk=["Ağaç ve İş","İş","Ağaç"]
    du_ekran(c,YOL,ilk,2,4,False,"  Kaydedildi")
    disk_durumu(c,part,"\n".join(ilk).encode(),"v1-ilk-kayit")
    tus(c,"q",ctrl=True);tamam(c,"duzenle",2)
    print(f"V1 ilk kayıt heap={c.heap()}",flush=True)
    klasor=YOL.rsplit("/",1)[0];cmd="dosyalar "+klasor
    c.input(cmd);c.akis+="> "+cmd+"\n"
    rows=ge_girdiler(part,"/NOTLAR");ge_ekran(c,klasor,rows,0)
    k=next(i for i,s in enumerate(rows) if s.startswith("günlük.txt "))
    ge_cift_tik(c,(k+1)*16+3)
    du_ekran(c,YOL,ilk,0,0)
    tus(c,"pgdn");tus(c,"end");tus(c,"ret");yaz(c,"Son");tus(c,"s",ctrl=True)
    du_ekran(c,YOL,ilk+["Son"],3,3,False,"  Kaydedildi")
    if kesinti:
        # "Kaydedildi" ekrandayken gücü kes. Sonraki süreç/kabuk sync işlemi,
        # kaydın o anda kalıcı olup olmadığını değiştiremez.
        c.q.call("quit"); c.p.wait(timeout=5)
        return
    disk_durumu(c,part,SON,"v1-ikinci-kayit")
    tus(c,"q",ctrl=True)
    ge_ekran(c,klasor,ge_girdiler(part,"/NOTLAR"),k)
    tus(c,"q");tamam(c,"dosyalar",3)
    print(f"V1 gezgin dönüşü heap={c.heap()}",flush=True)
    c.run("say "+YOL,f"satir={SON.count(bytes([10]))} kelime={len(SON.split())} bayt={len(SON)}\n[say.elf pid=5 cikis=0]\n".encode())
    c.run("kedi /diskler/nvme0p1/merhaba.txt",MERHABA+b"[kedi.elf pid=6 cikis=0]\n")
    c.run("baslat hesap &",b"pid=7\n")
    def ps(veri):
        durum = rb"(?:hazir|kosuyor)" if len(c.q.call("query-cpus-fast"))>1 else rb"hazir"
        m=re.fullmatch(rb"7 "+durum+rb" hesap\.elf (\d+)\n",veri);assert m and int(m[1])>=1,veri
    c.run("ps",ps);c.run("sonlandir 7");c.run("bekle 7",b"[hesap.elf pid=7 SONLANDIRILDI: sonlandir (kullanici)]\n")
