"""Pack the supplied menu artwork and generated climate keys for Raylib."""
import argparse
import json
import math
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw
from menu_environment import cloth_atlases, plant_atlases, write_wind

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets" / "menu"
SOURCES = ROOT / "design" / "menu_animado" / "fontes"
REFINEMENT = SOURCES.parent / "refinamento"
W, H = 1672, 941
Y, X = np.mgrid[:H, :W]
LAYERS = []
EXPORT = None


def rgba(rgb, alpha):
    return np.dstack((rgb, np.clip(alpha, 0, 255).astype(np.uint8)))


def polygon(points, origin=(0, 0), zoom=1):
    mask = np.zeros((H, W), np.uint8)
    vertices = [(round(origin[0]+x/zoom), round(origin[1]+y/zoom)) for x,y in points]
    cv2.fillPoly(mask, [np.array(vertices, np.int32)], 255)
    return mask


def grow(mask, amount, shape=cv2.MORPH_ELLIPSE):
    return cv2.dilate(mask, cv2.getStructuringElement(shape, (2*amount+1,)*2))


def atlas(name, frames, position=(0, 0), *, period=0, drift=(0, 0), alpha=1, delay=0,
          kind='n', start=0, duration=0):
    h, w = frames[0].shape[:2]
    columns = min(len(frames), max(1, 1920//w), 8)
    sheet = Image.new('RGBA', (columns*w, math.ceil(len(frames)/columns)*h))
    for i, frame in enumerate(frames):
        image = Image.fromarray(frame)
        sheet.paste(image, ((i%columns)*w, (i//columns)*h))
        if EXPORT and len(frames) > 1:
            folder = EXPORT / name
            folder.mkdir(parents=True, exist_ok=True)
            image.save(folder / f'{Path(name).name}_{i+1:04}.png', compress_level=3)
    target = ASSETS / f'{name}.png'
    target.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(target, compress_level=6)
    LAYERS.append(dict(path=f'{name}.png', kind=kind, x=position[0], y=position[1], width=w, height=h,
                       columns=columns, frames=len(frames), period=period, driftX=drift[0],
                       driftY=drift[1], opacity=alpha, delay=delay, pivot=[w//2,h//2],
                       fps=len(frames)/(duration or period) if period else 0,
                       start=start, duration=duration, loop=bool(period and not duration), order=len(LAYERS)))


def cut(rgb, mask, box):
    x0,y0,x1,y1 = box
    return rgba(rgb[y0:y1,x0:x1], mask[y0:y1,x0:x1])


def flexible(sprite, count, *, anchor, sway, lift, root='left', rate=1.0):
    height,width = sprite.shape[:2]
    rows,cols = np.mgrid[:height,:width].astype(np.float32)
    if root == 'left':
        weight = np.clip((anchor[0]-cols)/max(anchor[0]-7,1),0,1)
    elif root == 'top':
        weight = np.clip((rows-anchor[1])/max(height-anchor[1]-6,1),0,1)
    else:
        weight = np.clip((anchor[1]-rows)/max(anchor[1],1),0,1)
    frames = []
    for i in range(count):
        phase = 2*np.pi*i/count
        wave = np.sin(phase - 3*weight)*.75 + np.sin(phase*2 + rows/height*5)*.25
        dx = sway*weight*wave
        dy = lift*weight*np.sin(phase + weight*4 + cols/width*rate)
        frame = cv2.remap(sprite, cols-dx, rows-dy, cv2.INTER_NEAREST)
        # Discrete highlights travel with the folds, leaving the silhouette stable at the attachment.
        shade = np.rint(2.8*np.sin(phase + cols*.073 + rows*.029)*weight).astype(np.int16)
        frame[:,:,:3] = np.clip(frame[:,:,:3].astype(np.int16)+shade[:,:,None],0,255)
        frame[frame[:,:,3]==0] = 0
        frames.append(frame)
    return frames


def motion_part(name, art, mask, box, count, period, **motion):
    sprite = cut(art, mask, box)
    atlas(name, flexible(sprite,count,**motion), box[:2], period=period, delay=.22)


def morph_keys(keys, count, contours=False):
    # Both flows reference adjacent key drawings. Crossfade in premultiplied space.
    n = len(keys)
    gray = [cv2.cvtColor(k, cv2.COLOR_RGBA2GRAY) * (k[:,:,3] > 30) for k in keys]
    if contours:
        gray = [cv2.addWeighted(luma,.61,key[:,:,3],.39,0) for luma,key in zip(gray,keys)]
    y,x = np.mgrid[:keys[0].shape[0],:keys[0].shape[1]].astype(np.float32)
    flow = {}
    for a in range(n-1):
        for i,j in [(a,a+1),(a+1,a)]:
            flow[i,j] = cv2.calcOpticalFlowFarneback(gray[i],gray[j],None,.5,3,25,4,7,1.5,0)
    frames = []
    for i in range(count):
        location = (n-1)*(.5-.5*math.cos(2*math.pi*i/count))
        a = min(n-2, int(location))
        t = location-a
        ab,ba = flow[a,a+1], flow[a+1,a]
        one = cv2.remap(keys[a],x-t*ab[:,:,0],y-t*ab[:,:,1],cv2.INTER_NEAREST).astype(np.float32)
        two = cv2.remap(keys[a+1],x-(1-t)*ba[:,:,0],y-(1-t)*ba[:,:,1],cv2.INTER_NEAREST).astype(np.float32)
        alpha = (1-t)*one[:,:,3]+t*two[:,:,3]
        rgb = ((1-t)*one[:,:,:3]*one[:,:,3,None]+t*two[:,:,:3]*two[:,:,3,None])/np.maximum(alpha[:,:,None],1)
        if contours:
            # Let the illuminated edges develop inside the morph instead of multiplying a static tint.
            roll = (.5+.5*np.sin(x*.024-y*.041-i*math.tau/count))*np.clip((x/x.shape[1])**.7,.15,1)
            rgb = np.clip(rgb + roll[:,:,None]*np.array([13,1,3]),0,255)
        frames.append(rgba(rgb.astype(np.uint8),alpha))
    return frames


def weather():
    im = Image.open(SOURCES/'clima.png').convert('RGBA')
    width,height = im.size
    cloud_sheet = Image.open(REFINEMENT/'nuvens_chaves.png').convert('RGBA')
    for row,(name,pos,size,period,drift,strength,count) in enumerate([
        ('veu_distante',(319,237),(480,122),36,(23,3),.46,36),
        ('frente_intermediaria',(581,343),(500,144),28,(43,5),.36,40),
        ('banco_proximo',(68,320),(420,145),22,(66,7),.31,44),
    ]):
        y0,y1 = [(116,314),(347,554),(556,837)][row]
        keys=[]
        for col in range(6):
            x0,x1=(col*cloud_sheet.width)//6,((col+1)*cloud_sheet.width)//6
            tile = cloud_sheet.crop((x0,y0,x1,y1))
            key = np.array(tile.resize(size,Image.Resampling.NEAREST))
            edge = np.minimum(np.arange(size[0]),np.arange(size[0])[::-1])/15
            key[:,:,3] = np.where(key[:,:,3]>24,key[:,:,3]*np.clip(edge,0,1),0)
            keys.append(key)
        atlas('background/clouds_refinadas/'+name,morph_keys(keys,count,contours=True),pos,
              period=period,drift=drift,alpha=strength,delay=.5,kind='h')
    specs = [
        ('atmosphere/fog/distante',2,(693,580),(470,115),24,(54,8),.20),
        ('atmosphere/fog/intermediaria',2,(493,702),(480,138),20,(72,12),.25),
        ('atmosphere/fog/proxima',2,(945,757),(530,165),16,(91,11),.26),
    ]
    for name,row,pos,size,period,drift,strength in specs:
        keys = []
        for col in range(6):
            tile = im.crop((round(col*width/6),round(row*height/3),round((col+1)*width/6),round((row+1)*height/3)))
            key = np.array(tile.resize(size,Image.Resampling.NEAREST))
            # Suppress isolated keying specks from the generated sheet.
            key[:,:,3] = np.where(key[:,:,3] > 15,key[:,:,3],0)
            if name.startswith('atmosphere'):
                key[:,:,:3] = (key[:,:,:3] * np.array([.93,.55,.72])).astype(np.uint8)
                if name.endswith(('distante','proxima')):
                    key = key[:,::-1].copy()
            keys.append(key)
        atlas(name,morph_keys(keys,20),pos,period=period,drift=drift,alpha=strength,delay=.55)


def lights(stars):
    count, cycles, width = (8,10,25) if stars else (36,8,19)
    colors = [(255,176,107),(255,91,80),(228,129,170),(255,209,142)] if stars else [
        (244,87,62),(153,119,137),(191,150,158),(255,179,90),(213,36,66),(171,78,58)]
    frames = []
    for shape in range(count):
        for i in range(cycles):
            image = Image.new('RGBA',(width,width))
            draw = ImageDraw.Draw(image)
            peak = math.sin(math.pi*(i+.10)/cycles)**1.6
            color = colors[(shape if stars else shape//6)%len(colors)]
            c = width//2
            offset = 0 if stars else round(math.sin(i/cycles*math.tau+shape)*2)
            size = 1 + round(peak*(shape%4+1))
            for j in range(-size,size+1):
                opacity = round(175*peak*(1-abs(j)/(size+1)))
                draw.point((c+offset+j,c),fill=color+(opacity,))
                if stars or shape//6 in (0,3,4):
                    draw.point((c+offset,c+j),fill=color+(opacity,))
                if not stars and shape//6 == 5:
                    draw.point((c+offset+j,c+j//2),fill=color+(opacity,))
            draw.point((c+offset,c),fill=color+(round(245*peak),))
            frames.append(np.array(image))
    atlas('background/stars/brilho' if stars else 'atmosphere/particles/particulas',
          frames,kind='s' if stars else 'p',alpha=.72 if stars else .58,delay=.65)


def symbols():
    size = 164
    c = size//2
    ring = Image.new('RGBA',(size,size))
    d = ImageDraw.Draw(ring)
    d.ellipse((11,11,size-12,size-12),outline=(250,147,79,200),width=1)
    atlas('magic_circle/outer_ring', [np.array(ring)],(1290,178),period=52,kind='r',delay=.65)
    ring = Image.new('RGBA',(size,size))
    d = ImageDraw.Draw(ring)
    for i in range(24):
        angle = i*math.tau/24
        radius = 59 if i%3==0 else 65
        d.line((c+radius*math.cos(angle),c+radius*math.sin(angle),
                c+78*math.cos(angle),c+78*math.sin(angle)),fill=(255,168,94,195),width=1)
    atlas('magic_circle/radial_marks', [np.array(ring)],(1290,178),period=52,kind='r',delay=.65)
    ring = Image.new('RGBA',(size,size))
    d = ImageDraw.Draw(ring)
    d.ellipse((23,23,size-24,size-24),outline=(247,136,85,62),width=1)
    atlas('magic_circle/inner_ring',[np.array(ring)],(1290,178),period=46,kind='a',delay=.65)
    ring = Image.new('RGBA',(size,size))
    d = ImageDraw.Draw(ring)
    for i in range(12):
        a=i*math.tau/12
        x,y = round(c+57*math.cos(a)),round(c+57*math.sin(a))
        d.line([(x-2,y),(x,y-3),(x+2,y),(x,y+3),(x-2,y)],fill=(251,159,96,181))
    atlas('magic_circle/arcane_symbols',[np.array(ring)],(1290,178),period=46,kind='a',delay=.65)
    frames = []
    for frame in range(16):
        dots = Image.new('RGBA',(size,size))
        d = ImageDraw.Draw(dots)
        for i in range(6):
            angle = i*math.tau/6 + .09*math.sin(frame*math.tau/16+i)
            x,y = round(c+74*math.cos(angle)),round(c+74*math.sin(angle))
            level=round(85+75*math.sin(frame*math.tau/16+i*1.9)**2)
            d.point((x,y),fill=(250,167,90,level))
        frames.append(np.array(dots))
    atlas('magic_circle/points',frames,(1290,178),period=5.7,delay=.61)
    for lit,name,color,extent in [(True,'characters/necromancer/aura',(249,34,59),19),
                                  (False,'magic_circle/glow',(253,100,64),11)]:
        rows,cols = np.mgrid[:size,:size]
        dist=np.hypot((rows-c) * (.71 if lit else 1),cols-c)
        phi=np.arctan2(rows-c,cols-c)
        frames=[]
        for i in range(24):
            phase=math.tau*i/24
            ridge=51 if lit else 70
            aura=np.exp(-((dist-ridge-4*np.sin(phi*5+phase))/extent)**2)
            alpha=aura*(.4+.6*np.sin(phi*4-phase)**2)*(34 if lit else 22)
            rgb=np.broadcast_to(np.array(color,np.uint8),(size,size,3))
            frames.append(rgba(rgb,alpha))
        atlas(name,frames,(1290,246) if lit else (1290,178),period=7.8 if lit else 10,delay=.65)


def buttons():
    image = Image.open(ROOT/'assets/menu_principal_botoes.png').convert('RGBA')
    rows = [(48,186),(243,166),(416,165),(590,167),(764,166)]
    names = ('iniciar','continuar','configuracoes','creditos','sair')
    pulse_frames = []
    for name,(y,h) in zip(names,rows):
        source = image.crop((409,y,1129,y+h)).resize((360,93),Image.Resampling.NEAREST)
        src = np.array(source)
        v,u = np.mgrid[:93,:360]
        edge=np.clip(np.minimum.reduce([u/12,(359-u)/12,v/9,(92-v)/9]),0,1)
        strength=np.clip((np.max(src[:,:,:3],axis=2).astype(float)-28)/135,0,1)
        src[:,:,3] = (src[:,:,3]*edge*(.40+.60*strength)).astype(np.uint8)
        ornament = (v<31)|(v>60)|(u<64)|(u>296)
        for state,opacity,tint in [('normal',.88,None),('hover',1,(255,165,101)),('selected',1,(255,93,71)),
                                   ('pressed',1,(255,215,147)),('disabled',.50,None)]:
            variant = src.copy()
            if tint:
                bright = strength[:,:,None]
                variant[:,:,:3] = np.where(ornament[:,:,None], .4*variant[:,:,:3]+bright*np.array(tint)*.6,
                                          np.minimum(255,variant[:,:,:3]*1.07)).astype(np.uint8)
            variant[:,:,3] = (variant[:,:,3]*opacity).astype(np.uint8)
            path=ASSETS/'buttons'/state/f'{name}.png'
            path.parent.mkdir(parents=True,exist_ok=True)
            Image.fromarray(variant).save(path)
        for i in range(12):
            beat=(.5-.5*math.cos(i*math.tau/12))
            sweep=np.exp(-((u-i*35+20)/42)**2)
            color=np.where(ornament[:,:,None],np.array([255,57,63]),np.array([255,228,166]))
            frame=rgba(np.broadcast_to(color,src[:,:,:3].shape).astype(np.uint8),
                       src[:,:,3]*strength*np.where(ornament,.19+beat*.49,.07+sweep*.24))
            pulse_frames.append(frame)
            if EXPORT:
                path=EXPORT/'buttons'/name/f'brilho_{i+1:04}.png'
                path.parent.mkdir(parents=True,exist_ok=True)
                Image.fromarray(frame).save(path)
    path=ASSETS/'buttons'/'brilho.png'
    sheet=Image.new('RGBA',(6*360,10*93))
    for i, frame in enumerate(pulse_frames):
        sheet.paste(Image.fromarray(frame),(i%6*360,i//6*93))
    sheet.save(path)


def main():
    global EXPORT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frames', type=Path, help='optional lossless export of individual atlas frames')
    EXPORT = parser.parse_args().frames
    art = np.array(Image.open(ROOT/'assets/menu_principal_arte.png').convert('RGB'))
    red,green,blue = np.moveaxis(art.astype(np.int16),-1,0)
    logo_window = (X>396)&(X<1202)&(Y>16)&(Y<282)
    letter_window = (Y>=102)&(Y<220) | ((Y>=218)&(Y<237)&(X<474))
    red_title = ((red>105)&(red-green>28)&logo_window).astype(np.uint8)*255
    gold_title = ((red>127)&(green>139)&(green>blue+18)&logo_window).astype(np.uint8)*255
    title = red_title | np.where(letter_window,gold_title,0).astype(np.uint8)
    title_letters = np.where(letter_window,title,0).astype(np.uint8)
    title_symbols = np.where(~letter_window,title,0).astype(np.uint8)
    # Reference character contours, traced at 2x so the original poses and costume pixels stay intact.
    hero_origin=(260,625)
    cape = polygon([(34,130),(61,105),(105,83),(142,70),(166,69),(191,77),(227,96),(261,108),(304,110),(339,104),(377,106),
                    (405,120),(410,150),(388,164),(371,192),(345,227),(328,263),(306,283),(281,306),(264,328),(243,323),(221,330),
                    (229,308),(193,315),(174,318),(151,333),(118,345),(136,317),(100,341),(109,317),(98,304),(106,283),(70,287),
                    (25,297),(58,267),(32,271),(57,248),(95,225),(122,217),(181,197),(233,178),(276,150),(239,154),(219,155),
                    (200,144),(188,141),(171,124),(137,111),(101,110),(84,119),(68,113),(44,121)],hero_origin,2)
    hair = polygon([(316,72),(321,56),(326,51),(335,50),(332,41),(341,49),(357,44),(366,39),(381,43),(388,50),
                    (399,48),(396,57),(407,67),(399,72),(402,87),(393,95),(382,107),(361,108),(335,100),(326,89)],hero_origin,2)
    hero = polygon([(332,97),(383,91),(402,122),(410,155),(403,185),(412,199),(410,219),(422,244),(441,218),(456,211),
                    (457,225),(434,253),(427,279),(411,284),(415,339),(405,363),(404,391),(398,409),(419,428),(415,441),
                    (369,442),(363,406),(361,376),(337,373),(312,398),(304,410),(311,437),(303,454),(266,451),
                    (269,411),(282,387),(278,368),(285,336),(290,287),(303,255),(319,223),(332,181),(325,149)],hero_origin,2)
    hems= polygon([(295,263),(391,238),(412,274),(415,339),(404,369),(364,375),(333,357),(315,381),(279,379),(284,337)],hero_origin,2) & hero
    nec_origin=(1280,172)
    nec = polygon([(147,163),(159,144),(178,139),(192,145),(205,166),(215,198),(232,232),(246,253),(262,277),
                   (269,295),(301,287),(298,333),(299,408),(294,435),(269,435),(265,426),(243,437),(222,443),
                   (181,441),(148,453),(109,452),(76,456),(46,449),(69,434),(85,414),(73,378),(70,315),(92,302),
                   (113,274),(121,250),(133,225),(141,194)],nec_origin,2)
    robe = np.where(Y>=283,nec,0).astype(np.uint8)
    sigil_distance = np.hypot(X-1372,Y-260)
    sigil=((sigil_distance>56)&(sigil_distance<82)&(green>76)&(green>blue+37)&(Y<316)).astype(np.uint8)*255
    # Leave the ordinary stars in their own layer.
    star_mask = ((red>82)&(green>45)&(((abs(X-448)<14)&(abs(Y-44)<25))|
                   ((abs(X-923)<15)&(abs(Y-291)<18))|((abs(X-709)<10)&(abs(Y-297)<13)))).astype(np.uint8)*255
    ground = polygon([(0,695),(46,726),(133,780),(211,784),(242,805),(255,836),(323,845),(535,836),(547,858),
                      (602,864),(692,881),(778,903),(878,910),(1030,934),(1081,940),(0,940)])
    vines = polygon([(104,0),(334,0),(330,75),(276,92),(247,171),(162,183),(131,185),(117,123)])
    grass = polygon([(498,857),(621,846),(699,861),(743,862),(767,877),(822,891),(899,903),(956,868),
                     (1000,907),(1020,939),(581,935)])
    grass = np.where((red<44)&(green<32),grass & ~ground,0).astype(np.uint8)
    attached_vines=vines & np.where((red<66)&(green<38),255,0).astype(np.uint8)
    vine_mask=np.where((X>132)&(X<279)&(Y>75)&(red<42)&(green<28),attached_vines,0).astype(np.uint8)
    hidden=(grow(title,8)|grow(hero|cape|hair,4)|grow(nec,3)|grow(sigil,3)|
            grow(star_mask,4)|grow(grass,1)|grow(vine_mask,1))
    hidden[ground>0]=0
    # Use local inpainting at visible edges, with a texture transfer pass for the broader patches.
    clean=cv2.inpaint(art,hidden,7,cv2.INPAINT_TELEA)
    sky_input=SOURCES/'ceu.png'
    if sky_input.exists():
        sky=np.array(Image.open(sky_input).convert('RGB').resize((W,H),Image.Resampling.LANCZOS))
        detail=sky.astype(np.float32)-cv2.GaussianBlur(sky,(0,0),5).astype(np.float32)
        clean=np.clip(clean.astype(np.float32)+detail*(hidden[:,:,None]/255)*.55,0,255).astype(np.uint8)
    else:
        sky = cv2.GaussianBlur(clean,(0,0),14)

    moon_box=(1207,31,1585,403)
    moon_disc=((X-1395.5)**2/181.5**2+(Y-216.5)**2/180.5**2<1)
    # Colour and depth mask keep the original arches in front of the disc.
    moon_mask=np.where(moon_disc & (clean[:,:,0]>97) & (clean[:,:,0].astype(int)-clean[:,:,1]>53),255,0).astype(np.uint8)
    moon_mask[nec>0] = np.where(moon_disc[nec>0],255,0)
    xx0,yy0,xx1,yy1 = moon_box
    lunar = Image.open(SOURCES/'lua.png').convert('RGBA')
    lunar = lunar.crop(lunar.getbbox()).resize((xx1-xx0,yy1-yy0),Image.Resampling.NEAREST)
    texture=np.array(lunar)[:,:,:3].astype(np.int16)
    painted=cut(clean,moon_mask,moon_box)
    moon_frames=[]
    for i in range(20):
        phase=.5-.5*math.cos(i*math.tau/20)
        variation=painted.copy()
        base=painted[:,:,:3].astype(np.float32)
        variation[:,:,:3]=np.clip(base*(.97+.15*phase)+np.clip(texture-base,-75,75)*phase*.16,0,255)
        moon_frames.append(variation)

    # Ownership masks partition the fixed reference geometry; all plate pieces keep their exact positions.
    stairs=polygon([(1350,375),(1440,397),(1378,471),(1320,536),(1235,624),(1162,705),
                    (1056,781),(881,827),(759,816),(847,722),(995,646),(1143,572),(1228,420)])
    pillars=polygon([(1439,128),(1533,115),(1584,159),(1671,109),(1671,432),(1440,427)])
    far_ruins=polygon([(0,351),(228,474),(302,553),(392,704),(467,822),(0,843)])
    front=ground | polygon([(0,0),(235,0),(152,86),(141,169),(67,287),(91,438),(66,580),(67,741),(0,741)])
    front |= attached_vines
    filtered_cloud_zone=((Y>240)&(Y<680)&(X>230)&(X<1210))
    opacity = (np.sin(np.clip((X-230)/980,0,1)*np.pi)*np.sin(np.clip((Y-240)/440,0,1)*np.pi))*.08
    alpha = np.where(filtered_cloud_zone,opacity,0)
    clean=np.clip(clean*(1-alpha[:,:,None])+sky*alpha[:,:,None],0,255).astype(np.uint8)

    base_alpha = 255-np.maximum.reduce([moon_mask,front,stairs,pillars,far_ruins])
    # Whole-area underpainting is useful to downstream scene edits, while the fixed sky remains faithful.
    atlas('background/sky/ceu_base',[rgba(sky,np.full((H,W),255))])
    atlas('background/sky/ceu_pintado',[rgba(clean,base_alpha)])
    lights(True)
    atlas('background/stars/estrelas_da_referencia',[cut(art,grow(star_mask,3),(424,19,946,311))],(424,19),delay=.3)
    r=np.hypot((X-1395)/210,(Y-216)/210)
    moon_glow=np.maximum(0,1-np.abs(r-.91)*6)**2*22
    moon_glow=np.where((front|stairs|pillars)>0,0,moon_glow)
    atlas('background/moon/aura',[cut(np.broadcast_to([224,34,62],art.shape).astype(np.uint8),moon_glow,(1158,0,1645,470))],(1158,0),kind='l',delay=.35)
    atlas('background/moon/lua_carmesim',moon_frames,moon_box[:2],period=10,delay=.22)
    for name,mask,box in [('ruins/distantes',far_ruins,(0,349,470,844)),('stairs/escadaria',stairs,(752,374,1445,831)),
                          ('ruins/pilares',pillars,(1438,107,W,435))]:
        atlas('background/'+name,[cut(clean,mask & ~moon_mask,box)],box[:2])
    weather()
    # The rings must be drawn before the cloaked figure.
    symbols()
    motion_part('characters/necromancer/body',art,nec&~robe,(1300,235,1423,295),16,6,
                anchor=(63,56),sway=.9,lift=1.3,root='bottom')
    motion_part('characters/necromancer/robe',art,robe,(1297,279,1437,414),32,7.8,
                anchor=(67,10),sway=4.3,lift=1.6,root='top')
    for name,box,mask in [('degraus',(755,380,1441,823),stairs),('ruinas',(1438,104,W,436),pillars),
                          ('atmosfera',(541,473,1186,739),np.full((H,W),255,np.uint8))]:
        relevant=((red>100)&(red>green+42)).astype(np.uint8)*mask
        glow=rgba(np.full(art.shape,[247,59,68],np.uint8), relevant*.12)
        atlas('atmosphere/lighting/'+name,[glow[box[1]:box[3],box[0]:box[2]]],box[:2],kind='l',delay=.5)
    motion_part('characters/protagonist/body',art,hero&~hems,(385,660,497,856),16,5.2,
                anchor=(63,165),sway=.75,lift=1.8,root='bottom')
    motion_part('characters/protagonist/clothes',art,hems,(379,737,480,824),16,4.4,
                anchor=(53,11),sway=1.6,lift=1.4,root='top')
    cloth_atlases(art,cape,REFINEMENT/'capa_chaves.png',atlas)
    motion_part('characters/protagonist/hair',art,hair,(407,638,473,686),16,4.4,
                anchor=(42,39),sway=2.6,lift=2.5,root='bottom')
    atlas('foreground/ruins/primeiro_plano',[rgba(clean,front)],delay=0)
    plant_atlases(art,vine_mask,REFINEMENT/'vegetacao_chaves.png',atlas)
    lights(False)

    title_box=(391,19,1205,282)
    letters=cut(art,grow(title_letters,3),title_box)
    atlas('foreground/title/lettering',[letters],title_box[:2],kind='t',delay=1.34)
    center = (X>793)&(X<840)
    title_parts = [('ornamentos_superiores',(Y<112)&~center,0,title_box),
                   ('ornamentos_inferiores',(Y>=112)&~center,1,title_box),
                   ('espadas',center,0,(788,19,845,282))]
    for name,region,phase,box in title_parts:
        mask=np.where(region,title_symbols,0).astype(np.uint8)
        sprite=cut(art,grow(mask,2),box)
        frames=[]
        for i in range(16):
            frame=sprite.copy()
            factor=.89+.11*math.cos(i*math.tau/16+phase)
            frame[:,:,:3]=np.clip(sprite[:,:,:3]*factor,0,255)
            frames.append(frame)
        atlas('foreground/title/'+name,frames,box[:2],period=5.6,kind='t',delay=1.68)
    title_rows,title_cols=np.mgrid[:letters.shape[0],:letters.shape[1]]
    shine=[]
    for i in range(16):
        ridge=np.exp(-((title_cols+title_rows*.5-i*62)/29)**2)
        shine.append(rgba(np.full(letters.shape[:2]+(3,),[255,221,162],np.uint8),letters[:,:,3]*ridge*.45))
    atlas('foreground/title/glow',shine,title_box[:2],kind='g',delay=2.0)
    metadata=ASSETS/'metadata'
    metadata.mkdir(parents=True,exist_ok=True)
    (metadata/'layers.json').write_text(json.dumps({'reference':[W,H],'layers':LAYERS},indent=2),encoding='utf8')
    keys='path kind x y width height columns frames period driftX driftY opacity delay start duration'.split()
    (metadata/'layers.txt').write_text('# '+' '.join(keys)+'\n'+'\n'.join(' '.join(str(row[k]) for k in keys) for row in LAYERS)+'\n',encoding='ascii')
    buttons()
    write_wind(metadata/'vento.txt')
    print(f'{len(LAYERS)} layers, {sum(layer["frames"] for layer in LAYERS if layer["frames"]>1)} animation frames')


if __name__ == '__main__':
    main()
