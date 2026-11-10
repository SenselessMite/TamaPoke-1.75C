#!/usr/bin/env python3
"""Envoltorio TPKZ: un fichero de sprites (TPK2/TPTH) comprimido con zlib.

Formato (little-endian), lo descomprime sdmon.cpp con el tinfl de la ROM:
  char[4]  "TPKZ"
  uint32   tamano sin comprimir
  ...      flujo zlib (con cabecera y adler32)

Los sprites crudos ocupan ~41 MB pero son casi todo pixeles transparentes: con
zlib bajan a ~1.2 MB. Asi caben en la flash interna de la 1.75C (sin ranura SD)
y la subida por USB tarda segundos en vez de ~10 minutos. El firmware sigue
aceptando ficheros sin comprimir, asi que las SD ya cargadas siguen valiendo.
"""
import struct
import zlib

MAGIC = b'TPKZ'


def wrap(data: bytes) -> bytes:
    if data[:4] == MAGIC:
        return data  # ya envuelto
    return MAGIC + struct.pack('<I', len(data)) + zlib.compress(data, 9)


def unwrap(blob: bytes) -> bytes:
    if blob[:4] != MAGIC:
        return blob
    (raw,) = struct.unpack('<I', blob[4:8])
    data = zlib.decompress(blob[8:])
    if len(data) != raw:
        raise ValueError('TPKZ: tamano descomprimido no coincide')
    return data
