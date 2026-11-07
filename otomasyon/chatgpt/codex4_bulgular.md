# Dalga 4 bulguları

## Derleyici bulguları

## QEMU’da doğrulanacak

Gerçek donanım doğrulaması bu saf mantık dalgasının dışındadır. Kaynağı bulunamayan alanlar aşağıya ayrı ayrı kaydedilecektir.

- A0: `virtio_blk_init_test.kem` seçili `blk_cfg_adres` ithalatı eklendi. C/self --check ve çalıştırma temiz. `surucu_diff` muafiyeti artık kaldırılabilir; harness yasağı gereği değiştirilmedi.
