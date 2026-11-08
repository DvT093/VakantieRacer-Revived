"""Lists Delphi (D2-D7, 32-bit) classes in a PE: VMT address, parent, published methods.
Usage: delphi_vmt.py EXE [ClassPrefix]"""
import struct, sys
d = open(sys.argv[1], 'rb').read()
pref = sys.argv[2] if len(sys.argv) > 2 else ''
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]
optsz = struct.unpack_from('<H', d, pe + 20)[0]
base = struct.unpack_from('<I', d, pe + 52)[0]
secs = []
for i in range(nsec):
    o = pe + 24 + optsz + 40 * i
    vs, va, rs, ro = struct.unpack_from('<IIII', d, o + 8)
    secs.append((va, max(vs, rs), ro, rs))
def off(v):
    v -= base
    for va, sz, ro, rs in secs:
        if va <= v < va + rs: return ro + v - va
    return None
def u32(v):
    o = off(v); return None if o is None else struct.unpack_from('<I', d, o)[0]
def sstr(v):
    o = off(v); n = d[o]; return d[o+1:o+1+n].decode('latin1')
classes = {}
for va, sz, ro, rs in secs:
    for o in range(ro, ro + rs - 4, 4):
        v = base + va + o - ro
        if struct.unpack_from('<I', d, o)[0] == v + 76:   # vmtSelfPtr
            vmt = v + 76
            try:
                name = sstr(u32(vmt - 44))
            except Exception:
                continue
            if not name.isidentifier(): continue
            classes[vmt] = name
for vmt, name in sorted(classes.items(), key=lambda x: x[1]):
    if not name.startswith(pref): continue
    par = u32(vmt - 36)
    pname = classes.get(u32(par), '?') if par else '-'
    print(f'{name} vmt={vmt:08X} parent={pname}')
    mt = u32(vmt - 52)
    if mt:
        o = off(mt); n = struct.unpack_from('<H', d, o)[0]; o += 2
        for _ in range(n):
            size, code = struct.unpack_from('<HI', d, o)
            nm = d[o+7:o+7+d[o+6]].decode('latin1')
            print(f'   {code:08X} {nm}')
            o += size
