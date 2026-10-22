# Tur 26 — os_rastgele — `deepseek-v4.1-flash`

- Tarih: 2026-10-22 19:08:08 · süre 516.0 sn · finish `stop` · reasoning_effort `low`
- usage: `{"prompt_tokens": 12957, "total_tokens": 26100, "completion_tokens": 13143, "prompt_tokens_details": {"cached_tokens": 0, "created_cache_tokens": 12928}, "completion_tokens_details": {"reasoning_tokens": 11076}}`
- Değişiklik blokları: 2 — duzenle:stdlib/kripto/rastgele.kem, duzenle:test/stdlib/test_kripto_kosum.kem
- İstem: [tur_26_os_rastgele_istem.md](tur_26_os_rastgele_istem.md)

## Yanıt

```duzenle:stdlib/kripto/rastgele.kem
<<<<<<< ESKI
// Platform implementasyonu (V2):
//   - Linux: syscall(SYS_getrandom, buf, len, 0)
//   - Windows: BCryptGenRandom(NULL, buf, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG)
//   - macOS: getentropy(buf, len)
//
// V1: stub — xorshift64 fallback (KRİPTO GÜVENSİZ — sadece test).
işlev secure_rastgele_doldur(
    sonuc: Dizi<sabitsüre<dtam8>>,
    gereksinim: tam32
) -> tam32 {
    // V1 PLACEHOLDER — gerçek getrandom syscall V2'de.
    // Bu kod KRİPTO GÜVENSİZ, sadece compile/test için.
    değişken state: dtam64 = 0x9E3779B97F4A7C15;  // Fibonacci-türevsel sabit
    değişken i: tam32 = 0;
    iken i < gereksinim {
        state = xorshift64_adim(state);
        // dtam64 -> dtam8: önce dtam32'ye daralt (izinli), sonra dtam8'e (izinli).
        // E004 sadece kw_kaynak >= 64 && kw_hedef < 32 vurgular; iki-adım kaçınır.
        değişken orta: dtam32 = (state olarak dtam32);
        değişken alt: dtam8 = (orta olarak dtam8);
        sonuc[i] = sabitsüre_olustur(alt);
        i = i + 1;
    }
    ver gereksinim;
}
=======
// [D-713] OS entropisi (Linux /dev/urandom, Windows BCryptGenRandom); başarısızlıkta PANİK — sessiz 0 YOK.
işlev secure_rastgele_doldur(
    sonuc: Dizi<sabitsüre<dtam8>>,
    gereksinim: tam32
) -> tam32 {
    değişken k: dtam64 = 0;
    değişken i: tam32 = 0;
    iken i < gereksinim {
        eğer i % 8 == 0 { k = os_rastgele(); }
        // dtam64 -> dtam8: önce dtam32'ye daralt (izinli), sonra dtam8'e (izinli).
        // E004 sadece kw_kaynak >= 64 && kw_hedef < 32 vurgular; iki-adım kaçınır.
        değişken orta: dtam32 = (k olarak dtam32);
        değişken alt: dtam8 = (orta olarak dtam8);
        sonuc[i] = sabitsüre_olustur(alt);
        k = k >> 8;
        i = i + 1;
    }
    ver gereksinim;
}

// [D-713] Gizli 64 bitlik rastgele kelime.
işlev os_rastgele_kelime() -> sabitsüre<dtam64> { ver sabitsüre_olustur(os_rastgele()); }
>>>>>>> YENI
```

```duzenle:test/stdlib/test_kripto_kosum.kem
<<<<<<< ESKI
işlev main() -> tam32 {
    değişken skor: tam32 = 0;
    eğer kt_kontrol() == 1 { skor = skor + 1; }
    eğer kt_chacha_qr() == 1 { skor = skor + 2; }
    eğer kt_sha256_abc() == 1 { skor = skor + 4; }
    eğer kt_sec_u64_ust_bit() == 1 { skor = skor + 8; }   // D-629
    eğer kt_sha256_cok_blok() == 1 { skor = skor + 16; }   // D-709
    eğer kt_hmac_rfc4231() == 1 { skor = skor + 32; }     // D-709
    eğer kt_blake3() == 1 { skor = skor + 64; }           // D-710
    ver skor;   // 127 = tam geçiş
}
=======
// [D-713] OS rastgele: 32 kelimenin OR'u TÜM bitleri, AND'i HİÇBİR biti taşımamalı
// (bir bitin 32 çekilişte sabit kalma olasılığı 2^-32). Sabit dönen kaynak ikisini de düşürür.
işlev kt_os_rastgele() -> tam32 {
    değişken veya_t: dtam64 = 0;
    değişken ve_t: dtam64 = 0;
    ve_t = ve_t - 1;                 // tüm bitler 1
    değişken hepsi: dtam64 = ve_t;
    değişken i: tam32 = 0;
    iken i < 32 {
        değişken x: dtam64 = os_rastgele();
        veya_t = veya_t | x;
        ve_t = ve_t & x;
        i = i + 1;
    }
    eğer veya_t != hepsi { ver 0; }
    eğer ve_t != 0 { ver 0; }
    ver 1;
}
işlev main() -> tam32 {
    değişken skor: tam32 = 0;
    eğer kt_kontrol() == 1 { skor = skor + 1; }
    eğer kt_chacha_qr() == 1 { skor = skor + 2; }
    eğer kt_sha256_abc() == 1 { skor = skor + 4; }
    eğer kt_sec_u64_ust_bit() == 1 { skor = skor + 8; }   // D-629
    eğer kt_sha256_cok_blok() == 1 { skor = skor + 16; }   // D-709
    eğer kt_hmac_rfc4231() == 1 { skor = skor + 32; }     // D-709
    eğer kt_blake3() == 1 { skor = skor + 64; }           // D-710
    eğer kt_os_rastgele() == 1 { skor = skor + 128; }    // D-713
    ver skor;   // 255 = tam geçiş
}
>>>>>>> YENI
```

**Açıklama:** `secure_rastgele_doldur` gövdesi xorshift64 yer tutucusundan `os_rastgele()` tabanlı gerçek OS entropisine geçirildi; her 8 baytta bir yeni kelime çekilir ve kelime KÜÇÜK-UÇ sırayla (`k = k >> 8`) yazılır. E004'ten kaçınmak için mevcut iki-adımlı daraltma (`k → dtam32 → dtam8`) korundu. V1/V2/KRİPTO GÜVENSİZ yorumları kaldırılıp D-713 notuyla değiştirildi; dosya başlığındaki xorshift64/xoshiro işlevleri test PRNG'si olarak bırakıldı. Hemen altına `os_rastgele_kelime()` eklendi. Test dosyasına 32 kelimelik OR/AND tabanlı `kt_os_rastgele` sınaması ve `main`'e bit 128 katkısı eklendi, toplam yorumu 255'e güncellendi.

**VARSAYIMLAR**
- `secure_rastgele_doldur`'un kesitte görünmeyen kapanış satırlarının tam olarak `    }` (döngü), `    ver gereksinim;` ve `}` olduğu varsayıldı; ESKI bloğu bu kapanışı içerecek şekilde yazıldı (yeni işlevi "hemen altına" ekleyebilmek için). Gerçek kapanış `ver i;` gibi farklıysa ESKI eşleşmez.
- `i % 8` sabit bölenli olduğundan D-502 sıfıra-bölme panik yolu tetiklenmez (derleyici sabit katına indirger).

## Düşünme (reasoning)

Let me produce the edits carefully based on the cuts.

Edit 1: rastgele.kem. I need exact ESKI text from the cut. Let me look at what's shown:

```
// Doldur — sonuc dizisini secure random bytes ile doldurur.
// dönüş: dolan byte sayısı (eşit gereksinim'e veya hata için < gereksinim)
//
// Platform implementasyonu (V2):
//   - Linux: syscall(SYS_getrandom, buf, len, 0)
//   - Windows: BCryptGenRandom(NULL, buf, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG)
//   - macOS: getentropy(buf, len)
//
// V1: stub — xorshift64 fallback (KRİPTO GÜVENSİZ — sadece test).
işlev secure_rastgele_doldur(
    sonuc: Dizi<sabitsüre<dtam8>>,
    gereksinim: tam32
) -> tam32 {
    // V1 PLACEHOLDER — gerçek getrandom syscall V2'de.
    // Bu kod KRİPTO GÜVENSİZ, sadece compile/test için.
    değişken state: dtam64 = 0x9E3779B97F4A7C15;  // Fibonacci-türevsel sabit
    değişken i: tam32 = 0;
    iken i < gereksinim {
        state = xorshift64_adim(state);
        // dtam64 -> dtam8: önce dtam32'ye daralt (izinli), sonra dtam8'e (izinli).
        // E004 sadece kw_kaynak >= 64 && kw_hedef < 32 vurgular; iki-adım kaçınır.
        değişken orta: dtam32 = (state olarak dtam32);
        değişken alt: dtam8 = (orta olarak dtam8);
        sonuc[i] = sabitsüre_olustur(alt);
        i = i + 1;
```

The cut ends mid-block. Problem: I need to produce the edit matching exact text. The cut is truncated (ends at `i = i + 1;`). Since I can only see this portion, I need to construct the ESKI text using what's visible. But the full function presumably has a closing `}` and `ver ...`.

Hmm, the instruction says "ESKİ = kesitteki satırlar birebir". But the cut is incomplete. Let me re-read.

The cut shows lines 82-115. The function continues beyond what's shown? The snippet ends at `i = i + 1;`. There must be more: `    }`, `    ver gereksinim;`, `}`.

I need to be careful — I should only replace what I can see exactly. But the task says replace the body. Since I don't have the remaining lines, maybe I should replace up to what's shown and leave the rest? But that would produce broken code.

Actually, maybe the safest is to replace the body from the placeholder comment through to the end of the loop, but I don't know the exact closing.

Let me think. The instruction: "Gövdeyi (xorshift64 yer tutucusu) şununla DEĞİŞTİR". The pseudocode shows the new body ending with `ver gereksinim`.

I think I should replace exactly the text I can see, which starts at the "// Doldur" comment and ends at `i = i + 1;`. But the new text needs to include the closing of the loop and return, which would leave duplication.

Alternative: Replace only the body portion that I can see, keeping the trailing `}` lines. But I can't see them.

Hmm. Let me reconsider. Perhaps the cut is literally the whole function as it exists — ending with `i = i + 1;` and the cut notation implies there's more. The cut header says "KESİT (satır 82-115)". Line 115 would be around `i = i + 1;`. So the function body probably continues past 115.

Given the ambiguity, best approach: produce an edit whose ESKI text matches exactly the lines shown (from the comment block through the last visible line `i = i + 1;`), and whose YENI text is the new version. The remaining lines (closing `}`, `ver`, `}`) are left untouched after the edit.

But if I replace through `i = i + 1;`, the new body already has its own loop closing and return, so I'd need to NOT include them in YENI, leaving the original closing to follow. But then the new YENI would end with `i = i + 1` inside a `while` loop that... hmm, that's tricky because my new body has the same structure: `ik e i < gereksinim { ... i = i + 1` and then closing `}` and `ver gereksinim` from original.

Wait — actually the original's closing braces after `i = i + 1;` would be `    }` (close while), then likely `    ver gereksinim;` then `}`. If I keep those, but the new body I write ends with `i = i + 1`... but then I need to add `k = k >> 8` after `i = i + 1`. Hmm the pseudocode order is: sonuc[i]=..., k = k >> 8, i = i + 1.

So the last line of my replacement would be `i = i + 1;` and then the original closing `}` and `ver gereksinim;` and `}` follow. That works structurally IF the original indeed has those closings.

But wait, task pseudocode ends with `ver gereksinim`. If original already has `ver gereksinim;`, good. But I don't know that. It could have `ver i;` or something.

Given the risk, I think the cleanest is to replace the entire body including a fresh `ver gereksinim;` and rely on the original trailing lines having the braces. But that's assuming.

Actually, let me reconsider — the safest edit that doesn't depend on unseen lines: replace from `// V1 PLACEHOLDER` through `i = i + 1;` (the last visible line). This is exactly what I can see. Leave everything after untouched. My new text ends with `i = i + 1;` so it must include `k = k >> 8` before it. But the new pseudocode order is: `sonuc[i] = ...; k = k >> 8; i = i + 1;`. So my replacement ending at `i = i + 1;` works!

Let me structure:

ESKI:
```
    // V1 PLACEHOLDER — gerçek getrandom syscall V2'de.
    // Bu kod KRİPTO GÜVENSİZ, sadece compile/test için.
    değişken state: dtam64 = 0x9E3779B97F4A7C15;  // Fibonacci-türevsel sabit
    değişken i: tam32 = 0;
    iken i < gereksinim {
        state = xorshift64_adim(state);
        // dtam64 -> dtam8: önce dtam32'ye daralt (izinli), sonra dtam8'e (izinli).
        // E004 sadece kw_kaynak >= 64 && kw_hedef < 32 vurgular; iki-adım kaçınır.
        değişken orta: dtam32 = (state olarak dtam32);
        değişken alt: dtam8 = (orta olarak dtam8);
        sonuc[i] = sabitsüre_olustur(alt);
        i = i + 1;
```

YENI:
```
    değişken k: dtam64 = 0;
    değişken i: tam32 = 0;
    iken i < gereksinim {
        eğer i % 8 == 0 { k = os_rastgele(); }
        // dtam64 -> dtam8: önce dtam32'ye daralt (izinli), sonra dtam8'e (izinli).
        // E004 sadece kw_kaynak >= 64 && kw_hedef < 32 vurgular; iki-adım kaçınır.
        değişken orta: dtam32 = (k olarak dtam32);
        değişken alt: dtam8 = (orta olarak dtam8);
        sonuc[i] = sabitsüre_olustur(alt);
        k = k >> 8;
        i = i + 1;
```

But then the original after `i = i + 1;` has `    }` closing the while, then `ver gereksinim;` presumably. Good.

But also the instruction: "İşlevin üstündeki 'V1 PLACEHOLDER / KRİPTO GÜVENSİZ / V2' yorumlarını kaldır, yerine: // [D-713] ..." So I should remove the "V1: stub — xorshift64 fallback (KRİPTO GÜVENSİZ — sadece test)." too, which is above `işlev`. And the "// V1 PLACEHOLDER" inside.

Hmm, but there are also the Platform implementasyonu (V2) comments. Task says remove "V1 PLACEHOLDER / KRİPTO GÜVENSİZ / V2 yorumları". Let me interpret: the V1 PLACEHOLDER comment, KRİPTO GÜVENSİZ comments, and V2 references. Replace with the new comment.

But files: "Dosya başlığındaki 'V1: xorshift64 fallback' satırları ve xorshift/xoshiro işlevleri KALSIN (test PRNG'si)." — file header lines keep. So the header "V1: stub" line — wait that's not the file header, that's right above the function. Hmm.

Actually "Dosya başlığındaki" = file header (top of file). The "// V1: stub — xorshift64 fallback..." above the function is not file header, it's function comment. So it should be removed.

Let me reconsider the scope. I'll replace from the "// Doldur —" comment through the last visible line `i = i + 1;`.

Let me carefully reconstruct:

ESKI (starting with the Doldur comment):
```
// Doldur — sonuc dizisini secure random bytes ile doldurur.
// dönüş: dolan byte sayısı (eşit gereksinim'e veya hata için < gereksinim)
//
// Platform implementasyonu (V2):
//   - Linux: syscall(SYS_getrandom, buf, len, 0)
//   - Windows: BCryptGenRandom(NULL, buf, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG)
//   - macOS: getentropy(buf, len)
//
// V1: stub — xorshift64 fallback (KRİPTO GÜVENSİZ — sadece test).
işlev secure_rastgele_doldur(
    sonuc: Dizi<sabitsüre<dtam8>>,
    gereksinim: tam32
) -> tam32 {
    // V1 PLACEHOLDER — gerçek getrandom syscall V2'de.
    // Bu kod KRİPTO GÜVENSİZ, sadece compile/test için.
    değişken state: dtam64 = 0x9E3779B97F4A7C15;  // Fibonacci-türevsel sabit
    değişken i: tam32 = 0;
    iken i < gereksinim {
        state = xorshift64_adim(state);
        // dtam64 -> dtam8: önce dtam32'ye daralt (izinli), sonra dtam8'e (izinli).
        // E004 sadece kw_kaynak >= 64 && kw_hedef < 32 vurgular; iki-adım kaçınır.
        değişken orta: dtam32 = (state olarak dtam32);
        değişken alt: dtam8 = (orta olarak dtam8);
        sonuc[i] = sabitsüre_olustur(alt);
        i = i + 1;
```

YENI:
```
// Doldur — sonuc dizisini secure random bytes ile doldurur.
// dönüş: dolan byte sayısı (eşit gereksinim'e veya hata için < gereksinim)
//
// [D-713] OS entropisi (Linux /dev/urandom, Windows BCryptGenRandom); başarısızlıkta PANİK — sessiz 0 YOK.
işlev secure_rastgele_doldur(
    sonuc: Dizi<sabitsüre<dtam8>>,
    gereksinim: tam32
) -> tam32 {
    değişken k: dtam64 = 0;
    değişken i: tam32 = 0;
    iken i < gereksinim {
        eğer i % 8 == 0 { k = os_rastgele(); }
        // dtam64 -> dtam8: önce dtam32'ye daralt (izinli), sonra dtam8'e (izinli).
        // E004 sadece kw_kaynak >= 64 && kw_hedef < 32 vurgular; iki-adım kaçınır.
        değişken orta: dtam32 = (k olarak dtam32);
        değişken alt: dtam8 = (orta olarak dtam8);
        sonuc[i] = sabitsüre_olustur(alt);
        k = k >> 8;
        i = i + 1;
```

Then append the new function os_rastgele_kelime after secure_rastgele_doldur. But since the function ends after `i = i + 1;` with closing braces I don't see, I can't append cleanly within the same block.

Hmm. The task says "aynı dosyaya, secure_rastgele_doldur'un hemen ALTINA ekle". So I need to add os_rastgele_kelime after the closing brace of secure_rastgele_doldur, which I don't see.

This is problematic. Maybe I should do a second edit that inserts after something I can see... but I can't see the closure.

Alternatively, maybe the cut actually ends at line 115 which might include more. Let me count. The cut says satır 82-115, so 34 lines. Let me count the included lines from "// Doldur":

Actually let me count lines from the shown cut (starting after the blank line and comment):

Line: `// Doldur...` — that's probably line 82? Let me count all shown lines:
1. (blank line shown at top? there's a blank line before comment)
Actually cut starts:
```

// =============================================================================
// CSPRNG API — Bağımlı Platform (V1 placeholder)
...
```
The cut contains those header lines too. Let me not over-count.

I think the cut shows everything from line 82 to 115 which is 34 lines. Let me count actual lines:

```
(1)
// =============================================================================
// CSPRNG API — Bağımlı Platform (V1 placeholder)
// =============================================================================
//
// Bu fonksiyonlar OS'un secure RNG kaynağına gider. V1'de syscall altyapısı
// yok, bu yüzden gerçek implementasyon güvensiz blok + extern call gerek.
// API yüzeyi şimdiden tanımlı — V2 syscall layer tamamlandığında impl gelir.

// Doldur — sonuc dizisini secure random bytes ile doldurur.
// dönüş: dolan byte sayısı (eşit gereksinim'e veya hata için < gereksinim)
//
// Platform implementasyonu (V2):
//   - Linux: syscall(SYS_getrandom, buf, len, 0)
//   - Windows: BCryptGenRandom(NULL, buf, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG)
//   - macOS: getentropy(buf, len)
//
// V1: stub — xorshift64 fallback (KRİPTO GÜVENSİZ — sadece test).
işlev secure_rastgele_doldur(
    sonuc: Dizi<sabitsüre<dtam8>>,
    gereksinim: tam32
) -> tam32 {
    // V1 PLACEHOLDER — gerçek getrandom syscall V2'de.
    // Bu kod KRİPTO GÜVENSİZ, sadece compile/test için.
    değişken state: dtam64 = 0x9E3779B97F4A7C15;  // Fibonacci-türevsel sabit
    değişken i: tam32 = 0;
    iken i < gereksinim {
        state = xorshift64_adim(state);
        // dtam64 -> dtam8: önce dtam32'ye daralt (izinli), sonra dtam8'e (izinli).
        // E004 sadece kw_kaynak >= 64 && kw_hedef < 32 vurgular; iki-adım kaçınır.
        değişken orta: dtam32 = (state olarak dtam32);
        değişken alt: dtam8 = (orta olarak dtam8);
        sonuc[i] = sabitsüre_olustur(alt);
        i = i + 1;
```

Count: blank=1, ====2, CSPRNG=3, ====4, //=5, Bu=6, yok=7, API=8, blank=9, Doldur=10, dönüş=11, //=12, Platform=13, Linux=14, Windows=15, macOS=16, //=17, V1 stub=18, işlev secure...=19, sonuc=20, gereksinim=21, ) -> tam32=22, {=23 wait these are separate lines...

Hmm, `işlev secure_rastgele_doldur(` is one line, `    sonuc: Dizi<...>` another, `    gereksinim: tam32` another, `) -> tam32 {` another.

So: 18=V1 stub, 19=işlev secure..., 20=sonuc, 21=gereksinim, 22=)->tam32 {, 23=V1 PLACEHOLDER, 24=Bu kod, 25=değişken state, 26=değişken i, 27=iken i <, 28=state=xorshift, 29=değişken orta...(comment first line), 30=second comment line, 31=değişken orta, 32=değişken alt, 33=sonuc[i]=, 34=i=i+1.

So line 82+33 = 115. That matches! The last shown line `i = i + 1;` is line 115. So the cut truly ends at line 115, and the function continues past 115 (closing brace, ver, etc.).

So I cannot see the closing. My plan: edit replaces lines 82-115 exactly. The remaining lines 116+ contain `    }`, `    ver gereksinim;`, `}`.

But I need to add os_rastgele_kelime after the function. Since I don't know what's after line 115, I can't do a simple second edit there.

Wait — actually, maybe I can just do the second addition as part of a separate edit that targets the closing. But I don't see the closing.

Alternative approach: Are there known closing lines? The pseudocode says "ver gereksinim". So likely:
```
        i = i + 1;
    }
    ver gereksinim;
}
```

I'll make an edit that includes the `i = i + 1;` and append the new function after `}`. But I don't know the exact closing lines. Risky.

Hmm. Let me reconsider: maybe I should replace the whole visible portion AND include a fresh ending, leaving the original closing untouched — no.

Actually, the cleanest solution: do ONE edit that replaces lines 82-115 (the exact visible text), and the new text ends with `i = i + 1;` — same as original, so the following `}`, `ver gereksinim;`, `}` remain valid. This handles the body replacement correctly.

Then for the addition of os_rastgele_kelime: I need a separate edit but I don't see the anchor. 

Given the constraints, maybe the answer expects me to include the closing lines as part of ESKI (assuming standard). But the rule says ESKI must match exactly once. If I guess wrong on the closing, it fails.

Let me think about what's reasonable. The task author likely knows the full function. Since they gave the cut ending exactly at `i = i + 1;`, maybe the function ends differently and the "ver gereksinim" is expected.

Actually — wait. Maybe the body replacement is meant to replace lines up to `i = i + 1;` and the YENI text should include `ver gereksinim;`? No, because that would duplicate if original has it.

I think the safest interpretation given the exact cut: replace exactly the visible lines. For adding os_rastgele_kelime, I could include it in a second part of the same edit by extending ESKI to include the closing braces assuming they exist... but I shouldn't guess.

Let me reconsider the problem. Perhaps the whole point is: the visible cut IS the entire relevant editable region, and the closing `}` + `ver gereksinim;` + `}` exist right after. The task says "Gövdeyi ... DEĞİŞTİR" and pseudocode ends with `ver gereksinim`. So the closing `ver gereksinim; }` exist.

I'll construct the edit to include the closing as:
```
        i = i + 1;
    }
    ver gereksinim;
}
```

But do I know there's exactly this? The pseudocode says `ver gereksinim`. And typical KEMGU function closes with `}`.

Hmm, but if the actual last lines differ (e.g., `    ver i;` or something), my ESKI won't match.

Given the instruction "ESKİ = kesitteki satırlar birebir", the ESKI should be EXACTLY the cut lines. The cut ends at `i = i + 1;`. So ESKI = lines 82-115 including blank at 82? Actually the cut's first line is blank (line 82?). Hmm the cut shows a blank line then `// ===`.

Wait, the cut header says line 82-115. The first thing shown is a blank line. Let me assume line 82 is blank... no. Let me recount: the cut content:

```

// =============================================================================
// CSPRNG API — Bağımlı Platform (V1 placeholder)
```

There's a leading blank. So line 82 = blank? or line 82 = `// ===`? If line 82 is blank, then lines 82-115. Let me count with blank at 82:
82 blank, 83 ===, 84 CSPRNG, 85 ===, 86 //, 87 Bu, 88 yok, 89 API, 90 blank, 91 Doldur, 92 dönüş, 93 //, 94 Platform, 95 Linux, 96 Windows, 97 macOS, 98 //, 99 V1 stub, 100 işlev secure, 101 sonuc, 102 gereksinim, 103 )->tam32 {, 104 V1 placeholder, 105 Bu kod, 106 state, 107 i, 108 iken, 109 state=xorshift, 110 comment1, 111 comment2, 112 değişken orta, 113 değişken alt, 114 sonuc[i], 115 i=i+1.

Yes! So the cut is exactly lines 82-115. Line 82 is the blank line. So the "KESİT" starts with a blank line at line 82.

This means the ESKI text I should replace starts with the blank line 82. But then between line 81 and 82... whatever. The instruction shows the cut beginning with a blank line.

OK so ESKI = the exact cut content starting from blank line, through `i = i + 1;`.

Given that, I do a replacement where ESKI is the entire visible cut (including the leading blank and the section header comments) and YENI is the modified version.

But the section header comments (// CSPRNG API...) — should those be included? The task focuses on the function. But the cut includes them. If I include them in ESKI, my YENI must reproduce them.

Let me decide: I'll include the full cut as ESKI (lines 82-115) and produce YENI with modifications: remove V2/V1 stub comments as instructed, keep section header maybe, replace function body.

Actually the task says: "İşlevin üstündeki 'V1 PLACEHOLDER / KRİPTO GÜVENSİZ / V2' yorumlarını kaldır". So remove: "// V1: stub — xorshift64 fallback (KRİPTO GÜVENSİZ — sadece test)." and "// V1 PLACEHOLDER — gerçek getrandom syscall V2'de." and "// Bu kod KRİPTO GÜVENSİZ, sadece compile/test için.". Also the "// Platform implementasyonu (V2):" block? It says V2 yorumlarını. Hmm "V2" references. The Platform implementasyonu (V2) block mentions syscalls. I think removing them and replacing with the D-713 comment is fine.

But the section header "// CSPRNG API — Bağımlı Platform (V1 placeholder)" also references V1. Should I keep? "V1: xorshift64 fallback satırları ... KALSIN (test PRNG'si)" — but that's about file header lines, not this. Hmm.

This is getting complicated. Let me simplify by being minimal: replace only the function's body region, not the section headers. That means ESKI starts at "// Doldur" line and ends at `i = i + 1;`.

But the instruction says ESKI = cut lines exactly. However for a valid edit, I can choose a subrange as long as it appears exactly once. The rule "Var olan dosyada değişiklik — ESKI metin dosyada AYNEN ve TEK kez geçmeli" — the ESKI just needs to appear exactly once. It doesn't have to be the whole cut.

So I'll make two edits:
1. Replace the V2/V1 comment block + function body up through `i = i + 1;`, ending my YENI at `i = i + 1;` so the trailing `}`/`ver`/`}` remain.

Wait but then I can't add os_rastgele_kelime.

Alternative: Make one edit where ESKI is the function body from `// V1 st...` down to `i = i + 1;` — no.

Let me reconsider once more. The cleanest robust approach that ensures a valid result:

Edit 1: Replace from `// Doldur` comment through `i = i + 1;` (ends before the closing brace). YENI ends with `i = i + 1;`. This preserves the closing.

Edit 2: To add os_rastgele_kelime, I need to insert after the function's closing `}`. But I don't have it. 

UNLESS: I include the closing in edit 1's ESKI by guessing. Given the pseudocode explicitly says `ver gereksinim`, I'm fairly confident the closing is:
```
    }
    ver gereksinim;
}
```

Hmm, but the original might have had `ver gereksinim;` or `ver i;`. Since pseudocode says `ver gereksinim`, that's what the NEW should be. Original might be `ver i;` (common for a fill function returning count = i, which equals gereksinim if it completed). Actually `i` equals `gereksinim` after the loop, so either works. Risk.

Given uncertainty, I'll take a different approach: make the ESKI include the closing braces as they most likely are, but run a risk. Actually no.

Wait, re-reading: "İKİ `duzenle` bloğu üret (ESKİ = kesitteki satırlar birebir)." — ESKİ = cut lines EXACTLY. So the ESKI for edit 1 should be the cut's lines. But the cut includes the section headers and ends at `i = i + 1;`.

Maybe the intended edit 1: ESKI = whole cut (lines 82-115), YENI = modified whole cut. Then the closing lines 116+ remain. And edit 2 adds os_rastgele_kelime... but where?

Hmm, but then os_rastgele_kelime goes "hemen ALTINA secure_rastgele_doldur'un" — below the function. If I can't see the function end, maybe edit 2 modifies the closing. 

OK here's a thought: maybe the file only shows this much and the function actually closes with more lines that are shown in the "kesit"? No, cut ends at `i = i + 1;`.

Given all this, I'll go with the following pragmatic plan that maximizes correctness:

Edit 1: ESKI = from "// Doldur —" through "        i = i + 1;" (the visible function portion). YENI = new function portion ending at "        i = i + 1;". This leaves the trailing `}`, `ver ...`, `}` unchanged. Risk: if original had `ver gereksinim` vs `ver i` — not touched, stays same. Fine, but then it might be `ver i` which still equals gereksinim. OK acceptable.

Wait but the task says "ver gereksinim" in pseudocode and I'm not touching the return. Since the original probably returns gereksinim or i, both work. Acceptable. I won't worry.

Edit 2: Add os_rastgele_kelime after the function. I can't anchor. Hmm.

Could I add it BEFORE secure_rastgele_doldur instead? Task says "hemen ALTINA" (immediately below). 

Alternatively, I can include in edit 1's YENI a trailing `}` + the new function, and NOT have edit 1 touch the real closing. No, that duplicates.

OK let me just make a decision: I'll assume the original function closing is:
```
        i = i + 1;
    }
    ver gereksinim;
}
```
And I'll include it in ESKI, replacing with new function + os_rastgele_kelime. This is the version that fully satisfies the task. I'll note as assumption.

But the risk: if it's `ver i;` the edit fails to match. But `ver gereksinim` stated in pseudocode strongly suggests that's the current or intended. Actually the pseudocode is what the NEW body should do: "ver gereksinim". This might indicate the CURRENT function already has `ver gereksinim`. More likely yes.

Hmm, let me reconsider whether the current function even has `ver gereksinim`. Given `i` counts to gereksinim, returning `i` also valid. Ugh.

Let me look at the visible end: `i = i + 1;`. The next line likely `    }` (close while). Then `    ver ???;`. Then `}`.

I'll go with `ver gereksinim;` as assumption because pseudocode explicitly ends with `ver gereksinim`.

Actually, you know what, to minimize risk of mismatch, I'll structure the edit to replace ONLY through `i = i + 1;` and put the new function inside by... no.

Let me just commit: Edit 1 replaces from `// Doldur` through the function including closing, assuming `    }\n    ver gereksinim;\n}`.

Actually, safer: replace through `i = i + 1;` and do NOT include closing. Then append os_rastgele_kelime in the SAME YENI but as a comment? No.

I'll go with including the closing, assuming standard. Let me write it.

Actually, let me reconsider the whole thing one more time. The rule is strict: ESKI must appear EXACTLY once. If I include guessed lines that are wrong, the edit breaks. If I only include what's shown, the edit is safe but I can't append os_rastgele_kelime.

Compromise: Edit 1 replaces the visible body (safe). Edit 2 adds os_rastgele_kelime by replacing the `i = i + 1;`? No.

Hmm, what if I include the closing braces in ESKI but write them as part of edit — the risk is real.

Let me go with the full replacement including assumed closing. It's the only way to add the new function per task. I'll list the assumption.

Final decision on rastgele.kem edit:

ESKI (lines 82-115, must start exactly as cut, which starts with a blank line):
Actually if I start ESKI with a blank line, that blank line appears many times in the file → won't be unique match! So I must NOT include the leading blank. Start ESKI at `// ===...` or at `// Doldur`.

I'll start at the section header `// ====...` line to be unique, and include through the closing.

But wait, including `// ==...` headers makes the ESKI long. Let me instead start at `// Doldur`. That's unique.

Let me write ESKI from `// Doldur` through the function closing (assumed).

Actually to be safe and minimal-risk, let me only replace the function body, keeping the "// Doldur" comment line and everything above intact, and keeping the closing untouched. And for the new function, I'll... 

No. Final: I'll replace from `// V1: stub` line (the last V2-related comment before işlev) through the function closing. Wait, `// V1: stub` needs removal too. Let me include from `// Platform implementasyonu (V2):` block down.

Hmm, the task says remove V2 comments. The "// Platform implementasyonu (V2):" block is V2 comment. Remove it. And "// V1: stub" line. Since they're adjacent, I can replace from `// Platform implementasyonu (V2):` through closing.

But "// Doldur" and "// dönüş" and "//" (empty comment) lines above — keep? The task says replace body and remove V1/V2/KRIPTO comments. The "// Doldur" and "// dönüş" are fine to keep.

Actually the replacement comment `// [D-713] OS entropisi...` should go where? "İşlevin üstündeki ... yorumlarını kaldır, yerine: // [D-713] ...". So it replaces the comment above the function. The "// Doldur/dönüş" — keep or not? I'll keep them.

So ESKI:
```
// Platform implementasyonu (V2):
//   - Linux: syscall(SYS_getrandom, buf, len, 0)
//   - Windows: BCryptGenRandom(NULL, buf, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG)
//   - macOS: getentropy(buf, len)
//
// V1: stub — xorshift64 fallback (KRİPTO GÜVENSİZ — sadece test).
işlev secure_rastgele_doldur(
    sonuc: Dizi<sabitsüre<dtam8>>,
    gereksinim: tam32
) -> tam32 {
    // V1 PLACEHOLDER — gerçek getrandom syscall V2'de.
    // Bu kod KRİPTO GÜVENSİZ, sadece compile/test için.
    değişken state: dtam64 = 0x9E3779B97F4A7C15;  // Fibonacci-türevsel sabit
    değişken i: tam32 = 0;
    iken i < gereksinim {
        state = xorshift64_adim(state);
        // dtam64 -> dtam8: önce dtam32'ye daralt (izinli), sonra dtam8'e (izinli).
        // E004 sadece kw_kaynak >= 64 && kw_hedef < 32 vurgular; iki-adım kaçınır.
        değişken orta: dtam32 = (state olarak dtam32);
        değişken alt: dtam8 = (orta olarak dtam8);
        sonuc[i] = sabitsüre_olustur(alt);
        i = i + 1;
    }
    ver gereksinim;
}
```

YENI:
```
// [D-713] OS entropisi (Linux /dev/urandom, Windows BCryptGenRandom); başarısızlıkta PANİK — sessiz 0 YOK.
işlev secure_rastgele_doldur(
    sonuc: Dizi<sabitsüre<dtam8>>,
    gereksinim: tam32
) -> tam32 {
    değişken k: dtam64 = 0;
    değişken i: tam32 = 0;
    iken i < gereksinim {
        eğer i % 8 == 0 { k = os_rastgele(); }
        // dtam64 -> dtam8: önce dtam32'ye daralt (izinli), sonra dtam8'e (izinli).
        // E004 sadece kw_kaynak >= 64 && kw_hedef < 32 vurgular; iki-adım kaçınır.
        değişken orta: dtam32 = (k olarak dtam32);
        değişken alt: dtam8 = (orta olarak dtam8);
        sonuc[i] = sabitsüre_olustur(alt);
        k = k >> 8;
        i = i + 1;
    }
    ver gereksinim;
}

// [D-713] Gizli 64 bitlik rastgele kelime.
işlev os_rastgele_kelime() -> sabitsüre<dtam64> { ver sabitsüre_olustur(os_rastgele()); }
```

This assumes current closing is `    }\n    ver gereksinim;\n}`. Risk noted.

Hmm, but what if the current closes with `    }\n    ver i;\n}`? Then mismatch. Or maybe it has `ver state;`. I'll assume `ver gereksinim` per pseudocode.

Actually wait — pseudocode is the NEW behavior, and it says "ver gereksinim". If original was already `ver gereksinim`, great. I'll assume so.

Edit 2 for test file: add kt_os_rastgele before main, and modify main.

ESKI for main's addition:
```
işlev main() -> tam32 {
    değişken skor: tam32 = 0;
    eğer kt_kontrol() == 1 { skor = skor + 1; }
    eğer kt_chacha_qr() == 1 { skor = skor + 2; }
    eğer kt_sha256_abc() == 1 { skor = skor + 4; }
    eğer kt_sec_u64_ust_bit() == 1 { skor = skor + 8; }   // D-629
    eğer kt_sha256_cok_blok() == 1 { skor = skor + 16; }   // D-709
    eğer kt_hmac_rfc4231() == 1 { skor = skor + 32; }     // D-709
    eğer kt_blake3() == 1 { skor = skor + 64; }           // D-710
    ver skor;   // 127 = tam geçiş
}
```

YENI:
```
// [D-713] OS rastgele: 32 kelimenin OR'u TÜM bitleri, AND'i HİÇBİR biti taşımamalı
// (bir bitin 32 çekilişte sabit kalma olasılığı 2^-32). Sabit dönen kaynak ikisini de düşürür.
işlev kt_os_rastgele() -> tam32 {
    değişken veya_t: dtam64 = 0;
    değişken ve_t: dtam64 = 0;
    ve_t = ve_t - 1;                 // tüm bitler 1
    değişken hepsi: dtam64 = ve_t;
    değişken i: tam32 = 0;
    iken i < 32 {
        değişken x: dtam64 = os_rastgele();
        veya_t = veya_t | x;
        ve_t = ve_t & x;
        i = i + 1;
    }
    eğer veya_t != hepsi { ver 0; }
    eğer ve_t != 0 { ver 0; }
    ver 1;
}
işlev main() -> tam32 {
    değişken skor: tam32 = 0;
    eğer kt_kontrol() == 1 { skor = skor + 1; }
    eğer kt_chacha_qr() == 1 { skor = skor + 2; }
    eğer kt_sha256_abc() == 1 { skor = skor + 4; }
    eğer kt_sec_u64_ust_bit() == 1 { skor = skor + 8; }   // D-629
    eğer kt_sha256_cok_blok() == 1 { skor = skor + 16; }   // D-709
    eğer kt_hmac_rfc4231() == 1 { skor = skor + 32; }     // D-709
    eğer kt_blake3() == 1 { skor = skor + 64; }           // D-710
    eğer kt_os_rastgele() == 1 { skor = skor + 128; }    // D-713
    ver skor;   // 255 = tam geçiş
}
```

Good.

For rastgele.kem, let me reconsider the ESKI closing risk. I'll present my best guess and note the assumption.

Actually, let me reconsider once more: maybe the cut ends at `i = i + 1;` because that's line 115, and line 116 is `    }`, 117 `    ver gereksinim;`, 118 `}`. I'll trust the standard.

Wait, but the current function might return `i` not `gereksinim`. Let me think about what's more natural. The existing body:
```
değişken i: tam32 = 0;
iken i < gereksinim { ...; i = i + 1; }
```
After loop, i == gereksinim. Return could be `ver i;` or `ver gereksinim;`. The doc says "dönüş: dolan byte sayısı". Both equal. 

Pseudocode explicitly: "ver gereksinim". This is the NEW requested. So current might be `ver i;`! Uh oh.

Hmm. If current is `ver i;`, my ESKI with `ver gereksinim` fails.

Given the ambiguity, maybe I should NOT include the closing in ESKI. Let me do a two-part approach:

Part A (edit): Replace from `// V1: stub...` through `        i = i + 1;` (last visible). YENI ends at `        i = i + 1;`. This leaves `    }` and `ver ...` and `}` untouched — whatever they are, valid.

Part B (edit): Add os_rastgele_kelime. But can't anchor.

Hmm. What if I add os_rastgele_kelime by replacing the `        i = i + 1;` at the END with `        i = i + 1;\n    }\n    ver gereksinim;\n}\n\n// [D-713] Gizli...`? But then I'd need to know what follows to not duplicate. No.

New idea: Combine into one edit that replaces through `i = i + 1;` and YENI ends with `i = i + 1;\n    }\n    ver gereksinim;\n}\n\n// [D-713]...`. But the original still has `}\n ver...\n}` after line 115 → duplication! Bad.

OK. I think the only clean way is to include the closing in ESKI. I'll assume `    }\n    ver gereksinim;\n}`. Given pseudocode, and that this is the most standard, I'll go with it.

Hmm, but actually, let me reconsider: the task author wrote "İKİ duzenle bloğu üret (ESKİ = kesitteki satırlar birebir)". This strongly implies ESKI = the exact cut lines. The cut ends at `i = i + 1;`. So ESKI should END at `i = i + 1;`, NOT include the closing! Because "birebir" = exactly the cut.

If that's the case, then edit 1's ESKI = cut lines (ending at `i = i + 1;`), and YENI must end at `i = i + 1;` too (to preserve the trailing closing). Then how to add os_rastgele_kelime? It must go somewhere else... "hemen ALTINA" — but I can't see the bottom.

Unless: the whole cut IS the whole function region, and lines 116+ are `}` and `ver` and `}` which I'm supposed to leave. Then adding os_rastgele_kelime "hemen altına" would be impossible without seeing them. So maybe the task expects me to include them anyway, meaning "birebir" is approximate.

Given the strong hint that ESKI = cut lines ending at `i = i + 1;`, and there are TWO edits (one per file), maybe the os_rastgele_kelime addition is expected as a SECOND edit within rastgele.kem? No, "İKİ duzenle bloğu" = two edit blocks total: one for rastgele.kem, one for test file.

So within the ONE rastgele.kem edit, I must both replace the body AND add os_rastgele_kelime. That means ESKI must extend to include the closing braces so I can append. Therefore "birebir" for the cut region is satisfied, plus I extend slightly to the function end.

So ESKI = cut lines + closing braces. YENI = new body + closing + new function.

I'll go with assuming closing = `    }\n    ver gereksinim;\n}`.

Let me also double check: is there really `    }\n    ver gereksinim;\n}`? The while loop needs one `}`, function needs one `}`. Between them `ver`. Yes.

OK, finalizing. But the leading blank line issue: I'll start ESKI at `// Platform implementasyonu (V2):` (unique line). Good, no leading blank.

Let me now also reconsider: do I keep "// Doldur", "// dönüş", "//" lines that come before "// Platform"? Yes, they're before my ESKI start, untouched. But wait, "// Doldur" etc. are between line ~91-93 and "// Platform" at 94. Untouched is fine.

But the task said remove V1/V2 comments above function. "// Doldur" and "// dönüş" aren't V1/V2, keep. "// Platform (V2)" removed, "// V1 stub" removed. Good.

Now write final blocks.

Let me verify the `%` operator in KEMGU: `i % 8 == 0`. The pseudocode uses `i % 8 == 0`. Note the heap file warned about `%` requiring @kdl_panik for variable divisor. But 8 is a constant ⇒ compiler can optimize to AND, no panic. Since it's `i % 8` with constant 8, fine. Good, pseudocode uses it, so it's allowed.

Also `k = k >> 8` — right shift. Fine.

Let me write.
