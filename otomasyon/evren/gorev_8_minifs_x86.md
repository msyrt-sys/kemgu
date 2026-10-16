## İş 8 — `kem_minifs` x86'da işlevsel test (microvm diski üstünde)

Bugün x86 kanıt çekirdeği `kem_minifs.kem`'i İÇERMİYOR; minifs yalnız aarch64'te
test ediliyor. Fonksiyonlar (bağlamdaki `runtime/kem_minifs.kem`):
`mfs_format(base) -> tam32`, `mfs_dosya_yaz(base, a0,a1,a2,a3: dtam8, boyut: tam32) -> tam32`
(İÇERİK ÖNCEDEN vq_data'da, `vblk_data_yaz` ile), `mfs_dosya_oku(base, a0..a3) -> tam32`
(boyut ya da negatif hata; içerik vq_data'ya okunur, `vblk_data_oku` ile alınır).
Sektörler 0/1/2 kullanılır; disk testi sektör 7 → çakışma yok.

### Değişiklikler (yalnız bunlar — 5 madde)
1. **Makefile x86 CAT**: `runtime/kem_virtio_blk.kem`'den hemen SONRA `runtime/kem_minifs.kem`.
   ÇAPA: `runtime/kem_virtio_blk.kem runtime/kem_virtio_net.kem test/ornekler/kem_os_x86.kem > $(BUILD)/kem_os_x86_comb.kem`
2. **test/check_kapisi.sh KEMOS_X86_PARCALAR**: aynı yere `runtime/kem_minifs.kem`; mesajda `7 parça` → `8 parça`.
   ÇAPALAR: `                    runtime/kem_virtio_net.kem test/ornekler/kem_os_x86.kem"` (minifs'i bu satırın BAŞINA ekle)
   ve `kem_os_x86 (birleşik kaynak, 7 parça, --mimari x86_64)`.
3. **test/ornekler/kem_os_x86.kem**: `X86 DISK RW OK` + toplam basıldıktan HEMEN SONRA (aynı blok içinde, `vb` geçerli):
   - `mfs_format(vb)` ≠ 0 → `X86 FS RW HATA` + kod.
   - İçerik: i=0..99 için `vblk_data_yaz(i, (((i % 89) + 11) olarak dtam8))`;
     `mfs_dosya_yaz(vb, 75 olarak dtam8, 69 olarak dtam8, 77 olarak dtam8, 71 olarak dtam8, 100)` (ad "KEMG"); ≠ 0 → HATA + kod.
   - `vblk_data_yaz` ile vq_data'nın 0..511'ini SIFIRLA (okunan içerik diskten gelsin).
   - `değişken fb: tam32 = mfs_dosya_oku(vb, 75 olarak dtam8, 69 olarak dtam8, 77 olarak dtam8, 71 olarak dtam8)`;
     fb ≠ 100 → `X86 FS RW HATA boyut=` + fb. Değilse i=0..99 `vblk_data_oku(i)` desenle karşılaştır;
     hepsi eşleşirse `X86 FS RW OK` + ayrı satırda baytların toplamı (`yazdir_tam`), yoksa HATA + ilk indeks.
   ÇAPA (3 satır, 24 boşluk girinti):
   `                        yazdir_metin("X86 DISK RW OK");` / `                        yazdir_tam(dsum);` / `                        yazdir_satir();`
   (YENI'de bu 3 satır AYNEN kalır, yeni kod ARDINA gelir.)
4. **Makefile Kanıt 11**: Kanıt 10'un son satırından SONRA: `X86 FS RW OK` TAM satırı (`grep -qxF`) ve ardından gelen satır
   `^[1-9][0-9]*$$` olmalı; yoksa FAIL. ÇAPA: `		echo "FAIL: 'X86 NET DEV OK mac_w0=' satiri yok"; exit 1; fi` (iki TAB).
5. **Makefile hedef yorumu**: Kanıt 11 satırı ekle. ÇAPA: `#          mac_w0=" TAM satırı (microvm virtio-mmio disk/ağ davranışı).`

Blok yolları: `Makefile`, `test/check_kapisi.sh`, `test/ornekler/kem_os_x86.kem`. Başka dosya YOK.
