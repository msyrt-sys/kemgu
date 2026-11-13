/* Freestanding EFI köprüsü: dosya yükle, firmware verisini taşı, devret.
 * Politika/ayırıcı/ACPI ayrıştırma/çizim KEMGU tarafındadır. */
#include "uefi.h"
static Sistem *s;
static const U64 HATA = 1ULL<<63;
static Guid yuklu_guid={0x5b1b31a1,0x9562,0x11d2,{0x8e,0x3f,0,0xa0,0xc9,0x69,0x72,0x3b}};
static Guid fs_guid={0x964e5b22,0x6459,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
static Guid gop_guid={0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
static void sifir(void *p,U64 n){U8 *b=p;while(n--)*b++=0;}
static void kopya(void *p,const void *q,U64 n){U8 *b=p;const U8 *a=q;while(n--)*b++=*a++;}
static U64 le(const U8 *p,unsigned n){U64 v=0;for(unsigned i=0;i<n;i++)v|=(U64)p[i]<<(8*i);return v;}
static int saglama(const U8 *p,U64 n){U8 v=0;while(n--)v+=*p++;return v==0;}
static Durum hata(U16 *m,Durum rc){s->cikti->yaz(s->cikti,L"EFI HATA ");s->cikti->yaz(s->cikti,m);s->cikti->yaz(s->cikti,L"\r\n");return rc?rc:HATA|1;}
static void hex(U16 *ad,U64 v){U16 b[19];const U16 *d=L"0123456789abcdef";for(unsigned i=0;i<16;i++)b[i]=d[(v>>((15-i)*4))&15];b[16]='\r';b[17]='\n';b[18]=0;s->cikti->yaz(s->cikti,ad);s->cikti->yaz(s->cikti,b);}
static void harita_tani(U64 lo,U64 hi){
 void *buf=0;if(s->servis->havuz(2,65536,&buf))return;
 U64 n=65536,key=0,step=0;U32 ver=0;
 if(!s->servis->harita(&n,buf,&key,&step,&ver)&&step>=40){
  for(U64 i=0;i<n;i+=step){U8 *p=(U8*)buf+i;U64 a=le(p+8,8),z=a+le(p+24,8)*4096;
   if(a<hi&&lo<z){hex(L"MAP tur ",le(p,4));hex(L"MAP bas ",a);hex(L"MAP son ",z);}
  }
 }
 s->servis->birak(buf);
}
static U64 sayfa(U64 n){U64 a=0x3fffffff;return s->servis->sayfa(1,2,n,&a)?0:a;}
/* ACPI GUID varsayılmaz: imza ve iki sağlama doğrulanıp bulunan GUID taşınır. */
static void acpi(Acilis *b){
 for(U64 i=0;i<s->adet;i++){
  const U8 *p=s->tablolar[i].adres;if(!p)continue;
  const char *sig="RSD PTR ";unsigned j=0;while(j<8&&p[j]==(U8)sig[j])j++;
  if(j!=8||!saglama(p,20))continue;
  if(p[15]>=2){U64 n=le(p+20,4);if(n<36||n>4096||!saglama(p,n))continue;}
  if(!b->alan[13]||p[15]>=2){b->alan[13]=(U64)p;kopya(&b->alan[18],&s->tablolar[i].guid,16);}
 }
}
Durum efi_main(void *handle,Sistem *system){
 s=system;Servis *bs=s->servis;Durum rc;Yuklu *image=0;Fs *fs=0;Dosya *root=0,*f=0;Gop *g=0;
 bs->watchdog(0,0,0,0);
 if((rc=bs->protokol(handle,&yuklu_guid,(void**)&image)))return hata(L"LoadedImage",rc);
 if((rc=bs->protokol(image->aygit,&fs_guid,(void**)&fs)))return hata(L"SimpleFileSystem",rc);
 if((rc=fs->kok(fs,&root)))return hata(L"kok",rc);
 if((rc=root->ac(root,&f,L"\\KEMGU\\CEKIRDEK.ELF",1,0)))return hata(L"ELF ac",rc);
 U64 boy=0;if((rc=f->konumla(f,~0ULL))||(rc=f->konum(f,&boy))||(rc=f->konumla(f,0)))return hata(L"ELF boy",rc);
 if(boy<64||boy>64*1024*1024)return hata(L"ELF sinir",0);
 U8 *elf=0;if((rc=bs->havuz(2,boy,(void**)&elf)))return hata(L"ELF tampon",rc);
 U64 okunan=boy;if((rc=f->oku(f,&okunan,elf))||okunan!=boy)return hata(L"ELF oku",rc);
 f->kapat(f);root->kapat(root);
 if(le(elf,4)!=0x464c457f||elf[4]!=2||elf[5]!=1||elf[6]!=1||le(elf+20,4)!=1||le(elf+52,2)!=64||le(elf+16,2)!=2||le(elf+18,2)!=62||le(elf+54,2)!=56)return hata(L"ELF baslik",0);
 U64 ph=le(elf+32,8),adet=le(elf+56,2),entry=le(elf+24,8),alt=~0ULL,ust=0;int giris=0;
 if(!adet||adet>128||ph>boy||adet*56>boy-ph)return hata(L"ELF program tablosu",0);
 /* Önce bütün aralıkları doğrula; bozuk dosya kısmen yüklenmez. */
 for(U64 i=0;i<adet;i++){
  U8 *p=elf+ph+i*56;if(le(p,4)!=1)continue;
  U64 off=le(p+8,8),v=le(p+16,8),a=le(p+24,8),disk=le(p+32,8),mem=le(p+40,8),flags=le(p+4,4);
  if(!mem||v!=a||a<0x100000||a>=0x40000000||mem>0x40000000-a||disk>mem||off>boy||disk>boy-off||(flags&3)==3)return hata(L"ELF LOAD",0);
  U64 lo=a&~4095ULL,hi=(a+mem+4095)&~4095ULL;
  for(U64 j=0;j<i;j++){U8 *q=elf+ph+j*56;if(le(q,4)==1){U64 x=le(q+24,8)&~4095ULL,y=(le(q+24,8)+le(q+40,8)+4095)&~4095ULL;if(lo<y&&x<hi)return hata(L"ELF cakisma",0);}}
  if(lo<alt)alt=lo;if(hi>ust)ust=hi;if((flags&1)&&entry>=a&&entry<a+mem)giris=1;
 }
 if(!giris)return hata(L"ELF giris",0);
 for(U64 i=0;i<adet;i++){
  U8 *p=elf+ph+i*56;if(le(p,4)!=1)continue;
  U64 a=le(p+24,8),mem=le(p+40,8),lo=a&~4095ULL,hi=(a+mem+4095)&~4095ULL;
  if((rc=bs->sayfa(2,2,(hi-lo)/4096,&lo))){hex(L"LOAD bas ",lo);hex(L"LOAD son ",hi);hex(L"LOAD rc ",rc);harita_tani(lo,hi);return hata(L"AllocateAddress",rc);}
  sifir((void*)lo,hi-lo);kopya((void*)a,elf+le(p+8,8),le(p+32,8));
 }
 bs->birak(elf);
 Acilis *b=(Acilis*)sayfa(1);void *map=(void*)sayfa(16);
 if(!b||!map)return hata(L"bilgi/harita sayfasi",0);sifir(b,4096);
 b->alan[0]=3551447044756161867ULL;b->alan[1]=sizeof(*b);b->alan[2]=1;
 if((rc=bs->bul(&gop_guid,0,(void**)&g)))return hata(L"GOP yok",rc);
 b->alan[3]=g->kip->fb;b->alan[4]=g->kip->fb_boy;b->alan[5]=g->kip->bilgi->gen;b->alan[6]=g->kip->bilgi->yuk;
 b->alan[7]=(U64)g->kip->bilgi->satir*4;b->alan[8]=g->kip->bilgi->bicim;
 b->alan[9]=(U64)map;b->alan[14]=alt;b->alan[15]=ust;b->alan[16]=65536;
 b->alan[20]=(U64)image->taban;b->alan[21]=(U64)image->taban+image->boy;
 acpi(b);if(!b->alan[13])return hata(L"gecerli RSDP yok",0);
 s->cikti->yaz(s->cikti,L"KEMGU EFI DEVIR\r\n");
 /* Başarısız ExitBootServices sonrası yalnız harita/çıkış yeniden denenir. */
 for(unsigned dene=0;dene<8;dene++){
  U64 n=65536,key=0,adim=0;U32 ver=0;
  rc=bs->harita(&n,map,&key,&adim,&ver);if(rc)break;
  b->alan[10]=n;b->alan[11]=adim;b->alan[12]=ver;b->alan[17]=dene+1;
  rc=bs->bitir(handle,key);if(!rc){
   void (__attribute__((sysv_abi)) *gir)(Acilis*)=(void*)entry;gir(b);
   for(;;)__asm__ volatile("cli; hlt");
  }
  if(rc!=(HATA|2))break;
 }
 /* Kısmen sonlandırılmış firmware'e dönülmez; görünür debug portu ve HLT. */
 const char *m="EFI HATA ExitBootServices\n";
 while(*m){U8 c=*m++;__asm__ volatile("outb %0,%1"::"a"(c),"Nd"((U16)0x3f8));}
 for(;;)__asm__ volatile("cli; hlt");
}
