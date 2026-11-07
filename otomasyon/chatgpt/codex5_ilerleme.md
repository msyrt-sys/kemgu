# Dalga 5 ilerleme

Başlangıç: 2026-11-07 17:33:44 UTC (worktree açılışı). D-740, origin/main 3225ce1 üzerinde doğrulandı. Yalnız `/home/msyrt/Desktop/kemgu-codex5` içinde çalışıldı.

Görev listesinin sonucu: **5 kusur düzeltildi; 0 kusur karar bekliyor; 0 kusur TAKILDI.** T1 temizliği aşağıdaki ana dal ön koşulu nedeniyle uygulanmadı. Beş düzeltme PR'ı açık/draft; auto-merge kapalı.

| Kusur | Durum | Kanıt | PR |
|---|---|---|---|
| F003 | Düzeltildi | Önce 195/196 rc=2, sonra 196/196; sabotaj rc=2; seri kapılar ve tam takım rc=0; FIXPOINT 97497 satır; 65 korpus atlaması + 1 QEMU alt senaryosu, 52 muafiyet | https://github.com/msyrt-sys/kemgu/pull/190 |
| F005 | Düzeltildi | C/self link rc=1 → exit=42; geniş kapı 59/60 → 60/60, sabotaj rc=2; modül 57/57; tüm kapılar + bare-metal/x86 ve tam takım rc=0; FIXPOINT 97435; ad kararlılığı ve self2 ölçüldü | https://github.com/msyrt-sys/kemgu/pull/191 |
| F002 | Düzeltildi | Geniş kapı 59/60 → 60/60, sabotaj rc=2; ek bölge sapması 1/198 → 198/198 ve ayrı sabotaj rc=2; seri kapılar ve son tam takım rc=0, FIXPOINT 97415; ilk başarısız tam koşum ayrı kaydedildi | https://github.com/msyrt-sys/kemgu/pull/192 |
| F004 | Düzeltildi | C link rc=1 → exit=42; geniş kapı 59/60 → 60/60 ve sabotaj rc=2; ek bölge 1/198 → 198/198 ve ayrı sabotaj rc=2; son seri kapılar/tam takım rc=0, FIXPOINT 97357 | https://github.com/msyrt-sys/kemgu/pull/193 |
| F001 | Düzeltildi | Checker 299/301 → 301/301; self yerleşim 55/57 → 57/57; C ve self sabotajları rc=2, lineer bayrak ek sabotajı hatalı OK; son kapılar/tam takım rc=0, FIXPOINT 97431; NVMe/AHCI qtest C+self geçti | https://github.com/msyrt-sys/kemgu/pull/194 |

## Tabanlar ve commitler

| Kusur | Dal | Başlangıç origin/main | Commit |
|---|---|---|---|
| F003 | codex/d5-f003 | 3225ce1 | f7a2f5a |
| F005 | codex/d5-f005 | 3225ce1 | ffeb68a |
| F002 | codex/d5-f002 | 3225ce1 | 2fd2918 |
| F004 | codex/d5-f004 | 003d9e5 | 18419a7 |
| F001 | codex/d5-f001 | 98f39b7 | 378a54b |

## T1 — ana dal ön koşulu eksik

2026-11-07 20:13 UTC fetch sonrasında origin/main=98f39b7. Beş düzeltme commitinin her biri için `git merge-base --is-ancestor <commit> origin/main` rc=1; PR #190–#194 açık/draft, merged_at ve auto_merge null. Güncel main'den bağımsız temizlik dalında bu dolanmaları kaldırmak kusurları geri açardı. Düzeltme dalları merge/cherry-pick edilmedi; dolanmalar korunup envanter karar dosyasına yazıldı. Alias'ların C001/T024 nedeniyle gerekli kısmı ayrıca ayrıldı.

## Özet PR doğrulaması

Özet dalı güncel origin/main 98f39b7 tabanında yalnız bu ilerleme raporu ile `codex5_kararlar.md` dosyasını içerir. Beş PR'ın birleşik entegrasyon dalı değildir.
Sekiz zorunlu kapı seri çalıştırıldı: rc=0. Ardından tam `make test_tumu`: rc=0 (iki rc de `${PIPESTATUS[0]}` ile alındı), **FIXPOINT ✓, 97358 satır**. Codegen 195/195, checker 297/297, modül 55/55, yapısal 173/173, sürücü 17/17, geniş codegen 59/59, bölge operandı 197/197; 38 dosyada 0 uyarı. QEMU ARM64 2/2, x86_64 1/1; gerçek NVMe/AHCI qtest C+self toplam 4/4; köprü kart + 117 test geçti. ASan 128 PASS / 0 FAIL / 32 SKIP, 4 bilinen ve 2 sızıntı muafiyeti; ASan matrisi 12/12. Toplam 65 korpus atlama olayı + 1 QEMU üst bellek alt senaryosu; ayrıca 52 korpus muafiyeti. Başarısızlık özeti/make hata satırı yok. Lean araçları bulunmadığından yalnız 32 dosyada 0 sorry/admit metinsel kapısı çalıştı. Ayrıntılı sayım ve eşleşen bare-metal 139 çıkışlarının sınırı karar raporunda. Yerel günlükler: `build/codex5/ozet/kapi.log`, `build/codex5/ozet/tumu.log`.

## Süre

Worktree açılışından son tam doğrulamanın bitişine: **2 saat 44 dakika 23 saniye** (2026-11-07 17:33:44–20:18:07 UTC). PR gönderimi bu ölçüm aralığının dışındadır.
