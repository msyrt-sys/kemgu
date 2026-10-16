# Tur 1 — kem_heap_kur — istem

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

## linker/bare-metal-x86_64.ld
```
/*
 * KEMGU Bare-Metal Linker Script — x86_64 (PVH boot)
 * ==================================================
 *
 * Yük adresi 0x100000 (1 MB) — PVH/multiboot geleneği. QEMU `-kernel` 64-bit
 * ELF'i PVH note ile yükler, pvh_entry (32-bit) çalışır → long mode → main.
 *
 * Bölümler: .note.Xen (PVH note, başta) / .text / .rodata / .data / .bss
 * (sayfa tabloları dahil) / 16KB stack / 16MB heap (frame allocator backing).
 * aarch64 muadili linker/bare-metal-aarch64.ld.
 */

ENTRY(pvh_entry)

SECTIONS
{
    . = 0x100000;            /* 1 MB */

    .note.Xen : ALIGN(4) {
        *(.note.Xen)         /* PVH boot note — yüklü imajın başında olmalı */
    }

    .text : ALIGN(16) {
        *(.text._start)
        *(.text*)
        *(.text.*)
    }

    .rodata : ALIGN(16) {
        __rodata_start = .;      /* D-151: syscall okuma-pointer doğrulama sınırı (arch paritesi) */
        *(.rodata*)
        *(.rodata.*)
        __rodata_end = .;
    }

    .data : ALIGN(16) {
        *(.data*)
        *(.data.*)
    }

    .bss : ALIGN(16) {
        __bss_start = .;
        *(.bss*)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(16);
    . = . + 0x4000;          /* 16 KB stack */
    __stack_top = .;

    /* Heap (C1: bölge/dizi tahsisleri) — frame allocator backing. */
    . = ALIGN(4096);
    __heap_start = .;
    . = . + 0x1000000;       /* 16 MB */
    __heap_end = .;

    /DISCARD/ : {
        *(.comment*)
        *(.eh_frame*)
        *(.note.GNU-stack)
    }
}

```

## runtime/kdl_bare_heap.c
```
/*
 * KEMGU Bare-Metal Heap — freestanding malloc/free + memcpy/memset + global bölge
 * ===============================================================================
 *
 * C1 (OS region backing): host libc YOK. Bölge runtime'ı (kdl_bolge.c) ve
 * codegen-emit ham @malloc (bölge_al / closure env) bare-metal'de bu dosyadaki
 * sembollere link olur. Backing = linker script'in rezerve ettiği fiziksel RAM
 * bölgesi (__heap_start .. __heap_end) üzerinde bump + serbest-liste.
 *
 * Model: her tahsis 16-hizalı, başında {boyut, sonraki} header. free() bloğu
 * serbest-listeye iter; malloc() önce ilk-uyan serbest bloğu arar — region 64KB
 * blokları aynı boyutta olduğundan kernel-loop'ta F4.3 per-iter region-free
 * frame'leri geri kazandırır (= sınırlı bellek). Uyan blok yoksa bump'lar.
 *
 * Sınırlar (v1, bilinçli): split/coalesce YOK (ilk-uyum bloğu bütün döner →
 * boyut farkı iç-fragmentasyon; aynı-boyut geri kazanım kernel-loop için
 * yeterli). Tek-thread (concurrency Katman 2). bireysel free bölge modelinde
 * zaten gerekmez — kdl_bolge_serbest blokları + ham @malloc'u karşılar.
 *
 * CODEGEN'E DOKUNULMAZ: üretilen IR aynı @kdl_bolge_olustur/@kdl_bolge_serbest/
 * @kdl_global_bolge_al/@malloc sembollerini çağırır; yalnız backing değişir.
 */
#include <stdint.h>
#include <stddef.h>
#include "kdl_bolge.h"

/* Linker script (linker/bare-metal-*.ld) heap bölgesini rezerve eder. */
extern unsigned char __heap_start[];
extern unsigned char __heap_end[];

/* K1 (D-260): KEMGU_KEM_MALLOC tanımlıysa malloc/free/kem_heap_kur SAF-.kem'den
 * gelir (runtime/kem_heap.kem → çıplak C-ABI @malloc/@free/@kem_heap_kur). Bu dosya
 * o zaman yalnız memcpy/memset/kdl_global_bolge_al/kdl_panik/kdl_dizi sağlar. kem_os
 * bu varyantı (-DKEMGU_KEM_MALLOC) + kem_heap.o ile linkler → C bump-allocator = 0.
 * Diğer bare-metal kernel'ler bayrağı SET ETMEZ → C malloc/free + weak kem_heap_kur
 * no-op (boot `bl kem_heap_kur` çağırır; C-malloc lazy kdl_heap_init'le kendini kurar). */
#ifndef KEMGU_KEM_MALLOC

typedef struct KdlHeapBlok {
    size_t boyut;                 /* header dahil toplam blok boyutu (bayt) */
    struct KdlHeapBlok *sonraki;  /* serbest-liste bağlantısı */
} KdlHeapBlok;

#define KDL_HEAP_HIZA 16u

static unsigned char *kdl_heap_bump = 0;   /* bir sonraki bump adresi */
static unsigned char *kdl_heap_son  = 0;   /* heap üst sınırı (dışlayıcı) */
static KdlHeapBlok    *kdl_heap_bos  = 0;   /* serbest-liste başı (LIFO) */

static void kdl_heap_init(void) {
    if (!kdl_heap_bump) {
        kdl_heap_bump = __heap_start;
        kdl_heap_son  = __heap_end;
    }
}

/* Boot _start `bl kem_heap_kur(taban, son)` çağırır. C-malloc kernel'lerinde no-op
 * (lazy kdl_heap_init yeterli); .kem-malloc kem_os'ta kem_heap.o'daki STRONG
 * @kem_heap_kur bu weak'i override eder (linker: strong > weak). */
__attribute__((weak)) void kem_heap_kur(long taban, long son) {
    (void)taban; (void)son;
}

void *malloc(size_t n) {
    kdl_heap_init();
    if (n == 0) n = 1;                       /* 0-bayt → benzersiz geçerli ptr */

    /* header + istek, 16-hizalı toplam blok boyutu. */
    size_t toplam = (n + sizeof(KdlHeapBlok) + (KDL_HEAP_HIZA - 1))
                    & ~(size_t)(KDL_HEAP_HIZA - 1);
    if (toplam < n) return 0;                /* toplama taşması */

    /* 1) Serbest-liste ilk-uyum. Header'ı KORU (boyut bilgisi sonraki free
     *    için geçerli kalmalı) → bloğu bütün ver, böl/küçült yok. */
    KdlHeapBlok **pp = &kdl_heap_bos;
    for (KdlHeapBlok *b = kdl_heap_bos; b; b = b->sonraki) {
        if (b->boyut >= toplam) {
            *pp = b->sonraki;                /* listeden çıkar */
            return (void *)((unsigned char *)b + sizeof(KdlHeapBlok));
        }
        pp = &b->sonraki;
    }

    /* 2) Bump tahsis (16-hizalı, sınır + taşma korumalı). */
    uintptr_t cur  = (uintptr_t)kdl_heap_bump;
    uintptr_t ahiz = (cur + (KDL_HEAP_HIZA - 1)) & ~(uintptr_t)(KDL_HEAP_HIZA - 1);
    if (ahiz < cur) return 0;                                  /* hiza taşması */
    if (ahiz + toplam < ahiz) return 0;                        /* uç taşması */
    if (ahiz + toplam > (uintptr_t)kdl_heap_son) return 0;     /* OOM */
    kdl_heap_bump = (unsigned char *)(ahiz + toplam);

    KdlHeapBlok *h = (KdlHeapBlok *)ahiz;
    h->boyut = toplam;
    h->sonraki = 0;
    return (void *)((unsigned char *)ahiz + sizeof(KdlHeapBlok));
}

void free(void *p) {
    if (!p) return;
    KdlHeapBlok *h = (KdlHeapBlok *)((unsigned char *)p - sizeof(KdlHeapBlok));
    h->sonraki = kdl_heap_bos;               /* LIFO serbest-listeye it */
    kdl_heap_bos = h;
}

/* Freestanding memcpy/memset — libc yok; clang struct-kopya / kdl_dizi_buyut
 * bunları çağırır. K2 (D-262): kem_os'ta .kem çıplak memcpy/memset (kem_heap.o)
 * sağlar → burada guard-içi (diğer kernel'ler C sürümünü kullanır). */
void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

#endif  /* !KEMGU_KEM_MALLOC — malloc/free/memcpy/memset/kem_heap_kur .kem'den (kem_os) */

/* === Global bölge — kdl_runtime.c host eşinin bare-metal freestanding kopyası.
 * main() @kdl_global_bolge_al çağırır (her program). Tek lazy global bölge,
 * program ömrü boyu (status-quo leak; F4.4 dışı). K4b (D-263): kem_os'ta .kem
 * çıplak kdl_global_bolge_al (kem_heap.o) → guard-içi (diğer kernel'ler C). */
#ifndef KEMGU_KEM_MALLOC
static KdlBolge *kdl_global_bolge = 0;

KdlBolge *kdl_global_bolge_al(void) {
    if (!kdl_global_bolge) kdl_global_bolge = kdl_bolge_olustur();
    return kdl_global_bolge;
}
#endif  /* !KEMGU_KEM_MALLOC — kdl_global_bolge_al .kem'den (kem_os) */

/* === Evrensel panik (seam) — kdl_dizi_oob (kdl_dizi.inc) + codegen inline-OOB
 * (src/llvm.c) buraya çağırır. Bare-metal: UART "PANIK:" + CPU halt
 * (runtime/kdl_runtime_panik.c → kdl_panik_dur). Host eşi kdl_runtime.c
 * (stderr+abort). === */
__attribute__((noreturn)) void kdl_panik_dur(const char *);

__attribute__((noreturn)) void kdl_panik(const char *mesaj) {
    kdl_panik_dur(mesaj);
}

/* === Dizi runtime (KdlDizi + kdl_dizi_*) — host (kdl_runtime.c) ile TEK KAYNAK.
 * Bağımlılıklar yukarıda hazır: memcpy, kdl_global_bolge_al, kdl_panik;
 * kdl_bolge_ayir kdl_bolge.h'den. K2 (D-262): kem_os'ta .kem çıplak dizi runtime
 * (kem_heap.o) sağlar → guard-içi (diğer kernel'ler C kdl_dizi.inc kullanır). === */
#ifndef KEMGU_KEM_MALLOC
#include "kdl_dizi.inc"
#endif

```

# GÖREV

## İş 5 — x86 boot'unda `kem_heap_kur` çağrısı + minimum x86 kanıt çekirdeği

### Sorun (ölçüldü)
`boot/start_aarch64.S` (adım 2c) `main`'den önce `kem_heap_kur(__heap_start,
__heap_end)` çağırır. `boot/start_x86_64.S` ÇAĞIRMAZ. Saf-`.kem` allocator
(`kem_heap.kem`) x86'da bağlandığında `kem_son = 0` kalır → her `malloc` null →
dizi işlevleri sessizce 0 döner. Ölçüm: aşağıdaki test programı bugün x86 QEMU'da
`55` yerine `0` basıyor, hata/panik YOK.

### İstenen değişiklikler (yalnız bu üç dosya)

1. **`boot/start_x86_64.S`** — `long_entry` içinde `call kdl_idt_kur` ile
   `call main` ARASINA `kem_heap_kur` çağrısı ekle. SysV: 1. argüman `%rdi`,
   2. argüman `%rsi`; değerler linker sembolleri `__heap_start` / `__heap_end`
   (bkz. `linker/bare-metal-x86_64.ld`). Yorumu aarch64'teki 2c yorumunun
   x86 karşılığı olarak yaz (neden gerekli, C-malloc'lu kernel'lerde
   `kdl_bare_heap.c`'deki ZAYIF no-op'un devreye girdiği — mevcut x86
   kernel'leri bozulmaz).

2. **`test/ornekler/kem_os_x86.kem`** (YENİ) — x86 kanıt çekirdeğinin `main`'i.
   `kem_heap.kem` ile CAT edilecek (tek birim). Normal (çıplak OLMAYAN)
   `işlev main() -> tam32` olmalı: prologu bölgeyi `kdl_global_bolge_al` →
   `kdl_bolge_olustur` → `malloc` (saf-.kem) üzerinden kurar — test tam bu
   zinciri ölçer. Gövde:
   - `Dizi<tam32>` oluştur, `dizi_ekle` ile 1..10 ekle, `için` ile topla.
   - Toplam 55 ise TEK satırda `X86 HEAP DIZI OK toplam=55` bas; değilse
     `X86 HEAP DIZI HATA toplam=<değer>` bas. Değeri SABİT yazma — gerçek
     toplamı `yazdir_tam` ile bas (D-264 dersi: marker değil DEĞER denetlenir).
   - Yazdırma yerleşikleri: `yazdir_metin(m)`, `yazdir_tam(t)`,
     `yazdir_satir()` (argümansız, satır sonu). Bunlar bu çekirdekte C
     `kdl_runtime_yazdir_bare.c` + 16550 UART üzerinden gider (İş 2 sonra
     `.kem` UART'a çevirecek — şimdilik dokunma).
   - Başına dosyanın amacını anlatan Türkçe başlık yorumu.

3. **`Makefile`** — `calistir_uart_merhaba_x86_bare_metal` hedefinin hemen
   SONRASINA yeni hedef `calistir_kem_os_x86` ekle ve `.PHONY`'nin İLK satırına
   (satır 130) adını ekle. Tarif (tab ile girintili), sırayla:
   - Bağımlılıklar: `$(BUILD)/kemgu$(EXE)` ve şu nesneler (ölçüldü, bağlama
     yeten en küçük küme; `bm_x86_heap.o` ve `bm_x86_bolge.o` BİLEREK YOK —
     allocator/bölge saf-.kem'den gelmeli):
     `$(BUILD)/bm_x86_start.o $(BUILD)/bm_x86_kesme.o $(BUILD)/bm_x86_zaman.o
      $(BUILD)/bm_x86_gorev.o $(BUILD)/bm_x86_uart.o $(BUILD)/bm_x86_yazdir.o
      $(BUILD)/bm_x86_panik.o`
   - `cat runtime/kem_heap.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem`
   - `./$(BUILD)/kemgu$(EXE) --llvm --mimari x86_64 $(BUILD)/kem_os_x86_comb.kem > $(BUILD)/kem_os_x86_ham.ll`
     (AYRI satır — boruya bağlama; D-609: boru kemgu'nun çıkış kodunu maskeler)
   - `awk -f test/strip_defined_declares.awk $(BUILD)/kem_os_x86_ham.ll > $(BUILD)/kem_os_x86.ll`
   - `$(BM_X86) -O2 -Wno-override-module -x ir $(BUILD)/kem_os_x86.ll -c -o $(BUILD)/kem_os_x86.o`
   - `ld.lld -m elf_x86_64 -T linker/bare-metal-x86_64.ld -o $(BUILD)/kem_os_x86.elf $(BUILD)/kem_os_x86.o <nesneler>`
   - **Kanıt 1 (sembol kaynağı):** `llvm-nm $(BUILD)/kem_os_x86.o` içinde
     ` T malloc` ve ` T kem_heap_kur` OLMALI (saf-.kem tanımlıyor); yoksa
     `FAIL` + `exit 1`.
   - **Kanıt 2 (gerçek komut):** `llvm-objdump -d --no-show-raw-insn
     $(BUILD)/kem_os_x86.elf` çıktısında `kem_heap_kur`'a giden `call`
     satırını BUL ve O SATIRI OLDUĞU GİBİ yazdır (`echo` ile sabit metin değil,
     objdump'ın kendi satırı). Bulunamazsa `FAIL` + `exit 1`.
   - **Kanıt 3 (davranış):** QEMU: `timeout 8 qemu-system-x86_64 -kernel
     $(BUILD)/kem_os_x86.elf -display none -serial file:$(BUILD)/kem_os_x86.out
     2>/dev/null || true`; seri çıktıyı `cat` ile bas; `X86 HEAP DIZI OK
     toplam=55` YOKSA `FAIL` + `exit 1`. `qemu-system-x86_64` yoksa
     GÜRÜLTÜLÜ hata ver (`exit 1`) — sessiz atlama YASAK (D-486).
   - Diğer x86 hedeflerinin (`calistir_uart_merhaba_x86_bare_metal`) üslubunu
     izle (`@echo` başlıkları, Türkçe yorumlar).

### Kabul
- `start_x86_64.S` değişikliği yalnız ekleme; mevcut satırlar değişmez.
- Mevcut x86 kernel'leri (C malloc'lu) bozulmamalı — zayıf no-op sayesinde.

````
