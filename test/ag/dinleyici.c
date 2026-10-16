/* [D-466] Tek baglanti kabul eden minimal TCP dinleyici — YALNIZ TEST icin.
 * `calistir_ag_kosum` kapisi bunu baslatir, KEMGU istemcisi baglanir.
 * Neden C: KEMGU V1'de dinleyici API'si YOK (bilincli kapsam karari); kapinin
 * pozitif yolu olcebilmesi icin karsi tarafi baska bir dilde kurmak gerekti. */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define KAPAT closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#define KAPAT close
#define SOCKET int
#define INVALID_SOCKET (-1)
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    SOCKET ls, cs;
    struct sockaddr_in a;
    char buf[512];
    int r, port = 58421, opt = 1;
#ifdef _WIN32
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd)) return 1;
#endif
    if (argc > 1) port = atoi(argv[1]);
    ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == INVALID_SOCKET) return 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(ls, (struct sockaddr *)&a, sizeof(a))) return 1;
    if (listen(ls, 1)) return 1;
    printf("HAZIR\n"); fflush(stdout);
    cs = accept(ls, NULL, NULL);
    if (cs == INVALID_SOCKET) return 1;
    r = (int)recv(cs, buf, (int)sizeof(buf) - 1, 0);
    if (r > 0) { buf[r] = '\0'; printf("ALDIM:%s\n", buf); fflush(stdout); }
    send(cs, "MERHABA-KEMGU", 13, 0);
    KAPAT(cs); KAPAT(ls);
    return 0;
}
