# Tur 25 — turbofish_codegen — istem

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

## src/llvm.c — KESİT (satır 3226-3270; düzenleme hedefi: `src/llvm.c`, blok yolu `src/llvm.c` olmalı)
```
 * uyelerine cozulur). Use-site baglaminda YENIDEN cozum YOK. */

/* Generic islev'in i. parametresi icin codegen-beklenen IR tipi.
 * Generic-param iceren param (T, *T, &K<T>) -> NULL (T inference arg'in
 * dogal tipinden gelir); somut param -> IR tipi. Non-generic islevde tum
 * paramlar somut sayilir. (TANIMLAYICI yolundaki 2439-2478 ile ozdes
 * mantik; tek dogruluk kaynagi.) */
static const char *generic_param_beklenen(LlvmGen *g, const Dugum *islevd,
                                          int i) {
    if (i >= islevd->veri.islev.param_sayi) return NULL;
    const Dugum *pp = islevd->veri.islev.parametreler[i];
    const Dugum *ppt = pp ? pp->veri.parametre.tip : NULL;
    if (!ppt) return NULL;
    int generic_param_mi = 0;
    if (islevd->veri.islev.tip_param_sayi > 0) {
        for (int gi = 0; gi < islevd->veri.islev.tip_param_sayi; gi++) {
            const char *gp = islevd->veri.islev.tip_paramlar[gi];
            int gp_uz = (int)strlen(gp);
            const Dugum *bak = ppt;
            if (bak->tip == DUGUM_TIP_POINTER) {
                bak = bak->veri.tip_pointer.hedef_tip;
            }
            if (bak && bak->tip == DUGUM_TIP_REFERANS) {
                bak = bak->veri.tip_referans.hedef_tip;
            }
            if (bak && bak->tip == DUGUM_TIP_KULLANICI) {
                generic_param_mi = 1;  /* Liste<T> vb. */
                break;
            }
            if (bak && bak->tip == DUGUM_TIP_BASIT &&
                bak->veri.tip_basit.ad_uzunluk == gp_uz &&
                memcmp(bak->veri.tip_basit.ad, gp, (size_t)gp_uz) == 0) {
                generic_param_mi = 1;
                break;
            }
        }
    }
    if (generic_param_mi) return NULL;
    return ast_tip_to_ir(g, ppt);
}

/* Generic islev cagrisini specialize edip emit et. ALREADY-EVALUATED args
 * alir (her iki yol da kendi beklenen-baglamiyla evaluate eder). gislev =
 * generic islev AST (ad modul-nitelikli olabilir); donus = taban donus IR
 * tipi (ik/mik->donus_tip). Tip args param tiplerinden cikarsanir; mangled
```

## src/llvm.c — KESİT (satır 3272-3300; düzenleme hedefi: `src/llvm.c`, blok yolu `src/llvm.c` olmalı)
```
static IfadeSonuc generic_islev_cagri_uret(LlvmGen *g, const Dugum *d,
                                           const Dugum *gislev,
                                           IfadeSonuc *args, int n,
                                           const char *donus) {
    int tps = gislev->veri.islev.tip_param_sayi;
    const char **tip_args = (const char **)arena_ayir(g->arena,
        sizeof(const char *) * (size_t)tps);
    /* [D-449] tip argumanlarinin metin-ligi (IR dizgisi bunu silmez ama
     * "ptr" her seyi ayni gosterir) — arena_ayir_sifir: varsayilan 0. */
    int *tip_args_metin = (int *)arena_ayir_sifir(g->arena,
        sizeof(int) * (size_t)tps);
    /* Her generic param icin: parametrelerde T'yi bulan ilk arg tipinden
     * çıkar */
    for (int ti = 0; ti < tps; ti++) {
        const char *tp = gislev->veri.islev.tip_paramlar[ti];
        int tp_uz = (int)strlen(tp);
        const char *inferred = NULL;
        for (int pi = 0; pi < gislev->veri.islev.param_sayi &&
                           pi < n && !inferred; pi++) {
            const Dugum *p = gislev->veri.islev.parametreler[pi];
            const Dugum *pt = p->veri.parametre.tip;
            if (!pt) continue;
            if (pt->tip == DUGUM_TIP_BASIT) {
                const char *pad = pt->veri.tip_basit.ad;
                int puz = pt->veri.tip_basit.ad_uzunluk;
                if (puz == tp_uz && memcmp(pad, tp, (size_t)tp_uz) == 0) {
                    inferred = args[pi].tip;
                    if (tip_args_metin) {       /* [D-449] */
                        tip_args_metin[ti] = args[pi].metin_mi;
```

## src/llvm.c — KESİT (satır 3430-3440; düzenleme hedefi: `src/llvm.c`, blok yolu `src/llvm.c` olmalı)
```
                        }
                    }
                }
                continue;
            }
        }
        tip_args[ti] = inferred ? inferred : "i32";
    }
    /* Mangled name */
    const char *mangled = mangle_et(g,
        gislev->veri.islev.ad, gislev->veri.islev.ad_uzunluk,
```

## src/llvm.c — KESİT (satır 4310-4320; düzenleme hedefi: `src/llvm.c`, blok yolu `src/llvm.c` olmalı)
```
                    IfadeSonuc *args = NULL;
                    if (n > 0) {
                        args = (IfadeSonuc *)arena_ayir(g->arena,
                            sizeof(IfadeSonuc) * (size_t)n);
                        for (int i = 0; i < n; i++) {
                            const char *bekle = mik_generic
                                ? generic_param_beklenen(g, mik->ast, i)
                                : NULL;
                            args[i] = ifade_uret(g,
                                d->veri.cagri.argumanlar[i], bekle);
                            /* somut param int genislik uyumu */
```

## src/llvm.c — KESİT (satır 5625-5633; düzenleme hedefi: `src/llvm.c`, blok yolu `src/llvm.c` olmalı)
```
                     * imza-uyumsuz IR -> SEGFAULT. Generic islevde yalniz
                     * SOMUT paramlara beklenen verilir — generic-param
                     * iceren paramlar atlanir (T inference arg'in dogal
                     * tipinden). generic_param_beklenen ile tek-kaynak
                     * (YOL yolu da ayni helper'i kullanir). */
                    if (!bekle && ik && ik->ast) {
                        bekle = generic_param_beklenen(g, ik->ast, i);
                    }
                    /* D-070 (Sınıf A): arg `Dizi<T>` parametresine gidiyorsa
```

## selfhost/codegen.kem — KESİT (satır 3320-3345; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
    ver yanlış;
}
// Çağrı yerinde specialization kaydı. Argüman IR tiplerinden T'leri çıkarsar,
// mangled adı üretir ve gerekiyorsa kuyruğa alır. "" = mono YAPILAMADI (çağıran
// eski davranışa düşer; sessiz yanlış ad üretmektense mono'yu iptal etmek yeğdir).
işlev mono_islev_kayit(p: &değişken Ayr, base: metin, argt: Dizi<metin>, argm: Dizi<metin>, arge: Dizi<metin>) -> metin {
    değişken fn: tam32 = fn_dugum_bul(p, base);
    eğer fn < 0 { ver ""; }
    değişken sp: metin = "";
    değişken si: metin = "";
    değişken sm: metin = "";   // [D-449] sp/si ile paralel metin-lik listesi
    // Mangled ad TAM adı korur (`dizi.al$i64`); registry sorgusu ÇIPLAK adla
    // yapılır (`al`) — D-398 sonrası ikisi farklıdır.
    değişken bare: metin = nokta_son(base);
    değişken mang: metin = base;
    değişken i: tam32 = 0;
    iken i < dizi_boyut(p.tp_yad) {
        eğer metin_esit(dizi_al(p.tp_yad, i), bare) {
            değişken tp: metin = dizi_al(p.tp_ad, i);
            değişken pidx: tam32 = fn_param_idx(p, fn, tp);
            // D-411: ÇIKARSANAMAYAN tip paramı için mono'yu İPTAL ETME —
            // fallback IR ("i32") kullan. D-401'in V1'i iptal ediyordu ve bu
            // `hata_yap<T,E>(e: E) -> sonuç<T,E>` gibi şekillerde KIRIYORDU:
            // T çıplak parametre değil, E ise öyle. İptal edilince çağrı BASE
            // gövdeye (hepsi fallback → `{i8,i32,i32}`) gidiyor ama annotasyon
            // `sonuç<tam32, metin>` = `{i8,i32,ptr}` çözülüyor → store uyuşmazlığı.
```

## selfhost/codegen.kem — KESİT (satır 3410-3430; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
            //
            // ⚠ BU DAL OLMADAN D-411'İN FALLBACK'İ SESSİZ BELLEK BOZULMASI
            // ÜRETİYORDU: `büyü$i32` yayılıyor, büyüme sırasında eleman kopyası
            // 8 bayt yerine 4 bayt genişlikte yapılıyor → ARALIKLI SEGFAULT
            // (ölçüldü: `dizi_kullan` self ikilisi `42 42 139 42`, C 8/8 kararlı).
            // D-411'de "fallback yanlışsa LLVM REDDEDER, hata gürültülü kalır"
            // demiştim — O İDDİA YANLIŞTI: define ve çağrı i32'de ANLAŞIYOR,
            // LLVM kabul ediyor, kusur çalışma anına kayıyor.
            eğer bulundu == yanlış {
                değişken sv: metin = subst_bul(p, tp);
                eğer metin_esit(sv, "") == yanlış { ir = sv; }
            }
            eğer metin_uzunluk(sp) > 0 { sp = metin_birlestir(sp, " "); si = metin_birlestir(si, " "); sm = metin_birlestir(sm, " "); }
            eğer metin_esit(mmet, "1") { sm = metin_birlestir(sm, "1"); }
            değilse { sm = metin_birlestir(sm, "0"); }
            sp = metin_birlestir(sp, tp);
            si = metin_birlestir(si, ir);
            mang = metin_birlestir(metin_birlestir(mang, "$"), mono_ir_sanitize(ir));
        }
        i = i + 1;
    }
```

## selfhost/codegen.kem — KESİT (satır 6070-6100; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
        eğer metin_esit(fad, "soket_baglan") { i = 2; }
        iken i < cs {
            // [D-465] ARGÜMAN BAĞLAMI: parametrenin IR tipi bir tagged-union
            // (`{...}` = seçimlik/sonuç) ise `beklenen_ll`i ÖNCEDEN kur; yoksa
            // `f(hiç)` / `f(değer(42))` yapıcıları çözülemiyor ve `@değer`
            // tanımsız kalıyor (LINK-RED). C'de aynı boşluk vardı ve orada da
            // bağlam YALNIZ `Dizi<T>` için kuruluyordu (D-070) — mekanizma
            // vardı, kapısı dardı.
            değişken ab_eski: metin = p.beklenen_ll;
            eğer kullanici == 1 {
                değişken apt: metin = param_ir_tip(p, fname, i - 1);
                eğer metin_uzunluk(apt) > 0 {
                    eğer bayt(apt, 0) == 123 { p.beklenen_ll = apt; }
                }
            }
            değişken av: metin = ifade_uret(p, dizi_al(p.cocuk, cb + i));
            p.beklenen_ll = ab_eski;
            değişken at: metin = p.son_tip;
            // D-393: ARGÜMAN, PARAMETRENİN BİLDİRİLEN IR tipine genişletilmeli.
            // Eskiden argümanın DOĞAL tipi basılıyordu → `f(a: dtam64, ..)` için
            // `call @f(i32 10, ..)`; `define` i64 bekliyor. LLVM bunu SESSİZCE
            // kabul eder (D-295) → ilk 3 argüman register'a sığdığı için doğru
            // görünür, 4.'den itibaren ÇÖP okunur (D-392'de ölçüldü).
            // Tablo codegen'in KENDİ ön-geçişinden (`param_ir_topla`); checker'ın
            // `fn_ptip` tablosu bu yolda BOŞTUR ve sınır-güvensizdir.
            eğer kullanici == 1 {
                değişken pt: metin = param_ir_tip(p, fname, i - 1);
                eğer metin_esit(pt, "i64") ve (metin_esit(at, "i64") == yanlış) {
                    av = i64_genislet(p, av, at);
                    at = "i64";
                }
```

## selfhost/codegen.kem — KESİT (satır 6128-6140; düzenleme hedefi: `selfhost/codegen.kem`, blok yolu `selfhost/codegen.kem` olmalı)
```
        // D-401: GENERIC İŞLEV MONOMORFİZASYONU. Burada yapılmalı — T argüman IR
        // tiplerinden çıkarsanır ve `argt` ancak arg döngüsünden SONRA doludur.
        // Mono yapılamazsa (`""`) eski davranışa düşülür: yanlış bir ad uydurmak
        // yerine mevcut (kusurlu ama tanıdık) yolu korumak yeğdir.
        eğer kullanici == 1 {
            eğer fn_generic_mi(p, fname) {
                değişken spec: metin = mono_islev_kayit(p, fname, argt, argm, arge);
                eğer metin_esit(spec, "") == yanlış {
                    // Dönüş tipi de T'ye bağlı → ikame ALTINDA yeniden hesapla.
                    değişken fnn: tam32 = fn_dugum_bul(p, fname);
                    değişken sbas: tam32 = dizi_boyut(p.mono_sp);
                    değişken si_i: tam32 = 0;
                    iken si_i < dizi_boyut(p.fs_ad) {
```

# GÖREV

# Tur 25 — turbofish CODEGEN: açık argüman monomorfizasyonu EZER

Parser (tur 23) ve checker (tur 24) hazır:
- C: CAGRI düğümü `d` için `d->veri.cagri.tip_arg[ti]` (tip düğümleri),
  `d->veri.cagri.tip_arg_sayi` (yoksa 0).
- Self-host: `tf_bul(p, idx)` → k (yoksa -1); tip düğümleri
  `dizi_al(p.tf_tip, dizi_al(p.tf_bas, k) + j)`, j < `dizi_al(p.tf_say, k)`.

Hedef: `kare::<tam64>(92682)` → `@kare$i64` specialization'ı, argüman `i64`
olarak yayılır (bugün çıkarsama literalden `$i32` seçer ve 92682² taşar).
Açık argüman, argümanlardan ÇIKARSAMAYI EZER. Yalnız iki dosya:
`src/llvm.c` ve `selfhost/codegen.kem`. Başka dosyaya DOKUNMA. Her değişiklik
bir ```duzenle bloğu; ESKI metni harfiyen kopyala (tek kez geçer — ölçüldü).
KEMGU dizgi literallerinde KAÇIŞ YOK.

## C — `src/llvm.c` (4 blok)

### C1. `generic_param_beklenen` — imza + açık argüman dalı
ESKI (tam olarak, altı satır):
```
static const char *generic_param_beklenen(LlvmGen *g, const Dugum *islevd,
                                          int i) {
    if (i >= islevd->veri.islev.param_sayi) return NULL;
    const Dugum *pp = islevd->veri.islev.parametreler[i];
    const Dugum *ppt = pp ? pp->veri.parametre.tip : NULL;
    if (!ppt) return NULL;
```
YENI:
```
static const char *generic_param_beklenen(LlvmGen *g, const Dugum *d,
                                          const Dugum *islevd, int i) {
    if (i >= islevd->veri.islev.param_sayi) return NULL;
    const Dugum *pp = islevd->veri.islev.parametreler[i];
    const Dugum *ppt = pp ? pp->veri.parametre.tip : NULL;
    if (!ppt) return NULL;
    /* [D1 turbofish] ACIK generic argumanli cagri (`f::<tam64>(..)`): generic
     * parametre de SOMUTTUR -> beklenen IR, parametre tipinin acik argumanlarla
     * IKAME edilmis halidir. Literal `92682` boylece i64 dogar (bugun cikarsama
     * onu i32 sayar ve `kare(92682)` tasar). Acik argumanlarin IR'i DIS ikame
     * baglaminda (cagiran specialization'in T'si) hesaplanir — hepsi ONCE
     * hesaplanir, sonra itilir (callee'nin `T`si cagiranin `T`sini golgelemesin). */
    int tf_n = (d && d->tip == DUGUM_CAGRI) ? d->veri.cagri.tip_arg_sayi : 0;
    int tps0 = islevd->veri.islev.tip_param_sayi;
    if (tf_n > 0 && tf_n == tps0) {
        TipSubst *ts = (TipSubst *)arena_ayir_sifir(g->arena,
            sizeof(TipSubst) * (size_t)tps0);
        if (ts) {
            for (int ti = 0; ti < tps0; ti++) {
                ts[ti].ad = islevd->veri.islev.tip_paramlar[ti];
                ts[ti].ad_uz = (int)strlen(ts[ti].ad);
                ts[ti].ir = ast_tip_to_ir(g, d->veri.cagri.tip_arg[ti]);
                ts[ti].metin_mi = tip_metin_mi_subst(g, d->veri.cagri.tip_arg[ti]);
            }
            TipSubst *eski = g->substler;
            for (int ti = 0; ti < tps0; ti++) {
                ts[ti].sonraki = g->substler;
                g->substler = &ts[ti];
            }
            const char *r = ast_tip_to_ir(g, ppt);
            g->substler = eski;
            return r;
        }
    }
```

### C2 ve C3. İki çağrı yeri — `d` geçir
ESKI (tam olarak): `                                ? generic_param_beklenen(g, mik->ast, i)`
YENI: `                                ? generic_param_beklenen(g, d, mik->ast, i)`

ESKI (tam olarak): `                        bekle = generic_param_beklenen(g, ik->ast, i);`
YENI: `                        bekle = generic_param_beklenen(g, d, ik->ast, i);`

### C4. `generic_islev_cagri_uret` — açık argüman tip_args'ı EZER
ESKI (tam olarak, tek satır):
`        tip_args[ti] = inferred ? inferred : "i32";`
YENI:
```
        tip_args[ti] = inferred ? inferred : "i32";
        /* [D1 turbofish] ACIK argumanlar cikarsamayi EZER (checker'daki
         * gen_bagla "ilk gorulen kazanir" ile ayni karar). IR, DIS ikame
         * baglaminda (g->substler) hesaplanir. Metin-lik de acik tipten gelir
         * (D-449: generic govdede `==` icerik mi isaretci mi). */
        if (d->veri.cagri.tip_arg_sayi == tps) {
            tip_args[ti] = ast_tip_to_ir(g, d->veri.cagri.tip_arg[ti]);
            if (tip_args_metin) {
                tip_args_metin[ti] = tip_metin_mi_subst(g,
                    d->veri.cagri.tip_arg[ti]);
            }
        }
```

## Self-host — `selfhost/codegen.kem` (5 blok)

Mevcut ve kullanılacak işlevler (dosyada VAR): `tf_bul`, `fn_dugum_bul`, `nokta_son`,
`ll_tip`, `subst_kapat`, `param_ir_tip`, `dizi_*`, `metin_esit`. `p.tp_yad`/`p.tp_ad`
generic param kayıt defteri (işlev adı → param adları, bildirim SIRASIYLA);
`p.mono_sp`/`p.mono_si`/`p.mono_smet` aktif ikame yığını (`subst_bul` sondan arar;
`subst_kapat(p, bas)` bas'tan itibaren adları "" yapar).

### S1. Yardımcılar — `mono_islev_kayit` tanımından hemen ÖNCE
ESKI (tam olarak, tek satır):
`işlev mono_islev_kayit(p: &değişken Ayr, base: metin, argt: Dizi<metin>, argm: Dizi<metin>, arge: Dizi<metin>) -> metin {`
YENI: aşağıdaki işlevler + ESKI satırın YENİ hâli (S2'de verildi — bu blokta ESKI
satırı S2'deki YENİ imzayla değiştir; yani S1 ve S2 TEK blok olarak yazılır):
```
// [D1 turbofish] CAGRI'nın AÇIK tip argümanlarının IR'ları (aktif ikame altında —
// çağıran bir specialization'ın gövdesindeysek onun T'si çözülür). Yoksa [].
işlev tf_acik_ir(p: &değişken Ayr, idx: tam32) -> Dizi<metin> {
    değişken r: Dizi<metin> = [];
    değişken k: tam32 = tf_bul(p, idx);
    eğer k < 0 { ver r; }
    değişken j: tam32 = 0;
    iken j < dizi_al(p.tf_say, k) {
        dizi_ekle(r, ll_tip(p, dizi_al(p.tf_tip, dizi_al(p.tf_bas, k) + j)));
        j = j + 1;
    }
    ver r;
}
// [D-449] açık tip `metin` mi? ("1" / ""). Generic param adıysa aktif ikamenin
// metin-liğine bakar (mono_smet).
işlev tf_tip_metin(p: &değişken Ayr, n: tam32) -> metin {
    eğer metin_esit(dizi_al(p.a_ad, n), "TIP_BASIT") == yanlış { ver ""; }
    değişken ad: metin = dizi_al(p.a_deg, n);
    eğer metin_esit(ad, "metin") { ver "1"; }
    değişken i: tam32 = dizi_boyut(p.mono_sp) - 1;
    iken i >= 0 {
        eğer metin_esit(dizi_al(p.mono_sp, i), ad) {
            eğer i < dizi_boyut(p.mono_smet) {
                eğer metin_esit(dizi_al(p.mono_smet, i), "1") { ver "1"; }
            }
            ver "";
        }
        i = i - 1;
    }
    ver "";
}
işlev tf_acik_met(p: &değişken Ayr, idx: tam32) -> Dizi<metin> {
    değişken r: Dizi<metin> = [];
    değişken k: tam32 = tf_bul(p, idx);
    eğer k < 0 { ver r; }
    değişken j: tam32 = 0;
    iken j < dizi_al(p.tf_say, k) {
        dizi_ekle(r, tf_tip_metin(p, dizi_al(p.tf_tip, dizi_al(p.tf_bas, k) + j)));
        j = j + 1;
    }
    ver r;
}
// İşlev düğümünün j'inci PARAMETRE'sinin tip düğümü (-1 = yok).
işlev fn_param_tip_dugum(p: &değişken Ayr, fn: tam32, j: tam32) -> tam32 {
    değişken cb: tam32 = dizi_al(p.a_cb, fn);
    değişken cs: tam32 = dizi_al(p.a_cs, fn);
    değişken i: tam32 = 0;  değişken pi: tam32 = 0;
    iken i < cs {
        değişken c: tam32 = dizi_al(p.cocuk, cb + i);
        eğer metin_esit(dizi_al(p.a_ad, c), "PARAMETRE") {
            eğer pi == j {
                eğer dizi_al(p.a_cs, c) > 0 { ver dizi_al(p.cocuk, dizi_al(p.a_cb, c)); }
                ver 0 - 1;
            }
            pi = pi + 1;
        }
        i = i + 1;
    }
    ver 0 - 1;
}
// [D1 turbofish] Açık argümanlı çağrıda j'inci parametrenin IR tipi: parametre
// tip düğümü, AÇIK IR'lar ikame edilmiş halde ll_tip'ten geçer (C
// generic_param_beklenen aynası). Turbofish yok / sayı uyuşmuyor → param_ir_tip.
işlev tf_param_ir(p: &değişken Ayr, idx: tam32, fname: metin, j: tam32) -> metin {
    değişken acik: Dizi<metin> = tf_acik_ir(p, idx);   // DIŞ ikame altında, ÖNCE
    eğer dizi_boyut(acik) == 0 { ver param_ir_tip(p, fname, j); }
    değişken fn: tam32 = fn_dugum_bul(p, fname);
    eğer fn < 0 { ver param_ir_tip(p, fname, j); }
    değişken ptn: tam32 = fn_param_tip_dugum(p, fn, j);
    eğer ptn < 0 { ver param_ir_tip(p, fname, j); }
    değişken bare: metin = nokta_son(fname);
    değişken bas: tam32 = dizi_boyut(p.mono_sp);
    değişken gi: tam32 = 0;
    değişken i: tam32 = 0;
    iken i < dizi_boyut(p.tp_yad) {
        eğer metin_esit(dizi_al(p.tp_yad, i), bare) {
            eğer gi < dizi_boyut(acik) {
                dizi_ekle(p.mono_sp, dizi_al(p.tp_ad, i));
                dizi_ekle(p.mono_si, dizi_al(acik, gi));
                dizi_ekle(p.mono_smet, "");
            }
            gi = gi + 1;
        }
        i = i + 1;
    }
    eğer gi != dizi_boyut(acik) { subst_kapat(p, bas); ver param_ir_tip(p, fname, j); }
    değişken r: metin = ll_tip(p, ptn);
    subst_kapat(p, bas);
    ver r;
}
```

### S2. `mono_islev_kayit` — imza (S1 bloğunun son satırı)
YENİ imza satırı (S1 bloğunun YENI'sinin SON satırı budur):
`işlev mono_islev_kayit(p: &değişken Ayr, base: metin, argt: Dizi<metin>, argm: Dizi<metin>, arge: Dizi<metin>, acik: Dizi<metin>, acikm: Dizi<metin>) -> metin {`

### S3. `mono_islev_kayit` gövdesi — açık IR çıkarsamayı EZER
İki ayrı blok:

(a) ESKI (tam olarak, iki satır):
```
    değişken mang: metin = base;
    değişken i: tam32 = 0;
```
YENI:
```
    değişken mang: metin = base;
    değişken gj: tam32 = 0;   // [D1 turbofish] bu işlevin kaçıncı generic paramı
    değişken i: tam32 = 0;
```

(b) ESKI (tam olarak, tek satır):
`            eğer metin_uzunluk(sp) > 0 { sp = metin_birlestir(sp, " "); si = metin_birlestir(si, " "); sm = metin_birlestir(sm, " "); }`
YENI:
```
            // [D1 turbofish] AÇIK argüman çıkarsamayı EZER (C generic_islev_cagri_uret
            // ve checker gen_bagla ile aynı karar). tp_yad bildirim sırasını tutar.
            eğer gj < dizi_boyut(acik) {
                ir = dizi_al(acik, gj);
                mmet = "";
                eğer gj < dizi_boyut(acikm) { mmet = dizi_al(acikm, gj); }
            }
            gj = gj + 1;
            eğer metin_uzunluk(sp) > 0 { sp = metin_birlestir(sp, " "); si = metin_birlestir(si, " "); sm = metin_birlestir(sm, " "); }
```

### S4. Çağrı yeri — mono kaydına açık IR'lar
ESKI (tam olarak, tek satır):
`                değişken spec: metin = mono_islev_kayit(p, fname, argt, argm, arge);`
YENI:
`                değişken spec: metin = mono_islev_kayit(p, fname, argt, argm, arge, tf_acik_ir(p, idx), tf_acik_met(p, idx));`

### S5. Argüman döngüsü — parametre IR'ı açık argümanlarla
CAGRI ifade üretimindeki argüman döngüsünde `param_ir_tip(p, fname, i - 1)` İKİ kez
geçer (beklenen_ll kurulumu ve D-393 i64 genişletmesi). İkisini de değiştir:

(a) ESKI (tam olarak, tek satır):
`                değişken apt: metin = param_ir_tip(p, fname, i - 1);`
YENI:
`                değişken apt: metin = tf_param_ir(p, idx, fname, i - 1);   // [D1 turbofish]`

(b) ESKI (tam olarak, tek satır):
`                değişken pt: metin = param_ir_tip(p, fname, i - 1);`
YENI:
`                değişken pt: metin = tf_param_ir(p, idx, fname, i - 1);   // [D1 turbofish]`

Toplam blok: C 4 + self-host 6 (S1+S2 tek blok, S3a, S3b, S4, S5a, S5b) = 10.

````
