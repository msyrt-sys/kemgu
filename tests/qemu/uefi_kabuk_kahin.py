"""UEFI kabuğu: USB tuşları, tam seri, bağımsız PSF ve salt-okunur disk kâhini."""
import hashlib,re,struct,subprocess,time,os
from fat_yazici import MERHABA,NOT,KAYDIR
from psf_kahin import screendump,kabuk_goruntu,karsilastir

def keys(c):
    special={'/':('7',True),'.':('slash',False),' ':('spc',False),'\n':('ret',False),'ğ':('bracket_left',False),'ç':('dot',False),'ş':('semicolon',False),'İ':('apostrophe',True),'i':('apostrophe',False),'ı':('i',False)}
    return special.get(c,(c.lower(),c.isupper()))

def denetle(command,response,f,w,serial,p,n,ram,disk,mode='nvme',esp=False):
    start=serial.read_bytes().index(b'KEMGU-OS kabuk v0\n')
    before=hashlib.sha256(disk.read_bytes()).hexdigest()
    deadline=time.monotonic()+60
    expected=b'KEMGU-OS kabuk v0\nDISK '+{'nvme':b'NVMe','ahci':b'AHCI','usb':b'USB'}[mode]+b'\nKABUK_HAZIR\n'
    def actual():return serial.read_bytes()[start:]
    def await_output():
        while True:
            a=actual()
            if a==expected:return
            assert expected.startswith(a),(a,expected)
            assert p.poll() is None and time.monotonic()<deadline,(p.poll(),a)
            time.sleep(.01)
    await_output()
    if n==1:
        until=time.monotonic()+8
        while time.monotonic()<until:
            assert p.poll() is None and actual()==expected,'boşta kabuk değişti'
            time.sleep(.1)
    schema=command('query-qmp-schema')
    enum=next(x for x in schema if any(m.get('name')=='bracket_left' for m in x.get('members',[]) if isinstance(m,dict)))
    allowed=set(enum.get('values',[]))|{m['name'] for m in enum.get('members',[])}
    commands=['ls /','cat /merhaba.txt','cat /yok','echo Ağaç İş','cat /belgeler/not.txt','yanlis','cat /kaydir.txt']
    root=b'dalga8.txt\nbelgeler/\nmerhaba.txt\nkaydir.txt\n'
    if esp:root+=b'EFI/\nKEMGU/\n'
    outputs=[root,MERHABA+b'\n',b'HATA bulunamadi\n','Ağaç İş\n'.encode(),NOT,b'HATA bilinmeyen komut\n',KAYDIR]
    for text in commands:
        for c in text+'\n':assert keys(c)[0] in allowed
    assert {'shift','backspace'}<=allowed
    def key(k,down):
        command('input-send-event',{'events':[{'type':'key','data':{'down':down,'key':{'type':'qcode','data':k}}}]});time.sleep(.025)
    gw,gh,px=screendump(f,w/'kabuk.ppm',response)
    akis=expected.decode()
    def ekran(girdi=''):
        exp=kabuk_goruntu(akis,girdi,gen=gw,yuk=gh);until=time.monotonic()+5
        while True:
            width,height,px=screendump(f,w/'kabuk.ppm',response);assert (width,height)==(gw,gh)
            diff,first=karsilastir(px,exp,gw)
            if not diff:return
            assert time.monotonic()<until,(diff,first,girdi)
            time.sleep(.02)
    ekran()
    for text,out in zip(commands,outputs):
        for c in text:
            k,shift=keys(c)
            if shift:key('shift',True)
            key(k,True);key(k,False)
            if shift:key('shift',False)
        ekran(text)
        key('x',True);key('x',False);ekran(text+'x')
        key('backspace',True);key('backspace',False);ekran(text)
        assert actual()==expected
        key('ret',True);key('ret',False)
        expected+=out+b'KABUK_HAZIR\n';await_output()
        akis+='> '+text+'\n'+(out+b'KABUK_HAZIR\n').decode();ekran()
    if os.environ.get('UEFI_PS2_IRQ')=='1' or os.environ.get('UEFI_PS2_KARMA')=='1':
        from ps2_irq_kahin import denetle as irq_denetle
        irq_denetle(command,response,f,w,serial,p,expected,akis,start,gw,gh)
    assert time.monotonic()<deadline,'kabuk 60 saniyeyi aştı'
    symbols={l.split()[-1]:int(l.split()[0],16) for l in subprocess.check_output(['nm','-n',str(w/'k.elf')],text=True).splitlines() if len(l.split())==3}
    hp=w/'heap.bin';command('pmemsave',{'val':symbols['kem_bump'],'size':8,'filename':str(hp)})
    used=int.from_bytes(hp.read_bytes(),'little')-symbols['__heap_start']
    assert 0<used<symbols['__heap_end']-symbols['__heap_start'],used
    print(f'✓ UEFI heap üst sınırı {used} bayt',flush=True)
    assert hashlib.sha256(disk.read_bytes()).hexdigest()==before,'salt okumada disk değişti'
    (w/f'kabuk-{mode}-{ram}-{n}.seri').write_bytes(serial.read_bytes())
    print(f'✓ UEFI kabuk {mode} {ram} #{n}: 7 komut, UTF-8, yankı/geri silme, 70 satır, GOP {gw}x{gh} birebir, SHA256 aynı',flush=True)
