#!/usr/bin/env python3
print('CAP', (1<<31)|(3<<8)|2, 'PI', (1<<0)|(1<<2), 'port2',256+128*2)
print('DMA',8192+65536,'aktarim',130*512,'parcalar',[128,2], 'komut',2+2+1)
print('CMD RO', (1<<15)|(1<<14), 'RW mask',0xffffffff^((1<<15)|(1<<14)))

print('PRDT ayrılmış maske',sum(1<<i for i in range(22,31)))
print('port2 kayıtları', {x:512+x for x in [0,4,8,12,16,20,24,32,36,40,48,52,56]})
print('ST/FRE temiz maskesi',0xffffffff^17)
