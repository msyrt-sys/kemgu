# Codex 2 ilerleme

Başlangıç: 2026-11-06. Çalışma alanı: `../kemgu-codex2`.

## Tamamlanma özeti

- Düzeltilen: **9**; karar bekleyen: **2** (C001, C007); 60 dakika engeline takılan: **0**.
- Başlangıç: 2026-11-06T04:07:16.922232+00:00; rapor bitişi: 2026-11-06T07:01:34.986755+00:00.
- Toplam çalışma süresi (raporun gönderime hazırlanmasına kadar): **2 saat 54 dakika 18 saniye**.
- Dokuz bağımsız düzeltme dalı ve PR’ı; her biri işe başlarken güncel origin/main’den açıldı. İlk ikisinin tabanı abd9aa9, kalanların tabanı 71aa91e. Özet dalı codex/derleyici-ozet, tabanı 71aa91e.
- Dokuz PR’ın tamamı draft; auto-merge kapalı olduğu GitHub’dan doğrulandı. Main’e push, merge veya PR onayı yapılmadı.

| Kusur | Sonuç | PR |
|---|---|---|
| C009 | Kullanıcı işlevini yerleşik hedefe yönlendirme | [#164](https://github.com/msyrt-sys/kemgu/pull/164) |
| C002 | Yapı dizisi eleman genişliği | [#165](https://github.com/msyrt-sys/kemgu/pull/165) |
| C004 | Bayt değerinin işaretle genişlemesi | [#166](https://github.com/msyrt-sys/kemgu/pull/166) |
| C005 | İthal yapı sonrası tam64 parametre | [#167](https://github.com/msyrt-sys/kemgu/pull/167) |
| C011 | tam32 yapı alanına atama | [#168](https://github.com/msyrt-sys/kemgu/pull/168) |
| C003 | sonuç içindeki yapı dizisi | [#169](https://github.com/msyrt-sys/kemgu/pull/169) |
| C008 | Bileşik dönüşün sıfır değeri | [#170](https://github.com/msyrt-sys/kemgu/pull/170) |
| C006 | İthal yapıda boş dizi bağlamı | [#171](https://github.com/msyrt-sys/kemgu/pull/171) |
| C010 | İthal çeşit kurucusu kayıtları | [#172](https://github.com/msyrt-sys/kemgu/pull/172) |
| C001 | Modül/üye adı sözleşmesi; karar bekliyor | Özet karar kaydı |
| C007 | Geçişli tip fazı/kapsamı; karar bekliyor | Özet karar kaydı |


Her düzeltmede yeni fikstürle önce kırmızı kapı, sonra onarım ve derleyici kurulumu bozulmadan kırmızı sabotaj kanıtı alındı. Ardından sekiz kapı ve tam make test_tumu seri koşuldu; her iki rc PIPESTATUS ile 0, self-driver FIXPOINT ✓. Dokuz tam koşunun her birinde istenen tarama 32 eşleşen satır ve 0 pozitif başarısızlık sayacı verdi; 32 sayısı benzersiz atlanan test sayısı değildir. Ayrıntılı atlama kategorileri her PR’da bulunur.

Değişiklik denetimi: dokuz düzeltme commit’i yalnız src/llvm.c, selfhost/codegen.kem ve yeni test/moduller, test/cg_korpus, test/check_korpus dosyaları içeriyor. Eski fikstürler/beklentiler, harness, Makefile, muafiyetler, dondurulmuş selfhost/checker.kem, CLAUDE.md, LOOP.md ve sürücü dolanmaları değiştirilmedi. Ana çalışma ağacına ve dalga 1 ağacına yazılmadı. Karar maddelerinde politika seçilmedi, düzeltme/test başarısı iddia edilmedi.

Sınırlar: Lean/lake yok; tam takımın Lean kapısı gerçek tip denetimi değil, 32 dosyada metin kontrolleridir. C009’daki ayrı seçili dosya_sil imza çözümleme gözlemi karar raporunda açık bırakıldı. Bu özet PR yalnız iki Markdown raporunu içerir; dokuz düzeltmenin birlikte birleşmiş sonucu bu görevde test edilmedi.

## C009 — düzeltildi

- Draft PR: https://github.com/msyrt-sys/kemgu/pull/164 · commit b7ead16 · taban abd9aa9.
- Önce modül 30/31 ve codegen 188/189 (rc=2). Sonra 31/31 ve 189/189; altı doğrudan koşum exit=42.
- Sabotaj: derleyici kuruldu (rc=0), iki kapı tekrar kırmızı (rc=2); yedekten geri yükleme ve taze ikililer.
- Sekiz kapı rc=0; tam test_tumu rc=0 (PIPESTATUS). İki FIXPOINT ✓. QEMU ARM64 2/2 + x86_64 1/1, ASan matris 12/12.
- Atlama taraması 32 eşleşen satır, pozitif başarısızlık sayacı 0. ASan E2E PASS=128 FAIL=0 SKIP=32 ALLOW=4 SIZINTI-MUAF=2. Ayrıntılı kategori sayıları PR gövdesinde.
- Bitiş: 2026-11-06T04:26:59.355705+00:00.

## C002 — düzeltildi

- Draft PR: https://github.com/msyrt-sys/kemgu/pull/165 · commit 8c382e1 · taban abd9aa9.
- Önce C=1, self=42; codegen kapısı 188/189, rc=2. Sonra iki derleyici 42; kapı 189/189, rc=0.
- Sabotaj kurulumu rc=0; aynı değer farkıyla kapı rc=2. Yedekten geri yükleme ve taze derleme yapıldı.
- Sekiz kapı ve tam test_tumu PIPESTATUS rc=0; iki FIXPOINT ✓. Atlama taraması 32 satır, pozitif başarısızlık sayacı 0. Ayrıntılar PR gövdesinde.
- Bitiş: 2026-11-06T04:42:45.213577+00:00.

## C004 — düzeltildi

- Draft PR: https://github.com/msyrt-sys/kemgu/pull/166 · commit 1d3f949 · taban 71aa91e.
- Önce C=42, self=1; codegen 188/189 rc=2. Sonra iki derleyici 42; kapı 189/189 rc=0.
- Sabotaj kurulumu rc=0, kapı rc=2; kaynak geri yüklendi ve ikililer tazelendi. Fikstürün i8→i32 IR ölçümü zext 3→6, sext 9→6; işaretli yol korunur.
- Sekiz kapı + tam test_tumu PIPESTATUS rc=0; FIXPOINT ✓ (94.794 satır). Atlama taraması 32 satır, pozitif başarısızlık sayacı 0. Ayrıntılar PR gövdesinde.
- Bitiş: 2026-11-06T04:58:29.898204+00:00.

## C005 — düzeltildi

- Draft PR: https://github.com/msyrt-sys/kemgu/pull/167 · commit e99d871 · taban 71aa91e.
- Önce C=42, self=1; modül 29/30 rc=2. Sonra iki derleyici 42; modül 30/30 rc=0. Altı çağrının son parametresi artık i64 (önce dört i32, iki i64).
- Sabotaj kurulumu rc=0, modül kapısı aynı farkla rc=2; onarım ve taze ikililer geri getirildi.
- Sekiz kapı + tam test_tumu PIPESTATUS rc=0; FIXPOINT ✓ (94.689 satır). Atlama taraması 32 satır, pozitif başarısızlık sayacı 0. Ayrıntılar PR gövdesinde.
- Bitiş: 2026-11-06T05:14:07.615830+00:00.

## C011 — düzeltildi

- Draft PR: https://github.com/msyrt-sys/kemgu/pull/168 · commit f35ea23 · taban 71aa91e.
- Önce C=42, self link rc=1; codegen 188/189 rc=2. Sonra iki derleyici 42; kapı 189/189 rc=0.
- Sabotaj kurulumu rc=0, kapı aynı i64/i32 link hatasıyla rc=2. Yedek onarım geri yüklendi, ikililer tazelendi.
- Sekiz kapı + tam test_tumu PIPESTATUS rc=0; FIXPOINT ✓ (94.702 satır). Atlama taraması 32 satır, pozitif başarısızlık sayacı 0. Ayrıntılar PR gövdesinde.
- Bitiş: 2026-11-06T05:32:24.881805+00:00.

## C003 — düzeltildi

- Draft PR: https://github.com/msyrt-sys/kemgu/pull/169 · commit a651d95 · taban 71aa91e.
- Önce C=42, self link rc=1 (%Nokta yerine i32); codegen 188/189 rc=2. Sonra iki derleyici 42; kapı 189/189 rc=0.
- Sabotaj kurulumu rc=0, kapı aynı link hatasıyla rc=2; onarım yedekten geri yüklendi ve ikililer tazelendi.
- Sekiz kapı + tam test_tumu PIPESTATUS rc=0; FIXPOINT ✓ (94.795 satır). Atlama taraması 32 satır, pozitif başarısızlık sayacı 0. Ayrıntılar PR gövdesinde.
- Bitiş: 2026-11-06T05:48:24.167223+00:00.

## C008 — düzeltildi

- Draft PR: https://github.com/msyrt-sys/kemgu/pull/170 · commit 8e81878 · taban 71aa91e.
- Önce iki derleyici link rc=1 (aggregate türüne tamsayı 0); codegen 188/189 rc=2. Sonra iki derleyici 42; kapı 189/189 rc=0.
- C ve self onarımları ayrı ayrı sabote edildi: her kurulum rc=0, her kapı 188/189 rc=2. Onarımlar ve taze ikililer geri getirildi.
- Sekiz kapı + tam test_tumu PIPESTATUS rc=0; FIXPOINT ✓ (94.741 satır). Atlama taraması 32 satır, pozitif başarısızlık sayacı 0. Ayrıntılar PR gövdesinde.
- Bitiş: 2026-11-06T06:04:41.889784+00:00.

## C006 — düzeltildi

- Draft PR: https://github.com/msyrt-sys/kemgu/pull/171 · commit 51b72da · taban 71aa91e.
- Önce geçerli program C=42, self dört T014 ve LLVM rc=1; checker 241/243 rc=2. Sonra iki derleyici 42; dört yeni fikstürle checker 244/244 ve modül 31/31 rc=0.
- Negatif fikstürün T001/T013/T014 tanıları C ile birebir korundu; eski lambda beklentisi değiştirilmedi. Kaynak yedeği sabotajında kurulum rc=0, checker 241/244 rc=2.
- Sekiz kapı + tam test_tumu PIPESTATUS rc=0; FIXPOINT ✓ (94.830 satır). Self-driver checker 199/199. Atlama taraması 32 satır, pozitif başarısızlık sayacı 0. Ayrıntılar PR gövdesinde.
- Bitiş: 2026-11-06T06:24:06.707213+00:00.

## C010 — düzeltildi

- Draft PR: https://github.com/msyrt-sys/kemgu/pull/172 · commit 362d268 · taban 71aa91e.
- Önce C=42, self altı T016 ve LLVM rc=1. Yedi yeni fikstürle sonra iki derleyici 42; checker 247/247, modül 35/35 rc=0.
- Tam kaynak sabotajı 244/247, görünürlük sabotajı 244/247, tanım kapsamı sabotajı 246/247 rc=2; her kurulum rc=0. İthalat kapsamı sabotajı o aşamada 245/246 rc=2. İki ara koşu yeni prob farkı nedeniyle durduruldu; başarı sayılmadı.
- Son kaynakla sekiz kapı + tam test_tumu PIPESTATUS rc=0; FIXPOINT ✓ (95.356 satır). Atlama taraması 32 satır, pozitif başarısızlık sayacı 0. Ayrıntılar PR gövdesinde.
- Bitiş: 2026-11-06T06:56:09.869140+00:00.

## C001 — karar bekliyor

- Taban 71aa91e; ayrı yerel dal codex/derleyici-c001. C seçili aynı ad için T024/check=1, self OK/exit=42; çıplak modül + nitelikli çağrı iki tarafta exit=42.
- Modül bağı + tek ad alanı bilinçli sözleşmeyle ilişkili. Üç seçenek, kaynak kanıtları ve 814 .kem + 183 C/header dosyasındaki etki taraması codex2_kararlar.md içinde. Doğrudan mevcut aynı-ad tetikleyicisi 0; örtük modül bağı kaldırılırsa kırılacak doğrulanmış program 1.
- Kod/kalıcı beklenti değiştirilmedi; düzeltme PR’ı açılmadı. Tam takım/sabotaj bu karar maddesine uygulanmadı.
- Bitiş: 2026-11-06T06:57:03.937097+00:00.

## C007 — karar bekliyor

- Taban 71aa91e; ayrı yerel dal codex/derleyici-c007. Geçişli/ters sırada C T011, self exit=42; bağımlı tip önce yüklenince iki taraf exit=42. Nitelikli imza tek başına çözmedi (C T016).
- Faz sırası kusuru ölçüldü; açık ithalat yalıtımı ile bilinçli düz-ad fallback arasındaki sözleşme seçilmedi. Seçenekler ve kaynak kanıtları codex2_kararlar.md içinde.
- 9 tip referansı/4 aday dosya ölçüldü; 1 mevcut dosyada T011 doğrulandı. Geçici kopyada bağımlılığı öne almak T011’ü kaldırdı; ayrı T002×5 kaldı.
- Kod/kalıcı beklenti değiştirilmedi; düzeltme PR’ı açılmadı. Tam takım/sabotaj bu karar maddesine uygulanmadı.
- Bitiş: 2026-11-06T06:59:47.608286+00:00.
