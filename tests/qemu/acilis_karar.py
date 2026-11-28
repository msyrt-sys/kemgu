"""Açılış süresi kapısının kararı: çekişme altında eşiği aşan ölçümü sessiz ölçümle doğrular.

[D-794] Ölçüm duvar saatidir ve x86 konuk ARM64 konakta TCG ile koştuğu için konuk TSC'si
de konak saatinden türer: yük altında ikisi aynı oranda şişer (ölçüldü: konuk 708,9 /
host 720,6 ms). Konuk damgası bu yüzden yükten arınmış bir ölçü DEĞİLDİR. Eşik
değiştirilmez; yalnız başka QEMU süreçleri koşarken eşiği aşan bir ölçüm KARAR SAYILMAZ:
onlar bitene kadar (sınırlı) beklenir, ölçüm tekrarlanır ve kararı sessiz ölçüm verir.
Çekişmesiz bir aşım hemen kırmızıdır; sessizlik sağlanamazsa da çekişmeli yeniden ölçüm
karar verir (aşım yine kırmızı). Yani sessiz ortamda eşiği aşan hiçbir açılış geçmez.
"""
import os
import time


def yabanci_qemu(proc='/proc'):
    """Çalışan qemu-system süreçlerinin sayısı.

    Kendi QEMU'muz kapalıyken çağrılır (`boot` çıkışta süreci öldürüp bekler), dolayısıyla
    sayılanların hepsi başka kapılara ya da başka kullanıcılara aittir. /proc yoksa
    (Windows) 0 döner: çekişme bilinemez ve karar eskisi gibi ilk ölçümdür.
    """
    try:
        adlar = os.listdir(proc)
    except OSError:
        return 0
    say = 0
    for ad in adlar:
        if not ad.isdigit():
            continue
        try:
            with open(os.path.join(proc, ad, 'comm'), 'rb') as dosya:
                # comm 15 bayta kırpılır: "qemu-system-x86", "qemu-system-aar".
                if dosya.read().startswith(b'qemu-system'):
                    say += 1
        except OSError:
            continue
    return say


def sessizi_bekle(sure, *, say=yabanci_qemu, uyku=time.sleep, saat=time.monotonic, yerles=3.0):
    """Başka QEMU süreci kalmayana dek bekle; `yerles` sn kesintisiz sıfır gerekir.

    `sure` dolarsa False döner (çağıran yine ölçer ve karar o ölçümün olur).
    """
    son = saat() + sure
    sifir_bas = None
    while True:
        simdi = saat()
        if say() == 0:
            if sifir_bas is None:
                sifir_bas = simdi
            if simdi - sifir_bas >= yerles:
                return True
        else:
            sifir_bas = None
        if simdi >= son:
            return False
        uyku(1.0)


def karar(olc, esik, *, bekle=sessizi_bekle, sure=1200.0, yaz=print):
    """`olc()` -> (host_medyan, konuk_medyan, en_cok_yabanci_qemu). Geçtiyse True.

    İki medyan da eşiğin ALTINDA olmalı (biri yetmez). Eşiği aşan çekişmeli ölçüm bir kez,
    sessizlik beklendikten sonra tekrarlanır; çekişmesiz aşım tekrarlanmaz.
    """
    host, konuk, cekisme = olc()
    if host < esik and konuk < esik:
        return True
    if cekisme == 0:
        yaz(f'ACILIS_KARAR eşik aşıldı ve çekişme yok (host={host:.1f} konuk={konuk:.1f}) -> kırmızı',
            flush=True)
        return False
    yaz(f'ACILIS_KARAR eşik çekişme altında aşıldı (host={host:.1f} konuk={konuk:.1f}, en çok '
        f'{cekisme} başka QEMU) -> sessizlik bekleniyor (en çok {sure:.0f} sn)', flush=True)
    bas = time.monotonic()
    sessiz = bekle(sure)
    beklenen = time.monotonic() - bas
    host, konuk, cekisme = olc()
    gecti = host < esik and konuk < esik
    yaz(f'ACILIS_KARAR yeniden ölçüm ({beklenen:.0f} sn beklendi, '
        f'{"sessiz" if sessiz else "sessizlik SAĞLANAMADI"}, çekişme={cekisme}): '
        f'host={host:.1f} konuk={konuk:.1f} -> {"geçti" if gecti else "kırmızı"}', flush=True)
    return gecti
