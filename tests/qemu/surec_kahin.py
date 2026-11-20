#!/usr/bin/env python3
"""Ayrı kullanıcı ELF'lerini gerçek QEMU ve OVMF ile sınar."""

import argparse
import re
import struct
import time
from hashlib import sha256
from pathlib import Path

from dalga12_fat import Fat
from dalga12_kahin import Console, boot, fsck, sha, wait_for
from surec_imaj import BUYUK, GUNLUK, imajlar
from uefi_kabuk_kahin import keys


SAYFA = 4096
CERCEVELER = 516
ALAN = CERCEVELER * SAYFA
PENCERE = 0x54000000
YIGIN = 0x541F0000
KORUMA = YIGIN - SAYFA
NX = 1 << 63
ADRES = (1 << 52) - SAYFA
VARSAYILAN = ("baslat", "dosya", "buyuk", "yazma", "tus-usb", "tus-ps2",
             "yalitim", "tekrar", "dosya", "yazma")


class SurecConsole(Console):
    def input(self, text):
        tr = {"ü": ("bracket_right", False), "ö": ("comma", False),
              "-": ("equal", False), "_": ("equal", True)}
        for harf in text + "\n":
            ad, shift = tr.get(harf, keys(harf))
            if shift:
                self.key("shift", True)
            self.key(ad, True)
            self.key(ad, False)
            if shift:
                self.key("shift", False)

    def key(self, ad, bas):
        assert ad in self.qcodes, ad
        self.q.call("input-send-event", {"events": [{"type": "key", "data": {
            "down": bas, "key": {"type": "qcode", "data": ad},
        }}]})
        time.sleep(.012)

    def sayi(self, sembol):
        return struct.unpack("<Q", self.mem(self.symbols[sembol], 8))[0]

    def havuz(self):
        return self.sayi("uefi_surec_taban")

    def bos_cerceve(self):
        icerik = self.mem(self.havuz(), 2 * ALAN)
        bosluk = bytes(SAYFA)
        return sum(icerik[i:i + SAYFA] == bosluk for i in range(0, len(icerik), SAYFA))


def uefi_havuz_denetle(c):
    taban = c.havuz()
    assert taban % SAYFA == 0 and taban >= 0x100000
    b = int(re.search(rb"BILGI_ADRES (\d+)", c.ser.read_bytes())[1])
    alan = struct.unpack("<24Q", c.mem(b, 192))
    harita = c.mem(alan[9], alan[10])
    kayitlar = [struct.unpack_from("<IIQQQQ", harita, i) for i in range(0, len(harita), alan[11])]
    konum = taban
    for t, _, bas, _, sayfa, nitelik in sorted(kayitlar, key=lambda k: k[2]):
        if t in (3, 4, 7) and not nitelik & NX and bas <= konum < bas + sayfa * SAYFA:
            konum = bas + sayfa * SAYFA
    assert konum >= taban + 2 * ALAN
    korunan = [(alan[14], alan[15]), (b, b + SAYFA), (alan[9], alan[9] + alan[16]),
               (alan[3], alan[3] + alan[4]), (alan[20], alan[21])]
    assert all(taban + 2 * ALAN <= lo or taban >= hi for lo, hi in korunan)


def etkin_tablolar(c, elf):
    # Önce merhaba çalıştı. İkinci süreç öteki fiziksel alana geçmeli.
    taban = c.havuz() + ALAN
    regs = c.q.call("human-monitor-command", {"command-line": "info registers"})
    assert int(re.search(r"CR3=([0-9a-fA-F]+)", regs)[1], 16) == taban, regs
    assert int(re.search(r"EFER=([0-9a-fA-F]+)", regs)[1], 16) & 2048
    assert c.mem(c.havuz(), ALAN) == bytes(ALAN), "önceki süreç alanı temizlenmedi"
    for ofset, i in ((0, 0), (1, 1), (2, (PENCERE >> 21) & 511)):
        tablo = struct.unpack("<512Q", c.mem(taban + ofset * SAYFA, SAYFA))
        assert tablo[i] & (ADRES | 7) == (taban + (ofset + 1) * SAYFA) | 7
        assert all(not g & 4 for j, g in enumerate(tablo) if j != i), "gözetmen dalı kullanıcıya açıldı"
    ph = struct.unpack_from("<Q", elf, 32)[0]
    n = struct.unpack_from("<H", elf, 56)[0]
    beklenen = {}
    for i in range(n):
        tip, bayrak, _, va, _, _, boy, _ = struct.unpack_from("<II6Q", elf, ph + i * 56)
        if tip == 1 and boy:
            for a in range(va, va + boy, SAYFA):
                beklenen[a] = (5 if bayrak == 5 else NX | (7 if bayrak == 6 else 5))
    beklenen.update((a, NX | 7) for a in range(0x54100000, KORUMA, SAYFA))
    beklenen.update((a, NX | 7) for a in range(YIGIN, 0x54200000, SAYFA))
    ptes = struct.unpack("<512Q", c.mem(taban + 3 * SAYFA, SAYFA))
    for i, g in enumerate(ptes):
        a = PENCERE + i * SAYFA
        if a in beklenen:
            assert (g & (NX | 7)) == beklenen[a], (hex(a), hex(g), hex(beklenen[a]))
            assert (g & ADRES) == taban + (4 + i) * SAYFA
        else:
            assert g == 0, (hex(a), hex(g))
    print("✓ QMP: iki ayrı fiziksel alan, kullanıcı PTE izinleri, NX ve boş koruma sayfası", flush=True)


def tam_seri_bekle(c):
    def uygun():
        gercek = c.output()
        assert c.expected.startswith(gercek), (gercek, c.expected)
        return gercek == c.expected
    wait_for(c.p, uygun)


def tus_kos(c, metin, elf):
    c.input(metin)
    hazir = b"SAAT OK\nTUS_HAZIR\n"
    c.expected += hazir
    c.akis += "> " + metin + "\n" + hazir.decode()
    tam_seri_bekle(c)
    etkin_tablolar(c, elf)
    son = time.monotonic() + 8
    while time.monotonic() < son:
        assert c.p.poll() is None and c.output() == c.expected
        time.sleep(.05)
    for ad, bas in (("shift", True), ("a", True), ("a", False), ("shift", False),
                    ("bracket_left", True)):
        assert ad in c.qcodes, ad
        c.q.call("input-send-event", {"events": [{"type": "key", "data": {
            "down": bas, "key": {"type": "qcode", "data": ad},
        }}]})
        time.sleep(.04)
    c.q.call("input-send-event", {"events": [{"type": "key", "data": {
        "down": False, "key": {"type": "qcode", "data": "bracket_left"},
    }}]})
    satirlar = (b"TUS 42 0 1 1\nTUS 30 65 1 1\nTUS 30 65 0 1\nTUS 42 0 0 0\n"
                b"TUS 26 287 1 0\n[tus.elf pid=2 cikis=0]\nKABUK_HAZIR\n")
    c.expected += satirlar
    c.akis += satirlar.decode()
    tam_seri_bekle(c)


def yalitim_kos(c):
    def calistir(ad, satir):
        c.run(f"calistir /diskler/usb0p2/bin/{ad}.elf", satir.encode())

    gdt = c.mem(c.symbols["kdl_x86_gdt"], 56)
    ist = tuple(c.mem(c.symbols[f"kdl_ist{i}_alt"], 8) for i in (1, 2, 3))
    calistir("hata_parametre", "ISARETCI RED\n[hata_parametre.elf pid=2 cikis=0]\n")
    assert c.mem(c.symbols["kdl_x86_gdt"], 56) == gdt
    assert tuple(c.mem(c.symbols[f"kdl_ist{i}_alt"], 8) for i in (1, 2, 3)) == ist
    assert c.bos_cerceve() == 2 * CERCEVELER
    for pid, ad, adr, kod in (
        (3, "hata_adres", 0x100000, 5),
        (4, "hata_wx", PENCERE, 7),
        (5, "hata_yigin", KORUMA, 6),
        (6, "hata_komut", None, 0),
    ):
        neden = "#GP" if adr is None else f"#PF adr={adr:#x}"
        calistir(ad, f"[{ad}.elf pid={pid} SONLANDIRILDI: {neden} (kullanici)]\n")
        r = struct.unpack("<10Q", c.mem(c.symbols["kdl_x86_r3_blok"], 80))
        assert (r[2], r[3], r[5] & 3) == (13 if adr is None else 14, kod, 3), r
        if adr is not None:
            assert r[8] == adr, r
        assert c.bos_cerceve() == 2 * CERCEVELER
    calistir("sinir", "[sinir.elf pid=7 cikis=134]\n")
    c.run("calistir /diskler/usb0p2/bin/bozuk.elf", b"HATA ELF surec penceresi veya izinleri\n")
    c.run("echo yasiyor", b"yasiyor\n")


def tekrar_kos(c, exe):
    once = c.heap()
    surec_artisi = 0
    iz = c.symbols["kdl_x86_surec_olcum"]
    for pid in range(2, 52):
        c.run(f"calistir {exe}", f"{exe}\n[merhaba.elf pid={pid} cikis=1]\n".encode())
        assert c.bos_cerceve() == 2 * CERCEVELER, f"süreç {pid}"
        giris, cikis = struct.unpack("<QQ", c.mem(iz, 16))
        assert c.symbols["__heap_start"] <= giris <= cikis < c.symbols["__heap_end"], (giris, cikis)
        surec_artisi += cikis - giris
    sonra = c.heap()
    assert surec_artisi <= 64 * 1024 + 80, surec_artisi
    print(f"✓ 50 tekrar: {2 * CERCEVELER} boş çerçeve; süreç heap +{surec_artisi}; kabuk heap {once} → {sonra} bayt", flush=True)


def kos(w, tekrar, senaryo):
    programlar = {p.name: p.read_bytes() for p in (w / "kullanici").glob("*.elf")}
    assert "merhaba.elf" in programlar
    programlar["bozuk.elf"] = bytearray(programlar["merhaba.elf"])
    struct.pack_into("<H", programlar["bozuk.elf"], 16, 3)
    for n in range(1, tekrar + 1):
        kip = VARSAYILAN[(n - 1) % len(VARSAYILAN)] if senaryo == "matris" else senaryo
        usb, nvme, sata = imajlar(w, programlar)
        once = {p.disk: sha(p.disk) for p in (nvme, sata)}
        if kip != "yazma":
            once[usb[0].disk] = sha(usb[0].disk)
        esp = sha256(usb[0].read()).digest()
        with boot(w, usb[0], nvme, sata, klavye="ps2" if kip == "tus-ps2" else "usb") as (q, p, seri):
            c = SurecConsole(w, q, p, seri)
            uefi_havuz_denetle(c)
            assert c.bos_cerceve() == 2 * CERCEVELER
            exe = "/diskler/usb0p2/bin/merhaba.elf"
            c.run(f"calistir {exe} Ağaç İş",
                  (f"{exe}\nAğaç\nİş\n[merhaba.elf pid=1 cikis=3]\n").encode())
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
            if kip == "yalitim":
                yalitim_kos(c)
            if kip == "tekrar":
                tekrar_kos(c, exe)
            assert c.bos_cerceve() == 2 * CERCEVELER, "süreç çerçevesi geri dönmedi"
            c.screen()
            c.shutdown()
        assert all(sha(p) == h for p, h in once.items())
        assert sha256(usb[0].read()).digest() == esp
        if kip == "yazma":
            fsck(w, usb[1], "surec-yazma")
            fat = Fat(usb[1].read())
            assert fat.get("/NOTLAR/kalici.txt") == "Ağaç İşSon".encode()
            assert fat.get("/NOTLAR/kesilecek.txt") == b""
            assert fat.get("/NOTLAR/günlük.txt") == GUNLUK
            assert fat.get("/NOTLAR/buyuk.txt") == BUYUK
            assert not {"yeni.txt", "silinecek.txt", "gecici"} & fat.get("NOTLAR").keys()
            fat.verify_info()
        print(f"✓ Süreç OVMF {kip} #{n}: tek açılış, seri, GOP, kaynaklar ve disk", flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("dizin", type=Path)
    p.add_argument("--tekrar", type=int, default=10)
    p.add_argument("--senaryo", choices=("matris", "baslat", "dosya", "buyuk", "yazma", "tus-usb", "tus-ps2", "yalitim", "tekrar"), default="matris")
    args = p.parse_args()
    kos(args.dizin.resolve(), args.tekrar, args.senaryo)
