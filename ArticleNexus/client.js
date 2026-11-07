const capacity = document.querySelector('#capacity');
const workload = document.querySelector('#workload');
function recommend(){
 const gb=Number(capacity.value), need={everyday:16,creative:32,heavy:64}[workload.value];
 document.querySelector('#verdict').textContent=gb>=need?'A lovely amount of headroom.':'Your tabs would like a little more room.';
 document.querySelector('#advice').textContent=`For this workload, ${need} GB is a useful starting point. Actual needs vary by application; check your system’s compatibility before buying.`;
}
if(capacity){capacity.addEventListener('change',recommend);workload.addEventListener('change',recommend);recommend();}
const salute=document.querySelector('#salute');
if(salute){let liked=false;try{liked=localStorage.getItem('ram-salute')==='yes';}catch{}const render=()=>{salute.setAttribute('aria-pressed',String(liked));salute.textContent=liked?'♥  Your stick is appreciated':'♡  Appreciate a stick';};render();salute.addEventListener('click',()=>{liked=!liked;try{localStorage.setItem('ram-salute',liked?'yes':'no');}catch{}render();});}
const search=document.querySelector('#search');
if(search)search.addEventListener('input',()=>{let count=0;for(const card of document.querySelectorAll('.article-card')){card.hidden=!card.textContent.toLowerCase().includes(search.value.trim().toLowerCase());if(!card.hidden)count++;}document.querySelector('#results').textContent=`${count} article${count===1?'':'s'}`;});
