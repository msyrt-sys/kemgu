# Dalga 11 — kesme altyapısı

Başlangıç: 2026-11-13T19:27:07.346773
Taban: origin/main 26e9546; D-756 mevcut.

- Taban: panik, PS/2, UEFI kabuk ve kem_os_x86 kapıları seri, rc=0. UEFI 3 RAM × 5 açılış; PS/2 3 disk × 5 + xHCI’siz 2 açılış.
- A/B kırmızı başlangıç: bozuk RSP + UD2, eski IST=0; QEMU `-no-reboot` ile rc=0 kapanıyor, yalnız açılış işareti var; yeni kapı rc=1.
- A/B yeni kapı: C + self, 7 şekil × 10 açılış = 140/140 yeşil (rc=0). Sabotaj: IST1=0 → 10/10 kırmızı; ekran bayrağı → 1/1 kırmızı; boş OOB kancası → 1/1 kırmızı. Her değişiklik geri alındı.
- A/B regresyon: seri kapılar rc=0 (`build/dalga11_ab_kapilar.log`, PIPESTATUS[0]). İstisna kapısı yeniden 140/140; köprü, panik, PS/2, kabuk, UEFI kabuk/PC, x86 çekirdek, ARM64 24 faz ve qemu_cekirdek yeşil. check_kapisi: 351/369, 18 sınıflandırılmış, 0 kırmızı; baremetal_diff: 23/23, 0 atlama. Ring3 ve kurtarılabilir #PF korunuyor.
- C kırmızı başlangıç: eski UEFI çekirdeğinde `UEFI LAPIC yolu kurulmadı`, rc=1. Yeni MADT/GSI testi C+self yeşil. Gerçek APIC ölçümü C 10/10, self 10/10; her açılışta 8 saniye boşta LAPIC ilerliyor, PIT duruyor, PIC maskeli. ISO sabotajı saf testi ve 10/10 gerçek açılışı kırmızı yaptı (PIT ISO zaman aşımı); kaynak geri alındı. Seri regresyon tamamlandı: rc=0 (`build/dalga11_c_kapilar.log`, PIPESTATUS[0]).
- [x] A/B · 7 şekil × 2 derleyici × 10 açılış; regresyonda yeniden 140/140 · sabotaj: IST1=0 10/10, ekran kapalı 1/1, OOB kancası boş 1/1 kırmızı · 20:08, commit c9e978c.
- [x] C · MADT/IOAPIC/LAPIC, C+self yeni kapı 20/20; köprü 135 test, tüm §5 regresyon kapıları yeşil; check 351/369 (18 sınıflandırılmış), baremetal_diff 23/23 (0 atlama), x86 noredzone 462/462 · ISO sabotajı 10/10 kırmızı · 2026-11-13T20:45:08
- D kırmızı başlangıç: eski yoklamalı yol 7 komutu geçti, yeni IRQ kâhini `PS/2 kesme yolu kurulmadı` ile rc=1. İlk yeni gerçek koşum: tüm IRQ/halka/fare/piksel denetimleri rc=0; saf fare testi C+self yeşil. Tekrarlar ve sabotajlar sırada.

- D yeni kapı: C 10/10 + self 10/10 yeşil (`build/dalga11_d_yesil.log`, rc=0). IRQ12 maskesi 10/10, senkron denetimini kaldırma 10/10 gerçek açılışta kırmızı; ikincisi saf fare testini de kırmızı yaptı. Kaynaklar geri alındı ve D seri regresyonu başladı.

- VARSAYIM: #DF/NMI/#MC için ayrı 16 KiB IST ve alt kanarya; PVH PIC yolu korunur. UEFI PS/2 yolu IRQ kullanır; USB klavye mevcutsa mevcut USB giriş seçimi korunur, PS/2 fare IRQ ile beraber çalışır. Tekerlek ve tam imleç altyapısı bu dalgada genişletilmedi.

- D seri regresyonu rc=0 (`build/dalga11_d_kapilar_komut.log`, PIPESTATUS[0]): IRQ20/20, APIC20/20, istisna140/140; köprü 135 test + dil kartı, eski panik/PS2/USB kabuk/UEFI kabuk/PC, x86 ve ARM kapıları yeşil. baremetal_diff23/23, 0 atlama, x86 noredzone480/480. Karma USB klavye + USB disk + PS/2 fare etkileşimi ek ölçülüyor.

- VARSAYIM: opak port asm’inin statik gerçekzamanlı bütçesi port başına 1024 çevrim; bu fiziksel WCET ölçümü değildir. Ayrıntı ve QEMU kalibrasyon ölçümleri bulgular dosyasında.

- [x] D · PS/2 IRQ C+self20/20, USB klavye+USB FAT32+PS/2 fare C+self20/20; seri regresyon rc=0. Sabotaj: IRQ12 maskesi10/10, senkron guard10/10, USB bekleyişi10/10 kırmızı. Kaynak geri alındı; iki giriş biçimi yeniden yeşil. C11-001 açık tam64 dönüşümüyle dolanıldı · 2026-11-13T22:45:45
