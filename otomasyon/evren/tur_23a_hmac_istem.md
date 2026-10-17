# Tur 23a — hmac — istem

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

## stdlib/kripto/karma.kem — KESİT (satır 29-35; düzenleme hedefi: `stdlib/kripto/karma.kem`, blok yolu `stdlib/kripto/karma.kem` olmalı)
```
işlev _karma_rotr_u32(x: sabitsüre<dtam32>, n: tam32) -> sabitsüre<dtam32> {
    değişken sag: sabitsüre<dtam32> = x >> n;
    değişken sol: sabitsüre<dtam32> = x << (32 - n);
    ver sag | sol;
}

// SHA-256 Σ0 (Sigma0): (x ROTR 2) ^ (x ROTR 13) ^ (x ROTR 22)
```

## stdlib/kripto/karma.kem — KESİT (satır 109-140; düzenleme hedefi: `stdlib/kripto/karma.kem`, blok yolu `stdlib/kripto/karma.kem` olmalı)
```
işlev sha256_H0() -> Dizi<dtam32> {
    ver [
        0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
        0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
    ];
}

// =============================================================================
// SHA-256 Tek Blok Sıkıştırma (512-bit blok = 16 dtam32 = 64 bayt)
// =============================================================================
//
// State (a..h) 8 dtam32 — sabitsüre, içerikleri gizli (hash intermediate)
// Block (M[0..15]) — sabitsüre, çünkü gizli mesajdan geliyor
// Schedule W[0..63] — sabitsüre, türevsel
//
// Bu fonksiyon FIPS 180-4 §6.2.2 algoritması. CT discipline:
//   - Tüm aritmetik (+ - * & | ^ >>) sabit-süre opcode'lar
//   - Hiç dallanma yok (rounds for-loop public counter)
//   - K[i] erişimi public i ile — CT002 ihlali değil

işlev sha256_blok_sikistir(
    state: Dizi<sabitsüre<dtam32>>,
    blok_w: Dizi<sabitsüre<dtam32>>
) -> tam32 {
    // Message schedule W[64] inşa et
    değişken W: Dizi<sabitsüre<dtam32>> = [
        sabitsüre_olustur((0 olarak dtam32)), sabitsüre_olustur((0 olarak dtam32)),
        sabitsüre_olustur((0 olarak dtam32)), sabitsüre_olustur((0 olarak dtam32)),
        sabitsüre_olustur((0 olarak dtam32)), sabitsüre_olustur((0 olarak dtam32)),
        sabitsüre_olustur((0 olarak dtam32)), sabitsüre_olustur((0 olarak dtam32)),
        sabitsüre_olustur((0 olarak dtam32)), sabitsüre_olustur((0 olarak dtam32)),
        sabitsüre_olustur((0 olarak dtam32)), sabitsüre_olustur((0 olarak dtam32)),
```

## stdlib/kripto/karma.kem — KESİT (satır 259-275; düzenleme hedefi: `stdlib/kripto/karma.kem`, blok yolu `stdlib/kripto/karma.kem` olmalı)
```
// HMAC pad'leri üret — K ⊕ ipad ve K ⊕ opad
işlev hmac_pad_uret(
    anahtar: Dizi<sabitsüre<dtam8>>,
    anahtar_uzunluk: tam32,
    inner: Dizi<sabitsüre<dtam8>>,
    outer: Dizi<sabitsüre<dtam8>>
) -> tam32 {
    değişken ipad: dtam8 = 0x36;
    değişken opad: dtam8 = 0x5C;
    değişken i: tam32 = 0;
    iken i < anahtar_uzunluk {
        inner[i] = anahtar[i] ^ sabitsüre_olustur(ipad);
        outer[i] = anahtar[i] ^ sabitsüre_olustur(opad);
        i = i + 1;
    }
    ver 0;
}
```

## test/stdlib/test_kripto_kosum.kem — KESİT (satır 80-110; düzenleme hedefi: `test/stdlib/test_kripto_kosum.kem`, blok yolu `test/stdlib/test_kripto_kosum.kem` olmalı)
```
işlev kt_sha256_abc() -> tam32 {
    değişken w0: dtam32 = 0x61626380;   // "abc" + 0x80 padding
    değişken sifir: dtam32 = 0;
    değişken w15: dtam32 = 24;          // mesaj uzunluğu = 24 bit
    değişken blok: Dizi<sabitsüre<dtam32>> = [
        sabitsüre_olustur(w0),    sabitsüre_olustur(sifir), sabitsüre_olustur(sifir), sabitsüre_olustur(sifir),
        sabitsüre_olustur(sifir), sabitsüre_olustur(sifir), sabitsüre_olustur(sifir), sabitsüre_olustur(sifir),
        sabitsüre_olustur(sifir), sabitsüre_olustur(sifir), sabitsüre_olustur(sifir), sabitsüre_olustur(sifir),
        sabitsüre_olustur(sifir), sabitsüre_olustur(sifir), sabitsüre_olustur(sifir), sabitsüre_olustur(w15)
    ];
    değişken H: Dizi<dtam32> = sha256_H0();
    değişken h0: dtam32 = H[0];
    değişken h1: dtam32 = H[1];
    değişken h2: dtam32 = H[2];
    değişken h3: dtam32 = H[3];
    değişken h4: dtam32 = H[4];
    değişken h5: dtam32 = H[5];
    değişken h6: dtam32 = H[6];
    değişken h7: dtam32 = H[7];
    değişken state: Dizi<sabitsüre<dtam32>> = [
        sabitsüre_olustur(h0), sabitsüre_olustur(h1),
        sabitsüre_olustur(h2), sabitsüre_olustur(h3),
        sabitsüre_olustur(h4), sabitsüre_olustur(h5),
        sabitsüre_olustur(h6), sabitsüre_olustur(h7)
    ];
    sha256_blok_sikistir(state, blok);
    değişken s0: dtam32 = ifşa(state[0]);
    değişken s1: dtam32 = ifşa(state[1]);
    değişken s7: dtam32 = ifşa(state[7]);
    değişken gecti: tam32 = 1;
    eğer (s0 olarak tam64) != 0xBA7816BF { gecti = 0; }
```

# GÖREV

# Tur 23a — stdlib/kripto: çok bloklu SHA-256 + HMAC-SHA256 (RFC 2104) + koşum testleri

YALNIZ `stdlib/kripto/karma.kem`'e TEK bir ```duzenle bloğu yaz; başka dosyaya DOKUNMA. Saf KEMGU (asm YOK). Bağlamdaki
`stdlib/kripto/karma.kem` kesitleri deyimleri gösterir: `sabitsüre<dtam32>` üzerinde `^ | & >> << +`,
`sabitsüre_olustur(x)`, `ifşa(s)` (gizliyi açığa çıkarır), `(x olarak dtam32)`. Dizi büyütme:
`dizi_ekle(d, x)`, boyut `dizi_boyut(d)`, okuma `d[i]`, yazma `d[i] = x`. Boş dizi:
`değişken d: Dizi<sabitsüre<dtam8>> = [];`. Dizgi literalinde KAÇIŞ YOKTUR. `hata` bir anahtar
kelimedir — değişken adı olarak KULLANMA. Sabitler aşağıda araçla (Python hashlib/hmac) doğrulandı.

## 1) stdlib/kripto/karma.kem — dosyanın SONUNA (ESKİ = son satırlar, kesitte `hmac_pad_uret`'in sonu)
Mevcut `sha256_blok_sikistir(state, blok_w)` (16 kelimelik blok, state'i yerinde günceller) ve
`sha256_H0()` kullanılacak; yeniden tanımlama YOK.
- `işlev sha256_bayt(veri: Dizi<sabitsüre<dtam8>>, n: tam32) -> Dizi<sabitsüre<dtam32>>` —
  FIPS 180-4 dolgusu: n bayt + 0x80 + sıfırlar + 8 baytlık BÜYÜK-UÇ bit uzunluğu (n*8; üst 32 bit 0
  yeter), toplam 64'ün katı. Her 64 baytlık blok için 16 kelime BÜYÜK-UÇ kur
  (w = b0<<24 | b1<<16 | b2<<8 | b3, baytlar `olarak sabitsüre<dtam32>` ile genişletilir) ve
  `sha256_blok_sikistir` çağır. Durum sha256_H0()'dan başlar (kesitteki test bunu gösteriyor). 8
  kelimelik durumu döndür.
- `işlev hmac_sha256(anahtar: Dizi<sabitsüre<dtam8>>, an: tam32, mesaj: Dizi<sabitsüre<dtam8>>, mn: tam32) -> Dizi<sabitsüre<dtam32>>`
  — RFC 2104, blok 64 bayt: an > 64 ise anahtar = sha256_bayt(anahtar, an) özetinin 32 baytı
  (büyük-uç), sonra 64'e sıfırla doldur. ic = (K ^ 0x36)[64] || mesaj → iç = sha256_bayt(ic, 64 + mn);
  dis = (K ^ 0x5C)[64] || iç özetin 32 baytı (büyük-uç) → ver sha256_bayt(dis, 96).
  Kelime→bayt: (w >> 24), (w >> 16), (w >> 8), w; her biri `& 255` ve `olarak sabitsüre<dtam8>`.


````
