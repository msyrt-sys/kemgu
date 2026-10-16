# Tur 14 — zaman_x86 — istem

## SISTEM
````
Sen KEMGU deposunda mekanik kod üreten bir yürütücüsün. KEMGU, Türkçe
sözdizimli bir sistem programlama dili ve onunla yazılmış bir işletim sistemidir.
Kararları orkestratör verir; sen yalnız istenen değişikliği üretirsin.

KESİN KURALLAR:
- Yalnız görevde adı geçen dosyalara dokun. `src/*.c`, `selfhost/*.kem`,
  `runtime/kem_gorev.kem`, `runtime/kem_mmu.kem`, `runtime/kem_zaman.kem`
  YASAK.
- Bağlamdaki desene BİREBİR uy (adlandırma, yorum dili Türkçe, `çıplak işlev`).
- Emin olmadığın bir şeyi uydurma; varsayımını yanıtın sonunda "VARSAYIMLAR"
  başlığı altında listele.

ÇIKTI BİÇİMİ (zorunlu, başka biçim kabul edilmez):
Var olan dosyada değişiklik — ESKI metin dosyada AYNEN ve TEK kez geçmeli:
```duzenle:<depo-göreli yol>
<<<<<<< ESKI
<değişecek metin, boşluklar dahil birebir>
=======
<yeni metin>
>>>>>>> YENI
```
Yeni dosya:
```yeni:<depo-göreli yol>
<dosyanın tam içeriği>
```
Birden çok blok verebilirsin. Bloklardan sonra kısa bir açıklama yaz.
````

## KULLANICI
````
# ORTAK BAĞLAM (her turda aynı)

## KEMGU çekirdek kuralları (derleyiciden ölçülmüş)
- Normal işlev ilk parametre olarak bölge yetkisi `ptr %rho` alır ve prologunda
  `call ptr @kdl_bolge_olustur()` yapar. `çıplak işlev` ise C-ABI'dir:
  `define i64 @f(i64 %n)` — ρ YOK, bölge prologu YOK, ham deref `volatile`.
  Allocator ve çekirdek ilkelleri bu yüzden çıplaktır (aksi hâlde
  kdl_bolge_olustur → malloc → ... kendini tetikler).
- E013: çıplak işlev yalnız çıplak/extern (ve yerleşik) çağırabilir.
- `satıriçi_asm { mimari: <arm64|x86_64> şablon: r#"..."# girdi("k", e)
  çıktı("=k", &v) bozulan("~{memory}") }` — `güvensiz { }` içinde yazılır.
  AS001: etiket hedef mimariyle (`--mimari`) uyuşmazsa derleme DURUR → bir
  dosyada YALNIZ TEK mimarinin asm'si olabilir; mimariye özgü kod AYRI dosyaya
  konur ve Makefile CAT listesi mimariye göre seçer (D2b kararı).
- Şablon AT&T sözdizimi (x86); `$0..` çıktılar→girdiler sırası; `{dx}`,
  `{al}` belirli yazmaç kısıtları (boyut eşleşmeli: tam16↔dx, tam8↔al).
- KEMGU dizgi literalinde kaçış YOK (`"\n"` iki karakterdir).
- Sayılar ondalık yazılır (`1016` = 0x3F8); `olarak` tip dönüşümüdür.
- Üst düzey ileri bildirim YOK; çapraz dosya çağrısı için dosyalar CAT edilir
  (tek derleme birimi).

## runtime/kem_heap.kem (desen kaynağı, tamamı)
```kemgu
// kem_heap.kem — K1 (D-260): SAF-.kem çıplak `malloc`/`free` — C kdl_bare_heap YERİNE.
// ============================================================================
// Bootstrap-circularity F3'te (D-259) kırıldı; K1 bunu kem_os'a ENTEGRE eder:
// runtime malloc/free artık SAF-.kem. Çıplak (region-prologue'suz, C-ABI) →
// @malloc(i64)->ptr / @free(ptr) sembollerini kdl_bolge.c'nin beklediği C-ABI
// ile sağlar. kdl_bare_heap.c'de bu ikisi `#ifndef KEMGU_KEM_MALLOC` ile devre
// dışı (memcpy/memset/kdl_global_bolge_al/kdl_dizi orada KALIR — K4).
//
// Model (C kdl_bare_heap ile birebir): her tahsis 16-hizalı, {boyut, sonraki}
// 16-baytlık header. free() bloğu LIFO serbest-listeye iter; malloc() önce
// ilk-uyan serbest bloğu arar (F4.3 per-iter region-free frame'lerini geri
// kazanır → sınırlı 64KB heap'te döngü OOM'unu önler), yoksa bump'lar.
// Çıplak = güvensiz-tier → ham-pointer (inttoptr/ptrtoint/deref) + küresel
// erişimi explicit `güvensiz {}` gerektirmez.

küresel değişken kem_bump: tam64 = 0;   // bir sonraki bump adresi (16-hizalı)
küresel değişken kem_son: tam64  = 0;   // heap üst sınırı (dışlayıcı)
küresel değişken kem_bos: tam64  = 0;   // serbest-liste başı (blok adresi; 0=boş)
küresel değişken kem_global_bolge: tam64 = 0;   // K4b: lazy tek global bölge (adres)

// Boot-glue çağırır (main'in ilk malloc'undan ÖNCE): heap penceresini kur.
çıplak işlev kem_heap_kur(taban: tam64, son: tam64) {
    kem_bump = ((taban + 15) / 16) * 16;   // 16-hizala
    kem_son = son;
    kem_bos = 0;
}

// C-ABI malloc: void* malloc(size_t) → @malloc(i64)->ptr.
çıplak işlev malloc(n: tam64) -> *tam8 {
    değişken istek: tam64 = n;
    eğer istek == 0 { istek = 1; }               // 0-bayt → benzersiz geçerli ptr
    değişken toplam: tam64 = ((istek + 31) / 16) * 16;   // header(16)+istek, 16-hizalı

    // 1) Serbest-liste ilk-uyum (header'ı KORU → böl/küçült yok, bloğu bütün ver).
    değişken onceki: tam64 = 0;
    değişken cur: tam64 = kem_bos;
    iken cur != 0 {
        değişken hsize_p: *tam64 = cur olarak *tam64;             // header.boyut @ +0
        değişken bsize: tam64 = *hsize_p;
        değişken hnext_p: *tam64 = (cur + 8) olarak *tam64;       // header.sonraki @ +8
        değişken bnext: tam64 = *hnext_p;
        eğer bsize >= toplam {
            eğer onceki == 0 {
                kem_bos = bnext;                                  // liste başından çıkar
            }
            değilse {
                değişken op_p: *tam64 = (onceki + 8) olarak *tam64;
                *op_p = bnext;                                    // önceki.sonraki = bnext
            }
            ver (cur + 16) olarak *tam8;                          // blok + header
        }
        onceki = cur;
        cur = bnext;
    }

    // 2) Bump tahsis (sınır + OOM korumalı; bump zaten 16-hizalı, toplam 16-katı).
    değişken p: tam64 = kem_bump;
    eğer p + toplam > kem_son { ver 0 olarak *tam8; }             // OOM → null
    kem_bump = p + toplam;
    değişken hs_p: *tam64 = p olarak *tam64;
    *hs_p = toplam;                                               // header.boyut
    değişken hn_p: *tam64 = (p + 8) olarak *tam64;
    *hn_p = 0;                                                    // header.sonraki
    ver (p + 16) olarak *tam8;
}

// C-ABI free: void free(void*) → @free(ptr).
çıplak işlev free(p: *tam8) {
    değişken pa: tam64 = p olarak tam64;         // ptrtoint
    eğer pa == 0 { ver; }
    değişken h: tam64 = pa - 16;                 // header
    değişken hn_p: *tam64 = (h + 8) olarak *tam64;
    *hn_p = kem_bos;                             // header.sonraki = eski baş (LIFO it)
    kem_bos = h;
}

// ============================================================================
// K3 (D-261): SAF-.kem çıplak REGION (bölge) runtime — C kdl_bolge.c YERİNE.
// Bölge = malloc'lu blok tek-yönlü listesi + blok-içi bump. malloc/free AYNI
// dosyada (yukarıda, çıplak) → çıplak→çıplak call resolve (islev_bul). Region
// memcpy GEREKMEZ. Sızıntı-sayaçları YOK (kem_os leak-check etmez; C tarafı
// tests için tutar). Struct düzenleri (C kdl_bolge.c ile birebir):
//   KdlBolge     { bas@0(ptr), blok_sayisi@8(i32) }        malloc 16
//   KdlBolgeBlok { sonraki@0, kapasite@8, kullanilan@16, veri@24 }  malloc 24+kap
// Hizalama ÇALIŞMA-ZAMANI adresine göre (((x+15)/16)*16), bitwise gerekmez.

// Yeni blok: 24-bayt header + kap bayt veri malloc'la (NULL → 0).
çıplak işlev kem_blok_olustur(kap: tam64) -> *tam8 {
    değişken blk: *tam8 = malloc(24 + kap);
    eğer (blk olarak tam64) == 0 { ver 0 olarak *tam8; }
    değişken ba: tam64 = blk olarak tam64;
    değişken sp: *tam64 = ba olarak *tam64;
    *sp = 0;                                       // sonraki = NULL
    değişken kp: *tam64 = (ba + 8) olarak *tam64;
    *kp = kap;                                     // kapasite
    değişken up: *tam64 = (ba + 16) olarak *tam64;
    *up = 0;                                       // kullanilan
    ver blk;
}

// kdl_bolge_olustur() -> KdlBolge* : yeni bölge (KdlBolge + ilk 64KB blok).
çıplak işlev kdl_bolge_olustur() -> *tam8 {
    değişken b: *tam8 = malloc(16);
    eğer (b olarak tam64) == 0 { ver 0 olarak *tam8; }
    değişken blk: *tam8 = kem_blok_olustur(65536);
    eğer (blk olarak tam64) == 0 { free(b); ver 0 olarak *tam8; }
    değişken ba: tam64 = b olarak tam64;
    değişken basp: *tam64 = ba olarak *tam64;
    *basp = blk olarak tam64;                      // bas = ilk blok
    değişken bsp: *tam32 = (ba + 8) olarak *tam32;
    *bsp = 1;                                      // blok_sayisi = 1
    ver b;
}

// kdl_bolge_ayir(b, n) -> void* : aktif bloktan 16-hizalı bump; taşarsa yeni blok.
çıplak işlev kdl_bolge_ayir(b: *tam8, n: tam64) -> *tam8 {
    eğer (b olarak tam64) == 0 { ver 0 olarak *tam8; }
    değişken nn: tam64 = n;
    eğer nn == 0 { nn = 1; }
    değişken hn: tam64 = ((nn + 15) / 16) * 16;    // hiza_yukari
    değişken ba: tam64 = b olarak tam64;
    değişken basp: *tam64 = ba olarak *tam64;
    değişken blok: tam64 = *basp;                  // b->bas
    eğer blok != 0 {
        değişken kp: *tam64 = (blok + 8) olarak *tam64;
        değişken kapasite: tam64 = *kp;
        değişken up: *tam64 = (blok + 16) olarak *tam64;
        değişken kullanilan: tam64 = *up;
        değişken cur: tam64 = (blok + 24) + kullanilan;   // veri + kullanilan
        değişken ahiz: tam64 = ((cur + 15) / 16) * 16;
        değişken pad: tam64 = ahiz - cur;
        değişken kalan: tam64 = kapasite - kullanilan;
        eğer pad <= kalan {
            eğer hn <= (kalan - pad) {
                *up = kullanilan + pad + hn;
                ver ahiz olarak *tam8;
            }
        }
    }
    // yeni blok (hn + en kötü hiza payı; en az 64KB)
    değişken kap: tam64 = hn + 16;
    eğer kap < 65536 { kap = 65536; }
    değişken yeni: *tam8 = kem_blok_olustur(kap);
    eğer (yeni olarak tam64) == 0 { ver 0 olarak *tam8; }
    değişken ya: tam64 = yeni olarak tam64;
    değişken ysp: *tam64 = ya olarak *tam64;
    *ysp = blok;                                   // yeni->sonraki = eski bas
    *basp = ya;                                    // b->bas = yeni
    değişken bsp: *tam32 = (ba + 8) olarak *tam32;
    *bsp = *bsp + 1;                               // blok_sayisi++
    değişken veri: tam64 = ya + 24;
    değişken ahiz2: tam64 = ((veri + 15) / 16) * 16;
    değişken pad2: tam64 = ahiz2 - veri;
    değişken yup: *tam64 = (ya + 16) olarak *tam64;
    *yup = pad2 + hn;                              // yeni->kullanilan
    ver ahiz2 olarak *tam8;
}

// K4b (D-263): kdl_global_bolge_al() -> KdlBolge* : lazy tek global bölge (main +
// ρ-seed her program çağırır). C kdl_bare_heap.c kopyasının .kem çıplak eşi;
// kdl_bolge_olustur (.kem, aynı dosya) çağırır. küresel state (WALL-1).
çıplak işlev kdl_global_bolge_al() -> *tam8 {
    eğer kem_global_bolge == 0 {
        değişken b: *tam8 = kdl_bolge_olustur();
        kem_global_bolge = b olarak tam64;
    }
    ver kem_global_bolge olarak *tam8;
}

// kdl_bolge_serbest(b) : tüm blokları + KdlBolge'yi free et.
çıplak işlev kdl_bolge_serbest(b: *tam8) {
    eğer (b olarak tam64) == 0 { ver; }
    değişken ba: tam64 = b olarak tam64;
    değişken basp: *tam64 = ba olarak *tam64;
    değişken blok: tam64 = *basp;
    iken blok != 0 {
        değişken sp: *tam64 = blok olarak *tam64;
        değişken eski: tam64 = *sp;               // blok->sonraki
        free(blok olarak *tam8);
        blok = eski;
    }
    free(b);
}

// ============================================================================
// K4a helpers (D-262): SAF-.kem çıplak memcpy/memset — C kdl_bare_heap YERİNE.
// inttoptr byte-loop → clang -O2 loop-idiom'a YAKALANMAZ (self-recurse yok,
// disasm-doğrulandı). clang struct-kopya/zero + K2 dizi bunları çağırır.
// ============================================================================
çıplak işlev memcpy(d: *tam8, s: *tam8, n: tam64) -> *tam8 {
    değişken i: tam64 = 0;
    iken i < n {
        değişken sp: *tam8 = ((s olarak tam64) + i) olarak *tam8;
        değişken dp: *tam8 = ((d olarak tam64) + i) olarak *tam8;
        *dp = *sp;
        i = i + 1;
    }
    ver d;
}
çıplak işlev memset(d: *tam8, c: tam32, n: tam64) -> *tam8 {
    değişken cb: tam8 = c olarak tam8;             // düşük bayt
    değişken i: tam64 = 0;
    iken i < n {
        değişken dp: *tam8 = ((d olarak tam64) + i) olarak *tam8;
        *dp = cb;
        i = i + 1;
    }
    ver d;
}

// ============================================================================
// K2 (D-262): SAF-.kem çıplak DİZİ runtime — C kdl_dizi.inc YERİNE (kem_os yüzeyi:
// olustur/ekle_tam/al_tam/boyut/kapasite_ayarla + iç buyut/oob). KdlDizi düzeni:
//   { veri@0(ptr), boyut@8(i32), kapasite@12(i32), eleman_byte@16(i32) }  ayır 24.
// Bölge-sahipli (kdl_bolge_ayir[.kem] büyüme; realloc yok, eski bölgede sızar →
// bölge kapanışında topluca serbest). Sınır-kontrol (D-069) → oob spin-halt.
// AYNI dosyada kdl_bolge_ayir + memcpy (çıplak→çıplak resolve). tam64/ptr/yapi
// varyantları kem_os KULLANMADIĞI için atlandı (diğer kernel'ler C dizi'de).
// ============================================================================

// OOB (bellek-güvenliği ihlali) → temiz halt. kem_os'ta erişilmez (correct kod);
// cross-file panik/UART yok (leaf) → spin. Güvenlik: OOB asla belleği bozmaz.
çıplak işlev kdl_dizi_oob(i: tam32, boyut: tam32) {
    iken doğru { değişken x: tam32 = 0; }
}

// kdl_dizi_olustur(bolge, eleman_byte) -> KdlDizi* : boş dizi (veri=NULL).
çıplak işlev kdl_dizi_olustur(bolge: *tam8, eleman_byte: tam32) -> *tam8 {
    değişken d: *tam8 = kdl_bolge_ayir(bolge, 24);
    eğer (d olarak tam64) == 0 { ver 0 olarak *tam8; }
    değişken da: tam64 = d olarak tam64;
    değişken vp: *tam64 = da olarak *tam64;         *vp = 0;            // veri = NULL
    değişken bp: *tam32 = (da + 8) olarak *tam32;   *bp = 0;            // boyut
    değişken kp: *tam32 = (da + 12) olarak *tam32;  *kp = 0;            // kapasite
    değişken ep: *tam32 = (da + 16) olarak *tam32;  *ep = eleman_byte;  // eleman_byte
    ver d;
}

// İç: yeni_kap*eb kapasiteli tampon (bölgeden), canlı kopyala, d->veri/kapasite güncelle.
çıplak işlev kem_dizi_buyut(bolge: *tam8, d: *tam8, yeni_kap: tam32, eb: tam64) {
    değişken yeni: *tam8 = kdl_bolge_ayir(bolge, (yeni_kap olarak tam64) * eb);
    değişken da: tam64 = d olarak tam64;
    değişken vp: *tam64 = da olarak *tam64;
    değişken eski_veri: tam64 = *vp;
    değişken bp: *tam32 = (da + 8) olarak *tam32;
    değişken boyut: tam32 = *bp;
    eğer (yeni olarak tam64) != 0 {
        eğer eski_veri != 0 {
            eğer boyut > 0 {
                memcpy(yeni, eski_veri olarak *tam8, (boyut olarak tam64) * eb);
            }
        }
    }
    *vp = yeni olarak tam64;                         // d->veri = yeni
    değişken kp: *tam32 = (da + 12) olarak *tam32;
    *kp = yeni_kap;                                  // d->kapasite = yeni_kap
}

// kdl_dizi_ekle_tam(bolge, d, deger) : dolu ise büyüt, sonra ((i32*)veri)[boyut++]=deger.
çıplak işlev kdl_dizi_ekle_tam(bolge: *tam8, d: *tam8, deger: tam32) {
    eğer (d olarak tam64) == 0 { ver; }
    değişken da: tam64 = d olarak tam64;
    değişken bp: *tam32 = (da + 8) olarak *tam32;
    değişken boyut: tam32 = *bp;
    değişken kp: *tam32 = (da + 12) olarak *tam32;
    değişken kapasite: tam32 = *kp;
    eğer boyut == kapasite {
        değişken yk: tam32 = 4;
        eğer kapasite != 0 { yk = kapasite * 2; }
        kem_dizi_buyut(bolge, d, yk, 4);
    }
    değişken vp: *tam64 = da olarak *tam64;
    değişken veri: tam64 = *vp;                      // büyüme sonrası TAZE oku
    değişken elem: *tam32 = (veri + (boyut olarak tam64) * 4) olarak *tam32;
    *elem = deger;
    *bp = boyut + 1;                                 // boyut++
}

// kdl_dizi_al_tam(d, i) -> i32 : sınır-kontrollü oku (i<0 || i>=boyut → oob).
çıplak işlev kdl_dizi_al_tam(d: *tam8, i: tam32) -> tam32 {
    eğer (d olarak tam64) == 0 { ver 0; }
    değişken da: tam64 = d olarak tam64;
    değişken bp: *tam32 = (da + 8) olarak *tam32;
    değişken boyut: tam32 = *bp;
    eğer i < 0 { kdl_dizi_oob(i, boyut); }
    eğer i >= boyut { kdl_dizi_oob(i, boyut); }
    değişken vp: *tam64 = da olarak *tam64;
    değişken veri: tam64 = *vp;
    değişken elem: *tam32 = (veri + (i olarak tam64) * 4) olarak *tam32;
    ver *elem;
}

// [D-600] kdl_dizi_yaz_tam(d, i, deger) : sınır-kontrollü yerinde yaz
// (i<0 || i>=boyut → oob). C `kdl_dizi.inc` karşılığıyla BİREBİR; kem_os
// SHA-256 fazı `w[t] = yeni` yazdığı için gerekti (önceden kem_os'ta hiçbir
// yazma yolu yoktu → link `undefined symbol` ile GÜRÜLTÜLÜ düştü, S187).
çıplak işlev kdl_dizi_yaz_tam(d: *tam8, i: tam32, deger: tam32) {
    eğer (d olarak tam64) == 0 { ver; }
    değişken da: tam64 = d olarak tam64;
    değişken bp: *tam32 = (da + 8) olarak *tam32;
    değişken boyut: tam32 = *bp;
    eğer i < 0 { kdl_dizi_oob(i, boyut); }
    eğer i >= boyut { kdl_dizi_oob(i, boyut); }
    değişken vp: *tam64 = da olarak *tam64;
    değişken veri: tam64 = *vp;
    değişken elem: *tam32 = (veri + (i olarak tam64) * 4) olarak *tam32;
    *elem = deger;
}

// kdl_dizi_boyut(d) -> i32.
çıplak işlev kdl_dizi_boyut(d: *tam8) -> tam32 {
    eğer (d olarak tam64) == 0 { ver 0; }
    değişken da: tam64 = d olarak tam64;
    değişken bp: *tam32 = (da + 8) olarak *tam32;
    ver *bp;
}

// kdl_dizi_kapasite_ayarla(bolge, d, yeni_kapasite) : kapasiteyi (gerekiyorsa) büyüt.
çıplak işlev kdl_dizi_kapasite_ayarla(bolge: *tam8, d: *tam8, yeni_kapasite: tam32) {
    eğer (d olarak tam64) == 0 { ver; }
    değişken da: tam64 = d olarak tam64;
    değişken kp: *tam32 = (da + 12) olarak *tam32;
    değişken kapasite: tam32 = *kp;
    eğer yeni_kapasite <= kapasite { ver; }
    değişken ep: *tam32 = (da + 16) olarak *tam32;
    değişken eb: tam32 = *ep;
    eğer eb <= 0 { eb = 4; }
    kem_dizi_buyut(bolge, d, yeni_kapasite, eb olarak tam64);
}

// ============================================================================
// SUBSYSTEM/metin (D-264): SAF-.kem çıplak metin ilkeleri — C kdl_metin_bare.c
// YERİNE. Freestanding strlen/byte-at (allocation yok, leaf). Sınır-güvenli
// (NULL/OOB → 0 sentinel). @kdl_metin_uzunluk(ptr)->i32, @kdl_metin_bayt(ptr,i32)->i8.
// ============================================================================
çıplak işlev kdl_metin_uzunluk(s: *tam8) -> tam32 {
    eğer (s olarak tam64) == 0 { ver 0; }
    değişken n: tam32 = 0;
    iken doğru {
        değişken bp: *tam8 = ((s olarak tam64) + (n olarak tam64)) olarak *tam8;
        // D-265 sonrası: `*bp == 0` doğru i8-yük emit eder (deref-read pointee-tip fix).
        eğer *bp == 0 { ver n; }
        n = n + 1;
    }
    ver n;   // erişilmez
}
çıplak işlev kdl_metin_bayt(s: *tam8, i: tam32) -> tam8 {
    eğer (s olarak tam64) == 0 { ver 0 olarak tam8; }
    eğer i < 0 { ver 0 olarak tam8; }
    değişken n: tam32 = kdl_metin_uzunluk(s);
    eğer i >= n { ver 0 olarak tam8; }
    değişken bp: *tam8 = ((s olarak tam64) + (i olarak tam64)) olarak *tam8;
    ver *bp;
}

// ============================================================================
// SUBSYSTEM/mmio (D-266): SAF-.kem çıplak VOLATILE MMIO oku32/yaz32 — kem_os'un
// kullandığı widthler (donanım register erişimi). Çıplak = güvensiz-tier → deref
// VOLATILE emit (D-248; clang -O2 MMIO'yu elemez/yeniden-sıralamaz). D-265 pointee-tip:
// *p (p:*tam32) → i32-yük. Diğer widthler (16/64) virtio-dep, C'de kalır (kem_os.kem
// yalnız oku32/yaz32 çağırır). @kdl_mmio_oku32(i64)->i32, @kdl_mmio_yaz32(i64,i32).
çıplak işlev kdl_mmio_oku32(adres: tam64) -> tam32 {
    değişken p: *tam32 = adres olarak *tam32;
    ver *p;                       // volatile load i32
}
çıplak işlev kdl_mmio_yaz32(adres: tam64, deger: tam32) {
    değişken p: *tam32 = adres olarak *tam32;
    *p = deger;                   // volatile store i32
}

// ============================================================================
// SUBSYSTEM/yetki (D-268): SAF-.kem çıplak yetki<R> (object-capability) sağlayıcı.
// kdl_yetki_olustur/geri_al — kem_os'un tek kalan C-yetki bağımlılığını (kdl_yetki_bare.c)
// kaldırır (Yasa-4: kem_os'ta sıfır C). ABI = D-268 OUT-PTR: dönüş açık out-pointer
// (düz `ptr`, aarch64 x0 — sret DEĞİL). `%kdl_yetki` layout: {i64 id@0, i16 kaynak_tipi@8,
// i16 izin@10, i8 iptal@12, [3 x i8] rezerv@13} = 16 bayt. yetki DERLEME-ZAMANI ispatıdır
// (WCET sıfır runtime yükü); runtime değeri ABI-şeffaf (mmio_oku32/yaz32 yalnız adresi
// kullanır). Alanları offset-tabanlı yaz (kem_heap.kem tüm-ham-pointer idiomu; yapı yok).
küresel değişken kem_yetki_sayac: tam64 = 1;   // id sayacı (0 değil yeter — C sürümü aynısı)

çıplak işlev kdl_yetki_olustur(out: *tam8, kt: dtam16, izin: dtam16) {
    değişken taban: tam64 = out olarak tam64;
    değişken pid: *tam64 = taban olarak *tam64;              // id @ 0
    *pid = kem_yetki_sayac;
    kem_yetki_sayac = kem_yetki_sayac + 1;
    değişken pkt: *dtam16 = (taban + 8) olarak *dtam16;      // kaynak_tipi @ 8
    *pkt = kt;
    değişken piz: *dtam16 = (taban + 10) olarak *dtam16;     // izin @ 10
    *piz = izin;
    değişken pip: *dtam8 = (taban + 12) olarak *dtam8;       // iptal @ 12
    *pip = 0 olarak dtam8;
}

çıplak işlev kdl_yetki_geri_al(y: *tam8) {
    eğer (y olarak tam64) != 0 {                             // R-PAYLAŞ: iptal bayrağını kur
        değişken pip: *dtam8 = ((y olarak tam64) + 12) olarak *dtam8;
        *pip = 1 olarak dtam8;
    }
}

// ============================================================================
// SUBSYSTEM/kanal (D-623): SAF-.kem SPSC kanal — dilin `kanal_oluştur/gönder/al`
// ilkellerinin ÇEKİRDEK karşılığı. D-592'den beri kem_os yalnız boot .S + SAF-.kem
// bağlar; C `runtime/kdl_kanal.c` LİNKLENMİYORDU → `kanal_*` çağıran her kem_os kodu
// tanımsız sembolle düşerdi (ABI yalnız D-527'de DERLENİYORDU, hiç ÇALIŞMIYORDU).
// İmza host + C bare sürümüyle BİREBİR: @kdl_kanal_olustur(i32)->ptr,
// @kdl_kanal_gonder(ptr,i64), @kdl_kanal_al(ptr)->i64 (D-295: taşıyıcı i64).
//
// Yerleşim (hep 8-bayt hücre): [0]=bas (okuma) [1]=son (yazma) [2]=N (halka boyu)
// [3..3+N) = tampon. Klasik halka: bir slot rezerve → en çok N-1 öğe; dolu/boş
// ayrımı indeks eşitliğiyle. C sürümünden FARK (bilinçli): halka SABİT KAP=4 değil,
// istenen kapasite+1 — C sürümü büyük kapasiteyi REDDEDİYORDU, burada heap var.
// kapasite <= 0 → C ile aynı varsayılan (3 öğe).
//
// Bloklama = busy-wait (C sürümüyle aynı). Çıplak deref VOLATILE (D-248) → indeks
// okumaları döngüde elenmez. ⚠ Tek görevli bağlamda dolu kanala gönder / boş
// kanaldan al KALICI BEKLER (karşı taraf yok) — liveness, karşı görevin varlığına
// bağlıdır (D-296'nın dersi); çok-görevli kanal ayrı adım.
// ============================================================================
// Halka indeksini bir ilerlet. `%` BİLEREK YOK: değişken bölenli `%` sıfıra-bölme
// kontrolü (D-502) → @kdl_panik ister ve kem_os'ta o sembol YOK (D-592). n >= 2 daima.
çıplak işlev kem_kanal_ileri(i: tam64, n: tam64) -> tam64 {
    eğer i + 1 >= n { ver 0; }
    ver i + 1;
}

// Tampon hücresinin adresi — SINIR KONTROLLÜ (D-069 dizi sınırıyla aynı politika).
// Doğru halkada i < n daima; bozuk bir sarma (ya da bozuk tampon) indeksi n'e
// taşırsa yazma/okuma tamponun DIŞINA düşerdi = heap taşması. ÖLÇÜLDÜ: bu taşma
// FIFO sırasını bozmadığı ve malloc her kanala ayrı 64 KB blok verdiği için HİÇBİR
// davranışsal ölçümde görünmez → sessiz kalmasın diye burada gürültülü durdurulur.
çıplak işlev kem_kanal_hucre(taban: tam64, i: tam64, n: tam64) -> *tam64 {
    eğer i < 0 { kem_panik("PANIK: kanal indeks sinir ihlali"); }
    eğer i >= n { kem_panik("PANIK: kanal indeks sinir ihlali"); }
    ver (taban + 24 + i * 8) olarak *tam64;
}

// Çekirdek paniği: konsol artık mimari başına kem_uart_bayt
// (runtime/mimari/<arch>/kem_uart.kem). arm64'te yine PL011 MMIO; x86'da COM1
// 16550 port I/O. kdl_panik kem_os'ta YOK (D-592); çıplak bağlamda yalnız
// çıplak çağrılabilir (E013) → kem_uart_bayt da çıplaktır. Kanal (D-623) ve görev (D-624)
// ortak kullanır.
çıplak işlev kem_panik(m: metin) {
    değişken n: tam32 = metin_uzunluk(m);
    değişken i: tam32 = 0;
    iken i < n {
        kem_uart_bayt(metin_bayt(m, i) olarak tam32);
        i = i + 1;
    }
    kem_uart_bayt(10);
    iken doğru { }
}

çıplak işlev kdl_kanal_olustur(kapasite: tam32) -> *tam8 {
    değişken n: tam64 = 4;                                   // varsayılan: 3 öğe
    eğer kapasite > 0 { n = (kapasite olarak tam64) + 1; }
    değişken p: *tam8 = malloc((n + 3) * 8);
    değişken taban: tam64 = p olarak tam64;
    eğer taban == 0 { ver p; }                               // OOM → null (C ile aynı)
    değişken pbas: *tam64 = taban olarak *tam64;
    *pbas = 0;
    değişken pson: *tam64 = (taban + 8) olarak *tam64;
    *pson = 0;
    değişken pn: *tam64 = (taban + 16) olarak *tam64;
    *pn = n;
    ver p;
}

çıplak işlev kdl_kanal_gonder(k: *tam8, deger: tam64) {
    değişken taban: tam64 = k olarak tam64;
    değişken pbas: *tam64 = taban olarak *tam64;
    değişken pson: *tam64 = (taban + 8) olarak *tam64;
    değişken pn: *tam64 = (taban + 16) olarak *tam64;
    değişken n: tam64 = *pn;
    iken kem_kanal_ileri(*pson, n) == *pbas { }              // dolu → bekle
    değişken son: tam64 = *pson;
    değişken hucre: *tam64 = kem_kanal_hucre(taban, son, n);
    *hucre = deger;
    *pson = kem_kanal_ileri(son, n);
}

çıplak işlev kdl_kanal_al(k: *tam8) -> tam64 {
    değişken taban: tam64 = k olarak tam64;
    değişken pbas: *tam64 = taban olarak *tam64;
    değişken pson: *tam64 = (taban + 8) olarak *tam64;
    değişken pn: *tam64 = (taban + 16) olarak *tam64;
    değişken n: tam64 = *pn;
    iken *pson == *pbas { }                                  // boş → bekle
    değişken bas: tam64 = *pbas;
    değişken hucre: *tam64 = kem_kanal_hucre(taban, bas, n);
    değişken v: tam64 = *hucre;
    *pbas = kem_kanal_ileri(bas, n);
    ver v;
}

// D-543 kanal ömrü: codegen hapsedilme kanıtı olan kanalı işlev çıkışında serbest
// bırakır. Tampon tek malloc bloğu → tek free. null güvenli (free null'ı yoksayar).
çıplak işlev kdl_kanal_serbest(k: *tam8) {
    free(k);
}

```

# İŞE ÖZEL BAĞLAM

## runtime/mimari/arm64/kem_zaman_hw.kem
```
// kem_zaman_hw.kem — arm64 ZAMANLAYICI/KESME DONANIMI (D2b, D-683).
// ============================================================================
// runtime/kem_zaman.kem'den BİREBİR taşındı (gövdeler değişmedi): GIC
// (distributor/CPU interface MMIO), ARM generic timer (CNTV, msr/mrs) ve DAIF
// asm'leri + IRQ dağıtımı (kdl_irq_isle, .S kdl_irq_ortak çağırır; preempt için
// kem_gorev'deki kem_preempt'e devreder). Mimariden bağımsız kısım (tik sayacı,
// kem_timer_testi, kz_mmio yardımcıları) kem_zaman.kem'de kaldı.
// x86 ikizi: runtime/mimari/x86_64/kem_zaman_hw.kem (PIC + PIT, port I/O).

sabit KZ_GICD: tam64 = 134217728;    // 0x08000000 distributor

sabit KZ_GICC: tam64 = 134283264;    // 0x08010000 CPU interface

sabit KZ_TIMER_IRQ: tam32 = 27;      // sanal timer PPI (INTID 27)

// CNTV / DAIF — satıriçi_asm arm64 (msr/mrs, D-269 P1).
çıplak işlev kz_cntfrq() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz { satıriçi_asm { mimari: arm64 şablon: r#"mrs $0, cntfrq_el0"# çıktı("=r", &x) } }
    ver x;
}

çıplak işlev kz_cntv_tval(v: tam64) {
    güvensiz { satıriçi_asm { mimari: arm64 şablon: r#"msr cntv_tval_el0, $0"# girdi("r", v) } }
}

çıplak işlev kz_cntv_ctl(v: tam64) {
    güvensiz { satıriçi_asm { mimari: arm64 şablon: r#"msr cntv_ctl_el0, $0"# girdi("r", v) } }
}

çıplak işlev kz_irq_ac() {
    güvensiz { satıriçi_asm { mimari: arm64 şablon: r#"msr daifclr, #2"# bozulan("~{memory}") } }
}

// tik'i TAZE oku (IRQ-context'te yazılıyor → cache'lenmesin): inline-asm ldr @kem_tik.
çıplak işlev kem_tik_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: arm64
            şablon: r#"adrp x9, kem_tik
add x9, x9, :lo12:kem_tik
ldr $0, [x9]"#
            çıktı("=r", &x)
            bozulan("~{x9}") bozulan("~{memory}")
        }
    }
    ver x;
}

// GIC init (kdl_kesme_kur birebir). ISENABLER0 bit27 = 1<<27 = 134217728.
çıplak işlev kdl_kesme_kur() {
    kz_mmio_yaz(kmmu_gicd_taban() + 0, 1);              // GICD_CTLR = enable
    kz_mmio_yaz(kmmu_gicc_taban() + 4, 255);            // GICC_PMR = 0xFF (tüm öncelikler)
    kz_mmio_yaz(kmmu_gicc_taban() + 0, 1);              // GICC_CTLR = enable
    kz_mmio_yaz(kmmu_gicd_taban() + 256, 134217728);    // ISENABLER0 bit27
}

// Timer start (kdl_timer_baslat birebir): interval ~10ms, enable, daifclr.
çıplak işlev kdl_timer_baslat() {
    değişken f: tam64 = kz_cntfrq();
    kem_timer_aralik = f / 100;               // ~10ms
    kz_cntv_tval(kem_timer_aralik);
    kz_cntv_ctl(1);                           // enable + unmask
    kz_irq_ac();                              // daifclr #2 → IRQ aç
}

// IRQ dispatch — .S kdl_irq_ortak `bl kdl_irq_isle` (sp = trap-frame ptr). timer →
// re-arm + tik++; EOI. A3: preemption YOK → sp OLDUĞU GİBİ döner (aynı bağlam sürer).
çıplak işlev kdl_irq_isle(sp: tam64) -> tam64 {
    değişken iar: tam32 = kz_mmio_oku(kmmu_gicc_taban() + 12);       // GICC_IAR
    değişken irq: tam32 = iar - ((iar / 1024) * 1024);     // iar & 0x3FF
    eğer irq == KZ_TIMER_IRQ {
        kz_cntv_tval(kem_timer_aralik);                    // re-arm (ISTATUS temizler)
        kem_tik = kem_tik + 1;
    }
    kz_mmio_yaz(kmmu_gicc_taban() + 16, iar);                        // GICC_EOIR (kesme bitti)
    ver kem_preempt(sp);                                   // A4: scheduler (aktif değilse sp döner)
}

```

## runtime/kem_zaman.kem
```
// kem_zaman.kem — FAZ-A2/A3 (D-279): SAF-.kem timer + GIC + IRQ dispatch (zaman alt-sistemi).
// ============================================================================
// Kanıtlı-C kdl_zaman.c'nin BİREBİR .kem yeniden gerçekleştirmesi. GERÇEK timer-IRQ:
// CNTV (sanal generic timer) + GICv2 (QEMU virt) → periyodik IRQ → .S kdl_irq_ortak
// `bl kdl_irq_isle` → .kem handler tik++ + re-arm + EOI. Preemption YOK (A3; A4'te gelir).
//
// SAF-.kem primitifleri: GIC = MMIO çıplak-deref; CNTV/DAIF = satıriçi_asm arm64 (msr/mrs).
// [D-683] Mimariye özgü donanım (GIC/CNTV/DAIF/IRQ dağıtımı) runtime/mimari/<arch>/
// kem_zaman_hw.kem'e taşındı; burada tik sayacı + kem_timer_testi + kz_mmio kaldı.
//   .S kdl_irq_ortak (trap-frame + eret) DEĞİŞMEZ; yalnız `bl kdl_irq_isle` .kem'i çözer
//   (C kdl_zaman.c guard'lı, bm_a64_zaman_kem.o variant).
// GIC MMIO (0x08000000/0x08010000) = Device — kdl_mmu L1[0] Device blok haritalı (kem_mmu.kem).

// tik + aralık: YALNIZ .kem okur/yazar (.S paylaşımı YOK) → küresel internal-linkage yeterli.
küresel değişken kem_tik: tam64 = 0;
küresel değişken kem_timer_aralik: tam64 = 0;

çıplak işlev kz_mmio_yaz(adr: tam64, deger: tam32) { değişken p: *tam32 = adr olarak *tam32; *p = deger; }
çıplak işlev kz_mmio_oku(adr: tam64) -> tam32 { değişken p: *tam32 = adr olarak *tam32; ver *p; }

// A3 gate: timer'ı kur + IRQ'ların GERÇEKTEN tik ilerlettiğini doğrula. tik yalnız
// IRQ handler'da artar → tik>=hedef = gerçek periyodik IRQ tetiklendi. guard: timer
// bozuksa (hiç IRQ yok) sonsuz-döngü YOK → HATA raporla (0 döner).
çıplak işlev kem_timer_testi() -> tam32 {
    kdl_kesme_kur();
    kdl_timer_baslat();
    değişken guard: tam64 = 0;
    iken (kem_tik_oku() < 3) ve (guard < 50000000) {       // ~3 tik (30ms) bekle, guard'lı
        guard = guard + 1;
    }
    eğer kem_tik_oku() >= 3 {
        ver 1;                                             // GERÇEK IRQ tik ilerledi
    }
    ver 0;                                                 // timer tetiklemedi (guard doldu)
}

```

## runtime/mimari/x86_64/kem_uart.kem
```
// kem_uart.kem — İş 2 (D-672, D2b): x86_64 (COM1 16550) UART bayt-yaz arayüzü.
// kem_panik artık mimariden bağımsız kem_uart_bayt çağırır; bu dosya x86'da
// COM1 16550 port I/O yolunu sağlar: veri 0x3F8 (=1016), LSR 0x3FD (=1021),
// LSR bit 5 (32) = THRE (verici tutma yazmacı boş). arm64 ikizi:
// runtime/mimari/arm64/kem_uart.kem (PL011 MMIO). Ayrı dosya: AS001 gereği bir
// dosyada yalnız TEK mimarinin satıriçi_asm'i olabilir; Makefile CAT listesi
// mimariye göre seçer (D2b kararı). LSR yoklaması SINIRLI: UART yoksa (ya da
// THRE donmuşsa) panik yolu ASILMASIN diye en çok 100000 deneme; sonra veri
// yine yazılır (mesaj kaybolabilir ama çekirdek ilerler).
çıplak işlev kem_port_yaz8(port: dtam16, deger: dtam8) {
    güvensiz {
        satıriçi_asm { mimari: x86_64 şablon: r#"outb $1, $0"# girdi("{dx}", port) girdi("{al}", deger) bozulan("~{memory}") }
    }
}

çıplak işlev kem_port_oku8(port: dtam16) -> dtam8 {
    değişken v: dtam8 = 0 olarak dtam8;
    güvensiz {
        satıriçi_asm { mimari: x86_64 şablon: r#"inb $1, $0"# çıktı("={al}", &v) girdi("{dx}", port) bozulan("~{memory}") }
    }
    ver v;
}

çıplak işlev kem_uart_bayt(b: tam32) {
    değişken deneme: tam32 = 0;
    iken deneme < 100000 {
        değişken lsr: tam32 = kem_port_oku8(1021) olarak tam32;
        // THRE set → yoklamayı BİTİR (döngüden çık), yazma AŞAĞIDA yapılır.
        // ⚠ Burada `ver;` YAZMA: işlev yazmadan döner, mesaj SESSİZCE kaybolur
        // (tur 3 üretiminde öyleydi; calistir_kem_os_x86 Kanıt 5 yakaladı).
        eğer (lsr & 32) != 0 { deneme = 100000; }
        değilse { deneme = deneme + 1; }
    }
    kem_port_yaz8(1016, b olarak dtam8);
}

```

## runtime/kdl_zaman.c — KESİT (satır 121-155; düzenleme hedefi: `runtime/kdl_zaman.c`, blok yolu `runtime/kdl_zaman.c` olmalı)
```

/* ================= x86_64 (PIC 8259 + PIT 8254) ================= */
#elif defined(__x86_64__)

static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

void kdl_kesme_kur(void) {
    /* PIC 8259 remap: IRQ0-7 → vektör 32-39, IRQ8-15 → 40-47. */
    outb(0x20, 0x11); outb(0xA0, 0x11);              /* ICW1: init + ICW4 bekle */
    outb(0x21, 0x20); outb(0xA1, 0x28);              /* ICW2: vektör offset 32/40 */
    outb(0x21, 0x04); outb(0xA1, 0x02);              /* ICW3: cascade (slave@IRQ2) */
    outb(0x21, 0x01); outb(0xA1, 0x01);              /* ICW4: 8086 mode */
    outb(0x21, 0xFE); outb(0xA1, 0xFF);              /* mask: yalnız IRQ0 açık */
}

void kdl_timer_baslat(void) {
    /* PIT 8254 ch0: mode 3 (kare dalga), lo/hi, binary = 0x36. */
    outb(0x43, 0x36);
    uint16_t bolen = 11932;                          /* 1193182/11932 ≈ 100 Hz */
    outb(0x40, (uint8_t)(bolen & 0xFF));
    outb(0x40, (uint8_t)((bolen >> 8) & 0xFF));
    __asm__ volatile("sti");                         /* IRQ aç */
}

/* IRQ dispatch — boot kdl_irq0_stub'tan (irq = 0 timer). PIT mode 3 auto-reload. */
void kdl_kesme_isle(uint64_t irq) {
    (void)irq;                                       /* yalnız IRQ0 açık */
    kdl_tik();
}

void kdl_idle(void) { __asm__ volatile("hlt"); }

#endif
```

## Makefile — KESİT (satır 1956-1961; düzenleme hedefi: `Makefile`, blok yolu `Makefile` olmalı)
```
#          satır yedek sabitten (32768 KB) BÜYÜK — PVH memmap RAM tepesinden
#          türetilen DMA tabanı (kem_platform.kem). Değer de yazdırılır.
# Kanıt 14: kem_dtb x86 biriminde derlenir (mimariden bağımsız ayrıştırıcı);
#          kdtb_adres (mimari dosya) 0 döner → "X86 DTB YOK pvh_kesif" satırı
#          basılır ve objdump'da <kdtb_adres>: etiketi x86 biriminde bulunur.
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
```

## Makefile — KESİT (satır 2071-2077; düzenleme hedefi: `Makefile`, blok yolu `Makefile` olmalı)
```
	@echo "  Kanit 14: kem_dtb x86 biriminde derleniyor + kdtb_adres etiketi ..."
	@if ! grep -qxF "X86 DTB YOK pvh_kesif" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 DTB YOK pvh_kesif' satiri yok"; exit 1; fi
	@if ! llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep -q "<kdtb_adres>:"; then \
		echo "FAIL: <kdtb_adres>: etiketi x86 biriminde yok"; exit 1; fi

# === Bare-Metal Dizi Kernel x86_64 (C1-x86) — AYNI kernel, PVH boot ===
```

## test/ornekler/kem_os_x86.kem — KESİT (satır 1-30; düzenleme hedefi: `test/ornekler/kem_os_x86.kem`, blok yolu `test/ornekler/kem_os_x86.kem` olmalı)
```
// kem_os_x86.kem — İş 5: x86_64 kanıt çekirdeğinin `main`'i. kem_heap.kem ile
// CAT edilir (tek derleme birimi). Normal (çıplak OLMAYAN) işlev → prologu
// bölgeyi kdl_global_bolge_al → kdl_bolge_olustur → @malloc (saf-.kem) zinciriyle
// kurar; bu test TAM O ZİNCİRİ ölçer: start_x86_64.S kem_heap_kur'u main'den
// önce çağırmazsa saf-.kem malloc null döner ve toplam=0 olur (panik YOK —
// bu yüzden marker değil DEĞER yazdırılıp denetlenir, D-264 dersi).
//
// Amaç: bölge + dizi + metin yüzeyini x86 QEMU'da DAVRANIŞSAL kanıtla. Yazdırma
// yerleşikleri (yazdir_metin/yazdir_tam/yazdir_satir) bu çekirdekte C
// runtime/kdl_runtime_yazdir_bare.c + 16550 UART üzerinden gider (İş 2 sonra
// .kem UART'a çevirecek — şimdilik dokunulmaz).
//
// İş 9 (PVH): kem_panik öncesi RAM toplamı kem_pvh.kem'den (start_x86_64.S %ebx'i
// .data'daki kdl_pvh_bilgi'ye saklar). "X86 PVH OK ram_mb=" + pozitif satır;
// ram ≤ 0 → "X86 PVH HATA" + kod.
//
// Beklenen başarı çıktısı: "X86 HEAP DIZI OK toplam=" satırı + "55" satırı
// (yazdir_metin satır sonu ekler).
// Ayrıca main başında kem_dma_bariyeri() çağrılır ve "X86 DMA BARIYER OK"
// satırı basılır (gerçekten çalıştığının kanıtı; #UD olsaydı çekirdek düşerdi).
// main sonunda kem_panik("X86 PANIK YOLU OK") çağrılır: panik geri dönmez,
// mesaj .kem COM1 yolundan basılır, çekirdek orada durur; QEMU zaman aşımı keser.
//
// İş 3: kem_panik'ten ÖNCE üç DMA/MMIO testi koşar (her biri DEĞER denetler,
// sabit metin basmaz — D-264 dersi):
//   (a) Blok DMA RAM — vblk_data_yaz/oku 0..511 yaz-oku karşılaştırma →
//       "X86 DMA RAM BLK OK" (aksi hâlde "X86 DMA RAM BLK HATA" + ilk indeks).
//   (b) Ağ DMA RAM — vnet_tx_yaz + çıplak x86_net_tx_oku (aşağıda) 0..63
//       karşılaştırma → "X86 DMA RAM NET OK".
//   (c) MMIO UC pencere — kdl_mmio_oku32(0xFEE00030) LAPIC sürüm yazmacı
```

# GÖREV

## İş 14 — x86 zamanlayıcı/kesme donanımı `.kem`'de (kdl_zaman.c x86 portu)

`runtime/kem_zaman.kem` artık mimariden bağımsız: `kem_tik`, `kem_timer_aralik`
küreselleri, `kz_mmio_yaz/oku`, `kem_timer_testi()` (kdl_kesme_kur →
kdl_timer_baslat → kem_tik_oku() >= 3 bekler, guard'lı). arm64 donanımı
`runtime/mimari/arm64/kem_zaman_hw.kem`'de (bağlamda, ÖRNEK desen). x86
karşılığı bugün C'de (`runtime/kdl_zaman.c` x86 bölümü, bağlamda) — x86 kanıt
çekirdeğinin bağlantısından C nesnesi ÇIKARILDI (orkestratör yaptı), bu yüzden
aşağıdaki semboller `.kem`'den gelmek ZORUNDA (`bm_x86_kesme.o` `kdl_tik_al`'ı
çağırır; `start_x86_64.S` IRQ0 koçanı `kdl_kesme_isle(irq)`'yi çağırır ve
PIC EOI'yi KENDİSİ yapar).
DOĞRULANMIŞ SABİTLER (ondalık): PIC1 komut 32, PIC2 komut 160, PIC1 veri 33,
PIC2 veri 161; ICW1 17; ICW2 32 (PIC1) / 40 (PIC2); ICW3 4 / 2; ICW4 1;
maske 254 (PIC1, yalnız IRQ0) / 255 (PIC2). PIT komut portu 67, kanal 0 portu
64, mod baytı 54; bölen 11932 → düşük bayt 156, yüksek bayt 46 (~100 Hz).

### Değişiklikler (yalnız bunlar — 3 madde)
1. **YENİ `runtime/mimari/x86_64/kem_zaman_hw.kem`** — Türkçe başlık (D2b,
   kdl_zaman.c x86 portu, arm64 ikizi). Çıplak işlevler (C ile AYNI sıra ve
   değerler; port yazımı `kem_port_yaz8(port olarak dtam16, değer olarak dtam8)` —
   `runtime/mimari/x86_64/kem_uart.kem`'de, aynı CAT biriminde):
   - `kdl_kesme_kur()` — PIC remap 10 port yazımı (C'deki sırayla).
   - `kdl_timer_baslat()` — PIT: 67←54, 64←156, 64←46; sonra `kz_irq_ac()`.
   - `kz_irq_ac()` — `satıriçi_asm { mimari: x86_64 şablon: r#"sti"# bozulan("~{memory}") }`.
   - `kdl_kesme_isle(irq: tam64)` — `kem_tik = kem_tik + 1;` (yalnız IRQ0 açık).
   - `kdl_tik_al() -> tam64` — `ver kem_tik_oku();`
   - `kem_tik_oku() -> tam64` — IRQ'da yazılan sayacı TAZE oku:
     `satıriçi_asm { mimari: x86_64 şablon: r#"movq kem_tik(%rip), $0"# çıktı("=r", &x) bozulan("~{memory}") }`.
   - `kdl_idle()` — `hlt` asm (C ile parite).
   asm'ler `güvensiz { }` içinde.
2. **`test/ornekler/kem_os_x86.kem`** — `kem_panik("X86 PANIK YOLU OK");` ÖNCESİNE:
   `değişken t_ok: tam32 = kem_timer_testi();` 1 ise `X86 TIMER TIK OK tik=` +
   ayrı satırda `kem_tik_oku() olarak tam32` (sayıdan sonra `yazdir_satir()`;
   `yazdir_metin` zaten satır sonu ekler — araya `yazdir_satir` KOYMA); değilse
   `X86 TIMER HATA`. Başlık yorumuna ekle (gerçek IRQ0: PIT → PIC → IDT vektör
   32 → .S koçanı → .kem kdl_kesme_isle).
   ÇAPA: `    kem_panik("X86 PANIK YOLU OK");`
3. **`Makefile` Kanıt 15** — Kanıt 14'ün son satırından SONRA (çapa, iki TAB):
   `		echo "FAIL: <kdtb_adres>: etiketi x86 biriminde yok"; exit 1; fi`
   - `X86 TIMER TIK OK tik=` TAM satırı + ardından gelen satır `v` ile
     `[ "$$v" -ge 3 ]` (gerçek IRQ ilerletti); yoksa FAIL; değeri yazdır.
   - `llvm-nm $(BUILD)/kem_os_x86.o` içinde ` T kdl_kesme_isle` ve ` T kdl_tik_al`
     OLMALI (C değil .kem sağlıyor); yoksa FAIL.
   - Hedef yorumuna Kanıt 15 (çapa: `#          basılır ve objdump'da <kdtb_adres>: etiketi x86 biriminde bulunur.`).

Blok yolları yalnız: `runtime/mimari/x86_64/kem_zaman_hw.kem`,
`test/ornekler/kem_os_x86.kem`, `Makefile`.

````
