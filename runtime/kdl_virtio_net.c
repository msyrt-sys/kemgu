/*
 * KEMGU Bare-Metal VirtIO-Net TX Sürücüsü (kdl_virtio_net.c) — Faz G ağ başlangıcı.
 * ==============================================================================
 *
 * QEMU virt virtio-mmio üzerinden bir Ethernet ÇERÇEVESİ gönderir (TX). virtio-blk
 * (D-141) ile aynı virtio-mmio v2 + split-virtqueue makinesi; fark: DeviceID=1
 * (net), transmit queue = queue 1, buffer = virtio-net başlığı (12 bayt) + çerçeve.
 *
 * Gönderilen paket QEMU `-object filter-dump` ile pcap'e yakalanır → gate.
 * DMA tamponları RAM identity-map (VA=PA). Freestanding. Yalnız aarch64.
 */
#include <stdint.h>

#if defined(__aarch64__)

#define VMMIO_MAGIC 0x000
#define VMMIO_VERSION 0x004
#define VMMIO_DEVICE_ID 0x008
#define VMMIO_DRV_FEAT 0x020
#define VMMIO_DRV_FEAT_SEL 0x024
#define VMMIO_QUEUE_SEL 0x030
#define VMMIO_QUEUE_NUM_MAX 0x034
#define VMMIO_QUEUE_NUM 0x038
#define VMMIO_QUEUE_READY 0x044
#define VMMIO_QUEUE_NOTIFY 0x050
#define VMMIO_STATUS 0x070
#define VMMIO_Q_DESC_LO 0x080
#define VMMIO_Q_DESC_HI 0x084
#define VMMIO_Q_DRV_LO 0x090
#define VMMIO_Q_DRV_HI 0x094
#define VMMIO_Q_DEV_LO 0x0a0
#define VMMIO_Q_DEV_HI 0x0a4

#define VIRTIO_MAGIC 0x74726976
#define VIRTIO_DEV_NET 1
#define ST_ACK 1
#define ST_DRIVER 2
#define ST_DRIVER_OK 4
#define ST_FEAT_OK 8

#define NVQ_N 8   /* transmit queue boyutu */

struct nvq_desc { uint64_t addr; uint32_t len; uint16_t flags; uint16_t next; };
struct nvq_avail { uint16_t flags; uint16_t idx; uint16_t ring[NVQ_N]; uint16_t used_event; };
struct nvq_used_elem { uint32_t id; uint32_t len; };
struct nvq_used { uint16_t flags; uint16_t idx; struct nvq_used_elem ring[NVQ_N]; uint16_t avail_event; };

/* Transmit queue (1) tamponları. */
static struct nvq_desc  net_desc[NVQ_N] __attribute__((aligned(16)));
static struct nvq_avail net_avail       __attribute__((aligned(16)));
static struct nvq_used  net_used        __attribute__((aligned(16)));
static uint8_t          net_txbuf[2048] __attribute__((aligned(16)));   /* net-hdr(12) + çerçeve */

/* Receive queue (0) tamponları — cihaz gelen paketleri bunlara yazar (D-145). */
static struct nvq_desc  rx_desc[NVQ_N]  __attribute__((aligned(16)));
static struct nvq_avail rx_avail        __attribute__((aligned(16)));
static struct nvq_used  rx_used         __attribute__((aligned(16)));
static uint8_t          rx_buf[NVQ_N][2048] __attribute__((aligned(16)));
static uint16_t         rx_gorulen = 0;   /* işlenen used indeksi */

static inline uint32_t nm_r32(uint64_t b, uint64_t o) { return *(volatile uint32_t *)(uintptr_t)(b + o); }
static inline void nm_w32(uint64_t b, uint64_t o, uint32_t v) { *(volatile uint32_t *)(uintptr_t)(b + o) = v; }

/* virtio-net cihazını tara (DeviceID=1). Taban adresi döner (0 = yok). */
uint64_t kdl_virtio_net_bul(void) {
    for (int i = 0; i < 32; i++) {
        uint64_t base = 0x0a000000UL + (uint64_t)i * 0x200UL;
        if (nm_r32(base, VMMIO_MAGIC) == VIRTIO_MAGIC &&
            nm_r32(base, VMMIO_DEVICE_ID) == VIRTIO_DEV_NET) {
            return base;
        }
    }
    return 0;
}

/* Cihazı başlat: reset → feature(VERSION_1) → transmit queue (1) → DRIVER_OK. */
int kdl_virtio_net_kur(uint64_t base) {
    if (nm_r32(base, VMMIO_VERSION) != 2) return -1;

    nm_w32(base, VMMIO_STATUS, 0);
    nm_w32(base, VMMIO_STATUS, ST_ACK);
    nm_w32(base, VMMIO_STATUS, ST_ACK | ST_DRIVER);

    nm_w32(base, VMMIO_DRV_FEAT_SEL, 1);
    nm_w32(base, VMMIO_DRV_FEAT, 1);        /* bit 32 = VIRTIO_F_VERSION_1 */
    nm_w32(base, VMMIO_DRV_FEAT_SEL, 0);
    nm_w32(base, VMMIO_DRV_FEAT, 0);

    nm_w32(base, VMMIO_STATUS, ST_ACK | ST_DRIVER | ST_FEAT_OK);
    if (!(nm_r32(base, VMMIO_STATUS) & ST_FEAT_OK)) return -2;

    /* --- Receive queue (0): tüm tamponları cihaza AÇIK ver (device yazar). --- */
    nm_w32(base, VMMIO_QUEUE_SEL, 0);
    if (nm_r32(base, VMMIO_QUEUE_NUM_MAX) < NVQ_N) return -3;
    nm_w32(base, VMMIO_QUEUE_NUM, NVQ_N);
    for (int i = 0; i < NVQ_N; i++) {
        rx_desc[i].addr = (uint64_t)(uintptr_t)rx_buf[i];
        rx_desc[i].len = 2048;
        rx_desc[i].flags = 2;              /* WRITE — cihaz buraya yazar */
        rx_desc[i].next = 0;
        rx_avail.ring[i] = (uint16_t)i;
    }
    __asm__ volatile("dsb sy" ::: "memory");
    rx_avail.idx = NVQ_N;                  /* tüm tamponlar hazır */
    {
        uint64_t rda = (uint64_t)(uintptr_t)rx_desc;
        uint64_t rav = (uint64_t)(uintptr_t)&rx_avail;
        uint64_t rus = (uint64_t)(uintptr_t)&rx_used;
        nm_w32(base, VMMIO_Q_DESC_LO, (uint32_t)rda); nm_w32(base, VMMIO_Q_DESC_HI, (uint32_t)(rda >> 32));
        nm_w32(base, VMMIO_Q_DRV_LO,  (uint32_t)rav); nm_w32(base, VMMIO_Q_DRV_HI,  (uint32_t)(rav >> 32));
        nm_w32(base, VMMIO_Q_DEV_LO,  (uint32_t)rus); nm_w32(base, VMMIO_Q_DEV_HI,  (uint32_t)(rus >> 32));
        nm_w32(base, VMMIO_QUEUE_READY, 1);
    }

    /* --- Transmit queue (1). --- */
    nm_w32(base, VMMIO_QUEUE_SEL, 1);
    if (nm_r32(base, VMMIO_QUEUE_NUM_MAX) < NVQ_N) return -3;
    nm_w32(base, VMMIO_QUEUE_NUM, NVQ_N);
    uint64_t da = (uint64_t)(uintptr_t)net_desc;
    uint64_t av = (uint64_t)(uintptr_t)&net_avail;
    uint64_t us = (uint64_t)(uintptr_t)&net_used;
    nm_w32(base, VMMIO_Q_DESC_LO, (uint32_t)da); nm_w32(base, VMMIO_Q_DESC_HI, (uint32_t)(da >> 32));
    nm_w32(base, VMMIO_Q_DRV_LO,  (uint32_t)av); nm_w32(base, VMMIO_Q_DRV_HI,  (uint32_t)(av >> 32));
    nm_w32(base, VMMIO_Q_DEV_LO,  (uint32_t)us); nm_w32(base, VMMIO_Q_DEV_HI,  (uint32_t)(us >> 32));

    nm_w32(base, VMMIO_QUEUE_READY, 1);

    nm_w32(base, VMMIO_QUEUE_NOTIFY, 0);   /* RX tamponları hazır → cihaza bildir */
    nm_w32(base, VMMIO_STATUS, ST_ACK | ST_DRIVER | ST_FEAT_OK | ST_DRIVER_OK);
    rx_gorulen = 0;
    return 0;
}

/* Bir paket AL (RX). Gelen çerçeveyi (net-başlığı 12 bayt ATLANMIŞ) `hedef`e kopyala,
 * uzunluğu (bayt) döner. Paket yoksa `tikler` poll denemesi sonra 0. Negatif = hata. */
int kdl_virtio_net_al(uint64_t base, uint8_t *hedef, int max, long tikler) {
    (void)base;
    for (long spin = 0; rx_used.idx == rx_gorulen; spin++) {
        __asm__ volatile("dsb sy" ::: "memory");
        if (spin > tikler) return 0;    /* zaman aşımı — paket yok */
    }
    uint16_t slot = rx_gorulen % NVQ_N;
    uint32_t id  = rx_used.ring[slot].id;
    uint32_t len = rx_used.ring[slot].len;   /* net-başlığı(12) dâhil */
    rx_gorulen++;
    if (id >= NVQ_N) return -1;
    int govde = (int)len - 12;               /* net-başlığını atla */
    if (govde < 0) govde = 0;
    if (govde > max) govde = max;
    for (int i = 0; i < govde; i++) hedef[i] = rx_buf[id][12 + i];
    return govde;
}

/* `cerceve`deki `uzun` baytlık Ethernet çerçevesini gönder (TX). 0 = ok. */
int kdl_virtio_net_gonder(uint64_t base, const uint8_t *cerceve, int uzun) {
    for (int i = 0; i < 12; i++) net_txbuf[i] = 0;             /* virtio-net başlığı (12, sıfır) */
    for (int i = 0; i < uzun; i++) net_txbuf[12 + i] = cerceve[i];
    int toplam = 12 + uzun;

    net_desc[0].addr = (uint64_t)(uintptr_t)net_txbuf;         /* tek desc, cihaz OKUR (TX) */
    net_desc[0].len = (uint32_t)toplam;
    net_desc[0].flags = 0;
    net_desc[0].next = 0;

    uint16_t onceki = net_used.idx;
    net_avail.ring[net_avail.idx % NVQ_N] = 0;
    __asm__ volatile("dsb sy" ::: "memory");
    net_avail.idx++;
    __asm__ volatile("dsb sy" ::: "memory");

    nm_w32(base, VMMIO_QUEUE_NOTIFY, 1);                       /* transmit queue = 1 bildir */

    for (int spin = 0; net_used.idx == onceki; spin++) {
        __asm__ volatile("dsb sy" ::: "memory");
        if (spin > 200000000) return -4;
    }
    return 0;
}

#endif /* __aarch64__ */
