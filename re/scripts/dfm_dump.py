"""Dumps Delphi binary form resources (TPF0) from a PE as JSON: component tree with props."""
import sys, struct, re, json
d = open(sys.argv[1], 'rb').read()
want = set(sys.argv[2:])
class R:
    def __init__(s, b, o): s.b, s.o = b, o
    def u8(s): v = s.b[s.o]; s.o += 1; return v
    def ss(s): n = s.u8(); v = s.b[s.o:s.o+n].decode('latin1'); s.o += n; return v
    def i(s, f, n): v = struct.unpack(f, s.b[s.o:s.o+n])[0]; s.o += n; return v
def val(r):
    t = r.u8()
    if t == 0: return None
    if t == 1:
        out = []
        while r.b[r.o] != 0: out.append(val(r))
        r.o += 1; return out
    if t == 2: return r.i('<b', 1)
    if t == 3: return r.i('<h', 2)
    if t == 4: return r.i('<i', 4)
    if t == 5: r.o += 10; return 'ext'
    if t in (6, 7): return r.ss()
    if t == 8: return False
    if t == 9: return True
    if t == 10: n = r.i('<I', 4); r.o += n; return f'<binary {n}>'
    if t == 11:
        out = []
        while True:
            s = r.ss()
            if not s: break
            out.append(s)
        return out
    if t == 12: n = r.i('<I', 4); v = r.b[r.o:r.o+n].decode('latin1'); r.o += n; return v
    if t == 13: return None
    if t == 14:
        items = []
        while r.b[r.o] != 0:
            if r.b[r.o] in (2, 3, 4): val(r)
            props = {}
            while True:
                k = r.ss()
                if not k: break
                props[k] = val(r)
            items.append(props)
        r.o += 1; return items
    if t == 15: return r.i('<f', 4)
    if t in (16, 17, 19): r.o += 8; return 'num8'
    if t == 18: n = r.i('<I', 4); v = r.b[r.o:r.o+2*n].decode('utf-16le'); r.o += 2*n; return v
    if t == 20: n = r.i('<I', 4); v = r.b[r.o:r.o+n].decode('utf8', 'replace'); r.o += n; return v
    raise ValueError(f'type {t} at {r.o}')
def comp(r):
    b = r.b[r.o]
    if b & 0xF0 == 0xF0:
        r.o += 1
        if b & 2: val(r)
    cls = r.ss(); name = r.ss(); props = {}
    while True:
        k = r.ss()
        if not k: break
        props[k] = val(r)
    kids = []
    while r.b[r.o] != 0: kids.append(comp(r))
    r.o += 1
    return {'class': cls, 'name': name, 'props': props, 'children': kids}
out = {}
for m in re.finditer(rb'TPF0', d):
    r = R(d, m.start() + 4)
    try:
        c = comp(r)
    except Exception as e:
        continue
    if not want or c['class'] in want: out[c['class']] = c
def strip(c):
    p = {k: v for k, v in c['props'].items() if not (isinstance(v, str) and v.startswith('<binary'))}
    return {'class': c['class'], 'name': c['name'], 'props': p, 'children': [strip(k) for k in c['children']]}
json.dump({k: strip(v) for k, v in out.items()}, sys.stdout, indent=1)
