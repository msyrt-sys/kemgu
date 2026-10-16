# Tur 18 — wx_x86 — özet

- Durum: **SOR_HATA**
- finish: `None` · blok: `None`

## Hata (sor)
```
kota: günlük kalan 14932869 · pencere 0/2500000
Traceback (most recent call last):
  File "/home/msyrt/Desktop/kemgu/otomasyon/evren/executor.py", line 222, in <module>
    main()
  File "/home/msyrt/Desktop/kemgu/otomasyon/evren/executor.py", line 218, in main
    sor(a) if a.komut == "sor" else uygula(a)
    ^^^^^^
  File "/home/msyrt/Desktop/kemgu/otomasyon/evren/executor.py", line 158, in sor
    icerik, dusunme, usage, bitis, sure = akisli_sor(
                                          ^^^^^^^^^^^
  File "/home/msyrt/Desktop/kemgu/otomasyon/evren/executor.py", line 109, in akisli_sor
    with urllib.request.urlopen(istek, timeout=300) as y:   # okuma başına zaman aşımı
         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/usr/lib/python3.12/urllib/request.py", line 215, in urlopen
    return opener.open(url, data, timeout)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/usr/lib/python3.12/urllib/request.py", line 521, in open
    response = meth(req, response)
               ^^^^^^^^^^^^^^^^^^^
  File "/usr/lib/python3.12/urllib/request.py", line 630, in http_response
    response = self.parent.error(
               ^^^^^^^^^^^^^^^^^^
  File "/usr/lib/python3.12/urllib/request.py", line 559, in error
    return self._call_chain(*args)
           ^^^^^^^^^^^^^^^^^^^^^^^
  File "/usr/lib/python3.12/urllib/request.py", line 492, in _call_chain
    result = func(*args)
             ^^^^^^^^^^^
  File "/usr/lib/python3.12/urllib/request.py", line 639, in http_error_default
    raise HTTPError(req.full_url, code, msg, hdrs, fp)
urllib.error.HTTPError: HTTP Error 503: Service Unavailable
```

## git diff --stat
```
boot/start_x86_64.S | 20 ++++++++++++++++++++
 1 file changed, 20 insertions(+)
```
