"""PR kapsamını, Makefile istisnasını ve raporlanan test sayısını ölçer."""
from pathlib import Path
import re,subprocess,glob

def git(*args):return subprocess.check_output(['git',*args],text=True)
assert Path.cwd().name=='kemgu-codex','Yalnız ayrılmış worktree içinde çalıştır'
assert git('branch','--show-current').strip()=='codex/surucu-dalga1'
paths=git('diff','--name-only','origin/main').splitlines()+git('ls-files','--others','--exclude-standard').splitlines()
for p in paths:
 assert p=='Makefile' or p.startswith(('drivers/','tests/drivers/','tools/codex/')) or re.fullmatch(r'otomasyon/chatgpt/codex_[^/]+\.md',p),p
base=git('show','origin/main:Makefile');current=Path('Makefile').read_text()
pattern=r'CHATGPT_TESTLER :=.*?\n(?=\w)'
def normalize(s):return re.sub(pattern,'CHATGPT_TESTLER := <liste>\n',s,flags=re.S)
assert normalize(base)==normalize(current),'Makefile wildcard listesi dışında fark'
block=re.search(pattern,current,re.S).group();files=sorted({p for pat in re.findall(r'tests/drivers/[^\s)]+',block) for p in glob.glob(pat)})
rx=r'işlev test_[^(]+\([^)]*\) -> mantıksal'
count=sum(len(re.findall(rx,Path(p).read_text())) for p in files)
done=re.findall(r'^- \[x\] ([A-F]\d+)',Path('otomasyon/chatgpt/codex_ilerleme.md').read_text(),re.M)
assert len(done)==len(set(done))==39,done
subprocess.run(['git','diff','--check','origin/main'],check=True)
subprocess.run(['python3','tools/codex/envanter_kontrol.py'],check=True)
print('SON_KONTROL OK:',len(set(paths)),'izinli dosya;',len(files),'test dosyası;',count,'test işlevi;',len(done),'tamamlanan iş')
