# Dalga 7 ilerleme

Rapor: 2026-11-08 17:32 UTC. Dört zorunlu kusur düzeltildi ve bağımsız draft PR'lar açıldı. Yeni 28 fikstür (22 davranış, 6 tanı) ve mevcut LLVM test dosyasına 24 yeni vaka eklendi. T1 entegrasyon bekliyor; hiçbir PR merge edilmedi, auto-merge açılmadı.

## Ön koşul ve çalışma alanı

D-744 (`e1157d0`) main `5598106` içinde doğrulandı. Son `git fetch origin` sonrası main hâlâ `55981061a03a78c01215db53618f5d5bcfe82982`; dört düzeltme commit'i de main'in atası değil.

- İlk ön koşul turu 07:13–09:13 UTC'de iki saat sınırıyla bitti.
- Yeniden başlatılan tur 14:20–15:25 UTC'de beş dakikalık fetch kontrolleriyle bekledi; PR #198 merge olunca koşul sağlandı.
- Uygulama ve doğrulama 15:25 UTC'de başladı; rapor anına kadar yaklaşık 2 saat 7 dakika. Yeniden başlatılan görev, ön koşul bekleyişi dahil yaklaşık 3 saat 12 dakika.
- Yalnız `/home/msyrt/Desktop/kemgu-codex7` worktree'sinde yazıldı. Ana çalışma ağacı ve önceki Codex worktree'leri değiştirilmedi. Her kusur dalı güncel origin/main'den açıldı; birbirinin commit'lerini içermiyor.

## İş sırası ve PR'lar

| Kusur | Sonuç | Draft PR | Commit | UTC çalışma aralığı |
|---|---|---|---|---|
| C6-001 | Gerçek alan adresi; indeks/çağrı/geçici biçimleri T022; jenerik alan düzeni |[#201](https://github.com/msyrt-sys/kemgu/pull/201)|`8dcc116`|15:25–16:21|
| C6-003 | Alan dizisi döngüsü; metin, yapı ve işaretsiz eleman bilgisi |[#202](https://github.com/msyrt-sys/kemgu/pull/202)|`041f4bf`|16:21–16:44|
| C6-002 | Self boş alan atamasında eleman tipi; C iç içe alan çökmesi |[#203](https://github.com/msyrt-sys/kemgu/pull/203)|`dc8e1e9`|16:44–17:06|
| C6-004 | Doğrudan/boş yapı dizisi dönüşünde eleman tipi |[#204](https://github.com/msyrt-sys/kemgu/pull/204)|`1e0bb49`|17:06–17:31|

## Kırmızı, yeşil ve sabotaj

- C6-001: yerel kontrol42, dört alan biçimi1; altı yasak biçim kabul ediliyordu. Jenerik ek ölçümde C exit1, self geçersiz GEP. Son altı davranış iki derleyicide42, altı red fikstürü T022. Son sabotajda derleyici derlemesi0, LLVM305/294/11 ve rc2; geri yükleme305/305. Jenerik kapsam eklendikten sonra bütün kapılar tekrarlandı.
- C6-003: altı alan biçimi iki derleyicide1, yerel/boş kontroller42. LLVM301/295/6 kırmızıdan301/301 yeşile geçti. Sabotaj derlemesi0, kapı2; 12 kırmızı koşum ve4 pozitif kontrol. Geri yükleme16/16 koşum42.
- C6-002: C doğrudan/referans/yerel42, iç içedeSIGSEGV; self üç alan biçiminde2, yerelde42. Codegen199/202 ve LLVM297/296/1 kırmızı; sonra202/202 ve297/297. Sabotaj derlemesi0, iki kapı2; temiz geri yükleme8/8 koşum42.
- C6-004: C42; self doğrudan2, boş1, yerel42. İlk kapı199/201 kırmızı; ek pozitif kontrolle son kapı202/202. Sabotaj derlemesi0, codegen200/202 ve rc2; temiz geri yükleme8/8 koşum42.

Her sabotaj kaynak yedeğiyle yapıldı; ilgili nesne/ikili/IR silinerek geri derlendi. Sabotajlar derleyici derlemesinin bozulmasına dayanmıyor. Geçersiz ölçümler ve ek bulgular `codex7_kararlar.md` içinde açıkça ayrıldı.

## Son doğrulamalar

Bütün kapılar seri çalıştırıldı. `make` çıkışları `PIPESTATUS[0]` ile okundu. Her PR için görevdeki geniş kapılar, `make calistir_qtest`, `make calistir_bm_x86_nvme` ve tam `make test_tumu` rc0. Her PR'ın gövdesinde dosya:satır kök nedeni ve sayılı ölçümler var.

| Kusur | Codegen (her üç ikili) | Checker | Yapısal IR | FIXPOINT satırı | Yeni fikstür ASan koşumu | Son tam takım UTC |
|---|---:|---:|---:|---:|---:|---|
|C6-001|204/204|310/310|182/182|97876 ✓|12/12|16:05:36–16:20:18, rc0|
|C6-003|206/206|304/304|184/184|97879 ✓|ayrı koşulmadı|16:29:42–16:43:25, rc0|
|C6-002|202/202|304/304|180/180|97748 ✓|8/8|16:52:50–17:05:54, rc0|
|C6-004|202/202|304/304|180/180|97741 ✓|8/8|17:16:55–17:30:19, rc0|

Ortak sonuçlar: modül59/59; sürücü17/17; sıfır uyarı38 dosya; ChatGPT köprüsü kart+122 test; qtest6/6 dal (C/self ayrı QEMU örnekleri); bare-metal NVMe20/20 açılış; ASan matrisi12/12. C6-001 ve C6-004 için genel ASan kapısı ayrıca koşuldu. Yeni ASan ölçümlerinde LLVM fonksiyonları ölçüm kopyasında `sanitize_address` ile işaretlendi ve runtime enstrümante edildi; exit42/boş stderr doğrulandı.

## Atlama ve muafiyet sayımı

| Kapı | C6-001 | C6-003 / C6-002 / C6-004 (her biri) |
|---|---:|---:|
| Codegen üç koşum |12|12|
| Geniş codegen |8|9|
| Yapısal IR |2|2|
| Sürücü paritesi |1|1|
| Eski VirtIO host |5|5|
| Bölge operandı |4|4|
| ASan E2E |32|33|
| **Korpus atlama olayı** |**64**|**66**|

Her tam koşumda QEMU üst bellek alt senaryosu RAM≤1GiB nedeniyle ayrıca1 kez atlandı. Korpus muafiyetleri24 yapısal+1 sürücü+18 geniş checker+10 tip kontrol=53. ASan'ın4 bilinen ve2 sızıntı istisnası ayrıca sayılır. Özet ve ayrıntı satırları çift sayılmadı; yeni fikstürlerde atlama yok.

C6-001 genel ASan132 PASS/0 FAIL; diğerleri131 PASS/0 FAIL. C6-001 jenerik adres düzeltmesi mevcut05_yapi örneğinin çalışmasını sağladığı için iki atlama azaldı. Geniş codegen günlüklerindeki dört mevcut bare-metal host segmentation-fault satırı, eşleşen C/self çıkışlarıdır; donanım başarısı olarak sunulmadı. Lean/lake yok:32 dosyada0 sorry/admit metin kapısı geçti; gerçek Lean tip denetimi yapılmadı.

## Entegrasyon ve CI

Rapor anında #201/#202/#203 Linux CI başarılı; Windows koşuları sürüyor. #204 Linux ve Windows CI sürüyor. Dördü de OPEN+DRAFT ve autoMergeRequest=null. Yerel doğrulamalar tamamlandı; CI'ın tamamı bitmiş gibi sunulmadı.

Dallar ayrı ayrı test edildi; dört düzeltmenin birleştirildiği bir dal test edilmedi. T1, düzeltmeler origin/main'e alınmadığı için uygulanmadı. Ayrıntı ve devam noktaları `codex7_kararlar.md` içinde.

`selfhost/checker.kem`, runtime, Makefile, LOOP.md, CLAUDE.md, CI ve kapı betikleri değiştirilmedi. Mevcut test beklentileri veya muafiyet listeleri değiştirilmedi; test silinmedi. Yeni tanı kodu/D numarası verilmedi. Son özet PR yalnız bu iki raporu taşır.

Yerel ölçüm günlükleri `build/codex7/C6-00*-*.log` ve JSON dosyalarında; son kanıtlar `*-final-*`, `*-sabotage-*` ve `*-restored*` adlarını taşır.
