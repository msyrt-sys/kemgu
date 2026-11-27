#!/usr/bin/env python3
"""E4k §9.6 / §10.6 ölçümü: AP'de koşabilen kod BSP'nin bölge durumuna erişiyor mu?

Girdi: x86 kabuk çekirdeğinin birleşik biriminden üretilmiş LLVM IR'ı
(`bm_x86_uefi_harness.sh` ile aynı birleştirme + `test/x86_heap_bagla.py` +
`--llvm --mimari x86_64-none`). Kullanım ve tam komut belgede (§10.6).

Yöntem: AP köklerinden çağrı grafiği erişilebilirliği. Kenarlar bilerek FAZLA
yaklaşımdır: gövdedeki her `@sembol` başvurusu (çağrı, işlev adresi, küresel)
ve satır içi asm metnindeki `call AD` / `AD(%rip)` kenar sayılır. Fazla yaklaşım
yanlış-negatif üretmez; "dokunmuyor" sonucu gerçek grafikte de geçerlidir.

Çıkış kodu: 0 = bulgu yok · 1 = AP kümesinde yasak başvuru var ·
2 = ölçüm geçersiz (kök tanımı ya da kontrol küreseli IR'da yok; araç yanlış
dünyayı ölçüyor olabilir, D-425/D-486).

⚠ BUGÜN KAPI DEĞİL (D-786). Kural ancak A4'te (tahliye çekirdekte açılınca) yük
taşır; A4 bu aracı bir kapıya bağlamalıdır (D-446: var olan ama çağrılmayan kapı
"bu alan ölçülüyor" yanılsaması yaratır — bu araç bir kapı gibi okunmamalı).
"""
import re
import sys
from collections import deque

# AP'de koşabilen girişler (kaynakta doğrulandı, belge §10.6):
#   kem_ap_giris      AP giriş noktası (smp_trampolin_yaz: `leaq kem_ap_giris(%rip)`)
#   kxs_smp_isci      AP zaman kancası (kzs_baslat: `leaq kxs_smp_isci(%rip)`)
#   smp_ap_zaman_irq  vektör 49 (AP yerel zamanlayıcısı)
#   kem_ipi_isle      vektör 80-83 (IPI, TLB vurgulama)
#   kdl_istisna_isle  çekirdek kipindeki istisnalar (isr0..31)
#   kdl_panik         AP'den çağrılan panik yolu
VARSAYILAN_KOKLER = ['kem_ap_giris', 'kxs_smp_isci', 'smp_ap_zaman_irq',
                     'kem_ipi_isle', 'kdl_istisna_isle', 'kdl_panik']

# BSP'nin bölge durumuna işaret eden ham işaretçi küreselleri (yalnız BSP'nin
# hizmet penceresinde kurulu; kem_os_x86_kabuk.kem ks_adim).
HAM = ['ks_yonetim', 'ksv_ekran_adr', 'ksv_baglam_adresi', 'kxs_ek_vekil']

# Tahsis yapan giriş noktaları (bölge, dizi, metin üreticileri, ham yığın).
# Salt okuyan metin işlevleri (kdl_metin_bayt/uzunluk) BİLEREK dışarıda.
TAHSIS = re.compile(
    r'^(kdl_bolge_|kdl_global_bolge_al$|kdl_dizi_olustur|kdl_dizi_ekle|kdl_dizi_buyut|'
    r'kdl_dizi_kapasite_ayarla|kdl_metin_birlestir|kdl_metin_kes|kdl_metin_kucuk|'
    r'kdl_metin_buyuk|kdl_tam_metin|kdl_kesirli_metin|kdl_kod_metin|kem_bump|'
    r'malloc$|calloc$|realloc$)')

SEMBOL = r'@("(?:[^"\\]|\\.)*"|[A-Za-z_$.][\w$.]*)'
TANIM = re.compile(r'^define [^@]*' + SEMBOL + r'\(')
BASVURU = re.compile(SEMBOL)
ASM_CAGRI = re.compile(r'\bcall\s+\*?([A-Za-z_][\w$.]*)')
ASM_RIP = re.compile(r'\b([A-Za-z_][\w$.]*)\(%rip\)')


def ad(s):
    return s[1:-1] if s.startswith('"') else s


def grafik(satirlar):
    govde, cur = {}, None
    for s in satirlar:
        m = TANIM.match(s)
        if m:
            cur = ad(m.group(1))
            govde[cur] = []
            continue
        if cur is not None:
            if s.startswith('}'):
                cur = None
            else:
                govde[cur].append(s)
    kenar = {}
    for f, gs in govde.items():
        k = set()
        for s in gs:
            k.update(ad(m.group(1)) for m in BASVURU.finditer(s))
            if ' asm ' in s:
                k.update(m.group(1) for m in ASM_CAGRI.finditer(s))
                k.update(m.group(1) for m in ASM_RIP.finditer(s))
        kenar[f] = k
    return govde, kenar


def main():
    if len(sys.argv) < 2:
        print('kullanım: smp_ap_erisim.py <ir.ll> [kök1,kök2,...]', file=sys.stderr)
        return 2
    satirlar = open(sys.argv[1], encoding='utf-8').read().splitlines()
    kokler = sys.argv[2].split(',') if len(sys.argv) > 2 else VARSAYILAN_KOKLER
    govde, kenar = grafik(satirlar)

    gecersiz = False
    eksik = [k for k in kokler if k not in govde]
    if eksik:
        print('ÖLÇÜM GEÇERSİZ: IR içinde tanımı olmayan kök:', ', '.join(eksik))
        gecersiz = True
    for h in HAM:
        if not any(h in kenar[f] for f in govde):
            print(f'ÖLÇÜM GEÇERSİZ: kontrol küreseli {h} hiçbir işlevde yok '
                  '(adı değişti mi? araç yanlış dünyayı ölçüyor olabilir)')
            gecersiz = True
    if gecersiz:
        return 2

    eris, ebeveyn = set(kokler), {k: None for k in kokler}
    q = deque(kokler)
    while q:
        f = q.popleft()
        for g in sorted(kenar.get(f, ())):
            if g in govde and g not in eris:
                eris.add(g)
                ebeveyn[g] = f
                q.append(g)

    rho_alan = [f for f in sorted(eris)
                if any(re.match(r'^define [^@]*@"?' + re.escape(f) + r'"?\(ptr %rho', s)
                       for s in satirlar if s.startswith('define'))]
    print(f'AP kökleri: {", ".join(kokler)}')
    print(f'AP-erişilebilir tanımlı işlev: {len(eris)} · `ptr %rho` alan: {len(rho_alan)}')

    bulgu = 0
    for f in sorted(eris):
        ham = sorted(h for h in HAM if h in kenar[f])
        tah = sorted(g for g in kenar[f] if TAHSIS.match(g))
        if ham or tah or f in rho_alan:
            bulgu += 1
            zincir, x = [], f
            while x is not None:
                zincir.append(x)
                x = ebeveyn[x]
            print(f'  BULGU {f}: ham={ham} tahsis={tah} rho={f in rho_alan} '
                  f'(zincir: {" <- ".join(zincir)})')
    for h in HAM:
        kullanan = [f for f in govde if h in kenar[f]]
        print(f'  [kontrol] {h}: {len(kullanan)} işlev başvuruyor, AP kümesinde '
              f'{sum(1 for f in kullanan if f in eris)}')
    print(f'bulgu: {bulgu}')
    return 1 if bulgu else 0


if __name__ == '__main__':
    sys.exit(main())
