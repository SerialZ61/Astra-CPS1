import sys, io, bisect, collections, hashlib, json, struct
from pathlib import Path
sys.path.insert(0,'tests')
from arm_smoke import *
from compare_releases import cabinet_player
from unicorn import UC_HOOK_BLOCK
from elftools.elf.elffile import ELFFile
elf,game,tag=sys.argv[1:4];core=ArmCore(Path(elf),Path('Roms')/(game+'.zip'));m=core.mu
assert core.call('cps1_open',STR)==0,core.cstr(core.call('cps1_error'))
s=core.symbols;idx=core.call('cps1_current_game');id=core.cstr(core.call('cps1_game_id',idx))
extra=core.call('_Z15astraExtraIndexi',idx)!=0xffffffff
print(game,id,'extra',extra,flush=True)
tab=ELFFile(io.BytesIO(Path(elf).read_bytes())).get_section_by_name('.symtab')
sy=sorted((v['st_value']&~1,v.name) for v in tab.iter_symbols() if v['st_info']['type']=='STT_FUNC' and v['st_value'])
starts=[x for x,n in sy]
addr=next(v['st_value'] for v in tab.iter_symbols() if v.name=='CpsRamFF' and (v['st_info']['bind']=='STB_LOCAL')==extra)
ram=int.from_bytes(m.mem_read(addr,4),'little');assert ram
sig=[hashlib.sha256() for _ in range(3)]
probes={k+i for k in [1110,1500,1850,2230,2680,3010] for i in [0,1]}
work=[0,0]
for f in range(3200):
 draw=f%((1,2,4,3)[f//70%4])==0
 if f in probes:
  draw=f%2==0; bins=collections.Counter(); m.ctl_flush_tb()
  def count(m,a,size,_): bins[a]+=size//4
  hook=m.hook_add(UC_HOOK_BLOCK,count)
 n=core.call('cps1_frame',cabinet_player(f),BMP if draw else 0,PCM)
 assert 0<n<=370
 if f in probes:
  m.hook_del(hook); m.ctl_flush_tb(); val=sum(bins.values()); work[int(draw)]+=val
  if draw and f in (1850,2680):
   counts=collections.Counter()
   for a,v in bins.items():counts[sy[bisect.bisect_right(starts,a)-1][1]]+=v
   print(f,counts.most_common(16),flush=True)
 if draw:sig[0].update(m.mem_read(BMP,384*224*2))
 sig[1].update(m.mem_read(PCM,n*4));sig[2].update(m.mem_read(ram,65536))
 if f==2230:rgb565(m.mem_read(BMP,384*224*2)).save('out/v06/'+tag+'-'+game+'.png')
res=dict(id=id,extra=extra,work=work,hashes=[h.hexdigest() for h in sig])
print(res);Path('out/v06/'+tag+'-'+game+'.json').write_text(json.dumps(res))
