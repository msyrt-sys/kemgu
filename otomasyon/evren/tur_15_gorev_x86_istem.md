# Tur 15 — gorev_x86 — istem

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

## runtime/kem_gorev_cekirdek.kem
```
// kem_gorev_cekirdek.kem — MİMARİDEN BAĞIMSIZ preemptive zamanlayıcı çekirdeği (D-684).
// ============================================================================
// runtime/kem_gorev.kem'den taşındı. Mimariye özgü üç nokta KANCA:
//   kg_cerceve_kur(tepe, giris) -> sp   sentetik trap-frame (arm64 272 B / x86 iretq)
//   kg_alan_sifirla() / kg_alan_gec(i)  görev başına adres alanı (arm64 TTBR0; x86 yok)
// + sabit RAM adresleri (KG_PSP, KG_OLU, KG_G1/G2, KG_AKTIF, KG_STK1/2) ve görev
// giriş adresi yardımcıları (kg_gorev1_adr/2_adr) → runtime/mimari/<arch>/kem_gorev_hw.kem.

// Scheduler durumu — YALNIZ .kem okur/yazar (.S paylaşımı yok) → küresel internal OK.
// Sayaçlar (kem_g1/g2) FİXED RAM: yalnız asm-ile-kullanılan küresel DGE ile silinir →
// fixed-adres çıplak deref (volatile) hem stripping'i hem sonsuz-döngü store-sink'i çözer.
küresel değişken kem_paktif: tam64 = 0;    // o an koşan görev
küresel değişken kem_psayi: tam64 = 0;     // görev sayısı (0=main dahil kem_preempt_baslat'ta 1)

çıplak işlev kg_psp_yaz(i: tam64, v: tam64) { değişken p: *tam64 = (KG_PSP + i * 8) olarak *tam64; *p = v; }
çıplak işlev kg_psp_oku(i: tam64) -> tam64 { değişken p: *tam64 = (KG_PSP + i * 8) olarak *tam64; ver *p; }
çıplak işlev kg_yaz64(adr: tam64, v: tam64) { değişken p: *tam64 = adr olarak *tam64; *p = v; }
çıplak işlev kg_oku64(adr: tam64) -> tam64 { değişken p: *tam64 = adr olarak *tam64; ver *p; }

// main = görev 0; ilk preempt'te gerçek trap-frame SP'si kaydedilir.
çıplak işlev kem_preempt_baslat() {
    kem_paktif = 0;
    kem_psayi = 1;
    kg_yaz64(KG_AKTIF, 0);
    kg_yaz64(KG_G1, 0);          // sayaçları sıfırla (fixed RAM)
    kg_yaz64(KG_G2, 0);
    değişken k: tam64 = 0;
    iken k < 16 { kg_yaz64(KG_OLU + k * 8, 0); k = k + 1; }   // olu[] tablosu temizle
    kg_alan_sifirla();                       // [D-684] mimari kancası (arm64: ttbr[]=0)
}
çıplak işlev kem_preempt_ac() { kg_yaz64(KG_AKTIF, 1); }

// Sentetik trap-frame kur (kdl_preempt_gorev_olustur birebir): 34 slot × 8 = 272 bayt,
// tf@248=ELR=giriş, tf@256=SPSR=0x5 (EL1h IRQ-açık), gerisi 0. SP 16-hizalı. id döner.
// [D-606] GÖREV TABLOSU KAPASİTESİ. olu[]/psp[]/ttbr[] tabloları 16 girdiliktir (sys(14)
// durum zaten `arg >= 16` reddeder) ve olu[]'nun ARKASINDA KG_G1/KG_AKTIF durur. Önceden
// kem_psayi'nın ÜST SINIRI YOKTU: EL0'dan döngüyle sys(12) spawn çağıran bir program
// sayacı büyütüp zamanlayıcının olu[n]/psp[n] yazımlarını çekirdek durum değişkenlerine
// taşırabilirdi. Kapasite doluysa görev KURULMAZ, -1 döner (trap-frame yazılmadan ÖNCE).
sabit KG_GOREV_AZAMI: tam64 = 16;

çıplak işlev kem_gorev_olustur(giris: tam64, yigin_tepe: tam64) -> tam64 {
    eğer kem_psayi >= KG_GOREV_AZAMI { ver 0 - 1; }
    değişken sp: tam64 = kg_cerceve_kur(yigin_tepe, giris);   // [D-684] mimari kancası
    değişken i: tam64 = kem_psayi;
    kg_psp_yaz(i, sp);
    kem_psayi = kem_psayi + 1;
    ver i;
}

// Round-robin scheduler — .kem kdl_irq_isle her timer IRQ'da çağırır. sp = kesilen
// görevin trap-frame SP'si. aktif değilse/tek görevse sp (switch yok → A3 timer nötr).
// kdl_preempt birebir: bitmiş (kg_olu=1) görevler ATLANIR (round-robin sırada tara).
çıplak işlev kem_preempt(sp: tam64) -> tam64 {
    eğer (kg_oku64(KG_AKTIF) == 0) veya (kem_psayi < 2) { ver sp; }
    kg_psp_yaz(kem_paktif, sp);                   // mevcut görevin SP'sini kaydet
    değişken n: tam64 = kem_paktif;
    değişken t: tam64 = 0;
    değişken bulundu: tam64 = 0;
    iken (t < kem_psayi) ve (bulundu == 0) {      // en fazla psayi adım tara, ilk canlıda dur
        n = n + 1;
        eğer n >= kem_psayi { n = 0; }
        eğer kg_oku64(KG_OLU + n * 8) == 0 { kem_paktif = n; bulundu = 1; }
        t = t + 1;
    }
    // MODEL B / MB-1b: seçilen görevin adres alanına geç. ttbr[i]==0 ise Model A
    // davranışı (çekirdek alanı) — geriye uyumlu. Anahtarlama BURADA güvenli:
    // kem_preempt EL1'de, IRQ içinde koşar ve her adres alanı çekirdek RAM'ini
    // AYNI identity eşlemeyle içerir (kmmu_as_olustur L2'yi kopyalar) → bu
    // işlevin kendi kodu/yığını anahtarlamadan SONRA da haritalı kalır.
    kg_alan_gec(kem_paktif);                      // [D-684] mimari kancası (arm64: TTBR0)
    ver kg_psp_oku(kem_paktif);                   // sonraki (canlı) görevin SP'si → .S switch
}

// Aktif görevi BİTİR (izolasyon-ihlali/exit) — kdl_gorev_bitir birebir. Scheduler bir
// daha seçmez (kem_preempt yukarıdaki tarama sırasında atlar).
çıplak işlev kem_gorev_bitir() {
    kg_yaz64(KG_OLU + kem_paktif * 8, 1);
}

// Görev gövdeleri — sonsuz sayaç (preemption ile kesilir; timer IRQ round-robin döndürür).
// Fixed-RAM volatile deref (kg_yaz64/kg_oku64) → store sink EDİLMEZ, main taze görür.
çıplak işlev kem_gorev1() { iken 1 == 1 { kg_yaz64(KG_G1, kg_oku64(KG_G1) + 1); } }
çıplak işlev kem_gorev2() { iken 1 == 1 { kg_yaz64(KG_G2, kg_oku64(KG_G2) + 1); } }

// A4 gate: 2 preemptive görev kur → preemption aç → main spin ederken timer-IRQ
// round-robin ile görevlere geçer → her iki sayaç ilerler (INTERLEAVE) → preemption
// çalışıyor. guard'lı (switch bozuksa hang yok). Sonra preemption'ı KAPAT → main
// [6..10]'a devam eder (görevler artık seçilmez — sonsuz döngüleri terk edilir).
çıplak işlev kem_preempt_testi() -> tam32 {
    kem_preempt_baslat();
    değişken t1: tam64 = kem_gorev_olustur(kg_gorev1_adr(), KG_STK1);
    değişken t2: tam64 = kem_gorev_olustur(kg_gorev2_adr(), KG_STK2);
    kem_preempt_ac();
    değişken guard: tam64 = 0;
    iken ((kg_oku64(KG_G1) < 2) veya (kg_oku64(KG_G2) < 2)) ve (guard < 200000000) {
        guard = guard + 1;
    }
    kg_yaz64(KG_AKTIF, 0);                        // preemption DURDUR → main devam
    eğer (kg_oku64(KG_G1) >= 2) ve (kg_oku64(KG_G2) >= 2) {
        ver 1;                                     // her iki görev de koştu → interleave
    }
    ver 0;
}

```

## runtime/mimari/arm64/kem_gorev_hw.kem
```
// kem_gorev_hw.kem — arm64 görev/zamanlayıcı DONANIM kancaları (D2b, D-684).
// ============================================================================
// Sabitler, TTBR tablosu ve adrp yardımcıları runtime/kem_gorev.kem'den BİREBİR
// taşındı; kancaların gövdeleri özgün kem_gorev_olustur/kem_preempt_baslat/
// kem_preempt satırlarının AYNISIDIR. x86 ikizi: runtime/mimari/x86_64/kem_gorev_hw.kem.


sabit KG_PSP: tam64 = 1157636096;         // 0x45002000: psp[i] tablosu (@+i*8; page-table üstü)
sabit KG_OLU: tam64 = 1157639168;         // 0x45002c00: olu[i] tablosu (kdl_olu[] birebir; 1=bitmiş)
sabit KG_G1: tam64 = 1157640192;          // 0x45003000: görev1 sayacı (FİXED RAM, volatile deref)
sabit KG_G2: tam64 = 1157640200;          // 0x45003008: görev2 sayacı
sabit KG_AKTIF: tam64 = 1157640208;       // 0x45003010: preempt-aktif bayrağı (FİXED RAM — küresel
                                          // olsaydı store1→store0 arası IRQ-okuması clang'a görünmez →
                                          // dead-store-elim ile store1 silinir → asla switch olmaz)
sabit KG_STK1: tam64 = 1192230912;        // 0x47100000: görev1 yığın tepesi (free RAM)
sabit KG_STK2: tam64 = 1193279488;        // 0x47200000: görev2 yığın tepesi

// MODEL B / MB-1b: görev başına adres alanı (TTBR0). ttbr[i] = o görevin L1 PA'sı;
// 0 = "çekirdek alanını miras al" (Model A davranışı — geriye uyumlu).
sabit KG_TTBR: tam64 = 1157640448;        // 0x45003100: ttbr[i] tablosu (KG_SYSC üstü)

çıplak işlev kg_ttbr_yaz(i: tam64, v: tam64) { değişken p: *tam64 = (KG_TTBR + i * 8) olarak *tam64; *p = v; }
çıplak işlev kg_ttbr_oku(i: tam64) -> tam64 { değişken p: *tam64 = (KG_TTBR + i * 8) olarak *tam64; ver *p; }

// Görev giriş adreslerini asm ile materyalize et (.kem işlev-adresi → tam64).
çıplak işlev kg_gorev1_adr() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz { satıriçi_asm { mimari: arm64 şablon: r#"adrp $0, kem_gorev1
add $0, $0, :lo12:kem_gorev1"# çıktı("=r", &x) } }
    ver x;
}
çıplak işlev kg_gorev2_adr() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz { satıriçi_asm { mimari: arm64 şablon: r#"adrp $0, kem_gorev2
add $0, $0, :lo12:kem_gorev2"# çıktı("=r", &x) } }
    ver x;
}

// Sentetik trap-frame kur (özgün kem_gorev_olustur gövdesi): 34 slot × 8 = 272 bayt,
// tf@248=ELR=giriş, tf@256=SPSR=0x5 (EL1h IRQ-açık), gerisi 0. SP 16-hizalı.
çıplak işlev kg_cerceve_kur(yigin_tepe: tam64, giris: tam64) -> tam64 {
    değişken sp: tam64 = yigin_tepe - 272;
    sp = (sp / 16) * 16;                          // 16-hizala
    değişken k: tam64 = 0;
    iken k < 34 { kg_yaz64(sp + k * 8, 0); k = k + 1; }   // 34 slot sıfırla
    kg_yaz64(sp + 248, giris);                    // ELR_EL1 @248 = eret hedefi
    kg_yaz64(sp + 256, 5);                        // SPSR @256 = 0x5 (EL1h, IRQ açık)
    ver sp;
}
// ttbr[] = 0 (çekirdek alanı) — özgün kem_preempt_baslat döngüsü.
çıplak işlev kg_alan_sifirla() {
    değişken j: tam64 = 0;
    iken j < 16 { kg_ttbr_yaz(j, 0); j = j + 1; }             // ttbr[] = 0 (çekirdek alanı)
}
// Seçilen görevin adres alanına geç — özgün kem_preempt satırları.
çıplak işlev kg_alan_gec(i: tam64) {
    değişken yeni: tam64 = kg_ttbr_oku(i);
    eğer yeni == 0 { yeni = KMMU_L1; }
    kmmu_ttbr_yaz(yeni);
}

```

## boot/start_x86_64.S — KESİT (satır 243-272; düzenleme hedefi: `boot/start_x86_64.S`, blok yolu `boot/start_x86_64.S` olmalı)
```
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
```

# GÖREV

## İş 15 — x86 preemptive görev değiştirme: IRQ0 koçanı SP-takası + x86 görev kancaları

Zamanlayıcı çekirdeği `runtime/kem_gorev_cekirdek.kem` (bağlamda) mimariden
bağımsız; arm64 kancaları `runtime/mimari/arm64/kem_gorev_hw.kem` (bağlamda, ÖRNEK).
Model: her timer IRQ'da `.S` koçanı TÜM GP yazmaçlarını yığına iter,
`kdl_irq_isle(sp)` çağırır, dönen SP'ye geçer (başka görevin çerçevesi),
yazmaçları geri alır, `iretq`. Yeni görev = SENTETİK çerçeve.

### ÇERÇEVE DÜZENİ (orkestratör kararı — iki parça BUNA uymak ZORUNDA)
İtme sırası: `rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15`
(15 × 8 = 120 B). İtmeden sonra SP = r15 slotu. SP'den yukarı:
`[0]=r15 … [14]=rax` (ofset 0..112), `[15]=RIP @120`, `[16]=CS @128`,
`[17]=RFLAGS @136`, `[18]=RSP @144`, `[19]=SS @152` (CPU'nun ittiği iretq
çerçevesi). Toplam 160 B. CPU kesmede RSP'yi 16'ya hizalar → 5 itme (40 B) +
15 itme (120 B) = 160 → `call` öncesi SP 16-hizalı (SysV) ✓.

### Değişiklik 1 — `boot/start_x86_64.S` IRQ0 koçanı
Bağlamdaki `/* === IRQ0 (timer) stub (C3b) … */` yorumundan `iretq`'ya kadar olan
bloğun TAMAMINI (ÇAPA = o blok, birebir) şununla değiştir:
- Yorum: tam kayıt + SP takası (D-685); C kernel'leri için uyumluluk (aşağıdaki
  zayıf varsayılan SP'yi değiştirmez → davranış AYNI).
- `.global kdl_irq0_stub` / `kdl_irq0_stub:` → 15 `pushq` (yukarıdaki sırayla) →
  `movq $0, %rdi` / `call kdl_kesme_isle` (tik; C ya da .kem) →
  `movq %rsp, %rdi` / `call kdl_irq_isle` / `movq %rax, %rsp` (SP takası) →
  PIC EOI (`movb $0x20, %al` / `outb %al, $0x20`) → 15 `popq` (TERS sıra) → `iretq`.
- Hemen ardından ZAYIF varsayılan: `.weak kdl_irq_isle` / `kdl_irq_isle:` /
  `movq %rdi, %rax` / `ret` — yorum: `.kem` x86 güçlü tanımı (kem_preempt) ezer;
  C kernel'lerinde SP değişmez.

### Değişiklik 2 — YENİ `runtime/mimari/x86_64/kem_gorev_hw.kem`
Türkçe başlık (D2b, arm64 ikizi, çerçeve düzeni). İçerik:
- Sabitler (DOĞRULANMIŞ, ondalık; x86 RAM 48-50 MB — heap 17 MB üstü, DMA ~127 MB altı):
  `KG_PSP = 50331648` (0x03000000), `KG_OLU = 50334720` (0x03000C00),
  `KG_G1 = 50335744`, `KG_G2 = 50335752`, `KG_AKTIF = 50335760`,
  `KG_STK1 = 51380224` (0x03100000), `KG_STK2 = 52428800` (0x03200000). Hepsi `tam64`.
- `kg_gorev1_adr() -> tam64` / `kg_gorev2_adr()`: `satıriçi_asm { mimari: x86_64
  şablon: r#"leaq kem_gorev1(%rip), $0"# çıktı("=r", &x) }` (ve kem_gorev2).
- `kg_cerceve_kur(yigin_tepe: tam64, giris: tam64) -> tam64`:
  `ust = (yigin_tepe / 16) * 16`; `sp = ust - 160`; 0..14 slotları sıfırla
  (`kg_yaz64(sp + k*8, 0)`); `sp+120 = giris`; `sp+128 = 8` (CS); `sp+136 = 514`
  (RFLAGS 0x202, IF=1); `sp+144 = ust - 8` (görev RSP; `call` sonrası hizası);
  `sp+152 = 16` (SS); `ver sp;`
- `kg_alan_sifirla()` ve `kg_alan_gec(i: tam64)`: boş gövde (x86 ara hedefinde
  görev başına adres alanı yok; yorumla belirt).
- `kdl_irq_isle(sp: tam64) -> tam64 { ver kem_preempt(sp); }` — koçanın güçlü
  tanımı (arm64'te karşılığı kem_zaman_hw.kem'deki kdl_irq_isle).
asm'ler `güvensiz { }` içinde.

Blok yolları yalnız: `boot/start_x86_64.S`, `runtime/mimari/x86_64/kem_gorev_hw.kem`.

````
