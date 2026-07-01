/*
 * KEMGU Bare-Metal VirtIO-Blk Sürücüsü (kdl_virtio.c) — C5 gerçek disk depolama.
 * =============================================================================
 *
 * QEMU virt makinesi virtio-mmio transport'ları 0x0a000000'de (32 slot × 0x200).
 * Bir virtio-blk-device eklenince slotlardan birine bağlanır → tarayıp buluruz.
 * Bu sürücü GERÇEK diskten (QEMU -drive) 512-baytlık blok okur → RAM dosya
 * sistemini (D-131) kalıcı depolamaya bağlamanın ilk adımı (Faz E).
 *
 * VirtIO-MMIO v2 (modern). Split virtqueue: desc[] + avail + used (ayrı hizalı
 * tamponlar). DMA tamponları RAM'de (identity-map → VA=PA, cihaz fiziksel adres
 * kullanır). QEMU DMA'sı coherent → cache-flush gerekmez (dsb ordering yeter).
 *
 * Register offsetleri: drivers/virtio/constants.kem ile aynı (tek kaynak bilgi).
 * Freestanding (libc yok). Yalnız aarch64 (QEMU virt).
 */
#include <stdint.h>

#if defined(__aarch64__)

/* VirtIO-MMIO v2 register offsetleri (constants.kem). */
#define VMMIO_MAGIC        0x000
#define VMMIO_VERSION      0x004
#define VMMIO_DEVICE_ID    0x008
#define VMMIO_DEV_FEAT     0x010
#define VMMIO_DEV_FEAT_SEL 0x014
#define VMMIO_DRV_FEAT     0x020
#define VMMIO_DRV_FEAT_SEL 0x024
#define VMMIO_QUEUE_SEL    0x030
#define VMMIO_QUEUE_NUM_MAX 0x034
#define VMMIO_QUEUE_NUM    0x038
#define VMMIO_QUEUE_READY  0x044
#define VMMIO_QUEUE_NOTIFY 0x050
#define VMMIO_STATUS       0x070
#define VMMIO_Q_DESC_LO    0x080
#define VMMIO_Q_DESC_HI    0x084
#define VMMIO_Q_DRV_LO     0x090
#define VMMIO_Q_DRV_HI     0x094
#define VMMIO_Q_DEV_LO     0x0a0
#define VMMIO_Q_DEV_HI     0x0a4

#define VIRTIO_MAGIC       0x74726976   /* "virt" */
#define VIRTIO_DEV_BLK     2

#define ST_ACK      1
#define ST_DRIVER   2
#define ST_DRIVER_OK 4
#define ST_FEAT_OK  8

#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

#define VQ_N 8   /* virtqueue boyutu (2'nin kuvveti, <= QueueNumMax) */

struct virtq_desc { uint64_t addr; uint32_t len; uint16_t flags; uint16_t next; };
struct virtq_avail { uint16_t flags; uint16_t idx; uint16_t ring[VQ_N]; uint16_t used_event; };
struct virtq_used_elem { uint32_t id; uint32_t len; };
struct virtq_used { uint16_t flags; uint16_t idx; struct virtq_used_elem ring[VQ_N]; uint16_t avail_event; };

/* virtio-blk istek başlığı (16 bayt): type + reserved + sector. */
struct blk_req { uint32_t type; uint32_t reserved; uint64_t sector; };

/* DMA tamponları (RAM, identity-map, hizalı — cihaz fiziksel adresle erişir). */
static struct virtq_desc  vq_desc[VQ_N]  __attribute__((aligned(16)));
static struct virtq_avail vq_avail        __attribute__((aligned(16)));
static struct virtq_used  vq_used         __attribute__((aligned(16)));
static struct blk_req     vq_req          __attribute__((aligned(16)));
static uint8_t            vq_data[512]     __attribute__((aligned(16)));
static uint8_t            vq_status        __attribute__((aligned(16)));

static inline uint32_t mmio_r32(uint64_t base, uint64_t off) {
    return *(volatile uint32_t *)(uintptr_t)(base + off);
}
static inline void mmio_w32(uint64_t base, uint64_t off, uint32_t v) {
    *(volatile uint32_t *)(uintptr_t)(base + off) = v;
}

/* virtio-blk cihazını tara (DeviceID=2). Taban adresi döner (0 = yok). */
uint64_t kdl_virtio_blk_bul(void) {
    for (int i = 0; i < 32; i++) {
        uint64_t base = 0x0a000000UL + (uint64_t)i * 0x200UL;
        if (mmio_r32(base, VMMIO_MAGIC) == VIRTIO_MAGIC &&
            mmio_r32(base, VMMIO_DEVICE_ID) == VIRTIO_DEV_BLK) {
            return base;
        }
    }
    return 0;
}

/* Cihazı başlat: reset → feature negosiasyon (VERSION_1) → virtqueue 0 → DRIVER_OK.
 * 0 = ok; negatif = hata (versiyon/feature/queue). */
int kdl_virtio_blk_kur(uint64_t base) {
    if (mmio_r32(base, VMMIO_VERSION) != 2) return -1;   /* v2 (modern) bekliyoruz */

    mmio_w32(base, VMMIO_STATUS, 0);                     /* reset */
    mmio_w32(base, VMMIO_STATUS, ST_ACK);
    mmio_w32(base, VMMIO_STATUS, ST_ACK | ST_DRIVER);

    /* Feature: yalnız VIRTIO_F_VERSION_1 (bit 32) kabul et; düşük feature yok. */
    mmio_w32(base, VMMIO_DRV_FEAT_SEL, 1);
    mmio_w32(base, VMMIO_DRV_FEAT, 1);                   /* bit 32 = VERSION_1 */
    mmio_w32(base, VMMIO_DRV_FEAT_SEL, 0);
    mmio_w32(base, VMMIO_DRV_FEAT, 0);

    mmio_w32(base, VMMIO_STATUS, ST_ACK | ST_DRIVER | ST_FEAT_OK);
    if (!(mmio_r32(base, VMMIO_STATUS) & ST_FEAT_OK)) return -2;   /* feature reddi */

    mmio_w32(base, VMMIO_QUEUE_SEL, 0);
    if (mmio_r32(base, VMMIO_QUEUE_NUM_MAX) < VQ_N) return -3;
    mmio_w32(base, VMMIO_QUEUE_NUM, VQ_N);

    uint64_t da = (uint64_t)(uintptr_t)vq_desc;
    uint64_t av = (uint64_t)(uintptr_t)&vq_avail;
    uint64_t us = (uint64_t)(uintptr_t)&vq_used;
    mmio_w32(base, VMMIO_Q_DESC_LO, (uint32_t)da);  mmio_w32(base, VMMIO_Q_DESC_HI, (uint32_t)(da >> 32));
    mmio_w32(base, VMMIO_Q_DRV_LO,  (uint32_t)av);  mmio_w32(base, VMMIO_Q_DRV_HI,  (uint32_t)(av >> 32));
    mmio_w32(base, VMMIO_Q_DEV_LO,  (uint32_t)us);  mmio_w32(base, VMMIO_Q_DEV_HI,  (uint32_t)(us >> 32));

    mmio_w32(base, VMMIO_QUEUE_READY, 1);
    mmio_w32(base, VMMIO_STATUS, ST_ACK | ST_DRIVER | ST_FEAT_OK | ST_DRIVER_OK);
    return 0;
}

/* `sektor` numaralı 512-baytlık bloğu `hedef`e oku. 0 = ok; negatif = hata. */
int kdl_virtio_blk_oku(uint64_t base, uint64_t sektor, uint8_t *hedef) {
    vq_req.type = 0;            /* VIRTIO_BLK_T_IN = oku */
    vq_req.reserved = 0;
    vq_req.sector = sektor;
    vq_status = 0xff;

    /* 3-descriptor zinciri: başlık(RO) → veri(cihaz-yazar) → durum(cihaz-yazar). */
    vq_desc[0].addr = (uint64_t)(uintptr_t)&vq_req;    vq_desc[0].len = 16;  vq_desc[0].flags = VRING_DESC_F_NEXT; vq_desc[0].next = 1;
    vq_desc[1].addr = (uint64_t)(uintptr_t)vq_data;    vq_desc[1].len = 512; vq_desc[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE; vq_desc[1].next = 2;
    vq_desc[2].addr = (uint64_t)(uintptr_t)&vq_status; vq_desc[2].len = 1;   vq_desc[2].flags = VRING_DESC_F_WRITE; vq_desc[2].next = 0;

    uint16_t onceki = vq_used.idx;
    vq_avail.ring[vq_avail.idx % VQ_N] = 0;   /* zincir başı = desc 0 */
    __asm__ volatile("dsb sy" ::: "memory");
    vq_avail.idx++;
    __asm__ volatile("dsb sy" ::: "memory");

    mmio_w32(base, VMMIO_QUEUE_NOTIFY, 0);    /* queue 0'ı bildir */

    /* used ring'de tamamlanmayı bekle (poll). */
    for (int spin = 0; vq_used.idx == onceki; spin++) {
        __asm__ volatile("dsb sy" ::: "memory");
        if (spin > 200000000) return -4;      /* timeout */
    }
    if (vq_status != 0) return -5;            /* VIRTIO_BLK_S_OK = 0 */

    for (int i = 0; i < 512; i++) hedef[i] = vq_data[i];
    return 0;
}

/* `sektor` numaralı bloğa `kaynak`taki 512 baytı YAZ (kalıcı). 0 = ok; negatif = hata.
 * Okumadan farkı: type=1 (OUT); veri descriptor'ı cihaz-OKUR (WRITE flag YOK). */
int kdl_virtio_blk_yaz(uint64_t base, uint64_t sektor, const uint8_t *kaynak) {
    vq_req.type = 1;           /* VIRTIO_BLK_T_OUT = yaz */
    vq_req.reserved = 0;
    vq_req.sector = sektor;
    for (int i = 0; i < 512; i++) vq_data[i] = kaynak[i];
    vq_status = 0xff;

    vq_desc[0].addr = (uint64_t)(uintptr_t)&vq_req;    vq_desc[0].len = 16;  vq_desc[0].flags = VRING_DESC_F_NEXT; vq_desc[0].next = 1;
    vq_desc[1].addr = (uint64_t)(uintptr_t)vq_data;    vq_desc[1].len = 512; vq_desc[1].flags = VRING_DESC_F_NEXT; vq_desc[1].next = 2;   /* cihaz OKUR → WRITE flag yok */
    vq_desc[2].addr = (uint64_t)(uintptr_t)&vq_status; vq_desc[2].len = 1;   vq_desc[2].flags = VRING_DESC_F_WRITE; vq_desc[2].next = 0;

    uint16_t onceki = vq_used.idx;
    vq_avail.ring[vq_avail.idx % VQ_N] = 0;
    __asm__ volatile("dsb sy" ::: "memory");
    vq_avail.idx++;
    __asm__ volatile("dsb sy" ::: "memory");

    mmio_w32(base, VMMIO_QUEUE_NOTIFY, 0);

    for (int spin = 0; vq_used.idx == onceki; spin++) {
        __asm__ volatile("dsb sy" ::: "memory");
        if (spin > 200000000) return -4;
    }
    if (vq_status != 0) return -5;
    return 0;
}

#endif /* __aarch64__ */
