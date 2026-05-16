/*
 * KEMGU Bare-Metal Runtime — UART (Universal Asynchronous Receiver-
 * Transmitter) Sürücü Arayüzü
 * ================================================================
 *
 * Bu başlık, hedef bağımsız UART sürücülerinin ortak imzasını ve
 * test mock altyapısını bildirir. Gerçek implementasyonlar:
 *
 *   - runtime/kdl_runtime_uart_pl011.c  — ARM64 (ARM PrimeCell PL011)
 *   - runtime/kdl_runtime_uart_16550.c  — x86_64 (NS16550A / COM portu)
 *
 * Kullanım örüntüsü (yüksek seviye baskı, runtime/kdl_runtime_yazdir_bare.c
 * içinden):
 *
 *   extern void kdl_uart_pl011_putc(char c);
 *   void kdl_yazdir_metin(const char *s) {
 *       while (*s) kdl_uart_pl011_putc(*s++);
 *   }
 *
 * Heap YOK, libc YOK. Her sürücü kendi adlandırma alanında — final
 * bağlama yalnızca tek bir backend'i sürer.
 *
 * Test modu (KEMGU_UART_MOCK): MMIO/port erişimi gerçek donanım yerine
 * sürücü-özel global tampona yönelir. Bu sayede Windows host üzerinde
 * driver mantığı sentetik biçimde doğrulanabilir.
 *
 * Gerçek bare-metal modu (KEMGU_BARE_METAL): Mock kapalı; volatile MMIO
 * yazımı veya x86 port I/O üretilir.
 *
 * Her iki bayrak da tanımlı değilse: dosya derlenebilir ama bu host
 * yürütme için anlamsız — kdl_pl011 sembolleri yine de görünür, çünkü
 * çağrı yapılırsa pointer dereference segfault verir. CI bunu yakalamak
 * için iki mod'dan birini her zaman seçmelidir.
 */

#ifndef KDL_UART_H
#define KDL_UART_H

#include <stdint.h>

/* === ARM PL011 (ARM64) sürücü API'si === */

/* Sürücüyü başlat. V1 = no-op (QEMU virt veya firmware-init'li board
 * varsayılır). Gerçek donanım için baud rate, LCR_H, CR kayıtları
 * burada yazılır — board-özel. */
void kdl_uart_pl011_init(void);

/* Tek karakter yaz — FIFO dolu ise dolmayana kadar bekler. */
void kdl_uart_pl011_putc(char c);

/* NUL-sonlandırmalı diziyi sırayla yaz. NULL girdi no-op. */
void kdl_uart_pl011_yaz(const char *s);

/* RX yönü: tek karakter oku — FIFO boş ise dolana kadar bekler.
 * Return: 0-255 (8-bit veri), tampona yerleştirilmiş ham byte. */
int32_t kdl_uart_pl011_oku_karakter(void);

/* RX hazır mı? Bloklamadan kontrol. 1 = veri var, 0 = boş. */
int32_t kdl_uart_pl011_rx_hazir(void);

/* === x86_64 NS16550A (COM1) sürücü API'si === */

void kdl_uart_16550_init(void);
void kdl_uart_16550_putc(char c);
void kdl_uart_16550_yaz(const char *s);
int32_t kdl_uart_16550_oku_karakter(void);
int32_t kdl_uart_16550_rx_hazir(void);

/* === Cross-target UART sürücü tablosu (vtable) ===
 *
 * Üst katman kod hedef bağımsız çalışmak istiyorsa bu yapıyı kullanır:
 *
 *   const KdlUartSurucu *u = &kdl_uart_pl011_surucu;  // veya 16550
 *   u->init();
 *   u->yaz("Merhaba\n");
 *
 * Fonksiyon işaretçileri indirect call üretir (bare-metal'da ~1 cycle ek
 * maliyet). Tek hedefli binary'ler için doğrudan kdl_uart_pl011_putc
 * çağrısı daha hızlı.
 */
typedef struct {
    void    (*init)(void);
    void    (*putc)(char c);
    void    (*yaz)(const char *s);
    int32_t (*oku_karakter)(void);
    int32_t (*rx_hazir)(void);
} KdlUartSurucu;

extern const KdlUartSurucu kdl_uart_pl011_surucu;
extern const KdlUartSurucu kdl_uart_16550_surucu;

/* === Test mock altyapısı (yalnızca KEMGU_UART_MOCK tanımlıysa) ===
 *
 * Her sürücü kendi tamponuna yazar — aynı test binary'sine birden çok
 * sürücü derlenirse çakışma olmaz. Test kodu sürücüyü çağırır, sonra
 * tampon içeriği ve pozisyonu üzerinde doğrulama yapar.
 */

#ifdef KEMGU_UART_MOCK

#define KDL_UART_MOCK_BUF_SZ 1024U

/* PL011 mock — kdl_uart_pl011_putc her çağrıda buf[pos++]'a yazar. */
extern char     kdl_uart_pl011_mock_buf[KDL_UART_MOCK_BUF_SZ];
extern uint32_t kdl_uart_pl011_mock_pos;
/* >0 ise FR.TXFF biti set döndürülür ve her okumada 1 azaltılır.
 * Test bu sayede "FIFO dolu" sahnesini simüle eder. */
extern uint32_t kdl_uart_pl011_mock_fr_full_kalan;
/* RX mock — test, oku_karakter çağrılmadan önce mock_rx_doldur ile
 * tampona veri yerleştirir. Her okuma rx_pos'u ilerletir. */
extern char     kdl_uart_pl011_mock_rx_buf[KDL_UART_MOCK_BUF_SZ];
extern uint32_t kdl_uart_pl011_mock_rx_uzunluk;
extern uint32_t kdl_uart_pl011_mock_rx_pos;
void kdl_uart_pl011_mock_temizle(void);
void kdl_uart_pl011_mock_rx_doldur(const char *veri, uint32_t n);

/* 16550A mock — kdl_uart_16550_putc her çağrıda buf[pos++]'a yazar. */
extern char     kdl_uart_16550_mock_buf[KDL_UART_MOCK_BUF_SZ];
extern uint32_t kdl_uart_16550_mock_pos;
/* >0 ise LSR.THRE biti SIFIR döndürülür (THR henüz boş değil) ve her
 * okumada 1 azaltılır. */
extern uint32_t kdl_uart_16550_mock_thre_bekle;
extern char     kdl_uart_16550_mock_rx_buf[KDL_UART_MOCK_BUF_SZ];
extern uint32_t kdl_uart_16550_mock_rx_uzunluk;
extern uint32_t kdl_uart_16550_mock_rx_pos;
void kdl_uart_16550_mock_temizle(void);
void kdl_uart_16550_mock_rx_doldur(const char *veri, uint32_t n);

#endif /* KEMGU_UART_MOCK */

#endif /* KDL_UART_H */
