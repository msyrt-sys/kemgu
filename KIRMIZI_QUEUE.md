# KEMGU Kırmızı Karar Kuyruğu

Bu dosya agent'ın Kırmızı (spec gerektiren) kararlar için bekleme kuyruğudur.
Direktif Ek v1.1 Bölüm D formatına uyar.

Mehmet haftalık spec oturumunda veya istediği zaman temizler. Karar metni
sonra direktife yeni bölüm olarak işlenir.

---

## [K-1] DRF Teoremi Uzantı İspatı (Linear Types için)

TARİH: 2026-05-12

BAĞLAM: Linear Types Spec V1 Adım 10 (Bölüm B.4 / B.7) — spec'in production-grade
kullanımı için formal ispat şart. Şu an `--experimental-linear` flag arkasında
feature kapsamında.

SORU: DRF teoremi linear types eklemesi sonrası nasıl yeniden ispatlanır?
Mevcut DRF (Bölüm B.4 ispat eskizi): "tek sahip ⇒ aliasing yok ⇒ race yok"
Resmi ispat için Coq/Isabelle kodu güncellemesi gerek (~200-400 satır ek).
Bu agent'ın yeteneği dışı — Mehmet (veya teorem ispat uzmanı) yapmalı.

SEÇENEKLER:
  (a) Mehmet Coq/Isabelle ispatını kendi yapar → flag kaldırılır
  (b) Teorem uzmanı dış kaynaklı, ispat tamamlanır → flag kaldırılır
  (c) Pratik kullanım önce, ispat sonra (riskli ama feasible) → flag açık kalır

ETKİ EĞER ŞİMDİ CEVAPLANMAZSA: `--experimental-linear` flag arkasında kalır.
Production kullanım yapılamaz ama development ve test devam eder.

ALTERNATIF YOL: Tüm Linear Types Spec V1 implementasyon adımları (1-9) Yeşil
katmanda yürür — agent ispat olmadan çalışmaya devam eder.
