# Tur 23 — turbofish_parser — istem

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

## src/ast.h — KESİT (satır 427-432; düzenleme hedefi: `src/ast.h`, blok yolu `src/ast.h` olmalı)
```
        struct {
            Dugum *hedef;          /* cagrilan ifade */
            Dugum **argumanlar;
            int sayi;
        } cagri;

```

## src/ifade.c — KESİT (satır 500-575; düzenleme hedefi: `src/ifade.c`, blok yolu `src/ifade.c` olmalı)
```
    }
}

/* === Sonek operatorler (cagri, erisim, indeks, yol) === */

static Dugum *parse_sonek_op(Parser *p, Dugum *sol) {
    Token t = parser_simdiki(p);

    switch (t.tip) {
        case TOK_NOKTA: {
            parser_ilerle(p);
            Token alan_tok = parser_bekle(p, TOK_TANIMLAYICI, "P120",
                                           "alan adi bekleniyor");
            Dugum *yeni = yapi_dugum_olustur(p, DUGUM_ERISIM,
                                             t.satir, t.sutun);
            if (yeni) {
                yeni->veri.erisim.nesne = sol;
                yeni->veri.erisim.alan =
                    ast_string_kopyala(p->arena,
                                       alan_tok.baslangic, alan_tok.uzunluk);
                yeni->veri.erisim.alan_uzunluk = alan_tok.uzunluk;
            }
            return yeni;
        }

        case TOK_SOL_KOSELI: {
            parser_ilerle(p);
            Dugum *idx = parse_ifade(p);
            parser_bekle(p, TOK_SAG_KOSELI, "P121", "']' bekleniyor");
            Dugum *yeni = yapi_dugum_olustur(p, DUGUM_INDEKS,
                                             t.satir, t.sutun);
            if (yeni) {
                yeni->veri.indeks.nesne = sol;
                yeni->veri.indeks.indeks = idx;
            }
            return yeni;
        }

        case TOK_SOL_PAREN: {
            parser_ilerle(p);
            Liste args;
            liste_baslat(&args);
            if (!parser_eslesir(p, TOK_SAG_PAREN)) {
                do {
                    if (parser_eslesir(p, TOK_SAG_PAREN)) break;
                    Dugum *arg = parse_ifade(p);
                    liste_ekle(&args, p->arena, arg);
                } while (parser_tuket(p, TOK_VIRGUL));
            }
            parser_bekle(p, TOK_SAG_PAREN, "P122",
                         "cagri argumanlari icin ')' bekleniyor");
            Dugum *yeni = yapi_dugum_olustur(p, DUGUM_CAGRI,
                                             t.satir, t.sutun);
            if (yeni) {
                yeni->veri.cagri.hedef = sol;
                yeni->veri.cagri.argumanlar = liste_array_yap(&args, p->arena);
                yeni->veri.cagri.sayi = args.sayi;
            }
            return yeni;
        }

        case TOK_CIFT_IKI_NOKTA: {
            parser_ilerle(p);
            Token sag_tok = parser_bekle(p, TOK_TANIMLAYICI, "P123",
                                          "yol devami bekleniyor");
            Dugum *yeni = yapi_dugum_olustur(p, DUGUM_YOL, t.satir, t.sutun);
            if (yeni) {
                yeni->veri.yol.sol = sol;
                yeni->veri.yol.sag_ad =
                    ast_string_kopyala(p->arena,
                                       sag_tok.baslangic, sag_tok.uzunluk);
                yeni->veri.yol.sag_ad_uzunluk = sag_tok.uzunluk;
            }
            return yeni;
        }

```

## src/ast_yazdir.c — KESİT (satır 112-125; düzenleme hedefi: `src/ast_yazdir.c`, blok yolu `src/ast_yazdir.c` olmalı)
```
            break;

        case DUGUM_CAGRI:
            konum_yaz(c, d);
            fputc('\n', c);
            indent_yaz(c, derinlik + 1);
            fputs("hedef:\n", c);
            ast_yazdir_indent(d->veri.cagri.hedef, c, derinlik + 2);
            indent_yaz(c, derinlik + 1);
            fprintf(c, "argumanlar (%d):\n", d->veri.cagri.sayi);
            cocuk_listesi_yaz(c, d->veri.cagri.argumanlar,
                              d->veri.cagri.sayi, derinlik + 1);
            break;

```

## src/ast_yazdir.c — KESİT (satır 598-605; düzenleme hedefi: `src/ast_yazdir.c`, blok yolu `src/ast_yazdir.c` olmalı)
```
        case DUGUM_TEKLI:
            ast_duz_yaz(d->veri.tekli.operand, c, dd); break;
        case DUGUM_CAGRI:
            ast_duz_yaz(d->veri.cagri.hedef, c, dd);
            for (int i = 0; i < d->veri.cagri.sayi; i++)
                ast_duz_yaz(d->veri.cagri.argumanlar[i], c, dd);
            break;
        case DUGUM_ERISIM:
```

## selfhost/parser.kem — KESİT (satır 30-37; düzenleme hedefi: `selfhost/parser.kem`, blok yolu `selfhost/parser.kem` olmalı)
```
    a_cb: Dizi<tam32>;    // çocuk listesi başlangıç indeksi
    a_cs: Dizi<tam32>;    // çocuk sayısı
    cocuk: Dizi<tam32>;   // düz çocuk indeks listesi
    hata_say: tam32;     // parser hata sayısı
    yapi_izni: tam32;    // 1=Tip{..} yapı-oluştur izinli; 0=koşul bağlamı (blok başı)
    sk_pending: tam32;   // >> (SAGA_KAYDIR) yarı-tüketildi (generic kapanış böl)
}

```

## selfhost/parser.kem — KESİT (satır 1028-1070; düzenleme hedefi: `selfhost/parser.kem`, blok yolu `selfhost/parser.kem` olmalı)
```
}

// Sonek operatör (. [] () :: olarak). Pozisyon = sonek token'ı.
işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {
    değişken t: metin = simdiki(p);
    değişken s: tam32 = sim_sat(p);  değişken u: tam32 = sim_sut(p);
    eğer metin_esit(t, "NOKTA") {
        ilerle(p);
        değişken alan: metin = sim_lex(p);
        bekle(p, "TANIMLAYICI");
        ver dugum1(p, "ERISIM", alan, s, u, sol);
    }
    eğer metin_esit(t, "SOL_KOSELI") {
        ilerle(p);
        değişken idx: tam32 = parse_ifade(p);
        bekle(p, "SAG_KOSELI");
        ver dugum2(p, "INDEKS", "", s, u, sol, idx);
    }
    eğer metin_esit(t, "SOL_PAREN") {
        ilerle(p);
        değişken kids: Dizi<tam32> = [];
        dizi_ekle(kids, sol);  // çocuk[0] = hedef
        eğer sim_mi(p, "SAG_PAREN") == yanlış {
            değişken devam: mantıksal = doğru;
            iken devam {
                eğer sim_mi(p, "SAG_PAREN") { devam = yanlış; }
                değilse {
                    dizi_ekle(kids, parse_ifade(p));
                    eğer sim_mi(p, "VIRGUL") { ilerle(p); }
                    değilse { devam = yanlış; }
                }
            }
        }
        bekle(p, "SAG_PAREN");
        ver dugum_n(p, "CAGRI", "", s, u, kids);
    }
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        değişken sag: metin = sim_lex(p);
        bekle(p, "TANIMLAYICI");
        ver dugum1(p, "YOL", sag, s, u, sol);
    }
    eğer metin_esit(t, "OLARAK") {
```

## selfhost/parser.kem — KESİT (satır 1598-1628; düzenleme hedefi: `selfhost/parser.kem`, blok yolu `selfhost/parser.kem` olmalı)
```

// ---- Düz --ast dumper (preorder; C ast_duz_yaz ile birebir) ----
işlev duz_yaz(p: &değişken Ayr, idx: tam32, derinlik: tam32) -> tam32 {
    yaz_tam(derinlik);  yb(9);
    yaz_str(dizi_al(p.a_ad, idx));  yb(9);
    yaz_kacis(dizi_al(p.a_deg, idx));  yb(9);
    yaz_tam(dizi_al(p.a_sat, idx));  yb(9);
    yaz_tam(dizi_al(p.a_sut, idx));  yb(10);
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        i = i + 1;
    }
    ver 0;
}

işlev ayr_olustur(kaynak: metin) -> Ayr {
    ver Ayr {
        kaynak: kaynak,
        t_ad: [], t_sat: [], t_sut: [], t_off: [], t_uz: [],
        imlec: 0,
        a_ad: [], a_deg: [], a_sat: [], a_sut: [], a_cb: [], a_cs: [], cocuk: [],
        hata_say: 0,
        yapi_izni: 1,
        sk_pending: 0
    };
}

işlev main() -> tam32 {
```

## selfhost/checker.kem — KESİT (satır 176-184; düzenleme hedefi: `selfhost/checker.kem`, blok yolu `selfhost/checker.kem` olmalı)
```
    lin_basla: tam32;        // aktif işlevin lineer bağlama diliminin başı
    th_kod: Dizi<metin>;  // tip-hata kodları (traversal sırası)
    t043_node: Dizi<tam32>;  // D-630: T043 raporlanmış literal düğümleri (tekilleştirme)
    tasma_node: Dizi<tam32>;  // [D-632] 2^64+ (doymuş) TAM düğümleri — a_deg doymuş değeri taşır
    neg_lit: tam32;          // D-630: literal tekli eksinin operandı mı (alt sınır +1)
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
}

```

## selfhost/checker.kem — KESİT (satır 1321-1363; düzenleme hedefi: `selfhost/checker.kem`, blok yolu `selfhost/checker.kem` olmalı)
```
}

// Sonek operatör (. [] () :: olarak). Pozisyon = sonek token'ı.
işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {
    değişken t: metin = simdiki(p);
    değişken s: tam32 = sim_sat(p);  değişken u: tam32 = sim_sut(p);
    eğer metin_esit(t, "NOKTA") {
        ilerle(p);
        değişken alan: metin = sim_lex(p);
        bekle(p, "TANIMLAYICI");
        ver dugum1(p, "ERISIM", alan, s, u, sol);
    }
    eğer metin_esit(t, "SOL_KOSELI") {
        ilerle(p);
        değişken idx: tam32 = parse_ifade(p);
        bekle(p, "SAG_KOSELI");
        ver dugum2(p, "INDEKS", "", s, u, sol, idx);
    }
    eğer metin_esit(t, "SOL_PAREN") {
        ilerle(p);
        değişken kids: Dizi<tam32> = [];
        dizi_ekle(kids, sol);  // çocuk[0] = hedef
        eğer sim_mi(p, "SAG_PAREN") == yanlış {
            değişken devam: mantıksal = doğru;
            iken devam {
                eğer sim_mi(p, "SAG_PAREN") { devam = yanlış; }
                değilse {
                    dizi_ekle(kids, parse_ifade(p));
                    eğer sim_mi(p, "VIRGUL") { ilerle(p); }
                    değilse { devam = yanlış; }
                }
            }
        }
        bekle_k(p, "SAG_PAREN", "P122");   // [D-652] C ifade.c:549 cagri ")"
        ver dugum_n(p, "CAGRI", "", s, u, kids);
    }
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        değişken sag: metin = sim_lex(p);
        bekle(p, "TANIMLAYICI");
        ver dugum1(p, "YOL", sag, s, u, sol);
    }
    eğer metin_esit(t, "OLARAK") {
```

## selfhost/checker.kem — KESİT (satır 2032-2066; düzenleme hedefi: `selfhost/checker.kem`, blok yolu `selfhost/checker.kem` olmalı)
```

// ---- Düz --ast dumper (preorder; C ast_duz_yaz ile birebir) ----
işlev duz_yaz(p: &değişken Ayr, idx: tam32, derinlik: tam32) -> tam32 {
    yaz_tam(derinlik);  yb(9);
    yaz_str(dizi_al(p.a_ad, idx));  yb(9);
    yaz_kacis(dizi_al(p.a_deg, idx));  yb(9);
    yaz_tam(dizi_al(p.a_sat, idx));  yb(9);
    yaz_tam(dizi_al(p.a_sut, idx));  yb(10);
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        i = i + 1;
    }
    ver 0;
}

işlev ayr_olustur(kaynak: metin) -> Ayr {
    ver Ayr {
        kaynak: kaynak,
        t_ad: [], t_sat: [], t_sut: [], t_off: [], t_uz: [],
        imlec: 0,
        a_ad: [], a_deg: [], a_sat: [], a_sut: [], a_cb: [], a_cs: [], cocuk: [],
        hata_say: 0, ph_kod: [], ph_sat: [], ph_sut: [], sh_sat: 0, sh_sut: 0, sh_kod: "", sh_tek: 0, ph_kis: 0,
        yapi_izni: 1,
        sk_pending: 0,
        g_isim: [], yerel: [], yerel_tip: [], yerel_ptr: [], yerel_ham: [], yerel_dizi: [], yerel_tn: [], yerel_yig: [], yerel_fn: [], yerel_yet: [], yerel_yon: [], yerel_sab: [], drf_bek: "", dizi_bek: "?", cv_cesit: [], cv_ad: [], cv_pc: [], cv_pb: [], cv_pt: [], kur_ad: [], cip_node: [], rt_node: [], kd_ad: [], kd_durum: [], kd_aile: [], asm_cev: [], rt_yigin: [], rt_yigin_n: 0, rt_sessiz: 0, rt_dongu: 0, asm_node: [], asm_mim: [], si_ad: [], si_yol: [], si_sat: [], si_sut: [], al_ad: [], al_yol: [], mod_ad: [], gp_ad: [], gp_sahip: [], gp_idx: [], gp_bound: [], oz_ad: [], uyg_tip: [], uyg_oz: [], gen_node: [], priv_mod: [], priv_ad: [], yerel_basla: 0, aktif_donus: "boş", aktif_fn: 0 - 1,
        fn_ad: [], fn_donus: [], fn_psay: [], fn_pbase: [], fn_ptip: [], fn_rlin: [], fn_ptn: [], fn_plin: [], fn_pdizi: [],
        yapi_ad: [], yapi_abase: [], yapi_acount: [], alan_ad: [], alan_tip: [], alan_tn: [], t14_muaf: [], m4_node: [], m4_tn: [],
        giris_dizin: "", kullan_gorulen: [],
        ly_ad: [], lyf_yapi: [], lyf_alan: [], lin_ad: [], lin_sat: [], lin_sut: [], lin_tuk: [], dy_node: [], dy_yapi: [], dyf_node: [], dyf_alan: [], lin_maske: [], lin_fnk: [], lin_tek: [], lin_yet: [], gtas_ad: [], kt_ad: [], snap_tuk: [], snap_maske: [], snap_top: 0, imha_bag: 0, guv_bag: 0, yer_son: 0, tuk_kapali: 0, cast_sessiz: 0, cast_hata_var: 0, cip_bag: 0, lin_basla: 0,
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0
    };
}
```

## selfhost/codegen.kem — KESİT (satır 328-336; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
    lin_basla: tam32;        // aktif işlevin lineer bağlama diliminin başı
    th_kod: Dizi<metin>;     // tip-hata kodları (traversal sırası)
    t043_node: Dizi<tam32>;  // D-630: T043 raporlanmış literal düğümleri (tekilleştirme)
    tasma_node: Dizi<tam32>;  // [D-632] 2^64+ (doymuş) TAM düğümleri — a_deg doymuş değeri taşır
    neg_lit: tam32;          // D-630: literal tekli eksinin operandı mı (alt sınır +1)
    th_sat: Dizi<tam32>;
    th_sut: Dizi<tam32>;
}

```

## selfhost/codegen.kem — KESİT (satır 1518-1560; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
}

// Sonek operatör (. [] () :: olarak). Pozisyon = sonek token'ı.
işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {
    değişken t: metin = simdiki(p);
    değişken s: tam32 = sim_sat(p);  değişken u: tam32 = sim_sut(p);
    eğer metin_esit(t, "NOKTA") {
        ilerle(p);
        değişken alan: metin = sim_lex(p);
        bekle(p, "TANIMLAYICI");
        ver dugum1(p, "ERISIM", alan, s, u, sol);
    }
    eğer metin_esit(t, "SOL_KOSELI") {
        ilerle(p);
        değişken idx: tam32 = parse_ifade(p);
        bekle(p, "SAG_KOSELI");
        ver dugum2(p, "INDEKS", "", s, u, sol, idx);
    }
    eğer metin_esit(t, "SOL_PAREN") {
        ilerle(p);
        değişken kids: Dizi<tam32> = [];
        dizi_ekle(kids, sol);  // çocuk[0] = hedef
        eğer sim_mi(p, "SAG_PAREN") == yanlış {
            değişken devam: mantıksal = doğru;
            iken devam {
                eğer sim_mi(p, "SAG_PAREN") { devam = yanlış; }
                değilse {
                    dizi_ekle(kids, parse_ifade(p));
                    eğer sim_mi(p, "VIRGUL") { ilerle(p); }
                    değilse { devam = yanlış; }
                }
            }
        }
        bekle_k(p, "SAG_PAREN", "P122");   // [D-652] C ifade.c:549 cagri ")"
        ver dugum_n(p, "CAGRI", "", s, u, kids);
    }
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        değişken sag: metin = sim_lex(p);
        bekle(p, "TANIMLAYICI");
        ver dugum1(p, "YOL", sag, s, u, sol);
    }
    eğer metin_esit(t, "OLARAK") {
```

## selfhost/codegen.kem — KESİT (satır 9375-9392; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
    ver ad;
}
işlev duz_yaz(p: &değişken Ayr, idx: tam32, derinlik: tam32) -> tam32 {
    yaz_tam(derinlik);  yb(9);
    yaz_str(dump_ad(dizi_al(p.a_ad, idx)));  yb(9);
    yaz_kacis(dizi_al(p.a_deg, idx));  yb(9);
    yaz_tam(dizi_al(p.a_sat, idx));  yb(9);
    yaz_tam(dizi_al(p.a_sut, idx));  yb(10);
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        i = i + 1;
    }
    ver 0;
}

```

## selfhost/codegen.kem — KESİT (satır 14636-14641; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
        fn_donus: [], fn_psay: [], fn_pbase: [], fn_ptip: [], fn_ptn: [], fn_plin: [], fn_pdizi: [],
        giris_dizin: "", kullan_gorulen: [],
        ly_ad: [], lyf_yapi: [], lyf_alan: [], lin_ad: [], lin_sat: [], lin_sut: [], lin_tuk: [], dy_node: [], dy_yapi: [], dyf_node: [], dyf_alan: [], lin_maske: [], lin_fnk: [], lin_tek: [], lin_yet: [], gtas_ad: [], kt_ad: [], snap_tuk: [], snap_maske: [], snap_top: 0, imha_bag: 0, guv_bag: 0, yer_son: 0, tuk_kapali: 0, cast_sessiz: 0, cast_hata_var: 0, cip_bag: 0, lin_basla: 0,
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0
    };
}
```

# GÖREV

# Tur 23 — turbofish (`f::<T1, T2>(args)`) PARSER — C + üç self-host parser

Yeni sözdizimi: ifade konumunda `ad::<Tip, ...>(argümanlar)` = AÇIK generic
argümanlı işlev çağrısı. Bugün HER İKİ derleyici bunu reddediyor (C `P123`,
self-host `P000` — ölçüldü). Bu tur YALNIZ parser + AST dump'ı ekler; tip
kontrol ve codegen SONRAKİ turlarda. Aşağıdaki dosyalar DIŞINDA hiçbir dosyaya
DOKUNMA. Her değişiklik bir ```duzenle bloğu; ESKI metin dosyada AYNEN ve TEK
kez geçmeli (aşağıda verdiğim ESKI metinleri harfiyen kopyala, boşluklar dâhil).

## Belirsizlik kuralı (tasarım — kodda yorum olarak yaz)
1. `f<T>(x)` ifade konumunda generic çağrı DEĞİLDİR; eskisi gibi `(f < T) > (x)`
   karşılaştırmasıdır (hiçbir mevcut davranış değişmez).
2. Generic argüman ifade konumunda YALNIZ `::<` ile açılır. `::` sonrası `<`
   gelirse bu bir tip argüman listesidir (yol devamı tanımlayıcı ister, `<`
   olamaz → çakışma yok).
3. Liste kapanınca HEMEN `(` gelmeli (turbofish yalnız bir çağrıya bağlanır).
   Gelmezse `P125` hatası. `f::<A>::<B>()` → ikinci `::`da P125.
4. CAGRI düğümünün konumu, turbofish'siz çağrıyla AYNI: `(` simgesinin konumu.

## C (3 dosya)

### 1. `src/ast.h` — cagri yapısına iki alan
ESKI (tam olarak):
```
        struct {
            Dugum *hedef;          /* cagrilan ifade */
            Dugum **argumanlar;
            int sayi;
        } cagri;
```
YENI:
```
        struct {
            Dugum *hedef;          /* cagrilan ifade */
            Dugum **argumanlar;
            int sayi;
            /* [D1 turbofish] `f::<T1,T2>(..)` ACIK generic argumanlari
             * (tip dugumleri). Turbofish yoksa NULL / 0 — dugum_olustur
             * sifirlar, yani tum eski cagri yollari degismez. */
            Dugum **tip_arg;
            int tip_arg_sayi;
        } cagri;
```

### 2. `src/ifade.c` — `parse_sonek_op` içindeki `TOK_CIFT_IKI_NOKTA` dalı
ESKI (tam olarak):
```
        case TOK_CIFT_IKI_NOKTA: {
            parser_ilerle(p);
            Token sag_tok = parser_bekle(p, TOK_TANIMLAYICI, "P123",
```
YENI: aynı üç satır, ama `parser_ilerle(p);` satırından HEMEN sonra şu blok girer
(blok bitince orijinal `Token sag_tok = ...` satırı ve devamı aynen kalır):
```
            /* [D1 turbofish] `::<` -> ACIK generic argumanli cagri.
             * Belirsizlik kurali: ifade konumunda `f<T>(x)` KARSILASTIRMADIR
             * (degismedi); generic argumanlar YALNIZ `::<` ile acilir. `::`
             * sonrasi yol devami tanimlayici ister, `<` olamaz -> cakisma
             * yok. Liste kapaninca HEMEN `(` gelmeli; turbofish yalniz bir
             * cagriya baglanir (`f::<A>::<B>()` -> P125). CAGRI dugumu `(`
             * konumunda kurulur (turbofish'siz cagriyla ayni). */
            if (parser_eslesir(p, TOK_KUCUK)) {
                Liste targs;
                liste_baslat(&targs);
                (void)parse_generic_listesi(p, "P318", "P319", 1, &targs);
                if (!parser_eslesir(p, TOK_SOL_PAREN)) {
                    parser_hata(p, parser_simdiki(p), "P125",
                        "turbofish '::<..>' sonrasi '(' bekleniyor "
                        "(yalniz islev cagrisi)", NULL);
                    return sol;
                }
                Dugum *c = parse_sonek_op(p, sol);   /* '(' -> DUGUM_CAGRI */
                if (c && c->tip == DUGUM_CAGRI) {
                    c->veri.cagri.tip_arg = liste_array_yap(&targs, p->arena);
                    c->veri.cagri.tip_arg_sayi = targs.sayi;
                }
                return c;
            }
```
`parse_generic_listesi` bu dosyada `parse_sonek_op`tan SONRA tanımlı, `static`
ve İLERİ BİLDİRİMİ YOK (ölçüldü). Bu yüzden AYRI bir duzenle bloğu ZORUNLU:
ESKI olarak `/* === Sonek operatorler (cagri, erisim, indeks, yol) === */`
satırını kullan, YENI = aşağıdaki ileri bildirim + boş satır + o satır:
```
static Dugum *parse_generic_listesi(Parser *p, const char *kod_ac,
                                     const char *kod_kapa, int min_say,
                                     Liste *out_args);
```

### 3. `src/ast_yazdir.c` — iki CAGRI dalı
(a) Düz dump (`ast_duz_yaz`, `--ast`) — ESKI (tam olarak):
```
        case DUGUM_CAGRI:
            ast_duz_yaz(d->veri.cagri.hedef, c, dd);
            for (int i = 0; i < d->veri.cagri.sayi; i++)
```
YENI:
```
        case DUGUM_CAGRI:
            ast_duz_yaz(d->veri.cagri.hedef, c, dd);
            /* [D1 turbofish] sira: hedef, ACIK tip argumanlari, argumanlar.
             * Tip dugumleri (TIP_*) ifade olamaz -> dump belirsiz degil. */
            for (int i = 0; i < d->veri.cagri.tip_arg_sayi; i++)
                ast_duz_yaz(d->veri.cagri.tip_arg[i], c, dd);
            for (int i = 0; i < d->veri.cagri.sayi; i++)
```
(b) Girintili dump (`ast_yazdir_indent`) — ESKI (tam olarak):
```
            ast_yazdir_indent(d->veri.cagri.hedef, c, derinlik + 2);
            indent_yaz(c, derinlik + 1);
            fprintf(c, "argumanlar (%d):\n", d->veri.cagri.sayi);
```
YENI:
```
            ast_yazdir_indent(d->veri.cagri.hedef, c, derinlik + 2);
            if (d->veri.cagri.tip_arg_sayi > 0) {   /* [D1 turbofish] */
                indent_yaz(c, derinlik + 1);
                fprintf(c, "tip_argumanlari (%d):\n", d->veri.cagri.tip_arg_sayi);
                cocuk_listesi_yaz(c, d->veri.cagri.tip_arg,
                                  d->veri.cagri.tip_arg_sayi, derinlik + 1);
            }
            indent_yaz(c, derinlik + 1);
            fprintf(c, "argumanlar (%d):\n", d->veri.cagri.sayi);
```

## Self-host (3 dosya: `selfhost/parser.kem`, `selfhost/checker.kem`, `selfhost/codegen.kem`)

KEMGU dizgi literallerinde KAÇIŞ YOK. Türkçe karakterleri olduğu gibi yaz.
Self-host'ta CAGRI çocukları `[hedef, arg1, arg2, ...]` — bu invaryanta
checker/codegen her yerde dayanır, DEĞİŞTİRME. Tip argümanları PARALEL TABLOYA
(yan-kanal) gider; dump onları C ile aynı sırada basar.

Her üç dosyada AYNI dört değişiklik:

### A. `yapı Ayr` — dört alan
- `selfhost/parser.kem`: ESKI (tam olarak)
  `    sk_pending: tam32;   // >> (SAGA_KAYDIR) yarı-tüketildi (generic kapanış böl)`
  satırının hemen ardına (satır KALIR) şunu ekle.
- `selfhost/checker.kem` ve `selfhost/codegen.kem`: yapının SON alanı
  `    th_sut: Dizi<tam32>;` satırıdır ve hemen ardından `}` gelir. ESKI olarak
  şu İKİ satırı kullan (dosyada tek kez geçer):
  ```
      th_sat: Dizi<tam32>;
      th_sut: Dizi<tam32>;
  ```
  (checker.kem'de iki satırın başında 4 boşluk var; codegen.kem'de de 4 boşluk.)
  YENI = aynı iki satır + ardından eklenen alanlar.

Eklenecek alanlar:
```
    // [D1 turbofish] `f::<T1,T2>(..)` yan-kanalı: CAGRI düğümü → AÇIK tip
    // argüman düğümleri. tf_cagri[k] CAGRI idx; tipler tf_tip[tf_bas[k] ..
    // tf_bas[k] + tf_say[k]). Düğüme çocuk EKLENMEZ (çocuk[1..] = argüman).
    tf_cagri: Dizi<tam32>;
    tf_bas: Dizi<tam32>;
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
```

### B. `ayr_olustur` — dört alanı başlat
- parser.kem: ESKI `        sk_pending: 0` (bu dosyada ayr_olustur içinde,
  ardından satır sonu ve `    };`). Dikkat: parser.kem'de `sk_pending: 0`
  SON alandır (sonunda virgül yok). ESKI olarak şu İKİ satırı kullan:
  ```
          sk_pending: 0
      };
  ```
  YENI:
  ```
          sk_pending: 0,
          tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []
      };
  ```
- checker.kem ve codegen.kem: ayr_olustur'un SON satırı (her iki dosyada aynı
  metin, dosya başına tek kez):
  `        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0`
  YENI: `        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0,`
  ve ardından yeni satır
  `        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []`

### C. `parse_sonek` — `CIFT_IKI_NOKTA` dalı + iki yardımcı
ESKI (üç dosyada da harfiyen bu, her dosyada tek kez):
```
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        değişken sag: metin = sim_lex(p);
```
YENI:
```
    eğer metin_esit(t, "CIFT_IKI_NOKTA") {
        ilerle(p);
        // [D1 turbofish] `::<` → AÇIK generic argümanlı çağrı (C ifade.c
        // aynası; belirsizlik kuralı orada). Liste kapanınca HEMEN `(`
        // gelmeli (P125). CAGRI `(` konumunda kurulur (özyineli parse_sonek).
        eğer sim_mi(p, "KUCUK") {
            değişken targs: Dizi<tam32> = parse_generic_args(p);
            eğer sim_mi(p, "SOL_PAREN") == yanlış {
                HATA_SATIRI
                ver sol;
            }
            değişken tc: tam32 = parse_sonek(p, sol);
            tf_kaydet(p, tc, targs);
            ver tc;
        }
        değişken sag: metin = sim_lex(p);
```
`HATA_SATIRI` yerine:
- parser.kem: `p.hata_say = p.hata_say + 1;`
- checker.kem ve codegen.kem: `parse_hata_kaydet(p, "P125");`

İki yardımcıyı her dosyada `// ---- Öncelik tabloları` ... değil, `parse_sonek`
işlevinin HEMEN ÖNCESİNE ekle. ESKI olarak her dosyadaki
`işlev parse_sonek(p: &değişken Ayr, sol: tam32) -> tam32 {` satırını kullan
(tek kez geçer), YENI = aşağıdaki iki işlev + o satırın kendisi:
```
// [D1 turbofish] CAGRI düğümünün AÇIK tip argümanlarını yan-kanala kaydet.
işlev tf_kaydet(p: &değişken Ayr, cagri: tam32, tipler: Dizi<tam32>) -> tam32 {
    dizi_ekle(p.tf_cagri, cagri);
    dizi_ekle(p.tf_bas, dizi_boyut(p.tf_tip));
    dizi_ekle(p.tf_say, dizi_boyut(tipler));
    değişken i: tam32 = 0;
    iken i < dizi_boyut(tipler) { dizi_ekle(p.tf_tip, dizi_al(tipler, i)); i = i + 1; }
    ver 0;
}
// CAGRI idx → tf tablosundaki kayıt indeksi (-1 = turbofish yok).
işlev tf_bul(p: &değişken Ayr, cagri: tam32) -> tam32 {
    değişken i: tam32 = 0;
    iken i < dizi_boyut(p.tf_cagri) {
        eğer dizi_al(p.tf_cagri, i) == cagri { ver i; }
        i = i + 1;
    }
    ver 0 - 1;
}
```
(Önce `parse_sonek` satırının kendisinden önceki yorum satırını bozmamak için
ESKI'yi yalnız `işlev parse_sonek(...)` satırı tut; yardımcılar yorumdan sonra,
işlevden önce girer — sorun değil.)

### D. `duz_yaz` — tip argümanlarını C sırasıyla bas
ESKI (üç dosyada da harfiyen bu, tek kez):
```
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        i = i + 1;
    }
```
YENI:
```
    değişken cb: tam32 = dizi_al(p.a_cb, idx);
    değişken cs: tam32 = dizi_al(p.a_cs, idx);
    değişken tfk: tam32 = tf_bul(p, idx);   // [D1 turbofish] -1 = yok
    değişken i: tam32 = 0;
    iken i < cs {
        duz_yaz(p, dizi_al(p.cocuk, cb + i), derinlik + 1);
        // C ast_duz_yaz sırası: hedef (çocuk 0), AÇIK tip argümanları, argümanlar.
        eğer (i == 0) ve (tfk >= 0) {
            değişken ti: tam32 = 0;
            iken ti < dizi_al(p.tf_say, tfk) {
                duz_yaz(p, dizi_al(p.tf_tip, dizi_al(p.tf_bas, tfk) + ti), derinlik + 1);
                ti = ti + 1;
            }
        }
        i = i + 1;
    }
```

Toplam blok sayısı: C 5 + self-host 3×5 = 20.

````
