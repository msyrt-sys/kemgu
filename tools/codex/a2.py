from yardim import testler
header='''// Sabitler: python3 tools/codex/a2.py; x=b1-256*(b0>>4&1), z=(b3&15)-16*(b3&8!=0).
kullan drivers::ps2::fare::{ FareDurumu, FareOlayi, fare_yeni, fare_bayt };
işlev yok(o: seçimlik<FareOlayi>) -> mantıksal {
    eşleş o { hiç => { ver doğru; } değer(v) => { ver yanlış; } }
}
işlev esit(o: seçimlik<FareOlayi>, x: tam32, y: tam32, z: tam32, tus: tam32) -> mantıksal {
    eşleş o {
        hiç => { ver yanlış; }
        değer(v) => { ver v.dx == x ve v.dy == y ve v.dz == z ve
            v.sol == ((tus & 1) != 0) ve v.sag == ((tus & 2) != 0) ve v.orta == ((tus & 4) != 0); }
    }
}
'''
cases={}
packets=[[8,5,3],[9,0,0],[10,0,0],[12,0,0],[15,255,255],[56,251,254],[56,0,0],[8,0,0,1],[8,0,0,15],[8,0,0,8],[8,0,0,247]]
for i,v in enumerate(packets):
    x=v[1]-256*bool(v[0]&16); y=v[2]-256*bool(v[0]&32)
    z=(v[3]&15)-16*bool(v[3]&8) if len(v)==4 else 0
    s=f'    değişken d: FareDurumu = fare_yeni({"doğru" if len(v)==4 else "yanlış"});\n'
    for b in v[:-1]: s+=f'    eğer değil yok(fare_bayt(&değişken d, {b})) {{ ver yanlış; }}\n'
    s+=f'    ver esit(fare_bayt(&değişken d, {v[-1]}), {x}, {y}, {z}, {v[0]&7}) ve d.n == 0;'
    cases[f'paket_{i}']=s
for size in (3,4):
    for overflow in (64,128,192):
        s=f'    değişken d: FareDurumu = fare_yeni({"doğru" if size==4 else "yanlış"});\n'
        for b in [8+overflow,1,1]+([0] if size==4 else []): s+=f'    eğer değil yok(fare_bayt(&değişken d, {b})) {{ ver yanlış; }}\n'
        s+='    eğer d.n != 0 { ver yanlış; }\n'
        for b in [8,5]+([3] if size==4 else []): s+=f'    fare_bayt(&değişken d, {b});\n'
        s+=f'    ver esit(fare_bayt(&değişken d, {0 if size==4 else 3}), 5, 3, 0, 0);'
        cases[f'tasma_{size}_{overflow}']=s
cases['senkron']='''    değişken d: FareDurumu = fare_yeni(yanlış);
    eğer değil yok(fare_bayt(&değişken d, 0)) veya d.n != 0 { ver yanlış; }
    fare_bayt(&değişken d, 8); fare_bayt(&değişken d, 1);
    eğer değil esit(fare_bayt(&değişken d, 2), 1, 2, 0, 0) { ver yanlış; }
    fare_bayt(&değişken d, 9); fare_bayt(&değişken d, 0);
    ver esit(fare_bayt(&değişken d, 0), 0, 0, 0, 1);'''
cases['bozuk_girdi']='''    değişken d: FareDurumu = fare_yeni(yanlış);
    fare_bayt(&değişken d, 8);
    eğer değil yok(fare_bayt(&değişken d, -1)) veya d.n != 0 { ver yanlış; }
    eğer değil yok(fare_bayt(&değişken d, 256)) { ver yanlış; }
    d.paket_boyu = 0;
    ver yok(fare_bayt(&değişken d, 8)) ve d.n == 0;'''
testler('tests/drivers/ps2/fare_test.kem',header,cases)
print(len(cases),'test')
