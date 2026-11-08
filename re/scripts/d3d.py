"""Vakantie Racer .d3d mesh reader: header (i32 -1, u32 nverts, u32 nfaces),
verts 49 B (pos3, normal3, uv2, rgba4 floats, u8 flag), faces 9 B (u16 a,b,c, u16 material, u8 flag)."""
import struct
def load(path):
    b=open(path,'rb').read()
    nv,nf=struct.unpack('<II',b[4:12]); o=12; V=[];F=[]
    for i in range(nv):
        f=struct.unpack('<12f',b[o:o+48]); V.append(f); o+=49
    for i in range(nf):
        a,bb,c,m=struct.unpack('<4H',b[o:o+8]); F.append((a,bb,c,m)); o+=9
    assert o==len(b),(o,len(b))
    return V,F
