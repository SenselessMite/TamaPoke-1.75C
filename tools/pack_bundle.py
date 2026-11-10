#!/usr/bin/env python3
"""Empaqueta todos los sprites de la SD (tools/sdcard/mons/*.bin) en un unico
fichero web/sprites.pak para que el instalador web los suba de un clic.

Formato TPAK (little-endian):
  char[4]  "TPAK"
  uint16   count
  count x { uint8 nameLen; char name[nameLen]; uint32 size }   (indice)
  ...datos de cada fichero, en el mismo orden...

El instalador (web/index.html) lo descarga, lo parte por el indice y manda cada
fichero a la placa con el protocolo PUT (igual que tools/send_sd.py).

Cada fichero va envuelto en TPKZ (zlib, ver tools/tpkz.py): ~1.2 MB en vez de
~41 MB, lo que permite guardarlos en la flash interna de la 1.75C (sin SD).
Con --raw se empaquetan sin comprimir (solo para firmware anterior a la 1.19).
"""
import glob
import os
import struct
import sys

from tpkz import wrap

HERE = os.path.dirname(__file__)
MONS = os.path.join(HERE, 'sdcard', 'mons')
OUT = os.path.join(HERE, '..', 'web', 'sprites.pak')


def main():
    files = sorted(glob.glob(os.path.join(MONS, '*.bin')))
    if not files:
        raise SystemExit('no hay sprites en ' + MONS)
    names = ['mons/' + os.path.basename(f) for f in files]
    blobs = [open(f, 'rb').read() for f in files]
    raw_total = sum(len(b) for b in blobs)
    if '--raw' not in sys.argv[1:]:
        blobs = [wrap(b) for b in blobs]

    with open(OUT, 'wb') as o:
        o.write(b'TPAK')
        o.write(struct.pack('<H', len(files)))
        for name, blob in zip(names, blobs):
            nb = name.encode()
            o.write(struct.pack('<B', len(nb)))
            o.write(nb)
            o.write(struct.pack('<I', len(blob)))
        for blob in blobs:
            o.write(blob)

    print(f'{os.path.normpath(OUT)}: {len(files)} sprites, {raw_total / 1048576:.1f} MB datos '
          f'({os.path.getsize(OUT) / 1048576:.1f} MB en el bundle)')


if __name__ == '__main__':
    main()
