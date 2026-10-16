# Tur 16 — mmu_x86 — istem

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

## runtime/mimari/x86_64/kem_gorev_hw.kem
```
// kem_gorev_hw.kem — x86_64 görev/zamanlayıcı DONANIM kancaları (D2b, D-685).
// ============================================================================
// arm64 ikizi: runtime/mimari/arm64/kem_gorev_hw.kem. Model: IRQ0 koçanı 15 GP
// yazmacını iter, kdl_irq_isle(sp) çağırır, dönen SP'ye geçer → çerçeve düzeni:
//   [0]=r15 … [14]=rax (@0..112), RIP@120, CS@128, RFLAGS@136, RSP@144, SS@152;
//   toplam 160 B. Yeni görev = SENTETİK çerçeve (kg_cerceve_kur).
// Bu ara hedefte görev başına adres alanı YOK → kg_alan_* boş (arm64 TTBR0 yok).

sabit KG_PSP: tam64 = 50331648;           // 0x03000000: psp[i] tablosu (@+i*8)
sabit KG_OLU: tam64 = 50334720;           // 0x03000C00: olu[i] tablosu (1=bitmiş)
sabit KG_G1: tam64 = 50335744;            // 0x03001000: görev1 sayacı (FİXED RAM, volatile deref)
sabit KG_G2: tam64 = 50335752;            // 0x03001008: görev2 sayacı
sabit KG_AKTIF: tam64 = 50335760;         // 0x03001010: preempt-aktif bayrağı (FİXED RAM)
sabit KG_STK1: tam64 = 51380224;          // 0x03100000: görev1 yığın tepesi
sabit KG_STK2: tam64 = 52428800;          // 0x03200000: görev2 yığın tepesi

// Görev giriş adreslerini asm ile materyalize et (.kem işlev-adresi → tam64).
çıplak işlev kg_gorev1_adr() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz { satıriçi_asm { mimari: x86_64 şablon: r#"leaq kem_gorev1(%rip), $0"# çıktı("=r", &x) } }
    ver x;
}
çıplak işlev kg_gorev2_adr() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz { satıriçi_asm { mimari: x86_64 şablon: r#"leaq kem_gorev2(%rip), $0"# çıktı("=r", &x) } }
    ver x;
}

// Sentetik çerçeve kur (arm64 kg_cerceve_kur ikizi): 15 GP slotu sıfırla; ardından
// CPU'nun ittiği iretq çerçevesi — RIP=giriş, CS=8, RFLAGS=0x202 (IF=1),
// RSP=ust-8 (call sonrası hizası), SS=16. SP 16-hizalı.
çıplak işlev kg_cerceve_kur(yigin_tepe: tam64, giris: tam64) -> tam64 {
    değişken ust: tam64 = (yigin_tepe / 16) * 16;         // 16-hizala
    değişken sp: tam64 = ust - 160;
    değişken k: tam64 = 0;
    iken k < 15 { kg_yaz64(sp + k * 8, 0); k = k + 1; }   // 15 GP slotu sıfırla
    kg_yaz64(sp + 120, giris);                    // RIP @120 = görev girişi
    kg_yaz64(sp + 128, 8);                        // CS @128 = 8 (çekirdek kodu)
    kg_yaz64(sp + 136, 514);                      // RFLAGS @136 = 0x202 (IF=1)
    kg_yaz64(sp + 144, ust - 8);                  // RSP @144 (call sonrası hizası)
    kg_yaz64(sp + 152, 16);                       // SS @152 = 16 (çekirdek verisi)
    ver sp;
}

// x86 ara hedefinde görev başına adres alanı YOK → kancalar boş (arm64 TTBR0 yok).
çıplak işlev kg_alan_sifirla() { }
çıplak işlev kg_alan_gec(i: tam64) { }

// IRQ0 koçanının GÜÇLÜ tanımı (arm64'te karşılığı kem_zaman_hw.kem'deki kdl_irq_isle).
// .S içindeki zayıf varsayılanı ezer → kem_preempt ile SP takası yapılır.
çıplak işlev kdl_irq_isle(sp: tam64) -> tam64 { ver kem_preempt(sp); }

// [D-686] int 0x80 dağıtıcısı — boot/start_x86_64.S kdl_syscall_stub çağırır
// (rdi=num, rsi=arg, dönüş rax). Güçlü tanım C bm_x86_kesme.o'daki ZAYIF olanı ezer.
// Servisler mimariden bağımsız (kem_syscall_temel, kem_gorev_cekirdek.kem) — arm64
// kdl_syscall_isle ile AYNI yanıt. Bilinmeyen numara → -1.
çıplak işlev kdl_syscall_isle(num: tam64, arg: tam64, arg2: tam64) -> tam64 {
    eğer kem_syscall_temel_mi(num) { ver kem_syscall_temel(num, arg); }
    ver 0 - 1;
}

```

## runtime/kem_mmu.kem — KESİT (satır 20-86; düzenleme hedefi: `runtime/kem_mmu.kem`, blok yolu `runtime/kem_mmu.kem` olmalı)
```
// Haritasız test VA (0x80000000 = 2147483648): C-MMU L1[2..511]=0 → level-1 çeviri-fault.
sabit KMMU_FAULT_VA: tam64 = 2147483648;

// --- Recovery-scratch erişimi: C-global'lere inline-asm (adrp/add + ldr/str). x9=scratch. ---
çıplak işlev kmmu_bekle_yaz(v: tam64) {
    güvensiz {
        satıriçi_asm {
            mimari: arm64
            şablon: r#"adrp x9, kdl_fault_bekleniyor
add x9, x9, :lo12:kdl_fault_bekleniyor
str $0, [x9]"#
            girdi("r", v)
            bozulan("~{x9}") bozulan("~{memory}")
        }
    }
}
çıplak işlev kmmu_yakala_yaz(v: tam64) {
    güvensiz {
        satıriçi_asm {
            mimari: arm64
            şablon: r#"adrp x9, kdl_fault_yakalanan
add x9, x9, :lo12:kdl_fault_yakalanan
str $0, [x9]"#
            girdi("r", v)
            bozulan("~{x9}") bozulan("~{memory}")
        }
    }
}
çıplak işlev kmmu_bekle_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: arm64
            şablon: r#"adrp x9, kdl_fault_bekleniyor
add x9, x9, :lo12:kdl_fault_bekleniyor
ldr $0, [x9]"#
            çıktı("=r", &x)
            bozulan("~{x9}") bozulan("~{memory}")
        }
    }
    ver x;
}
çıplak işlev kmmu_yakala_oku() -> tam64 {
    değişken x: tam64 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: arm64
            şablon: r#"adrp x9, kdl_fault_yakalanan
add x9, x9, :lo12:kdl_fault_yakalanan
ldr $0, [x9]"#
            çıktı("=r", &x)
            bozulan("~{x9}") bozulan("~{memory}")
        }
    }
    ver x;
}

// Haritasız VA'yı volatile oku → GERÇEK data-abort (tek ldr; .S ELR+=4 ile atlar).
// Dönen değer ÇÖP (fault sırasında yakalanır) — çağıran atar.
çıplak işlev kmmu_oku_faulting(adr: tam64) -> tam64 {
    değişken p: *tam64 = adr olarak *tam64;
    ver *p;
}

// A2 (D-278): GERÇEK syndrome oku — mrs ESR_EL1 / FAR_EL1. Trap SONRASI (eret sonrası)
// bu register'lar bir-sonraki istisnaya dek fault değerlerini TUTAR (ARMv8: yalnız istisna
// girişinde yazılır). .S kdl_exc_ortak yalnız OKUR (mrs) → değiştirmez → post-recovery geçerli.
```

## runtime/kem_mmu.kem — KESİT (satır 333-345; düzenleme hedefi: `runtime/kem_mmu.kem`, blok yolu `runtime/kem_mmu.kem` olmalı)
```
// 1 = non-identity çeviri donanımda çalışıyor (identity olsaydı VA→PA 0x50000000
// backed-değil, alias olmazdı). Taklit edilemez: iki farklı VA aynı PA'yı görüyor.
çıplak işlev kmmu_ceviri_testi() -> tam32 {
    kmmu_y64(KMMU_CEVIRI_PA, 3735928559);            // 0xDEADBEEF → identity VA (=PA)
    değişken a: tam64 = kmmu_o64(KMMU_CEVIRI_VA);     // non-identity VA'dan oku → alias mı?
    kmmu_y64(KMMU_CEVIRI_VA, 3405691582);            // 0xCAFEBABE → non-identity VA (ters yön)
    değişken b: tam64 = kmmu_o64(KMMU_CEVIRI_PA);     // identity VA'dan oku → alias mı?
    eğer a == 3735928559 ve b == 3405691582 {
        ver 1;
    }
    ver 0;
}

```

## runtime/kem_mmu.kem — KESİT (satır 623-636; düzenleme hedefi: `runtime/kem_mmu.kem`, blok yolu `runtime/kem_mmu.kem` olmalı)
```
// GERÇEK fault-gate: haritasız VA oku → .S recovery → FAR doğru + kurtarıldı mı?
// 1 = MMU zorluyor + gerçek fault yakalandı + FAR==VA + OS devam ediyor. Sentetik DEĞİL.
çıplak işlev kmmu_fault_testi() -> tam32 {
    kmmu_yakala_yaz(0);                                        // FAR sonucu temizle
    kmmu_bekle_yaz(1);                                         // .S recovery iste (yoksa halt)
    değişken cop: tam64 = kmmu_oku_faulting(KMMU_FAULT_VA);    // GERÇEK fault → .S recovery
    değişken bekle: tam64 = kmmu_bekle_oku();                  // .S temizledi mi? (0 = kurtarıldı)
    değişken far: tam64 = kmmu_yakala_oku();                   // yakalanan FAR
    eğer bekle == 0 ve far == KMMU_FAULT_VA {
        ver 1;
    }
    ver 0;
}

```

# GÖREV

# Tur 16 — x86 MMU çeviri + kurtarılabilir #PF (.kem)

Tek bir YENİ dosya yaz: `runtime/mimari/x86_64/kem_mmu_hw.kem`. Başka dosyaya DOKUNMA.
Biçim: ```yeni:runtime/mimari/x86_64/kem_mmu_hw.kem bloğu.

Bağlamdaki `runtime/kem_mmu.kem` (arm64) ikizidir: aynı TEST ADLARI (`kmmu_ceviri_testi`,
`kmmu_fault_testi`) ve aynı test mantığı, x86 donanımıyla. Dosyada YALNIZ `mimari: x86_64`
asm olabilir (arm64 asm YASAK — AS001). Tüm işlevler `çıplak işlev`. Dizgi literalinde kaçış
yoktur. Sayılar ondalık yazılır (aşağıdakiler araçla doğrulandı, AYNEN kullan).

## Sabitler (aynen)
```
sabit KXM_PD: tam64 = 58720256;          // 0x3800000 (56 MB) yeni PD (PDPT[1] için, 1..2 GB)
sabit KXM_PT: tam64 = 58724352;          // 0x3801000 4KB-sayfa tablosu
sabit KMMU_CEVIRI_VA: tam64 = 1342177280; // 0x50000000 (PD indeksi 128, PT indeksi 0)
sabit KMMU_CEVIRI_PA: tam64 = 60817408;   // 0x3A00000 (58 MB) identity-eşli boş RAM
sabit KMMU_FAULT_VA: tam64 = 1610612736;  // 0x60000000 (PD[256] boş → #PF)
```

## İşlevler
1. `kxm_y64(adr: tam64, deger: tam64)` ve `kxm_o64(adr: tam64) -> tam64`: `*tam64` ham
   işaretçi ile yaz/oku (arm `kmmu_y64`/`kmmu_o64` ile birebir aynı gövde).
2. `kxm_cr3_oku() -> tam64`: asm `movq %cr3, $0`, çıktı("=r", &x).
3. `kxm_cr3_yaz(v: tam64)`: asm `movq $0, %cr3`, girdi("r", v), bozulan("~{memory}").
   (CR3 yeniden yazmak TLB'yi temizler.)
4. `kxm_cerceve(g: tam64) -> tam64`: tablo girdisinden çerçeve adresi = `g - (g % 4096)`.
5. `kdl_mmu_kur_x86()`:
   - KXM_PD'den başlayan 512 girdiyi ve KXM_PT'den başlayan 512 girdiyi 0 yap
     (`değişken i: tam64 = 0; iken i < 512 { ...; i = i + 1; }`, adres = taban + i * 8).
   - `kxm_y64(KXM_PT, KMMU_CEVIRI_PA + 3);`            // PT[0] = PA | P|RW
   - `kxm_y64(KXM_PD + 128 * 8, KXM_PT + 3);`          // PD[128] = PT | P|RW
   - `değişken pml4: tam64 = kxm_cerceve(kxm_cr3_oku());`
   - `değişken pdpt: tam64 = kxm_cerceve(kxm_o64(pml4));`  // PML4[0] → PDPT
   - `kxm_y64(pdpt + 8, KXM_PD + 3);`                   // PDPT[1] = PD | P|RW
   - `kxm_cr3_yaz(kxm_cr3_oku());`                      // TLB temizle
6. `kmmu_ceviri_testi() -> tam32`: arm sürümünün gövdesi BİREBİR
   (0xDEADBEEF = 3735928559 PA'ya yaz → VA'dan oku; 0xCAFEBABE = 3405691582 VA'ya yaz →
   PA'dan oku; ikisi tutarsa 1, yoksa 0). `kxm_y64`/`kxm_o64` kullan.
7. Kurtarma değişkenleri `.S`'de: `kdl_x86_pf_bekle` ve `kdl_x86_pf_cr2` (.data, 8 bayt).
   Erişim RIP-göreli: `kxm_bekle_yaz(v)` asm `leaq kdl_x86_pf_bekle(%rip), %r11` + yeni
   satır + `movq $0, (%r11)`, girdi("r", v), bozulan("~{r11}") bozulan("~{memory}").
   Aynı desenle `kxm_bekle_oku() -> tam64` (`movq (%r11), $0`, çıktı("=r", &x)),
   `kxm_cr2_yaz(v)`, `kxm_cr2_oku() -> tam64` (sembol kdl_x86_pf_cr2).
8. `kxm_oku_faulting(adr: tam64) -> tam64`: SABİT YAZMAÇLI tek komut — asm şablonu
   tam olarak `movq (%rdi), %rax`, girdi("{di}", adr), çıktı("={ax}", &x),
   bozulan("~{memory}"). Bu kodlama 3 bayttır (48 8B 07); `.S` RIP'i bu kadar ilerletir.
9. `kmmu_fault_testi() -> tam32`:
   - `kxm_cr2_yaz(0); kxm_bekle_yaz(3);`            // 3 = atlanacak komut uzunluğu
   - `değişken cop: tam64 = kxm_oku_faulting(KMMU_FAULT_VA);`
   - `değişken bekle: tam64 = kxm_bekle_oku(); değişken cr2: tam64 = kxm_cr2_oku();`
   - `eğer bekle == 0 ve cr2 == KMMU_FAULT_VA { ver 1; } ver 0;`

Her işlevin üstüne bir satırlık Türkçe yorum yaz; dosya başına kısa bir açıklama
(arm ikizi, [D-687], RAM yerleşimi: heap ~17 MB, zamanlayıcı 48–50 MB, MMU 56–58 MB).

````
