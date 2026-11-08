import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import d3d, matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.collections import PolyCollection
names=sys.argv[2:]; src=sys.argv[1]
fig,axs=plt.subplots(2,len(names)//2+len(names)%2,figsize=(18,7))
for ax,n in zip(axs.flat,names):
    V,F=d3d.load(os.path.join(src,n))
    polys=[[(V[i][2],V[i][1]) for i in f[:3]] for f in F]   # side view: z (length) vs y (height)
    shade=[abs(sum(V[i][3] for i in f[:3]))/3 for f in F]
    ax.add_collection(PolyCollection(polys,array=None,facecolors=[(0.2+0.6*s,)*3 for s in shade],edgecolors='k',linewidths=0.2))
    ax.autoscale(); ax.set_aspect('equal'); ax.set_title(f"{n}  ({len(V)}v/{len(F)}f)"); ax.axis('off')
plt.tight_layout(); plt.savefig(sys.argv[0].replace('render_cars.py','../../build/cars_side.png'),dpi=80)
