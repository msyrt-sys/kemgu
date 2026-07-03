/*
 * KEMGU-OS bare-metal KOOPERATİF scheduler testi (x86_64) — YIELD tabanlı
 * bağlam-değiştirme. C7a (aarch64 kooperatif sched_test.c) testinin x86 İKİZİ.
 * ============================================================================
 *
 * Milestone F: aarch64'te kooperatif scheduling (C7a, kdl_gorev_ver + runtime
 * kdl_baglam_degis) ÇALIŞIYOR. x86'da paylaşılan sched_test.c runtime'ın
 * kdl_gorev_* API'sini kullanır. Bu test x86 kooperatif scheduler'ın TCB +
 * yield + RSP-swap MEKANİZMASINI test dosyası İÇİNDE (self-contained) kurar —
 * preempt_x86.c'nin (D-212) IRQ tabanlı context-switch'inin YIELD tabanlı,
 * IRQ'suz ikizi. Kooperatif: görevler gönüllü sched_yield() çağırarak CPU
 * bırakır; timer/IRQ YOK. Kernel-mod (ring0), EL/ring geçişi YOK.
 *
 * -----------------------------------------------------------------------------
 * x86 kooperatif context-switch (RSP-swap) mekaniği:
 *   System-V AMD64 ABI'da callee-saved register'lar: rbx, rbp, r12, r13, r14,
 *   r15 (+ rsp). Kooperatif yield'de yalnız BUNLARI korumak yeter — caller-saved
 *   register'lar zaten C çağrı-noktasında (yield çağrısı) korunmuş sayılır.
 *
 *   sched_yield() (naked asm):
 *     1. push rbx/rbp/r12/r13/r14/r15  → mevcut görevin YIĞININA callee-saved.
 *     2. RSP'yi (yığın tepesi, callee-saved dahil) mevcut görevin TCB'sine kaydet.
 *     3. C seçici: round-robin sonraki READY göreve geç (aktif indeks güncelle).
 *     4. RSP'yi sonraki görevin TCB'sinden yükle → bağlam-değiştirme.
 *     5. pop r15/r14/r13/r12/rbp/rbx → sonraki görevin kaydettiği callee-saved.
 *     6. ret → sonraki görevin dönüş-adresine (kaldığı yer VEYA ilk-geçişte giriş).
 *
 *   İLK geçiş (görev henüz hiç yield etmedi): TCB'nin işaret ettiği yığına
 *   SENTETİK bir çerçeve kurulur — 6 sıfır callee-saved slotu + dönüş-adresi =
 *   görev-giriş fonksiyonu. İlk switch'te pop'lar sıfır yükler, ret giriş'e atlar.
 *
 * -----------------------------------------------------------------------------
 * Görevler ve akış (deterministik round-robin):
 *   Görev 0 = main (sürücü). Görev 1 = A, görev 2 = B, görev 3 = C.
 *   Her işçi görev (A/B/C) TUR_SAYISI kez döner: "[A]"/"[B]"/"[C]" basar, yield
 *   eder. Turları bitince kendini BİTMİŞ işaretler + sonsuza dek yield-spin eder
 *   (scheduler bitmiş görevi atlar → asla geri seçilmez).
 *
 *   main: A/B/C'yi kurar, sonra hepsi bitene kadar yield eder. Round-robin
 *   sırası deterministik: main→A→B→C→main→A→... → çıktı interleave:
 *     [A][B][C][A][B][C]...  (her tur bir satır)
 *   Hepsi bitince main "SCHED X86 OK" basar.
 *
 * Kanıt: HEM [A] HEM [B] HEM [C] çıktıda görülür + "SCHED X86 OK" → 3 görev
 *        yield ile dönüşümlü koştu, kooperatif bağlam-değiştirme çalışıyor.
 *
 * KISIT: Tüm TCB/yield/context-switch mantığı test-içi (naked asm + C).
 * runtime/boot/linker'a DOKUNULMAZ — yalnız kdl_yaz + kdl_yazdir extern'leri
 * kullanılır. main dönmez (başarı tespitinde hlt-loop → timeout ile ölür =
 * beklenen, deterministik).
 *
 * DETERMİNİSTİK: bounded turlar, sabit round-robin sıra → birden çok QEMU koşusu
 * byte-identik çıktı verir.
 */
#include <stdint.h>

extern void kdl_yazdir_metin(const char *);   /* metin + newline */
extern void kdl_yaz_metin(const char *);       /* metin, newline'siz */
extern void kdl_yazdir_satir(void);

static _Noreturn void halt(void) { for (;;) { __asm__ volatile("hlt"); } }

/* === Görev sayısı + her görevin turları === */
#define GOREV_SAYISI  4      /* 0=main, 1=A, 2=B, 3=C */
#define TUR_SAYISI    3      /* her işçi görev kaç kez [X] basıp yield eder */

/*
 * === TCB (Task Control Block) ===
 * Kooperatif x86 context-switch'te görevin TÜM callee-saved durumu KENDİ
 * yığınına push'lanır → TCB'de yalnız kaydedilmiş RSP tutmak yeter. rsp o görevin
 * (yield sırasında push'ladığı callee-saved + dönüş-adresi dahil) yığın tepesine
 * işaret eder.
 *
 * volatile: yield naked-asm hem okur hem yazar; derleyici cache'lememeli.
 */
typedef struct {
    volatile uint64_t rsp;      /* kaydedilmiş yığın işaretçisi */
} KdlTCB;

static KdlTCB tcb[GOREV_SAYISI];
static volatile uint32_t aktif = 0;                 /* şu an koşan görev indeksi */
static volatile uint32_t bitti[GOREV_SAYISI] = { 0, 0, 0, 0 };  /* 1 = görev bitmiş */

/* İşçi görev yığınları (aşağı büyür), 16-hizalı. */
static uint8_t yigin_a[8192] __attribute__((aligned(16)));
static uint8_t yigin_b[8192] __attribute__((aligned(16)));
static uint8_t yigin_c[8192] __attribute__((aligned(16)));

/*
 * === Round-robin sonraki READY görevi seç ===
 * yield naked-asm'den çağrılır. Mevcut RSP'yi (rdi) aktif görevin TCB'sine
 * kaydeder, round-robin sırada BİTMEMİŞ sonraki göreve geçer, onun kaydedilmiş
 * RSP'sini DÖNER (asm o RSP'ye geçer). Sonraki READY görev yoksa (hepsi bitti)
 * mevcut RSP'yi döner (switch yok).
 *
 * SysV ABI: 1. arg rdi, dönüş rax. Bu fonksiyon C ABI'siyle çağrılır (yield
 * asm callee-saved'ı ZATEN yığına push'ladı → C fonksiyonu callee-saved'ı
 * serbestçe kullanabilir; dönüşte asm onları geri pop'lar).
 */
uint64_t sched_sec(uint64_t mevcut_rsp);   /* prototip (naked asm .global çağırır) */
uint64_t sched_sec(uint64_t mevcut_rsp) {
    uint32_t eski = aktif;
    tcb[eski].rsp = mevcut_rsp;

    /* Round-robin: eski+1'den başlayıp bitmemiş ilk göreve dön. */
    for (uint32_t adim = 1; adim <= GOREV_SAYISI; adim++) {
        uint32_t aday = (eski + adim) % GOREV_SAYISI;
        if (!bitti[aday]) {
            aktif = aday;
            return tcb[aday].rsp;
        }
    }
    /* Hiç READY görev yok (kendisi dahil hepsi bitti sayılırsa) → switch yok. */
    aktif = eski;
    return mevcut_rsp;
}

/*
 * === sched_yield() — kooperatif bağlam-değiştirme (naked asm) ===
 *
 * Naked (prologue/epilogue YOK): callee-saved'ı ELLE yönetiriz. Akış:
 *   1. push rbx,rbp,r12,r13,r14,r15 → mevcut görevin yığınına (6×8 = 48 bayt).
 *   2. rdi = rsp (mevcut yığın tepesi); call sched_sec(rdi) → rax = yeni RSP.
 *      NOT: sched_sec çağrısı için rsp 16-hizalı olmalı (SysV ABI). 6 push
 *      sonrası + call'ın döneceği 8-bayt dönüş-adresi → yığın hizası korunur
 *      (giriş rsp 16-hizalıysa; sentetik çerçeve bunu garanti eder).
 *   3. rsp = rax → sonraki görevin yığınına geç (bağlam-değiştirme).
 *   4. pop r15,r14,r13,r12,rbp,rbx → sonraki görevin callee-saved'ı (ters sıra).
 *   5. ret → sonraki görevin dönüş-adresi (kaldığı yer / ilk-geçişte giriş).
 *
 * clobber/yield ABI: C tarafından normal fonksiyon gibi çağrılır. Naked olduğu
 * için derleyici hiçbir register'ı korumaz — biz callee-saved'ı push/pop ile,
 * caller-saved'ı ise C çağrı-noktasının zaten koruduğunu varsayarız (yield'i
 * çağıran C kodu caller-saved'ı canlıysa kendi kaydeder).
 */
extern void sched_yield(void);
__asm__(
    ".text\n"
    ".global sched_yield\n"
    "sched_yield:\n"
    "    pushq %rbx\n"
    "    pushq %rbp\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    movq %rsp, %rdi\n"        /* arg0 = mevcut RSP */
    "    call sched_sec\n"        /* rax = sonraki görevin RSP'si */
    "    movq %rax, %rsp\n"        /* bağlam-değiştir */
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %rbp\n"
    "    popq %rbx\n"
    "    ret\n"
);

/*
 * === Görev başlangıç yığını (sentetik çerçeve) ===
 * Görev henüz hiç yield etmedi → yığınında gerçek kaydedilmiş bağlam yok. İlk
 * kez ona geçildiğinde sched_yield'ın pop dizisi + ret çalışır. Bu yüzden yığını
 * yield'ın BEKLEDİĞİ düzende kurarız (yukarıdan aşağı, aşağı-büyür yığın):
 *
 *   [dönüş-adresi = giris]   ← ret bunu pop eder → giriş'e atlar
 *   [6× callee-saved = 0]    ← pop r15..rbx bunları yükler (sıfır)
 *   ← tcb[i].rsp buraya işaret eder (en düşük adres = pop başlangıcı)
 *
 * Hiza: ret'in atlayacağı giriş fonksiyonu çağrıldığında, giriş'in kendi
 * push'ları için rsp'nin 16-hizalı olması ABI gereği. Çağrı-noktasında (ret
 * sonrası) rsp = dönüş-adresi slotunun HEMEN ÜSTÜ olur. dönüş-adresi slotunu
 * 16-hizaya oturtursak, ret sonrası rsp 16-hizalı (giriş bir 'call' ile
 * çağrılmış gibi görünür → ABI tutarlı).
 */
static uint64_t *gorev_yigin_kur(uint8_t *yigin, uint64_t boyut, void (*giris)(void)) {
    uint64_t tepe = (uint64_t)(uintptr_t)(yigin + boyut);
    tepe &= ~(uint64_t)0xF;                 /* 16-hizala */
    uint64_t *sp = (uint64_t *)(uintptr_t)tepe;

    /* dönüş-adresi slotu 16-hizalı olacak şekilde: tepe zaten 16-hizalı; bir
     * slot in → 8-hizalı (dönüş-adresi), pop dizisi (6 slot) sonrası ret'te bu
     * slot pop edilir ve rsp tepe'ye (16-hizalı) döner → giriş ABI-uyumlu. */
    *(--sp) = (uint64_t)(uintptr_t)giris;   /* dönüş-adresi = görev girişi */

    /* 6 callee-saved slotu (r15,r14,r13,r12,rbp,rbx pop sırası) → sıfır. */
    for (int k = 0; k < 6; k++) {
        *(--sp) = 0ULL;
    }
    return sp;   /* tcb[i].rsp bu değere set edilir */
}

/* === İşçi görevler === */
static void gorev_govde(uint32_t idx, const char *etiket) {
    for (int t = 0; t < TUR_SAYISI; t++) {
        kdl_yaz_metin(etiket);
        kdl_yazdir_satir();
        sched_yield();              /* CPU'yu sonraki göreve bırak */
    }
    /* Turlar bitti → kendini BİTMİŞ işaretle, sonra sonsuza dek yield-spin.
     * scheduler bitmiş görevi atlar → bir daha buraya dönülmez. Yine de yield
     * çağırmalıyız ki kontrol sonraki göreve geçsin (aksi halde bu görevde
     * kalırız). Son yield'den sonra kontrol asla geri gelmez. */
    bitti[idx] = 1;
    for (;;) {
        sched_yield();
    }
}

__attribute__((noreturn)) static void gorev_a(void) { gorev_govde(1, "[A]"); halt(); }
__attribute__((noreturn)) static void gorev_b(void) { gorev_govde(2, "[B]"); halt(); }
__attribute__((noreturn)) static void gorev_c(void) { gorev_govde(3, "[C]"); halt(); }

int main(void) {
    kdl_yazdir_metin("SCHED X86 BASLA");
    kdl_yazdir_satir();

    /* main = görev 0 (kendi TCB'si ilk yield'de doldurulur). */
    aktif = 0;

    /* A/B/C görevlerinin sentetik başlangıç yığınlarını kur. */
    tcb[1].rsp = (uint64_t)(uintptr_t)gorev_yigin_kur(yigin_a, sizeof(yigin_a), gorev_a);
    tcb[2].rsp = (uint64_t)(uintptr_t)gorev_yigin_kur(yigin_b, sizeof(yigin_b), gorev_b);
    tcb[3].rsp = (uint64_t)(uintptr_t)gorev_yigin_kur(yigin_c, sizeof(yigin_c), gorev_c);

    /*
     * main döngüsü: A/B/C bitene kadar yield et. Her yield round-robin sonraki
     * READY göreve geçer → main→A→B→C→main→... Çıktı interleave: [A][B][C]...
     * Emniyet üst sınırı: sonsuz döngü olmasın diye bounded (GOREV*TUR + pay).
     */
    int guvenlik = 0;
    while (!(bitti[1] && bitti[2] && bitti[3])) {
        sched_yield();
        if (++guvenlik > (GOREV_SAYISI * TUR_SAYISI * 4 + 32)) {
            /* Beklenmedik: görevler bitmedi → deterministik başarısızlık. */
            kdl_yazdir_metin(
                "SCHED X86 BASARISIZ (gorevler bitmedi — yield/context-switch calismiyor)");
            halt();
        }
    }

    /* Hepsi bitti → 3 görev yield ile dönüşümlü koştu. */
    kdl_yazdir_metin(
        "SCHED X86 OK (gorev A/B/C yield ile donusumlu kostu, kooperatif baglam-degistirme)");
    halt();
    return 0;
}
