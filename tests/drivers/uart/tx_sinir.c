/* Sınırdaki son hazır okuma, kalıcı meşgul aygıt ve toparlanma. */
#include <stdint.h>
#include "kdl_uart.h"

#if defined(TEST_16550)
extern uint32_t kdl_uart_16550_tx_kayip;
#define PUT kdl_uart_16550_putc
#define BEKLE kdl_uart_16550_mock_thre_bekle
#define KAYIP kdl_uart_16550_tx_kayip
#define KONUM kdl_uart_16550_mock_pos
#define TAMPON kdl_uart_16550_mock_buf
#else
extern uint32_t kdl_uart_pl011_tx_kayip;
#define PUT kdl_uart_pl011_putc
#define BEKLE kdl_uart_pl011_mock_fr_full_kalan
#define KAYIP kdl_uart_pl011_tx_kayip
#define KONUM kdl_uart_pl011_mock_pos
#define TAMPON kdl_uart_pl011_mock_buf
#endif

int main(void) {
    BEKLE = UINT32_MAX;
    PUT('x');
    PUT('y');
    if (KONUM != 0 || KAYIP != 2) return 1;
    BEKLE = 99999;
    PUT('a');
    if (KONUM != 1 || TAMPON[0] != 'a' || KAYIP != 2) return 2;
    BEKLE = 100000;
    PUT('b');
    if (KONUM != 1 || KAYIP != 3) return 3;
    PUT('c');
    return KONUM == 2 && TAMPON[1] == 'c' && KAYIP == 3 ? 0 : 4;
}
