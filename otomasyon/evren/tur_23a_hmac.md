# Tur 23a — hmac — `deepseek-v4.1-flash`

- Tarih: 2026-10-17 18:33:42 · süre 156.0 sn · finish `stop` · reasoning_effort `low`
- usage: `{"prompt_tokens": 13438, "total_tokens": 24076, "completion_tokens": 10638, "prompt_tokens_details": {"cached_tokens": 11776, "created_cache_tokens": 1600}, "completion_tokens_details": {"reasoning_tokens": 7640}}`
- Değişiklik blokları: 1 — duzenle:stdlib/kripto/karma.kem
- İstem: [tur_23a_hmac_istem.md](tur_23a_hmac_istem.md)

## Yanıt

```duzenle:stdlib/kripto/karma.kem
<<<<<<< ESKI
    iken i < anahtar_uzunluk {
        inner[i] = anahtar[i] ^ sabitsüre_olustur(ipad);
        outer[i] = anahtar[i] ^ sabitsüre_olustur(opad);
        i = i + 1;
    }
    ver 0;
}
=======
    iken i < anahtar_uzunluk {
        inner[i] = anahtar[i] ^ sabitsüre_olustur(ipad);
        outer[i] = anahtar[i] ^ sabitsüre_olustur(opad);
        i = i + 1;
    }
    ver 0;
}

// =============================================================================
// D-XXX (Tur 23a) — Çok-bloklu SHA-256 + HMAC-SHA256 (RFC 2104)
// =============================================================================
//
// sha256_bayt: FIPS 180-4 §5.1 dolgusu (n bayt + 0x80 + sıfırlar + 8-baytlık
// BÜYÜK-UÇ bit uzunluğu; üst 32 bit 0 yeter; toplam 64'ün katı). Her 64-baytlık
// blok için 16 kelime BÜYÜK-UÇ kurulur (w = b0<<24 | b1<<16 | b2<<8 | b3) ve
// sha256_blok_sikistir çağrılır. Durum sha256_H0()'dan başlar; 8 kelimelik
// durum döner.
//
// hmac_sha256: RFC 2104, blok 64 bayt. anahtar > 64 ise önce sha256_bayt ile
// özetlenir (özetin 32 baytı büyük-uç), sonra 64'e sıfırla doldurulur. İç ped
// K ⊕ ipad(0x36), dış ped K ⊕ opad(0x5C). ic = ped || mesaj → sha256_bayt(ic,64+mn);
// dis = ped || iç özet (32 bayt büyük-uç) → sha256_bayt(dis,96).

işlev sha256_bayt(veri: Dizi<sabitsüre<dtam8>>, n: tam32) -> Dizi<sabitsüre<dtam32>> {
    // Toplam dolgulu uzunluk: n + 1 (0x80) + sıfırlar + 8 (uzunluk) → 64'ün katı
    değişken toplam: tam32 = ((n + 9 + 63) / 64) * 64;
    değişken tampon: Dizi<sabitsüre<dtam8>> = [];
    değişken i: tam32 = 0;
    iken i < n {
        dizi_ekle(tampon, veri[i]);
        i = i + 1;
    }
    dizi_ekle(tampon, sabitsüre_olustur(0x80 olarak dtam8));
    iken dizi_boyut(tampon) < toplam - 8 {
        dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    }
    // 8 bayt BÜYÜK-UÇ bit uzunluğu (n*8; üst 32 bit 0 yeter)
    değişken bit_uz: dtam32 = (n olarak dtam32) * 8;
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bit_uz >> 24) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bit_uz >> 16) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bit_uz >> 8) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur((bit_uz & 255) olarak dtam8));

    // Başlangıç durumu (FIPS 180-4 H0)
    değişken H: Dizi<dtam32> = sha256_H0();
    değişken state: Dizi<sabitsüre<dtam32>> = [
        sabitsüre_olustur(H[0]), sabitsüre_olustur(H[1]),
        sabitsüre_olustur(H[2]), sabitsüre_olustur(H[3]),
        sabitsüre_olustur(H[4]), sabitsüre_olustur(H[5]),
        sabitsüre_olustur(H[6]), sabitsüre_olustur(H[7])
    ];

    // Her 64-baytlık bloğu sıkıştır
    değişken blok_sayisi: tam32 = toplam / 64;
    değişken b: tam32 = 0;
    iken b < blok_sayisi {
        değişken blok: Dizi<sabitsüre<dtam32>> = [];
        değişken j: tam32 = 0;
        iken j < 16 {
            değişken off: tam32 = b * 64 + j * 4;
            değişken b0: dtam32 = ifşa(tampon[off]) olarak dtam32;
            değişken b1: dtam32 = ifşa(tampon[off + 1]) olarak dtam32;
            değişken b2: dtam32 = ifşa(tampon[off + 2]) olarak dtam32;
            değişken b3: dtam32 = ifşa(tampon[off + 3]) olarak dtam32;
            değişken w: dtam32 = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
            dizi_ekle(blok, sabitsüre_olustur(w));
            j = j + 1;
        }
        sha256_blok_sikistir(state, blok);
        b = b + 1;
    }
    ver state;
}

işlev hmac_sha256(
    anahtar: Dizi<sabitsüre<dtam8>>, an: tam32,
    mesaj: Dizi<sabitsüre<dtam8>>, mn: tam32
) -> Dizi<sabitsüre<dtam32>> {
    // K'yı 64 bayta normalize et (uzunsa önce hash'le → 32 bayt büyük-uç)
    değişken K: Dizi<sabitsüre<dtam8>> = [];
    değişken i: tam32 = 0;
    eğer an > 64 {
        değişken ozet: Dizi<sabitsüre<dtam32>> = sha256_bayt(anahtar, an);
        iken i < 8 {
            değişken w: dtam32 = ifşa(ozet[i]);
            dizi_ekle(K, sabitsüre_olustur(((w >> 24) & 255) olarak dtam8));
            dizi_ekle(K, sabitsüre_olustur(((w >> 16) & 255) olarak dtam8));
            dizi_ekle(K, sabitsüre_olustur(((w >> 8) & 255) olarak dtam8));
            dizi_ekle(K, sabitsüre_olustur((w & 255) olarak dtam8));
            i = i + 1;
        }
    } değilse {
        iken i < an {
            dizi_ekle(K, anahtar[i]);
            i = i + 1;
        }
    }
    iken dizi_boyut(K) < 64 {
        dizi_ekle(K, sabitsüre_olustur(0 olarak dtam8));
    }

    // İç: (K ⊕ ipad) || mesaj
    değişken ip: dtam8 = 0x36;
    değişken ic: Dizi<sabitsüre<dtam8>> = [];
    i = 0;
    iken i < 64 {
        dizi_ekle(ic, K[i] ^ sabitsüre_olustur(ip));
        i = i + 1;
    }
    i = 0;
    iken i < mn {
        dizi_ekle(ic, mesaj[i]);
        i = i + 1;
    }
    değişken ic_ozet: Dizi<sabitsüre<dtam32>> = sha256_bayt(ic, 64 + mn);

    // Dış: (K ⊕ opad) || iç özet (32 bayt büyük-uç)
    değişken op: dtam8 = 0x5C;
    değişken dis: Dizi<sabitsüre<dtam8>> = [];
    i = 0;
    iken i < 64 {
        dizi_ekle(dis, K[i] ^ sabitsüre_olustur(op));
        i = i + 1;
    }
    i = 0;
    iken i < 8 {
        değişken w2: dtam32 = ifşa(ic_ozet[i]);
        dizi_ekle(dis, sabitsüre_olustur(((w2 >> 24) & 255) olarak dtam8));
        dizi_ekle(dis, sabitsüre_olustur(((w2 >> 16) & 255) olarak dtam8));
        dizi_ekle(dis, sabitsüre_olustur(((w2 >> 8) & 255) olarak dtam8));
        dizi_ekle(dis, sabitsüre_olustur((w2 & 255) olarak dtam8));
        i = i + 1;
    }
    ver sha256_bayt(dis, 96);
}
>>>>>>> YENI
```

**Açıklama.** Mevcut `hmac_pad_uret`'in kapanış parantezine dayanarak dosyanın sonuna iki saf-KEMGU işlev eklendi. `sha256_bayt`, FIPS 180-4 §5.1 dolgusunu uygulayıp her 64-baytlık bloğu 16 big-endian kelimeye çevirir ve var olan `sha256_blok_sikistir`'i çağırır; durum `sha256_H0()`'dan başlar. `hmac_sha256`, RFC 2104 ped yapısını 64-bayt blok varsayımıyla kurar, gerekirse anahtarı önce hash'ler ve 8 kelimelik özeti dizi olarak döner. Hiçbir yeni tanım çakışmıyor; `hata` anahtar sözcüğü kullanılmadı; dizgi literalinde kaçış yok.

VARSAYIMLAR:
- `stdlib/kripto/karma.kem` dosyası kesitteki `hmac_pad_uret` işleviyle (yukarıdaki `ver 0;` ve `}`) bitmektedir.
- `dizi_ekle(d, x)`, `dizi_boyut(d)`, `d[i]` okuma ve `d[i] = x` yazma; hem `Dizi<sabitsüre<dtam8>>` hem `Dizi<sabitsüre<dtam32>>` için geçerlidir.
- `^`/`>>`/`<<`/`&`/`|` işleçleri `sabitsüre<dtam8>` üzerinde çalışıyor; `ifşa(s)` sabitsüre'yi açık değere çeviriyor ve `olarak dtam32`/`olarak dtam8` dönüşümleri kısaltma yapıyor.
- KEMGU'da `değişken` bildirimi aynı işlev içinde blok-yereldir; farklı bloklarda aynı adı yeniden tanımlamak yerine (`w2`) tekil adlar seçildi.

## Düşünme (reasoning)

The task: append functions to stdlib/kripto/karma.kem. Let me find the exact last lines of the file to use as ESKI.

From the context, the last shown section is hmac_pad_uret:

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

I'll use the last few lines as ESKI.

Now I need to write sha256_bayt and hmac_sha256.

Let me think about the details.

sha256_bayt(veri: Dizi<sabitsüre<dtam8>>, n: tam32) -> Dizi<sabitsüre<dtam32>>:
- Padding: n bytes + 0x80 + zeros + 8-byte big-endian bit length.
- Total length multiple of 64.

Compute total: let L = n + 1 (for 0x80), then pad zeros until length ≡ 56 mod 64, then add 8 bytes of length. So total = ((n + 1 + 8 + 63) / 64) * 64, i.e. ((n + 9 + 63)/64)*64. Actually simpler: total = ((n + 8) / 64 + 1) * 64. Let me verify: for n=3: n+8=11, 11/64=0, +1=1, total 64. ✓. For n=55: n+8=63, /64=0, +1=1, total=64. 55 bytes + 0x80 = 56, then 8 length = 64. ✓. For n=56: n+8=64, /64=1, +1=2, total 128. ✓ (56+1=57 needs padding to 56 mod 64... 57>56 so next block).

Actually the formula ((n + 8) / 64 + 1) * 64 gives: when n+8 is exactly a multiple of 64, we get +1... let's check n=56: n+8=64, 64/64=1, +1=2 → 128. Good. n=55: 63/64=0,+1=1→64. Good.

But careful: integer division. n+8/64. For n=119: n+8=127, /64=1, +1=2 → 128. Check: 119 bytes + 0x80 = 120, pad to 120... 120 mod 64 = 56. 120 is already 56 mod 64? 120 = 64+56 = 120. yes 56 mod 64, so add 8 → 128. ✓.

Hmm wait 120 mod 64 = 120-64 = 56. yes. Good.

Total = ((n + 8) / 64 + 1) * 64? For n=0: n+8=8, /64=0, +1=1 → 64. ✓.

But actually general formula total = ((n + 9 + 63)/64)*64 = ((n+72)/64)*64? Let's check n=3: (75/64)=1, *64=64. ✓. n=56: (128/64)=2,*64=128 ✓. n=55: 127/64=1 → 64 ✓. n=0: 72/64=1→64 ✓. n=119: 191/64=2→128 ✓. n=120: 192/64=3→192. Check: 120+1=121, 121 mod 64 = 57>56 so next block, 192. ✓. Good. Either works. I'll use ((n + 9 + 63) / 64) * 64.

Hmm but which is cleaner. Let me use total = ((n + 8) / 64 + 1) * 64? Wait need to double check boundary where n+8 divisible... we did n=56 gives 128, but is 128 correct? 56 bytes, +0x80 = 57, then pad to 56 mod 64: 57 to 120 (56 mod 64), that's 63 zeros, total 120, +8 length = 128. ✓ yes.

Hmm both formulas. Actually ((n+8)/64 + 1)*64: this equals ((n+8)/64)*64 + 64 = floor((n+8)/64)*64 + 64. And ((n+72)/64)*64. For n=56: floor(64/64)*64+64 = 64+64=128. floor(128/64)*64=128. same. For n=8: floor(16/64)*64+64 = 64. floor(80/64)*64 = 64. same. For n=63: floor(71/64)*64+64 = 64+64=128? Wait floor(71/64)=1, *64+64 = 128. Formula2: floor(135/64)*64 = 2*64=128. Hmm n=63: 63 bytes + 0x80 = 64 exactly, pad to 56 mod 64: 64 mod 64 = 0, need 56, add 56 zeros → 120, +8 = 128. ✓. Both agree.

Wait formula1 for n=63: n+8=71, 71/64=1 (integer), +1=2, *64=128. but is 128 correct? yes. Hmm but n+8=71 → this suggests... let me just use formula: total = ((n + 9 + 63) / 64) * 64. n=63: (72+63)/64 = 135/64=2 → 128. ✓.

Hmm, actually wait: I realize formula1: ((n+8)/64 + 1)*64 for n=8: (16/64=0, +1)*64 = 64. But n=8: 8+1=9 bytes, pad to 56 mod 64: 56, total 56... wait 9 to 56 = 47 zeros, then +8 = 64. ✓.

OK both are fine. I'll use total = ((n + 9 + 63) / 64) * 64.

Now let me construct padded byte array. We have Dizi<sabitsüre<dtam8>> — but we built `veri` as input. We need to create a padded buffer. Since the return is Dizi<sabitsüre<dtam32>>, we process blocks and produce state.

Approach:
```
işlev sha256_bayt(veri: Dizi<sabitsüre<dtam8>>, n: tam32) -> Dizi<sabitsüre<dtam32>> {
    // total padded uzunluk
    değişken toplam: tam32 = ((n + 9 + 63) / 64) * 64;
    değişken tampon: Dizi<sabitsüre<dtam8>> = [];
    değişken i: tam32 = 0;
    iken i < n {
        dizi_ekle(tampon, veri[i]);
        i = i + 1;
    }
    dizi_ekle(tampon, sabitsüre_olustur(0x80 olarak dtam8));
    iken dizi_boyut(tampon) < toplam - 8 {
        dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    }
    // 8 bayt büyük-uç bit uzunluğu, n*8
    değişken bit_uz: dtam32 = (n olarak dtam32) * 8;
    // üst 32 bit = 0
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur((bit_uz >> 24) & 255 olarak dtam8));
    ...
```

Hmm wait bit_uz >> 24 — bit_uz is dtam32. Shifts on dtam32? The task says shifts on sabitsüre<dtam32>. bit_uz is plain dtam32 (public). Then `& 255 olarak dtam8` conversion.

Actually careful with the conversion syntax. The task says: each `& 255` and `olarak sabitsüre<dtam8>`. So like `sabitsüre_olustur(((bit_uz >> 24) & 255) olarak dtam8)`.

Hmm but need parentheses. Let me write `sabitsüre_olustur(((bit_uz >> 24) & 255) olarak dtam8)`.

Then process blocks:
```
    değişken H: Dizi<dtam32> = sha256_H0();
    değişken state: Dizi<sabitsüre<dtam32>> = [
        sabitsüre_olustur(H[0]), ... 8
    ];
    değişken blok_sayisi: tam32 = toplam / 64;
    değişken b: tam32 = 0;
    iken b < blok_sayisi {
        değişken blok: Dizi<sabitsüre<dtam32>> = [];
        değişken j: tam32 = 0;
        iken j < 16 {
            değişken off: tam32 = b * 64 + j * 4;
            değişken b0: dtam32 = ifşa(tampon[off]) olarak dtam32;  // hmm
            ...
```

Wait, tampon elements are sabitsüre<dtam8>. To build word we need their values. `ifşa(tampon[off])` gives dtam8. Then convert `olarak dtam32`. Then shift.

w = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3.

Then `dizi_ekle(blok, sabitsüre_olustur(w))`.

Then sha256_blok_sikistir(state, blok).

Return state.

Hmm but the spec says return 8 kelimelik durumu. state is Dizi<sabitsüre<dtam32>>. Good.

Wait but state was built from H which is Dizi<dtam32>, and sha256_blok_sikistir takes Dizi<sabitsüre<dtam32>>. Good.

hmac_sha256:
```
işlev hmac_sha256(anahtar: Dizi<sabitsüre<dtam8>>, an: tam32, mesaj: Dizi<sabitsüre<dtam8>>, mn: tam32) -> Dizi<sabitsüre<dtam32>> {
    // K 64 bayta normalize
    değişken K: Dizi<sabitsüre<dtam8>> = [];
    eğer an > 64 {
        değişken ozet: Dizi<sabitsüre<dtam32>> = sha256_bayt(anahtar, an);
        // 32 bayt büyük-uç
        değişken q: tam32 = 0;
        iken q < 8 {
            değişken w: dtam32 = ifşa(ozet[q]);
            dizi_ekle(K, sabitsüre_olustur(((w >> 24) & 255) olarak dtam8));
            dizi_ekle(K, sabitsüre_olustur(((w >> 16) & 255) olarak dtam8));
            dizi_ekle(K, sabitsüre_olustur(((w >> 8) & 255) olarak dtam8));
            dizi_ekle(K, sabitsüre_olustur((w & 255) olarak dtam8));
            q = q + 1;
        }
    } değilse {
        değişken q: tam32 = 0;
        iken q < an {
            dizi_ekle(K, anahtar[q]);
            q = q + 1;
        }
    }
    // 64'e sıfırla
    iken dizi_boyut(K) < 64 {
        dizi_ekle(K, sabitsüre_olustur(0 olarak dtam8));
    }
    // iç = (K ^ ipad) || mesaj
    değişken ic: Dizi<sabitsüre<dtam8>> = [];
    değişken ip: dtam8 = 0x36;
    değişken i: tam32 = 0;
    iken i < 64 {
        dizi_ekle(ic, K[i] ^ sabitsüre_olustur(ip));
        i = i + 1;
    }
    i = 0;
    iken i < mn {
        dizi_ekle(ic, mesaj[i]);
        i = i + 1;
    }
    değişken ic_ozet: Dizi<sabitsüre<dtam32>> = sha256_bayt(ic, 64 + mn);
    // dış = (K ^ opad) || ic_ozet(32 bayt büyük-uç)
    değişken dis: Dizi<sabitsüre<dtam8>> = [];
    değişken op: dtam8 = 0x5C;
    i = 0;
    iken i < 64 {
        dizi_ekle(dis, K[i] ^ sabitsüre_olustur(op));
        i = i + 1;
    }
    i = 0;
    iken i < 8 {
        değişken w: dtam32 = ifşa(ic_ozet[i]);
        dizi_ekle(dis, sabitsüre_olustur(((w >> 24) & 255) olarak dtam8));
        dizi_ekle(dis, sabitsüre_olustur(((w >> 16) & 255) olarak dtam8));
        dizi_ekle(dis, sabitsüre_olustur(((w >> 8) & 255) olarak dtam8));
        dizi_ekle(dis, sabitsüre_olustur((w & 255) olarak dtam8));
        i = i + 1;
    }
    ver sha256_bayt(dis, 96);
}
```

Wait: the outer sha256_bayt(dis, 96) — dis is 64 + 32 = 96 bytes. Good.

Now for ic: 64 + mn bytes. sha256_bayt(ic, 64 + mn). Good.

Note about variable scoping — declaring same name in if/else branches. KEMGU may allow or not. The example in hmac_pad_uret declares değişken inside. To be safe, declare q before branches.

Actually in the `eğer an > 64 { değişken q... } değilse { değişken q... }` — duplicate declaration in separate blocks. Might conflict. Better declare once outside.

Let me restructure with variables declared once.

Also reusing `i` — declared once, reassigned. Fine.

Let me also be careful: `ifşa` — used in test. Good.

Now, the concern about declaration ordering: KEMGU may require declaration before use. Let me write carefully.

Also `dizi_ekle` — the task says "Dizi büyütme: dizi_ekle(d, x)". Good.

Now about integer division `(n + 9 + 63) / 64` — should be fine, tam32 arithmetic.

Let me also double-check `dizi_boyut` returns int.

Now about shifts on dtam32. bit_uz is dtam32; `bit_uz >> 24`. Should work.

Actually wait — for the 8-byte length, I wrote 4 zero bytes then the high word. But bit_uz >> 24 assumes bit_uz fits in 32 bits (n*8 where n is tam32). Since upper 32 bits are 0, the big-endian representation is: 00 00 00 00 then (bit_uz>>24)&255, (bit_uz>>16)&255, (bit_uz>>8)&255, bit_uz&255. That's 8 bytes. Good.

Now, careful about the type: `bit_uz & 255` — bit_uz: dtam32, 255 is int literal. Probably fine.

Now the ESKI text. The last lines of the file should be hmac_pad_uret's closing. But I don't know exactly what comes after. The context cut shows hmac_pad_uret ending with `}` and that's the cut. I'll assume the file ends with that. Let me use:

```
    ver 0;
}
```
as ESKI? That may not be unique. Better to include more context. Use the last several lines:

```
    iken i < anahtar_uzunluk {
        inner[i] = anahtar[i] ^ sabitsüre_olustur(ipad);
        outer[i] = anahtar[i] ^ sabitsüre_olustur(opad);
        i = i + 1;
    }
    ver 0;
}
```

That's likely unique.

Now, one concern: the task says "Boş dizi: `değişken d: Dizi<sabitsüre<dtam8>> = [];`". Good.

Another concern: `dizi_ekle` on `Dizi<sabitsüre<dtam32>>` — should work generically? The example uses it broadly. Assume yes.

Let me also think: is there a concern about `eğer ... { } değilse { }`? Context uses `eğer ... { } değilse { }` in heap example. Good.

Now writing out. Let me carefully handle the word assembly in sha256_bayt.

For each block j from 0..15:
off = b*64 + j*4
b0 = ifşa(tampon[off]) olarak dtam32
b1 = ifşa(tampon[off+1]) olarak dtam32
b2 = ifşa(tampon[off+2]) olarak dtam32
b3 = ifşa(tampon[off+3]) olarak dtam32
w = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3

Then `dizi_ekle(blok, sabitsüre_olustur(w))`.

Hmm — I need to make sure `ifşa(tampon[off])` returns dtam8 and converting with `olarak dtam32` works (zero-extends, since tam8 is unsigned byte). Good.

Now let me also reconsider the formula and the padding loop condition. After appending all n bytes and the 0x80, the buffer size is n+1. We pad with zeros while size < toplam - 8. Good, then append 8 length bytes → size = toplam.

Let me double-check when n=0: n+1=1 byte (0x80), toplam = ((0+9+63)/64)*64 = (72/64)*64 = 64. toplam-8 = 56. Pad from 1 to 56, then 8 → 64. ✓.

Alright.

Also declare all variables. Let me write final code.

```kemgu

// =============================================================================
// D-XXX (Tur 23a) — Çok-bloklu SHA-256 + HMAC-SHA256 (RFC 2104)
// =============================================================================
//
// sha256_bayt: FIPS 180-4 §5.1 dolgusu (n bayt + 0x80 + sıfır + 8-baytlık
// BÜYÜK-UÇ bit uzunluğu; üst 32 bit 0), her 64-baytlık blok için 16 kelime
// BÜYÜK-UÇ kurup sha256_blok_sikistir'i çağırır. Durum sha256_H0()'dan başlar.
//
// hmac_sha256: RFC 2104, blok 64 bayt. anahtar > 64 ise evvela SHA-256'lanır
// (özetin 32 baytı büyük-uç), sonra 64'e sıfırla doldurulur. İç/dış pedler
// K ⊕ ipad / K ⊕ opad. Tüm aritmetik dtam32 üzerinde; gizli ifşa yalnız
// kelime↔bayt dönüşümünde (protokol gereği dış dünyaya çıkacak).

işlev sha256_bayt(veri: Dizi<sabitsüre<dtam8>>, n: tam32) -> Dizi<sabitsüre<dtam32>> {
    // Toplam dolgulu uzunluk (64'ün katı): n + 1 (0x80) + sıfırlar + 8 (uzunluk)
    değişken toplam: tam32 = ((n + 9 + 63) / 64) * 64;
    değişken tampon: Dizi<sabitsüre<dtam8>> = [];
    değişken i: tam32 = 0;
    iken i < n {
        dizi_ekle(tampon, veri[i]);
        i = i + 1;
    }
    dizi_ekle(tampon, sabitsüre_olustur(0x80 olarak dtam8));
    iken dizi_boyut(tampon) < toplam - 8 {
        dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    }
    // 8 bayt BÜYÜK-UÇ bit uzunluğu (n*8; üst 32 bit 0 yeter)
    değişken bit_uz: dtam32 = (n olarak dtam32) * 8;
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bit_uz >> 24) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bit_uz >> 16) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bit_uz >> 8) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur((bit_uz & 255) olarak dtam8));

    // Başlangıç durumu
    değişken H: Dizi<dtam32> = sha256_H0();
    değişken state: Dizi<sabitsüre<dtam32>> = [
        sabitsüre_olustur(H[0]), sabitsüre_olustur(H[1]),
        sabitsüre_olustur(H[2]), sabitsüre_olustur(H[3]),
        sabitsüre_olustur(H[4]), sabitsüre_olustur(H[5]),
        sabitsüre_olustur(H[6]), sabitsüre_olustur(H[7])
    ];

    // Her 64-baytlık bloğu işle
    değişken blok_sayisi: tam32 = toplam / 64;
    değişken b: tam32 = 0;
    iken b < blok_sayisi {
        değişken blok: Dizi<sabitsüre<dtam32>> = [];
        değişken j: tam32 = 0;
        iken j < 16 {
            değişken off: tam32 = b * 64 + j * 4;
            değişken b0: dtam32 = ifşa(tampon[off]) olarak dtam32;
            değişken b1: dtam32 = ifşa(tampon[off + 1]) olarak dtam32;
            değişken b2: dtam32 = ifşa(tampon[off + 2]) olarak dtam32;
            değişken b3: dtam32 = ifşa(tampon[off + 3]) olarak dtam32;
            değişken w: dtam32 = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
            dizi_ekle(blok, sabitsüre_olustur(w));
            j = j + 1;
        }
        sha256_blok_sikistir(state, blok);
        b = b + 1;
    }
    ver state;
}

işlev hmac_sha256(
    anahtar: Dizi<sabitsüre<dtam8>>, an: tam32,
    mesaj: Dizi<sabitsüre<dtam8>>, mn: tam32
) -> Dizi<sabitsüre<dtam32>> {
    // K'yı 64 bayta normalize et (uzunsa önce hash'le)
    değişken K: Dizi<sabitsüre<dtam8>> = [];
    değişken i: tam32 = 0;
    eğer an > 64 {
        değişken ozet: Dizi<sabitsüre<dtam32>> = sha256_bayt(anahtar, an);
        iken i < 8 {
            değişken w: dtam32 = ifşa(ozet[i]);
            dizi_ekle(K, sabitsüre_olustur(((w >> 24) & 255) olarak dtam8));
            dizi_ekle(K, sabitsüre_olustur(((w >> 16) & 255) olarak dtam8));
            dizi_ekle(K, sabitsüre_olustur(((w >> 8) & 255) olarak dtam8));
            dizi_ekle(K, sabitsüre_olustur((w & 255) olarak dtam8));
            i = i + 1;
        }
    } değilse {
        iken i < an {
            dizi_ekle(K, anahtar[i]);
            i = i + 1;
        }
    }
    iken dizi_boyut(K) < 64 {
        dizi_ekle(K, sabitsüre_olustur(0 olarak dtam8));
    }

    // İç: (K ⊕ ipad) || mesaj
    değişken ip: dtam8 = 0x36;
    değişken ic: Dizi<sabitsüre<dtam8>> = [];
    i = 0;
    iken i < 64 {
        dizi_ekle(ic, K[i] ^ sabitsüre_olustur(ip));
        i = i + 1;
    }
    i = 0;
    iken i < mn {
        dizi_ekle(ic, mesaj[i]);
        i = i + 1;
    }
    değişken ic_ozet: Dizi<sabitsüre<dtam32>> = sha256_bayt(ic, 64 + mn);

    // Dış: (K ⊕ opad) || iç özet (32 bayt büyük-uç)
    değişken op: dtam8 = 0x5C;
    değişken dis: Dizi<sabitsüre<dtam8>> = [];
    i = 0;
    iken i < 64 {
        dizi_ekle(dis, K[i] ^ sabitsüre_olustur(op));
        i = i + 1;
    }
    i = 0;
    iken i < 8 {
        değişken w2: dtam32 = ifşa(ic_ozet[i]);
        dizi_ekle(dis, sabitsüre_olustur(((w2 >> 24) & 255) olarak dtam8));
        dizi_ekle(dis, sabitsüre_olustur(((w2 >> 16) & 255) olarak dtam8));
        dizi_ekle(dis, sabitsüre_olustur(((w2 >> 8) & 255) olarak dtam8));
        dizi_ekle(dis, sabitsüre_olustur((w2 & 255) olarak dtam8));
        i = i + 1;
    }
    ver sha256_bayt(dis, 96);
}
```

Wait: inside the `eğer an > 64` branch I declare `değişken ozet` and use `i` (outer). That's fine.

But there's a potential issue: in the `ikeni < 8` inside if branch, `i` is reassigned. After if/else, i is not needed until reset to 0. Good.

Inside `eğer` branch I declare `değişken w`. Outside, in the second loop (dis), I declare `w2` to avoid shadowing conflicts. Good since both in different scopes but KEMGU may be stricter. Actually different scopes anyway since the first `w` is inside `eğer` block. But `w2` in the dis loop is at function scope... actually dis loop body declares `değişken w2` inside `iken i < 8 { }`. The other w in eğer-branch's `iken i < 8`. Different scopes. Should be OK to reuse w but let's use w2 to be safe.

Hmm, wait: in eğer branch: `değişken w: dtam32`. Also earlier in sha256_bayt there's `değişken w: dtam32` but different function. Fine.

Now let me consider: `dizi_ekle` on Dizi<sabitsüre<dtam32>> — is it generic? The task explicitly says "dizi_ekle(d, x)". Assume yes.

Also concern: the task says test file `test/stdlib/test_kripto_kosum.kem` exists but I should NOT touch it. Only stdlib/kripto/karma.kem. OK.

One more concern: does `dizi_boyut` on `Dizi<sabitsüre<dtam8>>` work? Assume generic.

Let me also double check the `olarak` conversion from dtam32 expression to dtam8. `((bit_uz >> 24) & 255) olarak dtam8` — precedence of `olarak`? It's described as "tip dönüşümü". Typically `expr olarak tip`. With parens around expr it's clear.

Now, another thing: `(n olarak dtam32) * 8` — n is tam32, converting to dtam32. Fine.

`toplam` computed as ((n + 9 + 63) / 64) * 64. n is tam32. Good.

Now the ESKI block. Let me use:

```
    iken i < anahtar_uzunluk {
        inner[i] = anahtar[i] ^ sabitsüre_olustur(ipad);
        outer[i] = anahtar[i] ^ sabitsüre_olustur(opad);
        i = i + 1;
    }
    ver 0;
}
```

That should be unique enough.

Alright, output.

VARSAYIMLAR:
- Karma.kem dosyası hmac_pad_uret fonksiyonuyla bitiyor.
- `dizi_ekle`/`dizi_boyut`/`dizi[i]`/`dizi[i] = x` tüm sabitsüre<dtam8/32> tiplerinde çalışıyor.
- İfade tipleri ve `olarak` precedence'ı parantezlerle netleştirildi.
- `atum32 & 255` int literal ile uyumlu; dtam8'e `olarak` dönüşümü kısaltıyor.

Let me finalize.
