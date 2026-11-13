/* UEFI 2.10 §§4,7,9,12,13: yalnız kullanılan ABI alanları.
 * Kullanılmayan yuvalar adlarıyla korunur; ofsetler efi_duzen.py ile doğrulanır. */
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef uint64_t U64;
typedef uint32_t U32;
typedef uint16_t U16;
typedef uint8_t U8;
typedef U64 Durum;
typedef struct { U32 a; U16 b,c; U8 d[8]; } Guid;
typedef struct { U64 imza; U32 surum,boy,crc,ayrilmis; } Baslik;
typedef struct Metin Metin;
struct Metin { void *reset; Durum (*yaz)(Metin*,U16*); };
typedef struct { Guid guid; void *adres; } Tablo;
typedef struct {
 Baslik bas;
 void *raise_tpl,*restore_tpl;
 Durum (*sayfa)(U32,U32,U64,U64*); void *free_pages;
 Durum (*harita)(U64*,void*,U64*,U64*,U32*);
 Durum (*havuz)(U32,U64,void**); Durum (*birak)(void*);
 void *create_event,*set_timer,*wait_event,*signal_event,*close_event,*check_event;
 void *install_protocol,*reinstall_protocol,*uninstall_protocol;
 Durum (*protokol)(void*,Guid*,void**);
 void *reserved,*register_notify,*locate_handle,*locate_device,*install_table;
 void *load_image,*start_image,*exit,*unload_image;
 Durum (*bitir)(void*,U64);
 void *get_count,*stall;
 Durum (*watchdog)(U64,U64,U64,U16*);
 void *connect,*disconnect,*open_protocol,*close_protocol,*protocol_info;
 void *protocols_per_handle,*locate_buffer;
 Durum (*bul)(Guid*,void*,void**);
} Servis;
typedef struct {
 Baslik bas; U16 *uretici; U32 surum;
 void *girdi_handle,*girdi,*cikti_handle; Metin *cikti;
 void *hata_handle,*hata,*runtime; Servis *servis;
 U64 adet; Tablo *tablolar;
} Sistem;
typedef struct {
 U32 surum; void *ebeveyn; Sistem *sistem; void *aygit,*yol,*ayrilmis;
 U32 secenek_boy; void *secenek,*taban; U64 boy; U32 kod_turu,veri_turu; void *kaldir;
} Yuklu;
typedef struct Dosya Dosya;
struct Dosya {
 U64 surum;
 Durum (*ac)(Dosya*,Dosya**,U16*,U64,U64);
 Durum (*kapat)(Dosya*); void *sil;
 Durum (*oku)(Dosya*,U64*,void*); void *yaz;
 Durum (*konum)(Dosya*,U64*); Durum (*konumla)(Dosya*,U64);
};
typedef struct Fs Fs;
struct Fs { U64 surum; Durum (*kok)(Fs*,Dosya**); };
typedef struct { U32 surum,gen,yuk,bicim,maskeler[4],satir; } KipBilgi;
typedef struct { U32 azami,kip; KipBilgi *bilgi; U64 bilgi_boy,fb,fb_boy; } Kip;
typedef struct { void *sorgu,*ayarla,*blt; Kip *kip; } Gop;
/* Açılış sözleşmesi: little-endian 24 x u64, ofset = alan indeksi * 8. */
typedef struct { U64 alan[24]; } Acilis;
#define OFS(T,F,N) _Static_assert(offsetof(T,F)==(N),#T "." #F)
OFS(Servis,sayfa,40); OFS(Servis,harita,56); OFS(Servis,protokol,152);
OFS(Servis,bitir,232); OFS(Servis,bul,320); OFS(Sistem,servis,96);
OFS(Sistem,adet,104); OFS(Sistem,tablolar,112); OFS(Metin,yaz,8);
OFS(Yuklu,aygit,24); OFS(Yuklu,taban,64); OFS(Dosya,oku,32);
OFS(Kip,fb,24); OFS(KipBilgi,satir,32);
_Static_assert(sizeof(Acilis)==192,"Acilis boyu");
