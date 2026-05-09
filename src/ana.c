#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dosya_oku(const char *dosya_adi) {
    FILE *f = fopen(dosya_adi, "rb");
    if (!f) { fprintf(stderr, "Dosya acilamadi: %s\n", dosya_adi); return NULL; }
    fseek(f, 0, SEEK_END);
    long boyut = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *tampon = (char *)malloc(boyut + 1);
    if (!tampon) { fclose(f); return NULL; }
    fread(tampon, 1, boyut, f);
    tampon[boyut] = '\0';
    fclose(f);
    return tampon;
}

static char *stdin_oku(void) {
    size_t kapasite = 4096;
    size_t uzunluk = 0;
    char *tampon = (char *)malloc(kapasite);
    if (!tampon) return NULL;
    int c;
    while ((c = getchar()) != EOF) {
        if (uzunluk + 1 >= kapasite) {
            kapasite *= 2;
            char *yeni = (char *)realloc(tampon, kapasite);
            if (!yeni) { free(tampon); return NULL; }
            tampon = yeni;
        }
        tampon[uzunluk++] = (char)c;
    }
    tampon[uzunluk] = '\0';
    return tampon;
}

int main(int argc, char *argv[]) {
    char *kaynak;
    const char *dosya_adi;

    if (argc > 1) {
        dosya_adi = argv[1];
        kaynak = dosya_oku(dosya_adi);
    } else {
        dosya_adi = "<stdin>";
        kaynak = stdin_oku();
    }

    if (!kaynak) { fprintf(stderr, "Kaynak okunamadi\n"); return 1; }

    Lexer l;
    lexer_baslat(&l, kaynak, dosya_adi);

    Token t;
    do {
        t = lexer_sonraki_token(&l);
        printf("%-20s \"%.*s\"\t\t%d:%d\n",
               token_tipi_adi(t.tip), t.uzunluk, t.baslangic, t.satir, t.sutun);
    } while (t.tip != TOK_DOSYA_SONU);

    free(kaynak);
    return 0;
}
