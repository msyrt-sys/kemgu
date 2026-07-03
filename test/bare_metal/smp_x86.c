/*
 * KEMGU-OS bare-metal SMP testi (x86_64) — ÇOK-ÇEKİRDEK bring-up.
 * ================================================================
 *
 * Milestone: D-169 (aarch64 PSCI CPU_ON) testinin x86 ikizi/paritesi. 2. CPU
 * çekirdeğini (AP = Application Processor) Local APIC INIT-SIPI dizisi ile
 * başlat. QEMU `-smp 2` iki çekirdek verir; boot çekirdeği (BSP = Bootstrap
 * Processor) AP'yi Local APIC üzerinden uyandırır.
 *
 * -----------------------------------------------------------------------------
 * x86 SMP neden aarch64'ten ZOR:
 *   aarch64'te PSCI CPU_ON'a giriş noktası olarak DOĞRUDAN 64-bit bir C
 *   fonksiyonu verilir; firmware AP'yi hedef exception level'da başlatır.
 *   x86'da ise AP her zaman GERÇEK MOD'da (16-bit, paging KAPALI) başlar —
 *   SIPI vektörü × 4 KB fiziksel adreste. Long-mode kernel'de AP'yi kullanmak
 *   için real-mode → protected-mode → long-mode geçişini yapan bir
 *   "trampoline" gerekir (düşük bellekte, identity-map'li, kendi GDT'siyle).
 *   Bu test o trampoline'i düşük belleğe (0x8000) yazar + AP'yi oraya atlatır.
 *
 * -----------------------------------------------------------------------------
 * Akış:
 *   1. BSP "SMP X86 BASLA" basar.
 *   2. Local APIC MMIO (xAPIC, taban 0xFEE00000) erişimini KANITLA:
 *        - APIC ID register (0x20) oku  → BSP'nin APIC ID'si
 *        - APIC Version register (0x30) oku → sürüm + max LVT
 *        - Spurious Interrupt Vector (0xF0) bit 8 = APIC yazılım-etkin
 *   3. AP trampoline'ini düşük belleğe (0x8000, SIPI vektör 0x08) kopyala.
 *      Trampoline BSP'nin CR3'ünü (aynı identity-map PML4) ve boot GDT'sini
 *      paylaşır → AP long-mode'da BSP ile aynı adres uzayını görür.
 *   4. INIT-SIPI-SIPI dizisi (Intel MP/SDM önerisi):
 *        - ICR yaz: INIT IPI (delivery mode 0b101, level assert)
 *        - ICR yaz: STARTUP IPI (delivery mode 0b110, vektör = 0x08) ×2
 *      Her ICR yazımından sonra delivery-status (bit 12) temizlenene kadar bekle.
 *   5. AP trampoline: real → protected → long geçişi + KENDİ stack + paylaşılan
 *      bayrağı 1 yap + hlt.
 *   6. BSP bayrağı poll eder (timeout'lu) → set olursa "SMP X86 OK 2 cekirdek".
 *
 * -----------------------------------------------------------------------------
 * FALLBACK (real-mode trampoline AP'yi long-mode'a getiremezse):
 *   En azından Local APIC ALTYAPISININ erişilebilir olduğunu kanıtla —
 *   APIC ID + Version register okundu + INIT-SIPI ICR yazımı yapıldı
 *   (delivery-status ile gönderim onaylandı). Marker "APIC OK". Bu, x86 SMP
 *   temelinin (LAPIC MMIO + IPI mekanizması) çalıştığını gösterir; eksik olan
 *   yalnız AP'nin long-mode gövdesine ulaşmasıdır.
 *
 * Kanıt: "SMP X86 OK" → AP gerçekten long-mode'da koştu (bayrağı yazan o).
 *        "APIC OK"     → LAPIC MMIO + IPI altyapısı çalışıyor (AP gövdesi teyit yok).
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);   /* metin + newline */
extern void kdl_yaz_metin(const char *);       /* metin, newline'siz (inline etiket) */
extern void kdl_yazdir_satir(void);
extern void kdl_yaz_onaltilik(uint64_t);       /* onaltilik, newline'siz */
extern void kdl_yazdir_onaltilik(uint64_t);    /* onaltilik + newline */

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("hlt"); } }

/* === Local APIC MMIO (xAPIC modu) === */
#define LAPIC_TABAN      0xFEE00000ULL
#define LAPIC_ID         0x020u   /* [31:24] = APIC ID */
#define LAPIC_VERSION    0x030u   /* [7:0]=sürüm, [23:16]=max LVT */
#define LAPIC_SIVR       0x0F0u   /* Spurious Interrupt Vector; bit 8 = APIC etkin */
#define LAPIC_ICR_DUSUK  0x300u   /* Interrupt Command Register, düşük 32 bit */
#define LAPIC_ICR_YUKSEK 0x310u   /* Interrupt Command Register, yüksek 32 bit */

/* ICR düşük alan bitleri. */
#define ICR_VEKTOR(v)         ((uint32_t)((v) & 0xFF))
#define ICR_TESLIM_INIT       (0x5u << 8)   /* delivery mode = INIT */
#define ICR_TESLIM_STARTUP    (0x6u << 8)   /* delivery mode = STARTUP (SIPI) */
#define ICR_LEVEL_ASSERT      (1u << 14)    /* level = assert */
#define ICR_TESLIM_DURUM      (1u << 12)    /* delivery status (RO): 1=beklemede */

/* MMIO 32-bit oku/yaz — LAPIC register'ları yalnız 32-bit hizalı erişir. */
static inline uint32_t lapic_oku(uint32_t reg) {
    return *(volatile uint32_t *)(LAPIC_TABAN + reg);
}
static inline void lapic_yaz(uint32_t reg, uint32_t deger) {
    *(volatile uint32_t *)(LAPIC_TABAN + reg) = deger;
}

/*
 * === LAPIC MMIO sayfasını haritala ===
 * Boot page-table'ları (boot/start_x86_64.S) yalnız 0..1 GB'ı (PDPT[0] → tek PD,
 * 512×2MB) identity-map eder. LAPIC tabanı 0xFEE00000 (~3.98 GB) HARİTASIZ →
 * erişince #PF (vektör 14). boot read-only olduğundan haritayı BURADA, çalışma
 * anında kuruyoruz: 0xFEE00000'ı içeren 2 MB'lık bölgeyi identity 2MB-huge page
 * olarak ekle. PDPT[3] (3..4 GB) boot'ta boş → kendi PD'mizi bağlarız.
 *
 * Sayfa girişi bayrakları: present|write|PS(huge)|PCD(cache-disable)|PWT
 * (LAPIC MMIO güçlü-sırasız/uncacheable olmalı → PCD+PWT).
 */
static uint64_t lapic_pd[512] __attribute__((aligned(4096)));

static void lapic_mmio_harita_kur(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    /* CR3 → PML4 fiziksel taban (identity-map: fiziksel == sanal). */
    uint64_t *pml4 = (uint64_t *)(uintptr_t)(cr3 & ~0xFFFULL);

    /* 0xFEE00000 için PML4/PDPT/PD indeksleri. */
    uint64_t adr = LAPIC_TABAN;                 /* 0xFEE00000 */
    unsigned pml4_i = (adr >> 39) & 0x1FF;      /* = 0 */
    unsigned pdpt_i = (adr >> 30) & 0x1FF;      /* = 3 */
    unsigned pd_i   = (adr >> 21) & 0x1FF;      /* 2MB indeks */

    /* PML4[0] → mevcut PDPT (boot kurdu). Fiziksel tabanını çıkar. */
    uint64_t pml4e = pml4[pml4_i];
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4e & 0x000FFFFFFFFFF000ULL);

    /* PDPT[3]'e kendi PD'mizi bağla (present|write). */
    pdpt[pdpt_i] = ((uint64_t)(uintptr_t)lapic_pd & 0x000FFFFFFFFFF000ULL) | 0x3ULL;

    /* PD[pd_i] = 2MB huge page, taban = 0xFEE00000'ın 2MB-hizalı hâli,
       bayraklar present|write|PS|PCD|PWT. */
    uint64_t sayfa_taban = adr & ~0x1FFFFFULL;   /* 2MB-hizala */
    lapic_pd[pd_i] = sayfa_taban | 0x9BULL;       /* P|RW|PWT(0x8)|PCD(0x10)|PS(0x80) = 0x9B */

    /* TLB'yi temizle (yeni harita görünsün) — CR3 yeniden yükle. */
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

/* ICR delivery-status (bit 12) temizlenene kadar bekle (bounded). Dönüş:
 * 0 = temizlendi (IPI kabul edildi), 1 = timeout (hâlâ beklemede). */
static int icr_bekle(void) {
    for (int i = 0; i < 1000000; i++) {
        if ((lapic_oku(LAPIC_ICR_DUSUK) & ICR_TESLIM_DURUM) == 0) {
            return 0;
        }
    }
    return 1;
}

/* ICR yaz: önce yüksek (hedef APIC ID [31:24]), sonra düşük (yazım IPI'yi
 * tetikler). */
static void ipi_gonder(uint8_t hedef_apic_id, uint32_t icr_dusuk) {
    lapic_yaz(LAPIC_ICR_YUKSEK, (uint32_t)hedef_apic_id << 24);
    lapic_yaz(LAPIC_ICR_DUSUK, icr_dusuk);
}

/*
 * Kaba gecikme — INIT ile SIPI arasında (Intel: INIT sonrası ~10 ms, SIPI'ler
 * arası ~200 µs). Bare-metal'de kalibre saat yok → port 0x80 (POST) I/O ile
 * yaklaşık gecikme (her outb ~1 µs QEMU'da; sayı büyük tutuldu, deterministik).
 */
static void kaba_gecikme(int tur) {
    for (int i = 0; i < tur; i++) {
        __asm__ volatile("outb %%al, $0x80" : : "a"((uint8_t)0) : "memory");
    }
}

/* === Paylaşılan durum — BSP ile AP arasında ===
 * volatile: derleyici cache'lemesin. AP long-mode'da BSP ile aynı CR3'ü (WB
 * cacheable identity map) kullanır → x86 cache coherency (MESI) donanımda
 * otomatik; aarch64'teki manuel dc-ivac GEREKMEZ. */
static volatile uint32_t ap_canli __attribute__((aligned(64))) = 0;
static volatile uint32_t ap_apic_id __attribute__((aligned(64))) = 0xFFFFFFFFu;

/* AP'nin kendi long-mode yığını (BSP'ninkinden ayrı) — 16 KB. */
static uint8_t ap_yigin[16384] __attribute__((aligned(16)));

/*
 * AP'nin long-mode C giriş noktası. Trampoline buraya (64-bit) atlar. AP
 * KENDİ stack'ini trampoline'de kurdu; burada yalnız paylaşılan durumu yaz.
 * UART'a DOKUNMA (BSP ile paylaşımlı, senkronizasyonsuz → çakışma). Yalnız
 * bayrağı + kendi APIC ID'sini yaz, sonra hlt.
 */
__attribute__((noreturn))
static void ap_long_giris(void) {
    /* AP'nin kendi APIC ID'sini oku — "gerçekten 2. çekirdek koştu" kanıtı
     * (BSP'nin ID'sinden FARKLI olmalı). */
    ap_apic_id = (lapic_oku(LAPIC_ID) >> 24) & 0xFFu;
    __asm__ volatile("mfence" ::: "memory");
    ap_canli = 1;
    __asm__ volatile("mfence" ::: "memory");
    halt();
}

/*
 * === AP TRAMPOLINE (elle-derlenmiş makine kodu blob'u) ===
 *
 * AP gerçek mod'da (16-bit, CS=vektör<<8, IP=0) bu blob'un BAŞINDA başlar.
 * Blob konum-bağımsız DEĞİL — sabit fiziksel adrese (TRAMBOLIN_ADRES=0x8000)
 * kopyalanır; içindeki tüm mutlak adresler o tabana göre hesaplanmış olmalı.
 *
 * Neden elle-blob (inline .code16 değil): clang -target x86_64 gerçek-mod
 * 16-bit kod üretemez; ayrı .code16 assembly + link akışı da linker'a dokunmayı
 * gerektirir. Blob byte dizisi → test dosyasında bağımsız, linker'a dokunmaz.
 *
 * Geçiş dizisi (real → protected → long):
 *   16-bit: cli; A20 zaten açık (PVH); lgdtl [gdt_ptr]; CR0.PE=1;
 *           ljmpl → 32-bit kod segmenti
 *   32-bit: veri segmentlerini ayarla; CR3 = BSP_CR3; CR4.PAE=1;
 *           EFER.LME=1 (rdmsr/wrmsr); CR0.PG=1; ljmpl → 64-bit kod segmenti
 *   64-bit: RSP = ap_yigin tepesi; RDI yok; call ap_long_giris (mutlak 64-bit)
 *
 * Trampoline'in ihtiyaç duyduğu dış değerler blob'un SONUNA "yama alanları"
 * olarak gömülür ve C tarafından doldurulur:
 *   - BSP CR3 (32-bit; PML4 fiziksel < 4 GB)
 *   - ap_long_giris mutlak 64-bit adresi
 *   - ap_yigin tepesi (64-bit)
 *   - trampoline-yerel GDT (real→prot→long için 32-bit ve 64-bit kod/veri)
 *
 * NOT: Trampoline KENDİ GDT'sini taşır (boot GDT 1 MB+ adreste ve descriptor'ı
 * 32-bit lgdtl için uygun ama trampoline'i tam self-contained tutmak — ve
 * gerçek-mod lgdt'nin 24-bit taban sınırını aşmamak — için yerel GDT
 * düşük bellekte 0x8000+ofset'te durur).
 */

#define TRAMBOLIN_ADRES  0x8000u        /* SIPI vektör 0x08 → 0x08<<12 = 0x8000 */
#define SIPI_VEKTOR      0x08u

/*
 * Blob'u üretmek yerine, geçişi yürüten kodu ayrı bir "kaynak trampoline"
 * fonksiyonu olarak DEĞİL, doğrudan makine kodu byte'ları olarak yazmak en
 * güvenilir yol (derleyici 16-bit üretemediğinden). Aşağıdaki dizi elle
 * assemble edilmiştir; her satır yorumda mnemonic + ofset taşır.
 *
 * Yama ofsetleri (blob içinde, TRAMBOLIN_ADRES tabanlı) #define ile sabit.
 */

/* Blob içindeki mutlak-adres alanlarının ofsetleri (aşağıdaki dizide işaretli). */
/* Bu ofsetler blob elle kurulduktan SONRA doğrulanır (derleme-zamanı sabit). */

int main(void) {
    kdl_yazdir_metin("SMP X86 BASLA");
    kdl_yazdir_satir();

    /* --- 1b. LAPIC MMIO sayfasını haritala (boot yalnız 0..1GB haritalar) --- */
    lapic_mmio_harita_kur();

    /* --- 2. Local APIC MMIO erişimini kanıtla --- */
    uint32_t bsp_apic_id = (lapic_oku(LAPIC_ID) >> 24) & 0xFFu;
    uint32_t apic_ver_ham = lapic_oku(LAPIC_VERSION);
    uint32_t apic_surum   = apic_ver_ham & 0xFFu;
    uint32_t apic_max_lvt = (apic_ver_ham >> 16) & 0xFFu;
    uint32_t sivr         = lapic_oku(LAPIC_SIVR);

    kdl_yaz_metin("BSP APIC_ID=");
    kdl_yaz_onaltilik(bsp_apic_id);
    kdl_yaz_metin(" APIC_VER=");
    kdl_yaz_onaltilik(apic_surum);
    kdl_yaz_metin(" MAX_LVT=");
    kdl_yaz_onaltilik(apic_max_lvt);
    kdl_yaz_metin(" SIVR=");
    kdl_yazdir_onaltilik(sivr);

    /* APIC'i yazılım-etkin yap (SIVR bit 8) — IPI göndermek için gerekli.
     * Spurious vektör 0xFF (kullanılmayan). */
    lapic_yaz(LAPIC_SIVR, sivr | 0x100u | 0xFFu);

    /* Version register makul mü? (xAPIC: 0x10..0x15 tipik; QEMU 0x14.) Sadece
     * sıfır-olmayan + üst bitlerin makul olması LAPIC MMIO'nun çalıştığını
     * gösterir. */
    int lapic_erisilebilir = (apic_ver_ham != 0 && apic_ver_ham != 0xFFFFFFFFu);

    /* --- 3. AP trampoline'ini düşük belleğe kur --- *
     * Trampoline'i 0x8000'e byte-byte yazıyoruz. Kod konum-bağımlı olduğundan
     * mutlak adresler TRAMBOLIN_ADRES tabanlıdır. Yama alanları (CR3, giriş
     * adresi, stack, GDT) kod gövdesinden sonra yerleşir ve C ile doldurulur.
     *
     * Blob düzeni (ofsetler TRAMBOLIN_ADRES'e göre):
     *   0x00  16-bit giriş (real mode)
     *   ...   32-bit protected mode kodu
     *   ...   64-bit long mode kodu
     *   veri: gdt (yerel), gdt_ptr16 (real-mod lgdt için), gdt_ptr32,
     *         cr3_degeri, giris64, stack64
     */

    volatile uint8_t *T = (volatile uint8_t *)(uintptr_t)TRAMBOLIN_ADRES;

    /* BSP'nin CR3'ünü oku (AP aynı identity-map PML4'ü kullanacak). */
    uint64_t bsp_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(bsp_cr3));

    /*
     * --- Trampoline makine kodu ---
     * Elle assemble; ofsetler blob-yerel. Aşağıda `i` akan yazma indeksidir.
     * VERİ alanlarının blob-içi ofsetleri kod boyutuna bağlı; kodu yazdıktan
     * sonra hizalayıp sabitliyoruz.
     */
    int i = 0;
    #define B(x) (T[i++] = (uint8_t)(x))
    #define ADR16(ofs)  ((uint16_t)(TRAMBOLIN_ADRES + (ofs)))   /* 16-bit mutlak */
    #define ADR32(ofs)  ((uint32_t)(TRAMBOLIN_ADRES + (ofs)))   /* 32-bit mutlak */

    /*
     * Veri alanlarının ofsetlerini önce sabitle (kod bölümünden sonra gelir).
     * Kod bölümü ~0x80 byte'tan kısa; veriyi 0x80'de başlat (bol pay).
     */
    /*
     * Veri düzeni (çakışmasız): kod bölümü ~0x8D byte tutuyor (real+prot+long
     * geçiş kodu). Veriyi 0x100'de başlat (bol pay; kod büyürse çakışma yok).
     * GDT 5 descriptor × 8 = 40 (0x28) byte, 0x100'de → 0x128'e kadar. Kalan
     * alanlar 0x128'den itibaren sırayla.
     */
    const int OFS_GDT      = 0x100;  /* yerel GDT (5 descriptor × 8 = 40 byte) → 0x128'e kadar */
    const int OFS_GDTPTR32 = 0x128;  /* 6-byte: limit(2) + taban(4) → 0x12E */
    const int OFS_GDTPTR16 = 0x130;  /* 6-byte (kullanılmıyor, tutarlılık) → 0x136 */
    const int OFS_CR3      = 0x138;  /* 4-byte → 0x13C */
    const int OFS_GIRIS64  = 0x140;  /* 8-byte: ap_long_giris mutlak adres → 0x148 */
    const int OFS_STACK64  = 0x148;  /* 8-byte: AP yığın tepesi → 0x150 */

    /* ---- 16-bit real mode (ofset 0x00) ---- */
    /* cli */                         B(0xFA);
    /* cld */                         B(0xFC);
    /* xor ax,ax  (66? hayır, 16-bit: 31 C0) */ B(0x31); B(0xC0);
    /* mov ds,ax */                   B(0x8E); B(0xD8);
    /* mov es,ax */                   B(0x8E); B(0xC0);
    /* mov ss,ax */                   B(0x8E); B(0xD0);
    /* lgdtl [gdt_ptr32]  — 32-bit lgdt (operand-size prefix 66) yükler 6-byte
       pseudo-descriptor'ı. Real-mod'da 66-prefix'li lgdt 32-bit taban yükler.
       Kodlama: 66 0F 01 16 <disp16>  (mod=00, reg=/2, r/m=110 → disp16 mutlak) */
    B(0x66); B(0x0F); B(0x01); B(0x16); {
        uint16_t a = ADR16(OFS_GDTPTR32);
        B(a & 0xFF); B((a >> 8) & 0xFF);
    }
    /* mov eax,cr0 : 0F 20 C0 */      B(0x0F); B(0x20); B(0xC0);
    /* or eax,1 (PE) : 66 83 C8 01 */ B(0x66); B(0x83); B(0xC8); B(0x01);
    /* mov cr0,eax : 0F 22 C0 */      B(0x0F); B(0x22); B(0xC0);
    /* ljmpl 0x08:pm32 — far jump 32-bit protected mode.
       Kodlama: 66 EA <off32> <sel16> */
    B(0x66); B(0xEA);
    int yama_pm32 = i; B(0); B(0); B(0); B(0);   /* off32 (pm32 mutlak, sonra doldur) */
    B(0x08); B(0x00);                             /* selector 0x08 (32-bit kod) */

    /* ---- 32-bit protected mode ---- */
    int ofs_pm32 = i;
    /* mov ax,0x10 ; veri segmenti (16-bit imm, 66-prefix'siz çünkü artık 32-bit
       mod → operand default 32; segment reg yüklemesi 16-bit). Basitçe:
       B8 10 00 00 00 (mov eax,0x10) sonra segment yükle. */
    /* mov eax,0x10 : B8 10 00 00 00 */ B(0xB8); B(0x10); B(0x00); B(0x00); B(0x00);
    /* mov ds,ax : 8E D8 */            B(0x8E); B(0xD8);
    /* mov es,ax : 8E C0 */            B(0x8E); B(0xC0);
    /* mov ss,ax : 8E D0 */            B(0x8E); B(0xD0);
    /* mov fs,ax : 8E E0 */            B(0x8E); B(0xE0);
    /* mov gs,ax : 8E E8 */            B(0x8E); B(0xE8);
    /* mov eax,[cr3_degeri] : A1 <disp32> (mov eax, moffs32) */
    B(0xA1); { uint32_t a = ADR32(OFS_CR3); B(a & 0xFF); B((a>>8)&0xFF); B((a>>16)&0xFF); B((a>>24)&0xFF); }
    /* mov cr3,eax : 0F 22 D8 */       B(0x0F); B(0x22); B(0xD8);
    /* mov eax,cr4 : 0F 20 E0 */       B(0x0F); B(0x20); B(0xE0);
    /* or eax,0x20 (PAE bit5) : 83 C8 20 */ B(0x83); B(0xC8); B(0x20);
    /* mov cr4,eax : 0F 22 E0 */       B(0x0F); B(0x22); B(0xE0);
    /* mov ecx,0xC0000080 (EFER) : B9 80 00 00 C0 */ B(0xB9); B(0x80); B(0x00); B(0x00); B(0xC0);
    /* rdmsr : 0F 32 */                B(0x0F); B(0x32);
    /* or eax,0x100 (LME bit8) : 0D 00 01 00 00 (or eax,imm32) */ B(0x0D); B(0x00); B(0x01); B(0x00); B(0x00);
    /* wrmsr : 0F 30 */                B(0x0F); B(0x30);
    /* mov eax,cr0 : 0F 20 C0 */       B(0x0F); B(0x20); B(0xC0);
    /* or eax,0x80000000 (PG bit31) : 0D 00 00 00 80 */ B(0x0D); B(0x00); B(0x00); B(0x00); B(0x80);
    /* mov cr0,eax : 0F 22 C0 */       B(0x0F); B(0x22); B(0xC0);
    /* ljmpl 0x18:lm64 — far jump 64-bit kod segmenti (selector 0x18).
       Kodlama (32-bit mod): EA <off32> <sel16> */
    B(0xEA);
    int yama_lm64 = i; B(0); B(0); B(0); B(0);   /* off32 (lm64 mutlak) */
    B(0x18); B(0x00);                             /* selector 0x18 (64-bit kod) */

    /* ---- 64-bit long mode ---- */
    int ofs_lm64 = i;
    /* mov ax,0x20 ; 64-bit veri segmenti. 66 B8 20 00 (mov ax,imm16) sonra
       segment yükle. Long mode'da veri segment yüklemesi büyük ölçüde
       görmezden gelinir ama temiz olması için ayarla. */
    /* mov eax,0x20 : B8 20 00 00 00 */ B(0xB8); B(0x20); B(0x00); B(0x00); B(0x00);
    /* mov ds,ax : 8E D8 */            B(0x8E); B(0xD8);
    /* mov es,ax : 8E C0 */            B(0x8E); B(0xC0);
    /* mov ss,ax : 8E D0 */            B(0x8E); B(0xD0);
    /* mov fs,ax : 8E E0 */            B(0x8E); B(0xE0);
    /* mov gs,ax : 8E E8 */            B(0x8E); B(0xE8);
    /* mov rsp,[stack64] : 48 A1 <moffs64> (mov rax, moffs64) → rax, sonra rsp.
       Basitçe rsp'yi mov rax + mov rsp,rax ile kur. */
    /* mov rax,[stack64] : 48 A1 <off64> */
    B(0x48); B(0xA1);
    { uint64_t a = (uint64_t)ADR32(OFS_STACK64);   /* moffs64 = mutlak (üst 32=0) */
      for (int k = 0; k < 8; k++) B((a >> (8*k)) & 0xFF); }
    /* mov rsp,rax : 48 89 C4 */       B(0x48); B(0x89); B(0xC4);
    /* mov rax,[giris64] : 48 A1 <off64> */
    B(0x48); B(0xA1);
    { uint64_t a = (uint64_t)ADR32(OFS_GIRIS64);
      for (int k = 0; k < 8; k++) B((a >> (8*k)) & 0xFF); }
    /* call rax : FF D0 */             B(0xFF); B(0xD0);
    /* hlt (call dönmez ama güvenlik) : F4 */ B(0xF4);
    /* jmp $-1 : EB FE */              B(0xEB); B(0xFE);

    int kod_sonu = i;

    /* --- Yama: pm32 ve lm64 mutlak ofsetleri --- */
    { uint32_t a = ADR32(ofs_pm32);
      T[yama_pm32+0]=a&0xFF; T[yama_pm32+1]=(a>>8)&0xFF; T[yama_pm32+2]=(a>>16)&0xFF; T[yama_pm32+3]=(a>>24)&0xFF; }
    { uint32_t a = ADR32(ofs_lm64);
      T[yama_lm64+0]=a&0xFF; T[yama_lm64+1]=(a>>8)&0xFF; T[yama_lm64+2]=(a>>16)&0xFF; T[yama_lm64+3]=(a>>24)&0xFF; }

    /* Kod bölümü OFS_GDT'yi aşmamalı (aşarsa veri kodu ezer). Bunu runtime'da
     * kanıtla + rapora yaz. */
    int kod_sigdi = (kod_sonu <= OFS_GDT);

    /* --- Veri alanları --- */
    /* Yerel GDT: [0]=null, [1]=0x08 32-bit kod, [2]=0x10 32-bit veri,
       [3]=0x18 64-bit kod, [4]=0x20 64-bit veri. 5 descriptor. */
    {
        volatile uint8_t *g = T + OFS_GDT;
        /* Descriptor'ı 8-byte quad olarak yaz (little-endian). */
        uint64_t gdt[5];
        gdt[0] = 0x0000000000000000ULL;                 /* null */
        gdt[1] = 0x00CF9A000000FFFFULL;                 /* 0x08: 32-bit kod (G,D,present,exec/read, base0 lim4G) */
        gdt[2] = 0x00CF92000000FFFFULL;                 /* 0x10: 32-bit veri (G,B,present,r/w) */
        gdt[3] = 0x00209A0000000000ULL;                 /* 0x18: 64-bit kod (L,present,exec/read) */
        gdt[4] = 0x0000920000000000ULL;                 /* 0x20: 64-bit veri (present,r/w) */
        for (int d = 0; d < 5; d++)
            for (int k = 0; k < 8; k++)
                g[d*8 + k] = (uint8_t)((gdt[d] >> (8*k)) & 0xFF);
    }

    /* gdt_ptr32: limit (2 byte) = 40-1, taban (4 byte) = ADR32(OFS_GDT). */
    {
        volatile uint8_t *p = T + OFS_GDTPTR32;
        uint16_t limit = (uint16_t)(5*8 - 1);
        uint32_t taban = ADR32(OFS_GDT);
        p[0] = limit & 0xFF; p[1] = (limit>>8)&0xFF;
        p[2] = taban & 0xFF; p[3] = (taban>>8)&0xFF; p[4] = (taban>>16)&0xFF; p[5] = (taban>>24)&0xFF;
    }
    /* gdt_ptr16 (kullanılmıyor ama tutarlılık için aynı) */
    {
        volatile uint8_t *p = T + OFS_GDTPTR16;
        uint16_t limit = (uint16_t)(5*8 - 1);
        uint32_t taban = ADR32(OFS_GDT);
        p[0] = limit & 0xFF; p[1] = (limit>>8)&0xFF;
        p[2] = taban & 0xFF; p[3] = (taban>>8)&0xFF; p[4] = (taban>>16)&0xFF; p[5] = (taban>>24)&0xFF;
    }
    /* CR3 (32-bit; PML4 fiziksel < 4 GB). */
    {
        volatile uint8_t *p = T + OFS_CR3;
        uint32_t c = (uint32_t)bsp_cr3;
        p[0]=c&0xFF; p[1]=(c>>8)&0xFF; p[2]=(c>>16)&0xFF; p[3]=(c>>24)&0xFF;
    }
    /* giris64: ap_long_giris mutlak 64-bit adres. */
    {
        volatile uint8_t *p = T + OFS_GIRIS64;
        uint64_t a = (uint64_t)(uintptr_t)&ap_long_giris;
        for (int k = 0; k < 8; k++) p[k] = (uint8_t)((a >> (8*k)) & 0xFF);
    }
    /* stack64: AP yığın tepesi (aşağı büyür → dizinin sonu), 16-hizalı. */
    {
        volatile uint8_t *p = T + OFS_STACK64;
        uint64_t a = (uint64_t)(uintptr_t)&ap_yigin[sizeof(ap_yigin)];
        a &= ~(uint64_t)0xF;   /* 16-hizala */
        for (int k = 0; k < 8; k++) p[k] = (uint8_t)((a >> (8*k)) & 0xFF);
    }

    /* Yazımların RAM'e ulaştığından emin ol (AP okumadan önce). */
    __asm__ volatile("mfence" ::: "memory");

    /* --- 4. INIT-SIPI-SIPI dizisi --- *
     * Hedef AP APIC ID = 1 (QEMU -smp 2: BSP=0, AP=1). ICR yüksek [31:24]=hedef.
     * BSP kendi ID'sinden farklı olmalı; QEMU'da BSP=0 → AP=1. */
    uint8_t ap_hedef = (bsp_apic_id == 0) ? 1 : 0;

    int init_gonderildi = 0, sipi_gonderildi = 0;

    if (lapic_erisilebilir) {
        /* INIT IPI (assert). */
        ipi_gonder(ap_hedef, ICR_TESLIM_INIT | ICR_LEVEL_ASSERT);
        init_gonderildi = (icr_bekle() == 0);
        kaba_gecikme(20000);   /* ~10 ms hedefli kaba bekleme */

        /* STARTUP IPI ×2 (vektör = trampoline sayfası >> 12 = 0x08). */
        ipi_gonder(ap_hedef, ICR_TESLIM_STARTUP | ICR_VEKTOR(SIPI_VEKTOR));
        int s1 = (icr_bekle() == 0);
        kaba_gecikme(400);     /* ~200 µs */
        ipi_gonder(ap_hedef, ICR_TESLIM_STARTUP | ICR_VEKTOR(SIPI_VEKTOR));
        int s2 = (icr_bekle() == 0);
        sipi_gonderildi = (s1 || s2);
    }

    kdl_yaz_metin("INIT gonderildi=");
    kdl_yaz_onaltilik((uint64_t)init_gonderildi);
    kdl_yaz_metin(" SIPI gonderildi=");
    kdl_yaz_onaltilik((uint64_t)sipi_gonderildi);
    kdl_yaz_metin(" trampolin_kod_bytes=");
    kdl_yaz_onaltilik((uint64_t)kod_sonu);
    kdl_yaz_metin(" kod_sigdi=");
    kdl_yazdir_onaltilik((uint64_t)kod_sigdi);

    /* --- 6. AP bayrağını poll et (timeout'lu). --- */
    int ap_bootladi = 0;
    if (init_gonderildi && sipi_gonderildi) {
        for (volatile uint64_t bekle = 0; bekle < 100000000ULL; bekle++) {
            if (ap_canli != 0) { ap_bootladi = 1; break; }
        }
    }

    if (ap_bootladi) {
        /* AP gerçekten long-mode'a ulaştı + kendi APIC ID'sini yazdı. */
        kdl_yaz_metin("AP APIC_ID=");
        kdl_yazdir_onaltilik((uint64_t)ap_apic_id);
        kdl_yaz_metin("SMP X86 OK 2 cekirdek (INIT-SIPI, AP long-mode kostu, APIC_ID=");
        kdl_yaz_onaltilik((uint64_t)ap_apic_id);
        kdl_yazdir_metin(")");
    } else if (lapic_erisilebilir && (init_gonderildi || sipi_gonderildi)) {
        /* FALLBACK: AP long-mode'a ulaşmadı ama LAPIC MMIO + IPI altyapısı
         * çalışıyor (APIC ID/Version okundu, ICR yazımı delivery-status ile
         * onaylandı). x86 SMP temeli erişilebilir. */
        kdl_yaz_metin("APIC OK (LAPIC MMIO + INIT-SIPI altyapisi calisiyor; AP long-mode teyidi yok) BSP_APIC_ID=");
        kdl_yaz_onaltilik(bsp_apic_id);
        kdl_yaz_metin(" APIC_VER=");
        kdl_yazdir_onaltilik(apic_surum);
    } else {
        kdl_yazdir_metin("SMP X86 BASARISIZ (LAPIC MMIO erisilemedi)");
    }

    halt();
    return 0;
}
