"""FADT ham RAM karşılaştırması + gerçek reset/kapanış; -no-reboot zorunlu."""
import os,re,struct,time,hashlib
from uefi_kabuk_kahin import keys

def denetle(command,dump,w,descriptors,text,serial,p):
    tables=[]
    for i,d in enumerate(descriptors):
        if d[0] not in (9,10):continue
        raw=dump(d[2],d[4]*4096,w/f'guc-ram-{i}.bin');pos=0
        while (pos:=raw.find(b'FACP',pos))>=0:
            at=pos;pos+=4
            if at+129>len(raw):continue
            size=struct.unpack_from('<I',raw,at+4)[0]
            if not 129<=size<=65536 or at+size>len(raw):continue
            t=raw[at:at+size]
            if sum(t)%256==0:tables.append(t)
    assert len(tables)==1,len(tables)
    t=tables[0];rev=t[8];boot=struct.unpack_from('<H',t,109)[0];flags=struct.unpack_from('<I',t,112)[0]
    space,width,offset,access,address=struct.unpack_from('<BBBBQ',t,116);value=t[128];pm=struct.unpack_from('<I',t,76)[0]
    for key,vals in [('FADT_DURUM',(rev,boot,flags)),('FADT_RESET',(space,address,value)),('FADT_GAS',(width,offset,access)),('FADT_PM',(pm,0,0))]:
        assert key+' '+' '.join(map(str,vals))+'\n' in text,(key,vals,text[-500:])
    assert rev>=3 and boot&2 and flags&1024 and (space,width,offset,access)==(1,8,0,0)
    mode=os.environ['UEFI_GUC'];direct=os.environ.get('UEFI_GUC_DOGRUDAN')=='1'
    if direct:
        prefix=text[text.index('GUC_HAZIR\n'):].encode();start=serial.read_bytes().index(b'GUC_HAZIR\n');request='r'
    else:
        start=serial.read_bytes().index(b'KEMGU-OS kabuk v0\n')
        prefix=b'KEMGU-OS kabuk v0\nDISK NVMe\nKABUK_HAZIR\n';request=mode
        assert serial.read_bytes()[start:]==prefix
    tail={'yenidenbaslat':b'GUC YENIDENBASLAT FADT\n','8042':b'GUC YENIDENBASLAT 8042\n','uclu':b'GUC YENIDENBASLAT UCLU\n','kapat':'Sistem durduruldu; güç tuşuna basın.\n'.encode()}[mode]
    expected=prefix+tail;before=hashlib.sha256((w/'disk.img').read_bytes()).hexdigest() if not direct else None
    schema=command('query-qmp-schema');allowed=next({m['name'] for m in e['members']} for e in schema if any(m.get('name')=='apostrophe' for m in e.get('members',[])))
    def press(c):
        key,shift=keys(c);assert key in allowed and not shift
        command('input-send-event',{'events':[{'type':'key','data':{'down':v,'key':{'type':'qcode','data':key}}} for v in (True,False)]})
        time.sleep(.003)
    try:
        if direct:press('r')
        else:
            for c in request:press(c)
            press('\n')
    except (AssertionError,BrokenPipeError,ConnectionResetError):
        # Reset son key komutunun QMP yanıtından önce gelebilir; aşağıda çıkış/seri tamlığı zorunlu.
        pass
    until=time.monotonic()+5
    while p.poll() is None and time.monotonic()<until:time.sleep(.02)
    assert p.poll()==(1 if mode=='kapat' else 0),(mode,p.poll(),serial.read_bytes()[-400:])
    data=serial.read_bytes();assert data.count(b'KEMGU-OS UEFI\n')==1,'QEMU yeniden açıldı'
    assert data[start:]==expected,(data[start:],expected)
    if before is not None:assert hashlib.sha256((w/'disk.img').read_bytes()).hexdigest()==before
    (w/f'guc-{mode}.seri').write_bytes(data)
    print(f'✓ GUC {mode}: FADT RAM birebir, tek açılış, seri birebir, QEMU rc={p.returncode}',flush=True)
