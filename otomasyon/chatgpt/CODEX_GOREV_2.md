# CODEX GÖREVİ 2 — derleyici kusurları (C001–C011), kusur başına bir PR

Dalga 1'de bulduğun 11 derleyici kusurunu düzelteceksin. Bu kez **derleyiciye
dokunacaksın**, o yüzden kurallar dalga 1'den SIKIdır. Soru sorma, durma; karar
gerektiren yerde aşağıdaki "DOKUNMA" kuralına uy, kaydet ve sıradakine geç.

## 0. Önce oku (atlama)
1. `CLAUDE.md` — özellikle: "Türkçe UTF-8 Dikkat Noktası" (C'de `\x` hex kaçışı
   sonrası hex rakam → dizgi birleştirme ŞART), "ÖLÇÜM ARACI KONTROL LİSTESİ",
   "SELF-HOST CHECKER'IN TEK KAYNAĞI `selfhost/codegen.kem`" (`selfhost/checker.kem`
   DONDURULDU — oraya kural portlama).
2. `otomasyon/chatgpt/codex_bulgular.md` — kendi raporun.
3. `LOOP.md` → `E4b` maddesi: önem sırası ve kapsam.

## 1. Çalışma alanı
```bash
cd /home/msyrt/Desktop/kemgu && git fetch origin
git worktree add ../kemgu-codex2 --detach origin/main
cd ../kemgu-codex2
```
Her kusur için: `git fetch origin && git checkout -B codex/derleyici-<kod> origin/main`
(her PR güncel main'den; birbirine bağımlı değil). Yalnız `../kemgu-codex2`
içinde çalış. `../kemgu-codex` (dalga 1) ve ana klasör yasak.

## 2. Sıra (önem sırası — sessiz yanlış cevap önce)
1. **C009** GÜVENLİK: self-host nitelikli `m::dosya_sil(..)` çağrısını
   `@kdl_dosya_sil`'e (GERÇEK host dosya silme) yönlendiriyor. C tarafında da
   yerel `dosya_adi` yardımcısı `@kdl_dosya_adi`'ye gidiyor. Kök: yerleşik ad
   eşlemesi (önek: `dosya_`, `metin_`, `yaz_` …) KULLANICI işlevinden önce
   soruluyor. Self-host'ta D-735 çıplak/seçili yolu kapattı (`fn_var_mi(p, fn_coz(p, fad))`,
   `selfhost/codegen.kem` "D-735" ara); YOL (nitelikli) hedefi açık. C'de
   `src/llvm.c` yerleşik eşlemesinin kullanıcı işlevi kontrolünü ölç. İki
   derleyicide de kural: **kullanıcı işlevi (yerel, seçili, nitelikli) varsa
   yerleşik eşlemesi YAPILMAZ.** Fikstür `test/moduller/`e (modül + `main`):
   kullanıcı `dosya_sil`, `dosya_adi`, `metin_x` tanımlar, nitelikli ve çıplak
   çağırır, exit 42; POZİTİF olarak gerçek bir yerleşik (`metin_uzunluk`) de
   çağrılır.
2. **C002** C oracle: `değişken d: Dizi<Nokta> = dizi_olustur(0);` eleman
   genişliğini 4 bayt alıyor (`kdl_dizi_olustur(..., i32 4)`), `tam64` alan
   bozuk okunuyor. Self doğru. Fikstür `test/cg_korpus/`.
3. **C004** self: `dizi_ekle(b, dizi_al(a, 0))` (`Dizi<dtam8>`) değeri işaretle
   genişletiyor (203 → -53). C doğru. Fikstür `test/cg_korpus/`.
4. **C005** self: içe aktarılan yapı argümanından sonraki `tam64` parametreye
   literal `i32` geçiyor. Fikstür `test/moduller/`.
5. **C011** self LINK-RED: `d.n = -2147483648;` (tam32 alan) → i64.
6. **C003** self LINK-RED: `sonuç<Dizi<Nokta>, metin>` eşleşmesinde `dizi_al`.
7. **C008** İKİ derleyici LINK-RED: kapsayıcı `eşleş` gövdeli, `sonuç<Yapı, metin>`
   dönen işlev → `ret {i8, %N, ptr} 0`. D-464 (`CLAUDE.md`'de ara) aynı sınıfı
   `metin`/`kesirli` için kapattı; yapı payload'lı sonuç kolu açık.
8. **C006** self sahte T014: içe aktarılan yapının dizi alanına `[]`.
9. **C010** self sahte T016: içe aktarılan çeşidin kurucusu `TusOlayi::Karakter(65)`.
10. **C001** C T024 / self OK: modülle aynı adlı işlevin seçili içe aktarımı.
11. **C007** C T011 / self OK: geçişli içe aktarmada bağımlı yapı tipleri.

Bu kusurlar dalga 1'in dolanma kodunu GEREKSİZ kılar. Dolanmalara DOKUNMA
(ayrı iş); yalnız `tests/drivers/probe/` örnekleri artık dolanmasız da geçiyorsa
bunu PR açıklamasına yaz.

## 3. DOKUNMA kuralı (karar Mehmet'in)
C001, C007 ve hangi tarafın doğru olduğu belirsiz her check-paritesi farkında:
önce C kaynağında o davranışın **bilinçli** olup olmadığını ölç (`src/` yorumu,
`DECISIONS_LOG`/`CLAUDE.md`'de D-numarası, mevcut fikstür). Bilinçliyse kodu
DEĞİŞTİRME; `otomasyon/chatgpt/codex2_kararlar.md`'ye "seçenekler + ölçülen
etki alanı (depoda kaç dosya etkilenir)" yaz ve geç. Dil yüzeyini (neyin geçerli
program olduğunu) değiştiren hiçbir şeyi kendin seçme. Yeni tanı kodu EKLEME.

## 4. Her kusur için döngü (DEĞİŞMEZ)
1. Minimal yeniden üretim → iki derleyicide ölç (çıkış kodunu `$?` ile, boru
   olmadan oku; `| tr` / `| head` çıkış kodunu yutar).
2. **Fikstür ÖNCE**: doğru korpusa ekle (`test/cg_korpus/` davranış, exit 42 ·
   `test/check_korpus/` tanı · `test/moduller/` çapraz-dosya, `işlev main()`
   şart). Fikstür ayırt edici olmalı: 2^33 gibi i32'ye sığmayan değer, ilk
   olmayan varyant, ≥3 farklı değer. POZİTİF şekil ekle (yoksa "her şeyi
   reddet/izin ver" sabotajı geçer).
3. İlgili kapı onarımdan ÖNCE kırmızı olmalı (kanıt).
4. Onarım: hem C (`src/`) hem self (`selfhost/codegen.kem`) gerekiyorsa İKİSİ;
   C tarafı C'nin aynası ise yorumda belirt. Türkçe dosyalarda `sed/awk/perl -i`
   YASAK (python utf-8 ya da editör; `runtime/kdl_runtime.c` yalnız ikili mod).
5. Kapı yeşil + **sabotaj**: onarımı geri al (dosya yedeğiyle, `git checkout`
   DEĞİL) → kapı kırmızı → geri koy. Sabotaj derlemeyi kırıyorsa geçersizdir.
   Sabotajdan sonra ilgili `.o` ve ikilileri `rm -f` et (`build/codegen`,
   `build/kemgu`, `build/<modül>.o`).
6. Kapılar (hepsi, SERİ — aynı anda iki make koşturma):
   ```bash
   make calistir_codegen_diff calistir_checker_diff calistir_modul_codegen \
        calistir_yapi_diff calistir_surucu_diff calistir_self_driver \
        calistir_sifir_uyari calistir_chatgpt_kopru 2>&1 | tee build/kapi.log; echo "rc=${PIPESTATUS[0]}"
   make test_tumu 2>&1 | tee build/tumu.log; echo "rc=${PIPESTATUS[0]}"
   ```
   `rc=0` bir İDDİADIR: `grep -E "atla|ATLANDI|Basarisiz: [1-9]|FAIL=[1-9]" build/tumu.log`
   ile atlama/başarısızlık say. `self_driver` "FIXPOINT ✓" görmelisin.
7. Commit (`DERLEYICI(codex): CNNN — <tek cümle>`), push, **draft** PR:
   `gh pr create --draft --base main --head codex/derleyici-<kod> --title "DERLEYICI(codex): <kod> — ..." --body-file build/pr_<kod>.md`
   Gövde: kök neden (dosya:satır), önce/sonra ölçüm, fikstür, sabotaj sonucu,
   kapı sonuçları (sayılarla). D-numarası VERME, auto-merge AÇMA.
8. `otomasyon/chatgpt/codex2_ilerleme.md` (worktree'de, her dalda ayrı commit
   değil — son PR'da topla) satırı.

Bir kusurda 60 dk takılırsan: dalı push etme, `codex2_kararlar.md`'ye "TAKILDI:
neden + son ölçüm" yaz, sıradakine geç.

## 5. Yasaklar
- `selfhost/checker.kem` (donduruldu), `LOOP.md`, `CLAUDE.md`, `.github/`.
- Var olan bir fikstürün beklenen çıktısını değiştirmek (yeni fikstür ekle).
  Bir kapının muafiyet listesine satır eklemek.
- Kapıyı yeşile çevirmek için harness'ı düzenlemek.
- main'e push, merge, PR onayı.

## 6. Bitirince
Tüm kusurlar bittiğinde (ya da karar/takılma kaydıyla geçildiğinde) son bir
draft PR'da `codex2_ilerleme.md` + `codex2_kararlar.md`'yi gönder. Özet:
düzeltilen / karar bekleyen / takılan, her PR numarası, toplam süre.
