## İş 14 — x86 zamanlayıcı/kesme donanımı `.kem`'de (kdl_zaman.c x86 portu)

`runtime/kem_zaman.kem` artık mimariden bağımsız: `kem_tik`, `kem_timer_aralik`
küreselleri, `kz_mmio_yaz/oku`, `kem_timer_testi()` (kdl_kesme_kur →
kdl_timer_baslat → kem_tik_oku() >= 3 bekler, guard'lı). arm64 donanımı
`runtime/mimari/arm64/kem_zaman_hw.kem`'de (bağlamda, ÖRNEK desen). x86
karşılığı bugün C'de (`runtime/kdl_zaman.c` x86 bölümü, bağlamda) — x86 kanıt
çekirdeğinin bağlantısından C nesnesi ÇIKARILDI (orkestratör yaptı), bu yüzden
aşağıdaki semboller `.kem`'den gelmek ZORUNDA (`bm_x86_kesme.o` `kdl_tik_al`'ı
çağırır; `start_x86_64.S` IRQ0 koçanı `kdl_kesme_isle(irq)`'yi çağırır ve
PIC EOI'yi KENDİSİ yapar).
DOĞRULANMIŞ SABİTLER (ondalık): PIC1 komut 32, PIC2 komut 160, PIC1 veri 33,
PIC2 veri 161; ICW1 17; ICW2 32 (PIC1) / 40 (PIC2); ICW3 4 / 2; ICW4 1;
maske 254 (PIC1, yalnız IRQ0) / 255 (PIC2). PIT komut portu 67, kanal 0 portu
64, mod baytı 54; bölen 11932 → düşük bayt 156, yüksek bayt 46 (~100 Hz).

### Değişiklikler (yalnız bunlar — 3 madde)
1. **YENİ `runtime/mimari/x86_64/kem_zaman_hw.kem`** — Türkçe başlık (D2b,
   kdl_zaman.c x86 portu, arm64 ikizi). Çıplak işlevler (C ile AYNI sıra ve
   değerler; port yazımı `kem_port_yaz8(port olarak dtam16, değer olarak dtam8)` —
   `runtime/mimari/x86_64/kem_uart.kem`'de, aynı CAT biriminde):
   - `kdl_kesme_kur()` — PIC remap 10 port yazımı (C'deki sırayla).
   - `kdl_timer_baslat()` — PIT: 67←54, 64←156, 64←46; sonra `kz_irq_ac()`.
   - `kz_irq_ac()` — `satıriçi_asm { mimari: x86_64 şablon: r#"sti"# bozulan("~{memory}") }`.
   - `kdl_kesme_isle(irq: tam64)` — `kem_tik = kem_tik + 1;` (yalnız IRQ0 açık).
   - `kdl_tik_al() -> tam64` — `ver kem_tik_oku();`
   - `kem_tik_oku() -> tam64` — IRQ'da yazılan sayacı TAZE oku:
     `satıriçi_asm { mimari: x86_64 şablon: r#"movq kem_tik(%rip), $0"# çıktı("=r", &x) bozulan("~{memory}") }`.
   - `kdl_idle()` — `hlt` asm (C ile parite).
   asm'ler `güvensiz { }` içinde.
2. **`test/ornekler/kem_os_x86.kem`** — `kem_panik("X86 PANIK YOLU OK");` ÖNCESİNE:
   `değişken t_ok: tam32 = kem_timer_testi();` 1 ise `X86 TIMER TIK OK tik=` +
   ayrı satırda `kem_tik_oku() olarak tam32` (sayıdan sonra `yazdir_satir()`;
   `yazdir_metin` zaten satır sonu ekler — araya `yazdir_satir` KOYMA); değilse
   `X86 TIMER HATA`. Başlık yorumuna ekle (gerçek IRQ0: PIT → PIC → IDT vektör
   32 → .S koçanı → .kem kdl_kesme_isle).
   ÇAPA: `    kem_panik("X86 PANIK YOLU OK");`
3. **`Makefile` Kanıt 15** — Kanıt 14'ün son satırından SONRA (çapa, iki TAB):
   `		echo "FAIL: <kdtb_adres>: etiketi x86 biriminde yok"; exit 1; fi`
   - `X86 TIMER TIK OK tik=` TAM satırı + ardından gelen satır `v` ile
     `[ "$$v" -ge 3 ]` (gerçek IRQ ilerletti); yoksa FAIL; değeri yazdır.
   - `llvm-nm $(BUILD)/kem_os_x86.o` içinde ` T kdl_kesme_isle` ve ` T kdl_tik_al`
     OLMALI (C değil .kem sağlıyor); yoksa FAIL.
   - Hedef yorumuna Kanıt 15 (çapa: `#          basılır ve objdump'da <kdtb_adres>: etiketi x86 biriminde bulunur.`).

Blok yolları yalnız: `runtime/mimari/x86_64/kem_zaman_hw.kem`,
`test/ornekler/kem_os_x86.kem`, `Makefile`.
