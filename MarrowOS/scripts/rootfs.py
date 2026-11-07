#!/usr/bin/env python3
"""Assemble a user-owned root tree; no mount or root privileges required."""
import os,sys,shutil,subprocess,re
from pathlib import Path
project=Path(__file__).resolve().parents[1]
work=Path(os.environ.get('MARROW_WORK',str(project/'build'))).resolve()
tools=Path(os.environ.get('TOOLS_ROOT',str(work/'tools'))).resolve()
root=work/'rootfs'
root.mkdir(parents=True,exist_ok=True)
for p in ['bin','sbin','usr/bin','usr/sbin','usr/lib','lib','lib64','etc','proc','sys','dev','run','tmp','root','var/log','workspace/datasets','workspace/models','workspace/runs','workspace/src','workspace/config','boot']:(root/p).mkdir(parents=True,exist_ok=True)
os.chmod(root/'tmp',0o1777)
def put(src,dst):
 dst=root/str(dst).lstrip('/');dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst,follow_symlinks=True)
def binary(src,dst):
 put(src,dst)
 env=dict(os.environ,LD_LIBRARY_PATH=str(tools/'usr/lib/x86_64-linux-gnu'))
 p=subprocess.run(['ldd',str(src)],capture_output=True,text=True,env=env)
 for line in p.stdout.splitlines():
  if '=> not found' in line:raise RuntimeError(line)
  paths=re.findall(r'(/[^\s()]+)',line)
  for path in paths:
   if path.startswith(str(tools)):
    relative=Path(path).relative_to(tools);dest=Path('/')/relative
   elif path.startswith('/lib64/'):
    source=tools/'usr/lib64'/Path(path).name
    if not source.exists():source=tools/'usr/lib/x86_64-linux-gnu'/Path(path).name
    if source.exists():put(source,path);continue
    dest=Path(path)
   else:dest=Path(path)
   put(path,dest)
busybox=tools/'bin/busybox'
if not busybox.exists():busybox=tools/'usr/bin/busybox'
put(busybox,'bin/busybox')
for app in subprocess.check_output([str(busybox),'--list'],text=True).split():
 p=root/'bin'/app
 if app!='busybox' and not p.exists():p.symlink_to('busybox')
for name in ['marrow-train','marrow-desktop','marrow-wifi','marrow-cc']:put(work/name,'usr/bin/'+name)
put(work/'marrow-init','sbin/marrow-init')
for name in ['init','marrow-dhcp']:
 p=root/'sbin'/name
 if p.exists() or p.is_symlink():p.unlink()
 p.symlink_to('marrow-init')
for name in ['wpa_supplicant','wpa_cli','iw']:
 binary(tools/'usr/sbin'/name,'usr/sbin/'+name)
binary(tools/'usr/bin/wpa_passphrase','usr/bin/wpa_passphrase')
binary(tools/'usr/bin/tcc','usr/bin/tcc')
binary('/usr/bin/make','usr/bin/make')
for name in ['mke2fs','e2fsck','debugfs','resize2fs']:
 binary(tools/'usr/sbin'/name,'usr/sbin/'+name)
binary(tools/'usr/bin/x86_64-linux-gnu-as','usr/bin/as')
put(tools/'usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2','usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2')
for link in ['cc']:
 p=root/'usr/bin'/link
 if p.exists() or p.is_symlink():p.unlink()
 p.symlink_to('marrow-cc')
# Matching C development files for native C/ASM compilation.
shutil.copytree(tools/'usr/include',root/'usr/include',dirs_exist_ok=True)
shutil.copytree(tools/'usr/lib/x86_64-linux-gnu/tcc',root/'usr/lib/x86_64-linux-gnu/tcc',dirs_exist_ok=True)
for p in (tools/'usr/lib/x86_64-linux-gnu').iterdir():
 if p.suffix in ['.a','.o'] or (p.suffix=='.so' and p.name in ['libc.so','libm.so','libdl.so','libpthread.so','libutil.so','libresolv.so']):put(p,'usr/lib/x86_64-linux-gnu/'+p.name)
# libc linker scripts refer to /lib/x86_64-linux-gnu on some releases.
(root/'lib/x86_64-linux-gnu').mkdir(exist_ok=True)
for p in (root/'usr/lib/x86_64-linux-gnu').iterdir():
 if p.is_file() and ('.so' in p.name or p.suffix in ['.a','.o']):
  q=root/'lib/x86_64-linux-gnu'/p.name
  if not q.exists() and not q.is_symlink():q.symlink_to('/usr/lib/x86_64-linux-gnu/'+p.name)
for p in (tools/'usr/lib/firmware').glob('regulatory*'):put(p,'lib/firmware/'+p.name)
put(tools/'usr/lib/firmware/regulatory.db-debian','lib/firmware/regulatory.db')
put(tools/'usr/lib/firmware/regulatory.db.p7s-debian','lib/firmware/regulatory.db.p7s')
for p in project.joinpath('src').glob('*'):
 if p.is_file():put(p,'workspace/src/'+p.name)
put(project/'assets/font.h','workspace/assets/font.h')
for p in (work/'data').glob('*.csv'):put(p,'workspace/datasets/'+p.name)
put(work/'spiral.weights','workspace/models/spiral.weights')
put(work/'spiral.weights.metrics.csv','workspace/models/spiral.weights.metrics.csv')
put(work/'training.log','workspace/runs/first-training.log')
for p in project.joinpath('config').glob('*'):put(p,'workspace/config/'+p.name)
(root/'etc/os-release').write_text('NAME="MarrowOS"\nID=marrow\nVERSION="0.1"\nPRETTY_NAME="MarrowOS 0.1"\n')
(root/'etc/passwd').write_text('root:x:0:0:Marrow Developer:/root:/bin/sh\n')
(root/'etc/group').write_text('root:x:0:\n')
(root/'etc/shadow').write_text('root:*:20000:0:99999:7:::\n')
os.chmod(root/'etc/shadow',0o600)
(root/'etc/hosts').write_text('127.0.0.1 localhost marrow\n')
(root/'etc/resolv.conf').write_text('')
(root/'etc/fstab').write_text('/dev/vda / ext4 defaults 0 1\n')
(root/'etc/profile').write_text('export PATH=/usr/bin:/bin:/usr/sbin:/sbin\nexport HOME=/root\nexport TERM=linux\nexport PS1="marrow:\\w # "\ncd /workspace\n')
(root/'etc/motd').write_text('MarrowOS 0.1 / local AI development\nC + ASM / Linux 6.12 / persistent ext4\nF1-F6 switch desktop views. Ctrl+Alt+F2 opens the text console.\nmarrow-train: generate, train, eval, infer, info\n')
# The fallback script is also used by BusyBox udhcpc from the shell.
(root/'usr/share/udhcpc').mkdir(parents=True,exist_ok=True)
(root/'usr/share/udhcpc/default.script').write_text('#!/bin/sh\nexec /sbin/marrow-dhcp "$@"\n')
os.chmod(root/'usr/share/udhcpc/default.script',0o755)
(root/'workspace/src/hello.c').write_text('#include <stdio.h>\nint main(void){puts("Built inside MarrowOS.");return 0;}\n')
(root/'workspace/config/experiment.conf').write_text('dataset=/workspace/datasets/spiral-train.csv\ntest=/workspace/datasets/spiral-test.csv\nweights=/workspace/models/spiral.weights\narchitecture=2,24,2\nepochs=600\nlearning_rate=0.008\nbatch_size=32\nseed=42\n')
print(root)

# Python, HTTPS, Git and project tools.
binary(tools/'usr/bin/python3.13','usr/bin/python3.13')
p=root/'usr/bin/python3'
if p.exists() or p.is_symlink():p.unlink()
p.symlink_to('python3.13')
shutil.copytree(tools/'usr/lib/python3.13',root/'usr/lib/python3.13',dirs_exist_ok=True)
for p in (tools/'usr/lib/python3.13/lib-dynload').glob('*.so'):binary(p,'usr/lib/python3.13/lib-dynload/'+p.name)
for name in ['curl','git']:binary(tools/'usr/bin'/name,'usr/bin/'+name)
if (tools/'usr/lib/git-core').exists():
 shutil.copytree(tools/'usr/lib/git-core',root/'usr/lib/git-core',dirs_exist_ok=True)
 for p in (tools/'usr/lib/git-core').iterdir():
  if p.is_file() and p.read_bytes()[:4]==b'\x7fELF':binary(p,'usr/lib/git-core/'+p.name)
shutil.copytree(tools/'usr/share/ca-certificates',root/'usr/share/ca-certificates',dirs_exist_ok=True)
(root/'etc/ssl/certs').mkdir(parents=True,exist_ok=True)
with (root/'etc/ssl/certs/ca-certificates.crt').open('wb') as f:
 for p in (tools/'usr/share/ca-certificates').rglob('*.crt'):f.write(p.read_bytes()+b'\n')
put(project/'src/studio.py','usr/bin/marrow-studio')
(root/'usr/bin/marrow-studio').chmod(0o755)
(root/'workspace/modules').mkdir(exist_ok=True)
(root/'workspace/datasets/conversation.txt').write_text("hello\tHello. What would you like to work on?\nwhat is marrow\tMarrowOS is a local workspace for building and training models.\nhow do I train a model\tOpen Model Editor, select a template, edit the project configuration, and choose Train.\nwhere are my files\tYour datasets, modules, checkpoints and conversations are saved under /workspace on the ext4 disk.\nwhat is python\tPython is a programming language. You can run Python modules in Model Editor or the terminal.\nwhat is a dataset\tA dataset is a collection of training examples. Import text for a local chat model or CSV rows for the classifier.\n")
(root/'workspace/power.sh').write_text('uname -a\nfree -m\ndf -h /workspace\n')
# Build the bundled chat model using the same backend shipped to the guest.
import importlib.machinery,importlib.util
loader=importlib.machinery.SourceFileLoader('marrow_studio',str(project/'src/studio.py'));spec=importlib.util.spec_from_loader(loader.name,loader);studio=importlib.util.module_from_spec(spec);loader.exec_module(studio)
studio.ROOT=root/'workspace';studio.template('local-chat')
p=__import__('json').loads((root/'workspace/project.json').read_text())
p['name']='companion';p['model']='/workspace/models/companion.chat.json'
(root/'workspace/project.json').write_text(__import__('json').dumps(p,indent=2)+'\n')
local=dict(p,dataset=str(root/'workspace/datasets/conversation.txt'),model=str(root/'workspace/models/companion.chat.json'));studio.fit_local(local)

import zipfile
for wheel in (tools/'usr/share/python-wheels').glob('*.whl'):
 with zipfile.ZipFile(wheel) as z:z.extractall(root/'usr/lib/python3/dist-packages')
(root/'usr/bin/pip').write_text('#!/usr/bin/python3\nfrom pip._internal.cli.main import main\nraise SystemExit(main())\n');(root/'usr/bin/pip').chmod(0o755)

# Preserve multicall Git helpers without duplicating the executable for every command.
import hashlib
seen={}
for p in [root/'usr/bin/git',*sorted((root/'usr/lib/git-core').glob('*'))]:
 if p.is_file() and not p.is_symlink() and p.read_bytes()[:4]==b'\x7fELF':
  digest=hashlib.sha256(p.read_bytes()).digest()
  if digest in seen:p.unlink();p.symlink_to('/'+str(seen[digest].relative_to(root)))
  else:seen[digest]=p
(root/'usr/lib/python3.13/EXTERNALLY-MANAGED').unlink(missing_ok=True)
shutil.copytree(tools/'usr/share/python-wheels',root/'usr/share/python-wheels',dirs_exist_ok=True)
for alias,target in [('python','python3'),('pip3','pip')]:
 p=root/'usr/bin'/alias
 if p.exists() or p.is_symlink():p.unlink()
 p.symlink_to(target)
