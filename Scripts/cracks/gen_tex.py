# Procedural (original) crack textures for Zoogi Roll ground cracks. Tileable Voronoi (F2-F1 edge distance).
import numpy as np
from PIL import Image
N=512; rng=np.random.default_rng(7)
def voronoi(cells, jitter=0.85):
    g=int(np.sqrt(cells)); pts=[]
    for i in range(g):
        for j in range(g):
            pts.append(((i+0.5+(rng.random()-0.5)*jitter)/g,(j+0.5+(rng.random()-0.5)*jitter)/g))
    pts=np.array(pts); P=[]
    for dx in (-1,0,1):
        for dy in (-1,0,1): P.append(pts+[dx,dy])
    P=np.concatenate(P); ids=np.tile(np.arange(len(pts)),9)
    y,x=np.mgrid[0:N,0:N]/N; xy=np.stack([x,y],-1).reshape(-1,2)
    best1=np.full(len(xy),9.); best2=np.full(len(xy),9.); id1=np.zeros(len(xy),int); p1=np.zeros((len(xy),2))
    for k,p in enumerate(P):
        d=np.linalg.norm(xy-p,axis=1)
        m=d<best1
        best2=np.where(m,best1,np.minimum(best2,d)); p1[m]=p; id1=np.where(m,ids[k],id1); best1=np.where(m,d,best1)
    # exact edge distance: project onto bisector between nearest and second-nearest
    edge=np.full(len(xy),9.)
    for k,p in enumerate(P):
        d=np.linalg.norm(xy-p,axis=1)
        sel=(ids[k]!=id1)|(np.abs(p-p1).sum(1)>1e-6)
        mid=(p1+p)/2; n=p-p1; nl=np.linalg.norm(n,axis=1,keepdims=True)+1e-9
        e=np.abs(((mid-xy)*(n/nl)).sum(1))
        edge=np.where(sel&(d<best2+0.5/np.sqrt(cells)),np.minimum(edge,e),edge)
    return edge.reshape(N,N), id1.reshape(N,N), len(pts)
def fbm(oct=5):
    out=np.zeros((N,N))
    for o in range(oct):
        f=4*2**o; g=rng.random((f,f))
        idx=np.arange(N)*f/N; i0=idx.astype(int); t=idx-i0; t=t*t*(3-2*t); i1=(i0+1)%f
        a=g[i0][:,i0]*(1-t)[None,:]+g[i0][:,i1]*t[None,:]; b=g[i1][:,i0]*(1-t)[None,:]+g[i1][:,i1]*t[None,:]
        out+=(a*(1-t)[:,None]+b*t[:,None])*0.5**o
    return (out-out.min())/(out.max()-out.min())
edge,cid,nc=voronoi(36)
cell=edge*np.sqrt(36)              # 0 at seam, ~0.5 at cell centre
noise=fbm()
wob=(fbm(3)-0.5)*0.05
seam=np.exp(-np.maximum(cell+wob,0)/0.035)          # thin bright seam with jagged width
seam=np.clip(seam*(0.6+0.8*noise),0,1)
bevel=np.clip((cell-0.03)/0.16,0,1); bevel=bevel*bevel*(3-2*bevel)
rnd=np.random.default_rng(3).random(nc)[cid]
rgba=np.stack([seam,bevel,rnd,noise],-1)
Image.fromarray((rgba*255+0.5).astype(np.uint8),'RGBA').save('T_ZoogiCrackCells.png')
# shaft / spark soft gradient (R = vertical shaft falloff, G = round spark)
y,x=np.mgrid[0:256,0:256]/255.
sh=np.sin(np.pi*x)**3*(1-y)**1.5*np.clip(y*12,0,1)
r=np.clip(1-np.hypot(x-.5,y-.5)*2,0,1)**2
Image.fromarray((np.stack([sh,r,r*0,np.ones_like(r)],-1)*255).astype(np.uint8),'RGBA').save('T_ZoogiCrackSoft.png')
# preview
pv=np.zeros((N,N,3)); st=np.array([.32,.30,.29])*(0.35+0.65*bevel[...,None])*(0.7+0.3*noise[...,None])
pv=st+np.array([1,.35,.05])*seam[...,None]*1.5
Image.fromarray((np.clip(pv,0,1)*255).astype(np.uint8)).save('preview_cells.png')
print('ok',nc)
