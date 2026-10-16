> **⚠ ÖLÇÜM GEÇERSİZ — bütçe artefaktı (2026-10-11).** 16384 token'ın tamamı düşünmeye gitti (`reasoning_budget_exhausted: true`, `content` boş). Düşünme metni teknik olarak doğru yolda (COM1 `0x3F8`, `outb`, LSR yoklaması için `in`) ama adlandırma/bölümleme tartışmasında kalmış. Aşağıdaki KALDI, modelin üretim yeteneğini değil bütçe kesintisini ölçer; karar verilmedi.

# EVREN kalibrasyonu 2 (bağlamlı üretim) — `glm-5.3`

- Tarih: 2026-10-11 21:15:42
- HTTP: 200 · süre: 934.7 sn
- Parametreler: `max_tokens: 16384`, `stream: false`
- finish_reason: `length` · usage: `{"prompt_tokens": 10877, "total_tokens": 27261, "completion_tokens": 16384, "prompt_tokens_details": {"cached_tokens": 0, "created_cache_tokens": 10752, "multimodal_tokens": null}, "completion_tokens_details": {"reasoning_tokens": 16384}, "evren": {"credits_held_cr": "0.0000", "credits_remaining_cr": "1010.0000", "routed_model": "zai/glm-5.3-fp8", "reasoning_budget_exhausted": true}}`
- İstem: [kalibrasyon2_istem.md](kalibrasyon2_istem.md)

## KARAR: KALDI — yanıtta kod bloğu yok

## Yanıt (ham)

_(boş)_

## Düşünme (reasoning)

The task: Write a bare (`çıplak`) UART write function for x86_64 using port-mapped I/O (`out`/`in`) via inline assembly, following the pattern in `kem_heap.kem`. Must not use the `0x09000000` PL011 MMIO path or `kdl_mmio_yaz32`. Must use `satıriçi_asm` block with correct architecture label (x86_64).

Context from the repository examples:

x86_64 inline asm example:
```kemgu
işlev oku_arti_iki(g: tam32) -> tam32 {
    değişken s: tam32 = 0;
    değişken b: tam32 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"mov $2, $0
add $$2, $0
mov $$100, $1"#
            çıktı("=r", &s)
            çıktı("=r", &b)
            girdi("r", g)
            bozulan("~{cc}")
        }
    }
    eğer b != 100 { ver 1; }
    ver s;
}
```

arm64 bare function inside inline asm:
```kemgu
çıplak işlev vblk_dsb() {
    güvensiz {
        satıriçi_asm { mimari: arm64 şablon: r#"dsb sy"# bozulan("~{memory}") }
    }
}
```

Pattern from kem_heap.kem: bare functions (`çıplak işlev`), Turkish keywords: `değişken`, `iken` (while), `eğer` (if), `ver` (return), `olarak` (as/cast), types `tam64`, `tam32`, `tam8`, `dtam8`, pointers `*tam8` etc. Comments in Turkish. Global variables via `küresel değişken`.

The kem_panik function writes to PL011 at 0x09000000 (150994944 = 0x09000000). We must NOT use that. Instead, x86_64 port-mapped I/O. On x86, typical UART is COM1 at port 0x3F8. Writing a byte: `out dx, al` where dx = port, al = value. We can write with `out %al, %dx` in AT&T syntax.

AT&T syntax for out: `out %al, %dx` — outputs to port in DX the byte in AL.

Inline asm with port in a register and value in a register:

```kemgu
çıplak işlev kem_uart_yaz(bayt: tam8) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"out %al, %dx"#
            girdi("{al}", bayt)
            girdi("{dx}", port)
            bozulan("~{memory}")
        }
    }
}
```

Hmm, but constraints: specific register constraints in LLVM inline asm: `"{al}"`, `"{dx}"`. Input operand types — for `{al}` constraint the operand type must be i8; for `{dx}` it must be i16. In the KEMGU compiler, `girdi("r", g)` passes a tam32. What types does the compiler pass? We don't know exactly how the compiler lowers `girdi`/`çıktı` with register-specific constraints; presumably it passes the value as-is with its type. If we pass tam32 to "{dx}", LLVM would complain about operand type mismatch (physical register constraint requires exact type: for x86, `{dx}` requires i16, `{al}` requires i8).

But we don't know the compiler internals. The safest approach that matches the example: use general `"r"` constraints and move into place within the template.

Actually with AT&T template and `$0`, `$1` operands. For `out`, the port must be in dx and the value in al. We can do:

```kemgu
çıplak işlev kem_uart_gonder(port: tam32, bayt: tam32) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"mov $0, %edx
mov $1, %eax
out %al, %dx"#
            girdi("r", port)
            girdi("r", bayt)
            bozulan("~{memory}")
        }
    }
}
```

Wait: `mov $0, %edx` — but the operand `$0` is already a register allocated by the compiler (since constraint "r"), so `mov $0, %edx` would be `mov %reg, %edx`? In LLVM inline-asm, `$0` refers to operand 0 which is a register like `%rax` etc. The template text `mov $0, %edx` becomes `mov %ecx, %edx` e.g. That's a 32-bit move — fine.

But careful: in AT&T syntax with LLVM, immediate `$` needs doubling in the template (`$$100` in the example). Register names in template like `%edx` are fine (literal percent). Actually wait — in LLVM inline asm, `%` in the template is used for register references? No — in LLVM's inline asm (as opposed to GCC), `$` is the operand prefix, and `%%` escapes a literal `$`. Registers written literally as `%edx` are passed through to the assembler as-is; LLVM does not interpret `%reg` in templates. Actually in GCC inline asm, `%%` denotes a literal `%` (register prefix), and `%0` denotes operands. In LLVM, `$` denotes operands and `$$` denotes literal `$`. Since AT&T syntax registers start with `%`, you can write `%edx` directly in LLVM templates without escaping. Yes — in the example: `mov $2, $0`, `add $$2, $0`, `mov $$100, $1` — `$2` is operand, `$$2` is literal immediate 2, `%` not used. And in the arm64 example, `dsb sy` no registers.

Hmm, but there's a subtlety: in LLVM inline asm, if the template contains `$` followed by a digit, it's an operand. Registers `%edx` fine.

Alternatively use specific register constraints:

```
girdi("{dx}", port_i16)
girdi("{al}", val_i8)
```

But type matching issue. The compiler likely emits the operand with the KEMGU type's LLVM type (tam32 → i32). `{dx}` constraint on i32 operand → LLVM error "invalid operand type for inline asm constraint". Unless the compiler truncates. Too risky.

Better to use "r" constraints and move within template. But there's another subtlety: if operands are i32 and we `mov $0, %edx`, that's fine. Then `mov $1, %eax` fine, then `out %al, %dx` writes to port dx the byte in al.

Now, what should the function look like following kem_heap.kem pattern? The task: "x86_64 için port-mapped I/O (out/in) kullanan bir çıplak UART yazma fonksiyonu". So a bare UART write function. Probably write a string? Or write a byte? The kem_panik pattern writes a string via MMIO. The task says "UART yazma fonksiyonu" — a UART write function. Could be byte-level primitive plus a string writer, like the pattern in kem_heap (kem_panik writes a message char-by-char then newline, then halts).

Let me write:

1. A primitive `çıplak işlev kem_uart_port_yaz(port: tam32, deger: tam32)` — out instruction.
2. Maybe `çıplak işlev kem_uart_port_oku(port: tam32) -> tam32` — in instruction (task mentions `out`/`in`).
3. A UART write function that writes a string: init COM1 maybe? Standard COM1 0x3F8. For a simple write: check LSR (port 0x3F8+5) bit 5 (Transmitter holding register empty) via `in al, dx`, then out the byte.

The task: "port-mapped I/O (out/in) kullanan bir çıplak UART yazma fonksiyonu yaz" — write a bare UART write function using port-mapped I/O. The mention of both out and in suggests polling LSR with `in` then `out` the byte. And maybe a string-level writer like kem_panik.

Also "kem_heap.kem'deki desene birebir uyarak" — following the pattern in kem_heap.kem: Turkish comments, bare functions, byte loops, safe patterns. And `metin` type? kem_panik uses `metin` param, `metin_uzunluk`, `metin_bayt` builtins. Those resolve to bare `kdl_metin_*` per the roadmap note: "builtin (`metin_uzunluk` vb., çıplak `kdl_metin_*`'e çözülüyor)". Since kem_heap.kem includes kdl_metin_uzunluk/kdl_metin_bayt functions, a bare function can call `metin_uzunluk(m)` builtin (which resolves to the bare kdl_metin_uzunluk). E013 says bare fn can only call bare/extern — builtins resolve to bare kdl_metin_*, so allowed. Indeed kem_panik in kem_heap.kem uses `metin_uzunluk(m)` and `metin_bayt(m, i)` inside a `çıplak işlev`. So that's established pattern.

So I'll write, for x86_64:

```kemgu
// COM1 taban portu (0x3F8) — x86_64 port-mapped I/O.
çıplak işlev kem_uart_yaz(m: metin) {
    değişken n: tam32 = metin_uzunluk(m);
    değişken i: tam32 = 0;
    iken i < n {
        değişken b: tam8 = metin_bayt(m, i);
        kem_uart_bayt_gonder(b);
        i = i + 1;
    }
}
```

And byte send with LSR polling:

```kemgu
çıplak işlev kem_uart_bayt_gonder(b: tam8) {
    // LSR (0x3F8+5) bit 5: verici tutma yazmacı boş olana dek bekle (in al, dx).
    ...
}
```

Port constants: COM1 = 0x3F8 = 1016. LSR = 0x3F8+5 = 1021.

Implementation of `in`/`out` via inline asm with "r" constraints:

out:
```kemgu
çıplak işlev kem_port_yaz(port: tam32, deger: tam32) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"movl $0, %edx
movb $1, %eax
out %al, %dx"#
            girdi("r", port)
            girdi("r", deger)
            bozulan("~{memory}")
        }
    }
}
```

Hmm wait — `movb $1, %eax`: $1 is operand 1 (a register, 32-bit since tam32). `movb %ecx, %eax` moves low byte. Fine. Actually `movl $0, %edx` moves the 32-bit port into edx. Since ports are 16-bit, upper bits ignored by `out %al, %dx` (uses only dx). OK.

Alternatively, zero-extend not needed.

in:
```kemgu
çıplak işlev kem_port_oku(port: tam32) -> tam32 {
    değişken sonuc: tam32 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"movl $1, %edx
in %dx, %al"#
            çıktı("=r", &sonuc)
            girdi("r", port)
            bozulan("~{memory}")
        }
    }
    ...
}
```

Wait — problem: `in %dx, %al` puts result in al, but the output operand `sonuc` is assigned some register by compiler; the template must move al to $0. But the order: `$0` is the output operand register; we need `movzbl %al, %r?d`... we can't reference $0 after in because the output register is `$0`: 

```
movl $1, %edx
in %dx, %al
movzbl %al, $0    ??? 
```

Hmm — can you use `$0` as a destination in the template? Yes: `mov $100, $1` in the example writes immediate to output operand $1. So `movzbl %al, %0` — wait, template uses `$0` for operand 0. So:

```
şablon: r#"movl $1, %edx
in %dx, %al
movzbl %al, $0"#
çıktı("=r", &sonuc)
girdi("r", port)
```

Operand numbering: outputs first, then inputs (as stated: "çıktılar → girdiler biçiminde"). So $0 = sonuc (output), $1 = port (input). `movzbl %al, %0` — hmm, but wait: `movzbl %al, %ecx` where %ecx is the allocated output register — that's a 32-bit dest. Output type i32, so `$0` expands to a 32-bit register. `movzbl %al, $0` becomes e.g. `movzbl %al, %ecx`. 

But careful: does `in %dx, %al` clobber al? We're not declaring al as clobbered. We write into %al via `in` and then read it via movzbl. Since %al is written by the template but not declared as an output or clobber, LLVM might assume %rax's low byte is preserved. That could be a correctness issue in theory (compiler might keep a value in rax across the asm). Better to mark `bozulan("~{al}")`? Hmm, but if al is clobbered, and we read it before... no wait, we read it within the asm block itself (movzbl within the template). After the template, al is dead. But LLVM doesn't know the template clobbered al — it assumes only listed clobbers/outputs are modified. If LLVM holds a live value in rax before the asm and expects it after, the `in` clobbering al would corrupt it. So we should declare `bozulan("~{ax}")` or `~{al}`. GCC clobber name is "al"? For byte registers, GCC allows "~{al}"? GCC register names for x86: "al" is valid as a register name (regnames include al, ah, etc. in some contexts). In LLVM, clobber `"~{al}"` — LLVM accepts sub-register clobbers? LLVM's inline asm clobber list accepts register names like "ax", "eax", "rax", and I believe also "al" (it maps to the register class). Hmm, LLVM's X86 register names include AL as a named register ("al"). `InlineAsm` clobber with "{al}" — I think LLVM handles it by marking the whole containing register (rax) as clobbered? Actually LLVM's `getRegisterInfo` — for inline asm clobbers, it uses `TargetRegisterInfo::getRegClass` etc. GCC docs say clobbering "al" is accepted. LLVM also accepts "al" I believe. To be safe, use `bozulan("~{ax}")` — clobbering ax implies the full rax. That's definitely safe and standard.

Similarly for `out`: `out %al, %dx` only reads al, doesn't write it. But we wrote into al via `movb $1, %eax` inside the template — that writes eax! We didn't declare it. Hmm: the template does `movb $1, %eax` which modifies %eax without declaring. LLVM assumes only outputs/clobbers are written. This could corrupt a live value in rax. Safer: add `bozulan("~{ax}")` to the out function too. Or avoid writing to registers not declared.

Alternative approach avoiding implicit register writes: use specific-register constraints:

For out: 
```
girdi("{dx}", port)
girdi("{al}", val)
```
with val i8 and port i16. But KEMGU types: tam8 exists (bayt). `değer: tam8` → i8. For port we'd need tam16? There's `dtam16` (signed 16?). Hmm — in kem_heap, `dtam16` is used for yetki fields ("dtam16" = likely "değişken tam16"? Actually `tam8`/`tam32`/`tam64` are signed ints; `dtam8` used in yetki for iptal — `dtam` prefix maybe means "düz" (plain) or... In kdl_yetki_olustur: `kt: dtam16`, `izin: dtam16`, and `*pip: *dtam8`. The C struct has uint16_t and uint8_t. So `dtam16` = unsigned 16-bit? "d" maybe "düz" (unsigned)? tam = sayı... Hmm, `tam64` = integer 64. `dtam8` — maybe "dizi tam8"? No. Given C uint8_t maps to dtam8 and uint16_t maps to dtam16, "d" likely stands for "düz" (unsigned/plain). But we don't need to guess: we can use tam32 with "r" constraints and template moves.

Actually, does the compiler even pass typed operands for `{al}`-style constraints with correct widths? Unknown. The example in the repo only shows "r" and "=r" constraints. Safest: use "r"/"=r" only, do register moves inside template, declare `~{ax}` (and maybe `~{dx}` — well, dx is written by `movl $0, %edx`; also implicit). Let's declare both `~{dx}` and `~{ax}` clobbers. Hmm, but is `bozulan` repeatable? In the example, only one `bozulan("~{cc}")` — probably takes one string. Can we have multiple clobbers in one string like GCC: `"~{cc},~{memory}"`? Or multiple `bozulan(...)` lines? Unknown. GCC's asm clobber list is comma-separated within one string. LLVM's inline asm call has a clobber string like "~{cc},~{memory}". The KEMGU `bozulan("~{cc}")` presumably appends to the clobber list. Multiple `bozulan` lines might be allowed, or comma-separated in one: `bozulan("~{ax},~{dx}")`. 

Hmm. Risky. Alternative: avoid clobbering dx by using it... we must use dx for out/in — the instruction requires dx. Any use of dx must be declared (either as operand constraint or clobber) to be correct.

Options:
1. `bozulan("~{ax},~{dx},~{memory}")` — single call with comma-separated list (GCC/LLVM style). If KEMGU passes the string verbatim as the clobbers attribute, this works.
2. Multiple `bozulan(...)` clauses — if supported.
3. Use `{dx}` and `{al}` as input constraints with properly-typed operands.

Given uncertainty, which is most likely supported? The example shows one `bozulan` per block. The safest bet for a "derlenebilir" (compilable) answer: hmm.

Actually, wait. Let me reconsider: maybe I can make the output/input operands themselves use dx/al... Input constraint `"{dx}"` requires operand type i16 in LLVM. If KEMGU passes a tam32 (i32) operand, LLVM verifier error: "invalid type for inline asm constraint"? Actually LLVM's InlineAsm with physical register constraints checks that the operand type matches the register's type... Let me recall: In LLVM IR, `call void asm sideeffect "out %al, %dx", "{ax},{dx},~{memory}"(i32 %val, i32 %port)` — with "{ax}" constraint and i32 operand, LLVM accepts? Hmm. Physical register constraints in LLVM: when you tie an operand to a specific physical register like "{ax}", LLVM requires the type to be compatible? I recall that for x86, using "{al}" with an i32 operand gives an error at ISel: "inline assembly requires register '{al}' but it is not available" or type mismatch... Actually I believe there is a check: `X86TargetLowering::isInlineAsmCompatibleReg`? Not sure.

From experience with Rust/Clang: `asm!("out %al, %dx" :: "al"(x as u8), "dx"(p as u16))` — you must use u8 for al and u16 for dx, otherwise compile error: "constraint 'al' expects an l-value or value of size 1 byte"? Clang checks: for specific-register constraints, the operand type size must match register size. In LLVM IR generated: `call void asm sideeffect "out %al, %dx", "{al},{dx}"(i8 %x, i16 %p)` — operand types must be i8 and i16 respectively; if i32, LLVM's InlineAsm::Verify / SelectionDAG would fail with "invalid operand type for inline asm constraint '{al}'"? I'm fairly confident there's a type check in `TargetLowering::getConstraintRegister`... Actually the check is in `SelectionDAGBuilder::visitInlineAsm` / `GetRegistersForValue` — it checks the register class's VT against the operand VT; mismatch → error "inline asm constraint ... requires ... type". Yes, there is: "error: unsupported inline asm: constraint type mismatch"? Something like that.

So with KEMGU, does the compiler pass i16 operands? KEMGU has dtam16 (uint16?) and tam16? We saw `dtam16` in the yetki code and `tam32`, `tam8`, `tam64`. Is there a `tam16`? Probably (tam8/tam16/tam32/tam64 family). But whether `girdi("{dx}", port)` with `port: tam16` (i16) works — dtam16 is u16, tam16 would be i16. LLVM register constraint for dx accepts i16? For "{dx}", the register class GR16 — i16 works. For "{al}", i8 works.

But this is all speculative about the KEMGU compiler's lowering of `girdi` — it presumably just passes the operand value with its natural LLVM type. So `girdi("{dx}", port)` with `port: dtam16` would emit `i16` operand with `{dx}` constraint → valid LLVM. Hmm, but I'm not 100% sure KEMGU supports `dtam16` params — it does: `kdl_yetki_olustur(out: *tam8, kt: dtam16, izin: dtam16)` uses dtam16 params. So dtam16 is a valid type in KEMGU (u16). And tam8 (i8) is used.

But then for `in`, output constraint `"{al}"` with "=r"-style output? Output specific-register constraint `çıktı("={al}", &sonuc)` where sonuc: tam8 → i8. That's how C does it: `asm volatile("in %dx, %al" : "=a"(ret) : "d"(port))` — "=a" means al/ax/eax depending on operand size. With u8 ret, "=a" gives al. So `çıktı("={al}", &sonuc)` with tam8 output.

Hmm, but does the KEMGU `çıktı`/`girdi` syntax support register-specific constraints like "={al}"? The task statement says: 'Kısıt dizgileri LLVM inline-asm kısıtlarıdır ("r", "=r", "{ax}" gibi belirli yazmaç, "~{memory}").' — it explicitly says constraint strings are LLVM inline-asm constraints, including "{ax}"-style specific register. So `girdi("{ax}", x)` is supported syntax. 

So option: 
```kemgu
çıplak işlev kem_port_yaz(port: dtam16, deger: tam8) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"out %al, %dx"#
            girdi("{al}", deger)
            girdi("{dx}", port)
            bozulan("~{memory}")
        }
    }
}

çıplak işlev kem_port_oku(port: dtam16) -> tam8 {
    değişken sonuc: tam8 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"in %dx, %al"#
            çıktı("={al}", &sonuc)
            girdi("{dx}", port)
            bozulan("~{memory}")
        }
    }
    ver sonuc;
}
```

Wait — is `çıktı("={al}", ...)` the right form for a specific register with output? In LLVM, output constraint for specific register al is "={al}". The example uses `çıktı("=r", &s)`. So `çıktı("={al}", &sonuc)` follows. The task says constraint strings like "{ax}" are supported — for outputs you'd need "={ax}". Hmm, it says '"r", "=r", "{ax}" gibi belirli yazmaç' — the "{ax}" example is listed, so specific-register constraints are supported in the constraint string grammar. "={al}" is the standard way to write output-specific-register.

But risk: does the KEMGU compiler's asm parser handle "={al}"? If it validates constraints against a whitelist ("r", "=r", "{...}", "={...}", "~{...}")... unknown. The example only shows "=r" outputs.

Alternatively, safest pattern matching the repo example exactly: use "r"/"=r" with template moves, and declare clobbers. The repo example shows `bozulan("~{cc}")` — one clobber. For our out function we clobber ax (via movb ...,%eax? no wait).

Hmm, let me think again about the "r"-constraint approach with template doing moves:

```
şablon: r#"movl $0, %edx
movb $1, %al
out %al, %dx"#
girdi("r", port)     ; $0
girdi("r", deger)    ; $1
bozulan("~{memory}")
```

Problem: we write %edx and %al without declaring → LLVM doesn't know ax/dx clobbered → potential miscompilation. Also — subtle issue: `$0` and `$1` are compiler-allocated registers; if LLVM allocated the output... they're inputs, fine. But if LLVM allocated, say, %rax for $1 (deger), then `movb %al, %al` fine; but our `movl $0, %edx` could conflict if $0 was allocated %edx — that's fine, `movl %edx, %edx` nop. But if $1 allocated %rdx? Then `movl $0, %edx` = `movl %edx, %edx` then `movb %edx??` no — $1 would be expanded as `movb %dl, %al`? Wait template is `movb $1, %al` → `movb %dl, %al`. Then `movl $0, %edx` already happened before. Order: movl $0, %edx; movb $1, %al; out. If $0=%eax and $1=%edx: `movl %eax, %edx; movb %dl, %al; out %al, %dx`. Wait that's wrong! If $1 = %edx = deger, then after `movl %eax, %edx`, %edx contains port, destroying deger! Then `movb %dl, %al` moves the port's low byte, not deger! Miscompile!

That's the classic problem with using fixed registers in template while operands are "r"-allocated: the compiler may allocate the fixed registers to operands. GCC solves this by declaring fixed registers as clobbered, which prevents allocation of operands to those regs. So with `bozulan("~{ax},~{dx}")` (and the operands being only inputs), LLVM would not allocate $0/$1 into ax/dx, making the template correct.

So the "r"-approach REQUIRES proper clobbers: "~{ax},~{dx}" at least (also "~{memory}" for MMIO-ish ordering? For port I/O, memory clobber not strictly needed but good practice; the arm64 dsb example uses ~{memory}).

So the question becomes: how to express multiple clobbers in KEMGU? Options:
- `bozulan("~{ax},~{dx},~{memory}")` — single string with commas (LLVM clobber string format is comma-separated in the constraint string attribute; e.g., in LLVM IR: `sideeffect ... "~{ax},~{dx},~{memory}"`).
- Multiple `bozulan(...)` clauses.

If the KEMGU compiler concatenates bozulan strings into the LLVM constraint list, either could work. If it just takes one bozulan per asm block (overwriting?), the comma-separated single string is more likely to work IF the parser accepts arbitrary strings. The example `bozulan("~{cc}")` suggests the value is a raw string passed to LLVM. A comma-separated list in one string is exactly how LLVM represents multiple clobbers in the constraints string. If KEMGU builds the constraint string by appending each clause's constraint + commas, then `bozulan("~{ax},~{dx}")` might produce something weird like duplicate commas: e.g., final constraints "=r,r,~{ax},~{dx},~{memory}"? If it appends `~{ax},~{dx}` as one element, we might get "..." hmm.

Let me think about how the compiler likely works: it parses the satıriçi_asm block, collecting çıktı constraints, girdi constraints, bozulan clobbers, then builds LLVM's InlineAsm constraint string like "output1,output2,~clob1,~clob2". If bozulan's argument is inserted as-is (minus nothing), then `bozulan("~{ax},~{dx},~{memory}")` would insert "~{ax},~{dx},~{memory}" as a single "clobber" segment — in LLVM's constraint string parsing, the whole string is split on commas anyway, so it'd parse fine as three clobbers! LLVM's constraint string is just a comma-separated string; InlineAsm::ParseConstraints splits on ','. So passing "~{ax},~{dx},~{memory}" as one bozulan argument would work if the compiler doesn't validate the format strictly.

But if the compiler validates the bozulan argument format (must match ~{reg}), comma-separated might fail validation. Ugh.

Alternatively use multiple bozulan clauses:
```
bozulan("~{ax}")
bozulan("~{dx}")
bozulan("~{memory}")
```
If the parser allows repeated bozulan keys (many CFG parsers do), this works. If it allows only one (last wins), we'd lose clobbers → miscompile risk (silent).

Hmm. This is a "write code that compiles" task judged presumably by consistency with the pattern. Since we can't test, let's minimize risk by using the specific-register constraint approach, which avoids clobbers entirely except ~{memory}:

```kemgu
çıplak işlev kem_port_yaz(port: dtam16, deger: tam8) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"out %al, %dx"#
            girdi("{al}", deger)
            girdi("{dx}", port)
            bozulan("~{memory}")
        }
    }
}
```

Here ax and dx are fully declared as operand registers — no implicit clobbers. The only clobber needed is ~{memory} (optional but nice for ordering). This matches the LLVM constraint semantics described in the task ("{ax}" gibi belirli yazmaç). 

For `in`:
```kemgu
çıplak işlev kem_port_oku(port: dtam16) -> tam8 {
    değişken sonuc: tam8 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"in %dx, %al"#
            çıktı("={al}", &sonuc)
            girdi("{dx}", port)
            bozulan("~{memory}")
        }
    }
    ver sonuc;
}
```

Hmm wait — output constraint "={al}" — is that valid? In LLVM, output constraints for specific registers: "={al}". Yes, that's standard (e.g., clang generates `asm ("in %%dx, %%al" : "=a"(ret) : "d"(port))` where "=a" means the A register with size matching ret; in LLVM IR: `call i8 asm "in %dx, %al", "={al},{dx}"(i16 %port)` — hmm, actually clang's "=a" with u8 output → "={al}"? Let me recall: LLVM's inline asm constraint "={al}"? In LLVM IR you see things like `asm "foo" : "={al}"` hmm... Actually LLVM canonical specific-register constraint syntax in IR is like "={ax}" or "={al}" — e.g., `call { i32, i32 } asm "", "={ax},={dx}"()`. Yes, LLVM IR uses "={ax}" etc. For 8-bit: "={al}". I've seen `asm sideeffect "", "={al}"()` in LLVM test files? There are tests like `llvm/test/CodeGen/X86/inline-asm-constraints.ll`? I believe both are accepted; the register must be a valid name in the target's register info. X86 has AL as a named register. Clang emits "={al}" for "=a" with char. Yes — e.g., `asm ("..." : "=a"((char)c))` produces constraint "={al}". I'm fairly confident.

Alternatively, avoid the specific-register output by reading into a general reg via template move + output "=r"... but then the al clobber problem again (in writes al). We'd need `bozulan("~{al}")` — single clobber! That's cleaner:

```kemgu
çıplak işlev kem_port_oku(port: dtam16) -> tam8 {
    değişken sonuc: tam8 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"mov $1, %dx
in %dx, %al
mov %al, $0"#
            çıktı("=r", &sonuc)
            girdi("r", port)
            bozulan("~{al}")
        }
    }
    ver sonuc;
}
```

Hmm wait: operand order in template numbering: çıktılar → girdiler. So $0 = sonuc (out), $1 = port (in). Template: `mov $1, %dx` — $1 is a 16-bit reg (dtam16 → i16); `mov %si, %dx` OK (16-bit move). `in %dx, %al` reads port into al. `mov %al, $0` — $0 is 8-bit (tam8 → i8) → e.g. `mov %al, %r8b`. OK. Clobber ~{al} needed because we write al. But we also write %dx via `mov $1, %dx` — dx clobbered implicitly! Need ~{dx} too. Unless port operand uses {dx} constraint. So we're back to needing two clobbers or specific constraints.

OK so cleanest: use specific-register constraints for both, requiring only one clobber (~{memory}) or none.

But hold on — does `in`/`out` need `~{memory}`? Not strictly (no memory touched), but for I/O ordering with volatile — inline asm needs to be marked volatile; KEMGU presumably emits volatile side-effect asm always (the arm64 `dsb sy` example relies on not being eliminated — it has bozulan("~{memory}") but no outputs; asm with no outputs is implicitly volatile in LLVM? An asm with no outputs and sideeffect... In LLVM IR, InlineAsm needs either `sideeffect` keyword or outputs to not be deleted. Clang marks asm volatile. The KEMGU compiler presumably emits `sideeffect` or the KEMGU `satıriçi_asm` is always side-effect. Given `vblk_dsb` (dsb sy, no outputs) works, the compiler must emit sideeffect/volatile. Good.

For our functions: kem_port_oku has an output; is it volatile? If not marked volatile, LLVM could theoretically delete it if result unused — but result is used (returned). And kem_port_yaz has no outputs → sideeffect presumably → not deleted. OK.

Now, which approach to present? The task says: "x86_64 için port-mapped I/O (out/in) kullanan bir çıplak UART yazma fonksiyonu" — a bare UART write function. Singular "fonksiyon" but "işlev(ler)" in the response instructions ("yalnızca senin yazdığın işlev(ler)") — so multiple functions allowed. Following kem_heap.kem pattern (helpers + top-level), I'll write:

- `kem_port_yaz(port, deger)` — out primitive
- `kem_port_oku(port)` — in primitive  
- `kem_uart_bayt_gonder(b)` — poll LSR bit 5 (THR empty) then out to COM1 data port
- `kem_uart_yaz(m: metin)` — string writer (like kem_panik pattern), maybe with \n at end? kem_panik adds newline. I'll make a plain writer without newline, or with. Let me follow kem_panik style but as a plain "yaz" (write) — write bytes then maybe newline optional. I'll add a newline variant? Keep it simple: `kem_uart_yaz(m)` writes the message; maybe also writes trailing newline like kem_panik? I'll write without the halt (it's a write function, not panic). Could optionally include newline. Let me not add newline to keep it a pure write; or add a separate `kem_uart_satir(m)`? Keep minimal: byte-level + string-level.

COM1 constants: 0x3F8 = 1016 decimal. Data port = 1016; LSR = 1016 + 5 = 1021. LSR bit 5 (0x20 = 32) = THR empty.

Also line status polling loop: `iken (kem_port_oku(1021 olarak dtam16) olarak tam32) & 32 == 0 { }` — careful with operator precedence in KEMGU. kem_heap uses simple expressions. Bitwise ops: does KEMGU have `&` bitwise? Not shown in kem_heap.kem. Hmm! kem_heap.kem uses only arithmetic (+, -, *, /), comparisons, casts. No bitwise ops shown. The spec mentions "Hizalama ÇALIŞMA-ZAMANI adresine göre (((x+15)/16)*16), bitwise gerekmez" — implying bitwise ops exist in the language but the file avoids them. So `&` likely exists but unverified in this file. Risky to use `&`?

Alternative: avoid bitwise AND by testing specific LSR bit via arithmetic? Bit 5 set test without AND: hmm. Could use division/modulo: `(lsr / 32) % 2 == 1`. That's awkward but avoids bitwise. Or do the bit test inside asm!

Better: do the poll inside a single inline asm block! 

```kemgu
çıplak işlev kem_uart_bayt_gonder(b: tam8) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"mov $0, %edx
1:
in %dx, %al
test $32, %al
jz 1b
mov $1, %edx
mov $2, %al
out %al, %dx"#
            ...
        }
    }
}
```

Hmm, that's getting complicated with local labels (`1:`/`1b` in GAS). Also clobbers needed (al, dx). Getting messy.

Alternative simpler approach avoiding LSR polling entirely: many minimal x86 UART writers just poll; but a truly minimal one can just out directly (QEMU accepts anyway). But correctness-wise, polling is proper. The task says "port-mapped I/O (out/in) kullanan" — using both out and in. So including `in` for LSR polling is desirable.

Let me reconsider: use specific-register constraints; do polling in KEMGU loop with... we need bit test. Options:

a) Use `&` bitwise AND if supported (likely — "bitwise gerekmez" implies bitwise exists).
b) Arithmetic: `(lsr % 64) >= 32` tests bit 5! Because bit5 = value 32 in [0..63] range mod 64. `(lsr % 64) >= 32` ⇔ bit 5 set (for non-negative lsr). Modulo: KEMGU `%` exists? kem_heap mentions "`%` BİLEREK YOK: değişken bölenli `%` sıfıra-bölme kontrolü (D-502) → @kdl_panik ister" — so `%` EXISTS but with constant divisor it's fine? The comment says variable-divisor % requires zero-div check calling kdl_panik; constant divisor presumably fine (or also calls?). "değişken bölenli" = with variable divisor. With constant divisor 64, no panic path needed. Hmm, but does the compiler distinguish? The comment says variable divisor triggers the check; constant presumably lowers to plain urem/srem. Risky but plausible.

Hmm, actually let me reconsider using `&`. The doc quote from kem_heap: "Hizalama ÇALIŞMA-ZAMANI adresine göre (((x+15)/16)*16), bitwise gerekmez" — translation: "alignment via runtime address (((x+15)/16)*16), bitwise not needed". This strongly implies the language HAS bitwise ops but the author chose arithmetic (maybe because bitwise wasn't implemented in the bare tier at that time, or just preference). Given ambiguity, I could avoid needing bit ops entirely.

c) Do the entire poll+send in one asm block with local labels — self-contained, no KEMGU bit ops needed:

```kemgu
çıplak işlev kem_uart_bayt_gonder(bayt: tam8) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"mov $1021, %dx
1:
in %dx, %al
test $32, %al
jz 1b
mov $0, %al
mov $1016, %dx
out %al, %dx"#
            girdi("{al}", bayt)
            bozulan("~{dx}")
        }
    }
}
```

Wait: `mov $0, %al` — $0 is operand 0 (the al-constrained input bayt, i8) → `mov %al, %al`? Hmm, $0 in AT&T template with LLVM: `mov $0, %al` would render as `mov %al, %al` (operand 0 is already al). That's a no-op, fine. Actually careful: `test $32, %al` — $32 here: is `$3` interpreted as operand 3?! There's only 1 operand (0). `$32` — LLVM parses `$` followed by digits as operand number! "$32" would be parsed as operand 32 → error "invalid operand number"! That's why the example uses `$$100` for literal 100 — immediate literals need `$$`. So `test $$32, %al`. And `mov $1016, %dx` → must be `mov $$1021, %dx`. And local label `1:` — in GAS, numeric local labels `1:` and `jz 1b`. Does LLVM's asm parser handle numeric local labels inside inline asm? Inline asm goes through the integrated assembler (or external GAS); numeric local labels are a GAS feature and work in inline asm (commonly used, e.g., `1: ... jmp 1b` in kernel inline asm). LLVM's inline asm does support numeric local labels — there's a known caveat: LLVM rewrites `$` for operands but numeric labels like `1:` are fine. Actually, there IS a known issue: in LLVM inline asm, numeric local labels work with the integrated assembler (they're used in Linux kernel code compiled with clang). Yes, clang compiles Linux kernel asm with `1:` labels fine.

Hmm, but there's another subtlety: `jz 1b` — the `1b` — fine.

But wait, there's a subtlety with `$` parsing: "mov $$1021, %dx" → literal $1021 immediate. Good. "test $$32, %al". Good.

But also we clobber dx (mov $$1021, %dx) — declared via bozulan("~{dx}"). And al: al is an input operand ({al} constraint) — we also WRITE al via `in %dx, %al` and `mov $0, %al`... wait `mov $0, %al` where $0 IS the al operand — that's `mov %al, %al`, no write issue beyond itself. But `in %dx, %al` writes al — an input-only operand register being clobbered! LLVM would consider al still holding bayt's value after? Input operands are assumed clobberable after the asm? NO — inputs are assumed preserved? Actually, in GCC/LLVM inline asm semantics: input operands' registers are NOT guaranteed preserved unless listed as outputs or tied; the compiler doesn't assume input regs survive... Hmm, actually GCC docs: "Note that the compiler may reuse input registers as scratch after the asm" — from the compiler's perspective, input operand registers may be clobbered by the asm? No wait — the compiler assumes the asm only modifies outputs and listed clobbers. Input-only operands: the value in that register before the asm is not needed after (the SSA value lives elsewhere or is dead). Actually the input register holds a copy of the value; the compiler won't assume the register still holds it afterward — hmm, it might if it wants to reuse the value? GCC documentation states you may not write to input-only operands (it's undefined behavior if you do) because GCC may assume inputs are unmodified (e.g., if the same value is used in another operand or later, GCC could rely on the register). It's a real hazard.

So clobbering al (input) via `in` is UB-ish. To be safe: don't reuse al after `in`; move bayt somewhere else, or restructure:

```
mov $0, %ah      ; ??? 
```
Hmm. Alternative: copy bayt into ah first? But ah is also part of ax...

Better approach: two separate asm blocks — one poll (in) and one send (out) — in KEMGU code:

```kemgu
çıplak işlev kem_port_oku(port: dtam16) -> tam8 {
    değişken sonuc: tam8 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"in %dx, %al"#
            çıktı("={al}", &sonuc)
            girdi("{dx}", port)
        }
    }
    ver sonuc;
}

çıplak işlev kem_port_yaz(port: dtam16, deger: tam8) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"out %al, %dx"#
            girdi("{al}", deger)
            girdi("{dx}", port)
            bozulan("~{memory}")
        }
    }
}
```

Then bit test in KEMGU... back to needing bit ops. Hmm.

OR: make the poll loop entirely in asm with output telling "ready":

Actually simplest robust: write the poll in asm with local labels, keeping bayt input untouched by using it only at the end. Sequence:

```
mov $$1021, %dx
1:
in %dx, %al        ; clobbers al — but al is our input operand! 
```

Problem again. Unless bayt input uses a different register, e.g. constraint "r" (general reg) and we move it to al at the end:

```
mov $$1021, %dx
1:
in %dx, %al
test $$32, %al
jz 1b
movb $0, %al      ; $0 = bayt (any general 8-bit reg) → movb %dl?, %al — wait $0 is 8-bit reg if bayt is tam8
mov $$1016, %dx
out %al, %dx
```

Clobbers: dx (written), al (written). Inputs: $0 = bayt (r, 8-bit — hmm, "r" with i8 gives an 8-bit register like %bl; `movb %bl, %al` fine). But wait — could LLVM allocate $0 to... any 8-bit register, including al or dl! If $0 allocated %al: `movb %al, %al` fine. If %dl: we clobbered dx? We declared ~{dx} clobber → LLVM won't allocate dx for operands. And ~{al}? If we declare ~{ax}... but we WRITE al intentionally to feed out. Declaring ~{ax} prevents LLVM from allocating ax for $0 — good. So:

```
şablon: r#"mov $$1021, %dx
1:
in %dx, %al
test $$32, %al
jz 1b
movb $0, %al
mov $$1016, %dx
out %al, %dx"#
girdi("r", bayt)
bozulan("~{ax}")
bozulan("~{dx}")
bozulan("~{memory}")
```

Needs THREE bozulan clauses or one comma-combined. Hmm.

Alternatively specific-register input for bayt in a register that's not al/dx — e.g., `girdi("{bl}", bayt)`? That's specific but weird. Or use `{sil}`? Ugly.

Alternative: two asm blocks with KEMGU-level poll loop, and do the bit test in asm too? Like an asm "read LSR bit5 into 0/1":

```kemgu
çıplak işlev kem_uart_hazir() -> tam32 {
    değişken sonuc: tam32 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"mov $$1021, %edx
in %dx, %al
and $$32, %eax
shr $$5, %eax"#
            çıktı("=r", &sonuc)
            bozulan("~{ax}")
            bozulan("~{dx}")
        }
    }
    ver sonuc;
}
```

Hmm wait — `and $$32, %eax`: al holds the LSR byte, upper bits of eax unknown (in %dx,%al only writes al). `and $$32, %eax` uses eax — upper garbage in bits 8-31 ANDed with 32 → 0, since 32 = bit5 only. and clears bits not in mask, so result = al&32. Then shr 5 → 0/1. OK. But clobbers ax and dx, needs two clobbers again.

The multi-clobber question keeps arising. Let me decide: is multiple `bozulan` clauses allowed? Looking at the syntax in examples: the satıriçi_asm block has fields: mimari, şablon, then multiple çıktı(...) lines, multiple girdi(...) lines, one bozulan(...) line. Since multiple çıktı and girdi lines are allowed, multiple bozulan lines are plausibly allowed too (a list). A simple parser would collect all clauses of each kind into lists. I think multiple bozulan clauses is reasonably safe.

Also comma-separated single bozulan("~{ax},~{dx}") would work if passed through to LLVM's constraint string (LLVM splits on commas). Actually — if the compiler appends it into the constraint string as one segment "~{ax},~{dx}", the final constraint string would be e.g. "=r,~{ax},~{dx},~{memory}" — wait if it does `constraints += "," + bozulan_arg`, then with bozulan("~{ax},~{dx}") we get ",~{ax},~{dx}" — which parses as two clobbers! Both approaches likely work. 

Hmm, but if the compiler validates each bozulan arg with a regex like ^~\{[a-z0-9]+\}$, comma form fails; if it limits bozulan count to 1, multiple lines fail (maybe parse error "duplicate bozulan"?). I'll go with multiple bozulan lines since çıktı/girdi are already multiple — a keyword-based parser most likely accepts repeated clauses. Actually hmm, wait — maybe safest is to design to need at most ONE clobber beyond memory... Let me think if there's a formulation with only ~{memory}:

Use specific-register constraints for al and dx, and avoid clobbering anything else. The poll loop problem: `in %dx, %al` writes al — if al is an OUTPUT ("={al}"), that's declared. So:

```kemgu
çıplak işlev kem_port_oku(port: dtam16) -> tam8 {
    değişken sonuc: tam8 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"in %dx, %al"#
            çıktı("={al}", &sonuc)
            girdi("{dx}", port)
            bozulan("~{memory}")
        }
    }
    ver sonuc;
}
```

This needs only ~{memory}. And out:

```kemgu
çıplak işlev kem_port_yaz(port: dtam16, deger: tam8) {
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"out %al, %dx"#
            girdi("{al}", deger)
            girdi("{dx}", port)
            bozulan("~{memory}")
        }
    }
}
```

Only ~{memory}. 

Then the bit test. If KEMGU has bitwise `&`... The context says constraint strings are LLVM constraints, spec mentions tekkez/yetki... nothing about bitwise ops. kem_heap deliberately avoids them ("bitwise gerekmez"). Hmm, that phrase is about alignment computation; it says bitwise is not needed — implying either not available or just unnecessary. If not available in the compiler, using `&` fails compilation.

Alternative bit-5 test without AND: arithmetic. LSR bit 5 = 32. Test `(x mod 64) >= 32`. KEMGU `%`: exists (per kanal comment) but with variable-divisor zero-check issue; constant divisor 64 — the comment says "değişken bölenli % sıfıra-bölme kontrolü" — variable divisor needs check; constant divisor like 64 is presumably a constant-folded/remainder op without panic. Hmm, but actually maybe ALL % goes through the checked path? The comment: "`%` BİLEREK YOK: değişken bölenli `%` sıfıra-bölme kontrolü (D-502) → @kdl_panik ister ve kem_os'ta o sembol YOK" — translation: "% deliberately absent: variable-divisor % [triggers] divide-by-zero check (D-502) → requires @kdl_panik and that symbol doesn't exist in kem_os". This suggests that % with variable divisor emits a check calling kdl_panik. With a CONSTANT divisor, presumably no check needed (divisor known nonzero) → safe to use. So `lsr % 64` with constant 64 should be fine. But is `%` even parsed in çıplak functions... The kanal file avoided % entirely. Hmm.

Even simpler: avoid testing bit 5 specifically; test `(lsr / 32) % 2 == 1`? Same % issue.

Or use division and comparison: bit5 of x (x ≥ 0, 8-bit): `(x / 32) % 2`... needs %.

Alternative: `(x * ???)`. Hmm.

What about: `(lsr / 32) - (lsr / 64) * 2`? = bit5 as 0/1. Uses only / and *. Ugly but avoids % and &. Actually `(x/32) mod 2 = (x/32) - 2*(x/64)`. Since x < 256 (i8 unsigned... lsr as tam8 0..255, or tam32). Let me define: b5 = (lsr / 32) - ((lsr / 64) * 2). Check: lsr=0x20=32: 32/32=1, 32/64=0, → 1-0=1. lsr=0x3F=63: 63/32=1, 63/64=0 → 1. lsr=64: 64/32=2, 64/64=1 → 2-2=0. lsr=0x60=96: 96/32=3, 96/64=1 → 3-2=1 (bit5 of 96=0x60: 0110_0000 → bit5=1 ✓, bit6=1 too but we want bit5: 0x60 & 0x20 = 0x20 ✓ yes bit5=1). Works. lsr=255: 255/32=7, 255/64=3 → 7-6=1 ✓ (255&32=32 ✓). OK formula works for bit5.

But this is convoluted. Honestly, using `&` might just work — but evidence suggests the kemgu language in this repo may lack bitwise ops (or they exist; "bitwise gerekmez" is ambiguous).

Hmm, wait — maybe I'm overthinking. Let me reconsider: perhaps do the bit-test inside the inline asm and expose a clean boolean, accepting 2 clobbers via two bozulan lines. Or better: use "={al}" output trick with test:

```kemgu
çıplak işlev kem_uart_hazir() -> tam32 {
    değişken sonuc: tam8 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"mov $$1021, %dx
in %dx, %al
shr $$5, %al
and $$1, %al"#
            çıktı("={al}", &sonuc)
            bozulan("~{dx}")
        }
    }
    ver sonuc olarak tam32;
}
```

Here al is the output (declared), dx is clobbered (one bozulan besides memory — two bozulan lines total). Template: `mov $$1021, %dx; in %dx, %al; shr $$5, %al; and $$1, %al` → al = bit5. Output via {al}. Clobbers: ~{dx} (since dx written) and ~{memory} optional. Two bozulan lines.

Versus the {dx} input-constraint approach with fixed port inside... we can't pass port as {dx} input AND ALSO... wait we CAN:

```kemgu
çıplak işlev kem_lsb5(p: dtam16) -> tam8 {
    değişken sonuc: tam8 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"in %dx, %al
shr $$5, %al
and $$1, %al"#
            çıktı("={al}", &sonuc)
            girdi("{dx}", p)
            bozulan("~{memory}")
        }
    }
    ver sonuc;
}
```

Only ~{memory} clobber! dx is input-declared, al is output-declared. 

But hmm — do I even need this generic bit5 reader? I could just make `kem_uart_hazir()` with the port constant inside... but then dx is written via `mov $$1021, %dx` → needs clobber. Using girdi("{dx}", port) avoids it. So pass the LSR port as a parameter. Fine.

Actually, wait — even simpler: combine poll and send into one asm block where dx input is the data port and... no, poll needs LSR port (1016+5) and send needs data port (1016). Two different ports. Could pass both as inputs: {dx} can only hold one... Use two asm blocks with KEMGU loop:

```kemgu
çıplak işlev kem_uart_bayt_gonder(b: tam8) {
    // LSR bit5 (THR boş) olana dek bekle
    iken kem_uart_hazir() == 0 { }
    kem_port_yaz(1016 olarak dtam16, b);
}
```

where kem_uart_hazir uses the {dx}={1021} input asm returning bit5. Wait, pass 1021: `kem_lsr_bit5(1021 olarak dtam16)`. Hmm, `1016 olarak dtam16` — cast tam64 literal to dtam16. Literals: kem_heap uses decimal literals (150994944). Cast syntax `X olarak T`. `1021 olarak dtam16` should work. Or define the param type and let implicit conversion? Safer explicit casts, matching kem_heap style (`(cur + 16) olarak *tam8` etc.).

Hmm wait, actually — is `dtam16` appropriate, or should I use `tam16`? In kem_heap, `dtam16` used for u16 fields (kaynak_tipi, izin) and `dtam8` for u8 (iptal). The "d" prefix... In Turkish, "tam" = integer (tam sayı). "dtam" might be "düz tam" = unsigned? The C struct has uint16_t → dtam16, uint8_t → dtam8. And `tam32`/`tam64`/`tam8` appear for i32/i64/i8 (malloc n: tam64, c: tam32, cb: tam8 with `c olarak tam8` truncation). In memset: `değişken cb: tam8 = c olarak tam8; // düşük bayt` — comment says "low byte", so tam8 is i8 (signed?) and cast truncates. For port values 0x3F8 (1016) — fits in i16 positive range, so `tam16` (if exists) or dtam16 both fine. But do we know `tam16` exists? Not shown. `dtam16` is shown to exist. Port 1021 < 32768, so even signed 16-bit is fine. But is `dtam16` signed or unsigned? "d" = ? If dtam16 is unsigned (u16 per C mapping), 1021 fits fine either way.

Hmm, one concern: for the `{dx}` constraint, LLVM requires operand type i16 — both i16 and u16 are i16 in LLVM. Fine.

For `{al}` with tam8: i8. Fine (LSR value 0-255 read into i8; as unsigned byte it's fine — comparisons: I compute bit via shr/and in asm so result is 0/1, no sign issues).

Now the string writer:

```kemgu
çıplak işlev kem_uart_yaz(m: metin) {
    değişken n: tam32 = metin_uzunluk(m);
    değişken i: tam32 = 0;
    iken i < n {
        kem_uart_bayt_gonder(metin_bayt(m, i));
        i = i + 1;
    }
}
```

metin_bayt returns tam8 — matches bayt_gonder(b: tam8). metin_uzunluk returns tam32. Matches kem_panik pattern exactly. 

Now — should kem_uart_yaz add a newline? kem_panik writes '\n' after. For a general "yaz" I'll leave it pure; optionally add CR? Serial terminals want \r\n but keep simple. I might include a trailing newline variant... Keep it clean: just write the string. Maybe also write a newline in a separate small function `kem_uart_satir()`? Unnecessary. Keep: port primitives + hazir + bayt_gonder + yaz.

Wait, one more consideration: the task says "port-mapped I/O (out/in) kullanan bir çıplak UART yazma fonksiyonu" — a bare UART write function using PMIO. The essential deliverable is the write path (out), with in for LSR polling. My design covers both.

Now, about `çıplak` and calling rules: E013 — bare fn can only call bare/extern + builtins resolving to bare kdl_metin_*. My functions: kem_port_yaz (çıplak) calls nothing; kem_lsr... calls nothing; kem_uart_bayt_gonder calls kem_uart_hazir (bare ✓) and kem_port_yaz (bare ✓); kem_uart_yaz calls metin_uzunluk/metin_bayt builtins (→ bare kdl_metin_* ✓) and kem_uart_bayt_gonder (bare ✓). All good.

Also — `iken doğru { }` for the poll loop: kem_heap uses `iken doğru { }` (busy-wait) in kem_panik and `iken kem_kanal_ileri(...) == ... { }` — empty-bodied while with condition. So `iken kem_uart_hazir() == 0 { }` matches. Precedence: `==` in while condition used in kanal: `iken *pson == *pbas { }` ✓.

Now let me also double check the inline asm template details:

**out block:**
```
şablon: r#"out %al, %dx"#
girdi("{al}", deger)
girdi("{dx}", port)
bozulan("~{memory}")
```
LLVM: `call void asm sideeffect "out %al, %dx", "{al},{dx},~{memory}"(i8 %deger, i16 %port)`. Constraint "{al}" with i8 operand — valid. "{dx}" with i16 — valid. Template references: registers written literally `%al`, `%dx` — since operands are tied to those exact registers, the template uses them directly. Note: we don't even use $0/$1 in the template — operands are just there to pin registers. Is that OK? In LLVM inline asm, yes — operands with physical-register constraints pin values into those registers, and the template can reference the physical registers literally. GCC does this all the time (e.g., `asm("out %0, %1" : : "a"(val), "d"(port))` uses $0/$1, but `asm("out %al, %dx" : : "a"(val), "d"(port))` also works). Fine.

Hmm, but should I use $0/$1 in template for style? With specific-register constraints, `$0` would expand to `%al` anyway. Writing `out %al, %dx` directly is clearer. But — one concern: does LLVM's asm parser get confused by `%al` in the template? In LLVM inline asm templates syntax, `$` introduces operands; `%` is passed through literally to the assembler. AT&T syntax needs `%` before registers. The example `mov $2, $0` doesn't show register names... but the vblk arm64 example `dsb sy` has no registers either. Hmm — do ANY repo examples show literal `%reg` in templates? Not in the given context. But LLVM inline asm convention: in the template, `%` is literal (AT&T register prefix), `$` for operands, `$$` for literal $. This is standard LLVM behavior (clang emits `mov %eax, %ebx` style inside asm strings all the time — e.g., any `asm("mov %0, %1")`? no wait — clang users write `asm("mov %0, %1" ...)` where %0 is GCC operand syntax... in GCC, `%0` is operand and `%%eax` is register! In LLVM IR level, the template uses `$` for operands. The KEMGU compiler emits LLVM IR inline asm directly (it's an LLVM frontend — "build/kemgu --llvm" outputs .ll). The example `mov $2, $0` with `$$2` confirms LLVM-level syntax ($ operands, $$ immediates). So literal `%edx` in template is fine at LLVM level (passed to assembler).

Hmm wait, one more check on the example: `add $$2, $0` — LLVM: `$0` operand, `$$` escaped dollar → assembler sees `add $2, %eax`. ✓. So my templates: `out %al, %dx` — assembler sees `out %al, %dx` ✓ (AT&T: out source, port — `out %al, %dx` means output AL to port DX ✓).

`in %dx, %al` — AT&T: `in %dx, %al` reads from port DX into AL ✓.

**in block (LSR read with bit extraction):**
```
şablon: r#"in %dx, %al
shr $$5, %al
and $$1, %al"#
çıktı("={al}", &sonuc)
girdi("{dx}", port)
bozulan("~{memory}")
```
LLVM: `call i8 asm sideeffect "in %dx, %al
shr $$5, %al
and $$1, %al", "={al},{dx},~{memory}"(i16 %port)`. 

Wait — careful: `shr $$5, %al` — shifts are fine on al. `and $$1, %al` → al = bit5. Output "= {al}" with i8 ✓.

Hmm, actually — can I avoid the shr/and and just return the raw LSR byte, then do the bit test... in KEMGU (bit ops question again). OR return al masked: `and $$32, %al` → al = 32 or 0; then KEMGU: `iken kem_lsr_oku(1021 olarak dtam16) == 0 { }` — since result is 32 (nonzero) or 0! Even simpler:

```kemgu
çıplak işlev kem_uart_bos_mu(port: dtam16) -> tam8 {
    değişken sonuc: tam8 = 0;
    güvensiz {
        satıriçi_asm {
            mimari: x86_64
            şablon: r#"in %dx, %al
and $$32, %al"#
            çıktı("={al}", &sonuc)
            girdi("{dx}", port)
            bozulan("~{memory}")
        }
    }
    ver sonuc;
}
```

Result 32 (THR empty) or 0 (busy). KEMGU: `iken kem_uart_bos_mu(1021 olarak dtam16) == 0 { }`. tam8 comparison with 0 — fine (32 as i8 is positive 32). 

Hmm, but naming: `kem_uart_gonderime_hazir` (ready-to-transmit). LSR bit 5 (0x20) = Transmitter Holding Register Empty. Turkish: "verici tutma yazmacı boş". Name: `kem_uart_thr_bos` or `kem_uart_gonderim_hazir`. I'll use something descriptive.

Wait, actually, maybe even simpler: combine everything into fewer functions. Let me think about what's cleanest and most pattern-faithful:

kem_heap.kem style: small primitives with clear comments, then composites. I'll deliver:

1. `kem_port_yaz(port: dtam16, deger: tam8)` — out primitive
2. `kem_port_oku(port: dtam16) -> tam8` — in primitive (raw byte; general purpose, mirrors out)
3. `kem_uart_gonderim_hazir()` or with port param — hmm, if I have raw `kem_port_oku`, then bit-test problem returns for the poll. Unless... the poll does `iken (kem_port_oku(LSR) ...)`. Needs bit test.

Option A: poll via dedicated asm that ANDs inside (function 3 = "hazir" test).
Option B: skip LSR polling, just out directly (QEMU-tolerant). But task explicitly mentions in — using `in` for LSR poll is the right call.

Go with Option A: a third asm function that does in+and. But then `kem_port_oku` (raw in) might be unused — that's fine, it's a symmetric primitive... but unused functions might trigger warnings? kem_heap has functions kem_os doesn't use (mentioned: "tam64/ptr/yapi varyantları kem_os KULLANMADIĞI için atlandı" — they SKIP unused ones). Hmm — "kem_os yüzeyi: olustur/ekle
