"""F1 bağımsız FAT/LFN okuyucusunun qtest imajına uyarlaması (KEMGU kodu kullanmaz)."""
import struct

def dogrula(path):
    data=path.read_bytes()
    def u16(o):return struct.unpack_from('<H',data,o)[0]
    def u32(o):return struct.unpack_from('<I',data,o)[0]
    sb=u16(11);spc=data[13];reserved=u16(14);fsz=u32(36)
    assert sb==512 and spc and spc&(spc-1)==0 and data[16]==2
    first=reserved+2*fsz;count=(u32(32)-first)//spc
    assert len(data)==u32(32)*sb
    fat=[u32(reserved*sb+4*i)&0xfffffff for i in range(count+2)]
    assert data[reserved*sb:(reserved+fsz)*sb]==data[(reserved+fsz)*sb:(reserved+2*fsz)*sb]
    def chain(k):
        seen=set();out=[]
        while k<0xffffff8:
            assert 2<=k<len(fat) and k not in seen
            seen.add(k);out.append(k);k=fat[k]
        return out
    def content(k):
        return b''.join(data[(first+(c-2)*spc)*sb:(first+(c-1)*spc)*sb] for c in chain(k))
    def checksum(short):
        s=0
        for b in short:s=(((s&1)<<7)+(s>>1)+b)&255
        return s
    def directory(k):
        out={};lfns={};checks=[];expected=0
        raw=content(k)
        for off in range(0,len(raw),32):
            rec=raw[off:off+32]
            if rec[0]==0:break
            if rec[0]==229:lfns={};checks=[];continue
            if rec[11]==15:
                seq=rec[0]&31
                if rec[0]&64:lfns={};checks=[];expected=seq
                assert seq==expected;expected-=1;checks.append(rec[13])
                lfns[seq]=b''.join(rec[o:o+2] for o in list(range(1,11,2))+list(range(14,26,2))+list(range(28,32,2)))
                continue
            short=rec[:11]
            if lfns:
                assert expected==0 and all(x==checksum(short) for x in checks)
                units=b''.join(lfns[i] for i in sorted(lfns));words=struct.unpack('<'+'H'*(len(units)//2),units)
                words=words[:words.index(0)] if 0 in words else words
                name=struct.pack('<'+'H'*len(words),*words).decode('utf-16le')
            else:name=short[:8].decode().rstrip()+('.'+short[8:].decode().rstrip() if short[8:].strip() else '')
            lfns={};checks=[]
            if rec[11]&8:continue
            assert name not in out
            out[name]=(struct.unpack_from('<H',rec,20)[0]<<16|struct.unpack_from('<H',rec,26)[0],struct.unpack_from('<I',rec,28)[0])
        return out
    files=directory(u32(44));assert set(files)=={'DALGA6.TXT'}
    cluster,size=files['DALGA6.TXT'];expected=bytes((i*29+7)&255 for i in range(1300))
    assert size==len(expected) and content(cluster)[:size]==expected
    assert len(chain(cluster))>=3
    free=sum(x==0 for x in fat[2:]);info=u16(48);backup=u16(50)
    assert u32(info*sb+488)==free and u32((backup+info)*sb+488)==free
    print(f'PY_FAT_KAHIN OK DALGA6.TXT {size} bayt, {len(chain(cluster))} küme, iki FAT ve FSInfo={free}')
