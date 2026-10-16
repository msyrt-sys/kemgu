## Temizlik — iki değişiklik, yalnız `Makefile`

Bağlamda `Makefile`'ın iki KESİTİ var (başlıklarında satır aralığı yazılı).
Blok yolu HER İKİSİNDE de `Makefile` olmalı.

### 1. `export DEBUGINFOD_URLS :=` (hermetik test ortamı)
ÖLÇÜLDÜ (2026-10-12): Ubuntu kabuk ortamı `DEBUGINFOD_URLS=https://debuginfod.ubuntu.com`
taşıyor. ASan/LSan bir rapor (ör. bilinen 8 baytlık kanal sızıntısı) basarken
`llvm-symbolizer` bu sunucuya AĞDAN gidiyor; erişim asılınca ASan ikilisi
`timeout 60`ı aşıyor → `calistir_kanal_omru` 3/3 `rc=124` (sahte kırmızı).
Değişken boşken sembolleştirici anında yanıtlıyor; kod aynı. Bir test
takımının sonucu dış bir sunucunun erişilebilirliğine bağlı olamaz.

İstenen: `export EXE` satırının ALTINA (araya bir boş satır bırak, sonra
mevcut `# [D-476] ...` bloğu gelsin) — Türkçe yorum bloğu (yukarıdaki ölçümü
3-6 satırda özetle, `[D-676]` etiketiyle; Makefile'ın bu bölgesindeki
yorumlar ASCII Türkçe yazılmış — `cikti`, `olculdu` gibi; aynı üslubu izle)
ve ardından tam olarak: `export DEBUGINFOD_URLS :=`

ÇAPA (ESKI, birebir; TEK kez geçer):
```
export EXE

# [D-476] ASan + YUKSEK ASLR ENTROPISI
```
YENI'de `export EXE` ve `# [D-476] ASan + YUKSEK ASLR ENTROPISI` satırları
AYNEN korunmalı; yeni blok aralarına girer.

### 2. Ölü hedef `$(BUILD)/bm_a64_kem_heap.o` — SİL
ÖLÇÜLDÜ: tek referansı kendi tanımı (iki başka geçiş yalnız `@#` yorumu).
Bugün `make build/bm_a64_kem_heap.o` → `rc=0` ama `build/kem_heap.ll` 0
bayt ve nesne 0 sembollü: `kem_heap.kem` artık tek başına T002 verir ve
`2>/dev/null | awk` boru hattı bunu YUTAR (D-609 sınıfı) → SESSİZ boş nesne.
Hiçbir hedef kullanmıyor.

İstenen: aşağıdaki 7 satırı (3 yorum + hedef + 3 tarif satırı) tek satırlık bir
yorumla değiştir: `# [D-676] bm_a64_kem_heap.o KALDIRILDI: tuketicisi yoktu ve
kem_heap tek basina T002 verdigi icin SESSIZCE bos nesne uretiyordu (D-609).`
(tek satıra sığmazsa iki `#` satırı).

ÇAPA (ESKI, birebir; TEK kez geçer; tarif satırları TAB ile başlar):
```
# K1/K3 (D-260/261): SAF-.kem çıplak runtime → @malloc/@free/@kem_heap_kur (K1) +
# @kdl_bolge_olustur/ayir/serbest (K3). kemgu --llvm boilerplate `declare`ları define
# ile çakışır (LLVM redef) → STRIP + bare-metal target. Circularity yok (çıplak).
$(BUILD)/bm_a64_kem_heap.o: runtime/kem_heap.kem test/strip_defined_declares.awk $(BUILD)/kemgu$(EXE) | $(BUILD)
	./$(BUILD)/kemgu$(EXE) --llvm runtime/kem_heap.kem 2>/dev/null \
	  | awk -f test/strip_defined_declares.awk > $(BUILD)/kem_heap.ll
	$(BM_A64) -O2 -Wno-override-module -x ir $(BUILD)/kem_heap.ll -c -o $@
```

### Yasak
`Makefile` dışında hiçbir dosya. Başka hedeflere dokunma.
