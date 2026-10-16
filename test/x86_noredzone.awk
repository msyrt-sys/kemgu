# [D-696] x86_64 bare-metal: her `define`a `noredzone` işlev özniteliği ekle.
# NEDEN: SysV x86-64 yaprak işlevlere RSP'nin ALTINDAKİ 128 B'ı ("kırmızı bölge")
# yerel değişken için kullandırır. Çekirdekte ring0 kesmesi (IRQ0, int 0x80,
# istisna) çerçevesini AYNI yığına, tam RSP'nin altına iter → kırmızı bölgedeki
# yereller sessizce bozulur. clang `-mno-red-zone` IR girdisinde ETKİSİZDİR
# (ölçüldü: 8 → 8 RSP-altı başvuru); LLVM politikayı yalnız işlev başına
# `noredzone` özniteliğinden okur. Derleyici (src/llvm.c) bare-metal hedefte bunu
# kendisi yaymalı — o yapılana kadar bu adım yapım hattında ekler.
# Beklenen biçim: `define ... (...) {` (öznitelik grubu YOK). Biçim değişirse
# SESSİZCE atlamak yerine hata ver (çıkış 1).
/^define / {
    n_def++
    if ($0 !~ /\) \{$/ || $0 ~ /noredzone/) {
        print "x86_noredzone.awk: beklenmeyen define bicimi: " $0 > "/dev/stderr"
        hata = 1
    } else {
        sub(/\) \{$/, ") noredzone {")
        n_ek++
    }
}
{ print }
END {
    if (hata || n_def == 0 || n_def != n_ek) {
        print "x86_noredzone.awk: define=" n_def " eklenen=" n_ek > "/dev/stderr"
        exit 1
    }
}
