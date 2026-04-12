/**
 * PORTAIL - Interface HTML/CSS/JS
 * 
 * Page web responsive avec:
 * - Rafraîchissement temps réel (SSE)
 * - Gestion des catégories
 * - Filtres historique
 * - Titre personnalisable
 */

// ===== PAGE HTML PRINCIPALE =====
String getPageHTML() {
  String html = "<!DOCTYPE html>\n";
  html += "<html lang='fr'>\n";
  html += "<head>\n";
  html += "  <meta charset='UTF-8'>\n";
  html += "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n";
  html += "  <title>" + String(config.titre) + "</title>\n";
  html += getCSS();
  html += "</head>\n";
  html += "<body>\n";
  html += "  <div id='app'>\n";
  html += "    <header>\n";
  html += "      <h1 id='mainTitle'>" + String(config.titre) + "</h1>\n";
  html += "      <p class='subtitle'>Controle d'acces RFID</p>\n";
  html += "      <div id='statusBar'></div>\n";
  html += "    </header>\n";
  html += "    <nav>\n";
  html += "      <button class='nav-btn active' data-page='accueil'>Accueil</button>\n";
  html += "      <button class='nav-btn' data-page='personnel'>Personnel</button>\n";
  html += "      <button class='nav-btn' data-page='historique'>Historique</button>\n";
  html += "      <button class='nav-btn' data-page='categories'>Categories</button>\n";
  html += "      <button class='nav-btn' data-page='conges'>Conges</button>\n";
  html += "      <button class='nav-btn' data-page='config'>Config</button>\n";
  html += "    </nav>\n";
  html += "    <main id='content'></main>\n";
  html += "  </div>\n";
  html += "  <div id='modal' class='modal hidden'>\n";
  html += "    <div class='modal-content'>\n";
  html += "      <span class='modal-close'>&times;</span>\n";
  html += "      <div id='modal-body'></div>\n";
  html += "    </div>\n";
  html += "  </div>\n";
  html += getJS();
  html += "</body>\n";
  html += "</html>\n";
  return html;
}

// ===== STYLES CSS =====
String getCSS() {
  String css = "<style>\n";
  css += ":root{--primary:#2563eb;--primary-dark:#1d4ed8;--success:#16a34a;--danger:#dc2626;--warning:#d97706;--orange:#ea580c;--purple:#9333ea;--bg:#f1f5f9;--card:#fff;--text:#1e293b;--text-muted:#64748b;--border:#e2e8f0}\n";
  css += "*{box-sizing:border-box;margin:0;padding:0}\n";
  css += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:var(--bg);color:var(--text);min-height:100vh}\n";
  css += "#app{max-width:1200px;margin:0 auto;padding:1rem}\n";
  css += "header{text-align:center;padding:1rem 0}\n";
  css += "header h1{font-size:1.75rem;color:var(--primary)}\n";
  css += ".subtitle{color:var(--text-muted);font-size:.9rem}\n";
  css += "#statusBar{margin-top:.5rem;font-size:.85rem}\n";
  css += ".status-marche{background:#fef3c7;color:#92400e;padding:.25rem .75rem;border-radius:1rem;display:inline-block}\n";
  css += "nav{display:flex;gap:.5rem;margin-bottom:1rem;flex-wrap:wrap;justify-content:center}\n";
  css += ".nav-btn{padding:.5rem 1rem;border:none;border-radius:.5rem;background:var(--card);color:var(--text);cursor:pointer;font-size:.85rem;transition:all .2s;box-shadow:0 1px 2px rgba(0,0,0,.1)}\n";
  css += ".nav-btn:hover,.nav-btn.active{background:var(--primary);color:#fff}\n";
  css += ".card{background:var(--card);border-radius:.75rem;padding:1rem;margin-bottom:1rem;box-shadow:0 1px 3px rgba(0,0,0,.1)}\n";
  css += ".card-title{font-size:1rem;font-weight:600;margin-bottom:.75rem;display:flex;align-items:center;gap:.5rem}\n";
  css += ".stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(100px,1fr));gap:.75rem}\n";
  css += ".stat-box{text-align:center;padding:.75rem;background:var(--bg);border-radius:.5rem}\n";
  css += ".stat-value{font-size:1.5rem;font-weight:700;color:var(--primary)}\n";
  css += ".stat-label{font-size:.75rem;color:var(--text-muted);margin-top:.15rem}\n";
  css += ".stat-box.success .stat-value{color:var(--success)}\n";
  css += ".stat-box.danger .stat-value{color:var(--danger)}\n";
  css += ".stat-box.warning .stat-value{color:var(--warning)}\n";
  css += ".stat-box.orange .stat-value{color:var(--orange)}\n";
  css += ".access-list{display:flex;flex-direction:column;gap:.5rem}\n";
  css += ".access-item{display:flex;align-items:center;gap:.75rem;padding:.5rem;background:var(--bg);border-radius:.5rem;border-left:3px solid var(--success)}\n";
  css += ".access-item.refused{border-left-color:var(--danger)}\n";
  css += ".access-item.blocked{border-left-color:var(--orange)}\n";
  css += ".access-item.unknown{border-left-color:var(--purple)}\n";
  css += ".access-icon{font-size:1.25rem}\n";
  css += ".access-info{flex:1;min-width:0}\n";
  css += ".access-name{font-weight:600;font-size:.9rem;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}\n";
  css += ".access-uid{font-family:monospace;font-size:.7rem;color:var(--text-muted)}\n";
  css += ".access-time{font-size:.75rem;color:var(--text-muted)}\n";
  css += ".access-status{font-size:.7rem;padding:.2rem .4rem;border-radius:.25rem;white-space:nowrap}\n";
  css += ".status-ok{background:#dcfce7;color:var(--success)}\n";
  css += ".status-inconnu{background:#f3e8ff;color:var(--purple)}\n";
  css += ".status-bloque{background:#ffedd5;color:var(--orange)}\n";
  css += ".status-horaire{background:#fee2e2;color:var(--danger)}\n";
  css += ".status-ferie{background:#fef3c7;color:#92400e}\n";
  css += ".status-forcee{background:#dbeafe;color:var(--primary)}\n";
  css += ".btn{display:inline-flex;align-items:center;gap:.4rem;padding:.5rem .75rem;border:none;border-radius:.5rem;font-size:.85rem;cursor:pointer;transition:all .2s}\n";
  css += ".btn-primary{background:var(--primary);color:#fff}\n";
  css += ".btn-primary:hover{background:var(--primary-dark)}\n";
  css += ".btn-success{background:var(--success);color:#fff}\n";
  css += ".btn-danger{background:var(--danger);color:#fff}\n";
  css += ".btn-warning{background:var(--warning);color:#fff}\n";
  css += ".btn-orange{background:var(--orange);color:#fff}\n";
  css += ".btn-sm{padding:.3rem .5rem;font-size:.75rem}\n";
  css += ".form-group{margin-bottom:.75rem}\n";
  css += ".form-label{display:block;margin-bottom:.25rem;font-weight:500;font-size:.85rem}\n";
  css += ".form-input,.form-select{width:100%;padding:.5rem;border:1px solid var(--border);border-radius:.375rem;font-size:.9rem}\n";
  css += ".form-input:focus,.form-select:focus{outline:none;border-color:var(--primary);box-shadow:0 0 0 2px rgba(37,99,235,.1)}\n";
  css += ".table-container{overflow-x:auto}\n";
  css += "table{width:100%;border-collapse:collapse;font-size:.8rem}\n";
  css += "th,td{padding:.5rem;text-align:left;border-bottom:1px solid var(--border)}\n";
  css += "th{background:var(--bg);font-weight:600}\n";
  css += "tr:hover{background:var(--bg)}\n";
  css += ".modal{position:fixed;inset:0;background:rgba(0,0,0,.5);display:flex;align-items:center;justify-content:center;z-index:1000;padding:1rem}\n";
  css += ".modal.hidden{display:none}\n";
  css += ".modal-content{background:var(--card);border-radius:.75rem;width:100%;max-width:500px;max-height:90vh;overflow-y:auto;position:relative}\n";
  css += ".modal-close{position:absolute;top:.75rem;right:.75rem;font-size:1.25rem;cursor:pointer;color:var(--text-muted)}\n";
  css += "#modal-body{padding:1.25rem}\n";
  css += ".plages-grid{display:grid;gap:.4rem}\n";
  css += ".plage-row{display:flex;align-items:center;gap:.4rem;flex-wrap:wrap;font-size:.85rem}\n";
  css += ".plage-row label{min-width:60px}\n";
  css += ".plage-row input[type='time']{padding:.25rem;border:1px solid var(--border);border-radius:.25rem;font-size:.8rem}\n";
  css += ".plage-row input[type='checkbox']{width:16px;height:16px}\n";
  css += ".badge-display{font-family:monospace;background:var(--bg);padding:.25rem .5rem;border-radius:.25rem;font-size:.75rem;word-break:break-all}\n";
  css += ".badge-bloque{background:#ffedd5;color:var(--orange);padding:.15rem .4rem;border-radius:.25rem;font-size:.7rem;margin-left:.5rem}\n";
  css += ".filter-bar{display:flex;gap:.5rem;margin-bottom:.75rem;flex-wrap:wrap}\n";
  css += ".filter-btn{padding:.25rem .5rem;border:1px solid var(--border);border-radius:.25rem;background:var(--card);font-size:.75rem;cursor:pointer}\n";
  css += ".filter-btn.active{background:var(--primary);color:#fff;border-color:var(--primary)}\n";
  css += ".empty-state{text-align:center;padding:1.5rem;color:var(--text-muted);font-size:.9rem}\n";
  css += ".flex-between{display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:.75rem}\n";
  css += ".loading{opacity:.6;pointer-events:none}\n";
  css += ".toast{position:fixed;bottom:1rem;right:1rem;padding:.75rem 1rem;border-radius:.5rem;color:#fff;z-index:1001;animation:slideIn .3s;font-size:.9rem}\n";
  css += ".toast.success{background:var(--success)}\n";
  css += ".toast.error{background:var(--danger)}\n";
  css += ".switch{position:relative;display:inline-block;width:44px;height:24px}\n";
  css += ".switch input{opacity:0;width:0;height:0}\n";
  css += ".slider{position:absolute;cursor:pointer;inset:0;background:#ccc;transition:.3s;border-radius:24px}\n";
  css += ".slider:before{position:absolute;content:'';height:18px;width:18px;left:3px;bottom:3px;background:#fff;transition:.3s;border-radius:50%}\n";
  css += "input:checked+.slider{background:var(--success)}\n";
  css += "input:checked+.slider:before{transform:translateX(20px)}\n";
  css += "@keyframes slideIn{from{transform:translateX(100%);opacity:0}to{transform:translateX(0);opacity:1}}\n";
  css += "@media(max-width:768px){header h1{font-size:1.5rem}.stats-grid{grid-template-columns:repeat(2,1fr)}table{font-size:.7rem}th,td{padding:.35rem}.plage-row{font-size:.8rem}}\n";
  css += "</style>\n";
  return css;
}

// ===== JAVASCRIPT =====
String getJS() {
  String js = "<script>\n";
  js += "const API={get:u=>fetch(u).then(r=>r.json()),post:(u,d)=>fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)}).then(r=>r.json())};\n";
  js += "const JOURS=['Lundi','Mardi','Mercredi','Jeudi','Vendredi','Samedi','Dimanche'];\n";
  js += "const STATUTS={0:'OK',1:'Inconnu',2:'Bloque',3:'Hors horaire',4:'Jour ferie',5:'Marche forcee'};\n";
  js += "let currentPage='accueil',personnes=[],historique=[],conges=[],categories=[],stats={},config={},filtreStatut=-1;\n";
  
  // Polling léger pour rafraîchissement (remplace SSE qui bloquait le RFID)
  js += "let lastTs=0,lastNb=0,refreshInterval=null;\n";
  js += "function startPolling(){if(refreshInterval)clearInterval(refreshInterval);refreshInterval=setInterval(async()=>{if(currentPage!=='accueil')return;try{const r=await API.get('/api/refresh');if(r.ts!==lastTs||r.nb!==lastNb){lastTs=r.ts;lastNb=r.nb;loadPage('accueil');}}catch(e){}},3000);}\n";
  
  // Navigation
  js += "document.querySelectorAll('.nav-btn').forEach(btn=>{btn.addEventListener('click',()=>{document.querySelectorAll('.nav-btn').forEach(b=>b.classList.remove('active'));btn.classList.add('active');currentPage=btn.dataset.page;loadPage(currentPage);});});\n";
  
  // Modal
  js += "const modal=document.getElementById('modal'),modalBody=document.getElementById('modal-body');\n";
  js += "document.querySelector('.modal-close').onclick=()=>modal.classList.add('hidden');\n";
  js += "modal.onclick=e=>{if(e.target===modal)modal.classList.add('hidden');};\n";
  js += "function showModal(c){modalBody.innerHTML=c;modal.classList.remove('hidden');}\n";
  js += "function hideModal(){modal.classList.add('hidden');}\n";
  js += "function toast(m,t='success'){const e=document.createElement('div');e.className='toast '+t;e.textContent=m;document.body.appendChild(e);setTimeout(()=>e.remove(),3000);}\n";
  
  // Load page
  js += "async function loadPage(p){const c=document.getElementById('content');c.classList.add('loading');try{switch(p){case'accueil':await renderAccueil();break;case'personnel':await renderPersonnel();break;case'historique':await renderHistorique();break;case'categories':await renderCategories();break;case'conges':await renderConges();break;case'config':await renderConfig();break;}}catch(e){console.error(e);if(e.message&&e.message.includes('NetworkError')){toast('Connexion perdue','error');}else{toast('Erreur','error');}}c.classList.remove('loading');}\n";
   
  // Status bar
  js += "function updateStatusBar(){let sb=document.getElementById('statusBar');if(stats.marcheForcee){sb.innerHTML='<span class=\"status-marche\">⚡ Marche forcee active</span>';}else if(stats.agentsBloques>0){sb.innerHTML='<span style=\"color:var(--orange)\">🚫 '+stats.agentsBloques+' agent(s) bloque(s)</span>';}else{sb.innerHTML='';}}\n";
  
  // Page Accueilawait renderHistorique();br
  js += "async function renderAccueil(){stats=await API.get('/api/stats');historique=await API.get('/api/historique?limit=10');config=await API.get('/api/config');document.getElementById('mainTitle').textContent=config.titre;updateStatusBar();\n";
  js += "let h='<div class=\"card\"><div class=\"flex-between\"><div class=\"card-title\">📊 Statistiques du jour</div><button class=\"btn btn-sm btn-danger\" onclick=\"clearHistorique()\">🗑️ Effacer</button></div><div class=\"stats-grid\">';\n";
  js += "h+='<div class=\"stat-box\"><div class=\"stat-value\">'+stats.personnes+'</div><div class=\"stat-label\">Personnel</div></div>';\n";
  js += "h+='<div class=\"stat-box success\"><div class=\"stat-value\">'+stats.aujourdhui.ok+'</div><div class=\"stat-label\">OK</div></div>';\n";
  js += "h+='<div class=\"stat-box warning\"><div class=\"stat-value\">'+stats.aujourdhui.horsHoraire+'</div><div class=\"stat-label\">Hors horaire</div></div>';\n";
  js += "h+='<div class=\"stat-box orange\"><div class=\"stat-value\">'+stats.aujourdhui.bloque+'</div><div class=\"stat-label\">Bloques</div></div>';\n";
  js += "h+='<div class=\"stat-box danger\"><div class=\"stat-value\">'+stats.aujourdhui.inconnu+'</div><div class=\"stat-label\">Inconnus</div></div>';\n";
  js += "h+='<div class=\"stat-box\"><div class=\"stat-value\">'+stats.aujourdhui.total+'</div><div class=\"stat-label\">Accès</div></div></div></div>';\n";
  
  // Agents bloqués
  js += "let bloques=personnes.filter(p=>p.bloque);\n";
  js += "if(stats.agentsBloques>0){h+='<div class=\"card\" style=\"border-left:3px solid var(--orange)\"><div class=\"card-title\">🚫 Agents bloques ('+stats.agentsBloques+')</div>';personnes=await API.get('/api/personnes');bloques=personnes.filter(p=>p.bloque);bloques.forEach(p=>{h+='<span style=\"display:inline-block;margin:.25rem;padding:.25rem .5rem;background:#ffedd5;border-radius:.25rem;font-size:.85rem\">'+p.prenom+' '+p.nom+'</span>';});h+='</div>';}\n";
  
  // Derniers accès
  js += "h+='<div class=\"card\"><div class=\"card-title\">🕐 Derniers acces</div>';\n";
  js += "if(!historique.length)h+='<div class=\"empty-state\">Aucun acces</div>';else{h+='<div class=\"access-list\">';historique.forEach(e=>{let cls='';let icon='✅';let stCls='status-ok';if(e.statut===1){cls='unknown';icon='❓';stCls='status-inconnu';}else if(e.statut===2){cls='blocked';icon='🚫';stCls='status-bloque';}else if(e.statut===3){cls='refused';icon='⏰';stCls='status-horaire';}else if(e.statut===4){cls='refused';icon='📅';stCls='status-ferie';}else if(e.statut===5){icon='⚡';stCls='status-forcee';}let nom=e.prenom||e.nom?(e.prenom+' '+e.nom).trim():'';h+='<div class=\"access-item '+cls+'\"><div class=\"access-icon\">'+icon+'</div><div class=\"access-info\"><div class=\"access-name\">'+(nom||'Inconnu')+'</div>'+(e.statut===1?'<div class=\"access-uid\">'+e.uid+'</div>':'')+'<div class=\"access-time\">'+e.date+'</div></div><span class=\"access-status '+stCls+'\">'+e.statutStr+'</span></div>';});h+='</div>';}h+='</div>';\n";
  js += "document.getElementById('content').innerHTML=h;}\n";
  
  // Effacer historique
  js += "async function clearHistorique(){if(!confirm('Effacer tout l\\'historique ?'))return;await API.post('/api/historique/clear',{});toast('Historique efface');loadPage('accueil');}\n";
  
  // Page Personnel
  js += "async function renderPersonnel(){personnes=await API.get('/api/personnes');categories=await API.get('/api/categories');let h='<div class=\"card\"><div class=\"flex-between\"><div class=\"card-title\">👥 Personnel ('+personnes.length+')</div><button class=\"btn btn-primary\" onclick=\"showAddPersonne()\">+ Ajouter</button></div>';\n";
  js += "if(!personnes.length)h+='<div class=\"empty-state\">Aucune personne</div>';else{h+='<div class=\"table-container\"><table><thead><tr><th>Nom</th><th>Categorie</th><th>Badge</th><th>Actions</th></tr></thead><tbody>';personnes.forEach(p=>{let cat=categories.find(c=>c.id===p.categorie);h+='<tr><td><strong>'+p.prenom+' '+p.nom+'</strong>'+(p.bloque?'<span class=\"badge-bloque\">BLOQUE</span>':'')+'</td><td>'+(cat?cat.nom:'-')+'</td><td><span class=\"badge-display\">'+p.uid.substring(0,10)+'...</span></td><td><button class=\"btn btn-sm btn-primary\" onclick=\"showEditPersonne('+p.id+')\">✏️</button> <button class=\"btn btn-sm '+(p.bloque?'btn-success':'btn-orange')+'\" onclick=\"toggleBloque('+p.id+')\">'+(p.bloque?'✓':'🚫')+'</button> <button class=\"btn btn-sm btn-danger\" onclick=\"deletePersonne('+p.id+')\">🗑️</button></td></tr>';});h+='</tbody></table></div>';}h+='</div>';\n";
  js += "document.getElementById('content').innerHTML=h;}\n";
  
  // Toggle bloqué
  js += "async function toggleBloque(id){await API.post('/api/personnes/toggle-bloque',{id});toast('Statut modifie');renderPersonnel();}\n";
  
  // Ajout personne
  js += "async function showAddPersonne(){const b=await API.get('/api/dernier-badge');categories=await API.get('/api/categories');let h='<h3 style=\"margin-bottom:1rem\">Nouvelle personne</h3><form id=\"formP\" onsubmit=\"return savePersonne(event)\"><div class=\"form-group\"><label class=\"form-label\">Badge UID</label><input type=\"text\" class=\"form-input\" name=\"uid\" value=\"'+(b.uid||'')+'\" required maxlength=\"24\"><button type=\"button\" class=\"btn btn-sm\" onclick=\"refreshBadge()\" style=\"margin-top:.25rem\">🔄 Actualiser</button></div>';\n";
  js += "h+='<div class=\"form-group\"><label class=\"form-label\">Prenom</label><input type=\"text\" class=\"form-input\" name=\"prenom\" required></div><div class=\"form-group\"><label class=\"form-label\">Nom</label><input type=\"text\" class=\"form-input\" name=\"nom\" required></div><div class=\"form-group\"><label class=\"form-label\">Plaque</label><input type=\"text\" class=\"form-input\" name=\"plaque\"></div>';\n";
  js += "h+='<div class=\"form-group\"><label class=\"form-label\">Categorie</label><select class=\"form-select\" name=\"categorie\">';categories.forEach(c=>{h+='<option value=\"'+c.id+'\">'+c.nom+'</option>';});h+='</select></div>';\n";
  js += "h+='<button type=\"submit\" class=\"btn btn-primary\" style=\"width:100%\">Enregistrer</button></form>';showModal(h);}\n";
  
  js += "async function refreshBadge(){const b=await API.get('/api/dernier-badge');if(b.uid)document.querySelector('input[name=\"uid\"]').value=b.uid;toast('Badge actualise');}\n";
  
  // Save personne - avec gestion des erreurs
  js += "async function savePersonne(e){e.preventDefault();const f=e.target;const d={uid:f.uid.value.toUpperCase(),nom:f.nom.value,prenom:f.prenom.value,plaque:f.plaque.value,categorie:parseInt(f.categorie.value)};const id=f.dataset.id;let r;if(id){d.id=parseInt(id);r=await API.post('/api/personnes/update',d);}else{r=await API.post('/api/personnes',d);}if(r.error){toast(r.error,'error');return false;}hideModal();toast('Enregistre');renderPersonnel();return false;}\n";
  
  // Edit personne
  js += "async function showEditPersonne(id){const p=personnes.find(x=>x.id===id);if(!p)return;categories=await API.get('/api/categories');let h='<h3 style=\"margin-bottom:1rem\">Modifier '+p.prenom+'</h3><form id=\"formP\" data-id=\"'+id+'\" onsubmit=\"return savePersonne(event)\"><div class=\"form-group\"><label class=\"form-label\">Badge UID</label><input type=\"text\" class=\"form-input\" name=\"uid\" value=\"'+p.uid+'\" required></div><div class=\"form-group\"><label class=\"form-label\">Prenom</label><input type=\"text\" class=\"form-input\" name=\"prenom\" value=\"'+p.prenom+'\" required></div><div class=\"form-group\"><label class=\"form-label\">Nom</label><input type=\"text\" class=\"form-input\" name=\"nom\" value=\"'+p.nom+'\" required></div><div class=\"form-group\"><label class=\"form-label\">Plaque</label><input type=\"text\" class=\"form-input\" name=\"plaque\" value=\"'+p.plaque+'\"></div>';\n";
  js += "h+='<div class=\"form-group\"><label class=\"form-label\">Categorie</label><select class=\"form-select\" name=\"categorie\">';categories.forEach(c=>{h+='<option value=\"'+c.id+'\"'+(c.id===p.categorie?' selected':'')+'>'+c.nom+'</option>';});h+='</select></div>';\n";
  js += "h+='<button type=\"submit\" class=\"btn btn-primary\" style=\"width:100%\">Enregistrer</button></form>';showModal(h);}\n";
  
  // Delete personne
  js += "async function deletePersonne(id){if(!confirm('Supprimer?'))return;await API.post('/api/personnes/delete',{id});toast('Supprime');renderPersonnel();}\n";
  
  // Page Historique avec filtres
  js += "async function renderHistorique(){let url='/api/historique?limit=100';if(filtreStatut>=0)url+='&statut='+filtreStatut;historique=await API.get(url);let h='<div class=\"card\"><div class=\"card-title\">📋 Historique</div>';\n";
  js += "h+='<div class=\"filter-bar\"><button class=\"filter-btn'+(filtreStatut<0?' active':'')+'\" onclick=\"setFiltre(-1)\">Tous</button><button class=\"filter-btn'+(filtreStatut===0?' active':'')+'\" onclick=\"setFiltre(0)\">✅ OK</button><button class=\"filter-btn'+(filtreStatut===1?' active':'')+'\" onclick=\"setFiltre(1)\">❓ Inconnu</button><button class=\"filter-btn'+(filtreStatut===2?' active':'')+'\" onclick=\"setFiltre(2)\">🚫 Bloque</button><button class=\"filter-btn'+(filtreStatut===3?' active':'')+'\" onclick=\"setFiltre(3)\">⏰ Hors horaire</button></div>';\n";
  js += "if(!historique.length)h+='<div class=\"empty-state\">Aucun historique</div>';else{h+='<div class=\"table-container\"><table><thead><tr><th>Date</th><th>Personne</th><th>Badge</th><th>Statut</th></tr></thead><tbody>';historique.forEach(e=>{let nom=e.prenom||e.nom?(e.prenom+' '+e.nom).trim():'<em>Inconnu</em>';let stCls='status-ok';if(e.statut===1)stCls='status-inconnu';else if(e.statut===2)stCls='status-bloque';else if(e.statut===3)stCls='status-horaire';else if(e.statut===4)stCls='status-ferie';else if(e.statut===5)stCls='status-forcee';h+='<tr><td>'+e.date+'</td><td>'+nom+'</td><td><span class=\"badge-display\">'+e.uid.substring(0,12)+'</span></td><td><span class=\"access-status '+stCls+'\">'+e.statutStr+'</span></td></tr>';});h+='</tbody></table></div>';}h+='</div>';document.getElementById('content').innerHTML=h;}\n";
  js += "function setFiltre(s){filtreStatut=s;renderHistorique();}\n";
  
  // Page Catégories
  js += "async function renderCategories(){categories=await API.get('/api/categories');let h='<div class=\"card\"><div class=\"flex-between\"><div class=\"card-title\">📁 Categories</div><button class=\"btn btn-primary\" onclick=\"showAddCategorie()\">+ Ajouter</button></div>';\n";
  js += "if(!categories.length)h+='<div class=\"empty-state\">Aucune categorie</div>';else{categories.forEach(c=>{h+='<div style=\"background:var(--bg);padding:.75rem;border-radius:.5rem;margin-bottom:.5rem\"><div class=\"flex-between\"><strong>'+c.nom+'</strong><div><button class=\"btn btn-sm btn-primary\" onclick=\"showEditCategorie('+c.id+')\">✏️</button> <button class=\"btn btn-sm btn-danger\" onclick=\"deleteCategorie('+c.id+')\">🗑️</button></div></div><div style=\"font-size:.75rem;color:var(--text-muted);margin-top:.25rem\">';let jActifs=c.plages.map((p,i)=>p.actif?JOURS[i].substring(0,3):null).filter(Boolean);h+=jActifs.join(', ')||'Aucun jour';h+='</div></div>';});}h+='</div>';document.getElementById('content').innerHTML=h;}\n";
  
  // Add/Edit catégorie
  js += "function showAddCategorie(){let h='<h3 style=\"margin-bottom:1rem\">Nouvelle categorie</h3><form onsubmit=\"return saveCategorie(event)\"><div class=\"form-group\"><label class=\"form-label\">Nom</label><input type=\"text\" class=\"form-input\" name=\"nom\" required></div><div class=\"form-group\"><label class=\"form-label\">Plages horaires</label><div class=\"plages-grid\">';JOURS.forEach((j,i)=>{h+='<div class=\"plage-row\"><input type=\"checkbox\" name=\"pa'+i+'\" '+(i<5?'checked':'')+'><label>'+j+'</label><input type=\"time\" name=\"hd'+i+'\" value=\"08:00\"><span>-</span><input type=\"time\" name=\"hf'+i+'\" value=\"18:00\"></div>';});h+='</div></div><button type=\"submit\" class=\"btn btn-primary\" style=\"width:100%\">Enregistrer</button></form>';showModal(h);}\n";
  
  js += "async function showEditCategorie(id){const c=categories.find(x=>x.id===id);if(!c)return;let h='<h3 style=\"margin-bottom:1rem\">Modifier '+c.nom+'</h3><form data-id=\"'+id+'\" onsubmit=\"return saveCategorie(event)\"><div class=\"form-group\"><label class=\"form-label\">Nom</label><input type=\"text\" class=\"form-input\" name=\"nom\" value=\"'+c.nom+'\" required></div><div class=\"form-group\"><label class=\"form-label\">Plages horaires</label><div class=\"plages-grid\">';JOURS.forEach((j,i)=>{const p=c.plages[i];h+='<div class=\"plage-row\"><input type=\"checkbox\" name=\"pa'+i+'\" '+(p.actif?'checked':'')+'><label>'+j+'</label><input type=\"time\" name=\"hd'+i+'\" value=\"'+String(p.hd).padStart(2,'0')+':'+String(p.md).padStart(2,'0')+'\"><span>-</span><input type=\"time\" name=\"hf'+i+'\" value=\"'+String(p.hf).padStart(2,'0')+':'+String(p.mf).padStart(2,'0')+'\"></div>';});h+='</div></div><button type=\"submit\" class=\"btn btn-primary\" style=\"width:100%\">Enregistrer</button></form>';showModal(h);}\n";
  
  js += "async function saveCategorie(e){e.preventDefault();const f=e.target,pl=[];for(let i=0;i<7;i++){const hd=f['hd'+i].value.split(':'),hf=f['hf'+i].value.split(':');pl.push({actif:f['pa'+i].checked,hd:parseInt(hd[0]),md:parseInt(hd[1]),hf:parseInt(hf[0]),mf:parseInt(hf[1])});}const d={nom:f.nom.value,plages:pl};const id=f.dataset.id;if(id){d.id=parseInt(id);await API.post('/api/categories/update',d);}else{await API.post('/api/categories',d);}hideModal();toast('Enregistre');renderCategories();return false;}\n";
  
  js += "async function deleteCategorie(id){if(!confirm('Supprimer cette categorie?'))return;const r=await API.post('/api/categories/delete',{id});if(r.error){toast(r.error,'error');}else{toast('Supprime');renderCategories();}}\n";
  
  // Page Conges
  js += "async function renderConges(){conges=await API.get('/api/conges');let h='<div class=\"card\"><div class=\"flex-between\"><div class=\"card-title\">📅 Jours feries</div><button class=\"btn btn-primary\" onclick=\"showAddConge()\">+ Ajouter</button></div>';if(!conges.length)h+='<div class=\"empty-state\">Aucun conge</div>';else{h+='<div class=\"table-container\"><table><thead><tr><th>Date</th><th>Description</th><th>Action</th></tr></thead><tbody>';conges.forEach(c=>{const d=String(c.jour).padStart(2,'0')+'/'+String(c.mois).padStart(2,'0')+(c.annee?'/'+c.annee:' (annuel)');h+='<tr><td>'+d+'</td><td>'+c.desc+'</td><td><button class=\"btn btn-sm btn-danger\" onclick=\"deleteConge('+c.id+')\">🗑️</button></td></tr>';});h+='</tbody></table></div>';}h+='</div>';document.getElementById('content').innerHTML=h;}\n";
  
  js += "function showAddConge(){let h='<h3 style=\"margin-bottom:1rem\">Ajouter jour ferie</h3><form onsubmit=\"return saveConge(event)\"><div class=\"form-group\"><label class=\"form-label\">Jour</label><input type=\"number\" class=\"form-input\" name=\"jour\" min=\"1\" max=\"31\" required></div><div class=\"form-group\"><label class=\"form-label\">Mois</label><input type=\"number\" class=\"form-input\" name=\"mois\" min=\"1\" max=\"12\" required></div><div class=\"form-group\"><label class=\"form-label\">Annee (0=annuel)</label><input type=\"number\" class=\"form-input\" name=\"annee\" value=\"0\"></div><div class=\"form-group\"><label class=\"form-label\">Description</label><input type=\"text\" class=\"form-input\" name=\"desc\" required></div><button type=\"submit\" class=\"btn btn-primary\" style=\"width:100%\">Enregistrer</button></form>';showModal(h);}\n";
  
  js += "async function saveConge(e){e.preventDefault();const f=e.target;await API.post('/api/conges',{jour:parseInt(f.jour.value),mois:parseInt(f.mois.value),annee:parseInt(f.annee.value),desc:f.desc.value});hideModal();toast('Ajoute');renderConges();return false;}\n";
  
  js += "async function deleteConge(id){if(!confirm('Supprimer?'))return;await API.post('/api/conges/delete',{id});toast('Supprime');renderConges();}\n";
  
  // Page Config
  js += "async function renderConfig(){config=await API.get('/api/config');let h='<div class=\"card\"><div class=\"card-title\">⚙️ Configuration</div><form onsubmit=\"return saveConfig(event)\"><div class=\"form-group\"><label class=\"form-label\">Titre du portail</label><input type=\"text\" class=\"form-input\" name=\"titre\" value=\"'+config.titre+'\" maxlength=\"30\"></div>';\n";
  js += "h+='<div class=\"form-group\"><label class=\"form-label\">Mode marche forcee</label><div style=\"display:flex;align-items:center;gap:1rem\"><label class=\"switch\"><input type=\"checkbox\" name=\"marcheForcee\" '+(config.marcheForcee?'checked':'')+'><span class=\"slider\"></span></label><span style=\"font-size:.85rem\">'+( config.marcheForceeActive?'⚡ Actif maintenant':'Inactif')+'</span></div></div>';\n";
  js += "h+='<div class=\"form-group\"><label class=\"form-label\">Plage horaire marche forcee</label><div style=\"display:flex;gap:.5rem;align-items:center\"><input type=\"time\" class=\"form-input\" name=\"forceeDebut\" value=\"'+String(config.forceeHD).padStart(2,'0')+':'+String(config.forceeMD).padStart(2,'0')+'\" style=\"width:auto\"><span>a</span><input type=\"time\" class=\"form-input\" name=\"forceeFin\" value=\"'+String(config.forceeHF).padStart(2,'0')+':'+String(config.forceeMF).padStart(2,'0')+'\" style=\"width:auto\"></div></div>';\n";
  js += "h+='<button type=\"submit\" class=\"btn btn-primary\">Enregistrer</button></form></div>';\n";
  js += "h+='<div class=\"card\"><div class=\"card-title\">💾 Sauvegarde</div><div style=\"display:flex;gap:.75rem;flex-wrap:wrap\"><a href=\"/api/export\" class=\"btn btn-success\" download>📥 Récupérer les données du Portail</a><br><button class=\"btn btn-warning\" onclick=\"showImport()\">📤 Charger le portail avec de nouvelles données</button></div></div>';\n";
  js += "h+='<div class=\"card\"><div class=\"card-title\">ℹ️ Systeme</div><p style=\"font-size:.85rem\">WiFi: '+config.ssid+'<br>Personnel: '+config.nbPersonnes+'/50<br>Categories: '+config.nbCategories+'<br>Memoire: '+(config.freeHeap/1024).toFixed(1)+' / '+(config.totalHeap/1024).toFixed(1)+' Ko</p></div>';document.getElementById('content').innerHTML=h;}\n";
  
  js += "async function saveConfig(e){e.preventDefault();const f=e.target;const fd=f.forceeDebut.value.split(':'),ff=f.forceeFin.value.split(':');await API.post('/api/config',{titre:f.titre.value,marcheForcee:f.marcheForcee.checked,forceeHD:parseInt(fd[0]),forceeMD:parseInt(fd[1]),forceeHF:parseInt(ff[0]),forceeMF:parseInt(ff[1])});document.getElementById('mainTitle').textContent=f.titre.value;toast('Configuration enregistree');renderConfig();return false;}\n";
  
  js += "function showImport(){let h='<h3 style=\"margin-bottom:1rem\">Importer</h3><form onsubmit=\"return doImport(event)\"><div class=\"form-group\"><label class=\"form-label\">Fichier JSON</label><input type=\"file\" class=\"form-input\" name=\"file\" accept=\".json\" required></div><p style=\"color:var(--warning);margin-bottom:1rem;font-size:.85rem\">⚠️ Donnees actuelles remplacees</p><button type=\"submit\" class=\"btn btn-warning\" style=\"width:100%\">Importer</button></form>';showModal(h);}\n";
  
  js += "async function doImport(e){e.preventDefault();const f=e.target.file.files[0],t=await f.text();try{const d=JSON.parse(t);await API.post('/api/import',d);hideModal();toast('Importe');loadPage(currentPage);}catch(err){toast('Fichier invalide','error');}return false;}\n";
  
  // Démarrage
  js += "loadPage('accueil');startPolling();\n";
  js += "</script>\n";
  return js;
}
