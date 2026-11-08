import struct, sys
def rd(f,o,n): f.seek(o); return f.read(n)
def main(p):
    f=open(p,'rb'); pe=struct.unpack('<I',rd(f,0x3c,4))[0]
    nsec=struct.unpack('<H',rd(f,pe+6,2))[0]; osz=struct.unpack('<H',rd(f,pe+20,2))[0]
    opt=pe+24; base=struct.unpack('<I',rd(f,opt+28,4))[0]; ep=struct.unpack('<I',rd(f,opt+16,4))[0]
    secs=[]
    for i in range(nsec):
        s=rd(f,opt+osz+40*i,40); secs.append((s[:8].rstrip(b'\0').decode(),*struct.unpack('<IIII',s[8:24])))
    def off(rva):
        for n,vs,va,rs,ro in secs:
            if va<=rva<va+max(vs,rs): return rva-va+ro
    print('imagebase %x entry %x'%(base,ep)); [print(' sec',s) for s in secs]
    irva=struct.unpack('<I',rd(f,opt+104,4))[0]; o=off(irva); tot=0
    while True:
        d=struct.unpack('<5I',rd(f,o,20)); o+=20
        if d[3]==0: break
        name=rd(f,off(d[3]),64).split(b'\0')[0].decode(); t=off(d[0] or d[4]); fns=[]
        while True:
            v=struct.unpack('<I',rd(f,t,4))[0]; t+=4
            if v==0: break
            fns.append('#%d'%(v&0xffff) if v&0x80000000 else rd(f,off(v)+2,64).split(b'\0')[0].decode())
        tot+=len(fns); print('%s (%d): %s'%(name,len(fns),', '.join(fns)))
    print('total imports',tot)
main(sys.argv[1])
