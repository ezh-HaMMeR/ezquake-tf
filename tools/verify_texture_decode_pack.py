"""Compare every PNG/TGA in a PAK with independent Pillow RGBA decoding.
Usage: py tools/verify_texture_decode_pack.py decoder_tests.exe file.pak
Requires Pillow; writes temporary files only, leaves the PAK untouched.
"""
import hashlib
import io
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
from PIL import Image

def verify(executable, pak):
    data = Path(pak).read_bytes()
    magic, offset, size = struct.unpack_from('<4sII', data)
    assert magic == b'PACK' and size % 64 == 0
    count = 0
    with tempfile.TemporaryDirectory(prefix='ezquake-decode-') as tmp:
        source, output = Path(tmp)/'input', Path(tmp)/'output.rgba'
        for pos in range(offset, offset+size, 64):
            name, start, length = struct.unpack_from('<56sII', data, pos)
            name = name.split(b'\0')[0].decode('ascii')
            if not name.endswith(('.png', '.tga')):
                continue
            encoded = data[start:start+length]
            expected = Image.open(io.BytesIO(encoded)).convert('RGBA').tobytes()
            source.write_bytes(encoded)
            subprocess.run([str(executable),str(source),str(int(name.endswith('.png'))),str(output)],check=True)
            actual = output.read_bytes()
            assert actual == expected, name
            print(name, hashlib.sha256(actual).hexdigest())
            count += 1
    print(f'{pak}: {count} textures are pixel-exact')

if __name__ == '__main__':
    verify(Path(sys.argv[1]).resolve(), sys.argv[2])
