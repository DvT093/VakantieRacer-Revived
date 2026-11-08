import struct, sys, collections
ind=open(sys.argv[1],'rb').read()
n=struct.unpack('<H',ind[:2])[0]
ents=[]
for i in range(n):
    e=ind[2+24*i:26+24*i]; name=e[:20].split(b'\0')[0].decode('latin1'); off=struct.unpack('<I',e[20:24])[0]
    ents.append((name,off))
print('entries',n,'size ok',2+24*n==len(ind))
c=collections.Counter(x[0].rsplit('.',1)[-1].lower() for x in ents); print(c.most_common())
for k in sys.argv[2:]:
    print(k,':',' '.join(x[0] for x in ents if k.lower() in x[0].lower())[:1500])
