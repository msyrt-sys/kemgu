"""Bağımsız FAT zinciri ve uzun ad okuyucusu. Girdi tek bir bölümün baytlarıdır."""
import struct

class Fat:
    def __init__(self, data):
        self.data = data
        self.stamps = {}
        sb, spc, reserved, copies = struct.unpack_from('<HBHB',data,11)
        size, = struct.unpack_from('<I',data,36)
        self.first = reserved + copies*size
        assert sb == 512 and copies == 2 and spc and spc & (spc-1) == 0
        assert len(data) == struct.unpack_from('<I',data,32)[0]*sb
        self.spc = spc
        self.fat = memoryview(data)[reserved*sb:(reserved+size)*sb]
        assert self.fat == data[(reserved+size)*sb:self.first*sb], 'FAT kopyaları farklı'
        self.count = (len(data)//sb-self.first)//spc
        self.root, = struct.unpack_from('<I',data,44)
        self.info,self.backup = struct.unpack_from('<HH',data,48)

    def chain(self,k):
        seen = set()
        result = bytearray()
        while k < 0x0ffffff8:
            assert 2 <= k < self.count+2 and k not in seen, ('bozuk zincir',k)
            seen.add(k)
            i=(self.first+(k-2)*self.spc)*512
            result += self.data[i:i+self.spc*512]
            k = struct.unpack_from('<I',self.fat,k*4)[0] & 0x0fffffff
        return bytes(result)

    def directory(self,k):
        if k==0:
            return {}
        result = {}
        entries = []
        raw=self.chain(k)
        for offset in range(0,len(raw),32):
            e=raw[offset:offset+32]
            if not e[0]:
                break
            if e[0]==229:
                entries.clear()
                continue
            if e[11]==15:
                entries.append(e)
                continue
            short=e[:11]
            if entries:
                chk=0
                for b in short:
                    chk=(((chk&1)<<7)+(chk>>1)+b)&255
                assert all(row[13]==chk for row in entries), 'LFN sağlama'
                assert [row[0] for row in entries]==[64|len(entries),*range(len(entries)-1,0,-1)]
                offsets=(1,3,5,7,9,14,16,18,20,22,24,28,30)
                words=[struct.unpack_from('<H',row,o)[0] for row in reversed(entries) for o in offsets]
                if 0 in words:
                    words=words[:words.index(0)]
                name=struct.pack(f'<{len(words)}H',*words).decode('utf-16le')
                entries.clear()
            else:
                name=short[:8].rstrip().decode('ascii')
                if suff:=short[8:].rstrip():
                    name+='.'+suff.decode('ascii')
            if e[11]&8:
                continue
            assert name not in result, name
            c=struct.unpack_from('<H',e,20)[0]<<16 | struct.unpack_from('<H',e,26)[0]
            result[name]=(c,struct.unpack_from('<I',e,28)[0],bool(e[11]&16))
            self.stamps[k,name]=(e[13],*struct.unpack_from('<HHH',e,14),
                                 *struct.unpack_from('<HH',e,22))
        return result

    def get(self,path):
        parts=path.strip('/').split('/')
        k=self.root
        for i,name in enumerate(parts):
            k,n,d=self.directory(k)[name]
            if i+1 < len(parts):
                assert d, path
        if d:
            return self.directory(k)
        return self.chain(k)[:n] if n else b''

    def timestamp(self,path):
        k=self.root
        parts=path.strip('/').split('/')
        for name in parts[:-1]:
            k,_,d=self.directory(k)[name]
            assert d
        self.directory(k)
        return self.stamps[k,parts[-1]]

    def verify_info(self):
        free=sum((struct.unpack_from('<I',self.fat,4*k)[0]&0xfffffff)==0
                 for k in range(2,self.count+2))
        for s in (self.info,self.backup+self.info):
            count,hint=struct.unpack_from('<II',self.data,s*512+488)
            assert count==free and (hint==0xffffffff or 2<=hint<self.count+2),(count,free,hint)
