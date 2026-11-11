#!/usr/bin/env python3
"""[D-751] stdlib API'sini `genel` işaretler (G8). Gizlilik D-590'dan beri her yolda
zorunlu (T041) ve stdlib'in HİÇBİR bildirimi `genel` değildi → hiçbir modül başka bir
dosyadan içe alınamıyordu (ölçüldü: seçili ve takma adlı `kullan` ikisi de T041).
KURAL (dışa açma yönünde yanlı): üst-düzey HER bildirim genel; yalnız aşağıdaki iç
yardımcılar özel kalır. Test-tabanlı kural DENENDİ ve REDDEDİLDİ: testler API'yi
kapsamıyor (ag.gonder/al, regex.ara, sonuç/opsiyonel harita/bagla dışarıda kalıyordu).
Fazladan açılan yardımcı küçük bir kusurdur; kapalı kalan API kullanıcıyı kırar.
kripto/* hariç: o modüller birbirine `kullan`sız (birleştirme ile) bağlı — ayrı iş.
Kullanım: python3 tools/stdlib_genel.py [--uygula]"""
import re, sys
from pathlib import Path
KOK = Path(__file__).resolve().parent.parent
DEK = re.compile(r'^(genel )?(işlev|yapı(?: tekkez)?|çeşit|sabit) ([^\s(<:{]+)', re.M)
OZEL_ONEK = ('ja_', 'rx_', 'Rx', 'RX_')   # geo_/ist_ DEĞİL: o modüllerin API'si de önekli
OZEL_AD = {
    'json': {'JsonAyristirici', 'kacisla', 'sayi_metin', 'rakam', 'hex_deger', 'sekme',
             'satir_basi', 'geri_sil', 'sayfa_ileri', 'json_dizi_yaz', 'nesne_yaz'},
    'arama': {'arama_boyut'}, 'rastgele': {'rastgele_boyut'},
    'obek': {'obek_yukari_ele', 'obek_asagi_ele'}, 'tablo': {'tablo_kova'},
    'kesir': {'kesir_obeb'}, 'sirali': {'hizli_sirala_aralik'},
    'metin': {'ascii_harf_mi', 'tr_harf_2byte', 'bosluk_bayt_mi'},
    'karsilastir': {'dizi_uzunluk'},
    'geometri': {'geo_mutlak', 'geo_boyut'}, 'istatistik': {'ist_boyut', 'ist_sirala_kopya'},
}
uygula = '--uygula' in sys.argv
toplam = [0, 0]
for f in sorted(list((KOK/'stdlib').glob('*.kem')) + list((KOK/'stdlib/temel').glob('*.kem'))):
    if f.name == 'kripto.kem': continue
    s = f.read_text(encoding='utf-8')
    adlar = list(DEK.finditer(s))
    api = [m for m in adlar if not m.group(3).startswith(OZEL_ONEK)
           and m.group(3) not in OZEL_AD.get(f.stem, set())]
    yeni = s
    for m in sorted(api, key=lambda x: -x.start()):
        if not m.group(1): yeni = yeni[:m.start()] + 'genel ' + yeni[m.start():]
    toplam[0] += len(adlar); toplam[1] += len(api)
    print(f'  {str(f.relative_to(KOK)):30} {len(api):3}/{len(adlar):3} genel')
    if uygula and yeni != s: f.write_text(yeni, encoding='utf-8')
print(f'=== {toplam[1]}/{toplam[0]} bildirim genel{" (UYGULANDI)" if uygula else " (kuru koşum)"} ===')
