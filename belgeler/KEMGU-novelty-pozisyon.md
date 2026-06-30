# KEMGU Novelty-Pozisyon Analizi — Yayın-Öncesi DÜRÜST Literatür Değerlendirmesi

> **Amaç:** KEMGU'nun bellek-güvenliği modelinin (region-confinement) mevcut işe karşı *savunulabilir* bir yenilik delta'sı var mı — acımasız-hakem gözüyle.
> **Statü:** KARAR-GİRDİSİ (Mehmet + orchestrator yargılar; bu nihai verdict değil).
> **Kural:** oversell YASAK · charitable-yorum YASAK · delta inceyse "ince" denir.
> **Yöntem:** ADIM 1 repo'dan okundu (tahmin değil); ADIM 2 prior-art web-araştırması (5 sistem, paralel, sitasyonlu).

---

## ADIM 1 — KEMGU'nun bellek modeli (repo'dan, tam karakterizasyon)

Kaynaklar: `src/escape.c` (`ky_confined`, `ky_dizi_builtin_confined`, `escape_kesin_yerel`), `src/llvm.c` (`bolge_yerel_yonlendir`), `selfhost/codegen.kem` (`ky_confined` + `ky_iter_kesin` + ρ_iter), `belgeler/KEMGU_Bellek_Modeli.md`, `belgeler/F4.2b_Direktif.md`, `proofs/drf-v2-lean/`.

**Çekirdek model:**

1. **Region-tabanlı bellek.** Bölgeler: `ρ_yerel(f)`, `ρ_çağıran(f)`, `ρ_iterasyon(d)`, `ρ_global`, `ρ_sahip(thread)`, `ρ_kanal`. Ömür sırası `ρ_iter ≤ ρ_yerel ≤ ρ_çağıran ≤ ρ_global`. Bileşik değerler (dizi/yapı) bölgede (heap); skalerler stack.

2. **Otomatik, annotation-SUZ region çıkarımı.** Programcı HİÇBİR bölge/lifetime annotation'ı yazmaz (Rust lifetime'ları / Cyclone region annotation'larının aksine). Derleyici sözdizimsel kurallarla atar: kaçmayan → `ρ_yerel`; `ver`'lenen → `ρ_çağıran`; yapıya gömülen → hedef bölge; döngü-gövde-kaçmayan → `ρ_iterasyon`; koşullu → dalların LCA'sı.

3. **STATİK region-serbest.** Serbest noktaları derleme-zamanı emit edilir (runtime bölge/refcount izleme YOK). `ρ_yerel` fonksiyon dönüşünde; `ρ_iter` HER döngü-iterasyon gövde-çıkışında serbest. (Diziler runtime *sınır* kontrolü taşır — ama bu bölge-serbestlemeden bağımsız.)

4. **POZİTİF default-deny confinement ispatı** (F4.2b/F4.3 spesifik mekanizma — KEMGU'nun en ayırt edici tasarım kararı). Bir diziyi yerel bölgede serbest etme kararı, *may-escape* veri-akış analizine GÜVENMEZ (may-analiz kaçış yollarını kaçırabilir → sessiz UAF). Bunun yerine: dizi yalnızca POZİTİF bir ispat değişkenin HER kullanımının "confined" olduğunu gösterirse serbest edilir — yerinde indeks-oku `v[i]`, yerinde indeks-yaz `v[i]=x`, veya retain-etmeyen builtin'in ilk argümanı (`dizi_al/boyut/yaz/ekle/kapasite`). BAŞKA her kullanım (`ver v`, alias `b=v`, yapı/dizi'ye gömme, `&v`, non-first-arg, lambda-yakalama, reassign) → DENY → dizi ρ_çağıran'da kalır (erken serbest edilmez). Şüphe → konservatif. Tasarım may-analizi *açıkça reddeder* ve serbest için pozitif, inşa-gereği-sound bir ispat ister.

5. **Per-iterasyon serbest (F4.3).** Döngü-gövdesinde bildirilen confined dizi HER İTERASYONDA serbest (sonsuz kernel-loop sınırlı bellek). Kriter: confined-ispat ∧ bildirim-döngü-gövdesinde (her iterasyon taze binding → loop-carry yok).

6. **GC YOK, borrow-checker YOK, reference-counting YOK.**

7. **Mekanize soundness.** Data-Race-Freedom (DRF) teoremi Lean 4'te KISMEN mekanize (bölge-korunumu, bölge-thread-tekilliği, linear-move-cross-thread lemmaları); bellek/bölge soundness'i kâğıt üzerinde, DRF mekanizasyonu devam ediyor.

8. **Self-hosting.** Derleyici KEMGU'da yazılı + kendini byte-identik fixpoint'e derliyor — region-confinement KENDİ koduna uygulanmış (kendi döngü-gövde-dizilerini per-iterasyon serbest ederek doğru self-derliyor).

9. **OS hedefi** (devam ediyor); per-iterasyon serbest doğrudan OS kernel-loop bounded-memory gereksiniminden motive.

10. **Diğer katmanlar (odak değil):** linear tipler (`tekkez`), capability tipleri (`yetki`), realtime/WCET (`sabitsüre`), region-ownership concurrency (`görev`/`kanal`).

**Karakterizasyon özeti (tek cümle):** KEMGU = otomatik + annotation-suz + statik *region inference* (Tofte-Talpin soyu) + pozitif-default-deny confinement-escape analizi ile region-free; üstüne kısmi-mekanize DRF + self-hosting + OS hedefi.

---

## ADIM 2 — Prior art (5 sistem, sitasyonlu, model çıkarımı)

Her sistem için bellek modeli web-araştırması + birincil-kaynak okumasıyla çıkarıldı. KEMGU-yanlısı yorum yapılmadı; her sistemin KENDİ tasarım-uzayı noktası tarif edildi.

### P1. Tofte-Talpin region inference / ML Kit (1994–2004) — **çekirdek prior art**
- **Model:** Otomatik, **annotation-SUZ**, statik region çıkarımı (`letregion ρ in e`). Bölge ömürleri yığın-disiplinli (LIFO/blok-scoped), put/get effect'leriyle çıkarılır. Soundness: iyi-tipli program → dangling pointer YOK (Tofte-Talpin, Inf.&Comp. 1997). **Kâğıt ispatı** (mekanize değil).
- **Per-iterasyon ilgili kısım:** ML Kit'in **storage-mode analizi** (`attop`/`atbot`), **multiplicity inference** (finite/infinite region), **region reset** (`resetRegions`/`forceResetting`) — kuyruk-özyinelemeli döngülere **otomatik per-iterasyon sınırlı bellek** verir.
- **Bilinen zayıflık:** Uzay-sızıntısı kırılganlığı; "programcı çıkarım algoritmasını anlamak zorunda" (HOSC 2004 retrospektifi); küçük kod değişiklikleri ömürleri sert biçimde değiştirir.
- **Sitasyon:** Tofte&Talpin POPL'94 + Inf.&Comp. 132(2) 1997; Tofte&Birkedal TOPLAS 20(4) 1998 (çıkarım algoritması); ML Kit manuel DIKU TR 97/12; retrospektif HOSC 17 2004; Aiken-Fähndrich-Levien PLDI'95 (scope-sonundan önce serbest).

### P2. Cyclone (PLDI 2002) / ATS — region-tabanlı güvenli-sistem dilleri
- **Cyclone:** Region-tabanlı güvenli-C. **EXPLICIT annotation** (region-polimorfik `'r`, `*'r` pointer tipleri). TT bütün-program çıkarımını **bilinçle REDDEDER** → ayrı-derleme korumak için lokal çıkarım + default'lar (port edilen kodun ~%6'sı region annotation). Dangling-pointer güvenliği **type-and-effects + capability** ile (may-escape veri-akışı YOK). Dinamik region'lar **leksik blok**ta serbest — per-iterasyon DEĞİL. Kâğıt soundness ispatı. Self-hosting DEĞİL. (Linux device-driver portları → OS-bitişik.)
- **ATS:** Maksimal-annotated (linear viewtype + programcı-yazımı **ispat terimleri**), inşa-gereği-sound. **Self-hosting** (Anairiats ~100K LOC ATS'te). **OS kernel'i VAR** (xlq/aos). → delta (c)'nin en güçlü karşı-örneği.
- **Sitasyon:** Grossman et al. PLDI 2002; Cyclone region soundness TR2001-1856; ATS/Anairiats + xlq/aos.

### P3. Verona (OOPSLA 2023) — reference capabilities
- **Model:** Programcı-yazımı reference capability'ler (`iso`/`mut`/`cown`). Region'lar **manuel, programcı-bölümlenmiş**; her region için **takılabilir strateji** (tracing-GC / refcount / bump-bulk-free). Sahip-`iso` düştüğünde **TÜM region** toptan serbest. Region çıkarımı YOK, may-escape analizi YOK — güvenlik **inşa-gereği** capability tip-disiplininden. C++ implementasyon (self-hosting değil). Kâğıt soundness. OS değil.
- **Sitasyon:** Arvidsson et al. OOPSLA 2023, DOI 10.1145/3622846; arXiv 2309.02983; microsoft/verona docs.

### P4. Vale — single-ownership + generational references
- **Model:** Serbest mekanizması = **tek-sahiplik** (per-owner, C++ RAII/unique_ptr soyu) — statik, otomatik-sahiplikten, per-scope. Alias güvenliği = **RUNTIME generational references** (~%2–11 ek-yük; dereference'te generation-eşleşme assert'i; UAF runtime'da yakalanır). Region'lar = **semantik-koruyan immutability optimizasyonu** ("güvenle yok sayılabilir"), serbestlemeyi KONTROL ETMEZ. Region çıkarımı YOK, mekanize ispat YOK, serbest-kararı statik DEĞİL (sahiplik-scope) / alias-güvenliği RUNTIME.
- **Sitasyon:** verdagon.dev (zero-cost-borrowing-regions, generational-references); vale.dev/memory-safe.

### P5. Reachability types (Rompf grubu, OOPSLA 2021/24/25) + DRF à la Mode (POPL 2025) — **frontier**
- **Model:** Statik reachability qualifier'ları (tip-seviyesi erişilebilir-değişken kümeleri). Qualifier **çıkarımı** (Escape with Your Self, OOPSLA 2025) — ama fonksiyon imzaları hâlâ annotation taşır (**tam annotation-suz DEĞİL**). "**Free to Move**" (2025): **explicit `free t`** + flow-duyarlı **kill-effect** + no-use-after-kill, **TAM mekanize** (Rocq). Arena makalesi: **leksik-scope** serbest. **Per-iterasyon döngü-gövde serbesti HİÇBİR yerde YOK.** DRF à la Mode (POPL 2025): mod-tabanlı, **Iris-mekanize**. Self-hosting DEĞİL, OS yok.
- **Sitasyon:** Bao et al. OOPSLA 2021; λ◇ arXiv 2307.13844; Escape-with-Your-Self OOPSLA 2025 arXiv 2404.08217; Free-to-Move arXiv 2510.08939; When-Lifetimes-Liberate arXiv 2509.04253; DRF-à-la-Mode POPL 2025; github.com/TiarkRompf/reachability (~%97 Rocq).

### Karşılaştırma tablosu

| Eksen | **KEMGU** | Tofte-Talpin/ML Kit | Cyclone | ATS | Verona | Vale | Reachability/DRF-Mode |
|---|---|---|---|---|---|---|---|
| Serbest disiplini | region (per-fn + **per-iter**) | region (letregion, blok/LIFO; ML Kit reset) | region (leksik blok) | linear/owner (ispat-terimi) | region (iso-drop toptan) | owner (per-scope) | explicit-free / arena-leksik |
| Çıkarım | **otomatik, annotation-suz** | **otomatik, annotation-suz** | lokal + **annotation** | tam **annotation** | tam **annotation** (capability) | owner-örtük; region-annot (çıkarsanır) | qualifier-çıkarım + imza-annot |
| Per-iterasyon serbest | **EVET (statik, imperatif loop)** | EVET (ML Kit reset, kuyruk-özyineleme, kırılgan) | hayır (blok) | hayır | hayır | hayır (owner) | **hayır** |
| Serbest-kararı güvenliği | statik **pozitif-confinement** (runtime-check YOK) | statik effect (konservatif) | type-effect+capability | linear ispat | capability | **RUNTIME gen-check** | type-effect (kill-effect) |
| may-escape veri-akışına güven | **HAYIR (açıkça reddeder)** | — (effect-tabanlı) | — (type-tabanlı) | — | — | — | — (type-tabanlı) |
| Soundness mekanizasyonu | **kısmî** (Lean DRF; region kâğıt) | kâğıt | kâğıt | inşa-gereği (dependent/linear) | kâğıt | yok | **tam** (Rocq/Iris) |
| Self-hosting | **EVET** (fixpoint) | (SML-in-SML, trivial) | hayır | **EVET** | hayır | ~native | hayır |
| OS hedefi | **EVET** (devam) | hayır | driver portları | **EVET** (kernel) | hayır | hayır | hayır |
| GC | yok | yok | yok (heap-region hariç) | yok | takılabilir | yok | yok |

---

## ADIM 3 — Acımasız delta analizi (her aday: Yeni / Kısmen / Değil + neden)

### Delta (a) — otomatik + statik + annotation-suz region inference (per-iterasyon serbest dâhil)
**Verdict: DEĞİL (çekirdek) / en-fazla İNCE-mikro-delta (per-iter-imperatif sliver).**

- Otomatik + statik + annotation-suz region çıkarımı = **Tofte-Talpin'in 1994–98 ders-kitabı katkısı.** KEMGU'nun sözdizimsel kuralları (kaç→çağıran, koşul→LCA, store→hedef-bölge) region-polimorfizmi + effect-unification'ın birinci-derece yeniden-ifadesi.
- Per-iterasyon serbest bile **ML Kit storage-mode/region-reset** (atbot + multiplicity + resetRegions) tarafından kuyruk-özyinelemeli döngüler için **otomatik** kapsanıyor.
- **Tek gerçek sliver:** annotation-suz per-iterasyon serbestin **imperatif, C-benzeri döngüde** olması — TT/Cyclone'un letregion'u LIFO/blok-scoped ve döngülerde **bilinen biçimde SIZAR** (Cyclone: "LIFO arena'lar server/event-loop'a uygun değil"); ML Kit'in reset'i fonksiyonel kuyruk-özyinelemeye bağlı ve **kırılgan**. Yani "imperatif kernel-loop'ta her iterasyon taze-binding dizisini derleme-zamanı serbest" mikro-boşluğu **dolu değil** — ama bu, TT'nin yapamadığı bir şey değil, idiom/sunum farkı.
- **Ek bedel (dürüstçe):** tam-otomatik çıkarım bütün-program/modül-kapalı analiz → KEMGU muhtemelen Cyclone'un koruduğu **ayrı-derleme** özelliğini ödüyor. Yani "annotation-suz" Cyclone'a karşı *bedelli* bir delta.
- **Mehmet ASLA dememeli:** "annotation-suz region inference'ı biz icat ettik" / "döngü belleğini per-iterasyon ilk biz serbest ettik." Hakem TT'yi prior-art gösterir, reddeder.

### Delta (b) — pozitif default-deny confinement ispatı
**Verdict: KISMEN (paketleme/mühendislik olarak Yeni; ilke/teorem olarak DEĞİL).**

- "may-analizine güvenme; serbest için her kullanımın confined olduğunun POZİTİF ispatını iste" = **her sound escape/region analizinin standart konservatif duruşu** (must-not-escape under-approximation = ders-kitabı güvenli yön). Reachability "Free to Move" bunu **no-use-after-kill** olarak zaten uyguluyor; her sound effect sistemi bu duruşta.
- KEMGU'nun **SPESİFİK confined-kullanım beyaz-listesi** (yerinde `v[i]` oku/yaz + retain-etmeyen-builtin ilk-argümanı), qualifier/effect tip-makinesi **OLMADAN**, bilinçle minik-sözdizimsel-karar-verilebilir → reachability'nin "sonuç-qualifier'ı bu değişkeni içermemeli"sinin **ucuz, annotation-suz yaklaşığı**. Bu **özgün artifact**, ama yeni güvenlik-ilkesi değil.
- **En savunulabilir olduğu yer Vale'e karşı:** Vale aynı sorunu **runtime generational-check**'le aşar. KEMGU "**runtime-check YOK; statik pozitif-ispatla erken/per-iter serbest**" — bu kontrast somut ve yayınlanabilir (ama "yeni analiz-tasarım fikri", "yeni teorem" değil).
- **Mehmet ASLA dememeli:** "may-analizine güvenmeme" yeni bir fikir. Cyclone/Verona aynı garantiyi **inşa-gereği** zaten may-analizi olmadan kazanıyor → "güvenmiyoruz" çerçevesi onlara karşı hafif strawman.

### Delta (c) — mekanize-sound + self-hosting + OS kombinasyonu
**Verdict: KISMEN (bundle olarak Yeni; araştırma-katkısı olarak İNCE) — ve bazı bacaklarda KEMGU GERİDE.**

- **Bundle olarak hiçbir tek sistem birebir eşleştirmiyor** → 4'lü {annotation-suz-region-inference + kısmî-mekanize-DRF + self-host-with-confinement + OS} dolu değil.
- **AMA "yeni kombinasyon" PL-makalesi için en zayıf yenilik kaydı** — hakem ağır iskonto eder, "hangi TEK bacak araştırma-katkısı?" diye sorar.
- **Bacak-bacak KEMGU bazılarında GERİDE:**
  - *Mekanizasyon:* KEMGU yalnız **DRF**'i kısmen Lean'de mekanize etti; region/bellek soundness'i **kâğıtta**. Reachability **tam Rocq type-soundness**, DRF-à-la-Mode **Iris** — frontier KEMGU'yu **out-mekanize ediyor.** "Mekanize-sound region dili" KEMGU lehine bir üstünlük DEĞİL.
  - *Self-host + OS + sound-by-construction birlikte:* **ATS bunu zaten yapıyor** (Anairiats self-host + xlq/aos kernel + dependent/linear inşa-gereği-soundness, KEMGU'nun kısmî-Lean-DRF'inden daha güçlü temel).
- **Gerçek-kalan:** self-host-**with-region-confinement-applied-to-its-own-loop-arrays at byte-identik fixpoint** + per-iterasyon serbest — bu spesifik artifact-kombosu ATS'te bile bu haliyle yok. Ama bu "çalışan mühendislik portföyü", "araştırma yeniliği" değil.

---

## ADIM 4 — NET VERDICT

### (1) Top-PL-venue (TOPLAS / OOPSLA-sınıfı) **MEKANİZMA-yeniliği** için savunulabilir delta VAR MI?

**HAYIR.** Acımasız-dürüst sonuç: **mekanizma-yeniliği çekirdeği savunulamaz.**

- (a) çekirdek = **1994 Tofte-Talpin / ML Kit** (per-iter dâhil). 30+ yıllık prior art.
- (b) ilke = **standart sound-konservatif tip/effect duruşu** (reachability no-use-after-kill). Yeni teorem değil.
- (c) = **en zayıf yenilik kaydı** (bundle); üstelik mekanizasyon bacağında frontier (reachability/DRF-Mode) KEMGU'yu **geçiyor**, self-host+OS+soundness bacağında **ATS** önde.
- Frontier (reachability types, capturing types/Scala 3, DRF-à-la-Mode) bu tasarım-uzayını **dolu** tutuyor ve aliasing/closure/concurrency'de KEMGU'dan **daha derin + daha ifade-güçlü + daha mekanize.**

**Tek sivri-ama-İNCE mekanizma sliv'i:** "annotation-suz **per-iterasyon** region-serbest, **imperatif kernel-loop** için, pozitif-confinement predikatıyla, çalışan self-hosting derleyicide gösterilmiş." Bu **gerçek ve dolu-değil** — ama TOPLAS/OOPSLA tam-makale eşiğinin altında; en fazla **kısa-makale / workshop / mühendislik-notu**, ve **ML Kit region-reset + TT/Cyclone döngü-sızıntısına karşı dürüstçe benchmark'lanmak** zorunda, "yeni güvenlik-teorisi" değil "mühendislik" olarak çerçevelenerek.

### (2) Savunulabilir delta yoksa hangi alternatif framing değer taşır?

**ARTIFACT / EXPERIENCE / SİSTEM-ENTEGRASYONU çerçevesi — mekanizma değil.**

Dürüst katkı, *tek bir yeni mekanizma* değil, **bütünleşik çalışan bir artifact + deneyim raporu**:

> Tam-yığın, **self-hosting**, **OS-hedefli**, **annotation-suz region-inference** bir sistem programlama dili; defansif **pozitif-confinement** implementasyon-disiplini + kısmî **mekanize DRF** + **linear/capability/realtime** uzantıları ile — *çalışan, byte-identik-fixpoint'e self-derlenen, kendi döngü-dizilerini per-iterasyon serbest eden* bir bütün.

Yenilik kaydı açıkça **"BU kombinasyonu çalışır bir bellek-güvenli sistem dili olarak ilk sevk eden"** — bundle-yeniliği olduğu açıkça hedge edilerek.

**Uygun venue'ler (mekanizma-tam-makale DEĞİL):**
- Artifact / tool / experience track'leri: OOPSLA/PLDI/SPLASH **artifact**, `<Programming>`, **Onward!**, SLE, GPCE.
- Q1 yazılım dergileri: SP&E, Science of Computer Programming, JSS.
- Sistem venue'leri (OS bacağı olgunlaşınca): güvenli-sistem-dili + kernel deneyimi.

**Ne sevk EDİLMEMELİ (red garantili):** "annotation-suz region inference icat ettik" · "may-analizine güvenmeme yeni ilke" · "ilk mekanize-sound region dili" · "döngü belleğini per-iterasyon ilk serbest eden."

### (3) Mehmet için pratik öneri (karar-girdisi, emir değil)

1. **Region-inference-yeniliği çerçevesini TAMAMEN bırak.** İlk paragrafta TT/ML Kit/Cyclone'u prior-art olarak SEN sırala — hakem senden önce.
2. Eğer mekanizma-açısı zorlanacaksa: **tek sivri çekirdek = per-iterasyon confined serbest** (confined-ispat ∧ döngü-gövde-taze-binding) + pozitif-confinement predikatı; **ML Kit region-reset + TT/Cyclone döngü-sızıntısına karşı ölçülür**, mühendislik-delta olarak çerçevelenir. Bu en fazla kısa-makale.
3. **Gerçek değer = artifact/deneyim.** Self-host fixpoint + OS-hedefi + güvenlik-yığını (region + linear + capability + realtime + kısmî-DRF) **çalışan bir bütün** olarak — bunu headline yap.
4. **Yıllar-ölçekli risk:** Eğer hedef "yeni bellek-güvenliği teorisi"yse, frontier (reachability/capturing/modes) zaten önde ve hızlı; KEMGU oraya mekanizma-rakibi olarak girerse **kaybeder.** Eğer hedef "Türkçe-DNA, güvenli, self-hosting, OS-yetenekli üretim-dili + artifact"sa, bu **savunulabilir ve değerli** — ama akademik-yenilik kaydında değil, mühendislik/ürün/deneyim kaydında.

---

> **Tek-cümle NET:** KEMGU'nun bellek-modelinde top-PL-venue düzeyinde **savunulabilir MEKANİZMA-yeniliği YOK** (çekirdek = 1994 Tofte-Talpin; frontier dolu ve daha derin); **savunulabilir değer ARTIFACT/DENEYİM/ENTEGRASYON** ekseninde (self-host + OS + annotation-suz-region + güvenlik-yığını çalışan bir bütün olarak) — ve tek ince-ama-gerçek mekanizma sliv'i "imperatif kernel-loop için annotation-suz per-iterasyon confined serbest", yalnız kısa-makale/mühendislik-notu ölçeğinde, ML Kit/TT/Cyclone'a karşı dürüst benchmark'la.
