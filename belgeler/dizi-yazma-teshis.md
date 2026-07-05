# ARM64 bare-metal dizi-yazma teshisi

Bu rapor sadece teshistir. `src/llvm.c`, `src/tip_kontrol.c`, `selfhost/codegen.kem`
ve `runtime/*` degistirilmedi.

## Ozet

Stack/literal dizi uzerinde `alan[i] = v` ve `alan[1] = v` yazmalari LLVM IR'a
eleman-store olarak dusmuyor. Dizi ilk degerleri dogru yaziliyor, dizi okuma
dogru calisiyor, fakat indeks-yazma ifadesi no-op kaliyor. Bu nedenle QEMU'da
yazilan deger yerine eski eleman okunuyor.

Bug `tam8`-e ozel degil: `tam8`, `tam32`, `tam64` stack dizilerinde ayni.
GEP eleman tipi okuma tarafinda dogru (`i8`, `i32`, `i64`), alignment/eleman
boyutu birinci supheli degil. Birinci supheli: codegen'in atama sol-tarafi
`DUGUM_INDEKS` oldugunda RHS'i hesaplayip hedef elemana `store` etmemesi.

Heap dizi ayri bir bosluk: `dizi_olustur/dizi_ekle/dizi_al` host runtime ile
calisiyor, fakat bare-metal linkte `kdl_dizi_*` sembolleri yok. Bu nedenle heap
hucresi QEMU-boot'a ulasamiyor.

## Test matrisi

| Test | Eksen | Yazilan | Okunan | Esit mi? | Kanit |
|---|---|---:|---:|---|---|
| `diag_yaz_tam8.kem` | `Dizi<tam8>` stack | 65 | 22 | HAYIR | QEMU: `DIAG tam8 HAYIR`, sonra `22` |
| `diag_yaz_tam32.kem` | `Dizi<tam32>` stack | 57005 | 22 | HAYIR | QEMU: `DIAG tam32 HAYIR`, sonra `22` |
| `diag_yaz_tam64.kem` | `Dizi<tam64>` stack | 18838586676582 | 22 | HAYIR | QEMU: `DIAG tam64 HAYIR`, sonra `22` |
| `diag_syntax_indeks.kem` | `alan[i] = v` | 9001 | 200 | HAYIR | QEMU: `DIAG syntax_indeks HAYIR`, sonra `200` |
| `diag_syntax_sabit_indeks.kem` | `alan[1] = v` | 9001 | 200 | HAYIR | QEMU: `DIAG syntax_sabit_indeks HAYIR`, sonra `200` |
| `diag_syntax_dizi_yaz.kem` | `dizi_yaz(alan,i,v)` | 9001 | yok | QEMU yok | `--check`: `T002 tanimsiz sembol` |
| `diag_heap_dizi.kem` | `dizi_olustur` heap + `dizi_ekle/dizi_al` | 57005 | 57005 host'ta | QEMU yok | bare-metal link: `kdl_dizi_*` undefined |
| `diag_stack_dizi.kem` | yerel/literal stack dizi | 4444 | 8 | HAYIR | QEMU: `DIAG stack_dizi HAYIR`, sonra `8` |
| `diag_sadece_oku.kem` | sadece okuma kontrolu | yok | 4444 | EVET | QEMU: `DIAG sadece_oku EVET / beklenen=4444 / oku=4444` |

## Calistirilan dogrulama

Tip kontrolu gecenler:

```text
OK: diag_yaz_tam8.kem
OK: diag_yaz_tam32.kem
OK: diag_yaz_tam64.kem
OK: diag_syntax_indeks.kem
OK: diag_heap_dizi.kem
OK: diag_stack_dizi.kem
OK: diag_sadece_oku.kem
OK: diag_syntax_sabit_indeks.kem
```

`diag_syntax_dizi_yaz.kem`:

```text
hata[T002]: tanimsiz sembol
  --> test\ornekler\diag_syntax_dizi_yaz.kem:9:5
9 |     dizi_yaz(alan, i, yazilan);
  |     ^
```

QEMU stdout:

```text
--- diag_yaz_tam8 ---
DIAG tam8 HAYIR / yaz=65 / oku:
22
--- diag_yaz_tam32 ---
DIAG tam32 HAYIR / yaz=57005 / oku:
22
--- diag_yaz_tam64 ---
DIAG tam64 HAYIR / yaz=18838586676582 / oku:
22
--- diag_syntax_indeks ---
DIAG syntax_indeks HAYIR / yaz=9001 / oku:
200
--- diag_syntax_sabit_indeks ---
DIAG syntax_sabit_indeks HAYIR / yaz=9001 / oku:
200
--- diag_stack_dizi ---
DIAG stack_dizi HAYIR / yaz=4444 / oku:
8
--- diag_sadece_oku ---
DIAG sadece_oku EVET / beklenen=4444 / oku=4444
```

Heap dizi bare-metal link kaniti:

```text
ld.lld: error: undefined symbol: kdl_dizi_olustur
ld.lld: error: undefined symbol: kdl_dizi_kapasite_ayarla
ld.lld: error: undefined symbol: kdl_dizi_ekle_tam
ld.lld: error: undefined symbol: kdl_dizi_al_tam
```

Host runtime ayrimi:

```text
DIAG heap_dizi EVET / yaz=57005 / oku=57005
exit=0
```

Bu host sonucu QEMU yerine gecmez; sadece heap intrinsic yolunun host runtime'da
canli oldugunu ve bare-metal boslugunun runtime sembol eksigi oldugunu ayirir.

## IR bulgulari

### `diag_yaz_tam32.ll`

Kaynak:

```kemgu
değişken alan = [11, 22, 33];
alan[i] = yazilan;
değişken okunan: tam32 = alan[i];
```

IR'da dizi ilk degerleri icin store var:

```llvm
%7 = alloca [3 x i32]
%8 = getelementptr [3 x i32], ptr %7, i32 0, i32 0
store i32 %4, ptr %8
%9 = getelementptr [3 x i32], ptr %7, i32 0, i32 1
store i32 %5, ptr %9
%10 = getelementptr [3 x i32], ptr %7, i32 0, i32 2
store i32 %6, ptr %10
```

Sonra yazma beklenen noktada `store i32 <yazilan>, ptr <alan+i>` yok. Bir sonraki
dizi islemi okuma:

```llvm
%14 = load i32, ptr %0
%16 = getelementptr i32, ptr %13, i64 %15
%17 = load i32, ptr %16
store i32 %17, ptr %12
```

Bu desen QEMU sonucunu acikliyor: `alan[1]` hic guncellenmedigi icin eski `22`
okunuyor.

### `diag_yaz_tam8.ll` ve `diag_yaz_tam64.ll`

`tam8` IR'i stack diziyi gercekten byte dizi olarak kuruyor:

```llvm
%13 = alloca [3 x i8]
%22 = getelementptr i8, ptr %19, i64 %21
%23 = load i8, ptr %22
```

`tam64` IR'i de dogru eleman tipini kullaniyor:

```llvm
%13 = alloca [3 x i64]
%22 = getelementptr i64, ptr %19, i64 %21
%23 = load i64, ptr %22
```

Iki dosyada da yazilan degerin eleman adresine store edilmesi yok. Bu yuzden
bug eleman boyutu veya sign-extension degil.

### Sabit indeks

`diag_syntax_sabit_indeks.kem` icin `alan[1] = yazilan` da no-op:

```llvm
%9 = add i32 0, 9001
store i32 %9, ptr %8        ; bu sadece yazilan yerel degiskeni
%13 = getelementptr i32, ptr %11, i64 %12
%14 = load i32, ptr %13     ; sonraki islem okuma
```

Sabit indeks de bozuk oldugu icin sorun sadece dinamik indeks hesabinda degil.

### Sadece okuma

`diag_sadece_oku.ll` okuma tarafinda beklenen init-store + GEP + load desenini
uretiyor ve QEMU'da `4444` okuyor. Bu, genel dizi erisiminin degil, indeks-yazma
atamasinin bozuk oldugunu izole eder.

## Kok-neden hipotezi

Kok-neden buyuk olasilikla LLVM codegen'de atama LHS handling eksigi:

- `alan[i]` ifade olarak okunurken GEP + load uretiliyor.
- Dizi literal initialization icin GEP + store uretiliyor.
- Ama `alan[i] = v` statement/expression icin GEP + RHS store uretilmiyor.
- Bu eksiklik `tam8/tam32/tam64` ve sabit/degisken indekslerde ayni.

Dolayisiyla bu, ARM64 store alignment veya eleman boyutu bug'i gibi durmuyor.
ARM64 bare-metal bunu gorunur kildi, fakat yanlislik IR seviyesinde mevcut.
Heap/stack descriptor karisikligina benzer ikinci bir sinir da var: heap dizi
bare-metal runtime'da yok, stack dizi ise dogrudan GEP yolunda. Ancak asil
indeks-yazma bug'i stack GEP yolunda `store` uretilmemesi.
