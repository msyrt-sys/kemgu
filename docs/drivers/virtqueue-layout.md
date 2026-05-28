# Virtqueue Layout Validation

Bu not, VirtIO virtqueue layout dogrulamasinin saf KEMGU modelini anlatir. Bu PR
gercek MMIO register erisimi, DMA allocation, virtio-blk init, interrupt veya
read/write request handling yapmaz. Amac, sonraki transport ve blok cihaz
islerinde tekrar kullanilacak queue size ve hizalama kurallarini kucuk, test
edilebilir bir yuzeye indirmektir.

Model split virtqueue icin descriptor table, available ring ve used ring byte
boyutlarini hesaplar. Descriptor elemani 16 byte, available ring tabani 6 byte
ve her ring elemani 2 byte, used ring tabani 6 byte ve her used elemani 8 byte
kabul edilir. Toplam layout boyutu descriptor bolgesinden sonra available ring
boyutunu ekler, used ring baslangicini 4 byte sinirina yukari yuvarlar ve used
ring boyutunu ekleyerek bulunur.

Queue size validator su kurallari uygular: `queue_num_max == 0` queue yok
anlamina gelir ve gecersizdir; `queue_size == 0` gecersizdir; `queue_size`,
cihazin bildirdigi `queue_num_max` degerinden buyuk olamaz; ve `queue_size`
iki kuvveti olmak zorundadir. Minimum gecerli boyut `1` olarak modellenir.
Tipik gecerli degerler `1, 2, 4, 8, 16, 128, 256`; tipik gecersiz degerler
`0, 3, 6, 7, 12, 255` seklindedir.

Alignment helper'lari saf boolean validator olarak tutuldu: descriptor table
16 byte, available ring 2 byte, used ring 4 byte hizali olmalidir. Bu PR
fiziksel adres tahsisi veya register yazimi yapmadigi icin sadece verilen
adreslerin hizalama kosullarini denetler.

KEMGU tarafinda overflow-safe aritmetik icin yerlesik bir yardimci bu kapsamda
kullanilmadi. Bu nedenle layout hesaplari `virtqueue_layout_queue_size_ust_sinir()`
ile sinirlandirildi ve kodda `TODO(codex-next-pr): overflow-safe layout arithmetic`
notu birakildi. Sonraki dogal adim, bu saf modeli feature negotiation veya
VirtIO MMIO transport init akisi icinde kullanmaktir.
