# =============================================================================
# KEMGU — Windows PowerShell build wrapper
# =============================================================================
#
# Kullanim:
#   .\build.ps1                  # tum hedefi derler (build/kemgu.exe)
#   .\build.ps1 test_tumu        # butun testleri calistirir
#   .\build.ps1 calistir_lexer_test
#   .\build.ps1 clean
#
# MSYS2'yi varsayilan disi bir konuma kurduysaniz:
#   $env:MSYS2_ROOT = "D:\msys64"; .\build.ps1
#
# Wrapper su islemleri yapar:
#   1. MSYS2 UCRT64 + Clang64 yollarini gecici olarak PATH'e ekler
#   2. mingw32-make.exe'yi tum argumanlarla cagirir
#   3. Make'in cikis kodunu PowerShell'e iletir
#
# Neden PATH'i kalici yazmadik: KEMGU disindaki PowerShell oturumlarinda
# MSYS2 PATH'i istenmeyen yan etkilere yol acabilir (gcc gibi yaygin
# komutlar baska bir araca yonlendirilir). Wrapper sadece bu calistirma
# icin gecici set eder.
# =============================================================================

$ErrorActionPreference = 'Stop'

# MSYS2 koku — kullanici ortam degiskeni ile override edebilir
$msys2Root = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { 'C:\msys64' }

$ucrt64Bin  = Join-Path $msys2Root 'ucrt64\bin'
$clang64Bin = Join-Path $msys2Root 'clang64\bin'
$msysBin    = Join-Path $msys2Root 'usr\bin'                 # rm, cat, basename, ... (Bash recipe'lari icin)
$mingwMake  = Join-Path $ucrt64Bin 'mingw32-make.exe'
$bashExe    = Join-Path $msysBin 'bash.exe'

# Asgari saglik kontrolu — MSYS2 ve gerekli paketler kurulu mu?
if (-not (Test-Path $mingwMake)) {
    Write-Error @"
MSYS2 mingw32-make bulunamadi: $mingwMake

Cozum:
  1. MSYS2 kurun: https://www.msys2.org/
  2. MSYS2 shell'de:
       pacman -S mingw-w64-ucrt-x86_64-gcc \
                 mingw-w64-ucrt-x86_64-make \
                 mingw-w64-clang-x86_64-clang \
                 mingw-w64-clang-x86_64-llvm
  3. Varsayilan disi konuma kurduysaniz:
       `$env:MSYS2_ROOT = 'D:\msys64'
"@
    exit 1
}

if (-not (Test-Path $clang64Bin)) {
    Write-Warning "Clang64 bulunamadi: $clang64Bin (ASan testleri calismayabilir)"
}

# PATH'i sadece bu surec icin gecici set et — Clang64 once (ASan testleri
# clang aramasinda Clang64'u bulmali), UCRT64 sonra (gcc + make icin),
# MSYS /usr/bin son (Bash recipe'lardaki rm, cat, basename, ...).
$env:PATH = "$clang64Bin;$ucrt64Bin;$msysBin;" + $env:PATH

# Makefile recipe'lari Bash sozdizimi kullaniyor ($$var, [ -f ... ], for, ...).
# Windows'ta Make default SHELL=cmd.exe — Bash recipe'lar patlar. Forward-slash
# path Make'in MSYS path donusturucusu icin guvenli.
$bashShell = ($bashExe -replace '\\', '/')

# mingw32-make'i Bash SHELL + tum argumanlarla cagir
& $mingwMake "SHELL=$bashShell" @args
exit $LASTEXITCODE
