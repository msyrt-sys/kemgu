#!/usr/bin/env python3
# Linux nvme.h alanlarından sahte modelin bağımsız sayısal kâhini.
print('CAP',7|(1<<16)|(1<<24)|(1<<37))
print('DMA',4*4096+65536+4096,'MDTS',4096<<1,'aktarım',36*512)
print('IO parçaları', [16,16,4], 'toplam komut',3+3+1)
