from PIL import Image, ImageDraw, ImageFont
Q=[];B=[];L=[]
for ln in open('/tmp/atlas-shot/w.txt'):
    p=ln.split()
    if p[0]=='Q': Q.append(list(map(float,p[1:5]))+[int(p[5])]+list(map(float,p[6:9])))
    elif p[0]=='B': B.append(list(map(float,p[1:9]))+[int(p[9])])
    elif p[0]=='L': L.append((float(p[1]),float(p[2]),float(p[3])," ".join(p[4:])))
Wd,Ht=1800,1300; S=1.55; cx,cy=Wd/2,Ht/2+120
import math
# isometric-ish: rotate 35deg, tilt
a=math.radians(35); ca,sa=math.cos(a),math.sin(a)
def P(x,z,y=0):
    rx=x*ca-z*sa; rz=x*sa+z*ca
    return (cx+rx*S, cy+rz*S*0.55 - y*S*0.9)
img=Image.new('RGB',(Wd,Ht),(150,200,235)); d=ImageDraw.Draw(img)
def col(r,g,b,k=1): return (int(min(1,r*k)*255),int(min(1,g*k)*255),int(min(1,b*k)*255))
for x0,z0,x1,z1,s,r,g,b in Q:
    x0,x1=max(x0,-450),min(x1,450); z0,z1=max(z0,-450),min(z1,450)
    if x1<=x0 or z1<=z0: continue
    d.polygon([P(x0,z0),P(x1,z0),P(x1,z1),P(x0,z1)],fill=col(r,g,b))
# painter: sort by depth (rz)
def depth(b): x=(b[0]+b[2])/2; z=(b[1]+b[3])/2; return x*sa+z*ca
for b in sorted(B,key=depth):
    x0,z0,x1,z1,h,r,g,bb,st=b
    top=[P(x0,z0,h),P(x1,z0,h),P(x1,z1,h),P(x0,z1,h)]
    # visible faces: +z face (front) and +x face depending on rotation: front faces are z1 side and x0 side
    d.polygon([P(x0,z1,0),P(x1,z1,0),P(x1,z1,h),P(x0,z1,h)],fill=col(r,g,bb,0.75),outline=col(r,g,bb,0.5))
    d.polygon([P(x0,z0,0),P(x0,z1,0),P(x0,z1,h),P(x0,z0,h)],fill=col(r,g,bb,0.6),outline=col(r,g,bb,0.45))
    d.polygon(top,fill=col(r,g,bb,1.05),outline=col(r,g,bb,0.7))
try: f=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',17); F=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',34)
except: f=F=ImageFont.load_default()
for x,y,z,t in L:
    px,py=P(x,z,y); w=d.textlength(t,font=f)
    d.rectangle([px-w/2-4,py-11,px+w/2+4,py+11],fill=(255,255,255)); d.text((px-w/2,py-10),t,fill=(30,30,50),font=f)
d.text((24,20),"ZILLAPORT - procedural adventure city (purple = gauntlet-stone buildings + decoys)",fill=(20,30,60),font=F)
img.save('/workspace/tz_zillaport.png')
