# Tur 24 — turbofish_checker — istem

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

## src/tip_kontrol.c — KESİT (satır 2665-2672; düzenleme hedefi: `src/tip_kontrol.c`, blok yolu `src/tip_kontrol.c` olmalı)
```
        tk->t043_kap = yk;
    }
    tk->t043_dugum[tk->t043_sayi++] = d;
    tip_hata(tk, d, "T043", "sayi literali hedef tipe sigmiyor");
}

TipBilgisi *tip_belirle(TipKontrol *tk, const Dugum *d) {
    if (!d) return t_hata(tk);
```

## src/tip_kontrol.c — KESİT (satır 3208-3214; düzenleme hedefi: `src/tip_kontrol.c`, blok yolu `src/tip_kontrol.c` olmalı)
```

        /* === Cagri === */
        case DUGUM_CAGRI: {
            /* [D-517] HAM BELLEK YERLESIKLERI `güvensiz` ISTER.
             * OLCULEN ACIK: `bellek_al` / `bellek_kopyala` / `bellek_serbest`
             * `metin` olarak TIPLENIYOR (D-459: "HAM BELLEK", opak) — bu yuzden
             * G001'in `*T` kosulu onlara HIC uygulanmiyordu. Sonuc: `güvensiz`
```

## src/tip_kontrol.c — KESİT (satır 4390-4422; düzenleme hedefi: `src/tip_kontrol.c`, blok yolu `src/tip_kontrol.c` olmalı)
```
                }
            }
            /* Madde D: Multi-param + compound type generic inference.
             * GenBaglamalar ile her arg/param ciftinde unify, donus
             * tipini substitue et. */
            GenBaglamalar gb;
            gb.sayi = 0;
            /* Once unify arg tipleri ile (substitue olmadan) — daha sonra
             * argumanlar substitue edilmis param tipi context'inde tekrar
             * cikarsanir. Iki pas: pas 1 inference, pas 2 type check. */
            tk->lineer_sondaj++;    /* D-320: pas 1 = SONDAJ, tuketim sayilmaz */
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                TipBilgisi *param_tip = hedef_tip->veri.islev.parametreler[i];
                TipBilgisi *arg_tip = tip_belirle(tk,
                    d->veri.cagri.argumanlar[i]);
                gen_unify(&gb, param_tip, arg_tip);
            }
            tk->lineer_sondaj--;
            /* Pas 2: arg tipini substitue edilmis param tipi context'inde
             * cikarsama + tip kontrolu */
            for (int i = 0; i < d->veri.cagri.sayi; i++) {
                TipBilgisi *param_tip = hedef_tip->veri.islev.parametreler[i];
                TipBilgisi *bek = gen_substitue(tk, param_tip, &gb);
                TipBilgisi *arg_tip = tip_belirle_beklenen(tk,
                    d->veri.cagri.argumanlar[i], bek);
                if (param_tip->kategori == TIP_GENERIC_PARAM) {
                    /* Concrete'i gb'de tutuyoruz; bind onceden yapildi */
                    continue;
                }
                /* Sabitsüre Spec V1 CT003: arg sabitsure<T> → param T leak.
                 * Önce bunu kontrol et (T001'den önce); ifsa(...) gerek. */
                if (arg_tip->kategori != TIP_HATA &&
                    param_tip->kategori != TIP_HATA &&
```

## src/tip_kontrol.c — KESİT (satır 5095-5101; düzenleme hedefi: `src/tip_kontrol.c`, blok yolu `src/tip_kontrol.c` olmalı)
```
        }

        case DUGUM_CAGRI: {
            /* C3: çeşit varyant yapıcısı (Cesit::V(args)) — beklenen-bağlam
             * yolunda da modül-fonksiyon çağrısından ÖNCE dene. Generic çeşit
             * T'si beklenen'den (Secim<tam32>) substitue edilir (D-302). */
            {
```

## selfhost/checker.kem — KESİT (satır 185-197; düzenleme hedefi: `selfhost/checker.kem`, blok yolu `selfhost/checker.kem` olmalı)
```
    // tf_bas[k] + tf_say[k]). Düğüme çocuk EKLENMEZ (çocuk[1..] = argüman).
    tf_cagri: Dizi<tam32>;
    tf_bas: Dizi<tam32>;
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
}

// ---- Çıktı yardımcıları ----
// HAM bayt yaz (yaz_karakter UTF-8 encode eder → Türkçe bayt mojibake;
// yaz_bayt ham byte yazar — METIN değer dump'ında UTF-8 korunur).
işlev yb(c: tam32) -> tam32 { yaz_bayt(c); ver 0; }
işlev yaz_str(s: metin) -> tam32 {
    değişken n: tam32 = metin_uzunluk(s);
```

## selfhost/checker.kem — KESİT (satır 1738-1758; düzenleme hedefi: `selfhost/checker.kem`, blok yolu `selfhost/checker.kem` olmalı)
```
    değişken s: tam32 = sim_sat(p);  değişken u: tam32 = sim_sut(p);
    bekle(p, "ISLEV");
    değişken ad: metin = sim_lex(p);
    bekle(p, "TANIMLAYICI");
    atla_tip_paramlar(p);
    bekle_k(p, "SOL_PAREN", "P015");   // [D-650] C parser.c:343 imza "("
    değişken kids: Dizi<tam32> = [];
    eğer sim_mi(p, "SAG_PAREN") == yanlış {
        değişken devam: mantıksal = doğru;
        iken devam {
            dizi_ekle(kids, parse_parametre(p));
            eğer sim_mi(p, "VIRGUL") { ilerle(p); }
            değilse { devam = yanlış; }
        }
    }
    bekle_k(p, "SAG_PAREN", "P016");   // [D-650] C parser.c:354 imza ")"
    eğer sim_mi(p, "OK") { ilerle(p); dizi_ekle(kids, parse_tip(p)); }
    eğer sim_mi(p, "SOL_SUSLU") { dizi_ekle(kids, parse_blok(p)); }
    değilse {
        eğer (imza_yeterli == 1) ve sim_mi(p, "NOKTALI_VIRGUL") { ilerle(p); }
        değilse { parse_hata_kaydet(p, "P017"); }   // [D-648] C parser.c:367
```

## selfhost/checker.kem — KESİT (satır 2108-2114; düzenleme hedefi: `selfhost/checker.kem`, blok yolu `selfhost/checker.kem` olmalı)
```
        yapi_ad: [], yapi_abase: [], yapi_acount: [], alan_ad: [], alan_tip: [], alan_tn: [], t14_muaf: [], m4_node: [], m4_tn: [],
        giris_dizin: "", kullan_gorulen: [],
        ly_ad: [], lyf_yapi: [], lyf_alan: [], lin_ad: [], lin_sat: [], lin_sut: [], lin_tuk: [], dy_node: [], dy_yapi: [], dyf_node: [], dyf_alan: [], lin_maske: [], lin_fnk: [], lin_tek: [], lin_yet: [], gtas_ad: [], kt_ad: [], snap_tuk: [], snap_maske: [], snap_top: 0, imha_bag: 0, guv_bag: 0, yer_son: 0, tuk_kapali: 0, cast_sessiz: 0, cast_hata_var: 0, cip_bag: 0, lin_basla: 0,
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0,
        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []
    };
}
```

## selfhost/checker.kem — KESİT (satır 2240-2245; düzenleme hedefi: `selfhost/checker.kem`, blok yolu `selfhost/checker.kem` olmalı)
```
// `bilinen_tip_mi` kullanır. Böylece artım davranış-nötr başlar, kapsam
// tek tek ve ölçülerek açılır.
işlev tip_str(p: &değişken Ayr, idx: tam32) -> metin {
    eğer metin_esit(dizi_al(p.a_ad, idx), "TIP_BASIT") { ver dizi_al(p.a_deg, idx); }
    eğer metin_esit(dizi_al(p.a_ad, idx), "TIP_DIZI") {
        eğer dizi_al(p.a_cs, idx) < 1 { ver "?"; }
```

## selfhost/checker.kem — KESİT (satır 2904-2914; düzenleme hedefi: `selfhost/checker.kem`, blok yolu `selfhost/checker.kem` olmalı)
```
    }
    eğer metin_esit(ad, "CAGRI") {   // TC3d: kullanıcı işlevi çağrısı → dönüş tipi
        değişken h: tam32 = dizi_al(p.cocuk, dizi_al(p.a_cb, idx));   // çocuk[0]=hedef
        eğer metin_esit(dizi_al(p.a_ad, h), "TANIMLAYICI") {
            // D-386: Katman 2 intrinsikleri C'de imza TABLOSUNDA DEĞİL, bağlam
            // duyarlı özel-durumdur → dönüşleri ARGÜMAN tipinden türetilir.
            değişken kt: metin = intrinsik_donus(p, idx, dizi_al(p.a_deg, h));
            eğer metin_esit(kt, "?") == yanlış { ver kt; }
            ver fn_donus_bul(p, dizi_al(p.a_deg, h));   // "?" = kullanıcı işlevi değil
        }
        ver "?";   // method/dolaylı çağrı → ileri TC
```

## selfhost/checker.kem — KESİT (satır 5821-5826; düzenleme hedefi: `selfhost/checker.kem`, blok yolu `selfhost/checker.kem` olmalı)
```
    eğer metin_esit(ad, "CAGRI") {
        değişken hb: tam32 = dizi_al(p.a_cb, idx);
        değişken hc: tam32 = dizi_al(p.a_cs, idx);
        değişken hedef: tam32 = dizi_al(p.cocuk, hb);
        // [D-517] HAM BELLEK YERLEŞİKLERİ `güvensiz` İSTER — C tip_kontrol.c aynası.
        // `bellek_*` `metin` olarak tiplendiği için G001'in `*T` koşulu onlara
```

## selfhost/checker.kem — KESİT (satır 5896-5925; düzenleme hedefi: `selfhost/checker.kem`, blok yolu `selfhost/checker.kem` olmalı)
```
                // D-375: C argüman alt-ağacını İKİ KEZ tip-belirler → traversal
                // kaynaklı TÜM tanılar (T002/T005-T008/T010/G001/E002…) İKİLENİR.
                // Sıra ölçüldü: ÖNCE tüm argümanlar üzerinden bir geçiş, SONRA
                // ikinci geçiş + per-arg T001 (`al(s, yok())` → T002;T001;T002).
                // Ön-geçişte tüketim KAPALI: C lineeri ikilemiyor (`al(tuket(t))` → OK).
                p.tuk_kapali = p.tuk_kapali + 1;
                değişken pa2: tam32 = 1;
                iken pa2 < hc { kontrol_dugum(p, dizi_al(p.cocuk, hb + pa2)); pa2 = pa2 + 1; }
                p.tuk_kapali = p.tuk_kapali - 1;
                // arite OK → her arg: traverse (iç T002) + per-arg T001 (param tipi)
                değişken a: tam32 = 1;
                iken a < hc {
                    değişken arg: tam32 = dizi_al(p.cocuk, hb + a);
                    // D-376: İKİNCİ geçişte beklenen tip yayılır → param `Dizi<..>`
                    // ise boş `[]` bu geçişte T014 ALMAZ (C ile birebir).
                    t14_muaf_isaretle(p, arg, fn_ptn_bul(p, fad, a - 1));
                    kontrol_dugum(p, arg);
                    değişken pt: metin = fn_ptip_bul(p, fad, a - 1);
                    eğer metin_esit(pt, "?") == yanlış {
                        değişken at: metin = ifade_tip(p, arg, pt);
                        eğer (metin_esit(at, "?") == yanlış) ve (metin_esit(at, pt) == yanlış) {
                            dizi_ekle(p.th_kod, "T001");
                            dizi_ekle(p.th_sat, dizi_al(p.a_sat, arg));
                            dizi_ekle(p.th_sut, dizi_al(p.a_sut, arg));
                        }
                    }
                    eğer fn_plin_bul(p, fad, a - 1) == 1 { lin_tuket_dugum(p, arg); }   // TC5b move
                    g003_kontrol(p, fad, a - 1, arg);   // D-358: C sırası — lineer tüketimden SONRA
                    a = a + 1;
                }
```

## selfhost/codegen.kem — KESİT (satır 337-343; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
    // tf_bas[k] + tf_say[k]). Düğüme çocuk EKLENMEZ (çocuk[1..] = argüman).
    tf_cagri: Dizi<tam32>;
    tf_bas: Dizi<tam32>;
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
}

```

## selfhost/codegen.kem — KESİT (satır 2056-2084; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
    değişken s: tam32 = sim_sat(p);  değişken u: tam32 = sim_sut(p);
    bekle(p, "ISLEV");
    değişken ad: metin = sim_lex(p);
    bekle(p, "TANIMLAYICI");
    atla_tip_paramlar(p);
    // D-401: İŞLEVİN generic param adlarını KAYDET. Öncesinde `atla_tip_paramlar`
    // adları `tp_pending`e yakalıyor ama işlev yolunda kimse tüketmiyordu → bir
    // sonraki bildirimde ÜZERİNE YAZILIYORDU. Sonuç: self-host generic işlevleri
    // hiç monomorfize etmiyor, T'yi daima i32 sanıyordu (ölçüldü: tek
    // `@kimlik(ptr,i32)` gövde + `call ... i64` imza uyuşmazlığı; tamsayıda ABI
    // tesadüfüyle ayakta kalıyor, `kesirli64`te LLVM reddediyor).
    // Registry yapı/çeşit ile ORTAK (`tp_yad`/`tp_ad`): çakışma riski yok çünkü
    // `mono_gerek_yapi` yalnız `yapi_var_mi` doğruyken sorgulanır.
    tip_param_kaydet(p, ad);
    bekle_k(p, "SOL_PAREN", "P015");   // [D-650] C parser.c:343 imza "("
    değişken kids: Dizi<tam32> = [];
    eğer sim_mi(p, "SAG_PAREN") == yanlış {
        değişken devam: mantıksal = doğru;
        iken devam {
            dizi_ekle(kids, parse_parametre(p));
            eğer sim_mi(p, "VIRGUL") { ilerle(p); }
            değilse { devam = yanlış; }
        }
    }
    bekle_k(p, "SAG_PAREN", "P016");   // [D-650] C parser.c:354 imza ")"
    eğer sim_mi(p, "OK") { ilerle(p); dizi_ekle(kids, parse_tip(p)); }
    eğer sim_mi(p, "SOL_SUSLU") { dizi_ekle(kids, parse_blok(p)); }
    değilse {
        eğer (imza_yeterli == 1) ve sim_mi(p, "NOKTALI_VIRGUL") { ilerle(p); }
```

## selfhost/codegen.kem — KESİT (satır 9556-9561; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
// `bilinen_tip_mi` kullanır. Böylece artım davranış-nötr başlar, kapsam
// tek tek ve ölçülerek açılır.
işlev tip_str(p: &değişken Ayr, idx: tam32) -> metin {
    eğer metin_esit(dizi_al(p.a_ad, idx), "TIP_BASIT") { ver dizi_al(p.a_deg, idx); }
    eğer metin_esit(dizi_al(p.a_ad, idx), "TIP_DIZI") {
        eğer dizi_al(p.a_cs, idx) < 1 { ver "?"; }
```

## selfhost/codegen.kem — KESİT (satır 10224-10234; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
    }
    eğer metin_esit(ad, "CAGRI") {   // TC3d: kullanıcı işlevi çağrısı → dönüş tipi
        değişken h: tam32 = dizi_al(p.cocuk, dizi_al(p.a_cb, idx));   // çocuk[0]=hedef
        eğer metin_esit(dizi_al(p.a_ad, h), "TANIMLAYICI") {
            // D-386: Katman 2 intrinsikleri C.de imza TABLOSUNDA DEĞİL, bağlam
            // duyarlı özel-durumdur → dönüşleri ARGÜMAN tipinden türetilir.
            değişken kt: metin = intrinsik_donus(p, idx, dizi_al(p.a_deg, h));
            eğer metin_esit(kt, "?") == yanlış { ver kt; }
            ver fn_donus_bul(p, dizi_al(p.a_deg, h));   // "?" = kullanıcı işlevi değil
        }
        ver "?";   // method/dolaylı çağrı → ileri TC
```

## selfhost/codegen.kem — KESİT (satır 13146-13150; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
    eğer metin_esit(ad, "CAGRI") {
        değişken hb: tam32 = dizi_al(p.a_cb, idx);
        değişken hc: tam32 = dizi_al(p.a_cs, idx);
        değişken hedef: tam32 = dizi_al(p.cocuk, hb);
        // [D-517] HAM BELLEK YERLEŞİKLERİ `güvensiz` İSTER — C tip_kontrol.c
```

## selfhost/codegen.kem — KESİT (satır 13220-13250; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
                }
                // D-375: C argüman alt-ağacını İKİ KEZ tip-belirler → traversal
                // kaynaklı TÜM tanılar (T002/T005-T008/T010/G001/E002…) İKİLENİR.
                // Sıra ölçüldü: ÖNCE tüm argümanlar üzerinden bir geçiş, SONRA
                // ikinci geçiş + per-arg T001 (`al(s, yok())` → T002;T001;T002).
                // Ön-geçişte tüketim KAPALI: C lineeri ikilemiyor (`al(tuket(t))` → OK).
                p.tuk_kapali = p.tuk_kapali + 1;
                değişken pa2: tam32 = 1;
                iken pa2 < hc { kontrol_dugum(p, dizi_al(p.cocuk, hb + pa2)); pa2 = pa2 + 1; }
                p.tuk_kapali = p.tuk_kapali - 1;
                // arite OK → her arg: traverse (iç T002) + per-arg T001 (param tipi)
                değişken a: tam32 = 1;
                iken a < hc {
                    değişken arg: tam32 = dizi_al(p.cocuk, hb + a);
                    // D-376: İKİNCİ geçişte beklenen tip yayılır → param `Dizi<..>`
                    // ise boş `[]` bu geçişte T014 ALMAZ (C ile birebir).
                    t14_muaf_isaretle(p, arg, fn_ptn_bul(p, fad, a - 1));
                    kontrol_dugum(p, arg);
                    değişken pt: metin = fn_ptip_bul(p, fad, a - 1);
                    eğer metin_esit(pt, "?") == yanlış {
                        değişken at: metin = ifade_tip(p, arg, pt);
                        eğer (metin_esit(at, "?") == yanlış) ve (metin_esit(at, pt) == yanlış) {
                            dizi_ekle(p.th_kod, "T001");
                            dizi_ekle(p.th_sat, dizi_al(p.a_sat, arg));
                            dizi_ekle(p.th_sut, dizi_al(p.a_sut, arg));
                        }
                    }
                    eğer fn_plin_bul(p, fad, a - 1) == 1 { lin_tuket_dugum(p, arg); }   // TC5b move
                    g003_kontrol(p, fad, a - 1, arg);   // D-358: C sırası — lineer tüketimden SONRA
                    a = a + 1;
                }
```

## selfhost/codegen.kem — KESİT (satır 14684-14689; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
        giris_dizin: "", kullan_gorulen: [],
        ly_ad: [], lyf_yapi: [], lyf_alan: [], lin_ad: [], lin_sat: [], lin_sut: [], lin_tuk: [], dy_node: [], dy_yapi: [], dyf_node: [], dyf_alan: [], lin_maske: [], lin_fnk: [], lin_tek: [], lin_yet: [], gtas_ad: [], kt_ad: [], snap_tuk: [], snap_maske: [], snap_top: 0, imha_bag: 0, guv_bag: 0, yer_son: 0, tuk_kapali: 0, cast_sessiz: 0, cast_hata_var: 0, cip_bag: 0, lin_basla: 0,
        th_kod: [], th_sat: [], th_sut: [], t043_node: [], tasma_node: [], neg_lit: 0,
        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []
    };
}
```

# GÖREV

# Tur 24 — turbofish CHECKER: açık argüman çıkarsamayı EZER + T044

Önceki turda parser `f::<T1, T2>(args)` ekledi:
- C: `d->veri.cagri.tip_arg` (Dugum**) + `d->veri.cagri.tip_arg_sayi` (int; yoksa 0).
- Self-host: CAGRI düğümü `idx` için `tf_bul(p, idx)` → k (yoksa -1);
  tip düğümleri `dizi_al(p.tf_tip, dizi_al(p.tf_bas, k) + j)`, j < `dizi_al(p.tf_say, k)`.

Bu tur TİP KONTROLÜNÜ ekler; üç uygulamaya AYNI kural (CLAUDE.md D-517: bir checker
kuralı üç uygulamanın HEPSİNE portlanır): `src/tip_kontrol.c`, `selfhost/checker.kem`,
`selfhost/codegen.kem` (bu sonuncusunun İÇİNDEKİ checker kopyası). Başka dosyaya DOKUNMA.
Her değişiklik bir ```duzenle bloğu; ESKI metni harfiyen kopyala (tek kez geçer — ölçüldü).
KEMGU dizgi literallerinde KAÇIŞ YOK.

## Kural (üç uygulamada aynı)
1. **T044** — turbofish varsa ve hedef, generic parametre sayısı turbofish argüman
   sayısına EŞİT bir kullanıcı işlevi DEĞİLSE (generic olmayan işlev → 0 param,
   yerleşik, tanımsız ad, yerel değişken/lambda, metod çağrısı `x.m::<T>()` ...) →
   `T044` CAGRI düğümünün konumunda; çağrı işlemesi ERKEN DÖNER (argümanlar
   gezilmez). Bu denetim CAGRI işlemenin EN BAŞINDADIR (her şeyden önce).
   İSTİSNA: hedef `YOL` (`m::f::<T>(..)`) — C'de ön-denetim atlanır, sembol
   `tip_belirle(hedef)` sonrası bilinir ve orada denetlenir; self-host'ta YOL için
   hiçbir şey yapılmaz (V1 sınırı; self-host YOL çağrılarında T010/T001'i de
   zaten denetlemiyor).
2. **Ezme** — sayı tutuyorsa i'inci açık tip, işlevin i'inci generic parametresine
   BAĞLANIR, argümanlardan çıkarsama onu DEĞİŞTİREMEZ. Sonuç: parametre tipi
   (ikame edilmiş) beklenen tip olur → literal o tipte çıkarsanır
   (`kimlik::<tam64>(8589934592)` OK), uyuşmayan argüman **T001** (argüman
   konumu), dönüş tipi ikame edilmiş tiptir (`değişken b: tam32 =
   kimlik::<tam64>(5)` → DEGISKEN'de T001).

## C — `src/tip_kontrol.c` (5 blok)

### C1. Yardımcılar — `tip_belirle` tanımından hemen ÖNCE
ESKI: `TipBilgisi *tip_belirle(TipKontrol *tk, const Dugum *d) {` (tek kez)
YENI: aşağıdaki iki işlev + boş satır + o satırın kendisi.
```c
/* [D1 turbofish] `f::<T1,T2>(..)` hedefinin generic islev AST'si.
 * TANIMLAYICI: sozcuksel arama (tip_belirle ile ayni golgeleme kurali —
 * ayni adli yerel degisken islevi golgeler). YOL: resolver baglamasi
 * (tip_belirle(hedef) SONRASI dolar). Generic olmayan / islev olmayan /
 * bulunamayan -> NULL. */
static const Dugum *turbofish_islev_bul(TipKontrol *tk, const Dugum *h) {
    const Sembol *s = NULL;
    if (!h) return NULL;
    if (h->tip == DUGUM_TANIMLAYICI) {
        s = sembol_bul(tk->scope, h->veri.tanimlayici.metin,
                       h->veri.tanimlayici.uzunluk);
    } else if (h->tip == DUGUM_YOL) {
        s = h->cozum_sembol;
    }
    if (!s || s->kategori != SEMBOL_ISLEV || !s->ast_dugumu ||
        s->ast_dugumu->tip != DUGUM_ISLEV) return NULL;
    return s->ast_dugumu;
}

/* [D1 turbofish] On-denetim — CAGRI islemenin EN BASINDA (her iki giris
 * noktasinda: tip_belirle ve tip_belirle_beklenen). Acik generic arguman
 * sayisi, hedef islevin generic parametre sayisina esit olmali; aksi T044
 * ve 1 doner (cagiran t_hata ile erken doner, argumanlar gezilmez).
 * Yan etkisiz basari yolu: iki giris noktasinda ard arda kosmasi tanıyı
 * IKILEMEZ. YOL hedefi burada atlanir (sembol henuz cozulmedi) ve
 * baglama asamasinda denetlenir. Yerlesik (dizi_olustur, tekkez_olustur,
 * kanal_olustur ...) hedef -> T044: yerlesikler acik generic arguman ALMAZ;
 * sessizce yok saymak "kabul et, anlamini degistir" olurdu. */
static int turbofish_on_kontrol(TipKontrol *tk, const Dugum *d) {
    int n = d->veri.cagri.tip_arg_sayi;
    if (n <= 0) return 0;
    const Dugum *h = d->veri.cagri.hedef;
    if (h && h->tip == DUGUM_YOL) return 0;
    const Dugum *f = turbofish_islev_bul(tk, h);
    int gsay = f ? f->veri.islev.tip_param_sayi : 0;
    if (n != gsay) {
        tip_hata(tk, d, "T044",
                 "turbofish tip argumani sayisi islevin generic parametre "
                 "sayisiyla uyumsuz");
        return 1;
    }
    return 0;
}
```

### C2. `tip_belirle` içindeki CAGRI — en başa çağrı
ESKI (tam olarak, iki satır):
```
        case DUGUM_CAGRI: {
            /* [D-517] HAM BELLEK YERLESIKLERI `güvensiz` ISTER.
```
YENI:
```
        case DUGUM_CAGRI: {
            if (turbofish_on_kontrol(tk, d)) return t_hata(tk);  /* [D1 turbofish] */
            /* [D-517] HAM BELLEK YERLESIKLERI `güvensiz` ISTER.
```

### C3. `tip_belirle_beklenen` içindeki CAGRI — en başa çağrı
ESKI (tam olarak, iki satır):
```
        case DUGUM_CAGRI: {
            /* C3: çeşit varyant yapıcısı (Cesit::V(args)) — beklenen-bağlam
```
YENI:
```
        case DUGUM_CAGRI: {
            if (turbofish_on_kontrol(tk, d)) return t_hata(tk);  /* [D1 turbofish] */
            /* C3: çeşit varyant yapıcısı (Cesit::V(args)) — beklenen-bağlam
```

### C4. Bağlama — açık argümanlar çıkarsamadan ÖNCE `gb`ye
ESKI (tam olarak, üç satır):
```
            GenBaglamalar gb;
            gb.sayi = 0;
            /* Once unify arg tipleri ile (substitue olmadan) — daha sonra
```
YENI:
```
            GenBaglamalar gb;
            gb.sayi = 0;
            /* [D1 turbofish] ACIK generic argumanlar cikarsamayi EZER: once
             * baglanir; gen_bagla "ilk gorulen kazanir" oldugu icin pas 1'deki
             * gen_unify onlari DEGISTIREMEZ. Pas 2'de parametre tipi bu
             * baglamayla ikame edilip BEKLENEN tip olur (literal o tipte
             * cikarsanir, uyusmayan arguman T001), donus tipi de ikame edilir.
             * YOL hedefi on-denetimden gecmisti (sembol o an bilinmiyordu):
             * sayi burada denetlenir. */
            const Dugum *tf_islev = NULL;
            if (d->veri.cagri.tip_arg_sayi > 0) {
                tf_islev = turbofish_islev_bul(tk, d->veri.cagri.hedef);
                if (!tf_islev || tf_islev->veri.islev.tip_param_sayi !=
                                 d->veri.cagri.tip_arg_sayi) {
                    tip_hata(tk, d, "T044",
                             "turbofish tip argumani sayisi islevin generic "
                             "parametre sayisiyla uyumsuz");
                    return t_hata(tk);
                }
                for (int ti = 0; ti < d->veri.cagri.tip_arg_sayi; ti++) {
                    const char *tp = tf_islev->veri.islev.tip_paramlar[ti];
                    TipBilgisi *acik = ast_tip_to_bilgi(tk,
                        d->veri.cagri.tip_arg[ti]);
                    gen_bagla(&gb, tp, (int)strlen(tp), acik);
                }
            }
            /* Once unify arg tipleri ile (substitue olmadan) — daha sonra
```

### C5. Pas 2 — açıkça bağlı generic parametre artık DENETLENİR
ESKI (tam olarak, dört satır):
```
                if (param_tip->kategori == TIP_GENERIC_PARAM) {
                    /* Concrete'i gb'de tutuyoruz; bind onceden yapildi */
                    continue;
                }
```
YENI:
```
                if (param_tip->kategori == TIP_GENERIC_PARAM &&
                    (!tf_islev || bek->kategori == TIP_HATA)) {
                    /* Concrete'i gb'de tutuyoruz; bind onceden yapildi.
                     * [D1 turbofish] ACIK baglamada bek arguman tipinden
                     * TURETILMEDI -> asagidaki T001 denetimi GEREKLI. */
                    continue;
                }
```

## Self-host — `selfhost/checker.kem` VE `selfhost/codegen.kem` (her birinde AYNI 7 blok)

Aşağıdaki ESKI metinler İKİ dosyada da harfiyen aynıdır ve her dosyada tek kez geçer
(ölçüldü). Mevcut işlevler iki dosyada da VAR: `dizi_al/dizi_ekle/dizi_boyut/dizi_yaz`,
`metin_esit`, `tip_str`, `bilinen_tip_mi`, `yerel_baglama_mi`, `sekil_hata`,
`kontrol_dugum`, `fn_ptn_bul`, `fn_ptip_bul`, `fn_donus_bul`, `tf_bul`.

### S1. `yapı Ayr` — yeni alanlar
ESKI (iki satır, ilk turda eklendi):
```
    tf_say: Dizi<tam32>;
    tf_tip: Dizi<tam32>;
```
YENI: aynı iki satır + ardından:
```
    // [D1 turbofish] İŞLEV BAŞINA generic parametre tablosu (C islev.tip_paramlar
    // aynası): fg_ad[k] işlev adı, paramları gp_ad[fg_gb[k] .. +fg_gs[k]),
    // fg_dn[k] dönüş tip düğümü (-1 = yok). gp_ad (D-370) kapsam-duyarsız düz
    // listedir; SIRAYI yalnız bu tablo tutar.
    fg_ad: Dizi<metin>;
    fg_gb: Dizi<tam32>;
    fg_gs: Dizi<tam32>;
    fg_dn: Dizi<tam32>;
    // [D1 turbofish] tip_str İKAMESİ: ts_n > 0 iken TIP_BASIT adı ts_ad[i] ise
    // ts_tip[i] döner (mantıksal uzunluk ts_n; dizi küçülmez, yeniden yazılır).
    ts_ad: Dizi<metin>;
    ts_tip: Dizi<metin>;
    ts_n: tam32;
```

### S2. `ayr_olustur` — başlat
ESKI (tek satır, ilk turda eklendi):
`        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: []`
YENI:
`        tf_cagri: [], tf_bas: [], tf_say: [], tf_tip: [],`
`        fg_ad: [], fg_gb: [], fg_gs: [], fg_dn: [], ts_ad: [], ts_tip: [], ts_n: 0`

### S3. `parse_islev_genel` — generic param dilimini yakala
Bu blok İKİ dosyada FARKLI ESKI kullanır:
- `selfhost/checker.kem` ESKI (iki satır):
  ```
      atla_tip_paramlar(p);
      bekle_k(p, "SOL_PAREN", "P015");   // [D-650] C parser.c:343 imza "("
  ```
  YENI:
  ```
      değişken fg_gp0: tam32 = dizi_boyut(p.gp_ad);   // [D1 turbofish]
      atla_tip_paramlar(p);
      değişken fg_gp1: tam32 = dizi_boyut(p.gp_ad);
      bekle_k(p, "SOL_PAREN", "P015");   // [D-650] C parser.c:343 imza "("
  ```
- `selfhost/codegen.kem` ESKI (üç satır):
  ```
      bekle(p, "TANIMLAYICI");
      atla_tip_paramlar(p);
      // D-401: İŞLEVİN generic param adlarını KAYDET. Öncesinde `atla_tip_paramlar`
  ```
  YENI:
  ```
      bekle(p, "TANIMLAYICI");
      değişken fg_gp0: tam32 = dizi_boyut(p.gp_ad);   // [D1 turbofish]
      atla_tip_paramlar(p);
      değişken fg_gp1: tam32 = dizi_boyut(p.gp_ad);
      // D-401: İŞLEVİN generic param adlarını KAYDET. Öncesinde `atla_tip_paramlar`
  ```

### S4. `parse_islev_genel` — dönüş düğümünü yakala + kaydet
ESKI (iki dosyada aynı, tek satır):
`    eğer sim_mi(p, "OK") { ilerle(p); dizi_ekle(kids, parse_tip(p)); }`
YENI:
```
    değişken fg_dn: tam32 = 0 - 1;   // [D1 turbofish] dönüş tip düğümü
    eğer sim_mi(p, "OK") { ilerle(p); fg_dn = parse_tip(p); dizi_ekle(kids, fg_dn); }
    dizi_ekle(p.fg_ad, ad); dizi_ekle(p.fg_gb, fg_gp0);
    dizi_ekle(p.fg_gs, fg_gp1 - fg_gp0); dizi_ekle(p.fg_dn, fg_dn);
```
(`ad` değişkeni o işlevde zaten tanımlı: `değişken ad: metin = sim_lex(p);`.)

### S5. `tip_str` — TIP_BASIT ikamesi + yardımcılar
ESKI (iki dosyada aynı, tek satır):
`    eğer metin_esit(dizi_al(p.a_ad, idx), "TIP_BASIT") { ver dizi_al(p.a_deg, idx); }`
YENI:
`    eğer metin_esit(dizi_al(p.a_ad, idx), "TIP_BASIT") { ver ts_coz(p, dizi_al(p.a_deg, idx)); }`

Ve `tip_str` işlevinin HEMEN ÖNCESİNE yardımcılar. ESKI (iki dosyada aynı, tek satır):
`işlev tip_str(p: &değişken Ayr, idx: tam32) -> metin {`
YENI: aşağıdaki işlevler + o satırın kendisi:
```
// [D1 turbofish] tip_str ikamesi: generic param adı → açık tip dizgisi.
işlev ts_coz(p: &değişken Ayr, ad: metin) -> metin {
    değişken i: tam32 = 0;
    iken i < p.ts_n {
        eğer metin_esit(dizi_al(p.ts_ad, i), ad) { ver dizi_al(p.ts_tip, i); }
        i = i + 1;
    }
    ver ad;
}
// İşlev adı → fg kaydı (-1 = yok). İLK eşleşme (fn_* tablolarıyla aynı kural).
işlev fg_bul(p: &değişken Ayr, ad: metin) -> tam32 {
    değişken i: tam32 = 0;
    iken i < dizi_boyut(p.fg_ad) {
        eğer metin_esit(dizi_al(p.fg_ad, i), ad) { ver i; }
        i = i + 1;
    }
    ver 0 - 1;
}
// CAGRI → AÇIKÇA bağlanan generic işlevin fg kaydı. -1: turbofish yok, hedef
// TANIMLAYICI değil, yerel bağlama gölgeliyor, işlev yok ya da sayı uyuşmuyor.
işlev tf_fg(p: &değişken Ayr, cagri: tam32) -> tam32 {
    değişken k: tam32 = tf_bul(p, cagri);
    eğer k < 0 { ver 0 - 1; }
    değişken h: tam32 = dizi_al(p.cocuk, dizi_al(p.a_cb, cagri));
    eğer metin_esit(dizi_al(p.a_ad, h), "TANIMLAYICI") == yanlış { ver 0 - 1; }
    eğer yerel_baglama_mi(p, dizi_al(p.a_deg, h)) { ver 0 - 1; }
    değişken g: tam32 = fg_bul(p, dizi_al(p.a_deg, h));
    eğer g < 0 { ver 0 - 1; }
    eğer dizi_al(p.fg_gs, g) != dizi_al(p.tf_say, k) { ver 0 - 1; }
    ver g;
}
// node tipini, CAGRI'nın açık argümanlarıyla İKAME ederek dizgiye çevir.
// Bilinmeyen sonuç (iç içe generic, gövde-içi `T`) → "?" (denetim atlanır).
işlev tf_tip_coz(p: &değişken Ayr, cagri: tam32, g: tam32, node: tam32) -> metin {
    eğer node < 0 { ver "?"; }
    değişken k: tam32 = tf_bul(p, cagri);
    değişken n: tam32 = dizi_al(p.tf_say, k);
    // Önce açık tipleri İKAMESİZ çöz (p.ts_n == 0), sonra tabloyu kur.
    değişken tipler: Dizi<metin> = [];
    değişken j: tam32 = 0;
    iken j < n {
        dizi_ekle(tipler, tip_str(p, dizi_al(p.tf_tip, dizi_al(p.tf_bas, k) + j)));
        j = j + 1;
    }
    j = 0;
    iken j < n {
        değişken gad: metin = dizi_al(p.gp_ad, dizi_al(p.fg_gb, g) + j);
        eğer j < dizi_boyut(p.ts_ad) {
            dizi_yaz(p.ts_ad, j, gad);
            dizi_yaz(p.ts_tip, j, dizi_al(tipler, j));
        } değilse {
            dizi_ekle(p.ts_ad, gad);
            dizi_ekle(p.ts_tip, dizi_al(tipler, j));
        }
        j = j + 1;
    }
    p.ts_n = n;
    değişken s: metin = tip_str(p, node);
    p.ts_n = 0;
    eğer bilinen_tip_mi(s) { ver s; }
    ver "?";
}
// [D1 turbofish] T044 ön-denetimi — C turbofish_on_kontrol aynası. CAGRI
// işlemenin EN BAŞINDA; doğru → T044 raporlandı, çağıran erken döner.
// YOL hedefi atlanır (V1: self-host YOL çağrılarını denetlemiyor).
işlev tf_on_kontrol(p: &değişken Ayr, idx: tam32, hedef: tam32) -> mantıksal {
    değişken k: tam32 = tf_bul(p, idx);
    eğer k < 0 { ver yanlış; }
    eğer metin_esit(dizi_al(p.a_ad, hedef), "YOL") { ver yanlış; }
    eğer tf_fg(p, idx) >= 0 { ver yanlış; }
    sekil_hata(p, idx, "T044");
    ver doğru;
}
```

### S6. `kontrol_dugum` CAGRI — en başa ön-denetim
ESKI (iki dosyada aynı, dört satır; dosyada bu dört satırlık dizi TEK kez geçer):
```
    eğer metin_esit(ad, "CAGRI") {
        değişken hb: tam32 = dizi_al(p.a_cb, idx);
        değişken hc: tam32 = dizi_al(p.a_cs, idx);
        değişken hedef: tam32 = dizi_al(p.cocuk, hb);
```
YENI: aynı dört satır + ardından:
```
        eğer tf_on_kontrol(p, idx, hedef) { ver 0; }   // [D1 turbofish] T044
```

### S7. `kontrol_dugum` CAGRI — açık tiplerin denetimi + per-arg ikame
ESKI (iki dosyada aynı, tek satır):
`                p.tuk_kapali = p.tuk_kapali + 1;`
— DİKKAT: bu satır dosyada birden çok kez geçebilir; o yüzden ESKI olarak şu İKİ satırı kullan:
```
                // Ön-geçişte tüketim KAPALI: C lineeri ikilemiyor (`al(tuket(t))` → OK).
                p.tuk_kapali = p.tuk_kapali + 1;
```
YENI:
```
                // [D1 turbofish] Açık tip düğümleri de denetlenir (C: bağlamada
                // ast_tip_to_bilgi → T011/DRF/...). C sırası: E013'ten SONRA, argüman
                // geçişlerinden ÖNCE.
                değişken tfg: tam32 = tf_fg(p, idx);
                eğer tfg >= 0 {
                    değişken tfk2: tam32 = tf_bul(p, idx);
                    değişken tj: tam32 = 0;
                    iken tj < dizi_al(p.tf_say, tfk2) {
                        kontrol_dugum(p, dizi_al(p.tf_tip, dizi_al(p.tf_bas, tfk2) + tj));
                        tj = tj + 1;
                    }
                }
                // Ön-geçişte tüketim KAPALI: C lineeri ikilemiyor (`al(tuket(t))` → OK).
                p.tuk_kapali = p.tuk_kapali + 1;
```
VE aynı bölgede per-arg param tipi. ESKI (iki dosyada aynı, tek satır):
`                    değişken pt: metin = fn_ptip_bul(p, fad, a - 1);`
YENI:
```
                    değişken pt: metin = fn_ptip_bul(p, fad, a - 1);
                    // [D1 turbofish] açık bağlama: param tipi İKAME edilmiş tiptir
                    // (generic `T` param artık "?" değil → T001 denetlenir).
                    eğer tfg >= 0 { pt = tf_tip_coz(p, idx, tfg, fn_ptn_bul(p, fad, a - 1)); }
```

### S8. `ifade_tip` CAGRI — dönüş tipi ikamesi
ESKI (iki dosyada aynı, tek satır):
`            ver fn_donus_bul(p, dizi_al(p.a_deg, h));   // "?" = kullanıcı işlevi değil`
YENI:
```
            // [D1 turbofish] açık bağlamada dönüş tipi İKAME edilir (C gen_substitue).
            değişken tfg3: tam32 = tf_fg(p, idx);
            eğer tfg3 >= 0 {
                eğer dizi_al(p.fg_dn, tfg3) >= 0 { ver tf_tip_coz(p, idx, tfg3, dizi_al(p.fg_dn, tfg3)); }
            }
            ver fn_donus_bul(p, dizi_al(p.a_deg, h));   // "?" = kullanıcı işlevi değil
```

Toplam blok: C 5 + self-host her dosyada 10 (S1, S2, S3, S4, S5×2, S6, S7×2, S8) = 25. Hepsini yaz.

````
