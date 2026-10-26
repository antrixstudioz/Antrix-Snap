"""Prepare owner-supplied reference WAVs into a bounded, embedded PCM bank.
Usage: python tools/prepare_samples.py INPUT_DIRECTORY
Requires ffmpeg and numpy. Raw filenames are retained only in the local audit map.
"""
import sys, pathlib, subprocess, numpy as np, struct, json, hashlib
root=pathlib.Path(__file__).resolve().parents[1]
sources=sorted(pathlib.Path(sys.argv[1]).rglob('*.wav'))
assert sources and len(sources)<=128
bank=bytearray(b'SNAPBNK1'+struct.pack('<II',len(sources),48000));manifest=[];audit=[]
for i,p in enumerate(sources):
 result=subprocess.run(['ffmpeg','-v','error','-i',str(p),'-f','f32le','-acodec','pcm_f32le','-ar','48000','-ac','2','pipe:1'],capture_output=True,check=True)
 x=np.frombuffer(result.stdout,dtype='<f4').reshape(-1,2).copy()
 assert len(x)>0 and np.isfinite(x).all(),p.name
 peak=np.max(abs(x));assert peak>1e-5
 env=np.max(abs(x),axis=1)
 # Align first meaningful attack, keep 0.4ms lead-in and a natural release.
 onset=int(np.flatnonzero(env>peak*.04)[0]);start=max(0,onset-19)
 end=min(len(x),int(np.flatnonzero(env>peak*.001)[-1])+480,start+57600)
 x=x[start:end];x*=.72/np.max(abs(x))
 fade=min(240,len(x)//4);x[-fade:]*=np.linspace(1,0,fade)[:,None]
 x[:8]*=np.linspace(0,1,8)[:,None]
 pcm=np.round(np.clip(x,-1,1)*32767).astype('<i2')
 name=f'Snap_{i+1:03d}'
 bank+=struct.pack('<I',len(pcm))+pcm.tobytes()
 manifest.append({'name':name,'frames':len(pcm),'duration_seconds':round(len(pcm)/48000,4)})
 audit.append({'name':name,'original_name':p.name,'source_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'decoder_notice':result.stderr.decode(errors='replace')})
(root/'Assets'/'snaps.bank').write_bytes(bank)
(root/'Assets'/'samples.json').write_text(json.dumps({'sample_rate':48000,'channels':2,'format':'signed PCM16 little endian','samples':manifest},indent=2)+'\n')
pathlib.Path('/workspace/snap-reference/sample-provenance.json').write_text(json.dumps(audit,indent=2)+'\n')
print(json.dumps({'count':len(sources),'bank_bytes':len(bank),'longest_seconds':max(x['duration_seconds'] for x in manifest),'decoder_notices':[a for a in audit if a['decoder_notice']]},indent=2))
