## İş 11 — `baremetal_diff`'e x86 İKİZİ: birleşik x86 birimi C ↔ self-host yapısal parite

Ölçüldü (bugün, elle): x86 birleşik birimi `--mimari x86_64` ile C oracle ve
self-host'ta BİREBİR — 72 define (ad+dönüş tipi), 4 asm (şablon+kısıt dizgisi
aynı), 79 volatile, 83 inttoptr, üçlü aynı. Ama bunu HİÇBİR kapı ölçmüyor.
Yalnız `test/baremetal_diff_harness.sh` değişir (bağlamda tamamı var).

### Değişiklikler (3 madde)
1. **Yeni bölüm (5)** — `# ---- (4) SINIFLANDIRMA BÜTÜNLÜĞÜ ...` satırından HEMEN ÖNCE ekle:
   - Parça listesi TEK KAYNAKTAN: Makefile'daki x86 CAT satırından oku:
     `X86_PARCA=$(sed -n 's/.*cat \(runtime\/kem_heap\.kem.*kem_os_x86\.kem\) > \$(BUILD)\/kem_os_x86_comb\.kem/\1/p' Makefile)`
     Boşsa → `🔴 BİRLEŞİK x86 — Makefile'dan parça listesi okunamadı`, fail++.
     Listedeki dosyalardan biri yoksa → 🔴 + fail++.
   - `cat $X86_PARCA > "$TMP/x86.kem"`; C: `"$KEMGU" --llvm --mimari x86_64` (stderr
     `$TMP/x86_c.err`, rc al); self: `"$CODEGEN" --llvm --mimari x86_64`.
     C rc≠0 / ilk satırda `hata[` / boş → `🔴 BİRLEŞİK x86 — C oracle DERLEYEMEDİ`
     (+ ilk 3 `hata[` satırı), fail++ (ATLAMA YOK). Self boş → 🔴, fail++.
   - Karşılaştır (bölüm (1) ile aynı yöntem): define kümesi (`sed 's/(.*//' | sort`),
     üçlü, `asm sideeffect` sayısı, volatile sayısı (C > 0 şart), inttoptr sayısı
     (C > 0 şart) VE ek olarak asm şablon+kısıt kümesi:
     `grep -o 'asm sideeffect "[^"]*", "[^"]*"' | sort` iki tarafta `diff -q` eşit.
   - Geçerse `✅ BİRLEŞİK x86 — N işlev, A satıriçi asm (şablon+kısıt eşleşti), V volatile, I inttoptr, üçlü eşleşti`,
     pass++; değilse ayrıntılı 🔴 (ilk 5 diff satırı), fail++.
   - Bölüm başına Türkçe yorum: neden (D-674 sonrası x86 birimi var ama C↔self
     paritesini ölçen kapı yoktu), parça listesinin Makefile'dan okunması
     (liste kayması olmasın), üçlünün bugün `x86_64-pc-windows-gnu` olduğu
     (D1 açık; eşitlik yine ölçülür).
2. **Bölüm (4) sınıflandırma** — `runtime/mimari/x86_64/*.kem` dosyaları da
   denetlensin: her biri `$X86_PARCA` içinde geçmeli; geçmeyen → `🔴 <dosya> —
   SINIFLANDIRILMAMIŞ: Makefile calistir_kem_os_x86 CAT listesine ekle`, fail++.
   Bölüm (4)'ün başındaki "x86_64 bu kapının dışında" yorumunu buna göre güncelle.
   ÇAPA (3 satır, birebir):
   `# runtime/mimari/x86_64/*.kem bu kapının dışında (aarch64 hedefi AS001 verir);`
   `# onlar check_kapisi (birleşik x86, --mimari x86_64) + calistir_kem_os_x86'da.`
   `for f in runtime/*.kem runtime/mimari/arm64/*.kem; do`
   (YENI'de mevcut arm64 döngüsü AYNEN kalsın; x86 denetimi ayrı küçük döngü olarak
   onun ARDINA — yani mevcut döngünün `done` satırından sonra — gelsin. Bunu
   yapabilmek için çapayı döngünün tamamını kapsayacak şekilde genişletme;
   x86 döngüsünü bölüm (5)'in SONUNA koyman da kabul.)
3. **Özet satırı**: `(aarch64)` → `(aarch64 + x86_64)`.
   ÇAPA: `echo "=== bare-metal (aarch64) yapı paritesi: $pass/$((pass+fail)) (birleşik OS + birimler + parça kapsamı: $kapsam_islev işlev, $kapsam_kuresel küresel), 0 atlandı ==="`

Blok yolu YALNIZ `test/baremetal_diff_harness.sh`. bash, `set -u` altında çalışır.
