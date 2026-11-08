"""Annotates hex constants in a Ghidra decompile with the Delphi AnsiString
literal they point to (refcount -1, length, chars). Usage: EXE IN.c > OUT.c"""
import re, struct, sys
d = open(sys.argv[1], 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]; optsz = struct.unpack_from('<H', d, pe + 20)[0]
base = struct.unpack_from('<I', d, pe + 52)[0]
secs = [struct.unpack_from('<IIII', d, pe + 24 + optsz + 40 * i + 8) for i in range(nsec)]
def off(v):
    v -= base
    for vs, va, rs, ro in secs:
        if va <= v < va + rs: return ro + v - va
def lit(v):
    o = off(v)
    if o is None or o < 8: return None
    rc, n = struct.unpack_from('<iI', d, o - 8)
    if rc != -1 or not 0 < n < 400: return None
    s = d[o:o + n]
    if d[o + n] != 0: return None
    return s.decode('latin1')
def sub(m):
    v = int(m.group(1), 16); s = lit(v)
    return m.group(0) + (f' /*"{s}"*/' if s is not None else '')
for line in open(sys.argv[2], encoding='latin1'):
    sys.stdout.write(re.sub(r'(?:0x|PTR_|DAT_|s_\w*?_)([0-9a-fA-F]{6,8})\b', sub, line))
