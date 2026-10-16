# Tur 2 — dma_bariyeri — istem

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

// Çekirdek paniği: PL011'e (0x09000000) mesaj bas ve DUR. kdl_panik kem_os'ta YOK
// (D-592); çıplak bağlamda yalnız çıplak çağrılabilir (E013) → doğrudan MMIO.
// Kanal (D-623) ve görev (D-624) ortak kullanır.
çıplak işlev kem_panik(m: metin) {
    değişken n: tam32 = metin_uzunluk(m);
    değişken i: tam32 = 0;
    iken i < n {
        kdl_mmio_yaz32(150994944, metin_bayt(m, i) olarak tam32);
        i = i + 1;
    }
    kdl_mmio_yaz32(150994944, 10);
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

## runtime/kem_virtio_blk.kem
```
// kem_virtio_blk.kem — FAZ B1 (D-271): SAF-.kem VirtIO-Blk blok I/O sürücüsü.
// ============================================================================
// Kanıtlı-C kdl_virtio.c'nin saf-.kem yeniden gerçekleştirmesi (real-OS bring-up,
// migration DEĞİL). VirtIO-MMIO v2 (modern), split virtqueue: desc[] + avail + used.
// 512-baytlık sektör oku/yaz → gerçek disk kalıcılığı (QEMU -drive).
//
// SAF-.kem primitifleri (hepsi kanıtlandı, D-269 P1 + çıplak-tier):
//   • MMIO + DMA-RAM erişimi = çıplak VOLATILE deref (kdl_mmio deseni; store/load volatile).
//   • dsb sy bariyeri        = satıriçi_asm arm64 (--mimari arm64; DMA↔MMIO sıralama).
//   • DMA tamponları          = SABİT identity-map RAM adresi (0x43000000, EL0 üstü, backed),
//                              alt-tamponlar MANUEL 16-hizalı offset (P2 manuel over-align —
//                              A1'in 4KB-align'ının 16B önkoşul-kanıtı).
//   • Cross-file YOK          = tamamen kendi-içinde (malloc/memcpy YOK → T002 YOK). Build'de
//                              kem_os.kem ile CAT edilir (tek birim → çıplak→çıplak çözülür).
// Register offsetleri kdl_virtio.c / drivers/virtio/constants.kem ile birebir.

// --- VirtIO-MMIO v2 register offsetleri (ondalık) ---
sabit VM_MAGIC: tam64      = 0;
sabit VM_VERSION: tam64    = 4;
sabit VM_DEVICE_ID: tam64  = 8;
sabit VM_DRV_FEAT: tam64   = 32;
sabit VM_DRV_FEAT_SEL: tam64 = 36;
sabit VM_QUEUE_SEL: tam64  = 48;
sabit VM_QUEUE_NUM_MAX: tam64 = 52;
sabit VM_QUEUE_NUM: tam64  = 56;
sabit VM_QUEUE_READY: tam64 = 68;
sabit VM_QUEUE_NOTIFY: tam64 = 80;
sabit VM_STATUS: tam64     = 112;
sabit VM_Q_DESC_LO: tam64  = 128;
sabit VM_Q_DESC_HI: tam64  = 132;
sabit VM_Q_DRV_LO: tam64   = 144;
sabit VM_Q_DRV_HI: tam64   = 148;
sabit VM_Q_DEV_LO: tam64   = 160;
sabit VM_Q_DEV_HI: tam64   = 164;

sabit VBLK_MAGIC_DEG: tam32 = 1953655158;   // 0x74726976 "virt"
sabit VBLK_DEV_BLK: tam32   = 2;

// DMA taban: SABİT identity-map RAM (0x43000000 = 1124073472). EL0 (0x42000000) üstü,
// heap (~0x41xxxxxx) üstü, 128MB+ RAM'de backed, kdl_mmu Normal-WB haritalı. 16-hizalı.
küresel değişken vblk_dma: tam64 = 1124073472;

// DMA alt-tampon offsetleri (hepsi 16'nın katı — MANUEL over-align):
//   desc  @ +0    (8×16 = 128B),  avail @ +128 (22B→16-pad),  used @ +160 (70B→16-pad),
//   req   @ +256  (16B),          data  @ +272 (512B),        status @ +784 (1B).
sabit DO_DESC: tam64   = 0;
sabit DO_AVAIL: tam64  = 128;
sabit DO_USED: tam64   = 160;
sabit DO_REQ: tam64    = 256;
sabit DO_DATA: tam64   = 272;
sabit DO_STATUS: tam64 = 784;

// --- Çıplak VOLATILE erişim yardımcıları (MMIO + DMA-RAM; store/load volatile) ---
çıplak işlev vblk_y32(adr: tam64, deger: tam32) { değişken p: *tam32 = adr olarak *tam32; *p = deger; }
çıplak işlev vblk_o32(adr: tam64) -> tam32 { değişken p: *tam32 = adr olarak *tam32; ver *p; }
çıplak işlev vblk_y64(adr: tam64, deger: tam64) { değişken p: *tam64 = adr olarak *tam64; *p = deger; }
çıplak işlev vblk_y16(adr: tam64, deger: dtam16) { değişken p: *dtam16 = adr olarak *dtam16; *p = deger; }
çıplak işlev vblk_o16(adr: tam64) -> dtam16 { değişken p: *dtam16 = adr olarak *dtam16; ver *p; }
çıplak işlev vblk_y8(adr: tam64, deger: dtam8) { değişken p: *dtam8 = adr olarak *dtam8; *p = deger; }
çıplak işlev vblk_o8(adr: tam64) -> dtam8 { değişken p: *dtam8 = adr olarak *dtam8; ver *p; }

// dsb sy — DMA descriptor yazımı ↔ MMIO notify sıralaması (P1 arm64 asm).
çıplak işlev vblk_dsb() {
    güvensiz {
        satıriçi_asm { mimari: arm64 şablon: r#"dsb sy"# bozulan("~{memory}") }
    }
}

// Caller'ın data tamponunu doldurup okuması için erişimciler (vq_data = vblk_dma+DO_DATA).
çıplak işlev vblk_data_yaz(i: tam32, deger: dtam8) { vblk_y8(vblk_dma + DO_DATA + (i olarak tam64), deger); }
çıplak işlev vblk_data_oku(i: tam32) -> dtam8 { ver vblk_o8(vblk_dma + DO_DATA + (i olarak tam64)); }

// Cihaz tara (DeviceID=2). Taban adresi döner (0 = yok). 32 slot × 0x200 @ 0x0a000000.
çıplak işlev vblk_bul() -> tam64 {
    değişken i: tam32 = 0;
    iken i < 32 {
        değişken base: tam64 = 167772160 + (i olarak tam64) * 512;
        eğer vblk_o32(base + VM_MAGIC) == VBLK_MAGIC_DEG {
            eğer vblk_o32(base + VM_DEVICE_ID) == VBLK_DEV_BLK { ver base; }
        }
        i = i + 1;
    }
    ver 0;
}

// Cihazı başlat: reset → feature(VERSION_1) → virtqueue 0 → DRIVER_OK. 0=ok, negatif=hata.
çıplak işlev vblk_kur(base: tam64) -> tam32 {
    eğer vblk_o32(base + VM_VERSION) != 2 { ver 0 - 1; }
    vblk_y32(base + VM_STATUS, 0);          // reset
    vblk_y32(base + VM_STATUS, 1);          // ACK
    vblk_y32(base + VM_STATUS, 3);          // ACK|DRIVER
    // Feature: yalnız bit32 (VERSION_1). düşük feature yok.
    vblk_y32(base + VM_DRV_FEAT_SEL, 1);
    vblk_y32(base + VM_DRV_FEAT, 1);
    vblk_y32(base + VM_DRV_FEAT_SEL, 0);
    vblk_y32(base + VM_DRV_FEAT, 0);
    vblk_y32(base + VM_STATUS, 11);         // ACK|DRIVER|FEAT_OK (1|2|8)
    eğer (vblk_o32(base + VM_STATUS) & 8) == 0 { ver 0 - 2; }
    vblk_y32(base + VM_QUEUE_SEL, 0);
    eğer vblk_o32(base + VM_QUEUE_NUM_MAX) < 8 { ver 0 - 3; }
    vblk_y32(base + VM_QUEUE_NUM, 8);
    // Ring fiziksel adresleri (identity-map → VA=PA).
    değişken da: tam64 = vblk_dma + DO_DESC;
    değişken av: tam64 = vblk_dma + DO_AVAIL;
    değişken us: tam64 = vblk_dma + DO_USED;
    vblk_y32(base + VM_Q_DESC_LO, da olarak tam32);   vblk_y32(base + VM_Q_DESC_HI, (da >> 32) olarak tam32);
    vblk_y32(base + VM_Q_DRV_LO,  av olarak tam32);   vblk_y32(base + VM_Q_DRV_HI,  (av >> 32) olarak tam32);
    vblk_y32(base + VM_Q_DEV_LO,  us olarak tam32);   vblk_y32(base + VM_Q_DEV_HI,  (us >> 32) olarak tam32);
    // RE-INIT GÜVENLİ (D-271→B2): avail/used idx RAM'de sıfırla → device+driver taze başlar.
    // C sürücü tek-init'ti (.bss sıfır); kem_os çok-subsystem HER birinde kur çağırır → stale
    // idx desync olmasın (poll `once=used.idx` eski değeri beklerdi). QUEUE_READY ÖNCESİ.
    vblk_y16(av + 2, 0 olarak dtam16);                 // avail.idx = 0
    vblk_y16(us + 2, 0 olarak dtam16);                 // used.idx = 0
    vblk_y32(base + VM_QUEUE_READY, 1);
    vblk_y32(base + VM_STATUS, 15);         // ACK|DRIVER|FEAT_OK|DRIVER_OK
    ver 0;
}

// 3-descriptor zincirini kur (başlık RO → veri → durum WO). type: 0=oku(IN), 1=yaz(OUT).
// veri descriptor'ı: oku'da cihaz-YAZAR (WRITE flag), yaz'da cihaz-OKUR (flag yok).
çıplak işlev vblk_zincir(sektor: tam64, tip: tam32, veri_write: tam32) {
    değişken req: tam64 = vblk_dma + DO_REQ;
    değişken data: tam64 = vblk_dma + DO_DATA;
    değişken st: tam64 = vblk_dma + DO_STATUS;
    değişken d: tam64 = vblk_dma + DO_DESC;
    // blk_req: type(u32)@0, reserved(u32)@4, sector(u64)@8
    vblk_y32(req + 0, tip);
    vblk_y32(req + 4, 0);
    vblk_y64(req + 8, sektor);
    vblk_y8(st, 255 olarak dtam8);          // status = 0xff (VIRTIO_BLK_S_OK=0 bekliyoruz)
    // desc[0]: req, len16, NEXT(1), next=1
    vblk_y64(d + 0, req); vblk_y32(d + 8, 16); vblk_y16(d + 12, 1 olarak dtam16); vblk_y16(d + 14, 1 olarak dtam16);
    // desc[1]: data, len512, NEXT|(veri_write?WRITE), next=2   (flags = 1 | (veri_write?2:0))
    değişken f1: dtam16 = (1 + veri_write * 2) olarak dtam16;
    vblk_y64(d + 16, data); vblk_y32(d + 24, 512); vblk_y16(d + 28, f1); vblk_y16(d + 30, 2 olarak dtam16);
    // desc[2]: status, len1, WRITE(2), next=0
    vblk_y64(d + 32, st); vblk_y32(d + 40, 1); vblk_y16(d + 44, 2 olarak dtam16); vblk_y16(d + 46, 0 olarak dtam16);
}

// Zinciri gönder (avail bump + notify) + used ring poll + status kontrol. 0=ok, negatif=hata.
çıplak işlev vblk_gonder(base: tam64) -> tam32 {
    değişken av: tam64 = vblk_dma + DO_AVAIL;
    değişken usaddr: tam64 = vblk_dma + DO_USED + 2;   // used.idx offset
    değişken idx: dtam16 = vblk_o16(av + 2);           // avail.idx
    değişken once: dtam16 = vblk_o16(usaddr);          // used.idx (öncesi)
    değişken slot: tam64 = ((idx olarak tam64) % 8) * 2;
    vblk_y16(av + 4 + slot, 0 olarak dtam16);          // ring[idx%8] = desc0 (zincir başı)
    vblk_dsb();
    vblk_y16(av + 2, (idx + (1 olarak dtam16)) olarak dtam16);          // avail.idx++
    vblk_dsb();
    vblk_y32(base + VM_QUEUE_NOTIFY, 0);               // queue 0 bildir
    // used.idx değişene kadar poll (volatile → hoist yok). timeout ile QEMU-hang koruması.
    değişken spin: tam32 = 0;
    iken vblk_o16(usaddr) == once {
        vblk_dsb();
        spin = spin + 1;
        eğer spin > 100000000 { ver 0 - 4; }
    }
    eğer vblk_o8(vblk_dma + DO_STATUS) != (0 olarak dtam8) { ver 0 - 5; }
    ver 0;
}

// `sektor`ü vq_data'ya OKU (caller vblk_data_oku ile okur). 0=ok.
çıplak işlev vblk_oku(base: tam64, sektor: tam64) -> tam32 {
    vblk_zincir(sektor, 0, 1);              // type=IN, veri cihaz-YAZAR
    ver vblk_gonder(base);
}

// vq_data'daki (caller vblk_data_yaz ile doldurur) 512 baytı `sektor`e YAZ (kalıcı). 0=ok.
çıplak işlev vblk_yaz(base: tam64, sektor: tam64) -> tam32 {
    vblk_zincir(sektor, 1, 0);              // type=OUT, veri cihaz-OKUR
    ver vblk_gonder(base);
}

```

## test/ornekler/kem_os_x86.kem
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
// Beklenen başarı çıktısı: "X86 HEAP DIZI OK toplam=" satırı + "55" satırı
// (yazdir_metin satır sonu ekler).

işlev main() -> tam32 {
    değişken d: Dizi<tam32> = [];

    // 1..10 ekle (bölge dizi büyümesi 4→8→16; saf-.kem kem_dizi_buyut).
    değişken i: tam32 = 1;
    iken i <= 10 {
        dizi_ekle(d, i);
        i = i + 1;
    }

    // Topla (beklenen: 55). `için` heap dizide sınır-kontrollü okur.
    değişken toplam: tam32 = 0;
    için x: d {
        toplam = toplam + x;
    }

    // Değeri SABİT yazma: gerçek toplamı bas (marker değil DEĞER denetimi).
    eğer toplam == 55 {
        yazdir_metin("X86 HEAP DIZI OK toplam=");
    }
    değilse {
        yazdir_metin("X86 HEAP DIZI HATA toplam=");
    }
    yazdir_tam(toplam);
    yazdir_satir();
    ver 0;
}

```

## otomasyon/evren/baglam/makefile_kem_os_arm_kesit.txt
```
calistir_kem_os_arm: $(BUILD)/kemgu$(EXE) $(KEM_OS_A64_OBJS) $(BUILD)/bm_a64_mmio_kem.o $(BUILD)/kem_user.elf
	@echo "Faz-2/B1 .kem-native OS: kem_virtio_blk.kem + kem_os.kem -> ARM64 ELF..."
	@# FAZ-B1/B2: virtio-blk + minifs .kem sürücüleri kem_os ile CAT (tek birim → çıplak→çıplak, T002 yok).
	@# --mimari arm64: dsb sy satıriçi_asm (P1) açık. Sıra: sürücü-bağımlılık (blk→minifs) → kem_os.
	@# kem_heap.kem ARTIK BİRLEŞİK BİRİMİN PARÇASI (eskiden ayrı bm_a64_kem_heap.o idi).
	@# Sebep: kdl_metin_uzunluk/kdl_metin_bayt çağrıları çapraz-birimdi → tip denetimi
	@# onları TANIMSIZ görüyordu (8×T002) ve bu, --tip-atla borcunun son kalemiydi.
	@# KEMGU'da üst düzey ileri-bildirim (gövdesiz imza) YOK — ölçüldü: `imza_yeterli`
	@# yalnız `özellik` gövdesinde geçerli (src/parser.c:738). Dolayısıyla tek-birim
	@# birleştirme, dil değişikliği GEREKTİRMEYEN tek çözüm. Falsifiye-kanıt gate'leri
	@# (SAF-.kem allocator/metin/mmio/yetki) bm_a64_kem_heap.o yerine kem_os.o'ya bakar.
	@cat runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem runtime/kem_zaman.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem runtime/kem_virtio_net.kem runtime/kem_elf.kem runtime/kem_dtb.kem test/ornekler/kem_os.kem > $(BUILD)/kem_os_comb.kem
...
	echo "  (kul_prog_selam @0x$$KULADR5 [.user'da, ELF-yukleme YOK, derleme-zamani linkli] + sys12->kem_gorev_olustur_el0 + kem_spawn_testi wire — GERCEK spawn+join SAF-.kem)"
	@echo "FALSIFIYE-KANIT (SUBSYSTEM/virtio-blk, D-271 FAZ-B1): kem_os GERCEK blok I/O saf-.kem surucuden:"
	@if ! grep -qE "define[^@]*@vblk_(oku|yaz|kur|bul)\b" $(BUILD)/kem_os.ll; then \
		echo "FAIL: kem_os IR'inda .kem vblk_* virtio-blk surucu define YOK"; exit 1; \
	fi
	@if ! grep -qE "call[^@]*@disk_rw_testi\b" $(BUILD)/kem_os.ll; then \
		echo "FAIL: disk_rw_testi WIRE edilmemis — kem_os gercek disk I/O calistirmiyor"; exit 1; \
	fi
	@if ! grep -qE 'asm sideeffect "dsb sy"' $(BUILD)/kem_os.ll; then \
		echo "FAIL: dsb sy bariyeri (P1 arm64 asm) emit edilmedi — DMA siralama yok"; exit 1; \
	fi
	@echo "  (vblk_bul/kur/oku/yaz .kem-define + disk_rw_testi wire + dsb sy P1-asm — virtio-blk SAF-.kem)"
	@echo "FALSIFIYE-KANIT (SUBSYSTEM/fs, FAZ-B2): kem_os GERCEK dosya I/O saf-.kem minifs'ten:"

```

## otomasyon/evren/baglam/makefile_kem_os_x86_kesit.txt
```
# === Bare-Metal KEMGU-OS x86_64 (İş 5) — saf-.kem allocator + bölge ile boot ===
# kem_heap.kem + kem_os_x86.kem CAT → malloc/free/memcpy/memset/bolge/dizi/metin
# TANIMLARI .o'dan gelir; bm_x86_heap.o ve bm_x86_bolge.o BİLEREK YOK (aksi hâlde
# C sürümleri .kem sembolleriyle çakışır). start_x86_64.S kem_heap_kur'u
# main'den önce çağırır → kem_son dolu. Üç kanıt: (1) nm'de ` T malloc` ve
# ` T kem_heap_kur`; (2) objdump'da kem_heap_kur'a giden call satırı (sabit
# metin DEĞİL, objdump'ın kendi satırı); (3) QEMU seri çıktıda
# "X86 HEAP DIZI OK toplam=" satırı ve HEMEN ARDINDAN "55" (yazdir_metin satır
# sonu ekler → değer ayrı satırda; denetim değerin KENDİSİNE bakar, D-264).
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
		$(BUILD)/bm_x86_start.o $(BUILD)/bm_x86_kesme.o $(BUILD)/bm_x86_zaman.o \
		$(BUILD)/bm_x86_gorev.o $(BUILD)/bm_x86_uart.o $(BUILD)/bm_x86_yazdir.o \
		$(BUILD)/bm_x86_panik.o
	@echo "Bare-metal kem_os x86_64 (saf-.kem allocator): CAT -> LLVM IR..."
	cat runtime/kem_heap.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem
	./$(BUILD)/kemgu$(EXE) --llvm --mimari x86_64 $(BUILD)/kem_os_x86_comb.kem > $(BUILD)/kem_os_x86_ham.ll
	awk -f test/strip_defined_declares.awk $(BUILD)/kem_os_x86_ham.ll > $(BUILD)/kem_os_x86.ll
	$(BM_X86) -O2 -Wno-override-module -x ir $(BUILD)/kem_os_x86.ll -c -o $(BUILD)/kem_os_x86.o
	ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld -o $(BUILD)/kem_os_x86.elf \
		$(BUILD)/kem_os_x86.o \
		$(BUILD)/bm_x86_start.o $(BUILD)/bm_x86_kesme.o $(BUILD)/bm_x86_zaman.o \
		$(BUILD)/bm_x86_gorev.o $(BUILD)/bm_x86_uart.o $(BUILD)/bm_x86_yazdir.o \
		$(BUILD)/bm_x86_panik.o
	@echo "  Kanit 1: sembol kaynagi (saf-.kem malloc/kem_heap_kur)..."
	@if ! llvm-nm $(BUILD)/kem_os_x86.o | grep -q ' T malloc'; then \
		echo "FAIL: malloc .kem'den gelmedi (T malloc yok)"; exit 1; fi
	@if ! llvm-nm $(BUILD)/kem_os_x86.o | grep -q ' T kem_heap_kur'; then \
		echo "FAIL: kem_heap_kur .kem'den gelmedi (T kem_heap_kur yok)"; exit 1; fi
	@echo "  Kanit 2: kem_heap_kur call satiri (objdump'un kendi satiri)..."
	@if ! llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep -q 'call.*<kem_heap_kur>'; then \
		echo "FAIL: kem_heap_kur call satiri yok"; exit 1; fi
	@llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep 'call.*<kem_heap_kur>'
	@echo "  Kanit 3: QEMU davranis (toplam=55)..."
	@if ! command -v qemu-system-x86_64 > /dev/null 2>&1; then \
		echo "FAIL: qemu-system-x86_64 yok — qemu kurun (sessiz atlama yok, D-486)"; exit 1; fi
	@rm -f $(BUILD)/kem_os_x86.out; \
	timeout 8 qemu-system-x86_64 -kernel $(BUILD)/kem_os_x86.elf -display none \
		-serial file:$(BUILD)/kem_os_x86.out 2>/dev/null || true; \
	echo "--- QEMU COM1 cikti ---"; cat $(BUILD)/kem_os_x86.out; echo "--- son ---"; \
	if grep -A1 -xF "X86 HEAP DIZI OK toplam=" $(BUILD)/kem_os_x86.out | tail -1 | grep -qxF "55"; then \
		echo "kem_os x86_64 QEMU testi gecti (toplam=55, deger satiri dogrulandi)."; \
	else echo "FAIL: 'X86 HEAP DIZI OK toplam=' + '55' satiri yok"; exit 1; fi


```

## otomasyon/evren/baglam/baremetal_diff_kesit.txt
```
# runtime/*.kem dosyalarının ÇOĞU tek başına derlenmez (birbirlerinin sembollerine
# bakarlar; C oracle T002 verir) → dosya-dosya döngü onların 6'sını ATLIYOR.
# GERÇEK derleme birimi Makefile'ın kurduğu birleşiktir (bkz. kem_os_comb.kem):
# tüm runtime + kem_os.kem. ~4166 satır, 240 işlev, 44 satıriçi asm bloğu.
# Bu adım o birimi bir bütün olarak ölçer — yoksa OS'un 2/3'ü kapısız kalır.
OS_PARCA="runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem \
runtime/kem_zaman.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem \
runtime/kem_virtio_net.kem runtime/kem_elf.kem runtime/kem_dtb.kem \
test/ornekler/kem_os.kem"

```

## otomasyon/evren/baglam/check_kapisi_kesit.txt
```
# Çekirdeğin KENDİSİ dilin tip kapısından geçmeli. kem_os bir zamanlar
# `--tip-atla` ile derleniyordu: "derleme zamanı güvenlik" tezini savunan bir
# dilin işletim sistemi, kendi tip denetimini atlıyordu. Borç kapandı; bu blok
# geri düşmeyi engeller. Dosya sırası Makefile'daki `calistir_kem_os_arm`
# cat sırasıyla AYNI olmalı (kem_heap ÖNCE — çapraz-birim T002'yi kapatır).
KEMOS_PARCALAR="runtime/kem_heap.kem runtime/kem_mmu.kem runtime/kem_gorev.kem
                runtime/kem_zaman.kem runtime/kem_virtio_blk.kem
                runtime/kem_minifs.kem runtime/kem_virtio_net.kem
                runtime/kem_elf.kem runtime/kem_dtb.kem test/ornekler/kem_os.kem"

```

# GÖREV

## İş 1 — Taşınabilir DMA bariyeri, D2b deseni (mimari başına dosya + CAT seçimi)

### Karar (orkestratör, verilmiş — tartışma)
Derleyiciye (src/*.c, selfhost/*) DOKUNULMAZ. Yerleşik yerine mimari başına
AYRI `.kem` dosyası; Makefile CAT listesi doğru mimarininkini seçer. AS001
yüzünden bir dosyada yalnız TEK mimarinin asm'si olabilir (ölçüldü).
Emsal: D-468 `sabitsüre` bariyeri arm64 → `csdb`, x86_64 → `lfence`; burada
DMA sıralaması için arm64 → `dsb sy`, x86_64 → `mfence`.

### İstenen değişiklikler (yalnız bunlar)

1. **YENİ `runtime/mimari/arm64/kem_bariyer.kem`** — tek işlev:
   `çıplak işlev kem_dma_bariyeri()` gövdesi `güvensiz { satıriçi_asm {
   mimari: arm64 şablon: r#"dsb sy"# bozulan("~{memory}") } }` (bugünkü
   `vblk_dsb` gövdesinin aynısı). Türkçe başlık yorumu: amaç (DMA tanımlayıcı
   yazımı ↔ MMIO notify sıralaması), neden ayrı dosya (AS001 + D2b), x86 ikizi.
2. **YENİ `runtime/mimari/x86_64/kem_bariyer.kem`** — aynı imza,
   `mimari: x86_64 şablon: r#"mfence"# bozulan("~{memory}")`. Yorumda:
   x86-TSO'da mağazalar sırayla görünür; `mfence` notify öncesi tam sıralama
   için güvenli tercih (performans önemsiz).
3. **`runtime/kem_virtio_blk.kem`** — `vblk_dsb` gövdesindeki `güvensiz { ... }`
   bloğunu `kem_dma_bariyeri();` çağrısıyla değiştir; işlevin adını ve 8 çağrı
   yerini DEĞİŞTİRME. Üstteki yorumu (`// dsb sy — ...`) mimari-bağımsız hâle
   getir. Dosya başlığındaki `• dsb sy bariyeri = satıriçi_asm arm64 ...`
   satırını yeni düzene göre güncelle (bariyer `runtime/mimari/<arch>/
   kem_bariyer.kem`'den; CAT birimi gereği).
4. **`Makefile` — `calistir_kem_os_arm` CAT satırı**: `runtime/kem_virtio_blk.kem`
   ÖNCESİNE `runtime/mimari/arm64/kem_bariyer.kem` ekle.
5. **`test/baremetal_diff_harness.sh` `OS_PARCA`** ve
   **`test/check_kapisi.sh` `KEMOS_PARCALAR`**: aynı yere (kem_virtio_blk
   öncesi) `runtime/mimari/arm64/kem_bariyer.kem` ekle. ZORUNLU: eklenmezse
   birleşik OS birimi T002 verir ve `baremetal_diff` bunu KIRMIZI değil
   "atlandı" sayar (sessiz kapsam kaybı).
6. **`Makefile` — `calistir_kem_os_x86`**:
   - CAT satırına, `test/ornekler/kem_os_x86.kem` ÖNCESİNE
     `runtime/mimari/x86_64/kem_bariyer.kem runtime/kem_virtio_blk.kem` ekle.
   - Yeni **Kanıt 4 (gerçek komut)**: `llvm-objdump -d --no-show-raw-insn
     $(BUILD)/kem_os_x86.elf` çıktısında `mfence` satırlarını bul ve
     OLDUKLARI GİBİ yazdır; yoksa `FAIL` + `exit 1`. Ayrıca `dsb` geçmemeli
     (geçerse `FAIL`).
   - Hedefin başındaki açıklama yorumuna Kanıt 4'ü ekle.
7. **`Makefile` — `calistir_kem_os_arm`**: mevcut
   `asm sideeffect "dsb sy"` IR denetiminin (FAIL mesajı "dsb sy bariyeri (P1
   arm64 asm) emit edilmedi") hemen ARDINA: `llvm-objdump -d
   --no-show-raw-insn $(BUILD)/kem_os.elf` çıktısında `dsb\s+sy` satırları
   bulunmalı (OLDUĞU GİBİ ilk 3'ünü yazdır); `mfence` geçmemeli.
8. **`test/ornekler/kem_os_x86.kem`**: `main`'in BAŞINDA `kem_dma_bariyeri();`
   çağır ve ardından `X86 DMA BARIYER OK` satırını bas (komut gerçekten
   ÇALIŞTI — #UD olsaydı çekirdek düşerdi). Normal işlevden çıplak çağrısı
   serbesttir. Başlık yorumuna bunu ekle. Mevcut heap testi aynen kalsın.
   Ayrıca `calistir_kem_os_x86` Kanıt 3'üne `X86 DMA BARIYER OK` satırının
   varlığını ekle (yoksa FAIL).

### Çapalar (ESKI metinleri — AYNEN kullan; her biri dosyada TEK kez geçer)
- `runtime/kem_virtio_blk.kem`:
  `        satıriçi_asm { mimari: arm64 şablon: r#"dsb sy"# bozulan("~{memory}") }`
  (vblk_dsb içindeki satır; çevresindeki `    güvensiz {` / `    }` satırlarıyla
  birlikte değiştir — bağlamdaki dosyadan birebir kopyala).
- Makefile kem_os_arm CAT: ` runtime/kem_zaman.kem runtime/kem_virtio_blk.kem`
  (bu dizgi Makefile'da TEK kez geçer).
- Makefile dsb IR denetimi: `		echo "FAIL: dsb sy bariyeri (P1 arm64 asm) emit edilmedi — DMA siralama yok"; exit 1; \`
  ve hemen sonraki `	fi` satırı.
- Makefile kem_os_x86 CAT: `	cat runtime/kem_heap.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem`
- baremetal_diff: `runtime/kem_zaman.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem \`
- check_kapisi: `                runtime/kem_zaman.kem runtime/kem_virtio_blk.kem`

### Yasak
`src/*.c`, `selfhost/*`, `runtime/kem_gorev.kem`, `runtime/kem_mmu.kem`,
`runtime/kem_zaman.kem`, `runtime/kem_virtio_net.kem` (onun 5 çağrı yeri
`vblk_dsb` üzerinden zaten düzelir).

````
