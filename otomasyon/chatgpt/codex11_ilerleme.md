# Dalga 11 — kesme altyapısı

Başlangıç: 2026-11-13T19:27:07.346773
Taban: origin/main 26e9546; D-756 mevcut.

- Taban: panik, PS/2, UEFI kabuk ve kem_os_x86 kapıları seri, rc=0. UEFI 3 RAM × 5 açılış; PS/2 3 disk × 5 + xHCI’siz 2 açılış.
- A/B kırmızı başlangıç: bozuk RSP + UD2, eski IST=0; QEMU `-no-reboot` ile rc=0 kapanıyor, yalnız açılış işareti var; yeni kapı rc=1.
- A/B yeni kapı: C + self, 7 şekil × 10 açılış = 140/140 yeşil (rc=0). Sabotaj: IST1=0 → 10/10 kırmızı; ekran bayrağı → 1/1 kırmızı; boş OOB kancası → 1/1 kırmızı. Her değişiklik geri alındı.
- A/B regresyon: seri kapılar rc=0 (`build/dalga11_ab_kapilar.log`, PIPESTATUS[0]). İstisna kapısı yeniden 140/140; köprü, panik, PS/2, kabuk, UEFI kabuk/PC, x86 çekirdek, ARM64 24 faz ve qemu_cekirdek yeşil. check_kapisi: 351/369, 18 sınıflandırılmış, 0 kırmızı; baremetal_diff: 23/23, 0 atlama. Ring3 ve kurtarılabilir #PF korunuyor.
