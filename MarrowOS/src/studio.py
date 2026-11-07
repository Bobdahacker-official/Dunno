#!/usr/bin/python3
"""Persistent model projects, HTTP transfers, Python modules and chat inference."""
import argparse,collections,json,os,pathlib,random,re,shutil,subprocess,sys,tempfile,urllib.request
ROOT=pathlib.Path('/workspace')
def atomic(path,data):
 path=pathlib.Path(path);path.parent.mkdir(parents=True,exist_ok=True)
 fd,tmp=tempfile.mkstemp(dir=path.parent)
 try:
  with os.fdopen(fd,'w') as f: f.write(data);f.flush();os.fsync(f.fileno())
  os.replace(tmp,path)
 finally:
  if os.path.exists(tmp):os.unlink(tmp)
def config():return json.loads((ROOT/'project.json').read_text())
def template(kind):
 p={'name':'my-model','template':kind,'dataset':'/workspace/datasets/conversation.txt','model':'/workspace/models/my-model.chat.json','import_source':'','import_target':'/workspace/datasets/imported.txt','upload_url':'','api_url':'','api_model':'','api_key_env':'MARROW_API_KEY','api_key_file':'/workspace/config/api-key','temperature':0.8,'max_tokens':160,'module':'/workspace/modules/model.py'}
 if kind=='classifier':p.update(dataset='/workspace/datasets/spiral-train.csv',model='/workspace/models/custom.weights',epochs=600)
 if kind=='remote':p['api_url']='http://10.0.2.2:11434/v1/chat/completions'
 atomic(ROOT/'project.json',json.dumps(p,indent=2)+'\n')
 module=ROOT/'modules/model.py';module.parent.mkdir(exist_ok=True)
 if not module.exists():atomic(module,'import json\nfrom pathlib import Path\np = json.loads(Path("/workspace/project.json").read_text())\nprint("Project:", p["name"])\nprint("Dataset:", p["dataset"])\n')
 if kind=='python':atomic(module,"from runpy import run_path\napi = run_path('/usr/bin/marrow-studio')\napi['fit_local'](api['config']())\n")
 print('Created',kind,'project')
def transfer(src,dst):
 dst=pathlib.Path(dst);dst.parent.mkdir(parents=True,exist_ok=True)
 fd,tmp=tempfile.mkstemp(dir=dst.parent)
 try:
  with os.fdopen(fd,'wb') as out:
   if src.startswith(('https://','http://')):
    with urllib.request.urlopen(src,timeout=60) as r:shutil.copyfileobj(r,out)
   else:
    with open(src,'rb') as r:shutil.copyfileobj(r,out)
   out.flush();os.fsync(out.fileno())
  os.replace(tmp,dst)
 finally:
  if os.path.exists(tmp):os.unlink(tmp)
 print('Imported',dst)
def train():
 p=config()
 if p['template']=='python':subprocess.run(['python3',p['module']],check=True);return
 if p['template']=='classifier':subprocess.run(['marrow-train','train',p['dataset'],p['model'],str(p.get('epochs',600))],check=True);return
 if p['template']=='remote':
  atomic(p['model'],json.dumps({'type':'remote','url':p['api_url'],'model':p['api_model'],'key_env':p['api_key_env'],'key_file':p.get('api_key_file',''),'temperature':p['temperature'],'max_tokens':p['max_tokens']}));print('Saved remote model connection');return
 fit_local(p)
def fit_local(p):
 corpus=pathlib.Path(p['dataset']).read_text();pairs=[]
 for line in corpus.splitlines():
  if '\t' in line:a,b=line.split('\t',1);pairs.append([a,b])
 words=re.findall(r"\w+|[^\w\s]",corpus);table=collections.defaultdict(list)
 for a,b,c in zip(words,words[1:],words[2:]):table[a+'\x1f'+b].append(c)
 if not words:raise ValueError('Dataset is empty')
 atomic(p['model'],json.dumps({'type':'local','pairs':pairs,'transitions':dict(table),'words':words[:10000]}));print('Trained',len(words),'tokens;',len(pairs),'conversation examples;',len(table),'transition weights')
def reply(model,prompt):
 m=json.loads(pathlib.Path(model).read_text())
 if m['type']=='remote':
  history=ROOT/'runs/chat-history.json';messages=json.loads(history.read_text()) if history.exists() else []
  messages.append({'role':'user','content':prompt});headers={'Content-Type':'application/json'}
  key=os.getenv(m.get('key_env','MARROW_API_KEY'),'')
  if not key and m.get('key_file') and pathlib.Path(m['key_file']).exists():key=pathlib.Path(m['key_file']).read_text().strip()
  if key:headers['Authorization']='Bearer '+key
  req=urllib.request.Request(m['url'],data=json.dumps({'model':m['model'],'messages':messages[-20:],'temperature':m.get('temperature',.8),'max_tokens':m.get('max_tokens',160)}).encode(),headers=headers)
  with urllib.request.urlopen(req,timeout=90) as r:answer=json.load(r)['choices'][0]['message']['content']
  messages.append({'role':'assistant','content':answer});atomic(history,json.dumps(messages));return answer
 tokens=set(re.findall(r'\w+',prompt.lower()));best=None;score=0
 for a,b in m.get('pairs',[]):
  keys=set(re.findall(r'\w+',a.lower()));s=len(tokens&keys)/max(1,len(tokens|keys))
  if s>score:best=b;score=s
 if best and score>.15:return best
 table=m['transitions'];keys=list(table)
 if not keys:return ' '.join(m['words'][:60])
 choices=[k for k in keys if tokens & set(k.lower().split('\x1f'))];state=random.choice(choices or keys);out=state.split('\x1f')
 for _ in range(90):
  options=table.get('\x1f'.join(out[-2:]))
  if not options:break
  out.append(random.choice(options))
 return re.sub(r'\s+([.,!?;:])',r'\1',' '.join(out))
def main():
 a=sys.argv[1];p=config() if a not in ('template','chat') else None
 if a=='template':template(sys.argv[2])
 elif a=='train':train()
 elif a=='import':transfer(p['import_source'],p['import_target'])
 elif a=='upload':
  data=pathlib.Path(p['model']).read_bytes();req=urllib.request.Request(p['upload_url'],data=data,method='PUT',headers={'Content-Type':'application/octet-stream'})
  with urllib.request.urlopen(req,timeout=90) as r:print('Uploaded',len(data),'bytes; HTTP',r.status)
 elif a=='module':subprocess.run(['python3',p['module']],check=True)
 elif a=='chat':
  prompt=(ROOT/'runs/prompt.txt').read_text();answer=reply(sys.argv[2],prompt)
  with (ROOT/'runs/conversation.txt').open('a') as f:f.write('\nYou\n'+prompt+'\n\nModel\n'+answer+'\n')
  print(answer)
if __name__=='__main__':
 try:main()
 except Exception as e:print(type(e).__name__+': '+str(e),file=sys.stderr);sys.exit(1)
