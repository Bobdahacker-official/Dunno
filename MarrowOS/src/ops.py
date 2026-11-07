#!/usr/bin/python3
"""Task supervision, host administration and desktop sessions."""
import tempfile,re
import argparse,ctypes,datetime,json,os,pathlib,signal,socket,subprocess,sys,time,urllib.request,fcntl
ROOT=pathlib.Path('/workspace');RUN=ROOT/'runs';CFG=ROOT/'config';RUN.mkdir(exist_ok=True);CFG.mkdir(exist_ok=True)
def write(path,data):
 path=pathlib.Path(path);fd,tmp=tempfile.mkstemp(dir=path.parent)
 try:
  with os.fdopen(fd,'w') as f:f.write(json.dumps(data,indent=2)+'\n' if not isinstance(data,str) else data);f.flush();os.fsync(f.fileno())
  os.replace(tmp,path)
 finally:
  if os.path.exists(tmp):os.unlink(tmp)
def status(**kwargs):
 path=RUN/'task-status.json';old=json.loads(path.read_text()) if path.exists() else {};old.update(kwargs);old['updated']=datetime.datetime.now(datetime.timezone.utc).isoformat();write(path,old)
def stopped():return (RUN/'task.stop').exists()
def runstep(argv):
 p=subprocess.Popen(argv,start_new_session=True);status(child_pid=p.pid,command=argv)
 while p.poll() is None:
  if stopped():
   os.killpg(p.pid,signal.SIGTERM)
   try:p.wait(timeout=5)
   except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
   raise InterruptedError('Stopped by user')
  time.sleep(.2)
 if p.returncode:raise RuntimeError(f'Command exited with {p.returncode}')
def worker():
 lock=open('/run/marrow-task.lock','w')
 try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
 except BlockingIOError:return
 try:
  task=json.loads((CFG/'task.json').read_text());status(state='waiting',pid=os.getpid(),child_pid=0)
  deadline=time.monotonic()+max(0,int(task.get('delay_seconds',0)))
  while time.monotonic()<deadline:
   if stopped():raise InterruptedError('Stopped by user')
   time.sleep(.2)
  if stopped():raise InterruptedError('Stopped by user')
  if task.get('train',True):status(state='training');runstep(['python3','/usr/bin/marrow-studio','train'])
  destination=task.get('destination','')
  if destination:
   status(state='sending');model=json.loads((ROOT/'project.json').read_text())['model']
   if destination.startswith(('http://','https://')):runstep(['curl','--fail','--show-error','--upload-file',model,destination])
   else:runstep(['rsync','-a','--',model,destination])
  if task.get('command',''):status(state='command');runstep(['sh','-c',task['command']])
  status(state='finishing',child_pid=0)
  # A visible cancellation window also applies to a task that starts at boot.
  end=time.monotonic()+max(10,int(task.get('finish_grace_seconds',15)))
  while time.monotonic()<end:
   if stopped():raise InterruptedError('Stopped by user')
   time.sleep(.2)
  status(state='completed')
  if task.get('after') in ('poweroff','restart'):
   if stopped():raise InterruptedError('Stopped by user')
   os.sync();os.kill(1,signal.SIGUSR2 if task['after']=='poweroff' else signal.SIGTERM)
 except InterruptedError:status(state='stopped',child_pid=0)
 except Exception as e:status(state='failed',error=str(e),child_pid=0)
def start():
 path=RUN/'task-status.json'
 if path.exists():
  old=json.loads(path.read_text());pid=old.get('pid',0)
  if old.get('state') in ('waiting','training','sending','command','finishing','stopping') and pid:
   try:os.kill(pid,0);print('A task is already running');return
   except ProcessLookupError:pass
 (RUN/'task.stop').unlink(missing_ok=True)
 with (RUN/'task.log').open('a') as log:
  p=subprocess.Popen(['python3','/usr/bin/marrow-ops','worker'],stdout=log,stderr=log,start_new_session=True)
 print('Task started; Stop task cancels training, transfer and final power action.')
def desktop_install(name):
 allowed={'xfce':['xfce4','xfce4-terminal'],'mate':['mate-desktop-environment-core','mate-terminal'],'cinnamon':['cinnamon-core']}
 if name not in allowed:raise ValueError('Choose xfce, mate or cinnamon')
 base=pathlib.Path('/opt/desktops');base.mkdir(parents=True,exist_ok=True)
 if not (base/'usr/bin/apt-get').exists():
  subprocess.run(['debootstrap','--variant=minbase','--include=ca-certificates,udev,dbus-x11,xserver-xorg-core,xserver-xorg-video-fbdev,xserver-xorg-input-libinput,xinit,xauth,x11-xserver-utils,libgl1-mesa-dri','trixie',str(base),'https://deb.debian.org/debian'],check=True)
 mount_desktop(base)
 policy=base/'usr/sbin/policy-rc.d';policy.write_text('#!/bin/sh\nexit 101\n');policy.chmod(0o755)
 (base/'etc/resolv.conf').write_text(pathlib.Path('/etc/resolv.conf').read_text())
 subprocess.run(['chroot',str(base),'apt-get','update'],check=True)
 subprocess.run(['chroot',str(base),'apt-get','install','-y','--no-install-recommends',*allowed[name]],env=dict(os.environ,DEBIAN_FRONTEND='noninteractive'),check=True)
 (CFG/('desktop-'+name+'.installed')).touch();print('Installed',name)
def mount_desktop(base):
 for path in ['dev','dev/pts','proc','sys','run','workspace']:
  dst=base/path;dst.mkdir(parents=True,exist_ok=True)
  if not os.path.ismount(dst):subprocess.run(['mount','--rbind','/'+path,str(dst)],check=True);subprocess.run(['mount','--make-rslave',str(dst)],check=True)
def switch(name):
 if name not in ('marrow','xfce','mate','cinnamon'):raise ValueError('Unknown desktop')
 if name!='marrow' and not (CFG/('desktop-'+name+'.installed')).exists():raise RuntimeError('Install this desktop first')
 write(CFG/'desktop',name+'\n');os.kill(1,signal.SIGHUP);print('Switching desktop to',name)
def session():
 name=(CFG/'desktop').read_text().strip() if (CFG/'desktop').exists() else 'marrow'
 if name=='marrow':os.execl('/usr/bin/marrow-desktop','marrow-desktop')
 base=pathlib.Path('/opt/desktops');mount_desktop(base)
 subprocess.run(['chroot',str(base),'/usr/lib/systemd/systemd-udevd','--daemon'],check=False)
 subprocess.run(['chroot',str(base),'udevadm','trigger','--action=add'],check=True)
 subprocess.run(['chroot',str(base),'udevadm','settle','--timeout=15'],check=False)
 entry={'xfce':'xfce4-session','mate':'mate-session','cinnamon':'cinnamon-session'}[name]
 # Keep a visible recovery control inside every alternate session.
 restore=base/'usr/local/bin/marrow-restore';restore.parent.mkdir(parents=True,exist_ok=True)
 restore.write_text('#!/bin/sh\nprintf "marrow\\n" > /workspace/config/desktop\nkill -HUP 1\n');restore.chmod(0o755)
 apps=base/'usr/share/applications';apps.mkdir(parents=True,exist_ok=True)
 (apps/'marrow.desktop').write_text('[Desktop Entry]\nType=Application\nName=Return to MarrowOS\nExec=/usr/local/bin/marrow-restore\nIcon=system-reboot\nTerminal=false\n')
 stop_script=base/'usr/local/bin/marrow-task-stop';stop_script.write_text('#!/bin/sh\ntouch /workspace/runs/task.stop\nsed -i \'s/"on_boot": true/"on_boot": false/g\' /workspace/config/task.json\n');stop_script.chmod(0o755)
 (apps/'marrow-stop-task.desktop').write_text('[Desktop Entry]\nType=Application\nName=STOP MarrowOS task\nExec=/usr/local/bin/marrow-task-stop\nIcon=process-stop\nTerminal=false\n')

 xorg=base/'etc/X11/xorg.conf';xorg.write_text('Section "Device"\n Identifier "Framebuffer"\n Driver "fbdev"\n Option "fbdev" "/dev/fb0"\nEndSection\n')
 session_script=base/'usr/local/bin/marrow-session';session_script.write_text('#!/bin/sh\nexec /usr/bin/dbus-run-session -- /usr/bin/'+entry+'\n');session_script.chmod(0o755)
 runtime=base/'run/user/0';runtime.mkdir(parents=True,exist_ok=True);runtime.chmod(0o700)
 env=dict(os.environ,HOME='/root',USER='root',SHELL='/bin/sh',LIBGL_ALWAYS_SOFTWARE='1',XDG_RUNTIME_DIR='/run/user/0',XDG_SESSION_TYPE='x11',XDG_CURRENT_DESKTOP={'xfce':'XFCE','mate':'MATE','cinnamon':'X-Cinnamon'}[name])
 try:subprocess.run(['chroot',str(base),'xinit','/usr/local/bin/marrow-session','--','/usr/lib/xorg/Xorg',':0','vt1','-keeptty'],check=True,env=env)
 except Exception as e:
  print('Desktop session exited:',e);write(CFG/'desktop','marrow\n')
 os.execl('/usr/bin/marrow-desktop','marrow-desktop')
def report(kind):
 commands={'services':'ps -ef; ss -lntup','storage':'lsblk; df -h; mount','network':'ip -br addr; ip route; bridge link; wg show; nft list ruleset','security':'ss -lntup; ls -l /root/.ssh; cat /etc/ssh/sshd_config','resources':'free -m; uptime; ps -eo pid,ppid,pcpu,pmem,args','logs':'tail -80 /workspace/runs/task.log; dmesg | tail -40','backup':'tar -czf /workspace/runs/workspace-backup.tar.gz --exclude=runs --exclude=config/api-key -C /workspace .','ssh':'mkdir -p /run/sshd && ssh-keygen -A && (pgrep -x sshd >/dev/null || /usr/sbin/sshd) && ss -lntp','stop-ssh':'pkill -x sshd || true','keys':'mkdir -p /root/.ssh; chmod 700 /root/.ssh; test -f /root/.ssh/id_ed25519 || ssh-keygen -t ed25519 -N "" -f /root/.ssh/id_ed25519','sysctl':'sysctl -a','firewall':'nft list ruleset','task-log':'tail -100 /workspace/runs/task.log'}
 subprocess.run(['sh','-c',commands[kind]],check=True)
def main():
 a=sys.argv[1]
 if a=='configure':
  p=json.loads(pathlib.Path('/run/marrow-form.json').read_text())
  for key in ('train','on_boot'):p[key]=str(p.get(key,'false')).lower()=='true'
  for key in ('delay_seconds','finish_grace_seconds'):p[key]=max(0,int(p.get(key) or 0))
  if p.get('after') not in ('none','poweroff','restart'):raise ValueError('Choose none, poweroff or restart')
  p['name']=p.get('name') or 'default'
  if not re.fullmatch(r'[A-Za-z0-9_-]{1,40}',p['name']):raise ValueError('Task name: use letters, numbers, underscore or dash')
  profiles=CFG/'tasks';profiles.mkdir(exist_ok=True);write(profiles/(p['name']+'.json'),p);write(CFG/'task.json',p);print('Task saved')
 elif a=='task-next':
  profiles=sorted((CFG/'tasks').glob('*.json'))
  if not profiles:raise ValueError('Save a named task first')
  current=json.loads((CFG/'task.json').read_text()).get('name','default');names=[p.stem for p in profiles];index=(names.index(current)+1)%len(names) if current in names else 0
  write(CFG/'task.json',json.loads(profiles[index].read_text()));print('Selected',names[index])
 elif a=='worker':worker()
 elif a=='start':start()
 elif a=='stop':
  (RUN/'task.stop').touch();t=json.loads((CFG/'task.json').read_text());t['on_boot']=False;write(CFG/'task.json',t)
  profile=CFG/'tasks'/(t.get('name','default')+'.json')
  if profile.exists():write(profile,t)
  active=False
  for _ in range(60):
   current=json.loads((RUN/'task-status.json').read_text()) if (RUN/'task-status.json').exists() else {}
   if current.get('state') in ('stopped','failed','completed'):break
   pid=current.get('pid',0)
   if not pid:break
   try:os.kill(pid,0)
   except ProcessLookupError:break
   time.sleep(.1)
  else:active=True
  status(state='stopping' if active else 'stopped');print('Stop requested; boot task is also disabled')
 elif a=='boot':
  if (CFG/'task.json').exists() and json.loads((CFG/'task.json').read_text()).get('on_boot'):start()
 elif a=='desktop-install':desktop_install(sys.argv[2])
 elif a=='desktop-switch':switch(sys.argv[2])
 elif a=='session':session()
 else:report(a)
if __name__=='__main__':
 try:main()
 except Exception as e:print(type(e).__name__+': '+str(e),file=sys.stderr);sys.exit(1)
