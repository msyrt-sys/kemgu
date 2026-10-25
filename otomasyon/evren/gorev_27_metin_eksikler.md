# Tur 27 — stdlib/metin.kem eksik işlevleri (D5f) [D-731]

İKİ ```duzenle bloğu üret (ESKİ = kesitteki satırlar BİREBİR). Başka dosya YOK. Saf KEMGU.
Kullanılabilir yerleşikler: `metin_uzunluk(s)` (BAYT), `metin_bayt(s, i)` (tam8, İŞARETLİ —
u = (metin_bayt(s,i) olarak tam32); eğer u < 0 { u = u + 256; }), `metin_kes(s, bas, uz)`,
`metin_esit(a, b)`, `metin_birlestir(a, b)`, `kod_metin(kod) -> metin`, `dizi_ekle(d, v)`,
`dizi_boyut(d)`, `d[i]`. Dizgi literalinde KAÇIŞ YOK (`"\t"` iki karakterdir) — sekme/satır
sonu için `kod_metin(9)` / `kod_metin(10)`. `ve`/`veya` anahtar kelimedir. `seçimlik<T>`:
`ver değer(x);` / `ver hiç;`.

## 1) `stdlib/metin.kem` SONUNA ekle (her işlevin üstüne tek satır `// [D-731] ...` yorumu)
- `işlev son_indeks(s: metin, alt: metin) -> seçimlik<tam32>` — `alt`ın SON geçişinin bayt
  indeksi; boş `alt` → `değer(metin_uzunluk(s))`; yoksa `hiç`. (Mevcut `indeks`in aynası, sondan.)
- `işlev bosluk_bayt_mi(u: tam32) -> mantıksal` — u ∈ {32, 9, 10, 13}.
- `işlev kirp_sol(s: metin) -> metin` — baştaki boşluk baytlarını (bosluk_bayt_mi) at.
- `işlev kirp_sag(s: metin) -> metin` — sondaki boşluk baytlarını at.
- `işlev sozluk_karsilastir(a: metin, b: metin) -> tam32` — İŞARETSİZ bayt bayt; ilk farklı
  baytta a<b → `0 - 1`, a>b → `1`; ortak önek eşitse kısa olan küçük; tam eşit → `0`.
- `işlev kod_noktalari(s: metin) -> Dizi<tam32>` — UTF-8 çöz: u<128 → u (1 bayt);
  192..223 → ((u-192)*64 + (b1-128)) (2 bayt); 224..239 → ((u-224)*4096 + (b1-128)*64 + (b2-128))
  (3 bayt); 240..247 → ((u-240)*262144 + (b1-128)*4096 + (b2-128)*64 + (b3-128)) (4 bayt);
  diğer / eksik devam baytı → 65533 ekle ve 1 bayt ilerle. b1/b2/b3 işaretsiz okunur.

## 2) `test/stdlib/test_metin.kem` — testler + main satırları
`işlev main` satırının ÜSTÜNE (dosyadaki `test_*` desenine uyan, `mantıksal` döndüren) ekle:
- `test_son_indeks_var`: son_indeks("abcabc", "bc") == değer(4) → doğru
- `test_son_indeks_yok`: son_indeks("abc", "x") hiç → doğru
- `test_son_indeks_bos`: son_indeks("abc", "") == değer(3) → doğru
- `test_kirp_sol`: s = metin_birlestir(metin_birlestir("  ", kod_metin(9)), "ab ");
  metin_esit(kirp_sol(s), "ab ") → doğru
- `test_kirp_sag`: s = metin_birlestir(" ab ", kod_metin(10)); metin_esit(kirp_sag(s), " ab") → doğru
- `test_sozluk`: sozluk_karsilastir("abc","abd") == 0-1 ve ("b","a") == 1 ve ("ab","ab") == 0
  ve ("ab","abc") == 0-1 ve ("ç","z") == 1 → doğru
- `test_kod_noktalari`: d = kod_noktalari("aç€😀"); dizi_boyut(d) == 4 ve d[0]==97 ve
  d[1]==231 ve d[2]==8364 ve d[3]==128512 → doğru
`main` içinde `eğer h == 0 {` satırının ÜSTÜNE her biri için:
`    eğer değil test_X() { yazdir_metin("BASARISIZ (yanlış döndü): test_X"); h = h + 1; }`
