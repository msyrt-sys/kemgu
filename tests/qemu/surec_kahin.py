#!/usr/bin/env python3
"""Ayrı kullanıcı ELF'lerini gerçek QEMU ve OVMF ile sınar."""

from bekle import bekle, gozle, tam_seri
from protokol_suresi import BOSTA_GOZLEM
import argparse
import os
import re
import struct
import subprocess
import time
from hashlib import sha256
from contextlib import contextmanager
from pathlib import Path

from dalga12_fat import Fat
from dalga12_kahin import Console, boot, fsck, sha, wait_for
from surec_imaj import BUYUK, GUNLUK, imajlar
import kaynak14_kahin
import v1_kahin
from usb14_imaj import dogrula as usb_dogrula
from kabuk14_kahin import kabuk14_kos
from duzenle_kahin import duzenle_kos, gezgin_kos, salt_gezgin
from uefi_kabuk_kahin import keys
from psf_kahin import font_yukle, glif_ciz, screendump, karsilastir


SAYFA = 4096
YUVALAR = 8
TABLOLAR = 10
CERCEVELER = 7 * 512 + TABLOLAR
ALAN = CERCEVELER * SAYFA
PENCERE = 0x54200000
YIGIN = 0x54F00000
KORUMA = 0x54E00000
NX = 1 << 63
ADRES = (1 << 52) - SAYFA
VARSAYILAN = ("baslat", "dosya", "buyuk", "yazma", "tus-usb", "tus-ps2",
             "bolme", "tekrar", "dosya", "yazma")


def fiziksel_cerceve_haritasi(oku, ayirici):
    _, bitmap, _, toplam = struct.unpack("<3QI", oku(ayirici, 28))
    # KdlDizi başlığı { veri@0, boyut@8, kapasite@12, eleman_byte@16 }. [D-778] Yuva genişliği
    # BAŞLIKTAN okunur: dar eleman (dtam8) artık 1 bayt; eski düzen 4 bayttı (`[::4]` varsayımı
    # D-778'de bm_x86_kaynak ve bm_x86_ag_pc kapılarını yanlış "896 çerçeve" ile düşürmüştü).
    adres, boy, _kap, eb = struct.unpack("<QiiI", oku(bitmap, 20))
    assert boy == (toplam + 7) // 8
    adim = eb if eb in (1, 2) else 4
    bitler = oku(adres, boy * adim)[::adim]
    return bitler, toplam - sum(v.bit_count() for v in bitler)


class SurecConsole(Console):
    def input(self, text):
        tr = {"&": ("6", True), "ü": ("bracket_right", False), "ö": ("comma", False),
              "-": ("equal", False), "_": ("equal", True)}
        for harf in text + "\n":
            ad, shift = tr.get(harf, keys(harf))
            if shift:
                self.key("shift", True)
            self.key(ad, True)
            self.key(ad, False)
            if shift:
                self.key("shift", False)

    def bosta_gozle(self, heap=True, seri=False):
        ilk=self.heap() if heap else None
        def saglam():
            if heap: assert self.heap()==ilk, 'boşta heap değişti'
            if seri: assert self.output()==self.expected, 'boşta seri değişti'
        gozle('süreç boşta',BOSTA_GOZLEM,saglam,surec=self.p)

    def sayi(self, sembol):
        return struct.unpack("<Q", self.mem(self.symbols[sembol], 8))[0]

    def bos_cerceve(self):
        return fiziksel_cerceve_haritasi(self.mem, self.sayi("uefi_surec_ayirici"))[1]


def uefi_alan_denetle(c, taban):
    assert taban % SAYFA == 0 and taban >= 0x100000
    b = int(re.search(rb"BILGI_ADRES (\d+)", c.ser.read_bytes())[1])
    alan = struct.unpack("<24Q", c.mem(b, 192))
    harita = c.mem(alan[9], alan[10])
    kayitlar = [struct.unpack_from("<IIQQQQ", harita, i) for i in range(0, len(harita), alan[11])]
    konum = taban
    for t, _, bas, _, sayfa, nitelik in sorted(kayitlar, key=lambda k: k[2]):
        if t in (3, 4, 7) and not nitelik & NX and bas <= konum < bas + sayfa * SAYFA:
            konum = bas + sayfa * SAYFA
    assert konum >= taban + ALAN
    korunan = [(alan[14], alan[15]), (b, b + SAYFA), (alan[9], alan[9] + alan[16]),
               (alan[3], alan[3] + alan[4]), (alan[20], alan[21])]
    assert all(taban + ALAN <= lo or taban >= hi for lo, hi in korunan)


def etkin_tablolar(c, elf):
    # Birinci süreç toplandı. İkinci süreç klavye beklerken özel tabloları canlı.
    assert c.sayi("kxs_havuz_dolu") == 2
    # Bekleyen süreç işlemciyi bırakmıştır. Özel kök adresini kayıttan,
    # beklemeyi donanımın CR3 ve kullanıcı kipinden bağımsız olarak gözle.
    kayit = c.symbols["kdl_dilim_tablo"] + 272
    wait_for(c.p, lambda: int.from_bytes(c.mem(kayit + 160, 8), "little") == 7)
    taban = int.from_bytes(c.mem(kayit + 192, 8), "little")
    uefi_alan_denetle(c, taban)
    # Durum alanı ve CR3 farklı konuk talimatlarında yazılır. IRQ ile aynı
    # anda okursak bağlamı bırakmayı bitirene kadar bekle.
    def cekirdek_adres_alani():
        kayitlar = c.q.call("human-monitor-command", {"command-line": "info registers"})
        cr3 = int(re.search(r"CR3=([0-9a-fA-F]+)", kayitlar)[1], 16)
        return kayitlar if cr3 != taban else None
    regs = wait_for(c.p, cekirdek_adres_alani)
    assert int(re.search(r"EFER=([0-9a-fA-F]+)", regs)[1], 16) & 2048
    for ofset, indeksler in ((0, [0]), (1, [1]), (2, range((PENCERE >> 21) & 511, ((PENCERE >> 21) & 511) + 7))):
        tablo = struct.unpack("<512Q", c.mem(taban + ofset * SAYFA, SAYFA))
        for j, i in enumerate(indeksler):
            assert tablo[i] & (ADRES | 7) == (taban + (ofset + 1 + j) * SAYFA) | 7
        assert all(not g & 4 for j, g in enumerate(tablo) if j not in indeksler), "gözetmen dalı kullanıcıya açıldı"
    ph = struct.unpack_from("<Q", elf, 32)[0]
    n = struct.unpack_from("<H", elf, 56)[0]
    beklenen = {}
    for i in range(n):
        tip, bayrak, _, va, _, _, boy, _ = struct.unpack_from("<II6Q", elf, ph + i * 56)
        if tip == 1 and boy:
            for a in range(va, va + boy, SAYFA):
                beklenen[a] = (5 if bayrak == 5 else NX | (7 if bayrak == 6 else 5))
    beklenen.update((a, NX | 7) for a in range(0x54600000, KORUMA, SAYFA))
    beklenen.update((a, NX | 7) for a in range(YIGIN, 0x55000000, SAYFA))
    ptes = struct.unpack("<3584Q", c.mem(taban + 3 * SAYFA, 7 * SAYFA))
    for i, g in enumerate(ptes):
        a = PENCERE + i * SAYFA
        if a in beklenen:
            assert (g & (NX | 7)) == beklenen[a], (hex(a), hex(g), hex(beklenen[a]))
            assert (g & ADRES) == taban + (TABLOLAR + i) * SAYFA
        else:
            assert g == 0, (hex(a), hex(g))
    print("✓ QMP: bekleyen süreç, kullanıcı PTE izinleri, NX ve boş koruma sayfası", flush=True)


def tam_seri_bekle(c):
    tam_seri("süreç seri çıktısı", c.output, c.expected, c.p, 15)


def tus_kos(c, metin, elf):
    c.input(metin)
    hazir = b"SAAT OK\nTUS_HAZIR\n"
    c.expected += hazir
    c.akis += "> " + metin + "\n" + hazir.decode()
    tam_seri_bekle(c)
    etkin_tablolar(c, elf)
    c.bosta_gozle(heap=False,seri=True)
    for ad, bas in (("shift", True), ("a", True), ("a", False), ("shift", False),
                    ("bracket_left", True)):
        c.key(ad,bas)
    c.q.call("input-send-event", {"events": [{"type": "key", "data": {
        "down": False, "key": {"type": "qcode", "data": "bracket_left"},
    }}]})
    satirlar = (b"TUS 42 0 1 1\nTUS 30 65 1 1\nTUS 30 65 0 1\nTUS 42 0 0 0\n"
                b"TUS 26 287 1 0\n[tus.elf pid=2 cikis=0]\nKABUK_HAZIR\n")
    c.expected += satirlar
    c.akis += satirlar.decode()
    tam_seri_bekle(c)


def ekran_kos(c,usb_fare):
    komut = "calistir /diskler/usb0p2/bin/ekranden.elf"
    c.input(komut)
    c.akis += "> " + komut + "\nEKRAN_HAZIR\n"
    c.expected += b"EKRAN_HAZIR\n"
    tam_seri_bekle(c)
    w,h,px = screendump(c.q.f,c.w/'uygulama.ppm',c.q.response)
    renk = 0x203040
    beklenen = [renk]*(w*h)
    font = font_yukle()
    for i, harf in enumerate("Ağaç İş"):
        glif_ciz(beklenen,w,h,font,(5+i)*8,2*16,ord(harf),0xffffff,renk)
    for y in (78,79):
        for x in range(24,32):
            beklenen[y*w+x] ^= 0xffffff
    def ekran_farki():
        gen,yuk,pikseller=screendump(c.q.f,c.w/'uygulama.ppm',c.q.response)
        assert (gen,yuk)==(w,h)
        return karsilastir(pikseller,beklenen,w)
    bekle('uygulama pikselleri',ekran_farki,lambda fark:fark[0]==0,surec=c.p)
    if usb_fare:
        assert c.sayi("kh_iman") != 0, "xHCI MSI-X kurulmadı"
        once_irq = c.sayi("bh_bildirim")
    c.bosta_gozle(seri=True)
    def hareket(dx,dy,bas):
        olay = [{"type":"rel","data":{"axis":a,"value":v}} for a,v in (("x",dx),("y",dy))]
        if bas:
            olay.append({"type":"btn","data":{"button":"left","down":True}})
        c.q.call("input-send-event",{"events":olay})
    sembol='bh_bildirim' if usb_fare else 'ps2_irq12_sayi'
    once=c.sayi(sembol)
    hareket(-12,-7,False)
    bekle('ilk fare hareketi',lambda:c.sayi(sembol),lambda n:n>once,surec=c.p)
    hareket(12,7,True)
    if usb_fare:
        wait_for(c.p,lambda:c.sayi("bh_bildirim")>once_irq)
    sonuc = "[ekranden.elf pid=2 cikis=0]\nKABUK_HAZIR\n"
    c.expected += sonuc.encode()
    c.akis += sonuc
    tam_seri_bekle(c)


def bolme_kos(c):
    c.run("calistir /diskler/usb0p2/bin/bolme.elf",
          b"PANIK: sifira bolme\n[bolme.elf pid=2 cikis=134]\n")
    c.run("echo yasiyor", b"yasiyor\n")


def cocuk_kos(c, bos):
    # Aynı simgeyi iki canlı çocuğun gerçek PT yaprakları üzerinden oku.
    semboller = subprocess.check_output(
        ["llvm-nm", c.w / "kullanici/surecden.elf"], text=True)
    va = int(re.search(r"^([0-9a-f]+) [bB] surec_nobet$", semboller, re.M)[1], 16)
    komut = "calistir /diskler/usb0p2/bin/surecden.elf"
    c.input(komut)

    def iki_cocuk():
        assert b"[surecden.elf pid=2 cikis=" not in c.output(), "çocuklar ortak adres doğrulamasına ulaşamadı"
        kayitlar = c.mem(c.symbols["kdl_dilim_tablo"], 8 * 272)
        bulunan = []
        for i in range(8):
            durum, pid, _, _, cr3 = struct.unpack_from("<5Q", kayitlar, i * 272 + 160)
            if durum == 5:
                bulunan.append((pid, cr3))
        return bulunan if len(bulunan) >= 2 else None

    cocuklar = wait_for(c.p, iki_cocuk)
    fizikler = set()
    for pid, cr3 in cocuklar:
        ptr = cr3
        for kaydir in (39, 30, 21, 12):
            pte, = struct.unpack("<Q", c.mem(ptr + ((va >> kaydir) & 511) * 8, 8))
            assert pte & 7 == 7
            ptr = pte & ADRES
        fizik = ptr + va % SAYFA
        deger, = struct.unpack("<Q", c.mem(fizik, 8))
        assert fizik not in fizikler and deger == pid, (pid, deger, hex(fizik))
        fizikler.add(fizik)
    son = "[surecden.elf pid=2 cikis=0]\nKABUK_HAZIR\n"
    c.expected += son.encode()
    c.akis += "> " + komut + "\n" + son
    # Tek bir seri satırı yüz tahsisi özetler. Her tahsisi gözlemek gerekmez,
    # ancak PID ilerlemeden geçen süreyi ve tüm deneyin süresini ayrı sınırla.
    bas = son_ilerleme = time.monotonic()
    son_pid = 2
    def cocuk_durumu():
        nonlocal son_pid, son_ilerleme
        veri = c.output()
        assert c.expected.startswith(veri), veri[-240:]
        pid = c.sayi("kxs_pid_son")
        assert son_pid <= pid <= 102, (son_pid, pid)
        if pid > son_pid:
            son_pid, son_ilerleme = pid, time.monotonic()
        assert time.monotonic() - son_ilerleme < 15, f"çocuk PID {pid} ilerlemedi, seri={veri[-120:]!r}"
        return pid, veri

    bekle("100 çocuk toplandı", cocuk_durumu,
          lambda durum: durum == (102, c.expected), sure=90, aralik=.2, surec=c.p)
    print(f"Çocuklar son PID={son_pid}, {time.monotonic()-bas:.2f} sn", flush=True)
    assert c.bos_cerceve() == bos, "süreç çerçevesi geri dönmedi"
    print(f"✓ 100 çocuk, aynı VA için {len(fizikler)} farklı fiziksel sayfa, {bos} boş çerçeve", flush=True)


def tekrar_kos(c, exe, bos):
    once = c.heap()
    surec_artisi = 0
    for pid in range(2, 52):
        c.run(f"calistir {exe}", f"{exe}\n[merhaba.elf pid={pid} cikis=1]\n".encode())
        assert c.bos_cerceve() == bos, f"süreç {pid}"
        giris, cikis = c.sayi("kxs_heap_giris"), c.sayi("kxs_heap_cikis")
        assert c.symbols["__heap_start"] <= giris <= cikis < c.symbols["__heap_end"], (giris, cikis)
        surec_artisi += cikis - giris
    sonra = c.heap()
    assert surec_artisi <= 64 * 1024 + 80, surec_artisi
    if os.environ.get("SMP_TEK_BELLEK") == "1":
        cpular = c.q.call("query-cpus-fast")
        assert len(cpular) == 1
        elf = (c.w/"k.elf").read_bytes()
        ph, = struct.unpack_from("<Q", elf, 32)
        adim, adet = struct.unpack_from("<2H", elf, 54)
        alanlar = []
        for i in range(adet):
            tip, _, _, _, fizik, _, boy, _ = struct.unpack_from("<II6Q", elf, ph+i*adim)
            if tip == 1 and boy:
                alanlar.append((fizik//4096, (fizik+boy+4095)//4096))
        sayfa = max(b for _, b in alanlar) - min(a for a, _ in alanlar)
        print(f"SMP tek vCPU: boş={bos}, ELF={sayfa} sayfa, toplam={bos+sayfa}, "
              f"kabuk heap={once}/{sonra}, süreç artışı={surec_artisi}", flush=True)
        assert bos + sayfa == 53132, (bos, sayfa)
        # D-772'nin taban koşumlarında ve aynı kullanıcı imajıyla tekrarında ölçülen çiftler.
        taban = {
            "kemgu": {(4788464, 5247552)},
            "codegen": {(5050928, 5510016), (5116512, 5510016),
                        (5116512, 5575600), (5182096, 5575600)},
        }
        derleyici = Path(os.environ["SUREC_KAHIN_DERLEYICI"]).stem
        assert (once, sonra) in taban[derleyici], (derleyici, once, sonra)
        assert surec_artisi == 0
    print(f"✓ 50 tekrar: {bos} boş çerçeve; süreç heap +{surec_artisi}; kabuk heap {once} → {sonra} bayt", flush=True)


@contextmanager
def diskleri_koru(once, esp_bolum, esp):
    # Seri kâhini önce kırılsa da gerçek disk mutasyonu mutlaka ölçülür.
    try:
        yield
    finally:
        assert all(sha(p) == h for p, h in once.items()), "salt okunur disk SHA256 değişti"
        assert sha256(esp_bolum.read()).digest() == esp, "ESP SHA256 değişti"


def kos(w, tekrar, senaryo, ilk=1):
    ayar = os.environ.get("SUREC_SMP", os.environ.get("KEMGU_QEMU_SMP"))
    smp = int(ayar) if ayar is not None else None
    if ilk < 1 or tekrar < 1:
        raise ValueError("İlk açılış ve tekrar sayısı pozitif olmalı")
    programlar = {p.name: p.read_bytes() for p in (w / "kullanici").glob("*.elf")}
    assert "merhaba.elf" in programlar
    for n in range(ilk, ilk + tekrar):
        kip = VARSAYILAN[(n - 1) % len(VARSAYILAN)] if senaryo == "matris" else senaryo
        usb, nvme, sata = imajlar(w, programlar)
        if kip.startswith("v1"): usb_dogrula(w,usb,programlar)
        once = {p.disk: sha(p.disk) for p in (nvme, sata)}
        if kip not in ("kaynak-editor","yazma","duzenle-icerik","duzenle","gezgin","v1","v1-kesinti"):
            once[usb[0].disk] = sha(usb[0].disk)
        esp = sha256(usb[0].read()).digest()
        with diskleri_koru(once, usb[0], esp), boot(w, usb[0], nvme, sata, smp=smp, klavye="ps2" if kip in ("tus-ps2","ekran-ps2") else "usb", fare="usb" if kip in ("ekran-usb","gezgin-salt","gezgin","v1","v1-kesinti") else "ps2") as (q, p, seri):
            c = SurecConsole(w, q, p, seri)
            if smp:
                cpular = q.call("query-cpus-fast")
                assert len(cpular) == smp == len({cpu["thread-id"] for cpu in cpular})
                assert int.from_bytes(c.mem(c.symbols["smp_cevrimici"], 4), "little") == smp
            bos = c.bos_cerceve()
            assert bos >= 8 * CERCEVELER
            exe = "/diskler/usb0p2/bin/merhaba.elf"
            c.run(f"calistir {exe} Ağaç İş",
                  (f"{exe}\nAğaç\nİş\n[merhaba.elf pid=1 cikis=3]\n").encode())
            if kip=="kaynak-cocuk": kaynak14_kahin.cocuk(c,n)
            if kip=="kaynak-editor": kaynak14_kahin.editor(c,usb[1],n)
            if kip.startswith("v1"):
                v1_kahin.kos(c,usb[1],kip=="v1-kesinti")
            if kip == "kabuk14":
                kabuk14_kos(c)
            if kip == "gezgin-salt":
                salt_gezgin(c,nvme)
            if kip == "gezgin":
                gezgin_kos(c,usb[1])
            if kip in ("duzenle","duzenle-icerik"):
                duzenle_kos(c,usb[1],kip=="duzenle-icerik")
            if kip.startswith("ekran-"):
                ekran_kos(c,kip == "ekran-usb")
            if kip == "buyukelf":
                assert len(programlar["buyukelf.elf"]) >= 300 * 1024
                giris = c.heap()
                if n == 1:
                    c.bosta_gozle()
                    assert c.heap() == giris
                c.run("calistir /diskler/usb0p2/bin/buyukelf.elf", b"[buyukelf.elf pid=2 cikis=0]\n")
                fark = c.heap()-giris
                c.run("calistir /diskler/usb0p2/bin/eski.elf", b"/diskler/usb0p2/bin/eski.elf\n[eski.elf pid=3 cikis=1]\n")
                print(f"✓ {len(programlar['buyukelf.elf'])} bayt ELF, 1280 KiB BSS, 4 MiB heap, kabuk heap farkı {fark}", flush=True)
            if kip == "arka":
                c.run("baslat /diskler/usb0p2/bin/hesap.elf uzun &", b"pid=2\n")
                once_dilim = c.sayi("kdl_dilim_sayisi")
                c.run("echo cevap", b"cevap\n")
                c.run(f"calistir {exe}", f"{exe}\n[merhaba.elf pid=3 cikis=1]\n".encode())
                def ps(seri):
                    durum = rb"(?:hazir|kosuyor)" if (smp or 1)>1 else rb"hazir"
                    m = re.fullmatch(rb"2 "+durum+rb" hesap\.elf ([0-9]+)\n", seri)
                    assert m and int(m[1]) >= 2, seri
                c.run("ps", ps)
                assert c.sayi("kdl_dilim_sayisi") > once_dilim
                c.run("sonlandir 2")
                c.run("bekle 2", b"[hesap.elf pid=2 SONLANDIRILDI: sonlandir (kullanici)]\n")
            if kip == "cocuk":
                cocuk_kos(c, bos)
            if kip == "hesap":
                once_tik = c.sayi("kdl_dilim_sayisi")
                c.run("calistir /diskler/usb0p2/bin/hesap.elf", b"[hesap.elf pid=2 cikis=0]\n")
                fark = c.sayi("kdl_dilim_sayisi") - once_tik
                assert fark >= 2, f"LAPIC zaman dilimi sayısı: {fark}"
                print(f"✓ hesap: {fark} zaman dilimi, toplam doğru", flush=True)
            if kip == "dosya":
                c.run("calistir /diskler/usb0p2/bin/kedi.elf /diskler/usb0p2/notlar/günlük.txt",
                      "Ağaç ve İş\n[kedi.elf pid=2 cikis=0]\n".encode())
            if kip == "buyuk":
                tahmin = f"satir={BUYUK.count(bytes((10,)))} kelime={len(BUYUK.split())} bayt={len(BUYUK)}"
                c.run("calistir /diskler/usb0p2/bin/say.elf /diskler/usb0p2/notlar/buyuk.txt",
                      f"{tahmin}\n[say.elf pid=2 cikis=0]\n".encode())
            if kip == "yazma":
                c.run("bagla --yazilabilir usb0p2")
                c.run("calistir /diskler/usb0p2/bin/yazdene.elf /diskler/usb0p2/notlar /diskler/nvme0p1/yok.txt /diskler/usb0p1/yok.txt",
                      b"YAZDENE OK\n[yazdene.elf pid=2 cikis=0]\n")
                c.run("pwd", b"/\n")
            if kip in ("tus-usb", "tus-ps2"):
                tus_kos(c, "calistir /diskler/usb0p2/bin/tus.elf", programlar["tus.elf"])
            if kip == "bolme":
                bolme_kos(c)
            if kip == "tekrar":
                tekrar_kos(c, exe, bos)
            if kip != "v1-kesinti":
                assert c.bos_cerceve() == bos, "süreç çerçevesi geri dönmedi"
                c.screen()
            if kip == "yazma":
                # Kabuğun kapanış sync'i eksik süreç temizliğini gizleyemez.
                q.call("stop")
                fsck(w, usb[1], "surec-cikis")
                q.call("cont")
            if kip != "v1-kesinti": c.shutdown()
        assert all(sha(p) == h for p, h in once.items())
        assert sha256(usb[0].read()).digest() == esp
        if kip.startswith("v1"):
            fsck(w,usb[1],"v1-kapanis"); assert Fat(usb[1].read()).get("/NOTLAR/günlük.txt")==v1_kahin.SON
        if kip == "yazma":
            fsck(w, usb[1], "surec-yazma")
            fat = Fat(usb[1].read())
            assert fat.get("/NOTLAR/kalici.txt") == "Ağaç İşSon".encode()
            assert fat.get("/NOTLAR/kesilecek.txt") == b""
            assert fat.get("/NOTLAR/günlük.txt") == GUNLUK
            assert fat.get("/NOTLAR/buyuk.txt") == BUYUK
            assert not {"yeni.txt", "silinecek.txt", "gecici"} & fat.get("NOTLAR").keys()
            for ad, veri in programlar.items():
                assert fat.get("/BIN/" + ad) == veri, ad
            fat.verify_info()
        print(f"✓ Süreç OVMF {kip} #{n}: tek açılış, seri, GOP, kaynaklar ve disk", flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("dizin", type=Path)
    p.add_argument("--ilk", type=int, default=1)
    p.add_argument("--tekrar", type=int, default=10)
    p.add_argument("--senaryo", choices=("kaynak-cocuk", "kaynak-editor", "v1-kesinti", "v1", "kabuk14", "gezgin-salt", "gezgin", "duzenle-icerik", "duzenle", "ekran-ps2", "ekran-usb", "buyukelf", "arka", "cocuk", "hesap", "matris", "baslat", "dosya", "buyuk", "yazma", "tus-usb", "tus-ps2", "bolme", "tekrar"), default="matris")
    args = p.parse_args()
    kos(args.dizin.resolve(), args.tekrar, args.senaryo, args.ilk)
