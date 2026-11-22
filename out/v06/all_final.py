import sys, re, struct
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor
sys.path.insert(0,'tests')
from arm_smoke import *
from compare_releases import cabinet_player
from elftools.elf.elffile import ELFFile
def run(game):
 rom=Path('Roms')/(game+'.zip')
 if not rom.is_file(): return game,'absent'
 cores=[]
 for exe in ('v05.elf','card.elf'):
  elf=Path('out/v06')/exe
  c=ArmCore(elf,rom);assert not c.call('cps1_open',STR)
  private={s.name:s['st_value'] for s in ELFFile(io.BytesIO(elf.read_bytes())).get_section_by_name('.symtab').iter_symbols() if s['st_info']['bind']=='STB_LOCAL'}
  ram=int.from_bytes(c.mu.mem_read(private['CpsRamFF'],4),'little'); cpu=private['m68ki_cpu']
  cores.append((c,ram,cpu))
 for f in range(1460):
  draw=f%17==0 or f%181<5
  snap=[]
  for c,ram,cpu in cores:
   n=c.call('cps1_frame',cabinet_player(f),BMP if draw else 0,PCM)
   snap.append((c.mu.mem_read(BMP,W*H*2) if draw else b'',c.mu.mem_read(PCM,n*4),c.mu.mem_read(ram,65536),c.mu.mem_read(cpu,0x9c)))
  for name,x,y in zip(('rgb','audio','RAM','68k'),*snap):
   if x!=y:return game,(f,name)
 return game,'ok'
if __name__=='__main__':
 games=re.findall(r'^EXTRA\([^,]+, "([^"]+)"',Path('engine/compat/sets.inc').read_text(),re.M)
 with ProcessPoolExecutor(max_workers=7) as pool:
  rows=[]
  for row in pool.map(run,games):
   rows.append(row);print(*row,flush=True)
 assert all(status in ('ok','absent') for _,status in rows)
