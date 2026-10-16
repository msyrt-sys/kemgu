# Tur 4b — dma_harita — istem

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

## boot/start_x86_64.S
```
/*
 * KEMGU Bare-Metal Boot — x86_64 (PVH → long mode)
 * ================================================
 *
 * QEMU `-kernel <64-bit ELF>` PVH yoluyla boot eder: .note.Xen içindeki
 * XEN_ELFNOTE_PHYS32_ENTRY (tip 18) 32-bit giriş adresini verir. QEMU
 * pvh_entry'ye 32-bit korumalı modda (paging KAPALI, A20 açık, düz segment)
 * atlar. Görevler:
 *   1. (32-bit) BSS sıfırla (sayfa tabloları dahil)
 *   2. (32-bit) Identity sayfa tabloları (0..1GB, 2MB huge page) + PAE + LME +
 *      paging → long mode; 64-bit kod segmentine far jump
 *   3. (64-bit) Segment register'ları + RSP=__stack_top kur, main() çağır
 *   4. main dönerse hlt döngüsü
 *
 * Linker: linker/bare-metal-x86_64.ld (yük 0x100000). aarch64 muadili
 * boot/start_aarch64.S. -mgeneral-regs-only ile C SSE emit etmez (SSE'yi boot'ta
 * etkinleştirmeye gerek yok).
 */

/* === PVH ELF note — QEMU 64-bit ELF'i 32-bit pvh_entry'den boot eder === */
.section .note.Xen, "a"
.align 4
.long 4                 /* namesz: "Xen\0" */
.long 4                 /* descsz: 32-bit giriş adresi (1 long) */
.long 18                /* type = XEN_ELFNOTE_PHYS32_ENTRY */
.asciz "Xen"
.align 4
.long pvh_entry
.align 4

/* === 32-bit giriş: long mode kurulumu === */
.section .text
.code32
.global pvh_entry
.type pvh_entry, @function
pvh_entry:
    cli
    cld

    /* 1. BSS sıfırla (pml4/pdpt/pd .bss'te → temizlenir, sonra kurulur). */
    movl    $__bss_start, %edi
    movl    $__bss_end, %ecx
    subl    %edi, %ecx
    xorl    %eax, %eax
    rep stosb

    /* 2a. PD: 512 × 2MB identity page (present|write|PS huge). */
    movl    $pd, %edi
    movl    $0x83, %eax            /* present|write|PS, fiziksel adres 0 */
    movl    $512, %ecx
1:
    movl    %eax, (%edi)           /* düşük 32 bit */
    movl    $0, 4(%edi)            /* yüksek 32 bit = 0 */
    addl    $0x200000, %eax        /* sonraki 2MB */
    addl    $8, %edi
    loop    1b

    /* 2b. PDPT[0] = PD | present|write */
    movl    $pd, %eax
    orl     $0x3, %eax
    movl    %eax, pdpt
    movl    $0, pdpt+4

    /* 2c. PML4[0] = PDPT | present|write */
    movl    $pdpt, %eax
    orl     $0x3, %eax
    movl    %eax, pml4
    movl    $0, pml4+4

    /* 2d. CR3 = PML4 */
    movl    $pml4, %eax
    movl    %eax, %cr3

    /* 2e. CR4.PAE (bit 5) */
    movl    %cr4, %eax
    orl     $0x20, %eax
    movl    %eax, %cr4

    /* 2f. EFER.LME (MSR 0xC0000080, bit 8) */
    movl    $0xC0000080, %ecx
    rdmsr
    orl     $0x100, %eax
    wrmsr

    /* 2g. CR0: PG(31) | PE(0) → paging + long mode aktif */
    movl    %cr0, %eax
    orl     $0x80000001, %eax
    movl    %eax, %cr0

    /* 2h. GDT yükle, 64-bit kod segmentine far jump */
    lgdt    gdt_desc
    ljmp    $0x08, $long_entry

/* === 64-bit giriş === */
.code64
long_entry:
    movw    $0x10, %ax             /* data segment */
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    %ax, %fs
    movw    %ax, %gs

    movq    $__stack_top, %rsp     /* yığın (16-hizalı linker'da) */

    call    kdl_idt_kur            /* C3a: IDT kur (isr0..31 → istisna işleyici) */

    /* 2c (aarch64 eşi): heap penceresini main'den ÖNCE kur. Saf-.kem allocator
     * (runtime/kem_heap.kem) kem_bump/kem_son'u bu çağrıdan alır; çağrılmazsa
     * kem_son=0 kalır → her @malloc null döner → dizi işlevleri sessizce 0
     * basar, panik YOK. kem_heap.kem ile bağlanMAyan C-malloc'lu x86
     * kernel'lerinde runtime/kdl_bare_heap.c'deki ZAYIF kem_heap_kur no-op'u
     * devreye girer (strong .kem sürümü override eder) → eski kernel'ler
     * bozulmaz. */
    movq    $__heap_start, %rdi    /* 1. argüman: heap tabanı (linker sembolü) */
    movq    $__heap_end, %rsi      /* 2. argüman: heap üst sınırı (dışlayıcı) */
    call    kem_heap_kur

    call    main                   /* KEMGU kernel main() */

.global _halt
_halt:
    hlt
    jmp     _halt

/* === GDT (long mode: null + 64-bit code + data) === */
.section .rodata
.align 8
gdt:
    .quad   0x0000000000000000     /* null */
    .quad   0x00209A0000000000     /* 0x08: 64-bit code (L, present, code, exec/read) */
    .quad   0x0000920000000000     /* 0x10: data (present, data, read/write) */
gdt_end:
gdt_desc:
    .word   gdt_end - gdt - 1      /* limit */
    .long   gdt                    /* base (32-bit lgdt; gdt < 4GB) */

/* === Sayfa tabloları (.bss, 4KB-hizalı; boot'ta sıfırlanıp kurulur) === */
.section .bss
.align 4096
pml4:   .skip 4096
pdpt:   .skip 4096
pd:     .skip 4096

/* === x86 exception ISR stub'ları (C3a) ===
 * Hata-kodlu istisnalar (8,10,11,12,13,14,17,21) CPU error code push eder → yalnız
 * vektör no push (ISR_ERR). Diğerleri dummy 0 + vektör (ISR_NOERR). Long mode'da
 * same-privilege exception bile SS:RSP:RFLAGS:CS:RIP push eder → ortak yığın:
 *   0(rsp)=vektör  8(rsp)=hata_kodu  16(rsp)=RIP  → isr_ortak → kdl_istisna_isle. */
.section .text
.code64
.macro ISR_NOERR n
isr\n:
    pushq   $0          /* dummy hata kodu */
    pushq   $\n
    jmp     isr_ortak
.endm
.macro ISR_ERR n
isr\n:
    pushq   $\n         /* hata kodu CPU'dan zaten yığında */
    jmp     isr_ortak
.endm

    ISR_NOERR 0
    ISR_NOERR 1
    ISR_NOERR 2
    ISR_NOERR 3
    ISR_NOERR 4
    ISR_NOERR 5
    ISR_NOERR 6
    ISR_NOERR 7
    ISR_ERR   8
    ISR_NOERR 9
    ISR_ERR   10
    ISR_ERR   11
    ISR_ERR   12
    ISR_ERR   13
    ISR_ERR   14
    ISR_NOERR 15
    ISR_NOERR 16
    ISR_ERR   17
    ISR_NOERR 18
    ISR_NOERR 19
    ISR_NOERR 20
    ISR_ERR   21
    ISR_NOERR 22
    ISR_NOERR 23
    ISR_NOERR 24
    ISR_NOERR 25
    ISR_NOERR 26
    ISR_NOERR 27
    ISR_NOERR 28
    ISR_NOERR 29
    ISR_NOERR 30
    ISR_NOERR 31

isr_ortak:
    movq    0(%rsp), %rdi    /* vektör no */
    movq    8(%rsp), %rsi    /* hata kodu */
    movq    16(%rsp), %rdx   /* RIP */
    call    kdl_istisna_isle /* noreturn (yazdır + halt) */
1:
    hlt
    jmp     1b

/* === IRQ0 (timer) stub (C3b) — bağlamı kaydet, dispatch, EOI, iretq ===
 * İstisnadan farkı: KURTARILIR (iretq) → kesilen koda dönülür. SysV caller-saved
 * kaydedilir (callee-saved'i C korur). PIC EOI = port 0x20'ye 0x20. */
.global kdl_irq0_stub
kdl_irq0_stub:
    pushq   %rax
    pushq   %rcx
    pushq   %rdx
    pushq   %rsi
    pushq   %rdi
    pushq   %r8
    pushq   %r9
    pushq   %r10
    pushq   %r11
    movq    $0, %rdi          /* IRQ no = 0 (timer) */
    call    kdl_kesme_isle
    movb    $0x20, %al        /* PIC EOI (master) */
    outb    %al, $0x20
    popq    %r11
    popq    %r10
    popq    %r9
    popq    %r8
    popq    %rdi
    popq    %rsi
    popq    %rdx
    popq    %rcx
    popq    %rax
    iretq

/* === Sistem çağrısı stub (C6) — int 0x80; num=rax, arg=rdi. KURTARILIR (iretq). ===
 * kdl_syscall_isle(rdi=num, rsi=arg). rax = dönüş (void demo → kullanılmaz). */
.global kdl_syscall_stub
kdl_syscall_stub:
    pushq   %rcx
    pushq   %rdx
    pushq   %rsi
    pushq   %rdi
    pushq   %r8
    pushq   %r9
    pushq   %r10
    pushq   %r11
    movq    %rdi, %rsi        /* arg → 2. param */
    movq    %rax, %rdi        /* num → 1. param */
    call    kdl_syscall_isle
    popq    %r11
    popq    %r10
    popq    %r9
    popq    %r8
    popq    %rdi
    popq    %rsi
    popq    %rdx
    popq    %rcx
    iretq

.section .rodata
.align 8
.global kdl_isr_tablo
kdl_isr_tablo:
    .quad   isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
    .quad   isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
    .quad   isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    .quad   isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

/* === Cooperative bağlam-değiştirme (C7a) — kdl_baglam_degis(rdi=eski, rsi=yeni) ===
 * Callee-saved (rbx,rbp,r12-r15) + rsp kaydet/yükle. ret → yeni görev sürer.
 * TCB: [0]=rbx,[1]=rbp,[2..5]=r12-r15,[6]=rsp (×8 bayt). */
.section .text
.code64
.global kdl_baglam_degis
kdl_baglam_degis:
    movq    %rbx, 0(%rdi)
    movq    %rbp, 8(%rdi)
    movq    %r12, 16(%rdi)
    movq    %r13, 24(%rdi)
    movq    %r14, 32(%rdi)
    movq    %r15, 40(%rdi)
    movq    %rsp, 48(%rdi)
    movq    0(%rsi), %rbx
    movq    8(%rsi), %rbp
    movq    16(%rsi), %r12
    movq    24(%rsi), %r13
    movq    32(%rsi), %r14
    movq    40(%rsi), %r15
    movq    48(%rsi), %rsp
    ret

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
// Ayrıca main başında kem_dma_bariyeri() çağrılır ve "X86 DMA BARIYER OK"
// satırı basılır (gerçekten çalıştığının kanıtı; #UD olsaydı çekirdek düşerdi).
// main sonunda kem_panik("X86 PANIK YOLU OK") çağrılır: panik geri dönmez,
// mesaj .kem COM1 yolundan basılır, çekirdek orada durur; QEMU zaman aşımı keser.

işlev main() -> tam32 {
    kem_dma_bariyeri();
    yazdir_metin("X86 DMA BARIYER OK");

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

    kem_panik("X86 PANIK YOLU OK");
    ver 0;
}

```

## runtime/kem_virtio_net.kem
```
// kem_virtio_net.kem — FAZ-C: SAF-.kem virtio-net transport (POLLED, IRQ YOK).
// ============================================================================
// Kanıtlı-C kdl_virtio_net.c'nin saf-.kem yeniden gerçekleştirmesi. [8] NET DEV OK:
// cihaz bul (DeviceID=1) + feature-negotiate + RX(queue0, device-yazar tamponlar) +
// TX(queue1) + DRIVER_OK. Paket TX/RX sonraki rung (net-arp/[9]).
//
// Genel VOLATILE deref + dsb yardımcıları kem_virtio_blk.kem'den (aynı CAT birimi:
// vblk_y32/o32/y64/y16/o16/y8/o8 + vblk_dsb; VM_* register offsetleri + VBLK_MAGIC_DEG).
// DMA = SABİT identity-RAM (MANUEL 16-hizalı offset). IRQ YOK (used-ring poll).
// Allocator YOK (static-adres). T002 yok.
//
// ⚠ AH-1'DE TAŞINDI: 0x44000000 → 0x46800000. Eski adres firmware'in DEVICE-TREE'yi
// koyduğu yerdi (QEMU virt'te ölçüldü; RPi firmware'i de RAM'e DTB yazar). Yani ağ
// sürücüsü ilk paketi göndermeden önce çekirdeğin donanım-keşif verisini EZİYORDU.
// Ölçüm: main'in başında 0x44000000'da FDT magic VAR, testlerin sonunda YOK.
// Bugüne kadar fark edilmemesinin sebebi DTB'nin hiç OKUNMAMASI (her adres
// sabit-kodluydu) — yani hata değil, hatanın GÖRÜNMEZLİĞİ yerleşikti.
//
// SINIF OLARAK KALICI ÇÖZÜM DEĞİL: sabit-adres DMA, firmware'in RAM'e koyduğu
// HERHANGİ bir yapıyla çakışabilir. Doğrusu DTB'nin /memreserve + /reserved-memory
// düğümlerini okuyup ayrılmış bölgelerden KAÇINMAK. AH-2'nin işi.

sabit VN_DEV_NET: tam32 = 1;

küresel değişken vnet_dma: tam64 = 1182793728;   // 0x46800000 (DTB bölgesinden UZAK)
küresel değişken vnet_rx_gorulen: tam32 = 0;     // işlenen RX used indeksi (kur'da sıfırlanır)
küresel değişken vnet_rx_id: tam32 = 0;           // son alınan çerçevenin buffer id'si

// DMA offsetleri (hepsi 16-katı): RX ring + 8×2048 rx_buf + TX ring + tx_buf.
sabit NO_RXDESC: tam64  = 0;        // 8×16 = 128
sabit NO_RXAVAIL: tam64 = 128;      // 22 → pad
sabit NO_RXUSED: tam64  = 160;      // 70 → pad
sabit NO_RXBUF: tam64   = 256;      // 8×2048 = 16384 (rx_buf[i] @ 256 + i*2048)
sabit NO_TXDESC: tam64  = 16640;    // 128
sabit NO_TXAVAIL: tam64 = 16768;    // 22
sabit NO_TXUSED: tam64  = 16800;    // 70
sabit NO_TXBUF: tam64   = 16896;    // 2048 (TX çerçeve tamponu — [9])

// virtio-net cihazı tara (DeviceID=1). Taban döner (0=yok).
çıplak işlev vnet_bul() -> tam64 {
    değişken i: tam32 = 0;
    iken i < 32 {
        değişken base: tam64 = 167772160 + (i olarak tam64) * 512;
        eğer vblk_o32(base + VM_MAGIC) == VBLK_MAGIC_DEG {
            eğer vblk_o32(base + VM_DEVICE_ID) == VN_DEV_NET { ver base; }
        }
        i = i + 1;
    }
    ver 0;
}

// Cihazı başlat: reset → feature(VERSION_1) → RX queue0 → TX queue1 → DRIVER_OK. 0=ok.
çıplak işlev vnet_kur(base: tam64) -> tam32 {
    eğer vblk_o32(base + VM_VERSION) != 2 { ver 0 - 1; }
    vblk_y32(base + VM_STATUS, 0);
    vblk_y32(base + VM_STATUS, 1);
    vblk_y32(base + VM_STATUS, 3);
    vblk_y32(base + VM_DRV_FEAT_SEL, 1);
    vblk_y32(base + VM_DRV_FEAT, 1);        // bit32 VERSION_1
    vblk_y32(base + VM_DRV_FEAT_SEL, 0);
    vblk_y32(base + VM_DRV_FEAT, 0);
    vblk_y32(base + VM_STATUS, 11);
    eğer (vblk_o32(base + VM_STATUS) & 8) == 0 { ver 0 - 2; }   // FEAT_OK device onayı
    // --- RX queue (0): 8 descriptor, her biri rx_buf[i] (2048, device WRITE flag=2) ---
    vblk_y32(base + VM_QUEUE_SEL, 0);
    eğer vblk_o32(base + VM_QUEUE_NUM_MAX) < 8 { ver 0 - 3; }
    vblk_y32(base + VM_QUEUE_NUM, 8);
    değişken rd: tam64 = vnet_dma + NO_RXDESC;
    değişken i: tam32 = 0;
    iken i < 8 {
        değişken de: tam64 = rd + (i olarak tam64) * 16;
        değişken buf: tam64 = vnet_dma + NO_RXBUF + (i olarak tam64) * 2048;
        vblk_y64(de + 0, buf); vblk_y32(de + 8, 2048); vblk_y16(de + 12, 2 olarak dtam16); vblk_y16(de + 14, 0 olarak dtam16);
        vblk_y16(vnet_dma + NO_RXAVAIL + 4 + (i olarak tam64) * 2, i olarak dtam16);   // rx_avail.ring[i]=i
        i = i + 1;
    }
    vblk_dsb();
    vblk_y16(vnet_dma + NO_RXAVAIL + 2, 8 olarak dtam16);   // rx_avail.idx=8 (tüm tamponlar hazır)
    vblk_y16(vnet_dma + NO_RXUSED + 2, 0 olarak dtam16);    // rx_used.idx=0 (re-init güvenli)
    vnet_rx_gorulen = 0;                                    // RX seen sayacı taze (ring ile senkron)
    değişken rav: tam64 = vnet_dma + NO_RXAVAIL;
    değişken rus: tam64 = vnet_dma + NO_RXUSED;
    vblk_y32(base + VM_Q_DESC_LO, rd olarak tam32);  vblk_y32(base + VM_Q_DESC_HI, (rd >> 32) olarak tam32);
    vblk_y32(base + VM_Q_DRV_LO,  rav olarak tam32); vblk_y32(base + VM_Q_DRV_HI,  (rav >> 32) olarak tam32);
    vblk_y32(base + VM_Q_DEV_LO,  rus olarak tam32); vblk_y32(base + VM_Q_DEV_HI,  (rus >> 32) olarak tam32);
    vblk_y32(base + VM_QUEUE_READY, 1);
    // --- TX queue (1) ---
    vblk_y32(base + VM_QUEUE_SEL, 1);
    eğer vblk_o32(base + VM_QUEUE_NUM_MAX) < 8 { ver 0 - 3; }
    vblk_y32(base + VM_QUEUE_NUM, 8);
    vblk_y16(vnet_dma + NO_TXAVAIL + 2, 0 olarak dtam16);
    vblk_y16(vnet_dma + NO_TXUSED + 2, 0 olarak dtam16);
    değişken td: tam64 = vnet_dma + NO_TXDESC;
    değişken ta: tam64 = vnet_dma + NO_TXAVAIL;
    değişken tu: tam64 = vnet_dma + NO_TXUSED;
    vblk_y32(base + VM_Q_DESC_LO, td olarak tam32);  vblk_y32(base + VM_Q_DESC_HI, (td >> 32) olarak tam32);
    vblk_y32(base + VM_Q_DRV_LO,  ta olarak tam32);  vblk_y32(base + VM_Q_DRV_HI,  (ta >> 32) olarak tam32);
    vblk_y32(base + VM_Q_DEV_LO,  tu olarak tam32);  vblk_y32(base + VM_Q_DEV_HI,  (tu >> 32) olarak tam32);
    vblk_y32(base + VM_QUEUE_READY, 1);
    vblk_y32(base + VM_QUEUE_NOTIFY, 0);   // RX tamponları hazır → cihaza bildir
    vblk_y32(base + VM_STATUS, 15);        // DRIVER_OK
    ver 0;
}

// MAC config word0 (@ base+0x100, device-PROVIDED) — [8] gerçek-kanıt: cihaz canlı + config erişilir.
çıplak işlev vnet_mac_w0(base: tam64) -> tam32 { ver vblk_o32(base + 256); }
// MAC baytı oku (config @ base+0x100+i) — Ethernet src için.
çıplak işlev vnet_mac_bayt(base: tam64, i: tam32) -> dtam8 { ver vblk_o8(base + 256 + (i olarak tam64)); }

// ====================== Paket TX/RX (FAZ-C net-arp/[9], net-icmp/[10]) ======================
// TX çerçeve tamponuna bayt yaz (12-bayt virtio-net başlığı SONRASI, ofs 12+i).
çıplak işlev vnet_tx_yaz(i: tam32, b: dtam8) { vblk_y8(vnet_dma + NO_TXBUF + 12 + (i olarak tam64), b); }

// Ethernet çerçevesi gönder (TX queue1). uzun = çerçeve baytı (net-başlığı hariç). 0=ok.
çıplak işlev vnet_gonder(base: tam64, uzun: tam32) -> tam32 {
    değişken i: tam32 = 0;
    iken i < 12 { vblk_y8(vnet_dma + NO_TXBUF + (i olarak tam64), 0 olarak dtam8); i = i + 1; }   // net-başlığı sıfır
    değişken d: tam64 = vnet_dma + NO_TXDESC;                          // tek desc, cihaz OKUR
    vblk_y64(d + 0, vnet_dma + NO_TXBUF); vblk_y32(d + 8, (12 + uzun) olarak tam32); vblk_y16(d + 12, 0 olarak dtam16); vblk_y16(d + 14, 0 olarak dtam16);
    değişken ta: tam64 = vnet_dma + NO_TXAVAIL;
    değişken tu: tam64 = vnet_dma + NO_TXUSED + 2;
    değişken idx: dtam16 = vblk_o16(ta + 2);
    değişken once: dtam16 = vblk_o16(tu);
    değişken slot: tam64 = ((idx olarak tam64) % 8) * 2;
    vblk_y16(ta + 4 + slot, 0 olarak dtam16);                          // ring[idx%8]=desc0
    vblk_dsb();
    vblk_y16(ta + 2, (idx + (1 olarak dtam16)) olarak dtam16);
    vblk_dsb();
    vblk_y32(base + VM_QUEUE_NOTIFY, 1);                               // TX queue = 1
    değişken spin: tam32 = 0;
    iken vblk_o16(tu) == once {
        vblk_dsb();
        spin = spin + 1;
        eğer spin > 100000000 { ver 0 - 4; }
    }
    ver 0;
}

// Bir paket AL (RX queue0). Gövde baytını döner (net-başlığı 12 hariç), 0=paket yok/zaman aşımı.
çıplak işlev vnet_al(base: tam64, tikler: tam32) -> tam32 {
    değişken ru: tam64 = vnet_dma + NO_RXUSED;
    değişken spin: tam32 = 0;
    iken vblk_o16(ru + 2) == (vnet_rx_gorulen olarak dtam16) {
        vblk_dsb();
        spin = spin + 1;
        eğer spin > tikler { ver 0; }
    }
    değişken slot: tam32 = vnet_rx_gorulen % 8;
    değişken id: tam32 = vblk_o32(ru + 4 + (slot olarak tam64) * 8);         // used.ring[slot].id
    değişken len: tam32 = vblk_o32(ru + 4 + (slot olarak tam64) * 8 + 4);    // used.ring[slot].len (net-başlığı dâhil)
    vnet_rx_gorulen = vnet_rx_gorulen + 1;
    eğer id >= 8 { ver 0 - 1; }
    vnet_rx_id = id;
    değişken govde: tam32 = len - 12;
    eğer govde < 0 { ver 0; }
    ver govde;
}

// Alınan çerçeveden bayt oku (net-başlığı 12 atlanmış).
çıplak işlev vnet_rx_oku(i: tam32) -> dtam8 { ver vblk_o8(vnet_dma + NO_RXBUF + (vnet_rx_id olarak tam64) * 2048 + 12 + (i olarak tam64)); }

// RFC1071 internet checksum (IPv4 başlığı + ICMP mesajı — net-icmp/[10]). `adr`'den `uzun` bayt
// (big-endian 16-bit toplam + carry fold + one's complement). ~t & 0xffff = 65535 - t (XOR yok).
çıplak işlev vnet_checksum(adr: tam64, uzun: tam32) -> tam32 {
    değişken t: tam32 = 0;
    değişken i: tam32 = 0;
    iken (i + 1) < uzun {
        değişken hi: tam32 = vblk_o8(adr + (i olarak tam64)) olarak tam32;
        değişken lo: tam32 = vblk_o8(adr + ((i + 1) olarak tam64)) olarak tam32;
        t = t + hi * 256 + lo;
        i = i + 2;
    }
    eğer (uzun & 1) != 0 {
        değişken son: tam32 = vblk_o8(adr + ((uzun - 1) olarak tam64)) olarak tam32;
        t = t + son * 256;
    }
    iken (t >> 16) != 0 { t = (t & 65535) + (t >> 16); }
    ver 65535 - (t & 65535);
}

// TX çerçevesindeki `frame_ofs`'tan `uzun` bayt üzerinde RFC1071 checksum (çıplak: vnet_dma
// erişimi güvensiz-tier — normal işlev küresel'e dokunmadan kullanır, E010 kaçınılır).
çıplak işlev vnet_tx_checksum(frame_ofs: tam32, uzun: tam32) -> tam32 {
    ver vnet_checksum(vnet_dma + NO_TXBUF + 12 + (frame_ofs olarak tam64), uzun);
}

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
# Kanıt 4: objdump'da mfence (x86_64 DMA bariyeri) satırları; dsb YOK.
# Kanıt 5: QEMU seri çıktıda "X86 PANIK YOLU OK" tam satırı (kem_panik .kem
# UART yolundan basar; panik geri dönmez, QEMU zaman aşımı keser).
# Kanıt 6: objdump --disassemble-symbols=kem_uart_bayt içinde outb VE inb
# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
		$(BUILD)/bm_x86_start.o $(BUILD)/bm_x86_kesme.o $(BUILD)/bm_x86_zaman.o \
		$(BUILD)/bm_x86_gorev.o $(BUILD)/bm_x86_uart.o $(BUILD)/bm_x86_yazdir.o \
		$(BUILD)/bm_x86_panik.o
	@echo "Bare-metal kem_os x86_64 (saf-.kem allocator): CAT -> LLVM IR..."
	cat runtime/kem_heap.kem runtime/mimari/x86_64/kem_uart.kem runtime/mimari/x86_64/kem_bariyer.kem runtime/kem_virtio_blk.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem
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
	@if ! grep -qxF "X86 DMA BARIYER OK" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 DMA BARIYER OK' satiri yok"; exit 1; fi
	@echo "  Kanit 4: objdump mfence (x86_64 DMA bariyeri)..."
	@if ! llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep -q 'mfence'; then \
		echo "FAIL: mfence objdump'da yok"; exit 1; fi
	@llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep 'mfence'
	@# Komut SÜTUNUNDA ara (sekme + dsb): düz 'dsb' `<vblk_dsb>` sembol adıyla eşleşir.
	@if llvm-objdump -d --no-show-raw-insn $(BUILD)/kem_os_x86.elf | grep -qP '\tdsb\b'; then \
		echo "FAIL: dsb x86_64 binary'de bulundu"; exit 1; fi
	@echo "  Kanit 5: QEMU davranis (X86 PANIK YOLU OK)..."
	@if ! grep -qxF "X86 PANIK YOLU OK" $(BUILD)/kem_os_x86.out; then \
		echo "FAIL: 'X86 PANIK YOLU OK' satiri yok"; exit 1; fi
	@echo "  Kanit 6: objdump kem_uart_bayt (outb + inb, .kem UART)..."
	@if ! llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -q 'outb'; then \
		echo "FAIL: kem_uart_bayt icinde outb yok"; exit 1; fi
	@if ! llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -q 'inb'; then \
		echo "FAIL: kem_uart_bayt icinde inb yok"; exit 1; fi
	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -E 'outb|inb'


```

# GÖREV

## İş 3 — DMA adres haritası (mimari başına) + x86 MMIO penceresi UC eşleme

### Sorun (ölçüldü)
- `vblk_dma = 0x43000000` (1124073472) ve `vnet_dma = 0x46800000`
  (1182793728) aarch64 QEMU `virt` RAM'ine (taban 0x40000000) göre seçilmiş.
  x86 boot'u yalnız 0..1 GB'ı 2 MB sayfalarla eşler (`start_x86_64.S` 2a-2c,
  PD girdileri 0x83 = P|RW|PS, PCD/PWT YOK → önbellekli). İki adres de 1 GB
  DIŞINDA → x86'da sayfa hatası; ayrıca QEMU x86 varsayılan RAM 128 MB.
- x86 MMIO (LAPIC 0xFEE00000, IOAPIC 0xFEC00000, microvm virtio-mmio
  0xFEB00000) 3-4 GB aralığında ve HİÇ eşlenmemiş.
- Ölçülen x86 yerleşim: kernel 0x100000'den, `__heap_end = 0x110F000`
  (~17 MB).

### Karar (verilmiş) — D2b deseni
DMA tabanları mimari başına `runtime/mimari/<arch>/kem_platform.kem`
dosyasına taşınır; Makefile CAT seçer.
- arm64: değerler BİREBİR aynı (davranış değişmez).
- x86_64: `vblk_dma = 0x02000000` (33554432), `vnet_dma = 0x02100000`
  (34603008) — 1 GB eşleme ve 128 MB RAM içinde, heap'in üstünde.
x86 MMIO: `PDPT[3]` → yeni `pd_mmio`, 512 × 2 MB, bayrak **0x9B**
(P|RW|PWT|PCD|PS = önbelleksiz/UC), fiziksel 0xC0000000..0xFFFFFFFF identity.

### DÜZELTME TURU — KAPSAM DARALTILDI
Önceki turda bütçe çapa tartışmasında bitti. Bu turda YALNIZ aşağıdaki
beş madde istenir; CAT listeleri, `kem_virtio_blk/net.kem` küresel satırlarının
silinmesi ve `check_kapisi` düzenlemelerini ORKESTRATÖR yapar — bunlara
blok YAZMA. Çapaları tartışma: aşağıdakiler ölçüldü, her biri TEK kez geçer.

### İstenen değişiklikler (yalnız bunlar)
1. **YENİ `runtime/mimari/arm64/kem_platform.kem`**:
   `küresel değişken vblk_dma: tam64 = 1124073472;` ve
   `küresel değişken vnet_dma: tam64 = 1182793728;`. Türkçe başlık yorumu:
   aarch64 virt RAM 0x40000000 tabanlı; vnet_dma'nın AH-1'de DTB'den uzağa
   taşındığı notu (kem_virtio_net.kem 12-21. satırlardaki gerekçe) kısaca;
   x86 ikizi; D2b.
2. **YENİ `runtime/mimari/x86_64/kem_platform.kem`**: aynı iki küresel,
   değerler 33554432 ve 34603008. Yorum: neden bu adresler (1 GB eşleme + 128
   MB RAM içinde, `__heap_end` 0x110F000 üstünde; çakışma Makefile Kanıt 7
   ile ölçülür), blk ihtiyacı ~800 B, net ~19 KB (NO_TXBUF+2048).
5. **`boot/start_x86_64.S`**:
   - `/* 2d. CR3 = PML4 */` satırından HEMEN ÖNCE yeni adım `2c2`:
     `pd_mmio`'yu 512 girdiyle doldur — düşük dword `0xC000009B + i*0x200000`
     (0xC0000000 tabanı | 0x9B), yüksek dword 0 (2a döngüsünün aynası, `%edi`
     /`%eax`/`%ecx` + `loop`); sonra `PDPT[3]` (`pdpt+24`) = `pd_mmio | 3`,
     `pdpt+28` = 0. Yorum: neden UC (MMIO, önbellek yan etkisi/yazma birleştirme
     olmamalı), hangi cihazlar (LAPIC/IOAPIC/microvm virtio-mmio).
   - `.bss` sayfa tablolarına `pd:     .skip 4096` satırının ALTINA
     `pd_mmio: .skip 4096` ekle (4 KB hizalı bölümde; BSS sıfırlaması kapsar).
   - Dosya başlığındaki adım 2 açıklamasına 3-4 GB UC MMIO penceresini ekle.
7. **`test/ornekler/kem_os_x86.kem`** — `kem_panik("X86 PANIK YOLU OK");`
   satırından ÖNCE üç test (her biri DEĞER denetler, sabit metin basmaz):
   - **Blok DMA RAM:** `vblk_data_yaz(i, ((i % 251) + 3) olarak dtam8)` ile
     i=0..511 yaz, `vblk_data_oku(i)` ile geri oku ve karşılaştır; hepsi
     eşleşirse `X86 DMA RAM BLK OK` bas, yoksa `X86 DMA RAM BLK HATA` +
     ilk uyuşmayan indeks (`yazdir_tam`).
   - **Ağ DMA RAM:** `vnet_tx_yaz(i, ((i % 241) + 7) olarak dtam8)` ile i=0..63
     yaz; geri okumak için bu dosyaya bir ÇIPLAK yardımcı ekle:
     `çıplak işlev x86_net_tx_oku(i: tam32) -> dtam8 { ver vblk_o8(vnet_dma +
     NO_TXBUF + 12 + (i olarak tam64)); }` (küresele normal işlevden doğrudan
     erişim E010 verir; çıplak yardımcı şart). Eşleşirse `X86 DMA RAM NET OK`.
   - **MMIO UC pencere:** `kdl_mmio_oku32(4276092976)` (LAPIC sürüm yazmacı,
     0xFEE00030) oku; değer 0 değil ve -1 değilse `X86 MMIO UC OK
     lapic_surum=` satırı + değeri `yazdir_tam` ile ayrı satırda bas; yoksa
     `X86 MMIO UC HATA`.
   - Başlık yorumuna üç testi ekle.
8. **`Makefile` `calistir_kem_os_x86`** — Kanıt 6 bloğunun son satırından
   (`@llvm-objdump ... --disassemble-symbols=kem_uart_bayt ... | grep -E
   'outb|inb'`) SONRA:
   - **Kanıt 7 (yerleşim, yapısal):** `llvm-nm $(BUILD)/kem_os_x86.elf`
     içinden `__heap_end` değerini al; `0x02000000`'dan KÜÇÜK olmalı (DMA
     heap'le çakışmaz). Değeri yazdır. ⚠ Tarif kabuğu `/bin/sh` (dash):
     onaltılık için `$$((0x$$he))` kullan; `16#` YALNIZ bash'te çalışır
     (D-485 — bu depoda ölçülmüş kusur).
   - **Kanıt 8 (UC bayrağı, gerçek komut):** `llvm-objdump -d
     --no-show-raw-insn $(BUILD)/kem_os_x86.elf` içinde `0xc000009b`
     sabitini taşıyan komut satırı olmalı; satırı OLDUĞU GİBİ yazdır; yoksa
     FAIL.
   - **Kanıt 9 (davranış):** `kem_os_x86.out` içinde TAM satırlar
     `X86 DMA RAM BLK OK`, `X86 DMA RAM NET OK` ve `X86 MMIO UC OK
     lapic_surum=` olmalı (`grep -qxF`); yoksa FAIL.
   - Hedef başlık yorumuna Kanıt 7-9'u ekle (çapa: `# (yalnız .kem işlevi;
     tüm ikilide aramak C UART nesnesine de uyar).`).

### Çapalar (ESKI — AYNEN)
- start_x86_64.S: `    /* 2d. CR3 = PML4 */`
- start_x86_64.S: `pd:     .skip 4096`
- start_x86_64.S: ` *   2. (32-bit) Identity sayfa tabloları (0..1GB, 2MB huge page) + PAE + LME +`
- kem_os_x86.kem: `    kem_panik("X86 PANIK YOLU OK");`
- Makefile: `	@llvm-objdump -d --no-show-raw-insn --disassemble-symbols=kem_uart_bayt $(BUILD)/kem_os_x86.elf | grep -E 'outb|inb'`
- Makefile: `# (yalnız .kem işlevi; tüm ikilide aramak C UART nesnesine de uyar).`

````
