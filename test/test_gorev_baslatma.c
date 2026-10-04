/*
 * test_gorev_baslatma.c — [D-666] görev oluşturucularında BAŞLATILMAMIŞ ALAN
 *
 * Eski `kdl_gorev_basla_i32` oluşturucusu D-309'da eklenen `rho_serbest`
 * alanını başlatmıyordu; `kdl_gorev_birlestir` onu okuyordu
 * (`if (g->rho_serbest && g->rho)`). MemorySanitizer ile ölçüldü:
 * use-of-uninitialized-value @ kdl_runtime.c:1301. Onarım iki oluşturucuyu
 * da `calloc`a çevirdi — SINIF kapandı: sonradan eklenecek alanlar da sıfırla
 * başlar.
 *
 * ⚠ Bu oluşturucunun derleyicide ÇAĞIRANI YOK (ölü API). Kapı yine de değerli:
 * ölçtüğü şey bu tek çağrı değil, "oluşturucu alanı başlatmadan bırakırsa
 * birleştir onu okur" SINIFIDIR. Bu testi yalnız MSan ile koşmak anlamlıdır;
 * MSan'sız derlemede başlatılmamış okuma çoğu zaman sessizce 0 okur.
 */
#include <stdint.h>
typedef struct KdlGorev KdlGorev;
KdlGorev *kdl_gorev_basla_i32(int32_t (*f)(void));
int64_t kdl_gorev_birlestir(KdlGorev *g);
static int32_t kirk(void) { return 42; }
int main(void) {
    KdlGorev *g = kdl_gorev_basla_i32(kirk);
    if (!g) return 2;
    return (int)kdl_gorev_birlestir(g) == 42 ? 0 : 1;
}
