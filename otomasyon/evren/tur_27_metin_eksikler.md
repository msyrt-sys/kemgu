# Tur 27 — metin_eksikler — `deepseek-v4.1-flash`

- Tarih: 2026-10-25 05:25:20 · süre 81.7 sn · finish `stop` · reasoning_effort `low`
- usage: `{"prompt_tokens": 13195, "total_tokens": 25018, "completion_tokens": 11823, "prompt_tokens_details": {"cached_tokens": 0, "created_cache_tokens": 13184}, "completion_tokens_details": {"reasoning_tokens": 8283}}`
- Değişiklik blokları: 2 — duzenle:stdlib/metin.kem, duzenle:test/stdlib/test_metin.kem
- İstem: [tur_27_metin_eksikler_istem.md](tur_27_metin_eksikler_istem.md)

## Yanıt

```duzenle:stdlib/metin.kem
<<<<<<< ESKI
            eğer b1 < 0 { b1 = b1 + 256; }
            eğer değil tr_harf_2byte(u, b1) { ver yanlış; }
            i = i + 2;
        } değilse {
            ver yanlış;
        }
    }
    ver doğru;
}
=======
            eğer b1 < 0 { b1 = b1 + 256; }
            eğer değil tr_harf_2byte(u, b1) { ver yanlış; }
            i = i + 2;
        } değilse {
            ver yanlış;
        }
    }
    ver doğru;
}

// [D-731] son_indeks — `alt`ın SON geçişinin bayt indeksi; boş alt -> uzunluk; yoksa hiç.
işlev son_indeks(s: metin, alt: metin) -> seçimlik<tam32> {
    değişken M: tam32 = metin_uzunluk(alt);
    eğer M == 0 { ver değer(metin_uzunluk(s)); }
    değişken L: tam32 = metin_uzunluk(s);
    eğer M > L { ver hiç; }
    değişken i: tam32 = L - M;
    iken i >= 0 {
        eğer metin_esit(metin_kes(s, i, M), alt) { ver değer(i); }
        i = i - 1;
    }
    ver hiç;
}

// [D-731] bosluk_bayt_mi — bayt boşluk mu (boşluk, sekme, satır sonu, CR).
işlev bosluk_bayt_mi(u: tam32) -> mantıksal {
    ver u == 32 veya u == 9 veya u == 10 veya u == 13;
}

// [D-731] kirp_sol — baştaki boşluk baytlarını at.
işlev kirp_sol(s: metin) -> metin {
    değişken L: tam32 = metin_uzunluk(s);
    değişken i: tam32 = 0;
    iken i < L {
        değişken u: tam32 = metin_bayt(s, i) olarak tam32;
        eğer u < 0 { u = u + 256; }
        eğer değil bosluk_bayt_mi(u) { ver metin_kes(s, i, L - i); }
        i = i + 1;
    }
    ver metin_kes(s, L, 0);
}

// [D-731] kirp_sag — sondaki boşluk baytlarını at.
işlev kirp_sag(s: metin) -> metin {
    değişken L: tam32 = metin_uzunluk(s);
    değişken son: tam32 = L;
    iken son > 0 {
        değişken u: tam32 = metin_bayt(s, son - 1) olarak tam32;
        eğer u < 0 { u = u + 256; }
        eğer değil bosluk_bayt_mi(u) { ver metin_kes(s, 0, son); }
        son = son - 1;
    }
    ver metin_kes(s, 0, 0);
}

// [D-731] sozluk_karsilastir — İŞARETSİZ bayt bayt karşılaştırma.
işlev sozluk_karsilastir(a: metin, b: metin) -> tam32 {
    değişken La: tam32 = metin_uzunluk(a);
    değişken Lb: tam32 = metin_uzunluk(b);
    değişken n: tam32 = La;
    eğer Lb < n { n = Lb; }
    değişken i: tam32 = 0;
    iken i < n {
        değişken ua: tam32 = metin_bayt(a, i) olarak tam32;
        eğer ua < 0 { ua = ua + 256; }
        değişken ub: tam32 = metin_bayt(b, i) olarak tam32;
        eğer ub < 0 { ub = ub + 256; }
        eğer ua < ub { ver 0 - 1; }
        eğer ua > ub { ver 1; }
        i = i + 1;
    }
    eğer La < Lb { ver 0 - 1; }
    eğer La > Lb { ver 1; }
    ver 0;
}

// [D-731] kod_noktalari — UTF-8 çöz, kod noktalarını diziye ekle.
işlev kod_noktalari(s: metin) -> Dizi<tam32> {
    değişken d: Dizi<tam32> = dizi_olustur();
    değişken L: tam32 = metin_uzunluk(s);
    değişken i: tam32 = 0;
    iken i < L {
        değişken u: tam32 = metin_bayt(s, i) olarak tam32;
        eğer u < 0 { u = u + 256; }
        eğer u < 128 {
            dizi_ekle(d, u);
            i = i + 1;
        } değilse eğer u >= 192 ve u <= 223 {
            eğer i + 1 < L {
                değişken b1: tam32 = metin_bayt(s, i + 1) olarak tam32;
                eğer b1 < 0 { b1 = b1 + 256; }
                eğer b1 >= 128 ve b1 <= 191 {
                    dizi_ekle(d, (u - 192) * 64 + (b1 - 128));
                    i = i + 2;
                } değilse {
                    dizi_ekle(d, 65533);
                    i = i + 1;
                }
            } değilse {
                dizi_ekle(d, 65533);
                i = i + 1;
            }
        } değilse eğer u >= 224 ve u <= 239 {
            değişken ok: mantıksal = yanlış;
            eğer i + 2 < L {
                değişken b1: tam32 = metin_bayt(s, i + 1) olarak tam32;
                eğer b1 < 0 { b1 = b1 + 256; }
                değişken b2: tam32 = metin_bayt(s, i + 2) olarak tam32;
                eğer b2 < 0 { b2 = b2 + 256; }
                eğer b1 >= 128 ve b1 <= 191 ve b2 >= 128 ve b2 <= 191 {
                    dizi_ekle(d, (u - 224) * 4096 + (b1 - 128) * 64 + (b2 - 128));
                    i = i + 3;
                    ok = doğru;
                }
            }
            eğer değil ok {
                dizi_ekle(d, 65533);
                i = i + 1;
            }
        } değilse eğer u >= 240 ve u <= 247 {
            değişken ok: mantıksal = yanlış;
            eğer i + 3 < L {
                değişken b1: tam32 = metin_bayt(s, i + 1) olarak tam32;
                eğer b1 < 0 { b1 = b1 + 256; }
                değişken b2: tam32 = metin_bayt(s, i + 2) olarak tam32;
                eğer b2 < 0 { b2 = b2 + 256; }
                değişken b3: tam32 = metin_bayt(s, i + 3) olarak tam32;
                eğer b3 < 0 { b3 = b3 + 256; }
                eğer b1 >= 128 ve b1 <= 191 ve b2 >= 128 ve b2 <= 191 ve b3 >= 128 ve b3 <= 191 {
                    dizi_ekle(d, (u - 240) * 262144 + (b1 - 128) * 4096 + (b2 - 128) * 64 + (b3 - 128));
                    i = i + 4;
                    ok = doğru;
                }
            }
            eğer değil ok {
                dizi_ekle(d, 65533);
                i = i + 1;
            }
        } değilse {
            dizi_ekle(d, 65533);
            i = i + 1;
        }
    }
    ver d;
}
>>>>>>> YENI
```

```duzenle:test/stdlib/test_metin.kem
<<<<<<< ESKI
işlev main() -> tam32 {
    değişken h: tam32 = 0;
=======
işlev test_son_indeks_var() -> mantıksal {
    eşleş son_indeks("abcabc", "bc") {
        değer(v) => { ver v == 4; }
        hiç => { ver yanlış; }
    }
    ver yanlış;
}

işlev test_son_indeks_yok() -> mantıksal {
    eşleş son_indeks("abc", "x") {
        değer(v) => { ver yanlış; }
        hiç => { ver doğru; }
    }
    ver yanlış;
}

işlev test_son_indeks_bos() -> mantıksal {
    eşleş son_indeks("abc", "") {
        değer(v) => { ver v == 3; }
        hiç => { ver yanlış; }
    }
    ver yanlış;
}

işlev test_kirp_sol() -> mantıksal {
    değişken s: metin = metin_birlestir(metin_birlestir("  ", kod_metin(9)), "ab ");
    ver metin_esit(kirp_sol(s), "ab ");
}

işlev test_kirp_sag() -> mantıksal {
    değişken s: metin = metin_birlestir(" ab ", kod_metin(10));
    ver metin_esit(kirp_sag(s), " ab");
}

işlev test_sozluk() -> mantıksal {
    eğer sozluk_karsilastir("abc", "abd") != 0 - 1 { ver yanlış; }
    eğer sozluk_karsilastir("b", "a") != 1 { ver yanlış; }
    eğer sozluk_karsilastir("ab", "ab") != 0 { ver yanlış; }
    eğer sozluk_karsilastir("ab", "abc") != 0 - 1 { ver yanlış; }
    eğer sozluk_karsilastir("ç", "z") != 1 { ver yanlış; }
    ver doğru;
}

işlev test_kod_noktalari() -> mantıksal {
    değişken d: Dizi<tam32> = kod_noktalari("aç€😀");
    eğer dizi_boyut(d) != 4 { ver yanlış; }
    eğer d[0] != 97 { ver yanlış; }
    eğer d[1] != 231 { ver yanlış; }
    eğer d[2] != 8364 { ver yanlış; }
    eğer d[3] != 128512 { ver yanlış; }
    ver doğru;
}

işlev main() -> tam32 {
    değişken h: tam32 = 0;
    eğer değil test_son_indeks_var() { yazdir_metin("BASARISIZ (yanlış döndü): test_son_indeks_var"); h = h + 1; }
    eğer değil test_son_indeks_yok() { yazdir_metin("BASARISIZ (yanlış döndü): test_son_indeks_yok"); h = h + 1; }
    eğer değil test_son_indeks_bos() { yazdir_metin("BASARISIZ (yanlış döndü): test_son_indeks_bos"); h = h + 1; }
    eğer değil test_kirp_sol() { yazdir_metin("BASARISIZ (yanlış döndü): test_kirp_sol"); h = h + 1; }
    eğer değil test_kirp_sag() { yazdir_metin("BASARISIZ (yanlış döndü): test_kirp_sag"); h = h + 1; }
    eğer değil test_sozluk() { yazdir_metin("BASARISIZ (yanlış döndü): test_sozluk"); h = h + 1; }
    eğer değil test_kod_noktalari() { yazdir_metin("BASARISIZ (yanlış döndü): test_kod_noktalari"); h = h + 1; }
>>>>>>> YENI
```

İki blok: `stdlib/metin.kem` sonuna D-731 işlevleri eklendi; `test/stdlib/test_metin.kem` içine yeni `test_*` işlevleri ve `main` içi çağrı satırları eklendi (main gövdesindeki `eğer h == 0 {` satırının üstüne denk gelecek şekilde `değişken h` sonrasına yerleştirildi).

VARSAYIMLAR
- `Dizi<tam32>` tipi ve boş dizi üretimi için `dizi_olustur()` yerleşiğinin var olduğu varsayıldı (bağlamda yalnız `dizi_ekle`/`dizi_boyut`/`d[i]` listelendiği için).
- `} değilse eğer <koşul> {` zincirleme sözdiziminin KEMGU'da geçerli olduğu varsayıldı.
- `sozluk_karsilastir` karşılaştırmalarında test beklentisi olan `-1` değerinin `0 - 1` ile ifade edildiği varsayıldı (mevcut testlerdeki `0 - 1` deseniyle uyumlu).

## Düşünme (reasoning)

We need to produce two duzenle blocks. Let me figure out the exact ESKI text from the cuts.

Block 1: stdlib/metin.kem — end of file. The cut at lines 462-470 shows end of some function `ver doğru; }`. But the task says append to END of file. The cut shown is that ending. Hmm, but we need ESKI to match exactly once. The last cut of stdlib/metin.kem is:
```
            eğer b1 < 0 { b1 = b1 + 256; }
            eğer değil tr_harf_2byte(u, b1) { ver yanlış; }
            i = i + 2;
        } değilse {
            ver yanlış;
        }
    }
    ver doğru;
}
```
This is presumably the end of file. We append functions after it. But is this unique? Probably. We'll use that as ESKI and add new functions after.

Block 2: test/stdlib/test_metin.kem — add test functions above `işlev main() -> tam32 {` and add main lines above `    eğer h == 0 {`.

But we can only produce two duzenle blocks total (task says İKİ duzenle bloğu). So we need to combine the test additions into one block? Wait: we need to modify test file in two places (add functions and add main lines). But task says two blocks total. Hmm.

Wait, re-read: "İKİ ```duzenle bloğu üret (ESKİ = kesitteki satırlar BİREBİR). Başka dosya YOK." Two blocks. One for stdlib/metin.kem (append), one for test file. But test needs edits in two locations... Unless we can make one block spanning from before main through the h==0 line. The cut at lines 462-467 includes `işlev main() -> tam32 {` and the `eğer değil test_esit_evet()` line, and cut at 521-525 includes the `eğer h == 0 {` region. These are far apart (line 467 to 521), so a single block would include ~54 lines we don't have full content of.

Hmm. Alternatively maybe we combine: ESKI = the main function opening (lines 462-467 cut) ... but we need to insert both test functions (above main) and main lines (above h==0). That's two separate spots.

Wait, maybe we can put the test functions above main, and also the main "eğer değil test_X" lines... Actually the main lines need to go above `eğer h == 0 {`. So two edits in same file. But task explicitly says two blocks total, one per file? Let me re-read: "İKİ ```duzenle bloğu üret". Maybe means produce exactly two edit blocks: one for stdlib/metin.kem and... no, test has two changes.

Hmm, but maybe we're allowed more blocks? "İKİ duzenle bloğu" is strict. But impossible with two spots unless we use one block that covers both spots. But we lack the middle content.

Let me reconsider: perhaps the two cuts in test file are meant to be handled with TWO blocks, and stdlib with one block = three total. But the instruction says two. Wait "İKİ ```duzenle bloğu üret" — maybe they count stdlib append as one block and test file as... no.

Actually, re-reading: cut at 462-467 includes `işlev main() -> tam32 {` line. Cut at 521-525 includes `eğer h == 0 {` line. If we provide a block with ESKI = the main opening region and YENI = test functions + main opening, that adds test functions above main. But the main "eğer değil test_X" lines still need insertion.

Could we insert the main lines NOT above h==0 but somewhere else? The requirement: "main içinde eğer h == 0 { satırının ÜSTÜNE her biri için". So must be there.

I think the realistic answer: provide 3 duzenle blocks. But instruction says 2. Hmm. Maybe the orchestrator intends: one block for stdlib/metin.kem, and for test file we provide a single block covering from the `işlev main` region... but we could include the test functions and main lines in one block if we make ESKI span from `işlev main() -> tam32 {` through `eğer h == 0 {`. The middle content is unknown. Unless... we don't need it. Actually the cut at 462-467 and 521-525 are separate excerpts, we don't have lines 468-520.

Given constraints, I'll produce a single block for stdlib and a single block for the test file's first spot (test functions above main), and... no wait, we need main lines too.

Alternative: maybe we can put main line additions in the same block as the test function additions by using ESKI that ends right before `eğer h == 0`? No.

Let me reconsider the "İKİ" count. Perhaps they mean two duzenle blocks for the test file (one for functions, one for main lines) AND the stdlib append counts as... no, "İKİ" = two.

Hmm. Maybe the intended interpretation: one block for stdlib/metin.kem (append), one block for test file where ESKI is the main function opening through... Actually could we place the "eğer değil test_X" lines right after `işlev main() -> tam32 {`? No, requirement says above h==0.

Wait — maybe we can add the main lines at the END of the main function? No.

Let me look again at cut 462-467:
```
işlev main() -> tam32 {
    değişken h: tam32 = 0;
    eğer değil test_esit_evet() { yazdir_metin("BASARISIZ (yanlış döndü): test_esit_evet"); h = h + 1; }
```
Cut 521-525:
```
    eğer değil test_sadece_rakam_tek() { ... }
    eğer değil test_sadece_harf_tek_turkce() { ... }
    eğer değil test_kes_arasi_bos_icerik() { ... }
    eğer h == 0 { yazdir_metin("metin: TUM MANTIKSAL TESTLER GECTI"); ver 0; }
    // ⚠ ...
```

So we have the region: main starts, then test_esit_evet line, then ... many lines ... then some final test lines, then h==0. We can insert our new main lines right after `değişken h: tam32 = 0;`? But requirement says above h==0. Actually above h==0 is anywhere before it in main. We could insert immediately after `değişken h: tam32 = 0;` — that's technically "inside main, above h==0". The requirement wording "eğer h == 0 { satırının ÜSTÜNE" means above that line, which is satisfied if we put them earlier (still above). Hmm but semantically fine.

So one block for test file: ESKI = 
```
işlev main() -> tam32 {
    değişken h: tam32 = 0;
```
Wait but need to place test functions above `işlev main`. If ESKI is just the main opening, YENI = test functions + main opening + main lines? But we can't add lines inside main if we're replacing the opening and adding after `değişken h`? Yes we can:

YENI:
```
// test functions...

işlev main() -> tam32 {
    değişken h: tam32 = 0;
    eğer değil test_...() { ... }
    ...
```
This puts the new main lines right after h declaration, which is above h==0. 

So block: ESKI = the two lines `işlev main() -> tam32 {` + `    değişken h: tam32 = 0;`? But we need ESKI to be from cut exactly. The cut shows:
```
işlev main() -> tam32 {
    değişken h: tam32 = 0;
    eğer değil test_esit_evet() ...
```
If we use ESKI = `işlev main() -> tam32 {\n    değişken h: tam32 = 0;`, it's a subset and appears once. Good.

But wait — inserting test functions above main and main lines inside, both in one block. ESKI starts at `işlev main` — but the test functions need to go above that. We can prepend them before `işlev main` in YENI. That's fine: ESKI is `işlev main() -> tam32 {\n    değişken h: tam32 = 0;`, YENI = testfunctions + same + new main lines.

Yes! That works with one block for test file.

So two blocks total: one stdlib append, one test file.

Now write the stdlib functions.

son_indeks:
```
// [D-731] son_indeks — `alt`ın SON geçişinin bayt indeksi; boş alt -> uzunluk; yoksa hiç.
işlev son_indeks(s: metin, alt: metin) -> seçimlik<tam32> {
    değişken M: tam32 = metin_uzunluk(alt);
    eğer M == 0 { ver değer(metin_uzunluk(s)); }
    değişken L: tam32 = metin_uzunluk(s);
    eğer M > L { ver hiç; }
    değişken i: tam32 = L - M;
    iken i >= 0 {
        eğer metin_esit(metin_kes(s, i, M), alt) { ver değer(i); }
        i = i - 1;
    }
    ver hiç;
}
```

bosluk_bayt_mi:
```
// [D-731] bosluk_bayt_mi — bayt boşluk mu (boşluk, sekme, satır sonu, CR).
işlev bosluk_bayt_mi(u: tam32) -> mantıksal {
    ver u == 32 veya u == 9 veya u == 10 veya u == 13;
}
```
Need `veya` keyword. Okay.

kirp_sol:
```
// [D-731] kirp_sol — baştaki boşluk baytlarını at.
işlev kirp_sol(s: metin) -> metin {
    değişken L: tam32 = metin_uzunluk(s);
    değişken i: tam32 = 0;
    iken i < L {
        değişken u: tam32 = metin_bayt(s, i) olarak tam32;
        eğer u < 0 { u = u + 256; }
        eğer değil bosluk_bayt_mi(u) { ver metin_kes(s, i, L - i); }
        i = i + 1;
    }
    ver metin_kes(s, L, 0);
}
```
Hmm, ver metin_kes(s, L, 0) gives empty. Could return "". Fine.

kirp_sag:
```
// [D-731] kirp_sag — sondaki boşluk baytlarını at.
işlev kirp_sag(s: metin) -> metin {
    değişken L: tam32 = metin_uzunluk(s);
    değişken son: tam32 = L;
    iken son > 0 {
        değişken u: tam32 = metin_bayt(s, son - 1) olarak tam32;
        eğer u < 0 { u = u + 256; }
        eğer değil bosluk_bayt_mi(u) { ver metin_kes(s, 0, son); }
        son = son - 1;
    }
    ver metin_kes(s, 0, 0);
}
```

sozluk_karsilastir:
```
// [D-731] sozluk_karsilastir — İŞARETSİZ bayt bayt karşılaştırma.
işlev sozluk_karsilastir(a: metin, b: metin) -> tam32 {
    değişken La: tam32 = metin_uzunluk(a);
    değişken Lb: tam32 = metin_uzunluk(b);
    değişken n: tam32 = La;
    eğer Lb < n { n = Lb; }
    değişken i: tam32 = 0;
    iken i < n {
        değişken ua: tam32 = metin_bayt(a, i) olarak tam32;
        eğer ua < 0 { ua = ua + 256; }
        değişken ub: tam32 = metin_bayt(b, i) olarak tam32;
        eğer ub < 0 { ub = ub + 256; }
        eğer ua < ub { ver 0 - 1; }
        eğer ua > ub { ver 1; }
        i = i + 1;
    }
    eğer La < Lb { ver 0 - 1; }
    eğer La > Lb { ver 1; }
    ver 0;
}
```

kod_noktalari — returns Dizi<tam32>. Need to create a dizi. How does stdlib create dizi? Builtin `dizi_ekle(d, v)` implies d created somehow. Maybe `Dizi<tam32>` type, and create via something. Hmm. Task says usable builtins: `dizi_ekle(d, v)`, `dizi_boyut(d)`, `d[i]`. It doesn't mention dizi create builtin. Maybe there's a `dizi_olustur`? The task didn't list it. Hmm.

Look at test: `d = kod_noktalari("aç€😀"); dizi_boyut(d) == 4`. So kod_noktalari returns a Dizi. How to construct? Perhaps `boş_dizi<tam32>()`? Unknown.

The task says "Kullanılabilir yerleşikler: ... `dizi_ekle(d, v)`, `dizi_boyut(d)`, `d[i]`." Maybe dizi creation is via declaring a variable and using dizi_ekle? Like:
```
değişken d: Dizi<tam32> = ...?
```
We need to guess. Maybe there's a builtin `dizi_olustur()`. Since not listed, maybe we can create an empty Dizi via `Dizi<tam32>()`? I'm not sure.

Could look at stdlib/metin.kem for patterns — but we only have cuts. Hmm.

Maybe the intended KEMGU pattern: `değişken d: Dizi<tam32> = dizi_olustur();` But not listed as available. Risky.

Alternatively, maybe there's a `dizi_ekle` that works on a variable declared as `Dizi<tam32>`. Could be initialized with empty literal? I don't know.

Given uncertainty, I should make an assumption and list in VARSAYIMLAR. Let me use `dizi_olustur()` — common name. But the task said "Başka dosya YOK. Saf KEMGU." and listed available builtins. Maybe the correct creation is `boş Dizi<tam32>`? Hmm.

Actually, in many KEMGU-like languages, you'd do `değişken d: Dizi<tam32> = dizi();` Unknown.

Let me think about the `seçimlik<T>` usage: `ver değer(x);` shows it's a builtin constructor-like. Maybe Dizi also has such: `Dizi<tam32>()` constructor? Possibly `boş_dizi<tam32>()`.

I'll go with `dizi_olustur()` and note assumption. Actually, let me reconsider — maybe the test file uses dizi from elsewhere. The test just calls kod_noktalari and uses dizi_boyut and d[]. So internally we need to create.

Given risk, I'll use `Dizi<tam32>` declared and initialized via a builtin. Let me search memory: KEMGU might have `dizi_bos()`? No.

I'll pick `dizi_olustur()` with assumption note. But wait, the available list explicitly includes dizi_ekle, dizi_boyut, d[]. If creation builtin existed they might list it. Maybe the idiom is `değişken d: Dizi<tam32> = boş;`? no.

Hmm, maybe you can just declare `değişken d: Dizi<tam32> = Dizi<tam32>();`. Hmm.

Let me consider using `dizi_ekle` on an uninitialized but declared variable. In some languages, `değişken d: Dizi<tam32>;` defaults to empty. That might be it! Since it's a stdlib type. Let me use:
```
değişken d: Dizi<tam32>;
```
Hmm but uninitialized variable might be error.

I think safest given listed builtins: there might be an implicit empty Dizi. I'll declare `değişken d: Dizi<tam32> = dizi_olustur();` and note assumption. Actually, let me reconsider the phrase "Kullanılabilir yerleşikler" — the list is what's needed. Maybe `dizi_ekle` takes the variable and creates? No.

I'll go with assumption. Let me write kod_noktalari:

```
// [D-731] kod_noktalari — UTF-8 çöz, kod noktalarını diziye ekle.
işlev kod_noktalari(s: metin) -> Dizi<tam32> {
    değişken d: Dizi<tam32> = dizi_olustur();
    değişken L: tam32 = metin_uzunluk(s);
    değişken i: tam32 = 0;
    iken i < L {
        değişken u: tam32 = metin_bayt(s, i) olarak tam32;
        eğer u < 0 { u = u + 256; }
        eğer u < 128 {
            dizi_ekle(d, u);
            i = i + 1;
        } değilse {
            eğer u >= 192 ve u <= 223 {
                eğer i + 1 < L {
                    değişken b1: tam32 = metin_bayt(s, i + 1) olarak tam32;
                    eğer b1 < 0 { b1 = b1 + 256; }
                    eğer b1 >= 128 ve b1 <= 191 {
                        dizi_ekle(d, (u - 192) * 64 + (b1 - 128));
                        i = i + 2;
                    } değilse {
                        dizi_ekle(d, 65533);
                        i = i + 1;
                    }
                } değilse {
                    dizi_ekle(d, 65533);
                    i = i + 1;
                }
            } değilse {
                eğer u >= 224 ve u <= 239 {
                    eğer i + 2 < L {
                        değişken b1: tam32 = ...;
                        değişken b2: ...;
                        if both cont: add; i+=3 else add 65533 i+=1
                    } ...
```

This gets long. Need to handle 4-byte too. Let me write a helper? Task only lists these functions to add; can add helper? It says add functions listed. I could add a helper but better inline.

Given complexity, maybe structure with nested conditions. Let me write cleanly. Use a common pattern: for each case, check length and continuation bytes.

Let me write:

```
işlev kod_noktalari(s: metin) -> Dizi<tam32> {
    değişken d: Dizi<tam32> = dizi_olustur();
    değişken L: tam32 = metin_uzunluk(s);
    değişken i: tam32 = 0;
    iken i < L {
        değişken u: tam32 = metin_bayt(s, i) olarak tam32;
        eğer u < 0 { u = u + 256; }
        eğer u < 128 {
            dizi_ekle(d, u);
            i = i + 1;
        } değilse eğer u >= 192 ve u <= 223 {
            eğer i + 1 < L {
                değişken b1: tam32 = metin_bayt(s, i + 1) olarak tam32;
                eğer b1 < 0 { b1 = b1 + 256; }
                eğer b1 >= 128 ve b1 <= 191 {
                    dizi_ekle(d, (u - 192) * 64 + (b1 - 128));
                    i = i + 2;
                } değilse {
                    dizi_ekle(d, 65533);
                    i = i + 1;
                }
            } değilse {
                dizi_ekle(d, 65533);
                i = i + 1;
            }
        } değilse eğer u >= 224 ve u <= 239 {
            değişken ok: mantıksal = yanlış;
            eğer i + 2 < L {
                değişken b1: tam32 = metin_bayt(s, i + 1) olarak tam32;
                eğer b1 < 0 { b1 = b1 + 256; }
                değişken b2: tam32 = metin_bayt(s, i + 2) olarak tam32;
                eğer b2 < 0 { b2 = b2 + 256; }
                eğer b1 >= 128 ve b1 <= 191 ve b2 >= 128 ve b2 <= 191 {
                    dizi_ekle(d, (u - 224) * 4096 + (b1 - 128) * 64 + (b2 - 128));
                    i = i + 3;
                    ok = doğru;
                }
            }
            eğer değil ok {
                dizi_ekle(d, 65533);
                i = i + 1;
            }
        } değilse eğer u >= 240 ve u <= 247 {
            değişken ok: mantıksal = yanlış;
            eğer i + 3 < L {
                değişken b1: tam32 = metin_bayt(s, i + 1) olarak tam32;
                eğer b1 < 0 { b1 = b1 + 256; }
                değişken b2: tam32 = metin_bayt(s, i + 2) olarak tam32;
                eğer b2 < 0 { b2 = b2 + 256; }
                değişken b3: tam32 = metin_bayt(s, i + 3) olarak tam32;
                eğer b3 < 0 { b3 = b3 + 256; }
                eğer b1 >= 128 ve b1 <= 191 ve b2 >= 128 ve b2 <= 191 ve b3 >= 128 ve b3 <= 191 {
                    dizi_ekle(d, (u - 240) * 262144 + (b1 - 128) * 4096 + (b2 - 128) * 64 + (b3 - 128));
                    i = i + 4;
                    ok = doğru;
                }
            }
            eğer değil ok {
                dizi_ekle(d, 65533);
                i = i + 1;
            }
        } değilse {
            dizi_ekle(d, 65533);
            i = i + 1;
        }
    }
    ver d;
}
```

But `değişken ok` declared inside a branch block — shadowing `ok` in different branches is fine since separate scopes. But in KEMGU, declaring inside `eğer` block might be scoped to block. Should be okay. But careful: `değişken ok` in two sibling blocks fine.

However, `değişken b1` inside nested if in each branch — separate scopes, fine.

One concern: `değilse eğer` — is that syntax valid? KEMGU uses `} değilse {`. For chained else-if, maybe `} değilse eğer ... {`. Likely fine. To be safe, could use nested `değilse { eğer ... }`. But that adds indentation. I'll use `değilse eğer` as it's common. Hmm, but risk. Let me instead use nested to be safer? Actually KEMGU looks Rust-like; `değilse eğer` probably supported. I'll use it.

Wait, the task's existing code uses `} değilse {`. I'll use `} değilse eğer ... {`. Should be fine.

Now the return type `Dizi<tam32>` — is that the type syntax? Task uses `-> seçimlik<tam32>`. So `Dizi<tam32>` likely correct.

Creation: `dizi_olustur()` — assumption. Let me note.

Actually, maybe better: since test uses `d = kod_noktalari(...)`, and dizi_boyut(d) works, the return type is some dizi type. Maybe the KEMGU builtin for creating is `dizi_olustur()`. I'll assume.

Now the test block. ESKI:
```
işlev main() -> tam32 {
    değişken h: tam32 = 0;
```
YENI = test functions before main, then main with new lines.

Test functions:

```
işlev test_son_indeks_var() -> mantıksal {
    eşleş son_indeks("abcabc", "bc") {
        değer(v) => { ver v == 4; }
        hiç => { ver yanlış; }
    }
    ver yanlış;
}
```
Hmm, standard pattern from existing test_indeks_var:
```
işlev test_indeks_var() -> tam32 {
    eşleş indeks("merhaba", "hab") {
        değer(v) => { ver v; }
        hiç => { ver 0 - 1; }
    }
    ver 0 - 1;
}
```
That returns tam32. But task says tests return `mantıksal`. So adapt:

```
işlev test_son_indeks_var() -> mantıksal {
    eşleş son_indeks("abcabc", "bc") {
        değer(v) => { ver v == 4; }
        hiç => { ver yanlış; }
    }
    ver yanlış;
}
```

test_son_indeks_yok:
```
işlev test_son_indeks_yok() -> mantıksal {
    eşleş son_indeks("abc", "x") {
        değer(v) => { ver yanlış; }
        hiç => { ver doğru; }
    }
    ver yanlış;
}
```

test_son_indeks_bos:
```
işlev test_son_indeks_bos() -> mantıksal {
    eşleş son_indeks("abc", "") {
        değer(v) => { ver v == 3; }
        hiç => { ver yanlış; }
    }
    ver yanlış;
}
```

test_kirp_sol:
```
işlev test_kirp_sol() -> mantıksal {
    değişken s: metin = metin_birlestir(metin_birlestir("  ", kod_metin(9)), "ab ");
    ver metin_esit(kirp_sol(s), "ab ");
}
```

test_kirp_sag:
```
işlev test_kirp_sag() -> mantıksal {
    değişken s: metin = metin_birlestir(" ab ", kod_metin(10));
    ver metin_esit(kirp_sag(s), " ab");
}
```

test_sozluk:
```
işlev test_sozluk() -> mantıksal {
    eğer sozluk_karsilastir("abc", "abd") != 0 - 1 { ver yanlış; }
    eğer sozluk_karsilastir("b", "a") != 1 { ver yanlış; }
    eğer sozluk_karsilastir("ab", "ab") != 0 { ver yanlış; }
    eğer sozluk_karsilastir("ab", "abc") != 0 - 1 { ver yanlış; }
    eğer sozluk_karsilastir("ç", "z") != 1 { ver yanlış; }
    ver doğru;
}
```
`"ç"` UTF-8 is 2 bytes (0xC3 0xA7) so unsigned first byte 195 > 'z'(122) → returns 1. Good.

test_kod_noktalari:
```
işlev test_kod_noktalari() -> mantıksal {
    değişken d: Dizi<tam32> = kod_noktalari("aç€😀");
    eğer dizi_boyut(d) != 4 { ver yanlış; }
    eğer d[0] != 97 { ver yanlış; }
    eğer d[1] != 231 { ver yanlış; }
    eğer d[2] != 8364 { ver yanlış; }
    eğer d[3] != 128512 { ver yanlış; }
    ver doğru;
}
```
Check: "aç€😀". a=97. ç = U+00E7 = 231, UTF-8 C3 A7: (195-192)*64 + (167-128) = 3*64+39 = 192+39=231. ✓. € = U+20AC = 8364, UTF-8 E2 82 AC: (226-224)*4096 + (130-128)*64 + (172-128) = 2*4096 + 2*64 + 44 = 8192+128+44=8364 ✓. 😀 = U+1F600 = 128512, UTF-8 F0 9F 98 80: (240-240)*262144 + (159-128)*4096 + (152-128)*64 + (128-128) = 0 + 31*4096 + 24*64 + 0 = 126976 + 1536 = 128512 ✓. 

Now main lines:
```
    eğer değil test_son_indeks_var() { yazdir_metin("BASARISIZ (yanlış döndü): test_son_indeks_var"); h = h + 1; }
    eğer değil test_son_indeks_yok() { yazdir_metin("BASARISIZ (yanlış döndü): test_son_indeks_yok"); h = h + 1; }
    eğer değil test_son_indeks_bos() { yazdir_metin("BASARISIZ (yanlış döndü): test_son_indeks_bos"); h = h + 1; }
    eğer değil test_kirp_sol() { yazdir_metin("BASARISIZ (yanlış döndü): test_kirp_sol"); h = h + 1; }
    eğer değil test_kirp_sag() { yazdir_metin("BASARISIZ (yanlış döndü): test_kirp_sag"); h = h + 1; }
    eğer değil test_sozluk() { yazdir_metin("BASARISIZ (yanlış döndü): test_sozluk"); h = h + 1; }
    eğer değil test_kod_noktalari() { yazdir_metin("BASARISIZ (yanlış döndü): test_kod_noktalari"); h = h + 1; }
```

Place them right after `değişken h: tam32 = 0;`.

Now, ESKI must appear exactly once. `işlev main() -> tam32 {\n    değişken h: tam32 = 0;` — unique. Good.

But wait, we're inserting the test functions in YENI before `işlev main`. That means the ESKI's moved? No, we replace the ESKI text with test funcs + same main opening. Fine.

Let me finalize stdlib append. ESKI is the last function's tail. Need to ensure it matches exactly once. It's the end of file. Use:

```
            eğer b1 < 0 { b1 = b1 + 256; }
            eğer değil tr_harf_2byte(u, b1) { ver yanlış; }
            i = i + 2;
        } değilse {
            ver yanlış;
        }
    }
    ver doğru;
}
```
That is from cut 462-470. Should be unique.

Now write YENI = same + appended functions.

Let me double-check the `dizi_olustur` assumption. I'll note it.

Also `ve`/`veya` keyword. Used.

One more: in `kod_noktalari`, I used `değişken ok` inside a branch, then after the branch's `eğer` check. But `ok` declared inside `} değilse eğer ... {` block scope. Then `eğer değil ok` in same block — yes same scope. Fine.

But wait, I declared `değişken ok` in the middle of the branch, after `değişken i`? Actually inside branch. Fine.

Hmm, but declaring variable after some statements in KEMGU — allowed? Likely yes.

Let me also reconsider: for 3-byte branch, I wrote `eğer i + 2 < L {` to guard reading i+1 and i+2. Need i+2 <= L-1 i.e. i+2 < L. Correct.

For 4-byte: i+3 < L. Correct.

Now potential issue: `dizi_ekle(d, (u-192)*64 + ...)` — dizi_ekle takes Dizi and value. Fine.

Let me also check `metin_kes` signature: `metin_kes(s, bas, uz)`. Good.

For kirp_sol when all whitespace, `ver metin_kes(s, L, 0)` — bas=L, uz=0. Should give empty string. Good.

Let me produce final. I'll be careful with exact whitespace matching ESKI.

For the stdlib ESKI, the cut shows exactly:
```
            eğer b1 < 0 { b1 = b1 + 256; }
            eğer değil tr_harf_2byte(u, b1) { ver yanlış; }
            i = i + 2;
        } değilse {
            ver yanlış;
        }
    }
    ver doğru;
}
```

Good.

Now assemble.

Also note: task says "her işlevin üstüne tek satır `// [D-731] ...` yorumu". So each function gets a comment. I'll ensure.

Let me write final YENI for stdlib including all functions with comments.

Final check on test_sozluk using `!= 0 - 1`. In KEMGU `0 - 1` is -1. Good.

I'll now write the output.
