#include <stdio.h>
#include <stdlib.h>
typedef struct KdlBolge KdlBolge;
KdlBolge *kdl_global_bolge_al(void);
int kdl_bolge_blok_sayisi(const KdlBolge *b);
int kem_main(void);
int main(void){ int r = kem_main(); printf("BLOK %d RC %d\n", kdl_bolge_blok_sayisi(kdl_global_bolge_al()), r); return 0; }
