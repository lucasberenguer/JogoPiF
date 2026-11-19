"""Import user artwork/music and build quiet, reproducible cinematic sound cues."""
import argparse
from array import array
import math
from pathlib import Path
import random
import shutil
import subprocess
import wave

IMAGES = [
    '59866edb-b707-413f-a18e-4b156c736efa', '55c9a659-1142-4640-bd67-0052c0d32cd3',
    '5e24b75f-f329-45da-a3e4-06a82d53c542', '01b4d179-d205-449a-b83d-391c4f2fff29',
    '38be19c5-e4db-403e-8ea6-c0d935fed457', 'bcbfb42e-3777-4608-8ec7-ce785935e36e',
    '927d2fe2-d5f2-4808-91fe-d9bc29e2d61a', 'baaaa994-cf97-4c26-9c00-7f348096c8f7',
    'd84c5b8c-cc48-4424-95c7-4f5b06c6c08b', '3a041226-f530-4f15-ac25-ae0b8e787cf9',
    '4aae9137-6e8c-4a34-a80b-335552199fa6',
]


def sound(path, kind, duration):
    rate = 22050
    rng = random.Random(71)
    samples = array('h')
    breeze = 0
    for i in range(int(duration*rate)):
        t = i/rate
        envelope = min(1, t/.12)*min(1, (duration-t)/.5)
        if kind == 'vento':
            breeze = breeze*.985 + rng.uniform(-1, 1)*.015
            value = breeze*2*(.7+.3*math.sin(t*1.7))
        elif kind == 'sino':
            value = sum(math.sin(2*math.pi*220*f*t)*math.exp(-t*d)/a
                        for f, d, a in [(1, .65, 2), (2.76, 1.1, 4), (5.4, 1.8, 9)])
        elif kind == 'particulas':
            value = 0
            for start, freq in [(.1, 1046.5), (.45, 1318.5), (.85, 1568), (1.2, 2093)]:
                age = t-start
                if age >= 0:
                    value += .18*math.sin(2*math.pi*freq*age)*min(1, age/.025)*math.exp(-age*4)
        else:
            freq = 261.63 if kind == 'runas' else 196
            value = sum(math.sin(2*math.pi*freq*f*t)*math.exp(-t*.75)/6
                        for f in (1, 1.5, 2, 3))
            value *= min(1, t/.35)
        samples.append(int(max(-1, min(1, value*envelope)) * 22000))
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, rate, len(samples), 'NONE', 'not compressed'))
        out.writeframes(samples.tobytes())


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--images', type=Path, required=True)
    parser.add_argument('--music', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    target = root/'assets/cutscenes/guardian_aftermath'
    for folder in ('images', 'audio', 'data'):
        (target/folder).mkdir(parents=True, exist_ok=True)
    for i, name in enumerate(IMAGES, 1):
        shutil.copy2(args.images/f'codex-clipboard-{name}.png', target/f'images/quadro_{i:02}.png')
    duration = float(subprocess.check_output(['ffprobe', '-v', 'error', '-show_entries',
        'format=duration', '-of', 'csv=p=0', str(args.music)], text=True).strip())
    end = duration-2
    filters = (f'[0:a]asplit=3[m][t][h];[m]atrim=start=2:end={end},asetpts=PTS-STARTPTS[body];'
               f'[t]atrim=start={end},asetpts=PTS-STARTPTS[tail];'
               '[h]atrim=end=2,asetpts=PTS-STARTPTS[head];'
               '[tail][head]acrossfade=d=2:c1=tri:c2=tri[seam];'
               '[body][seam]concat=n=2:v=0:a=1,volume=0.9[out]')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(args.music), '-filter_complex', filters,
                    '-map', '[out]', '-c:a', 'libvorbis', '-q:a', '5', str(target/'audio/trilha.ogg')], check=True)
    shutil.copy2(root/'assets/cutscenes/boss_clareira/audio/raizes.wav', target/'audio/raizes.wav')
    for name, seconds in [('vento', 5), ('energia', 3), ('particulas', 2.5), ('runas', 3), ('sino', 5)]:
        sound(target/f'audio/{name}.wav', name, seconds)
    print(f'11 original images; music loop {duration-2:.2f}s; 6 sound cues.')


if __name__ == '__main__':
    main()
