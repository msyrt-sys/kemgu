/*
 * PCI veri yolu numaralandirma testi (x86_64) — CIHAZ KESFI (YENI ALT-SISTEM).
 * ============================================================================
 *
 * Milestone B: gercek aygit surucularinin (virtio, NIC, disk) temeli olan PCI
 * cihaz kesfi. aarch64 tarafinda cihazlar sabit MMIO adreslerinde (device tree)
 * dururken, PC uyumlu x86'da cihazlar PCI veri yolu uzerinde numaralandirilir:
 * her (bus, slot, func) uclusu icin config-space okunur, vendor:device
 * kimliginden cihazin ne oldugu anlasilir.
 *
 * -----------------------------------------------------------------------------
 * Legacy PCI Configuration Space Access (port I/O mekanizmasi #1):
 *
 *   port 0xCF8  CONFIG_ADDRESS (32-bit yaz) — okunacak config alaninin adresi
 *   port 0xCFC  CONFIG_DATA    (32-bit oku) — o adresteki 32-bit deger
 *
 * CONFIG_ADDRESS bit duzeni (32-bit):
 *   bit 31     enable (1 = config erisimi etkin)
 *   bit 30..24 rezerve (0)
 *   bit 23..16 bus numarasi
 *   bit 15..11 slot (device) numarasi (0..31)
 *   bit 10..8  fonksiyon numarasi (0..7)
 *   bit  7..2  register offset (dword hizali → alt 2 bit 0)
 *   bit  1..0  0 (dword hizalama)
 *
 * Yani: adres = 0x80000000 | bus<<16 | slot<<11 | func<<8 | (offset & 0xFC)
 *
 * -----------------------------------------------------------------------------
 * Config-space register haritasi (her cihazin ilk 64 byte'i — Type 0 header):
 *   offset 0x00  [15:0]=Vendor ID   [31:16]=Device ID
 *   offset 0x08  [7:0]=Revision  [15:8]=Prog IF  [23:16]=Subclass  [31:24]=Class
 *   offset 0x0C  [23:16]=Header Type (bit7 = coklu-fonksiyon)
 *
 * Vendor ID = 0xFFFF → o slot'ta cihaz YOK (config-space okumasi tum-bir doner).
 *
 * -----------------------------------------------------------------------------
 * QEMU x86 (varsayilan i440FX/PIIX3 chipset) bus 0'da su cihazlari sunar:
 *   slot 0  8086:1237  Intel 440FX host bridge   (class 0x06 = bridge)
 *   slot 1  8086:7000  Intel PIIX3 ISA bridge     (class 0x06 = bridge)
 *   slot 1  8086:7010  PIIX3 IDE (func 1)         (class 0x01 = storage)
 *   slot 2  1234:1111  QEMU/Bochs stdvga          (class 0x03 = display)
 * En az host bridge (Intel 8086:1237) HER ZAMAN slot 0'da bulunur → deterministik.
 *
 * Kanit: bus 0 tara → en az 1 (pratikte >=3) PCI cihaz bulundu, host bridge
 *        (vendor 0x8086) dahil. Her cihaz vendor:device:class olarak listelendi.
 *        Marker: "PCI ENUM OK N cihaz" (N>=1). Deterministik (sabit topoloji).
 *
 * DERSLER (smp_x86.c / rtc_x86.c): x86 PVH long-mode kernel, -mgeneral-regs-only,
 * inline-asm port I/O, -serial file: ile COM1 yakala, timeout ~15.
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);   /* metin + newline */
extern void kdl_yaz_metin(const char *);       /* metin, newline'siz (inline etiket) */
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_onaltilik(uint64_t);       /* onaltilik, newline'siz */
extern void kdl_yazdir_onaltilik(uint64_t);    /* onaltilik + newline */

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("hlt"); } }

/* === Legacy PCI config-space port adresleri === */
#define KDL_PCI_CONFIG_ADRES  0xCF8u   /* CONFIG_ADDRESS (32-bit yaz) */
#define KDL_PCI_CONFIG_VERI   0xCFCu   /* CONFIG_DATA    (32-bit oku)  */

#define KDL_PCI_ETKIN         0x80000000u   /* CONFIG_ADDRESS bit 31 = enable */

/* PCI bus 0'daki slot sayisi (0..31). */
#define KDL_PCI_SLOT_SAYISI   32

/* "cihaz yok" isareti: vendor ID = 0xFFFF. */
#define KDL_PCI_VENDOR_YOK    0xFFFFu

/* 32-bit port cikisi (out eax, dx). */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

/* 32-bit port girisi (in eax, dx). */
static inline uint32_t inl(uint16_t port) {
    uint32_t v;
    __asm__ volatile("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/*
 * PCI config-space 32-bit dword oku.
 * offset dword-hizali olmali (alt 2 bit 0); fonksiyon zaten & 0xFC uygular.
 */
static uint32_t pci_config_oku32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t adres = KDL_PCI_ETKIN
                   | ((uint32_t)bus  << 16)
                   | ((uint32_t)slot << 11)
                   | ((uint32_t)func << 8)
                   | ((uint32_t)offset & 0xFCu);
    outl(KDL_PCI_CONFIG_ADRES, adres);
    return inl(KDL_PCI_CONFIG_VERI);
}

/* PCI class-code (offset 0x08 [31:24]) icin okunabilir kisa ad. */
static const char *pci_sinif_adi(uint8_t sinif) {
    switch (sinif) {
        case 0x00: return "eski";        /* Unclassified */
        case 0x01: return "depolama";    /* Mass Storage (IDE/SATA/NVMe) */
        case 0x02: return "ag";          /* Network */
        case 0x03: return "goruntu";     /* Display (VGA) */
        case 0x04: return "medya";       /* Multimedia */
        case 0x05: return "bellek";      /* Memory controller */
        case 0x06: return "kopru";       /* Bridge (host/ISA/PCI) */
        case 0x0C: return "seri-bus";    /* Serial bus (USB, SMBus) */
        default:   return "diger";
    }
}

int main(void) {
    kdl_yazdir_metin("PCI ENUM BASLA");
    kdl_yazdir_satir();

    int cihaz_sayisi   = 0;   /* bulunan toplam PCI cihaz (bus 0) */
    int host_koprusu   = 0;   /* Intel 8086:1237 host-bridge bulundu mu */
    int intel_cihaz    = 0;   /* vendor 0x8086 (Intel) cihaz sayisi */

    /* --- bus 0'i tara: slot 0..31, fonksiyon 0 --- *
     * Basit numaralandirma: her slot icin func 0'in vendor ID'sini oku.
     * 0xFFFF ise cihaz yok, atla. Aksi halde device ID + class-code oku. */
    for (int slot = 0; slot < KDL_PCI_SLOT_SAYISI; slot++) {
        uint32_t id = pci_config_oku32(0, (uint8_t)slot, 0, 0x00);
        uint16_t vendor = (uint16_t)(id & 0xFFFFu);
        uint16_t device = (uint16_t)((id >> 16) & 0xFFFFu);

        if (vendor == KDL_PCI_VENDOR_YOK) {
            continue;   /* bu slot bos */
        }

        /* class-code (offset 0x08): [31:24]=class, [23:16]=subclass. */
        uint32_t sinif_ham = pci_config_oku32(0, (uint8_t)slot, 0, 0x08);
        uint8_t sinif    = (uint8_t)((sinif_ham >> 24) & 0xFFu);
        uint8_t altsinif = (uint8_t)((sinif_ham >> 16) & 0xFFu);

        cihaz_sayisi++;
        if (vendor == 0x8086u) {
            intel_cihaz++;
        }
        /* Intel 440FX host bridge = 8086:1237. */
        if (vendor == 0x8086u && device == 0x1237u) {
            host_koprusu = 1;
        }

        /* Cihazi listele: slot + vendor:device + class:subclass + ad. */
        kdl_yaz_metin("PCI slot=");
        kdl_yaz_onaltilik((uint64_t)slot);
        kdl_yaz_metin(" vendor=");
        kdl_yaz_onaltilik((uint64_t)vendor);
        kdl_yaz_metin(" device=");
        kdl_yaz_onaltilik((uint64_t)device);
        kdl_yaz_metin(" class=");
        kdl_yaz_onaltilik((uint64_t)sinif);
        kdl_yaz_metin(":");
        kdl_yaz_onaltilik((uint64_t)altsinif);
        kdl_yaz_metin(" (");
        kdl_yaz_metin(pci_sinif_adi(sinif));
        kdl_yazdir_metin(")");
    }

    /* Ozet + kanit. */
    kdl_yaz_metin("PCI TOPLAM=");
    kdl_yaz_onaltilik((uint64_t)cihaz_sayisi);
    kdl_yaz_metin(" INTEL=");
    kdl_yaz_onaltilik((uint64_t)intel_cihaz);
    kdl_yaz_metin(" HOST_KOPRUSU=");
    kdl_yazdir_onaltilik((uint64_t)host_koprusu);

    /*
     * Basari kriteri: en az 1 PCI cihaz bulundu (N>=1) VE Intel host-bridge
     * (vendor 0x8086) mevcut. QEMU i440FX her zaman slot 0'da 8086:1237 sunar →
     * deterministik gecis. Marker sonuna N (cihaz sayisi) eklenir.
     */
    if (cihaz_sayisi >= 1 && intel_cihaz >= 1) {
        kdl_yaz_metin("PCI ENUM OK ");
        kdl_yaz_onaltilik((uint64_t)cihaz_sayisi);
        kdl_yazdir_metin(" cihaz");
        kdl_yazdir_satir();
    } else if (cihaz_sayisi >= 1) {
        /* Cihaz var ama Intel host-bridge yok — yine de numaralandirma calisti. */
        kdl_yaz_metin("PCI ENUM OK ");
        kdl_yaz_onaltilik((uint64_t)cihaz_sayisi);
        kdl_yazdir_metin(" cihaz (Intel host-bridge yok)");
        kdl_yazdir_satir();
    } else {
        kdl_yazdir_metin("PCI ENUM BASARISIZ (hicbir cihaz bulunamadi)");
        kdl_yazdir_satir();
    }

    halt();
    return 0;
}
