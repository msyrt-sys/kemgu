"""Kâhinin ölçtüğü zaman aralıkları (saniye)."""
# D-747: xHCI yoklama zaman aşımı yaklaşık 4 sn; boşta iki zaman aşımı aralığı gözlenir.
BOSTA_GOZLEM = 8
# ARP kâhininde 30 sn komşu kaydı yaşlanma sınırının aşılması gerekir.
ARP_YASLANMA = 31
# Konuk TCP yeniden iletimini tetiklemek için ACK, ilk RTO'yu geçecek kadar tutulur.
TCP_ACK_GECIKMESI = .45
# NTP eşinin yanıtı geciksin; gecikme ve saat farkı hesabını gerçek ağda sınar.
NTP_ES_GECIKMESI = .15
