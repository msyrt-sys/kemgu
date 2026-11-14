"""MADT RAM dökümü, UC sayfa yürüyüşü ve gerçek LAPIC kesme sayacı."""
import re,struct,time,subprocess

def denetle(command,dump,w,descriptors,text):
    assert 'APIC_HAZIR\n' in text,'UEFI LAPIC yolu kurulmadı'
    tables=[]
    for i,d in enumerate(descriptors):
        if d[0] not in (9,10):continue
        raw=dump(d[2],d[4]*4096,w/f'apic-ram-{i}.bin');pos=0
        while (pos:=raw.find(b'APIC',pos))>=0:
            at=pos;pos+=4
            if at+44>len(raw):continue
            size=struct.unpack_from('<I',raw,at+4)[0]
            if not 44<=size<=65536 or at+size>len(raw):continue
            t=raw[at:at+size]
            if sum(t)%256==0:tables.append(t)
    assert len(tables)==1,len(tables)
    t=tables[0];lapic=struct.unpack_from('<I',t,36)[0];io=[];iso=[];i=44
    while i<len(t):
        typ,n=t[i:i+2];assert n>=2 and i+n<=len(t)
        if typ==1:io.append(struct.unpack_from('<II',t,i+4))
        elif typ==2:
            bus,source,gsi,flags=struct.unpack_from('<BBIH',t,i+2);assert bus==0
            iso.append((source,gsi,flags))
        elif typ==5:lapic=struct.unpack_from('<Q',t,i+4)[0]
        i+=n
    measured=[tuple(map(int,x)) for x in re.findall(r'APIC_ISO (\d+) (\d+) (\d+)',text)]
    assert measured==iso,(measured,iso)
    lp=re.search(r'APIC_LAPIC (\d+) (\d+) (\d+)',text);assert lp and int(lp[1])==lapic
    actual_io=[tuple(map(int,x)) for x in re.findall(r'APIC_IO (\d+) (\d+) (\d+)',text)]
    assert [(a,g) for a,g,n in actual_io]==io
    routes=[tuple(map(int,x)) for x in re.findall(r'APIC_YOL (\d+) (\d+) (\d+)',text)]
    expected=[]
    for irq,vector,masked in ((0,32,False),(1,33,True),(12,44,True),(0,32,True)):
        gsi=irq;flags=0
        for src,g,f in iso:
            if src==irq:gsi=g;flags=f
        low=vector|(int(masked)<<16)|((flags&3==3)<<13)|(((flags>>2)&3==3)<<15)
        expected.append((irq,gsi,low))
    assert routes==expected,(routes,expected)
    symbols={v[2]:int(v[0],16) for l in subprocess.check_output(['nm',str(w/'k.elf')],text=True).splitlines() if len(v:=l.split())==3}
    def mem(a,n=8):return int.from_bytes(dump(a,n,w/'apic-oku.bin'),'little')
    for a in [lapic]+[a for a,g in io]:
        pt=symbols['uefi_pml4']
        for shift in (39,30):
            v=mem(pt+((a>>shift)&511)*8);assert v&1 and not v&128
            pt=v&((1<<52)-4096)
        v=mem(pt+((a>>21)&511)*8)
        assert v&0x9b==0x9b and v&((1<<52)-(1<<21))==a&~((1<<21)-1),(hex(a),hex(v))
    kal=re.search(r'APIC_KAL (\d+) (\d+) (\d+)',text);assert kal
    ticks,elapsed,period=map(int,kal.groups());assert 10<=ticks<=100 and period==elapsed//ticks and period>=100
    # HMP durumları ayrıca ham kanıt olarak saklanır; aşağıdaki sayım donanım davranışıdır.
    hw={}
    for name in ('lapic','pic'):
        out=command('human-monitor-command',{'command-line':'info '+name});hw[name]=out
        (w/f'apic-{name}.txt').write_text(out)
    assert re.search(r'LVTT\s+0x00020030.*periodic.*vec 48',hw['lapic']),hw['lapic']
    assert re.search(r'DCR=0x3.*initial_count = '+str(period)+r'\b',hw['lapic']),hw['lapic']
    assert 'APIC enabled' in hw['lapic'] and 'spurious vec 255' in hw['lapic']
    for pic in (0,1):assert re.search(r'pic'+str(pic)+r':.*imr=ff',hw['pic']),hw['pic']
    pins={int(a):int(b,16) for a,b in re.findall(r'pin\s+(\d+)\s+0x([0-9a-f]+)',hw['pic'])}
    for irq,gsi,low in expected[-3:]:
        assert pins[gsi]&((1<<32)-1)==low,(gsi,pins[gsi],low)
    start=mem(symbols['kap_lapic_tik']);pit=mem(symbols['kap_pit_tik']);kem=mem(symbols['kem_tik'])
    until=time.monotonic()+8
    while time.monotonic()<until:time.sleep(.1)
    end=mem(symbols['kap_lapic_tik']);pit_end=mem(symbols['kap_pit_tik']);kem_end=mem(symbols['kem_tik'])
    assert 400<=end-start<=1200 and pit==pit_end and kem_end-kem>=400,(start,end,pit,pit_end,kem,kem_end)
    print(f'✓ APIC: MADT bağımsız, IRQ0/1/12 GSI, UC, kalibrasyon {period}, 8 sn LAPIC +{end-start}/PIT +{pit_end-pit}',flush=True)
