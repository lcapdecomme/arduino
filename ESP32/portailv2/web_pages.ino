/**
 * PORTAIL v2 - Pages web et handlers API
 * v3: Fix agents, catégories pré-remplies, PIN config,
 *     anti-rebond 10s, suppression bloc actions accueil
 */

// =====================================================
// PIN Configuration (modifiable)
// =====================================================
#define CONFIG_PIN "1234"

// =====================================================
// HELPERS HTML
// =====================================================
static String cssBlock();
static String htmlHead(const String &title);
static String navBar(const String &active);
static String htmlTail();

static String cssBlock() {
  String c = "<style>";
  c += "*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}";
  c += ":root{--p:#1a73e8;--ok:#0d9488;--ko:#dc2626;--warn:#f59e0b;--bg:#f1f5f9;--card:#fff;--txt:#1e293b;--mt:#64748b;--bd:#e2e8f0;--r:12px}";
  c += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:var(--bg);color:var(--txt);line-height:1.5}";
  c += ".nb{background:var(--p);color:#fff;padding:.75rem 1rem;display:flex;align-items:center;justify-content:space-between;position:sticky;top:0;z-index:100;box-shadow:0 2px 8px rgba(0,0,0,.15)}";
  c += ".nb h1{font-size:1.1rem;font-weight:600}.nl{display:flex;gap:.25rem}";
  c += ".nl a{color:rgba(255,255,255,.8);text-decoration:none;padding:.4rem .65rem;border-radius:6px;font-size:.82rem;transition:all .2s}";
  c += ".nl a:hover,.nl a.ac{background:rgba(255,255,255,.2);color:#fff}";
  c += ".mt{display:none;background:none;border:none;color:#fff;font-size:1.5rem;cursor:pointer}";
  c += ".ct{max-width:1100px;margin:0 auto;padding:1rem}";
  c += ".cd{background:var(--card);border-radius:var(--r);padding:1.25rem;margin-bottom:1rem;box-shadow:0 1px 3px rgba(0,0,0,.08);border:1px solid var(--bd)}";
  c += ".cd h2{font-size:1rem;color:var(--mt);margin-bottom:.75rem;font-weight:500}";
  c += ".db{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:.75rem;margin-bottom:1rem}";
  c += ".sc{text-align:center;padding:1rem}.sv{font-size:1.7rem;font-weight:700;color:var(--p)}.sl{font-size:.8rem;color:var(--mt);margin-top:.15rem}";
  c += "table{width:100%;border-collapse:collapse;font-size:.88rem}";
  c += "th{text-align:left;padding:.5rem .6rem;border-bottom:2px solid var(--bd);color:var(--mt);font-weight:500;font-size:.78rem;text-transform:uppercase}";
  c += "td{padding:.5rem .6rem;border-bottom:1px solid var(--bd);vertical-align:middle}tr:hover{background:#f8fafc}";
  c += ".bg{display:inline-block;padding:.12rem .45rem;border-radius:50px;font-size:.72rem;font-weight:500}";
  c += ".bg-ok{background:#d1fae5;color:#065f46}.bg-ko{background:#fee2e2;color:#991b1b}";
  c += ".bg-in{background:#fef3c7;color:#92400e}.bg-bl{background:#e0e7ff;color:#3730a3}.bg-fe{background:#fce7f3;color:#9d174d}";
  c += ".bg-fo{background:#d1fae5;color:#065f46;border:1px dashed #065f46}";
  c += ".av{width:34px;height:34px;border-radius:50%;background:var(--p);color:#fff;display:inline-flex;align-items:center;justify-content:center;font-size:.72rem;font-weight:600;flex-shrink:0}";
  c += ".av-bl{background:var(--ko)}";
  c += "input,select{width:100%;padding:.55rem .7rem;border:1px solid var(--bd);border-radius:8px;font-size:.88rem;background:#fff;color:var(--txt)}";
  c += "input:focus,select:focus{outline:none;border-color:var(--p);box-shadow:0 0 0 3px rgba(26,115,232,.15)}";
  c += "label{display:block;font-size:.82rem;color:var(--mt);margin-bottom:.25rem;font-weight:500}";
  c += ".fg{margin-bottom:.7rem}.fr{display:grid;grid-template-columns:1fr 1fr;gap:.7rem}";
  c += "button,.btn{display:inline-flex;align-items:center;gap:.35rem;padding:.55rem 1rem;border:none;border-radius:8px;font-size:.82rem;font-weight:500;cursor:pointer;text-decoration:none;transition:all .2s}";
  c += ".bp{background:var(--p);color:#fff}.bp:hover{background:#1557b0}";
  c += ".bs{background:var(--ok);color:#fff}.bs:hover{background:#0f766e}";
  c += ".bd{background:var(--ko);color:#fff}.bd:hover{background:#b91c1c}";
  c += ".bo{background:transparent;color:var(--p);border:1px solid var(--bd)}.bo:hover{background:#f1f5f9}";
  c += ".bsm{padding:.3rem .65rem;font-size:.78rem}.bg-grp{display:flex;gap:.5rem;flex-wrap:wrap}";
  c += ".dg{display:flex;flex-wrap:wrap;gap:.35rem}.dc{display:flex;align-items:center;gap:.25rem}";
  c += ".dc input[type=checkbox]{width:auto}.dc label{margin:0;font-size:.82rem}";
  c += ".toast{position:fixed;bottom:1.5rem;right:1.5rem;padding:.7rem 1.1rem;border-radius:8px;color:#fff;font-size:.88rem;z-index:999;transform:translateY(100px);opacity:0;transition:all .3s}";
  c += ".toast.show{transform:translateY(0);opacity:1}.to{background:var(--ok)}.te{background:var(--ko)}";
  c += ".mo{display:none;position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,.5);z-index:200;align-items:center;justify-content:center;padding:1rem}";
  c += ".mo.active{display:flex}.ml{background:#fff;border-radius:var(--r);padding:1.5rem;max-width:520px;width:100%;max-height:90vh;overflow-y:auto}";
  c += ".ml h2{font-size:1.1rem;color:var(--txt);margin-bottom:1rem}";
  c += ".fc{display:flex;gap:.5rem;flex-wrap:wrap;margin-bottom:.7rem}";
  c += ".fc button{font-size:.78rem;padding:.3rem .7rem;border-radius:20px;border:1px solid var(--bd);background:#fff;cursor:pointer;transition:all .2s}";
  c += ".fc button.active{background:var(--p);color:#fff;border-color:var(--p)}";
  c += ".warn-bar{background:#fef3c7;border:1px solid #f59e0b;border-radius:8px;padding:.7rem 1rem;margin-bottom:1rem;font-size:.85rem;color:#92400e}";
  c += ".pin-overlay{position:fixed;top:0;left:0;right:0;bottom:0;background:var(--bg);z-index:300;display:flex;align-items:center;justify-content:center}";
  c += ".pin-box{background:var(--card);border-radius:var(--r);padding:2rem;text-align:center;box-shadow:0 4px 20px rgba(0,0,0,.15);max-width:320px;width:90%}";
  c += ".pin-box h2{margin-bottom:1rem;color:var(--txt)}.pin-box input{text-align:center;font-size:1.5rem;letter-spacing:.5rem;margin-bottom:1rem}";
  c += "@media(max-width:768px){";
  c += ".mt{display:block}.nl{display:none;position:absolute;top:100%;left:0;right:0;background:var(--p);flex-direction:column;padding:.5rem}";
  c += ".nl.open{display:flex}.nl a{padding:.55rem 1rem}.fr{grid-template-columns:1fr}";
  c += ".db{grid-template-columns:repeat(2,1fr)}table{font-size:.78rem}th,td{padding:.35rem .4rem}.hm{display:none}";
  c += "}@media(max-width:480px){.ct{padding:.5rem}}";
  c += "</style>";
  return c;
}

static String htmlHead(const String &title) {
  String h = "<!DOCTYPE html>\n<html lang='fr'>\n<head>\n<meta charset='UTF-8'>\n";
  h += "<meta name='viewport' content='width=device-width,initial-scale=1.0'>\n";
  h += "<title>" + String(config.titre) + " - " + title + "</title>\n";
  h += cssBlock();
  h += "</head>\n<body>\n";
  h += navBar(title);
  h += "<div class='ct'>\n";
  return h;
}

static String navBar(const String &active) {
  String h = "<nav class='nb'><h1>&#x1F6E1; " + String(config.titre) + "</h1>";
  h += "<button class='mt' onclick=\"document.querySelector('.nl').classList.toggle('open')\">&#9776;</button>";
  h += "<div class='nl'>";
  h += "<a href='/' class='" + String(active=="Accueil"?"ac":"") + "'>Accueil</a>";
  h += "<a href='/enroll' class='" + String(active=="Personnel"?"ac":"") + "'>Personnel</a>";
  h += "<a href='/auth' class='" + String(active=="Autorisations"?"ac":"") + "'>Autorisations</a>";
  h += "<a href='/history' class='" + String(active=="Historique"?"ac":"") + "'>Historique</a>";
  h += "<a href='/config' class='" + String(active=="Config"?"ac":"") + "'>Config</a>";
  h += "</div></nav>\n";
  return h;
}

static String htmlTail() {
  String h = "</div><div id='toast' class='toast'></div>\n<script>\n";
  h += "function toast(m,ok=true){const t=document.getElementById('toast');t.textContent=m;t.className='toast '+(ok?'to':'te')+' show';setTimeout(()=>t.classList.remove('show'),3000);}\n";
  h += "async function api(u,m='GET',b=null){const o={method:m};if(b){o.headers={'Content-Type':'application/x-www-form-urlencoded'};o.body=b;}return(await fetch(u,o)).json();}\n";
  h += "function ini(n,p){return((p?p[0]:'')+(n?n[0]:'')).toUpperCase();}\n";
  h += "function cm(id){document.getElementById(id).classList.remove('active');}\n";
  h += "</script></body></html>";
  return h;
}

#define BADGE_JS "function stBg(s){const m={OK:'bg-ok',Inconnu:'bg-in',Bloque:'bg-bl',Horaire:'bg-ko',Ferie:'bg-fe',ForceOK:'bg-fo'};const l={OK:'Autoris\\u00e9',Inconnu:'Inconnu',Bloque:'Bloqu\\u00e9',Horaire:'Hors horaire',Ferie:'F\\u00e9ri\\u00e9',ForceOK:'Forc\\u00e9'};return '<span class=\"bg '+(m[s]||'bg-ko')+'\">'+(l[s]||s)+'</span>';}"

// =====================================================
// PAGE ACCUEIL (sans bloc Actions, refresh 3s)
// =====================================================
void handleRoot() {
  String h = htmlHead("Accueil");

  h += "<div id='force-bar' class='warn-bar' style='display:none'>&#9888; Mode marche forc&eacute;e actif</div>";

  h += "<div class='db'>";
  h += "<div class='cd sc'><div class='sv' id='s-ag'>-</div><div class='sl'>Agents</div></div>";
  h += "<div class='cd sc'><div class='sv' id='s-ok' style='color:var(--ok)'>-</div><div class='sl'>OK aujourd'hui</div></div>";
  h += "<div class='cd sc'><div class='sv' id='s-ko' style='color:var(--ko)'>-</div><div class='sl'>Refus&eacute;s</div></div>";
  h += "<div class='cd sc'><div class='sv' id='s-in' style='color:var(--warn)'>-</div><div class='sl'>Inconnus</div></div>";
  h += "<div class='cd sc'><div class='sv' id='s-bl' style='color:#6366f1'>-</div><div class='sl'>Bloqu&eacute;s</div></div>";
  h += "<div class='cd sc'><div class='sv' id='s-dk'>-</div><div class='sl'>Disque</div></div>";
  h += "</div>";

  h += "<div id='block-warn' class='warn-bar' style='display:none'></div>";

  h += "<div class='cd'><h2>&#128336; Derniers acc&egrave;s</h2>";
  h += "<table><thead><tr><th></th><th>Nom</th><th class='hm'>Heure</th><th>Statut</th><th class='hm'>Tag</th></tr></thead>";
  h += "<tbody id='la'><tr><td colspan='5'>Chargement...</td></tr></tbody></table></div>";

  h += "<div class='cd'><p style='font-size:.82rem;color:var(--mt)'>Heure: <span id='s-t'>-</span> | IP: " + WiFi.softAPIP().toString() + "</p></div>";

  h += "<script>" BADGE_JS "\n";
  h += "async function ld(){try{const d=await api('/api/stats');\n";
  h += "document.getElementById('s-ag').textContent=d.agentCount;\n";
  h += "document.getElementById('s-ok').textContent=d.ok+(d.forceOk>0?'+'+d.forceOk:'');\n";
  h += "document.getElementById('s-ko').textContent=(d.horaire||0)+(d.ferie||0);\n";
  h += "document.getElementById('s-in').textContent=d.inconnu||0;\n";
  h += "document.getElementById('s-bl').textContent=d.bloque||0;\n";
  h += "const p=d.diskTotal>0?Math.round(d.diskUsed*100/d.diskTotal):0;\n";
  h += "document.getElementById('s-dk').textContent=p+'%';\n";
  h += "document.getElementById('s-t').textContent=d.dateTime||'-';\n";
  h += "document.getElementById('force-bar').style.display=d.forceMode?'block':'none';\n";
  h += "const bw=document.getElementById('block-warn');\n";
  h += "if(d.agentsBloque>0){bw.style.display='block';bw.innerHTML='&#128683; '+d.agentsBloque+' agent(s) bloqu\\u00e9(s)';}\n";
  h += "else bw.style.display='none';\n";
  h += "const tb=document.getElementById('la');\n";
  h += "if(!d.last||!d.last.length){tb.innerHTML='<tr><td colspan=\"5\">Aucun acc\\u00e8s</td></tr>';return;}\n";
  h += "tb.innerHTML=d.last.map(e=>{\n";
  h += "const isInc=e.st==='Inconnu';const nm=isInc?'<em style=\"color:var(--warn)\">Inconnu</em>':(e.pre||'')+' '+(e.nom||'');\n";
  h += "const avCls=e.st==='Bloque'?' av-bl':'';\n";
  h += "const avTxt=isInc?'?':ini(e.nom,e.pre);\n";
  h += "const t=e.dt?(e.dt.split(' ')[1]||''):'';\n";
  h += "const tg=isInc?('<span style=\"font-family:monospace;font-size:.75rem\">'+e.tag+'</span>'):(e.tag?e.tag.substring(0,8)+'...':'');\n";
  h += "return '<tr><td><span class=\"av'+avCls+'\">'+avTxt+'</span></td><td>'+nm+'</td><td class=\"hm\">'+t+'</td><td>'+stBg(e.st)+'</td><td class=\"hm\">'+tg+'</td></tr>';\n";
  h += "}).join('');}catch(e){console.error(e);}}\n";
  h += "ld();setInterval(ld,3000);\n</script>";
  h += htmlTail();
  server.send(200, "text/html", h);
}

// =====================================================
// PAGE PERSONNEL
// =====================================================
void handleEnrollPage() {
  String h = htmlHead("Personnel");

  h += "<div class='cd'><h2>&#128100; Enr&ocirc;ler un agent</h2>";
  h += "<div class='fg'><label>Badge RFID</label><div style='display:flex;gap:.5rem'>";
  h += "<input type='text' id='e-tag' placeholder='Scannez un badge...' readonly style='font-family:monospace;background:#f8fafc'>";
  h += "<button class='btn bp' onclick='scan()'>&#128225; Scanner</button></div>";
  h += "<small id='tst' style='color:var(--mt);font-size:.78rem'></small></div>";
  h += "<div class='fr'><div class='fg'><label>Nom</label><input type='text' id='e-nom'></div>";
  h += "<div class='fg'><label>Pr&eacute;nom</label><input type='text' id='e-pre'></div></div>";
  h += "<div class='fr'><div class='fg'><label>Plaque</label><input type='text' id='e-plaq'></div>";
  h += "<div class='fg'><label>Cat&eacute;gorie</label><select id='e-cat'></select></div></div>";
  h += "<button class='btn bs' onclick='enroll()'>&#10004; Enr&ocirc;ler</button></div>";

  h += "<div class='cd'><h2>&#128101; Agents (<span id='cnt'>0</span>)</h2>";
  h += "<input type='text' id='srch' placeholder='&#128269; Rechercher...' oninput='filt()' style='margin-bottom:.7rem'>";
  h += "<div style='overflow-x:auto'><table><thead><tr><th></th><th>Nom</th><th class='hm'>Plaque</th><th>Cat.</th><th>Actions</th></tr></thead>";
  h += "<tbody id='agl'></tbody></table></div></div>";

  // Modal
  h += "<div class='mo' id='em'><div class='ml'><h2>&#9998; Modifier</h2>";
  h += "<input type='hidden' id='ei'>";
  h += "<div class='fr'><div class='fg'><label>Nom</label><input type='text' id='en'></div>";
  h += "<div class='fg'><label>Pr&eacute;nom</label><input type='text' id='ep'></div></div>";
  h += "<div class='fr'><div class='fg'><label>Plaque</label><input type='text' id='epl'></div>";
  h += "<div class='fg'><label>Cat&eacute;gorie</label><select id='ec'></select></div></div>";
  h += "<div class='bg-grp' style='margin-top:1rem'>";
  h += "<button class='btn bs' onclick='saveE()'>Enregistrer</button>";
  h += "<button class='btn bo' onclick='cm(\"em\")'>Annuler</button></div></div></div>";

  h += "<script>\nlet ag=[],cats=[];\n";

  // Charger catégories puis remplir les selects
  h += "async function loadCats(){\n";
  h += "  const d=await api('/api/categories');\n";
  h += "  cats=d.cat||[];\n";
  h += "  fillSel('e-cat');fillSel('ec');\n";
  h += "}\n";
  h += "function fillSel(id){\n";
  h += "  const s=document.getElementById(id);\n";
  h += "  if(!cats.length){s.innerHTML='<option value=\"0\">Aucune cat.</option>';return;}\n";
  h += "  s.innerHTML=cats.map(c=>'<option value=\"'+c.idx+'\">'+c.nom+'</option>').join('');\n";
  h += "}\n";

  // Charger agents
  h += "async function loadAg(){\n";
  h += "  const d=await api('/api/agents');\n";
  h += "  ag=d.agents||[];\n";
  h += "  document.getElementById('cnt').textContent=d.count||0;\n";
  h += "  render(ag);\n";
  h += "}\n";

  h += "function render(l){const t=document.getElementById('agl');\n";
  h += "if(!l.length){t.innerHTML='<tr><td colspan=\"5\">Aucun agent</td></tr>';return;}\n";
  h += "t.innerHTML=l.map(a=>{const av=ini(a.nom,a.prenom);const cls=a.bloque?' av-bl':'';\n";
  h += "const bl=a.bloque?'<span class=\"bg bg-bl\">Bloqu\\u00e9</span> ':'';\n";
  h += "const blBtn=a.bloque?'<button class=\"btn bo bsm\" onclick=\"block('+a.idx+',false)\" title=\"D\\u00e9bloquer\">&#9989;</button>':'<button class=\"btn bo bsm\" onclick=\"block('+a.idx+',true)\" title=\"Bloquer\">&#128683;</button>';\n";
  h += "return '<tr><td><span class=\"av'+cls+'\">'+av+'</span></td><td>'+bl+'<strong>'+(a.prenom||'')+' '+(a.nom||'')+'</strong></td>'+\n";
  h += "'<td class=\"hm\">'+(a.plaque||'')+'</td><td>'+(a.catNom||'?')+'</td>'+\n";
  h += "'<td><div class=\"bg-grp\"><button class=\"btn bo bsm\" onclick=\"edit('+a.idx+')\">&#9998;</button>'+blBtn+\n";
  h += "'<button class=\"btn bd bsm\" onclick=\"del('+a.idx+',\\''+a.nom+'\\')\">&times;</button></div></td></tr>';}).join('');}\n";

  h += "function filt(){const q=document.getElementById('srch').value.toLowerCase();render(ag.filter(a=>(a.nom+' '+a.prenom+' '+a.plaque+' '+a.catNom).toLowerCase().includes(q)));}\n";

  // Scanner
  h += "async function scan(){const el=document.getElementById('e-tag'),st=document.getElementById('tst');st.textContent='Pr\\u00e9sentez le badge...';st.style.color='var(--p)';let n=0;\n";
  h += "const p=setInterval(async()=>{n++;if(n>30){clearInterval(p);st.textContent='Timeout';st.style.color='var(--ko)';return;}\n";
  h += "const d=await api('/api/last-tag');if(d.tag&&d.tag.length===24&&d.tag!=='000000000000000000000000'){\n";
  h += "el.value=d.tag;st.textContent='D\\u00e9tect\\u00e9 !';st.style.color='var(--ok)';clearInterval(p);}},1000);}\n";

  // Enrôler - utiliser l'index de la catégorie sélectionnée
  h += "async function enroll(){\n";
  h += "  const t=document.getElementById('e-tag').value.trim();\n";
  h += "  const n=document.getElementById('e-nom').value.trim();\n";
  h += "  const p=document.getElementById('e-pre').value.trim();\n";
  h += "  const pl=document.getElementById('e-plaq').value.trim();\n";
  h += "  const c=document.getElementById('e-cat').value||'0';\n";
  h += "  if(!t||t.length!==24){toast('Scannez un badge',false);return;}\n";
  h += "  if(!n||!p){toast('Nom et pr\\u00e9nom requis',false);return;}\n";
  h += "  const b='tag='+encodeURIComponent(t)+'&nom='+encodeURIComponent(n)+'&prenom='+encodeURIComponent(p)+'&plaque='+encodeURIComponent(pl)+'&cat='+c;\n";
  h += "  const d=await api('/api/agent/add','POST',b);\n";
  h += "  if(d.ok){toast('Agent enr\\u00f4l\\u00e9 !');['e-tag','e-nom','e-pre','e-plaq'].forEach(x=>document.getElementById(x).value='');document.getElementById('tst').textContent='';loadAg();}\n";
  h += "  else toast(d.error||'Erreur',false);\n";
  h += "}\n";

  // Édition
  h += "function edit(i){const a=ag.find(x=>x.idx===i);if(!a)return;document.getElementById('ei').value=i;\n";
  h += "document.getElementById('en').value=a.nom;document.getElementById('ep').value=a.prenom;\n";
  h += "document.getElementById('epl').value=a.plaque;document.getElementById('ec').value=a.catIdx;\n";
  h += "document.getElementById('em').classList.add('active');}\n";

  h += "async function saveE(){const i=document.getElementById('ei').value;\n";
  h += "const b='idx='+i+'&nom='+encodeURIComponent(document.getElementById('en').value.trim())+'&prenom='+encodeURIComponent(document.getElementById('ep').value.trim())+'&plaque='+encodeURIComponent(document.getElementById('epl').value.trim())+'&cat='+document.getElementById('ec').value;\n";
  h += "const d=await api('/api/agent/update','POST',b);if(d.ok){toast('OK');cm('em');loadAg();}else toast(d.error||'Err',false);}\n";

  h += "async function del(i,n){if(!confirm('Supprimer '+n+' ?'))return;const d=await api('/api/agent/delete','POST','idx='+i);if(d.ok){toast('Supprim\\u00e9');loadAg();}}\n";

  h += "async function block(i,b){const d=await api('/api/agent/block','POST','idx='+i+'&bloque='+(b?1:0));if(d.ok){toast(b?'Bloqu\\u00e9':'D\\u00e9bloqu\\u00e9');loadAg();}}\n";

  // Init : charger catégories PUIS agents
  h += "async function init(){await loadCats();await loadAg();}\ninit();\n</script>";
  h += htmlTail();
  server.send(200, "text/html", h);
}

// =====================================================
// PAGE AUTORISATIONS
// =====================================================
void handleAuthPage() {
  String h = htmlHead("Autorisations");

  h += "<div class='cd'><h2>&#128274; Cat&eacute;gories d'acc&egrave;s</h2>";
  h += "<p style='font-size:.82rem;color:var(--mt);margin-bottom:.7rem'>Chaque agent appartient &agrave; une cat&eacute;gorie qui d&eacute;finit ses horaires d'acc&egrave;s.</p>";
  h += "<div id='cl'></div></div>";

  // Modal catégorie
  h += "<div class='mo' id='cm2'><div class='ml'><h2 id='cm2t'>Modifier</h2>";
  h += "<input type='hidden' id='ci'>";
  h += "<div class='fg'><label>Nom</label><input type='text' id='cn'></div>";
  h += "<div class='fg'><label>Jours autoris&eacute;s</label><div class='dg'>";
  for (int i = 1; i <= 7; i++) {
    const char* n[] = {"","Lun","Mar","Mer","Jeu","Ven","Sam","Dim"};
    h += "<div class='dc'><input type='checkbox' id='dj" + String(i) + "' value='" + String(i) + "'><label for='dj" + String(i) + "'>" + n[i] + "</label></div>";
  }
  h += "</div></div>";
  h += "<div class='fr'><div class='fg'><label>Heure d&eacute;but</label><input type='time' id='chd'></div>";
  h += "<div class='fg'><label>Heure fin</label><input type='time' id='chf'></div></div>";
  h += "<div class='bg-grp' style='margin-top:1rem'><button class='btn bs' onclick='saveCat()'>Enregistrer</button>";
  h += "<button class='btn bo' onclick='cm(\"cm2\")'>Annuler</button></div></div></div>";

  // Fériés
  h += "<div class='cd'><h2>&#128197; Jours f&eacute;ri&eacute;s</h2>";
  h += "<p style='font-size:.82rem;color:var(--mt);margin-bottom:.7rem'>Acc&egrave;s bloqu&eacute; pour tous ces jours-l&agrave;.</p>";
  h += "<div style='display:flex;gap:.5rem;margin-bottom:.7rem;flex-wrap:wrap'>";
  h += "<input type='date' id='hd' style='flex:1;min-width:140px'>";
  h += "<input type='text' id='hl' placeholder='Libell&eacute;' style='flex:1;min-width:140px'>";
  h += "<button class='btn bp' onclick='addH()'>Ajouter</button></div>";
  h += "<div id='hlist'></div></div>";

  h += "<script>\nconst dn=['','Lun','Mar','Mer','Jeu','Ven','Sam','Dim'];\nlet cats=[];\n";

  h += "async function loadC(){const d=await api('/api/categories');cats=d.cat||[];renderC();}\n";
  h += "function renderC(){const el=document.getElementById('cl');\n";
  h += "if(!cats.length){el.innerHTML='<p style=\"color:var(--mt)\">Aucune cat\\u00e9gorie</p>';return;}\n";
  h += "let t='<table><thead><tr><th>Cat&eacute;gorie</th><th>Jours</th><th class=\"hm\">Horaires</th><th></th></tr></thead><tbody>';\n";
  h += "cats.forEach(c=>{const j=(c.jours||'').split(',').map(d=>dn[parseInt(d)]||'').filter(Boolean).join(', ');\n";
  h += "t+='<tr><td><strong>'+c.nom+'</strong></td><td>'+j+'</td><td class=\"hm\">'+c.hDeb+' - '+c.hFin+'</td>';\n";
  h += "t+='<td><div class=\"bg-grp\"><button class=\"btn bo bsm\" onclick=\"editC('+c.idx+')\">&#9998;</button>';\n";
  h += "t+='<button class=\"btn bd bsm\" onclick=\"delC('+c.idx+',\\''+c.nom+'\\')\">&times;</button></div></td></tr>';});\n";
  h += "t+='</tbody></table>';\n";
  h += "t+='<button class=\"btn bp bsm\" onclick=\"addC()\" style=\"margin-top:.7rem\">&#10010; Nouvelle cat&eacute;gorie</button>';\n";
  h += "el.innerHTML=t;}\n";

  h += "function addC(){document.getElementById('ci').value=-1;document.getElementById('cn').value='';\n";
  h += "for(let i=1;i<=7;i++)document.getElementById('dj'+i).checked=(i<=5);\n";
  h += "document.getElementById('chd').value='08:00';document.getElementById('chf').value='18:00';\n";
  h += "document.getElementById('cm2t').textContent='Nouvelle cat\\u00e9gorie';\n";
  h += "document.getElementById('cm2').classList.add('active');}\n";

  h += "function editC(i){const c=cats.find(x=>x.idx===i);if(!c)return;\n";
  h += "document.getElementById('ci').value=i;document.getElementById('cn').value=c.nom;\n";
  h += "const j=(c.jours||'').split(',');for(let d=1;d<=7;d++)document.getElementById('dj'+d).checked=j.includes(String(d));\n";
  h += "document.getElementById('chd').value=c.hDeb;document.getElementById('chf').value=c.hFin;\n";
  h += "document.getElementById('cm2t').textContent='Modifier: '+c.nom;\n";
  h += "document.getElementById('cm2').classList.add('active');}\n";

  h += "async function saveCat(){const i=parseInt(document.getElementById('ci').value);\n";
  h += "const nom=document.getElementById('cn').value.trim();\n";
  h += "if(!nom){toast('Nom requis',false);return;}\n";
  h += "let j=[];for(let d=1;d<=7;d++)if(document.getElementById('dj'+d).checked)j.push(d);\n";
  h += "const b='idx='+i+'&nom='+encodeURIComponent(nom)+'&jours='+j.join(',')+'&hDeb='+document.getElementById('chd').value+'&hFin='+document.getElementById('chf').value;\n";
  h += "const url=i<0?'/api/category/add':'/api/category/update';\n";
  h += "const d=await api(url,'POST',b);if(d.ok){toast('OK');cm('cm2');loadC();}else toast(d.error||'Err',false);}\n";

  h += "async function delC(i,n){if(!confirm('Supprimer cat\\u00e9gorie '+n+' ?\\nLes agents seront d\\u00e9plac\\u00e9s.'))return;\n";
  h += "const d=await api('/api/category/delete','POST','idx='+i);if(d.ok){toast('Supprim\\u00e9');loadC();}else toast(d.error||'Err',false);}\n";

  // Fériés
  h += "async function loadH(){const d=await api('/api/holidays');const l=d.holidays||[];const el=document.getElementById('hlist');\n";
  h += "if(!l.length){el.innerHTML='<p style=\"color:var(--mt)\">Aucun jour f\\u00e9ri\\u00e9</p>';return;}\n";
  h += "let t='<table><thead><tr><th>Date</th><th>Libell&eacute;</th><th></th></tr></thead><tbody>';\n";
  h += "l.forEach(x=>{t+='<tr><td>'+x.date+'</td><td>'+x.label+'</td><td><button class=\"btn bd bsm\" onclick=\"delH('+x.idx+')\">&times;</button></td></tr>';});\n";
  h += "el.innerHTML=t+'</tbody></table>';}\n";

  h += "async function addH(){const dt=document.getElementById('hd').value,lb=document.getElementById('hl').value.trim();\n";
  h += "if(!dt){toast('Date requise',false);return;}\n";
  h += "const d=await api('/api/holiday/add','POST','date='+dt+'&label='+encodeURIComponent(lb||'F\\u00e9ri\\u00e9'));\n";
  h += "if(d.ok){toast('Ajout\\u00e9');document.getElementById('hd').value='';document.getElementById('hl').value='';loadH();}}\n";

  h += "async function delH(i){if(!confirm('Supprimer ?'))return;const d=await api('/api/holiday/delete','POST','idx='+i);if(d.ok){toast('OK');loadH();}}\n";

  h += "loadC();loadH();\n</script>";
  h += htmlTail();
  server.send(200, "text/html", h);
}

// =====================================================
// PAGE HISTORIQUE
// =====================================================
void handleHistoryPage() {
  String h = htmlHead("Historique");

  h += "<div class='cd'><h2>&#128203; Historique des acc&egrave;s</h2>";
  h += "<div style='display:flex;gap:.5rem;margin-bottom:.5rem;flex-wrap:wrap;align-items:center'>";
  h += "<input type='text' id='sq' placeholder='&#128269; Rechercher...' oninput='flt()' style='flex:1;min-width:180px'></div>";

  h += "<div class='fc' id='filters'>";
  h += "<button class='active' onclick='setF(this,\"\")'>Tous</button>";
  h += "<button onclick='setF(this,\"OK\")'>&#9989; OK</button>";
  h += "<button onclick='setF(this,\"Inconnu\")'>&#10067; Inconnus</button>";
  h += "<button onclick='setF(this,\"Bloque\")'>&#128683; Bloqu&eacute;s</button>";
  h += "<button onclick='setF(this,\"Horaire\")'>&#9200; Horaire</button>";
  h += "<button onclick='setF(this,\"Ferie\")'>&#127878; F&eacute;ri&eacute;s</button>";
  h += "<button onclick='setF(this,\"ForceOK\")'>&#128275; Forc&eacute;s</button></div>";

  h += "<div style='overflow-x:auto'><table><thead><tr><th>Date/Heure</th><th>Nom / Tag</th><th class='hm'>Tag</th><th>Statut</th></tr></thead>";
  h += "<tbody id='hl2'></tbody></table></div>";
  h += "<p style='font-size:.78rem;color:var(--mt);margin-top:.5rem'>Affich&eacute;: <span id='hc'>0</span> / <span id='ht'>0</span></p></div>";

  h += "<script>" BADGE_JS "\nlet allH=[],curF='';\n";
  h += "async function loadHi(){const d=await api('/api/history');allH=d.history||[];document.getElementById('ht').textContent=d.count;flt();}\n";
  h += "function setF(btn,f){curF=f;document.querySelectorAll('.fc button').forEach(b=>b.classList.remove('active'));btn.classList.add('active');flt();}\n";
  h += "function flt(){const q=document.getElementById('sq').value.toLowerCase();\n";
  h += "let list=allH;if(curF)list=list.filter(e=>e.st===curF);\n";
  h += "if(q)list=list.filter(e=>((e.nom||'')+(e.pre||'')+(e.tag||'')+(e.st||'')).toLowerCase().includes(q));\n";
  h += "document.getElementById('hc').textContent=list.length;renderH(list);}\n";

  h += "function renderH(l){const tb=document.getElementById('hl2');\n";
  h += "if(!l.length){tb.innerHTML='<tr><td colspan=\"4\">Aucun</td></tr>';return;}\n";
  h += "tb.innerHTML=l.slice(0,200).map(e=>{\n";
  h += "const isInc=e.st==='Inconnu';\n";
  h += "const nm=isInc?'<em style=\"color:var(--warn)\">Inconnu</em><br><span style=\"font-family:monospace;font-size:.75rem\">'+e.tag+'</span>':(e.pre||'')+' '+(e.nom||'');\n";
  h += "const tg=e.tag?e.tag.substring(0,10)+'...':'';\n";
  h += "return '<tr><td style=\"white-space:nowrap\">'+(e.dt||'')+'</td><td>'+nm+'</td><td class=\"hm\" style=\"font-family:monospace;font-size:.78rem\">'+tg+'</td><td>'+stBg(e.st)+'</td></tr>';}).join('');}\n";

  h += "loadHi();\n</script>";
  h += htmlTail();
  server.send(200, "text/html", h);
}

// =====================================================
// PAGE CONFIGURATION (avec PIN)
// =====================================================
void handleConfigPage() {
  String h = htmlHead("Config");

  // Overlay PIN
  h += "<div class='pin-overlay' id='pin-ov'><div class='pin-box'>";
  h += "<h2>&#128274; Code PIN</h2>";
  h += "<input type='password' id='pin-in' maxlength='6' placeholder='****' onkeyup='if(event.key===\"Enter\")checkPin()'>";
  h += "<br><button class='btn bp' onclick='checkPin()' style='margin-top:.7rem'>Valider</button>";
  h += "<p id='pin-err' style='color:var(--ko);font-size:.82rem;margin-top:.5rem'></p>";
  h += "</div></div>";

  // Contenu config (caché)
  h += "<div id='cfg-content' style='display:none'>";

  h += "<div class='cd'><h2>&#9881; Configuration g&eacute;n&eacute;rale</h2>";
  h += "<div class='fg'><label>Titre de l'application</label><input type='text' id='cfg-titre'></div>";
  h += "<button class='btn bs' onclick='saveCfg()'>Enregistrer</button></div>";

  h += "<div class='cd'><h2>&#128275; Marche forc&eacute;e</h2>";
  h += "<p style='font-size:.82rem;color:var(--mt);margin-bottom:.7rem'>Le portail reste ouvert pour tous durant cette plage.</p>";
  h += "<div class='fg'><label><input type='checkbox' id='cfg-force' style='width:auto;margin-right:.5rem'>Activer</label></div>";
  h += "<div class='fr'><div class='fg'><label>D&eacute;but</label><input type='time' id='cfg-fd'></div>";
  h += "<div class='fg'><label>Fin</label><input type='time' id='cfg-ff'></div></div>";
  h += "<button class='btn bs' onclick='saveCfg()'>Enregistrer</button></div>";

  h += "<div class='cd'><h2>&#128193; Cat&eacute;gories</h2>";
  h += "<a href='/auth' class='btn bp'>&#128274; G&eacute;rer les cat&eacute;gories</a></div>";

  h += "<div class='cd'><h2>&#128190; Syst&egrave;me</h2>";
  h += "<p style='font-size:.82rem;color:var(--mt)' id='sys-info'>...</p>";
  h += "<div class='bg-grp' style='margin-top:.7rem'>";
  h += "<button class='btn bo' onclick='doExp()'>&#128229; Exporter</button>";
  h += "<button class='btn bo' onclick='document.getElementById(\"impF\").click()'>&#128228; Importer</button>";
  h += "<input type='file' id='impF' accept='.json' style='display:none' onchange='doImp(this)'></div></div>";

  h += "</div>"; // fin cfg-content

  h += "<script>\n";
  // PIN check
  h += "function checkPin(){const v=document.getElementById('pin-in').value;\n";
  h += "if(v==='" CONFIG_PIN "'){document.getElementById('pin-ov').style.display='none';document.getElementById('cfg-content').style.display='block';loadCfg();}\n";
  h += "else{document.getElementById('pin-err').textContent='Code incorrect';document.getElementById('pin-in').value='';}}\n";

  h += "async function loadCfg(){const d=await api('/api/config');\n";
  h += "document.getElementById('cfg-titre').value=d.titre||'PORTAIL';\n";
  h += "document.getElementById('cfg-force').checked=d.force||false;\n";
  h += "document.getElementById('cfg-fd').value=d.fDeb||'06:00';\n";
  h += "document.getElementById('cfg-ff').value=d.fFin||'22:00';\n";
  h += "const s=await api('/api/stats');\n";
  h += "document.getElementById('sys-info').innerHTML='Agents: '+s.agentCount+' | Cat: '+s.catCount+' | Disque: '+Math.round(s.diskUsed*100/s.diskTotal)+'%';}\n";

  h += "async function saveCfg(){const b='titre='+encodeURIComponent(document.getElementById('cfg-titre').value.trim())+'&force='+(document.getElementById('cfg-force').checked?1:0)+'&fDeb='+document.getElementById('cfg-fd').value+'&fFin='+document.getElementById('cfg-ff').value;\n";
  h += "const d=await api('/api/config/update','POST',b);if(d.ok)toast('Enregistr\\u00e9');else toast('Erreur',false);}\n";

  h += "function doExp(){window.location.href='/api/export';}\n";
  h += "async function doImp(i){const f=i.files[0];if(!f)return;const t=await f.text();\n";
  h += "const r=await fetch('/api/import',{method:'POST',headers:{'Content-Type':'application/json'},body:t});\n";
  h += "const d=await r.json();if(d.ok){toast('Import OK');setTimeout(()=>location.reload(),1000);}else toast('Erreur',false);}\n";

  h += "document.getElementById('pin-in').focus();\n</script>";
  h += htmlTail();
  server.send(200, "text/html", h);
}

// =====================================================
// HANDLERS API
// =====================================================
void handleApiAgents()     { server.send(200, "application/json", getAgentsJson()); }
void handleApiHistory()    { server.send(200, "application/json", getHistoryJson()); }
void handleApiStats()      { server.send(200, "application/json", getStatsJson()); }
void handleApiCategories() { server.send(200, "application/json", getCategoriesJson()); }
void handleApiHolidays()   { server.send(200, "application/json", getHolidaysJson()); }
void handleApiConfig()     { server.send(200, "application/json", getConfigJson()); }

void handleApiAgentAdd() {
  String tag = server.arg("tag"); tag.toUpperCase(); tag.trim();
  String nom = server.arg("nom"), pre = server.arg("prenom"), pl = server.arg("plaque");
  int cat = server.arg("cat").toInt();
  if (cat < 0 || cat >= categoryCount) cat = 0;
  if (tag.length() != TAG_LENGTH) { server.send(200,"application/json","{\"ok\":false,\"error\":\"Tag invalide (24 car.)\"}"); return; }
  if (nom.length()==0||pre.length()==0) { server.send(200,"application/json","{\"ok\":false,\"error\":\"Nom/prenom requis\"}"); return; }
  int r = addAgent(tag, nom, pre, pl, cat);
  if (r==-1) server.send(200,"application/json","{\"ok\":false,\"error\":\"Max 200 agents\"}");
  else if (r==-2) server.send(200,"application/json","{\"ok\":false,\"error\":\"Badge deja enregistre\"}");
  else server.send(200,"application/json","{\"ok\":true}");
}

void handleApiAgentDelete() {
  if (deleteAgent(server.arg("idx").toInt())) server.send(200,"application/json","{\"ok\":true}");
  else server.send(200,"application/json","{\"ok\":false,\"error\":\"Introuvable\"}");
}

void handleApiAgentUpdate() {
  int i = server.arg("idx").toInt();
  int cat = server.arg("cat").toInt();
  if (cat < 0 || cat >= categoryCount) cat = 0;
  if (updateAgent(i, server.arg("nom"), server.arg("prenom"), server.arg("plaque"), cat))
    server.send(200,"application/json","{\"ok\":true}");
  else server.send(200,"application/json","{\"ok\":false,\"error\":\"Introuvable\"}");
}

void handleApiAgentBlock() {
  int i = server.arg("idx").toInt();
  bool b = server.arg("bloque") == "1";
  if (setAgentBlocked(i, b)) server.send(200,"application/json","{\"ok\":true}");
  else server.send(200,"application/json","{\"ok\":false,\"error\":\"Introuvable\"}");
}

void handleApiCategoryAdd() {
  if (categoryCount >= MAX_CATEGORIES) { server.send(200,"application/json","{\"ok\":false,\"error\":\"Max categories\"}"); return; }
  sCopy(categories[categoryCount].nom,           server.arg("nom"), 20);
  sCopy(categories[categoryCount].joursAutorise,  server.arg("jours"), 16);
  sCopy(categories[categoryCount].heureDebut,     server.arg("hDeb"), 6);
  sCopy(categories[categoryCount].heureFin,       server.arg("hFin"), 6);
  categoryCount++; saveCategories();
  server.send(200,"application/json","{\"ok\":true}");
}

void handleApiCategoryUpdate() {
  int i = server.arg("idx").toInt();
  if (i<0||i>=categoryCount) { server.send(200,"application/json","{\"ok\":false,\"error\":\"Introuvable\"}"); return; }
  sCopy(categories[i].nom,           server.arg("nom"), 20);
  sCopy(categories[i].joursAutorise,  server.arg("jours"), 16);
  sCopy(categories[i].heureDebut,     server.arg("hDeb"), 6);
  sCopy(categories[i].heureFin,       server.arg("hFin"), 6);
  saveCategories();
  server.send(200,"application/json","{\"ok\":true}");
}

void handleApiCategoryDelete() {
  int i = server.arg("idx").toInt();
  if (i<0||i>=categoryCount) { server.send(200,"application/json","{\"ok\":false,\"error\":\"Introuvable\"}"); return; }
  for (int j=i;j<categoryCount-1;j++) categories[j]=categories[j+1];
  categoryCount--; saveCategories();
  for (int j=0;j<agentCount;j++) {
    if (agents[j].categorieIdx==i) agents[j].categorieIdx=0;
    else if (agents[j].categorieIdx>i) agents[j].categorieIdx--;
  }
  saveAgents();
  server.send(200,"application/json","{\"ok\":true}");
}

void handleApiHolidayAdd() {
  String dt = server.arg("date"), lb = server.arg("label");
  if (dt.length()<10) { server.send(200,"application/json","{\"ok\":false,\"error\":\"Date invalide\"}"); return; }
  if (holidayCount>=MAX_HOLIDAYS) { server.send(200,"application/json","{\"ok\":false,\"error\":\"Max feries\"}"); return; }
  sCopy(holidays[holidayCount].date,  dt, 12);
  sCopy(holidays[holidayCount].label, lb.length()>0 ? lb : String("Ferie"), 20);
  holidayCount++; saveHolidays();
  server.send(200,"application/json","{\"ok\":true}");
}

void handleApiHolidayDelete() {
  int i = server.arg("idx").toInt();
  if (i<0||i>=holidayCount) { server.send(200,"application/json","{\"ok\":false}"); return; }
  for (int j=i;j<holidayCount-1;j++) holidays[j]=holidays[j+1];
  holidayCount--; saveHolidays();
  server.send(200,"application/json","{\"ok\":true}");
}

void handleApiConfigUpdate() {
  String titre = server.arg("titre");
  sCopy(config.titre, titre.length()>0 ? titre : String("PORTAIL"), 30);
  config.marcheForce = server.arg("force") == "1";
  sCopy(config.forceHeureDebut, server.arg("fDeb"), 6);
  sCopy(config.forceHeureFin,   server.arg("fFin"), 6);
  saveConfig();
  server.send(200,"application/json","{\"ok\":true}");
}

void handleApiExport() {
  String data = getAllDataJson();
  server.sendHeader("Content-Disposition","attachment; filename=portail_backup.json");
  server.send(200,"application/json",data);
}

void handleApiImport() {
  String body = server.arg("plain");
  if (body.length()==0) { server.send(200,"application/json","{\"ok\":false,\"error\":\"Vide\"}"); return; }
  if (importAllData(body)) server.send(200,"application/json","{\"ok\":true}");
  else server.send(200,"application/json","{\"ok\":false,\"error\":\"JSON invalide\"}");
}

void handleApiLastTag() {
  String json = "{\"tag\":\"" + lastDetectedTag + "\",\"age\":" + String(millis()-lastDetectedTime) + "}";
  server.send(200,"application/json",json);
}

// =====================================================
// CAPTIVE PORTAL
// =====================================================
void handleCaptiveDetect() {
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/html", "");
}

void handleNotFound() {
  String host = server.hostHeader();
  if (host.length() > 0 && host != WiFi.softAPIP().toString()) {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/html", "");
    return;
  }
  handleRoot();
}
