# KEMGU LLVM Backend

KEMGU AST'yi LLVM IR text formatına çeviren modül. `libLLVM` bağımlılığı
**yok** — sadece string output üretiyor; sonra `clang -x ir -` veya
`llc` ile native koda derleniyor.

Modül: `src/llvm.h`/`src/llvm.c`

## Pipeline

```
.kem (KEMGU kaynak)
   │
   ▼  Lexer + Parser
AST
   │
   ▼  Tip kontrol + Bölge atama
AST (tipe bağlanmış)
   │
   ▼  llvm_ir_uret_target(prog, out, triple)
LLVM IR text  (~/.ll)
   │
   ▼  clang -x ir -c
.o (object file, ELF veya COFF)
   │
   ▼  ld.lld -m elf_x86_64 -T linker.ld
ELF / PE executable
```

KEMGU CLI tüm pipeline'ı tek komut altında sarar:

```bash
kemgu --build prog.kem -o prog.exe              # tek dosya derleme + link
kemgu --build a.kem b.kem -o app                # çoklu dosya
kemgu -c lib.kem -o lib.o                       # sadece obj
kemgu --build --freestanding --target=x86_64-unknown-none \
      --linker-script=kernel.ld kernel.kem -o kernel.elf  # bare-metal
```

## Tasarım: Alloca Tabanlı

Clang `-O0` modelini taklit eder. Her değişken/parametre için `alloca`
emit edilir, okuma `load`, yazma `store`.

**Avantajları:**
- PHI nodu yok — kontrol akışı basit
- LLVM `mem2reg` opt pass'i sonradan SSA'ya çevirir (gerekirse)
- Mutability default

**Dezavantajı:** Optimizasyon yok — sonra `clang -O2` ile düzeltilir.

```llvm
; KEMGU: değişken x: tam32 = 5; x = x + 1;
%x.addr = alloca i32
store i32 5, ptr %x.addr
%1 = load i32, ptr %x.addr
%2 = add i32 %1, 1
store i32 %2, ptr %x.addr
```

## Tip Eşlemesi

| KEMGU | LLVM IR |
|-------|---------|
| `tam8/dtam8` | `i8` |
| `tam16/dtam16` | `i16` |
| `tam32/dtam32` | `i32` |
| `tam64/dtam64` | `i64` |
| `kesirli32` | `float` |
| `kesirli64` | `double` |
| `mantıksal` | `i1` |
| `karakter` | `i32` (UTF-32) |
| `metin` | `{ ptr, i64 }` (slice: ptr + uzunluk) |
| `boş` | `void` |
| `*T`, `&T` | `ptr` (opaque pointer) |
| `Dizi<T>` | `{ ptr, i64 }` (slice) |
| `yapı X { ... }` | `%struct.X = type { ... }` |
| `seçimlik<T>` | `{ i32, i64 }` (tag + payload) — taslak |
| `sonuç<T,H>` | `{ i32, i64 }` — taslak |
| `işlev(...) -> T` | `ptr` (function pointer) |

## Sembol Tablosu

`LLVMSem` struct'ı:

```c
typedef struct {
    const char *ad;
    int ad_uz;
    char yer_ssa[64];      // alloca pointer: "%x.addr" veya "@VGA_ADRES"
    char llvm_tip[128];    // "i32", "%struct.Foo", "{ ptr, i64 }"
    char eleman_tip[128];  // Dizi<T> ise T'nin LLVM tipi
    int kategori;          // 0=local, 1=fn ptr, 2=closure (env ptr)
    int lambda_no;         // closure için lambda ID
} LLVMSem;
```

Flat array `semboller[512]`. Scope (blok girişi/çıkışı) `sem_say`
snapshot ile yönetilir.

## Üretilenler

### Toplam IR çıktısı

```llvm
; KEMGU LLVM IR (alloca tabanli)
target triple = "x86_64-pc-windows-gnu"   ; veya --target=...

; 1. Yapı tanımları
%struct.Nokta = type { i32, i32 }

; 2. Sabit globalları
@VGA_ADRES = constant i64 753664
@BAYTLAR.data = private constant [4 x i8] [i8 65, i8 66, i8 67, i8 68]
@BAYTLAR = constant { ptr, i64 } { ptr @BAYTLAR.data, i64 4 }

; 3. İşlevler
define i32 @main() {
entry:
  ...
}

; 4. Lifted lambdalar
define i32 @__lambda_0(ptr %.env, i32 %.p0) {
  ...
}

; 5. Harici declare'lar (çoklu dosya için)
declare i32 @other_function(i32)

; 6. Attribute groups
attributes #0 = { naked }

; 7. String literal globals
@.str0 = private unnamed_addr constant [7 x i8] [i8 77, i8 101, ...]
@.s0 = private unnamed_addr constant { ptr, i64 } { ptr @.str0, i64 7 }
```

## Kontrol Akışı

Basic block + label + br:

### eğer/değilse
```llvm
%cond = ...
br i1 %cond, label %if.then.0, label %if.else.0
if.then.0:
  ...
  br label %if.end.0
if.else.0:
  ...
  br label %if.end.0
if.end.0:
```

### iken
```llvm
br label %while.cond.0
while.cond.0:
  %c = ...
  br i1 %c, label %while.body.0, label %while.end.0
while.body.0:
  ...
  br label %while.cond.0
while.end.0:
```

### için (Dizi<T> üzerinde)
```llvm
%ptr = extractvalue { ptr, i64 } %slice, 0
%len = extractvalue { ptr, i64 } %slice, 1
%i.addr = alloca i64
store i64 0, ptr %i.addr
br label %for.cond.0
for.cond.0:
  %i = load i64, ptr %i.addr
  %c = icmp slt i64 %i, %len
  br i1 %c, label %for.body.0, label %for.end.0
for.body.0:
  %ep = getelementptr inbounds i32, ptr %ptr, i64 %i
  %v = load i32, ptr %ep
  store i32 %v, ptr %x.addr     ; x = arr[i]
  ; ... gövde
  %i2 = load i64, ptr %i.addr
  %i3 = add i64 %i2, 1
  store i64 %i3, ptr %i.addr
  br label %for.cond.0
for.end.0:
```

### eşleş (literal pattern chain)
Her kol için `icmp eq` + branch; joker `_` ve tanımlayıcı her zaman match.

## Sistem Programlama Intrinsic'leri

### Inline asm
```llvm
call void asm sideeffect "hlt", ""()
```

### Volatile MMIO
```llvm
%p = inttoptr i64 %addr to ptr
store volatile i32 %val, ptr %p
%r = load volatile i32, ptr %p
```

### Atomic
```llvm
%v = load atomic i32, ptr %p seq_cst, align 4
store atomic i32 %val, ptr %p seq_cst, align 4
%old = atomicrmw add ptr %p, i32 %val seq_cst
%old = atomicrmw xchg ptr %p, i32 %val seq_cst
%pair = cmpxchg ptr %p, i32 %old_exp, i32 %new seq_cst seq_cst
%ok = extractvalue { i32, i1 } %pair, 1
fence seq_cst   ; veya acquire / release
```

### Calling convention attribute'ları

```kem
[ciplak]                      // naked function
[kesme]                       // interrupt handler
[bolum: ".text.boot"]         // linker section
[ciplak, bolum: ".multiboot"] // birleşik
```

Karşılığı:
```llvm
define void @f() #0 section ".text.boot" {  // naked
  ; prologue/epilogue yok
}
attributes #0 = { naked }

define x86_intrcc void @kesme_isleyici() {  // interrupt handler
  ; x86 interrupt calling convention
}
```

## Closure (Free Var Capture)

Lambda free var analizi → env struct → caller'da alloca + fill →
lambda function `(ptr env, ...)` imzalı:

```kem
işlev cagran() -> tam32 {
    değişken y: tam32 = 10;
    değişken topla = |x: tam32| x + y;   // y captured
    ver topla(32);
}
```

```llvm
define i32 @cagran() {
entry:
  %y.addr = alloca i32
  store i32 10, ptr %y.addr
  %env = alloca { i32 }                                  ; capture struct
  %y.v = load i32, ptr %y.addr
  %env.y = getelementptr inbounds { i32 }, ptr %env, i32 0, i32 0
  store i32 %y.v, ptr %env.y
  %arg = add i32 0, 32
  %r = call i32 @__lambda_0(ptr %env, i32 %arg)
  ret i32 %r
}

define i32 @__lambda_0(ptr %.env, i32 %.p0) {
entry:
  %y.gep = getelementptr inbounds { i32 }, ptr %.env, i32 0, i32 0
  %y.v = load i32, ptr %y.gep
  %y.addr = alloca i32
  store i32 %y.v, ptr %y.addr      ; binding: y artık normal local
  %x.addr = alloca i32
  store i32 %.p0, ptr %x.addr
  %x = load i32, ptr %x.addr
  %y = load i32, ptr %y.addr
  %r = add i32 %x, %y
  ret i32 %r
}
```

Non-capturing lambda → env yok, sadece fn ptr.

## Çoklu Dosya + Linker

`kemgu f1.kem f2.kem` → AST'ler tek `DUGUM_PROGRAM` altında birleşir.
Aynı global scope'ta tip kontrol.

`-c` mode object file üretir; harici çağrılar `declare` ile emit edilir:
```llvm
; ana.o.ll
%0 = call i32 @kare(i32 5)
...
declare i32 @kare(i32)
```

Linker (`ld.lld`) `kare` sembolünü `kare.o`'da bulup bağlar.

## Cross-Compile

`--target=TRIPLE` LLVM IR'in `target triple = "..."` satırına yansır
ve `clang -target ...` olarak verilir:

```bash
kemgu --target=aarch64-unknown-none --freestanding -c kernel.kem -o k.o
# ELF 64-bit ARM aarch64
```

`--freestanding` clang'e `-nostdlib -ffreestanding -fno-builtin
-fno-stack-protector` geçirir. libc yok, custom `_start` zorunlu.

## Test

Şu an LLVM backend için doğrudan unit test yok — entegrasyon testleri:
- `test/ornekler/*.kem` snapshot AST testleri
- `test/ornekler/faz1_kapsamli.kem` → `exit 42` (end-to-end)
- `test/ornekler/kernel/`, `kernel_arm64/` → ELF üretim doğrulama

## Bilinen Sınırlamalar

- **Generic monomorphization codegen yok** — `Kutu<tam32>` ve `Kutu<metin>`
  şu an aynı `%struct.Kutu`'ya gider (yanlış semantik). Tip kontrol farkında,
  LLVM değil.
- **Optimizasyon yok** — `clang -O2` ile optimize edilebilir ama KEMGU
  kendi opt pass'i çalıştırmıyor
- **Debug bilgisi (DWARF) yok** — DWARF section emit edilmiyor; GDB
  KEMGU kaynak satırına maplayemez
- **Stack overflow detection yok** — guard page setup KEMGU sorumluluğu değil
- **128-bit tipler yok** — `i128` desteği eklenmemiş
- **SIMD intrinsic'leri yok** — `<4 x i32>` vector tipleri henüz yok
- **TLS (thread-local) yok** — kernel için gerekecek
