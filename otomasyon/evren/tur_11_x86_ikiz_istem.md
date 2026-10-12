# Tur 11 — x86_ikiz — istem

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

## test/baremetal_diff_harness.sh
```
#!/usr/bin/env bash
# ============================================================================
# baremetal_diff_harness.sh — BARE-METAL / ARM64 codegen kapısı (D-418).
# ----------------------------------------------------------------------------
# NEDEN AYRI BİR KAPI: `runtime/*.kem` (OS/sürücü kodu) host'ta LİNKLENMEZ —
# aarch64 sysreg/MMIO içerir. Exit-kodu karşılaştırması imkânsız, bu yüzden
# davranışsal kapıların HEPSİ bu yüzeyi atlıyordu ve **self-host `--mimari`
# bayrağını hiç tanımadığı için bare-metal kodu DERLEYEMİYORDU BİLE** — bunu
# hiçbir kapı görmüyordu (ölçüldü: 0 define üretiyordu).
#
# Linkleyemediğimiz için DAVRANIŞ değil YAPI karşılaştırılır: emit edilen
# `define` kümesi (ad + DÖNÜŞ TİPİ) C oracle ile birebir mi. Bu, eksik işlevi,
# yanlış adı ve yanlış dönüş tipini yakalar — D-418'de üçü de gerçekten çıktı
# (void işlevler `i32` olarak yayılıyordu).
#
# ⚠ Dönüş tipini KESMEYİN: `sed 's/(.*//'` imzayı atar ama `define void @f` ile
# `define i32 @f` ayrımını KORUR — kusur tam da oradaydı.
#
# Kullanım: bash test/baremetal_diff_harness.sh (veya make calistir_baremetal_diff)
# ============================================================================
set -u
# [D-469] EXE uzantisi: Makefile `export EXE` ile gelir. Dogrudan cagrimda
# (make'siz) TANIMSIZ olurdu ve `set -u` altinda harness COKERDI -> ikilinin
# varligindan TESPIT et. Windows: .exe, Linux/macOS: bos.
: "${EXE=$(test -x build/kemgu.exe && echo .exe)}"
KEMGU=${KEMGU:-build/kemgu${EXE}}
CODEGEN=${CODEGEN:-build/codegen${EXE}}
# [D-562] GECICI DIZIN DEPO-GORELI. `/tmp` KULLANILAMAZ: Windows'ta
# recipe kabugu (Git-for-Windows sh) ile MSYS2 araclari (diff, cmp)
# AYRI `/tmp` baglamalari cozer -> ayni dizgi iki farkli gercek dizine
# isaret eder ve dosya 'yok' gorunur. D-561'de olculdu: `[ -f ]` VAR
# derken `diff` 'No such file' diyordu ve bu 'STDOUT farkli' diye
# YANLIS ATFEDILIYORDU. build/ zaten .gitignore'da.
TMP=$(mktemp -d "build/bmdiff.XXXXXX" 2>/dev/null || echo "build/bmdiff.$$")
mkdir -p "$TMP"

if [ ! -x "$CODEGEN" ]; then
# [D-486] EKSIK IKILI = HATA, ATLAMA DEGIL. Bu kapinin make hedefi
# `$(BUILD)/codegen$(EXE)`e BAGIMLIDIR -> make uzerinden ikili GARANTI.
# Yani bu dal make yolunda OLU KOD; tek islevi bir YOL HATASINI SESSIZCE
# YUTMAKTI. Olculdu: Makefile `CODEGEN=build/codegen.exe` diye SABIT
# geciyordu -> Linux'ta ikilinin adi `build/codegen` -> SEKIZ parite
# kapisi birden atlandi ve `make` yine 0 dondu (tam takim YESIL gorundu).
# D-446'nin sinifi: var olan ama kosmayan kapi, olmayandan tehlikelidir.
    echo "🔴 HATA: codegen ikilisi YOK ($CODEGEN) — kapı KOŞMADI"
    exit 1
fi

pass=0; fail=0
# ============================================================================
# [D-675] "ATLANDI" KALDIRILDI — derlenemeyen birim KIRMIZIDIR.
# Ölçüldü (2026-10-12, onarım ÖNCESİ): birleşik OS biriminden bir parça
# düşürülünce (T002) ya da bir parça dosyasına tip hatası sokulunca kapı
# `rc=0` + "2/2 birim (9 atlandı)" verdi — çekirdeğin TAMAMI ölçülmeden YEŞİL.
# ZERO-C audit'teki "pure-.kem" körlüğüyle aynı sınıf: kapı geçiyor ama hiçbir
# şey ölçmüyor (D-446/D-486). Yeni kurallar:
#   (1) birleşik OS birimi derlenemezse / parçası eksikse → KIRMIZI
#   (2) BIRIMLER tablosundaki her birim (dosya + mimari bağımlılıkları)
#       derlenemezse → KIRMIZI
#   (3) PARCALAR: tek başına derlenemeyen dosyalar — her birinin TÜM işlev ve
#       küresel sembolleri birleşik birimin C VE KEMGU IR'ında bulunmalı
#       (SEMBOL SAYIMI, iddia değil). Eksik → KIRMIZI. Parça tek başına
#       derleniyorsa → KIRMIZI (bayat sınıflandırma, D-534: BIRIMLER'e taşı).
#   (4) runtime/*.kem + runtime/mimari/arm64/*.kem içindeki HER dosya (2) ya
#       da (3)'te olmalı; sınıflandırılmamış dosya → KIRMIZI.
# Gerekçeler: docs/BAREMETAL_DIFF_KAPSAM.md
# ============================================================================

# ---- (1) BİRLEŞİK OS BİRİMİ — asıl kapsam ----
# Makefile `calistir_kem_os_arm` cat sırasıyla AYNI olmalı.
OS_PARCA="runtime/kem_heap.kem runtime/mimari/arm64/kem_uart.kem runtime/kem_mmu.kem runtime/kem_gorev.kem \
runtime/kem_zaman.kem runtime/mimari/arm64/kem_bariyer.kem runtime/mimari/arm64/kem_platform.kem runtime/kem_virtio_blk.kem runtime/kem_minifs.kem \
runtime/kem_virtio_net.kem runtime/kem_elf.kem runtime/kem_dtb.kem \
test/ornekler/kem_os.kem"
os_ok=0
os_eksik=""
for parca in $OS_PARCA; do [ -f "$parca" ] || os_eksik="$os_eksik $parca"; done
if [ -n "$os_eksik" ]; then
    echo "  🔴 BİRLEŞİK OS — parça dosyası YOK:$os_eksik"
    fail=$((fail+1))
else
    cat $OS_PARCA > "$TMP/os.kem"
    "$KEMGU" --llvm --mimari aarch64 "$TMP/os.kem" > "$TMP/os_c.ll" 2> "$TMP/os_c.err"; orc=$?
    "$CODEGEN" --llvm --mimari aarch64 "$TMP/os.kem" > "$TMP/os_s.ll" 2>/dev/null
    if [ "$orc" -ne 0 ] || head -1 "$TMP/os_c.ll" 2>/dev/null | grep -q "hata\[" || [ ! -s "$TMP/os_c.ll" ]; then
        echo "  🔴 BİRLEŞİK OS — C oracle DERLEYEMEDİ (rc=$orc) — birim ÖLÇÜLEMEDİ:"
        grep -m3 "hata\[" "$TMP/os_c.err" "$TMP/os_c.ll" 2>/dev/null | sed 's/^/      /'
        fail=$((fail+1))
    elif [ ! -s "$TMP/os_s.ll" ]; then
        echo "  🔴 BİRLEŞİK OS — KEMGU codegen IR üretemedi"
        fail=$((fail+1))
    else
        grep "^define" "$TMP/os_c.ll" | sed 's/(.*//' | sort > "$TMP/os_c.d"
        grep "^define" "$TMP/os_s.ll" | sed 's/(.*//' | sort > "$TMP/os_s.d"
        oc=$(grep -m1 "target triple" "$TMP/os_c.ll"); os=$(grep -m1 "target triple" "$TMP/os_s.ll")
        # satıriçi asm SAYISI da denetlenir: asm bloğu sessizce düşerse (D-416'nın
        # kusuru) define kümesi DEĞİŞMEZ — yalnız ad karşılaştırmak onu KAÇIRIR.
        ca=$(grep -c "asm sideeffect" "$TMP/os_c.ll"); sa=$(grep -c "asm sideeffect" "$TMP/os_s.ll")
        # [D-604] HAM İŞARETÇİ + VOLATILE paritesi. Eşitlik tek başına YETMEZ:
        # iki derleyici birlikte volatile'ı bıraksa 0=0 geçerdi → C > 0 da şart.
        cv=$(grep -cE "load volatile|store volatile" "$TMP/os_c.ll"); sv=$(grep -cE "load volatile|store volatile" "$TMP/os_s.ll")
        ci=$(grep -c "inttoptr" "$TMP/os_c.ll"); si=$(grep -c "inttoptr" "$TMP/os_s.ll")
        if [ "$oc" = "$os" ] && diff -q "$TMP/os_c.d" "$TMP/os_s.d" >/dev/null 2>&1 && [ "$ca" -eq "$sa" ] \
           && [ "$cv" -gt 0 ] && [ "$cv" -eq "$sv" ] && [ "$ci" -gt 0 ] && [ "$ci" -eq "$si" ]; then
            echo "  ✅ BİRLEŞİK OS — $(wc -l <"$TMP/os_c.d") işlev, $ca satıriçi asm, $cv volatile, $ci inttoptr, üçlü eşleşti"
            pass=$((pass+1)); os_ok=1
        else
            echo "  🔴 BİRLEŞİK OS — işlev C=$(wc -l <"$TMP/os_c.d") KEMGU=$(wc -l <"$TMP/os_s.d") · asm C=$ca KEMGU=$sa · volatile C=$cv KEMGU=$sv · inttoptr C=$ci KEMGU=$si"
            [ "$oc" = "$os" ] || echo "      üçlü: C=[$oc] KEMGU=[$os]"
            diff "$TMP/os_c.d" "$TMP/os_s.d" 2>/dev/null | head -5 | sed 's/^/      /'
            fail=$((fail+1))
        fi
    fi
fi

# ---- (2) BİRİMLER — tek başına ya da MİMARİ BAĞIMLILIKLARIYLA derlenen ----
# Biçim: ad|ilk dosya birimin SAHİBİ, sonrakiler aarch64 bağımlılıkları.
# kem_heap ve kem_virtio_blk D-671/D-672'de mimari başına dosyalara (D2b)
# bağlandığı için TEK BAŞINA derlenmez oldu; bağımlılıklarıyla birim olarak
# GERİ ALINDI (onarım öncesi sessizce "atlandı"ya düşmüşlerdi).
# [D-663] PER-BİRİM ZAMAN AŞIMI + BELLEK TAVANI (G15) — self-host ikilisi korpus
# üzerinde koşar; `timeout` yeteneği ÖLÇÜLÜR (D-661).
KAP_SN=${KAP_SN:-60}
KAP_KB=${KAP_KB:-4000000}
if command -v timeout >/dev/null 2>&1 && timeout 5 true >/dev/null 2>&1; then
    GTO="timeout $KAP_SN"
else
    GTO=""
    echo "  ⚠ 'timeout' YOK -> per-birim zaman asimi UYGULANMIYOR (kapi yine olcer). Bkz. D-663."
fi
ir_uret() { o="$1"; shift; ( ulimit -v "$KAP_KB" 2>/dev/null; $GTO "$@" > "$o" 2>/dev/null ); }

BIRIMLER="kem_dtb|runtime/kem_dtb.kem
kem_kanal_abi|runtime/kem_kanal_abi.kem
kem_heap|runtime/kem_heap.kem runtime/mimari/arm64/kem_uart.kem
kem_bariyer|runtime/mimari/arm64/kem_bariyer.kem
kem_virtio_blk|runtime/kem_virtio_blk.kem runtime/mimari/arm64/kem_bariyer.kem runtime/mimari/arm64/kem_platform.kem"

birim_sahipleri=""
while IFS='|' read -r b dosyalar; do
    [ -n "$b" ] || continue
    birim_sahipleri="$birim_sahipleri ${dosyalar%% *}"
    eksik=""; for d in $dosyalar; do [ -f "$d" ] || eksik="$eksik $d"; done
    if [ -n "$eksik" ]; then echo "  🔴 $b — dosya YOK:$eksik"; fail=$((fail+1)); continue; fi
    cat $dosyalar > "$TMP/$b.kem"
    "$KEMGU" --llvm --mimari aarch64 "$TMP/$b.kem" > "$TMP/c.ll" 2> "$TMP/c.err"; crc=$?
    if [ "$crc" -ne 0 ] || head -1 "$TMP/c.ll" 2>/dev/null | grep -q "hata\[" || [ ! -s "$TMP/c.ll" ]; then
        echo "  🔴 $b — C oracle DERLEYEMEDİ (rc=$crc) — birim ÖLÇÜLEMEDİ:"
        grep -m3 "hata\[" "$TMP/c.err" "$TMP/c.ll" 2>/dev/null | sed 's/^/      /'
        fail=$((fail+1)); continue
    fi
    ir_uret "$TMP/s.ll" "$CODEGEN" --llvm --mimari aarch64 "$TMP/$b.kem"; sir=$?
    if [ "$sir" -eq 124 ] || [ "$sir" -eq 137 ]; then
        echo "  🔴 $b — KEMGU IR URETIMI ASILDI/OLDURULDU (rc=$sir)"; fail=$((fail+1)); continue
    fi
    [ -s "$TMP/s.ll" ] || { echo "  🔴 $b — KEMGU codegen IR üretemedi"; fail=$((fail+1)); continue; }
    # Hedef üçlüsü de karşılaştırılır: --mimari yok sayılırsa burası yakalar.
    ct=$(grep -m1 "target triple" "$TMP/c.ll"); st=$(grep -m1 "target triple" "$TMP/s.ll")
    if [ "$ct" != "$st" ]; then
        echo "  🔴 $b — hedef üçlüsü farklı: C=[$ct] KEMGU=[$st]"; fail=$((fail+1)); continue
    fi
    grep "^define" "$TMP/c.ll" | sed 's/(.*//' | sort > "$TMP/c.d"
    grep "^define" "$TMP/s.ll" | sed 's/(.*//' | sort > "$TMP/s.d"
    if diff -q "$TMP/c.d" "$TMP/s.d" >/dev/null 2>&1; then
        echo "  ✅ $b — $(wc -l <"$TMP/c.d") işlev ($(echo $dosyalar | wc -w) dosya)"
        pass=$((pass+1))
    else
        echo "  🔴 $b — define kümesi farklı (C=$(wc -l <"$TMP/c.d") KEMGU=$(wc -l <"$TMP/s.d")):"
        diff "$TMP/c.d" "$TMP/s.d" 2>/dev/null | head -5 | sed 's/^/      /'
        fail=$((fail+1))
    fi
done <<EOF
$BIRIMLER
EOF

# ---- (3) PARÇALAR — tek başına derlenemeyen; kapsam BİRLEŞİK birimde ÖLÇÜLÜR ----
# Her parçanın dosyadan okunan TÜM `işlev` ve `küresel değişken` adları
# birleşik birimin C VE KEMGU IR'ında tanımlı olmalı (`define ... @ad(` /
# `@ad = `). `sabit`ler IR'da sembol bırakmaz (satır içi) → sayılamaz.
# Gerekçeler (neden tek başına derlenemiyor): docs/BAREMETAL_DIFF_KAPSAM.md
PARCALAR="runtime/kem_elf.kem runtime/kem_gorev.kem runtime/kem_minifs.kem
runtime/kem_mmu.kem runtime/kem_virtio_net.kem runtime/kem_zaman.kem
runtime/mimari/arm64/kem_uart.kem runtime/mimari/arm64/kem_platform.kem
test/ornekler/kem_os.kem"
kapsam_islev=0; kapsam_kuresel=0
for f in $PARCALAR; do
    b=$(basename "$f" .kem)
    [ -f "$f" ] || { echo "  🔴 parça $f — dosya YOK"; fail=$((fail+1)); continue; }
    # Bayat sınıflandırma: tek başına derleniyorsa parça DEĞİL → BIRIMLER'e taşı.
    if "$KEMGU" --llvm --mimari aarch64 "$f" > "$TMP/p.ll" 2>/dev/null && ! head -1 "$TMP/p.ll" | grep -q "hata\[" \
       && grep -q "^define" "$TMP/p.ll"; then
        echo "  🔴 parça $f — TEK BAŞINA DERLENİYOR: bayat sınıflandırma, BIRIMLER'e taşı (D-534)"
        fail=$((fail+1)); continue
    fi
    if [ "$os_ok" -ne 1 ]; then
        echo "  🔴 parça $f — birleşik birim ölçülemedi → kapsamı DOĞRULANAMADI"
        fail=$((fail+1)); continue
    fi
    adlar=$(grep -ohE '^\s*(çıplak\s+|genel\s+|gerçekzamanlı\s+)*işlev\s+[A-Za-z_][A-Za-z_0-9]*' "$f" | sed -E 's/.*işlev\s+//' | sort -u)
    kureseller=$(grep -ohE '^\s*küresel\s+değişken\s+[A-Za-z_][A-Za-z_0-9]*' "$f" | sed -E 's/.*değişken\s+//' | sort -u)
    n_i=$(printf '%s\n' "$adlar" | grep -c .); n_k=$(printf '%s\n' "$kureseller" | grep -c .)
    eksik=""
    for a in $adlar; do
        grep -qE "^define [^@]*@$a\(" "$TMP/os_c.ll" || eksik="$eksik C:@$a"
        grep -qE "^define [^@]*@$a\(" "$TMP/os_s.ll" || eksik="$eksik KEMGU:@$a"
    done
    for k in $kureseller; do
        grep -qE "^@$k = " "$TMP/os_c.ll" || eksik="$eksik C:@$k"
        grep -qE "^@$k = " "$TMP/os_s.ll" || eksik="$eksik KEMGU:@$k"
    done
    if [ $((n_i + n_k)) -eq 0 ]; then
        echo "  🔴 parça $f — ölçülecek sembol YOK (sayım 0 → kapsam iddiası BOŞ)"
        fail=$((fail+1))
    elif [ -n "$eksik" ]; then
        echo "  🔴 parça $f — birleşik birimde EKSİK:$(echo $eksik | cut -c1-200)"
        fail=$((fail+1))
    else
        echo "  ✅ parça $b — $n_i işlev + $n_k küresel birleşik birimde (C + KEMGU)"
        pass=$((pass+1)); kapsam_islev=$((kapsam_islev + n_i)); kapsam_kuresel=$((kapsam_kuresel + n_k))
    fi
done

# ---- (4) SINIFLANDIRMA BÜTÜNLÜĞÜ — sessiz dışarıda kalma YOK ----
# runtime/mimari/x86_64/*.kem bu kapının dışında (aarch64 hedefi AS001 verir);
# onlar check_kapisi (birleşik x86, --mimari x86_64) + calistir_kem_os_x86'da.
for f in runtime/*.kem runtime/mimari/arm64/*.kem; do
    [ -f "$f" ] || continue
    case " $birim_sahipleri $(echo $PARCALAR) " in
        *" $f "*) ;;
        *) echo "  🔴 $f — SINIFLANDIRILMAMIŞ: BIRIMLER ya da PARCALAR'a ekle"; fail=$((fail+1)) ;;
    esac
done

echo "=== bare-metal (aarch64) yapı paritesi: $pass/$((pass+fail)) (birleşik OS + birimler + parça kapsamı: $kapsam_islev işlev, $kapsam_kuresel küresel), 0 atlandı ==="
[ "$fail" -eq 0 ]

```

## Makefile — KESİT (satır 1956-1960; düzenleme hedefi: `Makefile`, blok yolu `Makefile` olmalı)
```
#          satır yedek sabitten (32768 KB) BÜYÜK — PVH memmap RAM tepesinden
#          türetilen DMA tabanı (kem_platform.kem). Değer de yazdırılır.
calistir_kem_os_x86: $(BUILD)/kemgu$(EXE) \
		$(BUILD)/bm_x86_start.o $(BUILD)/bm_x86_kesme.o $(BUILD)/bm_x86_zaman.o \
		$(BUILD)/bm_x86_gorev.o $(BUILD)/bm_x86_uart.o $(BUILD)/bm_x86_yazdir.o \
```

# GÖREV

## İş 11 — `baremetal_diff`'e x86 İKİZİ: birleşik x86 birimi C ↔ self-host yapısal parite

Ölçüldü (bugün, elle): x86 birleşik birimi `--mimari x86_64` ile C oracle ve
self-host'ta BİREBİR — 72 define (ad+dönüş tipi), 4 asm (şablon+kısıt dizgisi
aynı), 79 volatile, 83 inttoptr, üçlü aynı. Ama bunu HİÇBİR kapı ölçmüyor.
Yalnız `test/baremetal_diff_harness.sh` değişir (bağlamda tamamı var).

### Değişiklikler (3 madde)
1. **Yeni bölüm (5)** — `# ---- (4) SINIFLANDIRMA BÜTÜNLÜĞÜ ...` satırından HEMEN ÖNCE ekle:
   - Parça listesi TEK KAYNAKTAN: Makefile'daki x86 CAT satırından oku:
     `X86_PARCA=$(sed -n 's/.*cat \(runtime\/kem_heap\.kem.*kem_os_x86\.kem\) > \$(BUILD)\/kem_os_x86_comb\.kem/\1/p' Makefile)`
     Boşsa → `🔴 BİRLEŞİK x86 — Makefile'dan parça listesi okunamadı`, fail++.
     Listedeki dosyalardan biri yoksa → 🔴 + fail++.
   - `cat $X86_PARCA > "$TMP/x86.kem"`; C: `"$KEMGU" --llvm --mimari x86_64` (stderr
     `$TMP/x86_c.err`, rc al); self: `"$CODEGEN" --llvm --mimari x86_64`.
     C rc≠0 / ilk satırda `hata[` / boş → `🔴 BİRLEŞİK x86 — C oracle DERLEYEMEDİ`
     (+ ilk 3 `hata[` satırı), fail++ (ATLAMA YOK). Self boş → 🔴, fail++.
   - Karşılaştır (bölüm (1) ile aynı yöntem): define kümesi (`sed 's/(.*//' | sort`),
     üçlü, `asm sideeffect` sayısı, volatile sayısı (C > 0 şart), inttoptr sayısı
     (C > 0 şart) VE ek olarak asm şablon+kısıt kümesi:
     `grep -o 'asm sideeffect "[^"]*", "[^"]*"' | sort` iki tarafta `diff -q` eşit.
   - Geçerse `✅ BİRLEŞİK x86 — N işlev, A satıriçi asm (şablon+kısıt eşleşti), V volatile, I inttoptr, üçlü eşleşti`,
     pass++; değilse ayrıntılı 🔴 (ilk 5 diff satırı), fail++.
   - Bölüm başına Türkçe yorum: neden (D-674 sonrası x86 birimi var ama C↔self
     paritesini ölçen kapı yoktu), parça listesinin Makefile'dan okunması
     (liste kayması olmasın), üçlünün bugün `x86_64-pc-windows-gnu` olduğu
     (D1 açık; eşitlik yine ölçülür).
2. **Bölüm (4) sınıflandırma** — `runtime/mimari/x86_64/*.kem` dosyaları da
   denetlensin: her biri `$X86_PARCA` içinde geçmeli; geçmeyen → `🔴 <dosya> —
   SINIFLANDIRILMAMIŞ: Makefile calistir_kem_os_x86 CAT listesine ekle`, fail++.
   Bölüm (4)'ün başındaki "x86_64 bu kapının dışında" yorumunu buna göre güncelle.
   ÇAPA (3 satır, birebir):
   `# runtime/mimari/x86_64/*.kem bu kapının dışında (aarch64 hedefi AS001 verir);`
   `# onlar check_kapisi (birleşik x86, --mimari x86_64) + calistir_kem_os_x86'da.`
   `for f in runtime/*.kem runtime/mimari/arm64/*.kem; do`
   (YENI'de mevcut arm64 döngüsü AYNEN kalsın; x86 denetimi ayrı küçük döngü olarak
   onun ARDINA — yani mevcut döngünün `done` satırından sonra — gelsin. Bunu
   yapabilmek için çapayı döngünün tamamını kapsayacak şekilde genişletme;
   x86 döngüsünü bölüm (5)'in SONUNA koyman da kabul.)
3. **Özet satırı**: `(aarch64)` → `(aarch64 + x86_64)`.
   ÇAPA: `echo "=== bare-metal (aarch64) yapı paritesi: $pass/$((pass+fail)) (birleşik OS + birimler + parça kapsamı: $kapsam_islev işlev, $kapsam_kuresel küresel), 0 atlandı ==="`

Blok yolu YALNIZ `test/baremetal_diff_harness.sh`. bash, `set -u` altında çalışır.

````
