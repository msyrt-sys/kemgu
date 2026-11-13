"""Gerçek i8042 IRQ1/IRQ12, hızlı tuşlar, kayık AUX baytı ve save-under kâhini."""
import time,subprocess,re,os
from psf_kahin import screendump,kabuk_goruntu,karsilastir,fare_imleci

def denetle(command,response,f,w,serial,p,expected,akis,start,gw,gh):
    karma=os.environ.get('UEFI_PS2_KARMA')=='1'
    header=serial.read_bytes()[:start]
    assert b'GIRDI PS2 IRQ1 IRQ12\n' in header and b'PS2_FARE 0 3\n' in header,'PS/2 kesme yolu kurulmadı'
    enums=[set(v.get('values',[]))|{m['name'] for m in v.get('members',[]) if 'name' in m} for v in command('query-qmp-schema')]
    assert any({'left','right','middle'}<=e for e in enums),'QEMU düğme sözlüğü'
    assert any(set('abcdefghijklm')|{'o','ret','spc','apostrophe'}<=e for e in enums),'QEMU qcode sözlüğü'
    symbols={v[2]:int(v[0],16) for line in subprocess.check_output(['nm',str(w/'k.elf')],text=True).splitlines() if len(v:=line.split())==3}
    def mem(a):
        path=w/'irq-oku.bin';command('pmemsave',{'val':a,'size':8,'filename':str(path)});return int.from_bytes(path.read_bytes(),'little')
    time.sleep(.05)
    heap=mem(symbols['kem_bump']);ticks=mem(symbols['kap_lapic_tik'])
    until=time.monotonic()+8
    while time.monotonic()<until:
        assert p.poll() is None and serial.read_bytes()[start:]==expected,'boşta IRQ kabuğu değişti'
        time.sleep(.1)
    assert mem(symbols['kem_bump'])==heap,'timer uyanışları heap tüketti'
    assert 400<=mem(symbols['kap_lapic_tik'])-ticks<=1200,'boşta LAPIC ilerlemedi'
    if not karma:
        for attempt in range(20):
            regs=command('human-monitor-command',{'command-line':'info registers'})
            if 'HLT=1' in regs:break
            time.sleep(.01)
        assert 'HLT=1' in regs,'PS/2 kesme tüketicisi HLT ile beklemiyor'
    end=time.monotonic()+25;cursor=None
    def actual():return serial.read_bytes()[start:]
    def bekle():
        while actual()!=expected:
            assert expected.startswith(actual()) and p.poll() is None and time.monotonic()<end,(actual()[-200:],expected[-200:])
            time.sleep(.01)
    def ekran():
        exp=kabuk_goruntu(akis,'',gen=gw,yuk=gh)
        if cursor is not None:fare_imleci(exp,gw,gh,*cursor)
        until=time.monotonic()+3
        while True:
            width,height,px=screendump(f,w/'fare.ppm',response);assert (width,height)==(gw,gh)
            diff,first=karsilastir(px,exp,gw)
            if diff==0:return
            assert time.monotonic()<until,(diff,first,cursor)
            time.sleep(.02)
    def burst(text):
        # QEMU i8042 aygıt kuyruğu 16 bayt: 108 olay tek QMP çağrısına sığmaz.
        # Ardışık 54 tuş, 3 ms aralık; eski kâhinin 50 ms/tuş hızından >16 kat hızlı.
        if karma:
            from uefi_kabuk_kahin import keys
            def usb_key(k,down):
                command('input-send-event',{'events':[{'type':'key','data':{'down':down,'key':{'type':'qcode','data':k}}}]});time.sleep(.025)
            for c in text+'\n':
                key,shift=keys(c)
                assert any(key in e and (not shift or 'shift' in e) for e in enums),'QEMU qcode sözlüğü'
                if shift:usb_key('shift',True)
                usb_key(key,True);usb_key(key,False)
                if shift:usb_key('shift',False)
            return
        for c in text+'\n':
            key={' ':'spc','\n':'ret','i':'apostrophe'}.get(c,c)
            command('input-send-event',{'events':[{'type':'key','data':{'down':down,'key':{'type':'qcode','data':key}}} for down in (True,False)]})
            time.sleep(.003)
    sent=0
    if not karma:
        ring=mem(symbols['ps2_irq_klavye']);head=mem(ring+8)
        for letter in 'abcdefghjklm':
            text='echo '+letter*48;burst(text)
            out=(letter*48+'\nKABUK_HAZIR\n').encode();expected+=out;bekle();akis+='> '+text+'\n'+out.decode();ekran()
        # Her seri 54 tuş × bas/bırak; 12 seri 1024 yuvalı halkayı kesin sarar.
        sent=12*(len('echo ')+48+1)*2
        until=time.monotonic()+3
        while mem(ring)!=mem(ring+8) and time.monotonic()<until:time.sleep(.01)
        assert mem(ring)==mem(ring+8)==(head+sent)%1024,(head,mem(ring),mem(ring+8),sent)
    def fare(dx,dy,buttons):
        nonlocal expected,cursor
        command('input-send-event',{'events':[
            {'type':'rel','data':{'axis':'x','value':dx}},
            {'type':'rel','data':{'axis':'y','value':dy}},
            *({'type':'btn','data':{'button':name,'down':bool(buttons&bit)}} for name,bit in (('left',1),('right',2),('middle',4)))]})
        expected+=f'FARE dx={dx} dy={-dy} dugme={buttons}\n'.encode();bekle()
        x,y=(16,16) if cursor is None else cursor
        cursor=(min(gw-7,max(0,x+dx)),min(gh-7,max(0,y+dy)));ekran()
    fare(17,11,1)
    # Gerçek i8042 AUX çıkış tamponuna senkron biti olmayan bir bayt; belleğe test kancası yok.
    command('human-monitor-command',{'command-line':'o /b 0x64 0xd3'})
    command('human-monitor-command',{'command-line':'o /b 0x60 0x00'})
    time.sleep(.03);assert actual()==expected
    fare(-9,-4,0)
    text='echo imlec';burst(text);out=b'imlec\nKABUK_HAZIR\n';expected+=out;bekle();akis+='> '+text+'\n'+out.decode();ekran()
    if karma:
        # Aynı xHCI halkasında bekleyen klavye, fare ve ardından USB FAT32 okuması.
        from fat_yazici import MERHABA
        text='cat /merhaba.txt';burst(text);out=MERHABA+b'\nKABUK_HAZIR\n';expected+=out;bekle();akis+='> '+text+'\n'+out.decode();ekran()
    fare(4,3,2);fare(-5,-2,4);fare(0,0,0)
    counts=[mem(symbols[s]) for s in ('ps2_irq1_sayi','ps2_irq12_sayi')];assert (counts[0]==0 if karma else counts[0]>=sent) and counts[1]>=16,counts
    for name in ('ps2_irq_klavye','ps2_irq_fare'):
        ring=mem(symbols[name]);assert ring and mem(ring+16)==0,(name,ring)
    assert mem(symbols['ps2_irq_hata'])==0
    pic=command('human-monitor-command',{'command-line':'info pic'})
    for chip in (0,1):assert re.search(r'pic'+str(chip)+r':.*imr=ff',pic),pic
    for pin,vector in ((1,33),(12,44)):
        line=next(l for l in pic.splitlines() if re.search(r'pin\s+'+str(pin)+r'\s',l))
        assert 'masked' not in line and re.search(r'vec='+str(vector)+r'\s',line),line
    if karma:print(f'✓ PS2 KARMA: USB klavye + USB FAT32 + IRQ12, AUX senkron kurtarma, 5 hareket/düğme, save-under {gw*gh} piksel; IRQ sayıları {counts}, kayıp0',flush=True)
    else:
        print(f'✓ PS2 IRQ: kimlik0/3bayt, 12×48 hızlı tuş, halka sarımı, AUX senkron kurtarma, 5 hareket/düğme, save-under {gw*gh} piksel; IRQ sayıları {counts}, kayıp0',flush=True)
