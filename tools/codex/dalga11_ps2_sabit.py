import re
from pathlib import Path
roots=sorted(Path('/usr/src').glob('*/include/linux/i8042.h'))
p=roots[-1];s=p.read_text()
for name,n in {'I8042_CMD_AUX_ENABLE':168,'I8042_CMD_AUX_SEND':212,'I8042_CMD_AUX_LOOP':211,'I8042_CTR_KBDINT':1,'I8042_CTR_AUXINT':2,'I8042_CTR_XLATE':64,'I8042_STR_AUXDATA':32,'I8042_STR_PARITY':128,'I8042_STR_TIMEOUT':64}.items():
 m=re.search(r'#define\s+'+name+r'\s+(0x[0-9a-f]+)',s);assert m and int(m[1],16)&255==n,(name,m)
 print(name,n)
print('i8042 başlığı',p,'CFG IRQ+XLATE',1|2|64,'saat engellerini kaldır',255^(16|32))
print('PIT böleni',round(1193182/100),'lo',11932&255,'hi',11932>>8,'mode0',3<<4)

print('PS2 komut/ACK', {s:int(s,16) for s in ('ad','a8','d4','f6','f2','f4','fa','ae')})
print('halka',1<<10,'başlık',3*8,'ayırma',3*8+(1<<10),'maske',(1<<10)-1,'hata',128|64)
print('imleç',7*7,'beyaz',(1<<24)-1,'sarma',12*(len('echo ')+48+1)*2)
