"""Produce the cloth, planted silhouettes and wind schedule for the title scene."""
import cv2
import numpy as np
from PIL import Image, ImageDraw


WIND_PERIOD = 18.0
WIND = [(0,.07),(1.35,.12),(2.4,.13),(3.7,.32),(4.8,.43),(6.9,.54),(8,.50),
        (8.7,.77),(9.35,.65),(10,.82),(10.7,1.0),(11.3,.79),(11.8,.96),
        (12.4,.51),(13.1,.27),(14.1,.11),(14.8,.18),(16,.27),(17.0,.13),(18,.07)]
CLOTH_PHASES = [
    ('01_calma',24,2.4), ('02_brisa',32,2.4), ('03_vento',48,3.2),
    ('04_rajada_moderada',40,2.0), ('05_rajada_intensa',48,2.4),
    ('06_recuperacao',32,2.4), ('07_transicao',48,3.2),
]


def wind_value(t):
    t %= WIND_PERIOD
    for (a,u),(b,v) in zip(WIND,WIND[1:]):
        if t <= b:
            s = (t-a)/(b-a)
            return u+(v-u)*s*s*(3-2*s)
    return WIND[-1][1]


def write_wind(path):
    path.write_text(f'{WIND_PERIOD}\n'+''.join(f'{x} {y}\n' for x,y in WIND),encoding='ascii')


def cloth_dynamics(samples):
    """Damped, coupled strips with increasing mass and weaker tension farther from the shoulder."""
    strips, joints = 5, 10
    distance = np.broadcast_to(np.linspace(0,1,joints),(strips,joints))
    band = np.linspace(-1,1,strips)[:,None]
    pos = np.zeros((strips,joints,2))
    speed = np.zeros_like(pos)
    step = 1/120
    start = -WIND_PERIOD*5
    t = start
    frames = []
    for target in samples:
        while t < target - 1e-7:
            dt = min(step,target-t)
            gust = np.array([[wind_value(t-float(s)*.34 + float(v)*.13) for s in row]
                             for row,v in zip(distance,band[:,0])])
            # Two wakes push different sections of the hem, including the separate upper scarf.
            wake = np.sin(t*3.76-distance*5.9+band*1.4)
            peel = np.sin(t*5.13-distance*11.1-band*2.7)
            load = (gust-.09)*( .7+.3*distance )
            force = np.stack((-load*(4.2+band*.9)+gust*distance*(1.5*wake+.8*peel),
                -load*(7.5-3.7*band) + distance*(2+gust*12)*wake +
                gust*gust*distance*(2+band)*peel),axis=2)
            along = (np.roll(pos,-1,axis=1)-pos)+(np.roll(pos,1,axis=1)-pos)
            along[:,-1] = pos[:,-2]-pos[:,-1]
            across = (np.roll(pos,-1,axis=0)-pos)+(np.roll(pos,1,axis=0)-pos)
            across[0],across[-1] = pos[1]-pos[0],pos[-2]-pos[-1]
            acceleration = (along*(92-26*distance[:,:,None])+across*18
                            -pos*(1.5+4*(1-distance[:,:,None])) -speed*3.2 +force*10)
            speed += acceleration*dt
            pos += speed*dt
            speed[:,0] = pos[:,0] = 0
            t += dt
        frames.append(pos.copy())
    # Remove the tiny nonperiodic residual left by the wake when the full timeline wraps.
    end_difference = frames[-1]-frames[0]
    return [p-end_difference*(time/WIND_PERIOD) for p,time in zip(frames,samples)]


def crease_sources(path, size):
    if not path.exists():
        return None
    sheet = Image.open(path).convert('RGBA')
    maps = []
    for i in range(6):
        col,row = i%2,i//2
        tile = sheet.crop((col*sheet.width//2,row*sheet.height//3,
                           (col+1)*sheet.width//2,(row+1)*sheet.height//3))
        box = tile.getchannel('A').point(lambda a: 255 if a>110 else 0).getbbox()
        if not box:
            return None
        rgb = np.asarray(tile.crop(box).resize(size,Image.Resampling.BILINEAR)).astype(np.float32)
        luma = .52*rgb[:,:,0]+.28*rgb[:,:,1]+.20*rgb[:,:,2]
        local = cv2.GaussianBlur(luma,(0,0),2)-cv2.GaussianBlur(luma,(0,0),8)
        maps.append(np.clip(local,-15,15)*(rgb[:,:,3]/255))
    return maps


def cloth_atlases(art, mask, source, atlas):
    box = (238,622,483,831)
    left,top,right,bottom = box
    pixels = np.dstack((art[top:bottom,left:right],mask[top:bottom,left:right]))
    h,w = pixels.shape[:2]
    y,x = np.mgrid[:h,:w].astype(np.float32)
    gx,gy = x+left,y+top
    length = np.clip((448-gx)/170,0,1)
    breadth = np.clip((gy-652-(448-gx)*.065)/107,0,1)
    # The brooch and broad collar remain rigid. The scarf separates from the lower hem on the left.
    loose = np.clip((435-gx)/25,0,1)
    loose = loose*loose*(3-2*loose)
    scarf = np.clip((710-gy)/34,0,1)
    loose = np.maximum(loose, scarf*np.clip((424-gx)/35,0,1))
    u = length*9
    v = (breadth**.74)*4
    ui,vi = np.minimum(u.astype(int),8),np.minimum(v.astype(int),3)
    ub,vb = (u-ui)[:,:,None],(v-vi)[:,:,None]
    times = []
    start = 0
    for _,count,duration in CLOTH_PHASES:
        times += [start+i*duration/count for i in range(count)]
        start += duration
    dynamics = cloth_dynamics(times+[WIND_PERIOD])[:-1]
    keys = crease_sources(source,(w,h))
    frames = []
    for t,points in zip(times,dynamics):
        low = points[vi,ui]*(1-ub)+points[vi,ui+1]*ub
        high = points[vi+1,ui]*(1-ub)+points[vi+1,ui+1]*ub
        field = (low*(1-vb)+high*vb)*loose[:,:,None]
        gust = wind_value(t)
        # Fold ridges travel downhill through the fabric and expose alternating faces.
        ridge = (gy-704) - .32*(448-gx) + field[:,:,0]*.43 + field[:,:,1]*.76
        trough = ridge + 23 - (17*gust+3*length)
        turn = np.exp(-(ridge/(4+gust*4))**2) - .68*np.exp(-(trough/6)**2)
        pinched = np.clip(turn*length*loose, -.6, .6) * (1.2+3.2*gust)
        mx = (x-field[:,:,0] + pinched*.24).astype(np.float32)
        my = (y-field[:,:,1] + pinched).astype(np.float32)
        frame = cv2.remap(pixels,mx,my,cv2.INTER_NEAREST)
        rgb = frame[:,:,:3].astype(np.float32)
        gold = (rgb[:,:,0]>128)&(rgb[:,:,1]>104)&(rgb[:,:,1]>rgb[:,:,2]+23)
        shade = turn*(6+10*gust)
        shade += (field[:,:,1]-.55*field[:,:,0])*.47
        if keys:
            k = gust*(len(keys)-1)
            ix = min(int(k),len(keys)-2)
            shade += (keys[ix]*(1-(k-ix))+keys[ix+1]*(k-ix))*.24
        shade *= loose*np.where(gold,.22,1)
        edge = (frame[:,:,3]>0)&(cv2.erode(frame[:,:,3],np.ones((3,3),np.uint8))==0)
        rim = edge*loose*gust*np.clip((427-gx)/135,0,1)*np.clip((720-gy)/60,0,1)
        frame[:,:,:3] = np.clip(rgb + shade[:,:,None]*np.array([1,.38,.39]) +
                               rim[:,:,None]*np.array([10,2,3]),0,255).astype(np.uint8)
        frame[frame[:,:,3]==0] = 0
        assert np.array_equal(frame[(loose==0)&(pixels[:,:,3]>0)],pixels[(loose==0)&(pixels[:,:,3]>0)])
        frames.append(frame)
    assert len({frame.tobytes() for frame in frames})==272
    first,start = 0,0.0
    for name,count,duration in CLOTH_PHASES:
        atlas('characters/protagonist/cape_refinada/'+name,frames[first:first+count],(left,top),
              period=WIND_PERIOD,kind='c',start=round(start,3),duration=duration,delay=.22)
        first += count
        start += duration


def plant_keys(path):
    if not path.exists():
        return []
    sheet = Image.open(path).convert('RGBA')
    tiles = []
    for i in range(8):
        x,y=i%4,i//4
        tile = sheet.crop((x*sheet.width//4,y*sheet.height//2,
                          (x+1)*sheet.width//4,(y+1)*sheet.height//2))
        alpha = tile.getchannel('A').point(lambda a: 255 if a>134 else 0)
        box = alpha.getbbox()
        tiles.append(tile.crop(box) if box else None)
    return tiles


def plant_base(width, height, kind, key):
    """Roots converge at the origin. Branch topology varies independently of the motion grid."""
    dpr = 2
    cx,feet = width//2,height-4
    canvas = Image.new('RGBA',(width*dpr,height*dpr))
    draw=ImageDraw.Draw(canvas)
    rng=np.random.default_rng(kind*247+14)
    w=width-16
    h=height-8
    colors=[(24,14,23,255),(29,15,27,255),(38,18,32,255),(46,19,33,255)]
    if key:
        tile=key.copy()
        tile.thumbnail((max(5,int(w*.8))*dpr,max(6,int(h*.89))*dpr),Image.Resampling.LANCZOS)
        rgba=np.array(tile)
        luminance=np.max(rgba[:,:,:3],axis=2)/255
        rgba[:,:,:3]=np.stack((23+18*luminance,13+9*luminance,23+17*luminance),axis=2)
        rgba[:,:,3]=np.where(rgba[:,:,3]>95,rgba[:,:,3],0)
        tile=Image.fromarray(rgba)
        canvas.alpha_composite(tile,((width*dpr-tile.width)//2,feet*dpr-tile.height+3))
    count=5+kind%5
    for j in range(count):
        sign=-1 if j<count*.63 else 1
        stem_w=1+(kind%4 in (2,3))
        reach=float(rng.uniform(.20,.95))
        height_fraction=float(rng.uniform(.38,.95))
        dx=sign*(w*.43)*reach
        dy=-h*height_fraction
        root=(cx+(j-count/2)*.47,feet)
        knee=(root[0]+dx*.43,feet+dy*.52)
        tip=(root[0]+dx,feet+dy)
        def p(point):
            return tuple(round(z*dpr) for z in point)
        draw.line([p(root),p(knee),p(tip)],fill=colors[j%3],width=stem_w*dpr)
        if kind%4 in (2,3):
            draw.polygon([p(knee),p((tip[0]+sign*stem_w*1.5,tip[1]+4)),p(tip),
                          p((knee[0]-sign*stem_w,knee[1]+3))],fill=colors[1+j%3])
        if kind%5 in (0,3):
            draw.line([p(knee),p((knee[0]-dx*.23,knee[1]-h*.23))],fill=colors[j%3],width=dpr)
    return np.asarray(canvas.resize((width,height),Image.Resampling.NEAREST))


def bend_poses(sprite, flexibility, phase, *, hanging=False):
    h,w=sprite.shape[:2]
    y,x=np.mgrid[:h,:w].astype(np.float32)
    root = 80 if hanging else h-6
    length = np.clip((y-root)/max(h-root-4,1) if hanging else (root-y)/max(root-1,1),0,1)
    side = (x-w/2)/max(w/2,1)
    frames=[]
    for level in range(8):
        wind = level/7
        for flicker in range(8):
            beat=flicker*2*np.pi/8+phase
            ripple=np.sin(beat-length*4+side*2)*(.25+wind*.7)
            flex=length**1.72
            dx = flex*flexibility*(-wind+ripple*.22)
            dy = flex*length*flexibility*(.32*wind + .09*ripple)*(1 if not hanging else -1)
            frame=cv2.remap(sprite,(x-dx).astype(np.float32),(y-dy).astype(np.float32),cv2.INTER_NEAREST)
            assert np.array_equal(frame[length==0],sprite[length==0])
            frames.append(frame)
    return frames


def plant_atlases(art, vines, key_path, atlas):
    # Absolute floor contacts match the ledge and the visible top edge of the dark foreground.
    specs=[
        ('curta_esquerda',23,739,25,19,2.6),('fina_pedra',63,742,31,22,5.2),
        ('arbusto_seco',102,766,44,32,4.7),('capim_medio',141,783,36,23,6.4),
        ('folha_rente',196,781,29,17,3.0),('ramo_partido',239,820,32,36,7.9),
        ('pontas_secas',268,843,26,20,3.8),('capim_alto',518,854,51,48,12.7),
        ('folha_larga',553,894,46,37,7.7),('tufos_pedras',583,892,36,25,4.2),
        ('lanca_seca',622,880,34,29,7.9),('retorcida',684,893,43,32,9.3),
        ('junco_duplo',747,915,39,39,11.9),('capim_fino',794,920,38,41,13.0),
        ('folhas_agudas',869,927,49,38,9.5),('ramo_frontal',974,932,54,47,13.3),
    ]
    keys = plant_keys(key_path)
    for i,(name,x,y,w,h,flexibility) in enumerate(specs):
        key = keys[i//2] if keys and i%2 else None
        sprite=plant_base(w,h,i,key)
        latency=.13+(i*13%11)*.059
        atlas('vegetation/grass_refinada/'+name,bend_poses(sprite,flexibility,i*.61),
              (x-w//2,y-(h-4)),period=1.65+.13*i,kind='v',start=latency,duration=.38+.03*(i%5))
    box=(107,0,339,191)
    x,y,right,bottom=box
    sprite=np.dstack((art[y:bottom,x:right],vines[y:bottom,x:right]))
    atlas('vegetation/vines_refinadas/arco',bend_poses(sprite,5.4,1.17,hanging=True),(x,y),
          period=3.5,kind='v',start=.49,duration=.57)
