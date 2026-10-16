## İş 9 — PVH `start_info` adresini sakla ve x86'da oku (aarch64 `kdl_dtb_adres` ikizi)

PVH ABI: QEMU 32-bit `pvh_entry`'ye atlarken `%ebx` = `struct hvm_start_info`
fiziksel adresi. `start_x86_64.S` bunu SAKLAMIYOR (aarch64'te `_start` X0'daki
DTB adresini `.data`'daki `kdl_dtb_adres`'e yazar — bkz. AH-1 yorumu).
DOĞRULANMIŞ SABİTLER (Linux `xen/interface/hvm/start_info.h` + QEMU `pvh.bin`):
- `magic` @0 (u32) = 0x336ec578 = **862897528**; `version` @4 (u32)
- `memmap_paddr` @40 (u64); `memmap_entries` @48 (u32)  (version >= 1)
- memmap girdisi 24 bayt: `addr` @0 (u64), `size` @8 (u64), `type` @16 (u32); RAM türü = **1**

### Değişiklikler (yalnız bunlar — 5 madde)
1. **`boot/start_x86_64.S`**:
   - `pvh_entry:` / `cli` / `cld` satırlarından HEMEN SONRA (BSS temizliğinden
     ÖNCE, `%ebx` henüz bozulmadan): `movl %ebx, kdl_pvh_bilgi`. Yorum: PVH ABI,
     aarch64 ikizi, neden ilk iş.
   - `/* === Sayfa tabloları (.bss, ...` satırından HEMEN ÖNCE yeni bölüm:
     `.section .data` / `.align 8` / `.global kdl_pvh_bilgi` /
     `kdl_pvh_bilgi:` / `.quad 0` — yorum: `.data`da çünkü `.bss` boot'ta
     SIFIRLANIR (aarch64 `kdl_dtb_adres` ile aynı gerekçe); yüksek dword 0 kalır
     (adres < 4 GB).
   ÇAPALAR: `pvh_entry:` + `    cli` + `    cld` (3 satır, birebir) ve
   `/* === Sayfa tabloları (.bss, 4KB-hizalı; boot'ta sıfırlanıp kurulur) === */`
2. **YENİ `runtime/mimari/x86_64/kem_pvh.kem`** (Türkçe başlık yorumu, D2b):
   - `çıplak işlev kem_pvh_bilgi() -> tam64`: `güvensiz { satıriçi_asm { mimari: x86_64
     şablon: r#"movq kdl_pvh_bilgi(%rip), $0"# çıktı("=r", &v) } }` ile oku, `ver v;`
   - `çıplak işlev kem_pvh_ram_toplam() -> tam64`: `b = kem_pvh_bilgi()`;
     b == 0 → `ver 0`; `kdl_mmio_oku32(b)` ≠ 862897528 → `ver 0 - 1`;
     `kdl_mmio_oku32(b + 4)` < 1 → `ver 0 - 2`; `mm` = u64 @ b+40, `n` = u32 @ b+48;
     i=0..n-1: girdi `e = mm + i*24`, `type` (u32 @ e+16) == 1 ise `size` (u64 @ e+8)
     toplama ekle; toplamı döndür. u64 okumak için çıplak deref:
     `değişken p: *tam64 = adr olarak *tam64; ver *p;` (yardımcı `kem_pvh_oku64`).
3. **`test/ornekler/kem_os_x86.kem`**: `kem_panik("X86 PANIK YOLU OK");` satırından
   ÖNCE: `değişken ram: tam64 = kem_pvh_ram_toplam();` ram ≤ 0 → `X86 PVH HATA` +
   değer (`olarak tam32` ile `yazdir_tam`); değilse `X86 PVH OK ram_mb=` + ayrı satırda
   `(ram / 1048576) olarak tam32`. Başlık yorumuna ekle.
   ÇAPA: `    kem_panik("X86 PANIK YOLU OK");`
4. **CAT listeleri**: Makefile x86 CAT'te `test/ornekler/kem_os_x86.kem`'den hemen ÖNCE
   `runtime/mimari/x86_64/kem_pvh.kem`; check_kapisi KEMOS_X86_PARCALAR'da aynı yer;
   mesajda `8 parça` → `9 parça`.
   ÇAPALAR: Makefile `runtime/kem_minifs.kem runtime/kem_virtio_net.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem`;
   check_kapisi `                    runtime/kem_minifs.kem runtime/kem_virtio_net.kem test/ornekler/kem_os_x86.kem"`
   ve `kem_os_x86 (birleşik kaynak, 8 parça, --mimari x86_64)`.
5. **Makefile Kanıt 12**: Kanıt 11'in son satırından SONRA: `X86 PVH OK ram_mb=` TAM satırı
   (`grep -qxF`) + ardından gelen satır `^[1-9][0-9]*$$`; ayrıca `llvm-objdump -d
   --no-show-raw-insn --disassemble-symbols=pvh_entry $(BUILD)/kem_os_x86.elf` içinde
   `%ebx` ile başlayan `movl` satırı (OLDUĞU GİBİ yazdır); yoksa FAIL.
   ÇAPA: `		echo "FAIL: FS OK sonrasi pozitif sayi satiri yok"; exit 1; fi` (iki TAB).
   Hedef yorumuna Kanıt 12 (çapa: `#          sektör 0/1/2 üstünde format→yaz→oku davranışsal kanıtı).`).

Blok yolları yalnız: `boot/start_x86_64.S`, `runtime/mimari/x86_64/kem_pvh.kem`,
`test/ornekler/kem_os_x86.kem`, `Makefile`, `test/check_kapisi.sh`.
