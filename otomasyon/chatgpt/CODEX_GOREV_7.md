# CODEX GÖREVİ 7 — derleyici kusurları C6-001..C6-004 (dalga 6 bulguları)

Dalga 2'nin kuralları AYNEN geçerlidir: `otomasyon/chatgpt/CODEX_GOREV_2.md` §0 (oku),
§3 (DOKUNMA), §4 (döngü), §5 (yasaklar), §6 (bitirince) — yeniden oku. Kusur başına
ayrı dal + draft PR, en sonda özet PR. Soru sorma, durma; 60 dk takılırsan
`codex7_kararlar.md`'ye yaz ve geç. Kaynak: `otomasyon/chatgpt/codex6_bulgular.md`, `LOOP.md` E4i.

## 0. Ön koşul ve çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep D-744     # yoksa 5 dk'da bir, en çok 2 sa; sonra DUR, kaydet
git worktree add ../kemgu-codex7 --detach origin/main && cd ../kemgu-codex7
```
Her kusur: `git fetch origin && git checkout -B codex/d7-<kod> origin/main`.

## 1. Sıra
### C6-001 — BELLEK MODELİ: `&değişken` geçici kopyaya bağlanıyor (İKİ derleyici)
Claude ölçtü (iki derleyicide aynı, hepsi sessiz yanlış):
```
artir(&değişken i)        yerel değişken            -> 4  DOĞRU
artir(&değişken d.i)      yapı alanı                -> 3  YANLIŞ (geçici kopya)
artir(&değişken p.i)      &değişken parametrenin alanı -> 3  YANLIŞ
artir(&değişken e.d.i)    iç içe alan               -> 3  YANLIŞ (self; C'de ayrı T011)
artir(&değişken xs[0])    dizi elemanı              -> 3  YANLIŞ
```
Depoda alan ve dizi elemanı biçiminin kullanımı **0** (Python ile sayıldı; düz `&değişken x`
2710 kez geçiyor — sayaç doğrulandı). Bu yüzden karar ucuz:

**KARAR (Claude, Mehmet adına öneri uygulandı):**
1. **Alan yolu** — kökü bir TANIMLAYICI (yerel ya da `&değişken` parametre) olan
   `.alan` zinciri: alanın **gerçek adresi** geçer (C: `getelementptr`, self aynı).
   Depolama çağrı boyunca sabittir → güvenli.
2. **Diğer her biçim** — yolda `[indeks]` olan (`xs[0]`, `xs[i].f`), çağrı sonucu
   (`f()`), geçici değer: **T022 ile REDDET** (mevcut "geçersiz lvalue" kodu, yeni kod YOK).
   Gerekçe: dizi çağrı sırasında büyürse (`dizi_ekle` → yeniden tahsis) eleman adresi
   boşa düşer = use-after-free; dil bunu önlemeyi vaat ediyor. Mesaj kullanıcıya yolu
   söylesin: elemanı yerele al, değiştir, `dizi_yaz` ile geri yaz.
3. Salt okunur `&` (`f(&d.i)`) bu işin dışında; değiştirme.

Fikstürler: `test/cg_korpus/` davranış (alan / iç içe alan / parametre alanı / iki farklı
alan aynı çağrıda — `swap(&değişken d.i, &değişken d.j)` gibi — exit 42, değerler
ayırt edici) + `test/check_korpus/` red (dizi elemanı, `xs[i].f`, çağrı sonucu → T022) +
POZİTİF yerel `&değişken i`. Kapılar: tam takım (`checker_diff`, `codegen_diff`,
`self_driver` FIXPOINT, ASan dahil).

### C6-003 — `için a: s.xs` gövdesi çalışmıyor (İKİ derleyici, sessiz yanlış)
Yapı alanındaki dizi üzerinde `için` → toplam 0 (21 olmalı). Yerel diziyle çalışıyor.
Kök muhtemelen `için` dalının dizi kaynağını yalnız TANIMLAYICI olarak çözmesi (ölç).
Fikstür: alan, iç içe alan, `&değişken` parametrenin alanı, boş alan dizisi (0 tur),
`Dizi<metin>` ve `Dizi<Yapı>` alanları.

### C6-002 — self: `s.xs = []` (Dizi<Çeşit> alanı) sonraki payload'ı bozuyor
C 68, self 272412716344864. Boş dizinin alan atamasında eleman tipi/genişliği kaybı
(ölç). Fikstür `test/cg_korpus/`.

### C6-004 — self: `ver [R { .. }]` (yapı dizisi literali doğrudan dönüş) alanları bozuk
C doğru; self bozuk ve koşuma göre değişen değerler → başlatılmamış bellek okuma
şüphesi. ASan altında da koş (`calistir_asan_denetim` korpusuna örnek eklenebilir).

## 2. Ardından (zaman kalırsa)
- **T1** Dalga 6 dolanmalarını kaldır (`codex6_bulgular.md` "kalan dolanmalar"): xHCI olay
  halkası kaydını yerele kopyalayıp geri yazma (C6-001), `değişken bos = []; s.kuyruk = bos`
  (C6-002), alan dizisi için yerel kopya (C6-003), `değişken kayitlar = [...]` ara
  değişkeni (C6-004). Kusuru kapananın dolanması kalkar; kalkmayanın yorumu nedenini söyler.
  `make calistir_chatgpt_kopru` + `make calistir_qtest` (6 dal) yeşil olmalı — dolanma
  kaldırma GERÇEK QEMU sürücülerini etkiler, qtest şart.

## 3. Kapılar (her PR)
`CODEX_GOREV_2.md` §4.6 listesi + `make calistir_qtest` + `make test_tumu` (rc PIPESTATUS
ile, atlama/başarısızlık sayılarak). FIXPOINT ✓ şart. C6-001 ve C6-004 için ASan
denetimi (`calistir_asan_denetim`) temiz olmalı.
