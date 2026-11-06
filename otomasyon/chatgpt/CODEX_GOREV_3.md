# CODEX GÖREVİ 3 — karar uygulamaları + yetki ödünç + FAT32 performansı

Dalga 2'nin kurallarının TAMAMI geçerlidir (`otomasyon/chatgpt/CODEX_GOREV_2.md`
§0, §4, §5 — yeniden oku). Aşağıda yalnız farklar ve iş listesi var. Soru sorma,
durma; takılırsan (60 dk) `codex3_kararlar.md`'ye yaz, sıradakine geç.

## 0. Ön koşul — main'de D-737 ve D-738 olmalı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git log origin/main --oneline | grep -E "D-737|D-738"
```
İkisi de yoksa her 5 dakikada `git fetch` ile tekrar bak (en çok 2 saat).
Yoksa `codex3_kararlar.md`'ye "ÖN KOŞUL YOK" yaz ve DUR. Varsa:
```bash
git worktree add ../kemgu-codex3 --detach origin/main && cd ../kemgu-codex3
```
Her iş: `git checkout -B codex/d3-<kısa-ad> origin/main` → draft PR. D numarası
verme, auto-merge açma, merge etme.

## 1. Kararlar (Mehmet adına Opus önerisi uygulandı — kaynak: LOOP.md E4d)
Bunlar artık "DOKUNMA" kapsamında DEĞİL; uygula.

### 1a. C001 — seçili içe aktarımda modül adı ile üye adı çakışması
Karar: **C'nin kuralı korunur** (`src/tip_kontrol.c` seçili ithalat = modül bağı +
üye bağları, aynı ad alanı → T024). Self-host (`selfhost/codegen.kem` checker
yolu) da AYNI konumda T024 vermeli. C DEĞİŞMEZ. Fikstür `test/moduller/`
(`kullan c001_ad::{c001_ad};` → T024) + POZİTİF (`kullan c001_ad;` +
`c001_ad::c001_ad(40)` → OK). Kapı: `checker_diff`, `modul_codegen`.

### 1b. C007 — geçişli tiplerde sıraya duyarlı T011
Karar: **tüm modüllerin tip kayıtları (yapı/çeşit), herhangi bir modülün işlev
imzaları çözülmeden ÖNCE kurulur** (C `src/tip_kontrol.c` modül ön-doldurma
fazları; rapordaki :5718–5746). Mevcut düz-ad fallback kabul kümesi
DEĞİŞTİRİLMEZ — yalnız `kullan` sırasına bağımlılık kalkar. Ölçüt: senin
`build/codex2/c007` üç dosyalı örneğin İKİ sırada da C=OK, exit 42; self zaten
OK. `tests/drivers/virtio/virtio_blk_init_test.kem` C'de T011 1 → 0 olmalı
(kalan T002×5 AYRI kusurdur — ölç, `codex3_kararlar.md`'ye kök neden yaz,
düzeltme). Fikstür `test/moduller/` (iki sıra, iki ayrı ana dosya). Kapılar:
tam takım; özellikle `check_genis`, `surucu_diff`, `checker_diff`.

### 1c. Self checker'ın yerleşik imza yakalaması (dalga 2 yan bulgusu)
Seçili/nitelikli içe aktarılan kullanıcı işlevi `dosya_sil(tam32)` self-host
checker'da YERLEŞİK `dosya_sil(metin)` imzasıyla denetleniyor (sahte T001).
D-735/C009'un checker kolu: kullanıcı işlevi varsa yerleşik imza
kullanılmaz. Fikstür `test/moduller/` (kullanıcı `dosya_sil(n: tam32)`,
`metin_yirmi()`; seçili + nitelikli çağrı; C=self=OK, exit 42).

## 2. `yetki<R>` ödünç alma (LOOP D2b–e)
Tasarım: `belgeler/KEMGU_Yetki_Odunc_Tasarim.md` — TAMAMINI oku; kurallar
oradan BİREBİR. Yeni tanı kodu **CP006** (yalnız bu kod; başka kod ekleme).
Sıra: (a) C checker + korpus fikstürü (`test/check_korpus/`, her yasak madde
ayrı satır + olumlu şekiller) → (b) C codegen (`&yetki<R>` = `%kdl_yetki`
işaretçisi; `test/cg_korpus/` davranış fikstürü, exit 42) → (c) self-host
(`selfhost/codegen.kem`; `checker_diff` + `codegen_diff` birebir) → (d)
`drivers/virtio`'da İADE desenli EN AZ bir işlevi `&yetki<MMIO>`'ya çevir,
`tests/drivers/Makefile.drivers` testleri aynı kalsın. Her alt adım ayrı commit,
tek PR. Kural 3'ün her maddesi için ayrı sabotaj.

## 3. FAT32 yazma performansı (LOOP E4c)
`tests/drivers/fs/fat32_coklu_test.kem` self-host 16 sn, `fat32_yaz_test` 14 sn
(Spark). **Önce PROFİLLE, tahmin etme:** sayaç enstrümantasyonu ya da
`perf record` (varsa) ile sıcak yolu ölç, `codex3_kararlar.md`'ye yaz. Hedef:
ikisi de self-host ≤ 3 sn, **testler değişmeden** (beklenen değer, dosya sayısı,
kâhin aynı). `tools/codex/fat32_kahin.sh` ve `fat32_coklu_kahin.sh` yine
`fsck.fat` temiz vermeli. Önce/sonra süreleri PR'a yaz.

## 4. Dalga 1 dolanmalarını temizle
Dalga 2 bu kusurları kapattı: C002 C004 C005 C006 C008 C009 C010 C011 C003.
(a) `otomasyon/chatgpt/codex_bulgular.md`'deki ORİJİNAL örnekleri iki
derleyicide yeniden koş, sonuçları tabloya yaz (`codex3_kararlar.md`).
(b) Kusuru kapanmış dolanmaları kaldır: `imaja_dosya_yaz`/`imajda_dizin_olustur`/
`imajdan_dosya_sil`/`imajdan_dosya_oku`/`imaj_dizin_listele` ek adlarını, asıl
API adlarına geçerek SİL (tüm çağıranlar dahil); C010 `satir_karakter/satir_tus`
sarmalayıcılarını kaldır; `tests/drivers/probe/` örneklerini kaldır.
C001 (karar: T024 kalır) ve C007 (1b sonrası) dolanmaları da yeniden ölçülerek
gerekiyorsa kalır — nedenini yorumda yaz. `make calistir_chatgpt_kopru` yeşil.

## 5. Kapılar (her PR)
`CODEX_GOREV_2.md` §4.6'daki liste + `make test_tumu` (rc PIPESTATUS ile,
atlama/başarısızlık sayılarak). FIXPOINT ✓ şart.

## 6. Bitirince
Özet draft PR: `codex3_ilerleme.md` + `codex3_kararlar.md` (PR numaraları,
önce/sonra ölçümler, kalanlar, toplam süre).
