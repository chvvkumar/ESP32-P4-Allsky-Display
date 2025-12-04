#pragma once
#ifndef WEB_CONFIG_HTML_H
#define WEB_CONFIG_HTML_H

#include <Arduino.h>

// Store large HTML/CSS/JavaScript blocks in PROGMEM to save RAM and reduce code size
// This file contains all the static web assets that were previously in web_config.cpp

const char HTML_CSS[] PROGMEM = R"rawliteral(
@import url('https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0-beta3/css/all.min.css');
@import url('https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500;700&display=swap');
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Roboto',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background-color:#0f172a;color:#f8fafc;min-height:100vh;line-height:1.6;display:flex;flex-direction:column}
.modal{display:none;position:fixed;z-index:1000;left:0;top:0;width:100%;height:100%;background-color:rgba(15,23,42,0.8);backdrop-filter:blur(4px)}
.modal.show{display:flex;align-items:center;justify-content:center;animation:fadeIn 0.2s ease}
@keyframes fadeIn{from{opacity:0}to{opacity:1}}
.modal-content{background:#1e293b;border:1px solid #334155;border-radius:16px;padding:2rem;max-width:500px;box-shadow:0 25px 50px -12px rgba(0,0,0,0.5);animation:slideUp 0.3s ease}
@keyframes slideUp{from{transform:translateY(20px);opacity:0}to{transform:translateY(0);opacity:1}}
.modal-header{display:flex;align-items:center;gap:1rem;margin-bottom:1.5rem;color:#f1f5f9;border-bottom:1px solid #334155;padding-bottom:1rem}
.modal-title{font-size:1.5rem;font-weight:bold;flex:1}
.modal-close{background:none;border:none;color:#94a3b8;font-size:1.5rem;cursor:pointer;padding:0;width:2rem;height:2rem;display:flex;align-items:center;justify-content:center;border-radius:8px;transition:all 0.2s}
.modal-close:hover{background:rgba(239,68,68,0.1);color:#f8fafc}
.modal-body{margin-bottom:1.5rem;color:#cbd5e1;line-height:1.6}
.modal-footer{display:flex;gap:1rem;justify-content:flex-end}
.modal-btn{padding:0.75rem 1.5rem;border:none;border-radius:8px;font-weight:500;cursor:pointer;transition:all 0.2s;font-size:0.95rem;letter-spacing:0.5px}
.modal-btn-confirm{background:#0ea5e9;color:white}
.modal-btn-confirm:hover{background:#0284c7;transform:translateY(-1px);box-shadow:0 4px 12px rgba(14,165,233,0.4)}
.modal-btn-cancel{background:#475569;color:white}
.modal-btn-cancel:hover{background:#334155}
.modal-success{border-left:4px solid #10b981}
.modal-error{border-left:4px solid #ef4444}
.modal-warning{border-left:4px solid #f59e0b}
.header{background:#1e293b;padding:1rem 0;box-shadow:0 4px 6px -1px rgba(0,0,0,0.3);border-bottom:1px solid #334155}
.container{max-width:1200px;margin:0 auto;padding:0 1rem}
.header-content{display:flex;justify-content:space-between;align-items:center;color:#f8fafc;flex-wrap:wrap;gap:1rem}
.logo{font-size:1.5rem;font-weight:bold;color:#38bdf8;letter-spacing:-0.5px}
.status-badges{display:flex;gap:0.5rem;flex-wrap:wrap;align-items:center}
.badge{padding:0.35rem 0.85rem;border-radius:9999px;font-size:0.75rem;font-weight:600;letter-spacing:0.5px;text-transform:uppercase}
.badge.success{background:#059669;color:#ecfdf5}
.badge.error{background:#dc2626;color:#fef2f2}
.badge.warning{background:#d97706;color:#fffbeb}
.github-link{display:inline-flex;align-items:center;padding:0.5rem 0.9rem;background:#334155;color:#e2e8f0;border-radius:8px;text-decoration:none;font-size:0.85rem;border:1px solid #475569;transition:all 0.2s ease;white-space:nowrap;font-weight:500}
.github-link:hover{background:#475569;border-color:#64748b;transform:translateY(-1px);box-shadow:0 2px 8px rgba(0,0,0,0.3)}
.github-link .github-icon{font-family:'Font Awesome 6 Brands';margin-right:0.4rem}
.nav{background:#0f172a;padding:0;border-bottom:1px solid #334155;position:sticky;top:0;z-index:100;backdrop-filter:blur(8px);background:rgba(15,23,42,0.95)}
.nav-content{display:flex;gap:0.5rem;overflow-x:auto;padding:0.5rem 0}
.nav-item{padding:0.75rem 1.25rem;border-radius:8px;text-decoration:none;color:#94a3b8;white-space:nowrap;transition:all 0.2s ease;font-weight:500;font-size:0.95rem}
.nav-item:hover{background:#1e293b;color:#38bdf8}
.nav-item.active{background:#1e293b;color:#38bdf8;box-shadow:inset 0 -2px 0 #38bdf8}
.main{padding:2rem 0;flex:1}
.card{background:#1e293b;border:1px solid #334155;border-radius:12px;padding:1.5rem;margin-bottom:1.5rem;box-shadow:0 4px 6px -1px rgba(0,0,0,0.3);transition:transform 0.2s ease,box-shadow 0.2s ease;display:flex;flex-direction:column;height:100%}
.card:hover{transform:translateY(-2px);box-shadow:0 10px 15px -3px rgba(0,0,0,0.4);border-color:#475569}
.card h2{margin-bottom:1.25rem;color:#f8fafc;display:flex;align-items:center;gap:0.75rem;font-weight:600;font-size:1.25rem;border-bottom:1px solid #334155;padding-bottom:0.75rem}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:1.5rem}
.form-group{margin-bottom:1.25rem}
.form-group label{display:block;margin-bottom:0.5rem;font-weight:500;color:#cbd5e1;font-size:0.9rem}
.form-control{width:100%;padding:0.75rem;border:1px solid #475569;border-radius:8px;font-size:1rem;background:#334155;color:#f8fafc;transition:border-color 0.2s ease,box-shadow 0.2s ease}
.form-control:focus{outline:none;border-color:#38bdf8;box-shadow:0 0 0 3px rgba(56,189,248,0.2);background:#1e293b}
.form-control::placeholder{color:#64748b}
.btn{display:inline-flex;align-items:center;justify-content:center;padding:0.75rem 1.5rem;border:none;border-radius:8px;text-decoration:none;font-weight:500;cursor:pointer;transition:all 0.2s ease;font-size:0.95rem;letter-spacing:0.3px}
.btn-primary{background:#0ea5e9;color:white}
.btn-primary:hover{background:#0284c7;transform:translateY(-1px);box-shadow:0 4px 12px rgba(14,165,233,0.3)}
.btn-success{background:#10b981;color:white}
.btn-success:hover{background:#059669;transform:translateY(-1px);box-shadow:0 4px 12px rgba(16,185,129,0.3)}
.btn-danger{background:#ef4444;color:white}
.btn-danger:hover{background:#dc2626;transform:translateY(-1px);box-shadow:0 4px 12px rgba(239,68,68,0.3)}
.btn-secondary{background:#475569;color:white}
.btn-secondary:hover{background:#334155;transform:translateY(-1px)}
.status-indicator{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:0.75rem}
@keyframes pulse-green{0%{box-shadow:0 0 0 0 rgba(16,185,129,0.7)}70%{box-shadow:0 0 0 6px rgba(16,185,129,0)}100%{box-shadow:0 0 0 0 rgba(16,185,129,0)}}
.status-online{background:#10b981;animation:pulse-green 2s infinite}
.status-offline{background:#ef4444;box-shadow:0 0 10px rgba(239,68,68,0.5)}
.status-warning{background:#f59e0b;box-shadow:0 0 10px rgba(245,158,11,0.5)}
.progress{background:#334155;border-radius:9999px;height:12px;overflow:hidden;border:none}
.progress-bar{background:linear-gradient(90deg,#38bdf8,#0ea5e9);height:100%;transition:width 0.3s ease}
.stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:1rem;margin-bottom:2rem}
.stat-card{background:#1e293b;padding:1.5rem;border-radius:12px;text-align:center;color:#f8fafc;border:1px solid #334155;transition:transform 0.2s ease;position:relative;overflow:hidden}
.stat-card:hover{transform:translateY(-2px);border-color:#475569;box-shadow:0 10px 15px -3px rgba(0,0,0,0.3)}
.stat-value{font-size:2.25rem;font-weight:700;margin-bottom:0.25rem;color:#38bdf8;letter-spacing:-1px;position:relative;z-index:2}
.stat-label{font-size:0.875rem;font-weight:500;color:#94a3b8;text-transform:uppercase;letter-spacing:0.5px;position:relative;z-index:2}
.stat-icon{position:absolute;right:10px;bottom:0px;font-size:4rem;opacity:0.05;color:#f8fafc;z-index:1;transform:rotate(-15deg)}
.footer{text-align:center;padding:2rem 1rem;color:#64748b;border-top:1px solid #334155;margin-top:auto;font-size:0.9rem}
@media(max-width:768px){.header-content{flex-direction:column;gap:1rem;text-align:center}.nav-content{justify-content:flex-start}.grid{grid-template-columns:1fr}}
.error{border-left:4px solid #ef4444}.warning{border-left:4px solid #f59e0b}.success{border-left:4px solid #10b981}
.image-source-item{background:#0f172a !important;border:1px solid #334155 !important;padding:1.25rem !important}
.transform-section{background:#1e293b !important;border:1px dashed #475569 !important}
)rawliteral";

const char HTML_JAVASCRIPT[] PROGMEM = R"rawliteral(
function showButtonFeedback(btn,type,message){if(!btn)return;const originalContent=btn.getAttribute('data-original-content')||btn.innerHTML;if(!btn.getAttribute('data-original-content')){btn.setAttribute('data-original-content',originalContent)}const originalClass=btn.getAttribute('data-original-class')||btn.className;if(!btn.getAttribute('data-original-class')){btn.setAttribute('data-original-class',originalClass)}if(type==='loading'){btn.disabled=true;btn.innerHTML='<i class="fas fa-circle-notch fa-spin"></i> '+(message||'Working...');return}btn.disabled=false;if(type==='success'){btn.className='btn btn-success';btn.innerHTML='<i class="fas fa-check"></i> '+(message||'Saved')}else{btn.className='btn btn-danger';btn.innerHTML='<i class="fas fa-exclamation-triangle"></i> '+(message||'Error')}setTimeout(()=>{btn.innerHTML=originalContent;btn.className=originalClass;btn.disabled=false},2000)}
function showInputFeedback(input,type){const originalBorder=input.style.borderColor;if(type==='success'){input.style.borderColor='#10b981';input.style.boxShadow='0 0 0 3px rgba(16, 185, 129, 0.2)'}else{input.style.borderColor='#ef4444';input.style.boxShadow='0 0 0 3px rgba(239, 68, 68, 0.2)'}setTimeout(()=>{input.style.borderColor='';input.style.boxShadow=''},2000)}
function showModal(title,message,type='info'){const modal=document.getElementById('modal');const modalTitle=document.querySelector('.modal-title');const modalBody=document.querySelector('.modal-body');const modalContent=document.querySelector('.modal-content');if(!modal||!modalTitle||!modalBody||!modalContent)return;modalTitle.innerHTML=title;modalBody.innerHTML=message;modalContent.className='modal-content';if(type==='success')modalContent.classList.add('modal-success');else if(type==='error')modalContent.classList.add('modal-error');else if(type==='warning')modalContent.classList.add('modal-warning');modal.classList.add('show')}
function closeModal(){const modal=document.getElementById('modal');if(modal)modal.classList.remove('show')}
function showConfirmModal(title,message,onConfirm,onCancel){const modal=document.getElementById('confirmModal');const modalTitle=document.querySelector('#confirmModal .modal-title');const modalBody=document.querySelector('#confirmModal .modal-body');const confirmBtn=document.querySelector('#confirmModal .modal-btn-confirm');if(!modal||!modalTitle||!modalBody||!confirmBtn)return;modalTitle.innerHTML=title;modalBody.innerHTML=message;modal.classList.add('show');confirmBtn.onclick=()=>{modal.classList.remove('show');if(onConfirm)onConfirm()};const cancelBtn=document.querySelector('#confirmModal .modal-btn-cancel');if(cancelBtn)cancelBtn.onclick=()=>{modal.classList.remove('show');if(onCancel)onCancel()};const closeBtn=document.querySelector('#confirmModal .modal-close');if(closeBtn)closeBtn.onclick=()=>{modal.classList.remove('show');if(onCancel)onCancel()}}
document.addEventListener('DOMContentLoaded',function(){const closeButtons=document.querySelectorAll('.modal-close');closeButtons.forEach(btn=>{btn.onclick=()=>closeModal()})});
function submitForm(formId,endpoint){const form=document.getElementById(formId);if(!form)return;form.addEventListener('submit',function(e){e.preventDefault();const btn=form.querySelector('button[type="submit"]');if(btn)showButtonFeedback(btn,'loading','Saving...');const formData=new FormData(form);fetch(endpoint,{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){if(btn)showButtonFeedback(btn,'success','Saved!');if(data.willRestart){showModal('✓ Restarting',data.message,'success');setTimeout(()=>location.reload(),10000)}}else{if(btn)showButtonFeedback(btn,'error','Error: '+data.message)}}).catch(error=>{if(btn)showButtonFeedback(btn,'error','Failed')})})}
document.addEventListener('DOMContentLoaded',function(){submitForm('networkForm','/api/save');submitForm('mqttForm','/api/save');submitForm('imageForm','/api/save');submitForm('cyclingForm','/api/save');submitForm('displayForm','/api/save');submitForm('advancedForm','/api/save')});
function updateBrightnessValue(value){document.getElementById('brightnessValue').textContent=value}
function updateMainBrightnessValue(value){document.getElementById('mainBrightnessValue').textContent=value}
function updateBrightnessMode(isAuto){const slider=document.getElementById('main_brightness');const container=document.getElementById('brightness_slider_container');const saveButton=document.getElementById('save_brightness_btn');const checkbox=document.getElementById('brightness_auto_mode');if(isAuto){slider.disabled=true;saveButton.disabled=true;container.style.opacity='0.5'}else{slider.disabled=false;saveButton.disabled=false;container.style.opacity='1'}const formData=new FormData();formData.append('brightness_auto_mode',isAuto?'on':'');fetch('/api/save',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status!=='success'){showInputFeedback(checkbox,'error')}})}
function saveMainBrightness(btn){showButtonFeedback(btn,'loading','Applying...');const value=document.getElementById('main_brightness').value;const formData=new FormData();formData.append('default_brightness',value);fetch('/api/save',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Applied!')}else{showButtonFeedback(btn,'error','Error')}}).catch(e=>{showButtonFeedback(btn,'error','Failed')})}
function restart(){showConfirmModal('🔄 Restart Device','Are you sure you want to restart the device?',()=>{fetch('/api/restart',{method:'POST'}).then(()=>{showModal('✓ Restarting','Device is restarting...','success');setTimeout(()=>location.reload(),10000)})})}
function factoryReset(){showConfirmModal('🏭 Factory Reset','Are you sure you want to reset to factory defaults? This cannot be undone!',()=>{showConfirmModal('⚠️ Confirm Factory Reset','This will erase ALL your settings. Are you absolutely sure?',()=>{fetch('/api/factory-reset',{method:'POST'}).then(()=>{showModal('✓ Factory Reset','Device will restart now...','success');setTimeout(()=>location.reload(),5000)})})})}
function addImageSource(btn){const url=prompt('Enter image URL:');if(url&&url.trim()){showButtonFeedback(btn,'loading','Adding...');const formData=new FormData();formData.append('url',url.trim());fetch('/api/add-source',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Added!');setTimeout(()=>location.reload(),1000)}else{showButtonFeedback(btn,'error','Error: '+data.message)}}).catch(error=>{showButtonFeedback(btn,'error','Failed')})}}
function removeImageSource(index,btn){showConfirmModal('🗑️ Remove Image Source','Are you sure you want to remove this image source?',()=>{showButtonFeedback(btn,'loading','Removing...');const formData=new FormData();formData.append('index',index);fetch('/api/remove-source',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Removed!');setTimeout(()=>location.reload(),1000)}else{showButtonFeedback(btn,'error','Error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed')})})}
function updateImageSource(index,input){const url=input.value;const formData=new FormData();formData.append('index',index);formData.append('url',url);const errorDiv=document.getElementById('imageError_'+index);if(errorDiv)errorDiv.style.display='none';fetch('/api/update-source',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status!=='success'){showInputFeedback(input,'error');if(data.message&&errorDiv){errorDiv.textContent='⚠️ '+data.message;errorDiv.style.display='block'}}else{showInputFeedback(input,'success');if(data.warning&&errorDiv){errorDiv.textContent='⚠️ WARNING: '+data.warning;errorDiv.style.display='block';errorDiv.style.borderLeftColor='#f59e0b';errorDiv.style.backgroundColor='rgba(245,158,11,0.1)'}}}).catch(error=>{showInputFeedback(input,'error')})}
function clearAllSources(btn){showConfirmModal('🗑️ Clear All Sources','Are you sure you want to clear all image sources? This will reset to a single default source.',()=>{showButtonFeedback(btn,'loading','Clearing...');fetch('/api/clear-sources',{method:'POST'}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Cleared!');setTimeout(()=>location.reload(),1000)}else{showButtonFeedback(btn,'error','Error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed')})})}
function toggleTransformSection(index){const section=document.getElementById('transformSection_'+index);if(section){section.style.display=section.style.display==='none'?'block':'none'}}
function updateImageTransform(index,property,input){const value=input.value;const formData=new FormData();formData.append('index',index);formData.append('property',property);formData.append('value',value);fetch('/api/update-transform',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status!=='success'){showInputFeedback(input,'error')}else{showInputFeedback(input,'success')}}).catch(error=>{showInputFeedback(input,'error')})}
function copyDefaultsToImage(index,btn){showConfirmModal('📋 Copy Global Defaults','Are you sure you want to copy global default transformation settings to this image?',()=>{showButtonFeedback(btn,'loading','Copying...');const formData=new FormData();formData.append('index',index);fetch('/api/copy-defaults',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Copied!');setTimeout(()=>location.reload(),1000)}else{showButtonFeedback(btn,'error','Error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed')})})}
function applyTransformImmediately(index,btn){showConfirmModal('⚙️ Apply Transform','Apply these transformation settings immediately?',()=>{showButtonFeedback(btn,'loading','Applying...');const formData=new FormData();formData.append('index',index);fetch('/api/apply-transform',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Applied!')}else{showButtonFeedback(btn,'error','Error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed')})})}
function nextImage(btn){showButtonFeedback(btn,'loading','Switching...');fetch('/api/next-image',{method:'POST'}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Switched!');setTimeout(()=>location.reload(),2000)}else{showButtonFeedback(btn,'error','Error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed')})}
)rawliteral";

const char HTML_MODALS[] PROGMEM = R"rawliteral(
<div id='modal' class='modal'><div class='modal-content'>
<div class='modal-header'><h2 class='modal-title'></h2><button class='modal-close' onclick='closeModal()'>&times;</button></div>
<div class='modal-body'></div>
<div class='modal-footer'><button class='modal-btn modal-btn-confirm' onclick='closeModal()'>OK</button></div>
</div></div>
<div id='confirmModal' class='modal'><div class='modal-content'>
<div class='modal-header'><h2 class='modal-title'></h2><button class='modal-close' onclick='closeModal()'>&times;</button></div>
<div class='modal-body'></div>
<div class='modal-footer'><button class='modal-btn modal-btn-cancel'>Cancel</button><button class='modal-btn modal-btn-confirm'>Confirm</button></div>
</div></div>
)rawliteral";

#endif // WEB_CONFIG_HTML_H
