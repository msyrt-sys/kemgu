#!/usr/bin/env python3
"""[D-794] Açılış süresi kapısının çekişme kararını QEMU'suz, sahte ölçümlerle sınar."""
import sys
import tempfile
from pathlib import Path

DIZIN = Path(__file__).resolve().parent
sys.path.insert(0, str(DIZIN.parent / "tests" / "qemu"))
from acilis_karar import karar, sessizi_bekle, yabanci_qemu  # noqa: E402

ESIK = 650.0


def sessiz_yaz(*_a, **_k):
    pass


class Olcum:
    """Sıradaki (host, konuk, çekişme) değerlerini döndürür; kaç kez ölçüldüğünü sayar."""

    def __init__(self, *degerler):
        self.degerler = list(degerler)
        self.sayi = 0

    def __call__(self):
        self.sayi += 1
        return self.degerler.pop(0)


class Bekleme:
    def __init__(self, sonuc=True):
        self.sonuc = sonuc
        self.sayi = 0

    def __call__(self, _sure):
        self.sayi += 1
        return self.sonuc


def durum(ad, olcum, bekleme, beklenen, olcum_sayisi, bekleme_sayisi):
    sonuc = karar(olcum, ESIK, bekle=bekleme, yaz=sessiz_yaz)
    if (sonuc, olcum.sayi, bekleme.sayi) != (beklenen, olcum_sayisi, bekleme_sayisi):
        raise AssertionError(f"{ad}: karar={sonuc} ölçüm={olcum.sayi} bekleme={bekleme.sayi}, "
                             f"beklenen {beklenen}/{olcum_sayisi}/{bekleme_sayisi}")


def karar_durumlari():
    # Eşik altı: tek ölçüm, bekleme yok (çekişme olsa bile).
    durum("eşik altı", Olcum((500.0, 490.0, 3)), Bekleme(), True, 1, 0)
    # Çekişmesiz aşım: HEMEN kırmızı, yeniden ölçüm yok — sessiz aşım asla geçmez.
    durum("çekişmesiz aşım", Olcum((700.0, 690.0, 0)), Bekleme(), False, 1, 0)
    # Çekişmeli aşım: sessizlik beklenir, kararı sessiz ölçüm verir.
    durum("çekişmeli aşım, sessiz geçer", Olcum((700.0, 690.0, 4), (500.0, 495.0, 0)),
          Bekleme(), True, 2, 1)
    durum("çekişmeli aşım, sessiz de aşar", Olcum((700.0, 690.0, 4), (1866.5, 1850.0, 0)),
          Bekleme(), False, 2, 1)
    # Sessizlik sağlanamazsa bile karar yeniden ölçümündür (aşım kırmızı).
    durum("sessizlik sağlanamadı", Olcum((700.0, 690.0, 4), (720.0, 710.0, 2)),
          Bekleme(False), False, 2, 1)
    # İki medyanın İKİSİ de eşiğin altında olmalı.
    durum("yalnız konuk aşar", Olcum((640.0, 660.0, 0)), Bekleme(), False, 1, 0)
    durum("yalnız host aşar", Olcum((660.0, 640.0, 0)), Bekleme(), False, 1, 0)
    # Eşiğe EŞİT ölçüm geçmez (katı küçük).
    durum("eşiğe eşit", Olcum((ESIK, 600.0, 0)), Bekleme(), False, 1, 0)


def proc_sayimi():
    with tempfile.TemporaryDirectory() as kok:
        kok = Path(kok)
        for pid, comm in (("101", b"qemu-system-x86\n"), ("102", b"qemu-system-aar\n"),
                          ("103", b"bash\n"), ("104", b"qemu-img\n")):
            (kok / pid).mkdir()
            (kok / pid / "comm").write_bytes(comm)
        (kok / "self").mkdir()                  # sayısal olmayan girdi sayılmaz
        (kok / "105").mkdir()                   # comm'u olmayan (yarış) süreç atlanır
        bulunan = yabanci_qemu(str(kok))
        if bulunan != 2:
            raise AssertionError(f"sahte /proc: {bulunan} qemu-system, beklenen 2")
    if yabanci_qemu("/var/empty/kemgu-yok-proc") != 0:
        raise AssertionError("/proc yokken 0 dönmeli (çekişme bilinemez)")


def bekleme_mantigi():
    class Saat:
        def __init__(self):
            self.t = 0.0

        def __call__(self):
            return self.t

        def uyu(self, s):
            self.t += s

    # Sayılar sıfıra düşer ve yerleşme süresi boyunca sıfır kalır -> True.
    saat = Saat()
    seri = iter([2, 1, 0, 1, 0, 0, 0, 0, 0, 0])
    if not sessizi_bekle(60, say=lambda: next(seri, 0), uyku=saat.uyu, saat=saat, yerles=3.0):
        raise AssertionError("sessizlik oluştuğu hâlde False döndü")
    if not 3.0 <= saat.t <= 10.0:
        raise AssertionError(f"yerleşme penceresi yanlış ölçüldü: t={saat.t}")
    # Hep çekişme -> süre dolunca False (sonsuz bekleme yok).
    saat = Saat()
    if sessizi_bekle(30, say=lambda: 1, uyku=saat.uyu, saat=saat, yerles=3.0):
        raise AssertionError("çekişme sürerken True döndü")
    if not 30.0 <= saat.t <= 32.0:
        raise AssertionError(f"zaman aşımı yanlış: t={saat.t}")


def kahin_baglantisi():
    """Kâhin bu kararı GERÇEKTEN kullanmalı: eski `assert median(..) < esik` dönerse bu test
    hiçbir şey ölçmüyor olurdu (birim testi yeşil, kapı eski davranışta)."""
    metin = (DIZIN.parent / "tests" / "qemu" / "acilis_suresi.py").read_text(encoding="utf-8")
    for parca in ("from acilis_karar import", "assert karar(", "cekisme=yabanci_qemu()",
                  "max(cekisme,yabanci_qemu())"):
        if parca not in metin:
            raise AssertionError(f"acilis_suresi.py karar bağlantısını kaybetti: '{parca}' yok")


if __name__ == "__main__":
    karar_durumlari()
    proc_sayimi()
    bekleme_mantigi()
    kahin_baglantisi()
    print("ACILIS_KARAR OK: eşik altı, çekişmesiz aşım, çekişmeli aşım (sessiz geçer / aşar / "
          "sessizlik yok), iki medyan, eşiğe eşit, /proc sayımı, sessizlik bekleme, kâhin bağlantısı")
