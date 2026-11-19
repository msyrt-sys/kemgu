"""GPT sağlama ve EFI HD düğümü fikstürünü Python struct/uuid/zlib ile kur."""
import struct
import uuid
import zlib

kind = uuid.UUID('EBD0A0A2-B9E5-4433-87C0-68B6B72699C7').bytes_le
part = uuid.UUID('fedcba98-7654-3210-1122-334455667788').bytes_le
table = bytearray(512)
struct.pack_into('<16s16sQQQ', table, 128, kind, part, 2048, 6143, 0)
header = bytearray(512)
struct.pack_into('<8sIIIIQQQQ16sQIII', header, 0, b'EFI PART', 0x10000,
                 92, 0, 0, 1, 8191, 34, 8158, part, 2, 4, 128, zlib.crc32(table))
struct.pack_into('<I', header, 16, zlib.crc32(header[:92]))
node = struct.pack('<BBHIQQ16sBB', 4, 1, 42, 2, 2048, 4096, part, 2, 2)
for name, raw in [('baslik', header), ('girdiler', table), ('dugum', node)]:
    print(f'işlev {name}() -> Dizi<dtam8> {{')
    print(f'    değişken b: Dizi<dtam8> = bayt_sifir({len(raw)});')
    for i, v in enumerate(raw):
        if v:
            print(f'    yaz_u8(b,{i},{v});')
    print('    ver b;\n}')
