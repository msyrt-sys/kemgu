# Tur 23 — hmac — `deepseek-v4.1-flash`

- Tarih: 2026-10-17 18:30:55 · süre 269.3 sn · finish `length` · reasoning_effort `low`
- usage: `{"prompt_tokens": 14794, "total_tokens": 31178, "completion_tokens": 16384, "prompt_tokens_details": {"cached_tokens": 13120, "created_cache_tokens": 1664}, "completion_tokens_details": {"reasoning_tokens": 16361}}`
- Değişiklik blokları: 0 — 
- İstem: [tur_23_hmac_istem.md](tur_23_hmac_istem.md)

## Yanıt

```duzenle:stdlib/kripto/karma.kem
<<<<<<< ESKI


## Düşünme (reasoning)

Let me carefully build the edits needed.

We need three files edited. Let me look at the task carefully.

### File 1: stdlib/kripto/karma.kem

We need to append after the last lines (the `hmac_pad_uret` function shown). The context says the ESKİ should be the last lines shown. The `hmac_pad_uret` function ends with:

```
    ver 0;
}
```

But that might not be unique. Hmm. We need to match carefully. The task says: "ESKİ = son satırlar, kesitte `hmac_pad_uret`'in sonu".

Let me use the whole function as the ESKİ anchor, since it's unique. Actually we can use the closing brace with surrounding lines. Let me use:

```
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

That's the full function; the ESKİ includes it and the YENİ is the same + new functions appended.

Wait, but we don't know what comes after `hmac_pad_uret` in the file. Actually the task says this is at the end of the file (EREIGN...) Actually it says "dosyanın SONUNA (ESKİ = son satırlar, kesitte `hmac_pad_uret`'in sonu)". Hmm, "sonuna" means to the end of the file. But wait, there might be functions after `hmac_pad_uret` like hmac_sha256 already? Actually the task says we're adding new functions. But the kesit shows `hmac_pad_uret` at lines 259-275. So it may not be the end of the file. Hmm.

Actually re-reading: "Mevcut `sha256_blok_sikistir(state, blok_w)`... kullanılacak; yeniden tanımlama YOK." So we add new functions. The task says ESKİ = son satırlar. Given the context, `hmac_pad_uret` is the last shown, but there might be more after. Hmm. But the instruction says "dosyanın SONUNA" and "ESKİ = son satırlar, kesitte `hmac_pad_uret`'in sonu". This suggests `hmac_pad_uret` is at the end. But it also says the file may continue...

Actually, I'll trust: the ESKİ anchor is the `hmac_pad_uret` function block, and we append after it. If there's stuff after, the edit will insert in the middle, which might be wrong. But given the task description, I'll go with it.

Hmm, but wait — there might also be an existing `hmac_sha256` in the file that the task says "yeniden tanımlama YOK". Let me re-read: "Mevcut `sha256_blok_sikistir(state, blok_w)` (16 kelimelik blok, state'i yerinde günceller) ve `sha256_H0()` kullanılacak; yeniden tanımlama YOK." This is about not redefining sha256_blok_sikistir and sha256_H0, not about hmac.

OK let me write the new functions.

#### sha256_bayt

```kemgu
işlev sha256_bayt(veri: Dizi<sabitsüre<dtam8>>, n: tam32) -> Dizi<sabitsüre<dtam32>> {
    // Dolgu: n bayt + 0x80 + sıfırlar + 8 baytlık büyük-uç bit uzunluğu, 64'ün katı
    değişken tampon: Dizi<sabitsüre<dtam8>> = [];
    değişken i: tam32 = 0;
    iken i < n {
        dizi_ekle(tampon, veri[i]);
        i = i + 1;
    }
    dizi_ekle(tampon, sabitsüre_olustur(0x80 olarak dtam8));
    // toplam_uzunluk = n + 1 + 8 + pad → 64'ün katı
    değişken toplam: tam32 = n + 9;
    iken toplam % 64 != 0 {
        dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
        toplam = toplam + 1;
    }
    // bit uzunluğu (büyük-uç, 64 bit; üst 32 bit 0)
    değişken bitlen: tam64 = (n olarak tam64) * 8;
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 56) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 48) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 40) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 32) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 24) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 16) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 8) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur((bitlen & 255) olarak dtam8));
    ...
```

Hmm wait — `tampon` is `Dizi<sabitsüre<dtam8>>`, and shifts on `sabitsüre<dtam8>` might not be supported for tam8? The context says `sabitsüre<dtam32>` supports `^ | & >> << +`. For tam8 it's less clear. But I'm computing bitlen as tam64 and shifting, then casting to dtam8. That should be fine.

Actually wait — is `bitlen` a tam64? Shifts on regular tam64 should be fine.

But wait, does the `dizi_ekle` function accept `sabitsüre<dtam8>`? The context says "Dizi büyütme: `dizi_ekle(d, x)`". For `Dizi<sabitsüre<dtam8>>`, x should be `sabitsüre<dtam8>`. OK.

Then process blocks:

```kemgu
    // Her 64 baytlık blok: 16 kelime büyük-uç
    değişken durum: Dizi<dtam32> = sha256_H0();
    değişken state: Dizi<sabitsüre<dtam32>> = [
        sabitsüre_olustur(durum[0]), sabitsüre_olustur(durum[1]),
        sabitsüre_olustur(durum[2]), sabitsüre_olustur(durum[3]),
        sabitsüre_olustur(durum[4]), sabitsüre_olustur(durum[5]),
        sabitsüre_olustur(durum[6]), sabitsüre_olustur(durum[7])
    ];
    değişken blok_sayisi: tam32 = dizi_boyut(tampon) / 64;
    değişken b: tam32 = 0;
    iken b < blok_sayisi {
        değişken blok: Dizi<sabitsüre<dtam32>> = [];
        değişken j: tam32 = 0;
        iken j < 16 {
            değişken taban: tam32 = b * 64 + j * 4;
            değişken b0: tam32 = ifşa(tampon[taban]) olarak tam32;
            ...
```

Hmm, wait — `tampon[taban]` returns `sabitsüre<dtam8>`. We need to convert to tam32 to shift. We can't `ifşa` to get dtam8? Let's see. `ifşa(s)` açığa çıkarır. So `ifşa(tampon[taban])` gives dtam8. Then `olarak tam32`? Hmm.

Actually let me think. We need to build word:
w = b0<<24 | b1<<16 | b2<<8 | b3

Each byte b0 is a dtam8 value 0..255. Convert to dtam32 then shift. Then OR is on sabitsüre<dtam32>.

So:
```
değişken b0d: tam32 = ifşa(tampon[taban]) olarak tam32;
```

Hmm, can we `olarak` from dtam8 to tam32? The `olarak` is a type cast. Let me just use `ifşa` which extracts the underlying value. Actually, `ifşa` on `sabitsüre<dtam8>` returns `dtam8`. Then cast to tam32.

Actually the issue is whether `olarak tam32` on a dtam8 works. dtam8 presumably is a distinct type... hmm. Actually `dtam8`, `dtam16`, `dtam32`, `dtam64` — "dtam" might mean "değişken tam" (dynamic tam). These are runtime integer types of various widths.

Let me look at the byte-word assembly more carefully.

Actually, simpler: build the word in a `sabitsüre<dtam32>` by combining.

```
değişken kelime: sabitsüre<dtam32> = sabitsüre_olustur(0 olarak dtam32);
değişken k: tam32 = 0;
iken k < 4 {
    değişken bayt: sabitsüre<dtam32> = tampon[taban + k] olarak sabitsüre<dtam32>;
    ...
}
```

Hmm, casting from sabitsüre<dtam8> to sabitsüre<dtam32>? Not sure that works. Let me do:

```
değişken bayt_deger: dtam32 = ifşa(tampon[taban + k]) olarak dtam32;
```

Hmm, `ifşa` on a dtam8 sabitsüre values returning a dtam8. Then `olarak dtam32` casts. I hope widths up-cast.

Let me be simpler: just extract tam32 for each and combine.

Actually the task description explicitly says: "(w = b0<<24 | b1<<16 | b2<<8 | b3, baytlar `olarak sabitsüre<dtam32>` ile genişletilir)".

So the casts are `olarak sabitsüre<dtam32>`. So there's a cast from `sabitsüre<dtam8>` to `sabitsüre<dtam32>`. So:

```
değişken b0: sabitsüre<dtam32> = tampon[taban] olarak sabitsüre<dtam32>;
değişken b1: sabitsüre<dtam32> = tampon[taban + 1] olarak sabitsüre<dtam32>;
değişken b2: sabitsüre<dtam32> = tampon[taban + 2] olarak sabitsüre<dtam32>;
değişken b3: sabitsüre<dtam32> = tampon[taban + 3] olarak sabitsüre<dtam32>;
değişken kelime: sabitsüre<dtam32> = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
dizi_ekle(blok, kelime);
```

OK.

Then `sha256_blok_sikistir(state, blok)`.

Then after the loop, return state.

Wait, is `blok` refreshed as a new variable each iteration? We do `değişken blok: ... = [];` inside the loop. Hmm, declaring inside loop — probably OK since it would shadow. Actually it's a fresh declaration each iteration. Should be fine.

Actually there's a concern: the sha256_blok_sikistir expects `Dizi<sabitsüre<dtam32>>` for `blok_w`. Yes. And state is `Dizi<sabitsüre<dtam32>>`. Good.

Return type: `Dizi<sabitsüre<dtam32>>`. Return state.

Now for HMAC.

#### hmac_sha256

```kemgu
işlev hmac_sha256(anahtar: Dizi<sabitsüre<dtam8>>, an: tam32, mesaj: Dizi<sabitsüre<dtam8>>, mn: tam32) -> Dizi<sabitsüre<dtam32>> {
    // K'yı 64 bayta getir
    değişken k64: Dizi<sabitsüre<dtam8>> = [];
    eğer an > 64 {
        // anahtar = hash'in 32 baytı
        değişken ozet: Dizi<sabitsüre<dtam32>> = sha256_bayt(anahtar, an);
        değişken i: tam32 = 0;
        iken i < 8 {
            değişken w: sabitsüre<dtam32> = ozet[i];
            dizi_ekle(k64, (w >> 24) olarak ... );
            ...
        }
    } değilse {
        değişken i: tam32 = 0;
        iken i < an { dizi_ekle(k64, anahtar[i]); i = i + 1; }
    }
    // 64'e sıfırla doldur
    iken dizi_boyut(k64) < 64 {
        dizi_ekle(k64, sabitsüre_olustur(0 olarak dtam8));
    }
    ...
}
```

For the key-to-bytes extraction from word: "Kelime→bayt: (w >> 24), (w >> 16), (w >> 8), w; her biri `& 255` ve `olarak sabitsüre<dtam8>`."

So `(w >> 24) & 255` then cast to `sabitsüre<dtam8>`? Wait — but `w` is `sabitsüre<dtam32>`. `(w >> 24)` is `sabitsüre<dtam32>`. `& 255` is still `sabitsüre<dtam32>`. Then `olarak sabitsüre<dtam8>`.

Hmm, but 255 as literal in the sabitsüre<dtam32> context — probably auto-widened.

OK so:

```
dizi_ekle(k64, ((w >> 24) & 255) olarak sabitsüre<dtam8>);
dizi_ekle(k64, ((w >> 16) & 255) olarak sabitsüre<dtam8>);
dizi_ekle(k64, ((w >> 8) & 255) olarak sabitsüre<dtam8>);
dizi_ekle(k64, (w & 255) olarak sabitsüre<dtam8>);
```

Now the ic and dis.

```
    değişken ic: Dizi<sabitsüre<dtam8>> = [];
    değişken i: tam32 = 0;
    iken i < 64 {
        dizi_ekle(ic, k64[i] ^ sabitsüre_olustur(0x36 olarak dtam8));
        i = i + 1;
    }
    i = 0;
    iken i < mn {
        dizi_ekle(ic, mesaj[i]);
        i = i + 1;
    }
    değişken ic_ozet: Dizi<sabitsüre<dtam32>> = sha256_bayt(ic, 64 + mn);
```

Wait — `k64[i] ^ sabitsüre_olustur(...)` — both are `sabitsüre<dtam8>`. XOR result is sabitsüre<dtam8>. Good.

Then dis:
```
    değişken dis: Dizi<sabitsüre<dtam8>> = [];
    i = 0;
    iken i < 64 {
        dizi_ekle(dis, k64[i] ^ sabitsüre_olustur(0x5C olarak dtam8));
        i = i + 1;
    }
    i = 0;
    iken i < 8 {
        değişken w: sabitsüre<dtam32> = ic_ozet[i];
        dizi_ekle(dis, ((w >> 24) & 255) olarak sabitsüre<dtam8>);
        dizi_ekle(dis, ((w >> 16) & 255) olarak sabitsüre<dtam8>);
        dizi_ekle(dis, ((w >> 8) & 255) olarak sabitsüre<dtam8>);
        dizi_ekle(dis, (w & 255) olarak sabitsüre<dtam8>);
        i = i + 1;
    }
    ver sha256_bayt(dis, 96);
}
```

Wait — there's a name collision. `w` might be used elsewhere. Let me use `iw` to be safe. Actually `w` is a local inside loop so should be fine. But wait, later I have another `w` in the k64 building. Since they're in separate blocks, fine.

Hmm, actually reusing the variable name `i` after having declared it in an if/else scope... Let me be careful. In KEMGU, is `değişken` block-scoped? Let me avoid redeclaring `i`. I'll declare once at top of function.

Let me restructure:

```
işlev hmac_sha256(...) -> Dizi<sabitsüre<dtam32>> {
    değişken i: tam32 = 0;
    değişken k64: Dizi<sabitsüre<dtam8>> = [];
    eğer an > 64 {
        değişken ozet: Dizi<sabitsüre<dtam32>> = sha256_bayt(anahtar, an);
        i = 0;
        iken i < 8 {
            değişken w: sabitsüre<dtam32> = ozet[i];
            dizi_ekle(k64, ((w >> 24) & 255) olarak sabitsüre<dtam8>);
            dizi_ekle(k64, ((w >> 16) & 255) olarak sabitsüre<dtam8>);
            dizi_ekle(k64, ((w >> 8) & 255) olarak sabitsüre<dtam8>);
            dizi_ekle(k64, (w & 255) olarak sabitsüre<dtam8>);
            i = i + 1;
        }
    } değilse {
        i = 0;
        iken i < an {
            dizi_ekle(k64, anahtar[i]);
            i = i + 1;
        }
    }
    iken dizi_boyut(k64) < 64 {
        dizi_ekle(k64, sabitsüre_olustur(0 olarak dtam8));
    }
    ...
}
```

Hmm, "eğer ... { } değilse { }" — I've seen `değilse` used in the heap code. Good.

Now `sha256_bayt(anahtar, an)` returns `Dizi<sabitsüre<dtam32>>`. Good.

Now, careful with the `w` variable inside the while — declaring inside the loop body might be OK, similar to how `değişken hsize_p` is declared inside `while`. Yes, that's fine.

OK. Now moving on.

### File 2: test/stdlib/test_kripto_kosum.kem

Three edits.

Edit (a): The line `    eğer kt_sec_u64_ust_bit() == 1 { skor = skor + 8; }   // D-629` gets the two new lines added below.

Edit (b): `    ver skor;   // 15 = tam geçiş` → `    ver skor;   // 63 = tam geçiş`

Edit (c): The line `işlev main() -> tam32 {` → new functions + that line.

Wait — edit (a) and (c) both involve `main`... actually (a) is inside main. But (c) inserts before main. These are separate locations. Fine.

But careful: the ESKİ for (c) is `işlev main() -> tam32 {` — this must be unique in the file. Yes probably.

But wait, there's a concern: (a)'s ESKİ `    eğer kt_sec_u64_ust_bit() == 1 { skor = skor + 8; }   // D-629` — should be unique.

Now write the new functions for (c):

```kemgu
işlev kt_metin_bayt(s: metin) -> Dizi<sabitsüre<dtam8>> {
    değişken d: Dizi<sabitsüre<dtam8>> = [];
    değişken i: tam32 = 0;
    iken i < metin_uzunluk(s) {
        dizi_ekle(d, sabitsüre_olustur((metin_bayt(s, i) olarak dtam8)));
        i = i + 1;
    }
    ver d;
}

işlev kt_tekrar(b: dtam8, n: tam32) -> Dizi<sabitsüre<dtam8>> {
    değişken d: Dizi<sabitsüre<dtam8>> = [];
    değişken i: tam32 = 0;
    iken i < n {
        dizi_ekle(d, sabitsüre_olustur(b));
        i = i + 1;
    }
    ver d;
}

işlev kt_ozet_kontrol(d: Dizi<sabitsüre<dtam32>>, w0: tam64, w1: tam64, w7: tam64) -> tam32 {
    değişken gecti: tam32 = 1;
    eğer (ifşa(d[0]) olarak tam64) != w0 { gecti = 0; }
    eğer (ifşa(d[1]) olarak tam64) != w1 { gecti = 0; }
    eğer (ifşa(d[7]) olarak tam64) != w7 { gecti = 0; }
    ver gecti;
}

işlev kt_sha256_cok_blok() -> tam32 {
    değişken s: metin = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    değişken veri: Dizi<sabitsüre<dtam8>> = kt_metin_bayt(s);
    değişken ozet: Dizi<sabitsüre<dtam32>> = sha256_bayt(veri, metin_uzunluk(s));
    ver kt_ozet_kontrol(ozet, 613247585, 3523623096, 433784513);
}

işlev kt_hmac_rfc4231() -> tam32 {
    değişken k1: Dizi<sabitsüre<dtam8>> = kt_tekrar(11 olarak dtam8, 20);
    değişken m1: Dizi<sabitsüre<dtam8>> = kt_metin_bayt("Hi There");
    değişken r1: Dizi<sabitsüre<dtam32>> = hmac_sha256(k1, 20, m1, 8);
    eğer kt_ozet_kontrol(r1, 2956217441, 3638245459, 775081975) != 1 { ver 0; }

    değişken k2: Dizi<sabitsüre<dtam8>> = kt_metin_bayt("Jefe");
    değişken m2: Dizi<sabitsüre<dtam8>> = kt_metin_bayt("what do ya want for nothing?");
    değişken r2: Dizi<sabitsüre<dtam32>> = hmac_sha256(k2, 4, m2, 28);
    eğer kt_ozet_kontrol(r2, 1541194054, 3210769742, 1693202499) != 1 { ver 0; }

    değişken k3: Dizi<sabitsüre<dtam8>> = kt_tekrar(170 olarak dtam8, 131);
    değişken m3: Dizi<sabitsüre<dtam8>> = kt_metin_bayt("Test Using Larger Than Block-Size Key - Hash Key First");
    değişken r3: Dizi<sabitsüre<dtam32>> = hmac_sha256(k3, 131, m3, 54);
    eğer kt_ozet_kontrol(r3, 1625567577, 518043263, 249790292) != 1 { ver 0; }

    ver 1;
}

işlev main() -> tam32 {
```

Hmm wait — the message lengths. Let me count strings.

"Hi There" = 8 characters. Good.
"what do ya want for nothing?" = let me count: "what do ya want for nothing?" — w-h-a-t(4) space(5) d-o(7) space(8) y-a(10) space(11) w-a-n-t(15) space(16) f-o-r(19) space(20) n-o-t-h-i-n-g-?(28). Yes 28.

"Test Using Larger Than Block-Size Key - Hash Key First" — let me count later. Actually the task says mesaj "Test Using Larger Than Block-Size Key - Hash Key First". Let me count: 
T-e-s-t(4) space(5) U-s-i-n-g(10) space(11) L-a-r-g-e-r(17) space(18) T-h-a-n(22) space(23) B-l-o-c-k-S-i-z-e(32) space(33) K-e-y(36) space(37) -(38) space(39) H-a-s-h(43) space(44) K-e-y(47) space(48) F-i-r-s-t(53). So 53? Hmm let me recount.

"Test Using Larger Than Block-Size Key - Hash Key First"

T(1)e(2)s(3)t(4) (5)U(6)s(7)i(8)n(9)g(10) (11)L(12)a(13)r(14)g(15)e(16)r(17) (18)T(19)h(20)a(21)n(22) (23)B(24)l(25)o(26)c(27)k(28)-(29)S(30)i(31)z(32)e(33) (34)K(35)e(36)y(37) (38)-(39) (40)H(41)a(42)s(43)h(44) (45)K(46)e(47)y(48) (49)F(50)i(51)r(52)s(53)t(54)

54 characters. Good, matches my guess 54.

Let me double-check "Hi There" = H(1)i(2) (3)T(4)h(5)e(6)r(7)e(8) = 8. Good.

Now — the task says TC6 mesaj is "Test Using Larger Than Block-Size Key - Hash Key First" → yes.

Hmm, but the task says for TC6: 1625567577, 518043263, 249790292. Let me verify... I can't verify but trust the task.

Wait — actually there's a concern. The task says "TC6 anahtar kt_tekrar(170, 131) (BLOKTAN UZUN → önce hash)". 131 > 64, so yes.

Now message byte counts: I need to pass correct mn. 20, 28, 54.

Hmm — actually could use `dizi_boyut(m1)` instead of hardcoding. But I'll just hardcode matching the task's description (probably fine).

Actually let me use `dizi_boyut` to be safer? Hmm, but the task hints at specific values... Actually, no I'll just use dizi_boyut for message lengths since it's safer. Wait, but that could break if I miscounted. Let me just use dizi_boyut for messages.

Hmm, but the task explicitly might expect certain calls. Let me just use `dizi_boyut(m1)` and `dizi_boyut(m2)` etc. Actually — wait, using dizi_boyut is more robust. Let me do that.

Also, for the key length: `dizi_boyut(k1)` for the 20-byte key. Good.

Actually let me use dizi_boyut for everything:

```
değişken r1: Dizi<sabitsüre<dtam32>> = hmac_sha256(k1, dizi_boyut(k1), m1, dizi_boyut(m1));
```

But wait — the task explicitly uses `an` parameter. Using dizi_boyut is fine and equivalent.

Hmm, but might there be an issue with `dizi_boyut` returning `tam32`? Yes. Good.

Actually, let me use explicit lengths as given in the task to reduce risk of unexpected behavior. Hmm. The task explicitly says "anahtar kt_tekrar(11, 20)". So key length 20 is known. Fine — I'll use the literal values.

Let me reconsider. Using literals is fine.

Now, `kt_tekrar(11 olarak dtam8, 20)` — the signature is `(b: dtam8, n: tam32)`. So first arg should be dtam8. So cast `11 olarak dtam8`. Or maybe just `11` will auto-convert. Let me be explicit.

Actually `of_cast` — hmm, `olarak` in the description. Let me just write `11`. Might not work. Let me be explicit with `olarak dtam8`.

Alternatively, use `sabitsüre_olustur` and `ifşa`? No, kt_tekrar takes `dtam8`.

Hmm, actually `dtam8` is the runtime integer type. `11` is a literal, likely inferred. Let me not cast and see. Actually to be safe, `11 olarak dtam8`.

Now for `kt_ozet_kontrol(ozet, 613247585, 3523623096, 433784513)` — these are tam64 values compared as `(ifşa(d[0]) olarak tam64)`. The ifşa returns dtam32 (32-bit), cast to tam64 gives zero-extended. 3523623096 < 2^32. 433784513 < 2^32. Good.

But wait — 3523623096 > 2^31. Is it representable as dtam32? dtam32 is signed probably. So the value might be negative when interpreted. Casting to tam64... hmm. If dtam32 is signed 32-bit, then 3523623096 becomes -771344200. Sign-extended to tam64 = -771344200. But our expected is 3523623096 (as unsigned). This would mismatch.

Hmm. This is a real concern. The task says "ifşa(d[0]) olarak tam64` değerleri (`olarak tam64`) w0/w1/w7'ye eşitse 1".

Let me look at the existing test for sha256_abc:
```
değişken s0: dtam32 = ifşa(state[0]);
...
eğer (s0 olarak tam64) != 0xBA7816BF { gecti = 0; }
```

0xBA7816BF = 3125917887 which is > 2^31. So the existing test uses `(s0 olarak tam64) != 0xBA7816BF` and this presumably passes. So the `olarak tam64` conversion somehow gives the unsigned value. Maybe dtam32 is unsigned, or the conversion is zero-extending.

OK so `olarak tam64` gives zero-extended. Good. So my approach works.

But wait — actually the check needs `ifşa(d[0])` returning dtam32. But `d` is `Dizi<sabitsüre<dtam32>>`. So `d[0]` is `sabitsüre<dtam32>`, `ifşa(d[0])` is `dtam32`. Then `olarak tam64`. Good.

Now, what values did the task give? "w0=613247585, w1=3523623096, w7=433784513". And for HMAC: "2956217441, 3638245459, 775081975", "1541194054, 3210769742, 1693202499", "1625567577, 518043263, 249790292".

All look plausible.

Now — I should double-check the check for `kc_ozet_kontrol`. Using strict comparison.

### File 3: test/kripto_kosum_harness.sh

```
BEKLENEN=15
```
→
```
BEKLENEN=63   # D-707: +16 çok bloklu SHA-256, +32 HMAC-SHA256 (RFC 4231)
```

Now let me also double-check `sha256_bayt` byte count and other details.

One concern: `toplam % 64 != 0` — does the language support `%`? The heap code uses `/` heavily. For `%` I saw in kem_kanal: "değişken bölenli `%`" implies `%` exists. OK good.

But actually, the SHA padding logic: I need total length to be a multiple of 64. My approach:

After appending n bytes + 1 (for 0x80), the total is n+1. Then I want (n+1) padded with zeros to reach a length L such that L + 8 ≡ 0 (mod 64), i.e., L ≡ 56 (mod 64).

Actually FIPS says: message + 0x80 + zeros + 64-bit length. Total = multiple of 64.

So after message + 0x80, we have n+1 bytes. We need zeros until length ℓ satisfies ℓ % 64 == 56. Then append 8 bytes.

So actually the zero-padding should bring to (ℓ % 64) == 56.

My code: compute `toplam = n + 9` (n + 1 + 8), then pad with zeros until `toplam % 64 == 0`. Then add 8 bytes.

Wait: n+9 is n (message) + 1 (0x80) + 8 (length). After padding totam to multiple of 64, then append 8 more bytes → total becomes multiple of 64 + 8 which is NOT multiple of 64!

Hmm, let me recompute.

Actually let me trace: after adding message bytes and 0x80, `dizi_boyut(tampon) = n + 1`. I want to pad with zeros until `dizi_boyut(tampon) ≡ 56 (mod 64)`. Then add 8 bytes → ≡ 0.

But my pseudocode: set `toplam = n + 9` (= n+1+8). Then pad until `toplam % 64 == 0`. Then add 8 bytes (via 8 dizi_ekle calls). Hmm — but the padding I add increases `toplam` but also increases `dizi_boyut(tampon)`. Wait, I'm tracking two things: `toplam` counter and actual `tampon` size. These should match.

Let me redo: After adding 0x80, `dizi_boyut(tampon) = n+1`. I want to add zeros until `dizi_boyut(tampon) = L` where L ≡ 56 (mod 64). Since I'll add 8 bytes of length, the final size = L+8 ≡ 0.

How to compute L? Start with L = n+1. Increment until L ≡ 56 (mod 64).

Alternatively: L = ((n+1+8+63)/64)*64 - 8. Hmm.

Let me think algebraically: Find smallest L ≥ n+1 with L ≡ 56 (mod 64). i.e., L+8 ≡ 0 (mod 64), so L+8 is next multiple of 64 that is ≥ n+9.

L + 8 = ceil((n+9)/64)*64. Then L = that - 8.

Let me just do a loop on dizi_boyut:

```
iken dizi_boyut(tampon) % 64 != 56 {
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
}
```

But wait — this could infinite-loop if n+1 ≡ 56 already and no zero needed? No: if n+1 ≡ 56, then the loop body doesn't execute, adding 0 zeros. But per FIPS we need at least... hmm, actually FIPS always appends 0x80 first, then zeros, then length. If after 0x80 the length is ≡ 56 mod 64, we add 0 zeros and then 8 length bytes. Total ≡ 0. But is that correct? Hmm — FIPS says: "append a 1 bit, then k zero bits, then 64-bit length, such that total is multiple of 512 bits (= 64 bytes)". If after append 1, we're at ≡ 56 mod 64, then k=0 and then length. That works. Except if we're at 56 exactly when there's no room? No — if we're at 56 mod 64 after 1 bit, then 56+8=64, we're good. OK.

But careful: the condition `% 64 != 56` with the loop. If starting at some value, we increment until we hit 56. Good.

But what if starting value (n+1) mod 64 is > 56? E.g., 57. Then we add 1 (58), 2 (59), ... 62, 63, 0, ..., 56. That works but requires possibly up to 63 iterations. Fine.

Middle case: starting 56 → 0 iterations. Good.

Hmm wait, actually let me reconsider. We should think: after adding 1 bit (0x80), the current length is n+1. If n+1 ≡ 56, we add length = 8 more bytes, total = 64. If n+1 ≡ 0 (say n=63), we need... n+1=64, 64 mod 64 = 0. Loop until ≡ 56: from 0, we increment to 56, adding 56 zeros. Then add 8 length bytes → 64 + 56 + 8 = 128. OK — 2 blocks.

Good, this works.

Let me rewrite the padding:

```
dizi_ekle(tampon, sabitsüre_olustur(0x80 olarak dtam8));
iken dizi_boyut(tampon) % 64 != 56 {
    dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
}
değişken bitlen: tam64 = (n olarak tam64) * 8;
dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 56) & 255) olarak dtam8));
...
dizi_ekle(tampon, sabitsüre_olustur((bitlen & 255) olarak dtam8));
```

Now, is `%` on `dizi_boyut(tampon)` OK? `dizi_boyut` returns tam32. `tam32 % 64`. Yes.

Actually the concern: `%` on dynamic divisor requires `kdl_panik` (per the heap comment). But 64 is a compile-time constant, so should be fine.

Wait, actually the heap comment says: "değişken bölenli `%`" (variable-divisor %). Constant divisor OK.

Now — number of blocks. `dizi_boyut(tampon) / 64`. Should be a multiple of 64. Good.

Then iterate.

Now, hmm — the byte extraction of `tampon[taban]` — `taban` is `b * 64 + j * 4`. And `tampon[taban]` gives `sabitsüre<dtam8>`. Casting to `sabitsüre<dtam32>`.

OK let me also check: does the block processing work with sha256_blok_sikistir expecting 16 words? Yes.

Now, hmm — `değişken blok: Dizi<sabitsüre<dtam32>> = [];` inside the while loop. Every iteration creates a fresh empty array. Then we ekle 16 words. Then sha256_blok_sikistir(state, blok). Good.

Wait, but `blok_w` param name in sha256_blok_sikistir... doesn't matter, positional.

Then return state.

Let me write sha256_bayt fully:

```
işlev sha256_bayt(veri: Dizi<sabitsüre<dtam8>>, n: tam32) -> Dizi<sabitsüre<dtam32>> {
    değişken tampon: Dizi<sabitsüre<dtam8>> = [];
    değişken i: tam32 = 0;
    iken i < n {
        dizi_ekle(tampon, veri[i]);
        i = i + 1;
    }
    dizi_ekle(tampon, sabitsüre_olustur(0x80 olarak dtam8));
    iken dizi_boyut(tampon) % 64 != 56 {
        dizi_ekle(tampon, sabitsüre_olustur(0 olarak dtam8));
    }
    değişken bitlen: tam64 = (n olarak tam64) * 8;
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 56) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 48) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 40) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 32) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 24) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 16) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur(((bitlen >> 8) & 255) olarak dtam8));
    dizi_ekle(tampon, sabitsüre_olustur((bitlen & 255) olarak dtam8));

    değişken durum: Dizi<dtam32> = sha256_H0();
    değişken state: Dizi<sabitsüre<dtam32>> = [
        sabitsüre_olustur(durum[0]), sabitsüre_olustur(durum[1]),
        sabitsüre_olustur(durum[2]), sabitsüre_olustur(durum[3]),
        sabitsüre_olustur(durum[4]), sabitsüre_olustur(durum[5]),
        sabitsüre_olustur(durum[6]), sabitsüre_olustur(durum[7])
    ];
    değişken blok_sayisi: tam32 = dizi_boyut(tampon) / 64;
    değişken b: tam32 = 0;
    iken b < blok_sayisi {
        değişken blok: Dizi<sabitsüre<dtam32>> = [];
        değişken j: tam32 = 0;
        iken j < 16 {
            değişken taban: tam32 = b * 64 + j * 4;
            değişken b0: sabitsüre<dtam32> = tampon[taban] olarak sabitsüre<dtam32>;
            değişken b1: sabitsüre<dtam32> = tampon[taban + 1] olarak sabitsüre<dtam32>;
            değişken b2: sabitsüre<dtam32> = tampon[taban + 2] olarak sabitsüre<dtam32>;
            değişken b3: sabitsüre<dtam32> = tampon[taban + 3] olarak sabitsüre<dtam32>;
            dizi_ekle(blok, (b0 << 24) | (b1 << 16) | (b2 << 8) | b3);
            j = j + 1;
        }
        sha256_blok_sikistir(state, blok);
        b = b + 1;
    }
    ver state;
}
```

Hmm — casting `sabitsüre<dtam8>` to `sabitsüre<dtam32>` — is that supported? The task says yes: "baytlar `olarak sabitsüre<dtam32>` ile genişletilir".

But careful: shifting `b0 << 24` — b0 is a byte value (0..255), so `<< 24` won't overflow 32 bits (max 255<<24 = 0xFF000000). Good.

Actually — hmm, one concern. `sabitsüre<dtam32>` shifted by 24 — does the shift use the same width? Should be fine.

Now for `hmac_sha256`. Let me finalize:

```kemgu
işlev hmac_sha256(
    anahtar: Dizi<sabitsüre<dtam8>>,
    an: tam32,
    mesaj: Dizi<sabitsüre<dtam8>>,
    mn: tam32
) -> Dizi<sabitsüre<dtam32>> {
    değişken k64: Dizi<sabitsüre<dtam8>> = [];
    değişken i: tam32 = 0;
    eğer an > 64 {
        değişken ozet: Dizi<sabitsüre<dtam32>> = sha256_bayt(anahtar, an);
        i = 0;
        iken i < 8 {
            değişken w: sabitsüre<dtam32> = ozet[i];
            dizi_ekle(k64, ((w >> 24) & 255) olarak sabitsüre<dtam8>);
            dizi_ekle(k64, ((w >> 16) & 255) olarak sabitsüre<dtam8>);
            dizi_ekle(k64, ((w >> 8) & 255) olarak sabitsüre<dtam8>);
            dizi_ekle(k64, (w & 255) olarak sabitsüre<dtam8>);
            i = i + 1;
        }
    } değilse {
        i = 0;
        iken i < an {
            dizi_ekle(k64, anahtar[i]);
            i = i + 1;
        }
    }
    iken dizi_boyut(k64) < 64 {
        dizi_ekle(k64, sabitsüre_olustur(0 olarak dtam8));
    }

    değişken ic: Dizi<sabitsüre<dtam8>> = [];
    i = 0;
    iken i < 64 {
        dizi_ekle(ic, k64[i] ^ sabitsüre_olustur(0x36 olarak dtam8));
        i = i + 1;
    }
    i = 0;
    iken i < mn {
        dizi_ekle(ic, mesaj[i]);
        i = i + 1;
    }
    değişken ic_ozet: Dizi<sabitsüre<dtam32>> = sha256_bayt(ic, 64 + mn);

    değişken dis: Dizi<sabitsüre<dtam8>> = [];
    i = 0;
    iken i < 64 {
        dizi_ekle(dis, k64[i] ^ sabitsüre_olustur(0x5C olarak dtam8));
        i = i + 1;
    }
    i = 0;
    iken i < 8 {
        değişken w: sabitsüre<dtam32> = ic_ozet[i];
        dizi_ekle(dis, ((w >> 24) & 255) olarak sabitsüre<dtam8>);
        dizi_ekle(dis, ((w >> 16) & 255) olarak sabitsüre<dtam8>);
        dizi_ekle(dis, ((w >> 8) & 255) olarak sabitsüre<dtam8>);
        dizi_ekle(dis, (w & 255) olarak sabitsüre<dtam8>);
        i = i + 1;
    }
    ver sha256_bayt(dis, 96);
}
```

Now — `w` is declared inside the first `eğer` block and also inside the last `iken`. These are in different scopes so OK. But — hmm, are they? The `eğer { ... }` creates a scope, and the second `w` is in the top-level function scope within the while. Actually let me just rename one to `ww` to avoid any shadowing issue. Let me keep the first as `w` and the second as `ww`.

Actually the issue: is a variable declared in an `eğer` block visible afterwards? Probably not. But to be safe, let me rename.

Let me use `ws` and `wi`.

Also, `256` and `0x80` etc. — literals. 0x80 as dtam8. Fine.

Now, one potential problem: `k64[i] ^ sabitsüre_olustur(0x36 olarak dtam8)`. The `0x36 olarak dtam8` — 0x36 = 54, fits in dtam8. OK.

Now — the ESKİ for file 1. Let me use the hmac_pad_uret function as the anchor with the full body.

Hmm wait — the task said the ESKİ is "son satırlar, kesitte `hmac_pad_uret`'in sonu". So I should match the end of hmac_pad_uret, i.e., `    ver 0;\n}`. But that's not unique (many functions end with `ver 0; }` or `ver; }`).

Let me use the full function body as the anchor for uniqueness. That's safest.

Actually, the task shows only the hmac_pad_uret function in the latest kesit. And it says this is at line 259-275. If the file is longer than 275 lines... hmm. Actually if there's stuff after, the anchor insertion happens after hmac_pad_uret but before the later stuff. Position matters for output—actually for correctness it might matter if the later stuff calls HMAC functions? Unlikely since HMAC just defined. But I'll trust the task that hmac_pad_uret is at the end (or that appending there is intended).

Actually — re-reading: "dosyanın SONUNA (ESKİ = son satırlar, kesitte `hmac_pad_uret`'in sonu)". OK.

Let me just use the closing of hmac_pad_uret:

ESKİ:
```
        inner[i] = anahtar[i] ^ sabitsüre_olustur(ipad);
        outer[i] = anahtar[i] ^ sabitsüre_olustur(opad);
        i = i + 1;
    }
    ver 0;
}
```

That's unique enough.

YENİ: same + new functions.

Now file 2. Three blocks.

Block (a):
ESKİ: `    eğer kt_sec_u64_ust_bit() == 1 { skor = skor + 8; }   // D-629`
YENİ: that line + two more lines.

Block (b):
ESKİ: `    ver skor;   // 15 = tam geçiş`
YENİ: `    ver skor;   // 63 = tam geçiş`

Block (c):
ESKİ: `işlev main() -> tam32 {`
YENİ: new functions + that line.

Hmm — actually the ESKİ for (b) `    ver skor;   // 15 = tam geçiş` is inside main, and (a) is also inside main. But they don't overlap. Fine.

The (c) ESKİ `işlev main() -> tam32 {` — need to be unique. Should be.

Now file 3.

Let me finalize the code and write the edits.

Actually wait — I realize `değişken durum: Dizi<dtam32> = sha256_H0();` — this returns `Dizi<dtam32>`. Good.

But wait — this is called every time `sha256_bayt` runs. And `sha256_H0()` is at line 109. Fine.

Also I want to check: is `dizi_boyut` in scope? The context mentions "boyut `dizi_boyut(d)`". So yes.

Also `dizi_ekle` — mentioned.

OK.

One more thing — `0x61626380` in the test. And for me the byte-by-byte would be 0x61, 0x62, 0x63 = "abc". With padding 0x80. Let me verify my sha256_bayt on "abc": n=3. tampon = [0x61,0x62,0x63,0x80]. Then pad until % 64 == 56 → 56 - 4 = 52 zeros. Then 8 bytes of length (24). Total = 4+52+8 = 64. Matches the existing test just with 0-length block. Good.

Now let me also double-check the byte ordering for SHA word assembly. FIPS: message is big-endian. So w = b0<<24 | b1<<16 | b2<<8 | b3. That matches.

Now — the test comment says "w = b0<<24 | b1<<16 | b2<<8 | b3, baytlar `olarak sabitsüre<dtam32>` ile genişletilir". Good.

Let me also consider: in sha256_blok_sikistir, if the state is updated in place, then returns... The test calls it and then reads state. So state is updated. Good.

OK, let me write it out.

Hmm — one more consideration about `değilse` vs. `yoksa`. In the heap code, I saw `eğer ... { } değilse { }` — yes, `değilse`. OK.

Now for the harness, the exact old text: `BEKLENEN=15` (with possible trailing whitespace?). The kesit shows `BEKLENEN=15` alone. YENİ: `BEKLENEN=63   # D-707: +16 çok bloklu SHA-256, +32 HMAC-SHA256 (RFC 4231)`.

OK writing it all out now.

Wait — let me reconsider the `işlev main() -> tam32 {` anchor. If main's `skor` accumulation has different content, but the ESKİ is literally `işlev main() -> tam32 {` — that line appears once.

Now — order of blocks. The task says "başka dosyaya DOKUNMA". Fine.

Let me also verify: the new test uses `metin_uzunluk` and `metin_bayt`. Are those in scope in the test file? They're runtime primitives. The kesit uses `metin_uzunluk` in kem_heap. Should be available. And the test file... hmm.

Actually — kem_heap.kem defines `kdl_metin_uzunluk` and `kdl_metin_bayt` (with `kdl_` prefix). But the prelude uses `metin_uzunluk` and `metin_bayt` without prefix — these are presumably built-in intrinsic names that map to `kdl_metin_uzunluk`/`kdl_metin_bayt`. OK.

So in the test, `metin_uzunluk(s)` and `metin_bayt(s, i)` should work. Let me trust that.

Now, `sabitsüre_olustur((metin_bayt(s, i) olarak dtam8))` — `metin_bayt` returns tam8, cast to dtam8. OK.

Alright. Let me now also make sure `işlev kt_metin_bayt(s: metin)` — `metin` is the string type. Good.

Now — the `Dizi<sabitsüre<dtam8>> = []` empty literal. The task says: "Boş dizi: `değişken d: Dizi<sabitsüre<dtam8>> = [];`". Good.

Writing everything.

Actually — one more thought. In `kt_sha256_cok_blok`, I use `dizi_boyut(veri)` for n. But that's `tam32`. Good. Actually "56 bayt" — let me verify:
"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"

Let me count:
a-b-c-d (4) b-c-d-e (8) c-d-e-f (12) d-e-f-g (16) e-f-g-h (20) f-g-h-i (24) g-h-i-j (28) h-i-j-k (32) i-j-k-l (36) j-k-l-m (40) k-l-m-n (44) l-m-n-o (48) m-n-o-p (52) n-o-p-q (56). Yes, 56 bytes.

56+1=57, 57 mod 64 = 57. Pad until 56 mod 64. From 57: 58,...,63,0,...,55,56 — 63 iterations. Then +8 = 64... wait 56+8=64, total = 64? No wait — current size after pad is 56, but the block started at 64? Let me retrace.

We append 56 message bytes + 0x80 = 57 bytes. Then we pad to 56 mod 64. From 57, next ≡56 mod 64 is 120 (57, 58, ..., 120). So pad 63 zeros → 120 bytes. Then 8 length bytes → 128 bytes = 2 blocks. Yes matches "56 bayt → İKİ blok dolgu".

Good.

Now let me finalize.

Actually — hmm, one issue: the `iken dizi_boyut(tampon) % 64 != 56` — dizi_boyut returns tam32. Comparing tam32 to 56. Fine.

But there's a potential issue: `% 64` on tam32. If tam32 is signed and value fits... fine.

OK. Let me also double check `kt_hmac_rfc4231` for TC2:
- key "Jefe" 4 bytes.
- message "what do ya want for nothing?" 28 bytes.
OK.

Message length for TC6: 54. Let me write 54.

Actually, safer to use `dizi_boyut(m3)`. Let me just use dizi_boyut for message lengths and key lengths. Actually — let me be careful: what if metin_uzunluk is not the count of "bytes" for the string? For ASCII, yes. OK.

I'll use `dizi_boyut`.

Actually, hmm. Let me just use literals as I know them, because the task gave specific numbers that were verified. If I use dizi_boyut, it should give the same. Either works.

I'll use `dizi_boyut` since it avoids counting errors. Actually wait — the task description says "mesaj "Hi There" → 2956217441...". Hinting mesaj length. Using dizi_boyut gives the same. OK let me use dizi_boyut everywhere.

Final code for test file:

```kemgu
işlev kt_metin_bayt(s: metin) -> Dizi<sabitsüre<dtam8>> {
    değişken d: Dizi<sabitsüre<dtam8>> = [];
    değişken i: tam32 = 0;
    iken i < metin_uzunluk(s) {
        dizi_ekle(d, sabitsüre_olustur((metin_bayt(s, i) olarak dtam8)));
        i = i + 1;
    }
    ver d;
}

işlev kt_tekrar(b: dtam8, n: tam32) -> Dizi<sabitsüre<dtam8>> {
    değişken d: Dizi<sabitsüre<dtam8>> = [];
    değişken i: tam32 = 0;
    iken i < n {
        dizi_ekle(d, sabitsüre_olustur(b));
        i = i + 1;
    }
    ver d;
}

işlev kt_ozet_kontrol(d: Dizi<sabitsüre<dtam32>>, w0: tam64, w1: tam64, w7: tam64) -> tam32 {
    değişken gecti: tam32 = 1;
    eğer (ifşa(d[0]) olarak tam64) != w0 { gecti = 0; }
    eğer (ifşa(d[1]) olarak tam64) != w1 { gecti = 0; }
    eğer (ifşa(d[7]) olarak tam64) != w7 { gecti = 0; }
    ver gecti;
}

işlev kt_sha256_cok_blok() -> tam32 {
    değişken s: metin = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    değişken veri: Dizi<sabitsüre<dtam8>> = kt_metin_bayt(s);
    değişken ozet: Dizi<sabitsüre<dtam32>> = sha256_bayt(veri, dizi_boyut(veri));
    ver kt_ozet_kontrol(ozet, 613247585, 3523623096, 433784513);
}

işlev kt_hmac_rfc4231() -> tam32 {
    değişken k1: Dizi<sabitsüre<dtam8>> = kt_tekrar(11 olarak dtam8, 20);
    değişken m1: Dizi<sabitsüre<dtam8>> = kt_metin_bayt("Hi There");
    değişken r1: Dizi<sabitsüre<dtam32>> = hmac_sha256(k1, dizi_boyut(k1), m1, dizi_boyut(m1));
    eğer kt_ozet_kontrol(r1, 2956217441, 3638245459, 775081975) != 1 { ver 0; }

    değişken k2: Dizi<sabitsüre<dtam8>> = kt_metin_bayt("Jefe");
    değişken m2: Dizi<sabitsüre<dtam8>> = kt_metin_bayt("what do ya want for nothing?");
    değişken r2: Dizi<sabitsüre<dtam32>> = hmac_sha256(k2, dizi_boyut(k2), m2, dizi_boyut(m2));
    eğer kt_ozet_kontrol(r2, 1541194054, 3210769742, 1693202499) != 1 { ver 0; }

    değişken k3: Dizi<sabitsüre<dtam8>> = kt_tekrar(170 olarak dtam8, 131);
    değişken m3: Dizi<sabitsüre<dtam8>> = kt_metin_bayt("Test Using Larger Than Block-Size Key - Hash Key First");
    değişken r3: Dizi<sabitsüre<dtam32>> = hmac_sha256(k3, dizi_boyut(k3), m3, dizi_boyut(m3));
    eğer kt_ozet_kontrol(r3, 1625567577, 518043263, 249790292) != 1 { ver 0; }

    ver 1;
}

işlev main() -> tam32 {
```

Hmm — the task said the w0/w1/w7 checks should be exact. Note the `! = 1` with `ver 0`.

Actually the task says "üç vektörün üçü de geçerse 1". So return 1 if all pass. Good.

Now, `170 olarak dtam8` — 170 fits in dtam8? dtam8 is 8-bit. 170 < 256. Good, but if signed 8-bit, 170 > 127 → negative. Hmm. But the test is about a byte. And key byte is 0xAA = 170. It should be fine (bit pattern).

Hmm — actually the compiler might refuse if dtam8 is signed and 170 overflows. Let me think. The heap code has `değişken cb: tam8 = c olarak tam8;` — casting tam32 to tam8. Not a literal. Hmm.

Let me just write `170` and hope for auto-conversion. Actually, to be safe, maybe write `kt_tekrar(170 olarak dtam8, 131)` and hope `olarak` handles wraparound (like C).

Alternatively, use `0xAA`. Hmm.

Actually, maybe the test file needs a different approach. Let me just do `170 olarak dtam8`.

Hmm, but what if the parser rejects? Actually since this is the same pattern as elsewhere. Let me search... in kem_heap: `*piz = izin;` (dtam16). `*pip = 1 olarak dtam8;`. So `1 olarak dtam8` used. And `0 olarak dtam8`. Yes.

But `170 olarak dtam8` — will the literal be inferred as tam64 then truncated? If "olarak" is a truncating cast, yes.

OK.

Now let me worry about `11 olarak dtam8` — 11 < 128, fits signed. Fine.

`170 olarak dtam8` — hmm. If the compiler warns/errors on literal overflow for signed dtam8... Let me instead use the pattern `255` (in hmac), which we use in shifts: `& 255`. That adds to `sabitsüre<dtam32>` so no overflow issue there.

For the key byte, I could avoid by writing `ifşa(sabitsüre_olustur(...))`... no.

Hmm, alternative: `170 & 255`? Still a literal.

Actually, `170` as bytes — let me just leave it and hope. Or, safer, cast to tam8 first: `(170) olarak dtam8`. Same thing.

Actually let me check if there's a mutable way... could do `değişken kb: dtam8 = 0;` then... no, can't easily make 170.

Let me try: `kt_tekrar(0xAA olarak dtam8, 131)`. 0xAA = 170.

Hmm, same problem. Let me just use `170 olarak dtam8`.

Actually wait — a cleverer approach. Since `170` might wrap in dtam8 anyway, and `sabitsüre_olustur` takes dtam8. Hmm.

OK let me just try `170 olarak dtam8` and hope the compiler handles it. If it errors, we'd need a different approach.

Alternatively, I could avoid the literal: build the key from a string. 131 bytes... hmm. Actually, RFC 4231 TC6 key = 131 bytes of 0xAA. Not a string.

Alternatively — can we compute it? `170` = `128 + 42`? Still literals. `42 olarak dtam8 + 128 olarak dtam8`? If dtam8 is signed, 128 overflows. Hmm.

OK let me just use `170 olarak dtam8`.

Hmm, actually the reasonable thing: since the task explicitly says "anahtar kt_tekrar(170, 131)", it must work. So write it as is.

OK. Let me now also verify — actually I realize the task said the parametgen for kt_tekrar is `kt_tekrar(11, 20)`, `kt_tekrar(170, 131)`.

So the b param is literal. OK.

Writing now.

Final output.

Actually, wait — one more concern about the `hmac_sha256` signature. The task said:

`işlev hmac_sha256(anahtar: Dizi<sabitsüre<dtam8>>, an: tam32, mesaj: Dizi<sabitsüre<dtam8>>, mn: tam32) -> Dizi<sabitsüre<dtam32>>`

OK on one line. But multi-line is fine stylistically. I'll follow the multi-line style like the file does for hmac_pad_uret.

Similarly for sha256_bayt — the task writes it on one line. I'll write it one line or multi-line. Follow the file's style. Let me use single line for them to match the task hint.

Actually — hmm. Doesn't matter functionally.

OK.

Let me write out all `duzenle` blocks.
