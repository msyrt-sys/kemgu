#!/usr/bin/env python3
"""Dalga 4: mevcut kapıyı değiştirmeden yeşil / çalışan sabotaj / köprü kanıtı."""
import os
from pathlib import Path
import subprocess
import sys
import time


def run(args, log, env=None):
    with log.open("w", encoding="utf-8") as out:
        p = subprocess.run(args, stdout=out, stderr=subprocess.STDOUT, env=env)
    print(log, "rc=", p.returncode, flush=True)
    if p.returncode:
        print(log.read_text(encoding="utf-8")[-5000:], flush=True)
    return p.returncode


def main():
    name, test, module, before, after = sys.argv[1:6]
    dest = Path("build/codex4")
    dest.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, CHATGPT_ZAMAN_ASIMI="5")
    gate = ["bash", "otomasyon/chatgpt/dogrula.sh", test]
    assert run(gate, dest / (name + "-yesil.log"), env) == 0
    ir = dest / (name + ".ll")
    with ir.open("w") as out:
        subprocess.run(["build/codegen", "--llvm", test], stdout=out, check=True)
    exe = dest / (name + ".self")
    subprocess.run(["clang", "-Wno-override-module", "-x", "ir", str(ir), "-x", "none", "build/kdl_runtime.o", "build/kdl_runtime_mmio.o", "-o", str(exe)], check=True)
    start = time.monotonic()
    subprocess.run([str(exe)], check=True, timeout=5, stdout=subprocess.DEVNULL)
    seconds = time.monotonic() - start
    (dest / (name + "-sure.txt")).write_text(f"self saniye={seconds:.6f}\n")
    print("self saniye=", seconds, flush=True)
    path = Path(module)
    original = path.read_bytes()
    text = original.decode("utf-8")
    assert text.count(before) == 1, "Sabotaj hedefi tek olmalı"
    try:
        path.write_text(text.replace(before, after), encoding="utf-8")
        log = dest / (name + "-sabotaj.log")
        assert run(gate, log, env) != 0, "Sabotaj yakalanmadı"
        assert "HATA[C çalıştır]" in log.read_text() or "HATA[self çalıştır]" in log.read_text(), "Sabotaj derlenmelidir"
    finally:
        path.write_bytes(original)
    assert run(gate, dest / (name + "-geri.log"), env) == 0
    assert run(["make", "calistir_chatgpt_kopru"], dest / (name + "-kopru.log")) == 0
    print(name, "TAMAM", flush=True)


if __name__ == "__main__":
    main()
