"""ACPI kâhini: RAM bölgelerinde bağımsız imza taraması ve iki düzey sağlama."""
import struct,re

def denetle(dump,w,descriptors,text):
    regions=[]
    for i,d in enumerate(descriptors):
        if d[0] in (9,10):
            assert d[4]*4096<=16*1024*1024,'ACPI dump üst sınırı'
            regions.append((d[2],dump(d[2],d[4]*4096,w/f'acpi-ram-{i}.bin')))
    def read(addr,n):
        for base,data in regions:
            if base<=addr and addr+n<=base+len(data):return data[addr-base:addr-base+n]
        raise AssertionError(('ACPI adresi dump dışında',addr,n))
    def table(addr):
        h=read(addr,36);n=struct.unpack_from('<I',h,4)[0];assert 36<=n<=65536
        data=read(addr,n);assert sum(data)%256==0,(addr,data[:4])
        return data
    roots=[]
    for base,data in regions:
        pos=0
        while (pos:=data.find(b'RSD PTR ',pos))>=0:
            at=base+pos;pos+=8
            r=read(at,20)
            if sum(r)%256:continue
            rev=r[15];addr=struct.unpack_from('<I',r,16)[0];sig='RSDT';step=4
            if rev>=2:
                r=read(at,36);n=struct.unpack_from('<I',r,20)[0]
                if not 36<=n<=4096 or sum(read(at,n))%256:continue
                x=struct.unpack_from('<Q',r,24)[0]
                if x:addr=x;sig='XSDT';step=8
            if not addr:continue
            root=table(addr);assert root[:4].decode()==sig and (len(root)-36)%step==0
            children=[]
            for i in range(36,len(root),step):
                a=int.from_bytes(root[i:i+step],'little');t=table(a)
                children.append((t[:4].decode(),a,len(t)))
            roots.append((sig,addr,children,at))
    assert roots,'RAM taramasında geçerli RSDP bulunamadı'
    k=re.search(r'ACPI_KOK (XSDT|RSDT) (\d+)',text);assert k,text
    children=[(sig,int(a),int(n)) for sig,a,n in re.findall(r'ACPI_TABLO (.{4}) (\d+) (\d+)',text)]
    assert any((r[0],r[1],r[2])==(k[1],int(k[2]),children) for r in roots),(roots,k.groups(),children)
    print(f'✓ Bağımsız RAM ACPI: {[(r[0],hex(r[3])) for r in roots]}; tablolar {[x[0] for x in children]}',flush=True)
