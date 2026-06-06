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
body{font-family:'Roboto',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background-color:#0f172a;color:#f8fafc;min-height:100vh;line-height:1.6;display:flex;flex-direction:column;overflow-x:hidden}
::-webkit-scrollbar{width:8px;height:8px}
::-webkit-scrollbar-track{background:#1e293b}
::-webkit-scrollbar-thumb{background:#475569;border-radius:4px}
::-webkit-scrollbar-thumb:hover{background:#64748b}
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
.nav-content{display:flex;gap:0.35rem;padding:0.5rem 0;justify-content:center;overflow-x:auto;flex-wrap:nowrap}
.nav-item{padding:0.6rem 0.9rem;border-radius:8px;text-decoration:none;color:#94a3b8;white-space:nowrap;transition:all 0.2s ease;font-weight:500;font-size:0.875rem;flex:0 0 auto}
.nav-item:hover{background:#1e293b;color:#38bdf8}
.nav-item.active{background:#1e293b;color:#38bdf8;box-shadow:inset 0 -2px 0 #38bdf8}
.main{padding:2rem 0;flex:1}
.card{background:#1e293b;border:1px solid #334155;border-radius:12px;padding:1.5rem;margin-bottom:1.5rem;box-shadow:0 4px 6px -1px rgba(0,0,0,0.3);transition:transform 0.2s ease,box-shadow 0.2s ease;display:flex;flex-direction:column;height:100%}
.card:hover{transform:translateY(-2px);box-shadow:0 10px 15px -3px rgba(0,0,0,0.4);border-color:#475569}
.card h2{margin-bottom:1.25rem;color:#f8fafc;display:flex;align-items:center;gap:0.75rem;font-weight:600;font-size:1.25rem;border-bottom:1px solid #334155;padding-bottom:0.75rem}
.help-icon{display:inline-flex;align-items:center;justify-content:center;color:#64748b;font-size:0.85rem;text-decoration:none;transition:color 0.2s ease,transform 0.2s ease;margin-left:0.25rem;opacity:0.6}
.help-icon:hover{color:#38bdf8;opacity:1;transform:scale(1.15)}
.help-icon i{font-size:0.95rem}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:1.5rem}
.form-group{margin-bottom:0.5rem}
.form-group label{display:block;margin-bottom:0.25rem;font-weight:500;color:#cbd5e1;font-size:0.85rem}
.form-control{width:100%;padding:0.5rem 0.75rem;border:1px solid #475569;border-radius:8px;font-size:0.95rem;background:#334155;color:#f8fafc;transition:border-color 0.2s ease,box-shadow 0.2s ease}
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
.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:1rem;margin-bottom:2rem}
.stat-card{background:#1e293b;padding:1.5rem;border-radius:12px;text-align:center;color:#f8fafc;border:1px solid #334155;transition:transform 0.2s ease;position:relative;overflow:hidden}
.stat-card:hover{transform:translateY(-2px);border-color:#475569;box-shadow:0 10px 15px -3px rgba(0,0,0,0.3)}
.stat-value{font-size:2.25rem;font-weight:700;margin-bottom:0.25rem;color:#38bdf8;letter-spacing:-1px;position:relative;z-index:2}
.stat-label{font-size:0.875rem;font-weight:500;color:#94a3b8;text-transform:uppercase;letter-spacing:0.5px;position:relative;z-index:2}
.stat-icon{position:absolute;right:10px;bottom:0px;font-size:4rem;opacity:0.05;color:#f8fafc;z-index:1;transform:rotate(-15deg)}
.footer{text-align:center;padding:2rem 1rem;color:#64748b;border-top:1px solid #334155;margin-top:auto;font-size:0.9rem}
@media(max-width:1024px){.stats{grid-template-columns:repeat(2,1fr)}}
@media(max-width:768px){.header-content{flex-direction:column;gap:1rem;text-align:center}.nav-item{padding:0.5rem 0.7rem;font-size:0.8rem}.grid{grid-template-columns:1fr}}
@media(max-width:600px){.nav-item{padding:0.4rem 0.5rem;font-size:0.75rem}.stats{grid-template-columns:1fr}}
.error{border-left:4px solid #ef4444}.warning{border-left:4px solid #f59e0b}.success{border-left:4px solid #10b981}
.image-source-item{background:#0f172a !important;border:1px solid #334155 !important;padding:1.25rem !important}
.transform-section{background:#1e293b !important;border:1px dashed #475569 !important}
.toast{position:fixed;top:20px;right:20px;background:#1e293b;color:#fff;padding:1rem 1.5rem;border-radius:8px;box-shadow:0 4px 12px rgba(0,0,0,0.5);z-index:10000;min-width:300px;max-width:500px;border-left:4px solid #38bdf8;animation:slideInRight 0.3s ease,fadeOut 0.3s ease 2.7s;display:flex;align-items:center;gap:0.75rem}
@keyframes slideInRight{from{transform:translateX(400px);opacity:0}to{transform:translateX(0);opacity:1}}
@keyframes fadeOut{to{opacity:0;transform:translateX(400px)}}
.toast.success{border-left-color:#10b981;background:#1e293b}
.toast.error{border-left-color:#ef4444;background:#1e293b}
.toast.warning{border-left-color:#f59e0b;background:#1e293b}
.toast.info{border-left-color:#38bdf8;background:#1e293b}
.toast-icon{font-size:1.5rem;flex-shrink:0}
.toast-content{flex:1}
.toast-message{margin:0;font-weight:500;color:#f8fafc}
.toast-close{background:none;border:none;color:#94a3b8;cursor:pointer;padding:0;font-size:1.2rem;transition:color 0.2s}
.toast-close:hover{color:#f8fafc}
.spinner{border:4px solid #334155;border-top-color:#38bdf8;border-radius:50%;width:50px;height:50px;animation:spin 1s linear infinite;margin:0 auto}
@keyframes spin{to{transform:rotate(360deg)}}
.loading-overlay{position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.8);display:flex;align-items:center;justify-content:center;z-index:9999;backdrop-filter:blur(4px)}
.loading-content{background:#1e293b;padding:2rem;border-radius:12px;text-align:center;color:#f8fafc;min-width:300px;border:1px solid #334155;box-shadow:0 25px 50px -12px rgba(0,0,0,0.5)}
.loading-text{margin:1rem 0 0;font-size:1.1rem;color:#cbd5e1}
*:focus{outline:2px solid #38bdf8;outline-offset:2px}
.btn:focus,.form-control:focus{box-shadow:0 0 0 3px rgba(56,189,248,0.3)}
.nav-toggle{display:none;background:none;border:none;color:#f8fafc;font-size:1.5rem;cursor:pointer;padding:0.5rem}
@media (max-width:768px){.nav-toggle{display:block;position:absolute;right:1rem;top:50%;transform:translateY(-50%)}.nav-content{display:none;flex-direction:column;position:absolute;top:100%;left:0;right:0;background:#0f172a;padding:0;box-shadow:0 4px 6px rgba(0,0,0,0.3);border-top:1px solid #334155}.nav-content.active{display:flex}.nav-item{padding:1rem;width:100%;text-align:left;border-bottom:1px solid #1e293b}.nav{position:relative}}
:root{--accent:#38bdf8;--accent-strong:#0ea5e9;--sel-border:#3b82f6;--sel-bg:#1e3a5f;--surface:#1e293b;--sunken:#0f172a;--border:#334155;--text:#f8fafc;--muted:#94a3b8;--dim:#64748b;--ok:#10b981;--warn:#f59e0b;--danger:#ef4444;--tap:44px;--radius:8px}
.img-toolbar{display:flex;flex-direction:column;gap:0.75rem;margin-bottom:1rem}
.img-add-bar{display:flex;flex-wrap:wrap;gap:0.5rem;align-items:center}
.img-add-bar .form-control{flex:1 1 220px;min-width:0;min-height:var(--tap)}
.img-add-bar .btn{min-height:var(--tap)}
.img-moon-box{background:var(--sunken);border:1px solid var(--border);border-radius:var(--radius);padding:0.75rem}
.img-moon-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:0.5rem;margin:0.5rem 0}
.img-bulk-bar{display:flex;flex-wrap:wrap;gap:0.75rem;align-items:center;padding:0.5rem 0;border-top:1px solid var(--border)}
.img-bulk-bar label{display:inline-flex;align-items:center;gap:0.4rem;color:var(--muted);font-size:0.85rem;min-height:var(--tap)}
.img-list{display:flex;flex-direction:column;gap:0.5rem}
.img-row{display:flex;flex-wrap:wrap;gap:0.5rem;padding:0.75rem;border:1px solid var(--border);border-radius:var(--radius);background:var(--surface)}
.img-row.is-active{border-color:var(--sel-border);background:var(--sel-bg)}
.img-row.is-disabled .img-row-meta,.img-row.is-disabled .img-idx{opacity:0.5}
.img-row-main{display:flex;flex-wrap:wrap;gap:0.5rem;align-items:center;width:100%}
.img-row-main .form-control{flex:1 1 200px;min-width:0;min-height:var(--tap)}
.img-idx{color:var(--muted);font-weight:600;min-width:1.5rem}
.img-row-meta{display:flex;flex-wrap:wrap;gap:0.75rem;align-items:center;width:100%;color:var(--muted);font-size:0.8rem}
.img-row-meta .form-control{width:5.5rem;min-height:36px;padding:0.3rem 0.5rem}
.img-summary{font-family:monospace;color:var(--muted)}
.img-toggle-btn{min-height:var(--tap);min-width:var(--tap);padding:0.4rem 0.7rem;display:inline-flex;align-items:center;gap:0.4rem;border:1px solid var(--border);border-radius:var(--radius);background:var(--sunken);color:var(--text);cursor:pointer;font-size:0.85rem}
.img-toggle-btn[aria-pressed="false"]{color:var(--dim)}
.img-caret{min-height:var(--tap);min-width:var(--tap);background:var(--sunken);border:1px solid var(--border);border-radius:var(--radius);color:var(--text);cursor:pointer}
.img-drawer{display:none;width:100%;background:var(--sunken);border-radius:var(--radius);padding:0.75rem;margin-top:0.25rem}
.img-drawer.is-open{display:block}
.transform-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:0.6rem}
.transform-field{display:flex;flex-direction:column;gap:0.2rem}
.transform-field label{color:var(--muted);font-size:0.85rem}
.transform-field .form-control{min-height:var(--tap)}
.img-drawer-actions{display:flex;flex-wrap:wrap;gap:0.5rem;margin-top:0.75rem;align-items:center}
.img-drawer-actions .btn{min-height:var(--tap)}
.status-pill{display:inline-flex;align-items:center;gap:0.4rem;padding:0.3rem 0.75rem;border-radius:9999px;font-size:0.8rem;font-weight:600}
.status-pill--active{background:rgba(16,185,129,0.15);color:var(--ok)}
.status-pill--paused{background:rgba(245,158,11,0.15);color:var(--warn)}
.img-save-bar{position:sticky;bottom:0;display:flex;flex-wrap:wrap;gap:0.75rem;align-items:center;margin-top:1rem;padding:0.75rem;background:var(--surface);border:1px solid var(--border);border-radius:var(--radius)}
.img-save-bar .btn{min-height:var(--tap)}
.img-dirty-dot{display:inline-flex;align-items:center;gap:0.4rem;color:var(--warn);font-size:0.85rem}
.img-collapse-head{cursor:pointer;user-select:none}
.img-collapse-body{display:none;margin-top:1rem}
.img-collapse-body.is-open{display:block}
.img-note{color:var(--dim);font-size:0.8rem;margin-bottom:0.5rem}
.img-count{color:var(--muted);font-weight:400;font-size:0.95rem;margin-left:0.5rem}
)rawliteral";

const char HTML_JAVASCRIPT[] PROGMEM = R"rawliteral(
function showToast(message,type='info'){const icons={success:'fa-check-circle',error:'fa-exclamation-circle',warning:'fa-exclamation-triangle',info:'fa-info-circle'};const colors={success:'#10b981',error:'#ef4444',warning:'#f59e0b',info:'#38bdf8'};const toast=document.createElement('div');toast.className='toast '+type;toast.innerHTML='<i class="fas '+icons[type]+' toast-icon"></i><div class="toast-content"><p class="toast-message">'+message+'</p></div><button class="toast-close" onclick="this.parentElement.remove()">&times;</button>';document.body.appendChild(toast);setTimeout(()=>toast.remove(),3000)}
function showLoading(message='Working...'){const overlay=document.createElement('div');overlay.className='loading-overlay';overlay.id='loadingOverlay';overlay.innerHTML='<div class="loading-content"><div class="spinner"></div><p class="loading-text">'+message+'</p></div>';document.body.appendChild(overlay);return overlay}
function hideLoading(){const overlay=document.getElementById('loadingOverlay');if(overlay)overlay.remove()}
function showButtonFeedback(btn,type,message){if(!btn)return;const originalContent=btn.getAttribute('data-original-content')||btn.innerHTML;if(!btn.getAttribute('data-original-content')){btn.setAttribute('data-original-content',originalContent)}const originalClass=btn.getAttribute('data-original-class')||btn.className;if(!btn.getAttribute('data-original-class')){btn.setAttribute('data-original-class',originalClass)}if(type==='loading'){btn.disabled=true;btn.innerHTML='<i class="fas fa-circle-notch fa-spin"></i> '+(message||'Working...');return}btn.disabled=false;if(type==='success'){btn.className='btn btn-success';btn.innerHTML='<i class="fas fa-check"></i> '+(message||'Saved')}else{btn.className='btn btn-danger';btn.innerHTML='<i class="fas fa-exclamation-triangle"></i> '+(message||'Error')}setTimeout(()=>{btn.innerHTML=originalContent;btn.className=originalClass;btn.disabled=false},2000)}
function showInputFeedback(input,type){const originalBorder=input.style.borderColor;if(type==='success'){input.style.borderColor='#10b981';input.style.boxShadow='0 0 0 3px rgba(16, 185, 129, 0.2)'}else{input.style.borderColor='#ef4444';input.style.boxShadow='0 0 0 3px rgba(239, 68, 68, 0.2)'}setTimeout(()=>{input.style.borderColor='';input.style.boxShadow=''},2000)}
function showModal(title,message,type='info'){const modal=document.getElementById('modal');const modalTitle=document.querySelector('.modal-title');const modalBody=document.querySelector('.modal-body');const modalContent=document.querySelector('.modal-content');if(!modal||!modalTitle||!modalBody||!modalContent)return;modalTitle.innerHTML=title;modalBody.innerHTML=message;modalContent.className='modal-content';if(type==='success')modalContent.classList.add('modal-success');else if(type==='error')modalContent.classList.add('modal-error');else if(type==='warning')modalContent.classList.add('modal-warning');modal.classList.add('show')}
function closeModal(){const modal=document.getElementById('modal');if(modal)modal.classList.remove('show')}
function toggleNav(){const nav=document.querySelector('.nav-content');if(nav)nav.classList.toggle('active')}
function validateImageUrl(input){const url=input.value.trim();if(!url){showInputFeedback(input,'error');return false}if(!url.match(/^https?:\/\/.+/i)){showInputFeedback(input,'error');showToast('URL must start with http:// or https://','error');return false}return true}
function validateRequired(input){if(!input.value.trim()){showInputFeedback(input,'error');showToast('This field is required','error');return false}return true}
function validateNumber(input,min,max){const val=parseFloat(input.value);if(isNaN(val)){showInputFeedback(input,'error');showToast('Please enter a valid number','error');return false}if(min!==undefined&&val<min){showInputFeedback(input,'error');showToast('Value must be at least '+min,'error');return false}if(max!==undefined&&val>max){showInputFeedback(input,'error');showToast('Value must be at most '+max,'error');return false}return true}
function showConfirmModal(title,message,onConfirm,onCancel){const modal=document.getElementById('confirmModal');const modalTitle=document.querySelector('#confirmModal .modal-title');const modalBody=document.querySelector('#confirmModal .modal-body');const confirmBtn=document.querySelector('#confirmModal .modal-btn-confirm');if(!modal||!modalTitle||!modalBody||!confirmBtn)return;modalTitle.innerHTML=title;modalBody.innerHTML=message;modal.classList.add('show');confirmBtn.onclick=()=>{modal.classList.remove('show');if(onConfirm)onConfirm()};const cancelBtn=document.querySelector('#confirmModal .modal-btn-cancel');if(cancelBtn)cancelBtn.onclick=()=>{modal.classList.remove('show');if(onCancel)onCancel()};const closeBtn=document.querySelector('#confirmModal .modal-close');if(closeBtn)closeBtn.onclick=()=>{modal.classList.remove('show');if(onCancel)onCancel()}}
document.addEventListener('DOMContentLoaded',function(){const closeButtons=document.querySelectorAll('.modal-close');closeButtons.forEach(btn=>{btn.onclick=()=>closeModal()})});
function submitForm(formId,endpoint){const form=document.getElementById(formId);if(!form)return;form.addEventListener('submit',function(e){e.preventDefault();const btn=form.querySelector('button[type="submit"]');if(btn)showButtonFeedback(btn,'loading','Saving...');const formData=new FormData(form);fetch(endpoint,{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){if(btn)showButtonFeedback(btn,'success','Saved!');showToast('Settings saved successfully','success');if(data.willRestart||data.needsRestart){showModal('✓ Restarting',data.message,'success');setTimeout(()=>{fetch('/api/restart',{method:'POST'}).then(()=>location.reload()).catch(()=>location.reload())},2000)}}else{if(btn)showButtonFeedback(btn,'error','Error');showToast('Error: '+data.message,'error')}}).catch(error=>{if(btn)showButtonFeedback(btn,'error','Failed');showToast('Failed to save settings','error')})})}
document.addEventListener('DOMContentLoaded',function(){submitForm('networkForm','/api/save');submitForm('mqttForm','/api/save');submitForm('imageForm','/api/save');submitForm('displayForm','/api/save');submitForm('advancedForm','/api/save');if(typeof initBrightnessModeUI==='function')initBrightnessModeUI();document.addEventListener('keydown',function(e){if(e.ctrlKey||e.metaKey){switch(e.key){case 's':e.preventDefault();const form=document.querySelector('form');if(form)form.requestSubmit();showToast('Saving...','info');break;case 'r':e.preventDefault();if(confirm('Restart device?'))restart();break;case 'n':e.preventDefault();const nextBtn=document.querySelector('[onclick*=\"nextImage\"]');if(nextBtn)nextBtn.click();break}}});if(window.location.pathname==='/'||window.location.pathname===''){console.log('Dashboard detected, starting auto-refresh...');startDashboardAutoRefresh()}});
function startDashboardAutoRefresh(){refreshDashboard();setInterval(refreshDashboard,5000)}
function refreshDashboard(){console.log('Fetching dashboard data from /api/info...');fetch('/api/info').then(res=>{console.log('Response received:',res.status);return res.json()}).then(data=>{console.log('Dashboard data received:',data);updateDashboardMetrics(data)}).catch(err=>{console.error('Dashboard refresh error:',err)})}
function updateDashboardMetrics(data){if(!data||!data.system){console.error('Invalid data structure');return}console.log('Updating dashboard metrics...');if(data.network&&data.network.connected&&data.network.rssi){const wifiCard=Array.from(document.querySelectorAll('.card')).find(c=>c.textContent.includes('Network Status'));if(wifiCard){const gridDivs=wifiCard.querySelectorAll('div[style*=\"grid-template\"] > div');if(gridDivs[1]){gridDivs[1].innerHTML='<strong style=\"color:#64748b\">Signal:</strong><br>'+data.network.rssi+' dBm';console.log('Updated WiFi signal:',data.network.rssi)}}}if(data.mqtt){const mqttCard=Array.from(document.querySelectorAll('.card')).find(c=>c.textContent.includes('MQTT Status'));if(mqttCard){const statusIndicator=mqttCard.querySelector('.status-indicator');const statusText=mqttCard.querySelector('p');if(statusIndicator&&statusText){if(data.mqtt.connected){statusIndicator.className='status-indicator status-online';statusText.innerHTML='<span class=\"status-indicator status-online\"></span>Connected to broker';console.log('Updated MQTT status: Connected')}else{statusIndicator.className='status-indicator status-offline';statusText.innerHTML='<span class=\"status-indicator status-offline\"></span>Not connected';console.log('Updated MQTT status: Disconnected')}}}}if(data.system){const sysCard=Array.from(document.querySelectorAll('.card')).find(c=>c.textContent.includes('System Information'));if(sysCard){const gridDivs=sysCard.querySelectorAll('div[style*=\"grid-template\"] > div');if(gridDivs[2]&&data.system.free_heap){gridDivs[2].innerHTML='<strong style=\"color:#64748b\">Free Heap:</strong><br>'+formatBytes(data.system.free_heap)+' / '+formatBytes(data.system.total_heap);console.log('Updated heap:',formatBytes(data.system.free_heap))}if(gridDivs[3]&&data.system.free_psram){gridDivs[3].innerHTML='<strong style=\"color:#64748b\">Free PSRAM:</strong><br>'+formatBytes(data.system.free_psram)+' / '+formatBytes(data.system.total_psram);console.log('Updated PSRAM:',formatBytes(data.system.free_psram))}if(gridDivs[4]&&data.system.temperature_celsius){gridDivs[4].innerHTML='<strong style=\"color:#64748b\">Temperature:</strong><br>'+data.system.temperature_celsius.toFixed(1)+'\u00b0C / '+data.system.temperature_fahrenheit.toFixed(1)+'\u00b0F';console.log('Updated temperature:',data.system.temperature_celsius.toFixed(1)+'°C')}if(gridDivs[5]&&data.system.healthy!==undefined){const healthText=data.system.healthy?'<span style=\"color:#10b981\">Healthy</span>':'<span style=\"color:#ef4444\">Issues</span>';gridDivs[5].innerHTML='<strong style=\"color:#64748b\">Health:</strong><br>'+healthText;console.log('Updated health status:',data.system.healthy?'Healthy':'Issues')}}}if(data.display){const dispCard=Array.from(document.querySelectorAll('.card')).find(c=>c.textContent.includes('Display Information'));if(dispCard){const gridDivs=dispCard.querySelectorAll('div[style*=\"grid-template\"] > div');if(gridDivs[1]&&data.display.brightness!==undefined){let brightnessMode='(Manual)';if(data.display.use_ha_rest_control){brightnessMode='(Home Assistant)'}else if(data.display.brightness_auto_mode){brightnessMode='(MQTT Auto)'}const brightnessText=data.display.brightness+'% '+brightnessMode;gridDivs[1].innerHTML='<strong style=\"color:#64748b\">Brightness:</strong><br>'+brightnessText;console.log('Updated display brightness:',brightnessText)}}}if(data.image){console.log('Image data:',data.image);const imgTotal=document.getElementById('imgTotal');const imgEnabled=document.getElementById('imgEnabled');const imgActive=document.getElementById('imgActive');const imgUpdate=document.getElementById('imgUpdate');const enabledCount=data.image.sources?data.image.sources.filter(s=>s.enabled).length:0;if(imgTotal&&data.image.source_count!==undefined){const count=data.image.source_count;imgTotal.innerHTML='<p style=\"margin:0;font-size:0.9rem;color:#94a3b8\"><strong style=\"color:#e2e8f0\">Total:</strong> '+count+' source'+(count!==1?'s':'')+'</p>'}if(imgEnabled){imgEnabled.innerHTML='<p style=\"margin:0;font-size:0.9rem;color:#94a3b8\"><strong style=\"color:#e2e8f0\">Enabled:</strong> '+enabledCount+'</p>'}if(imgActive&&data.image.current_index!==undefined){imgActive.innerHTML='<p style=\"margin:0;font-size:0.9rem;color:#94a3b8\"><strong style=\"color:#e2e8f0\">Active:</strong> #'+(data.image.current_index+1)+'</p>'}if(imgUpdate&&data.image.update_interval){imgUpdate.innerHTML='<p style=\"margin:0;font-size:0.9rem;color:#94a3b8\"><strong style=\"color:#e2e8f0\">Refresh:</strong> '+(data.image.update_interval/1000/60)+'m</p>'}console.log('Updated image status: Source #'+(data.image.current_index+1));const currentIndex=data.image.current_index||0;const sourceCount=data.image.source_count||0;for(let i=0;i<sourceCount;i++){const sourceDiv=document.getElementById('source-'+i);const sourceLabel=document.getElementById('source-label-'+i);const isActive=(i===currentIndex);if(sourceDiv){sourceDiv.style.background=isActive?'#1e3a2e':'#1e293b';sourceDiv.style.borderLeft=isActive?'4px solid #10b981':'4px solid #475569'}if(sourceLabel){const indicatorSpan=sourceLabel.querySelector('span');const strongTag=sourceLabel.querySelector('strong');if(indicatorSpan){indicatorSpan.style.color=isActive?'#10b981':'#64748b';indicatorSpan.style.fontSize=isActive?'1.2rem':'1rem';indicatorSpan.innerHTML=isActive?'►':'•'}if(strongTag){strongTag.style.color=isActive?'#10b981':'#64748b';strongTag.textContent='Source '+(i+1)+(isActive?' (Active)':'')}}}}console.log('Dashboard metrics update complete')}
function formatUptime(ms){const seconds=Math.floor(ms/1000);const days=Math.floor(seconds/86400);const hours=Math.floor((seconds%86400)/3600);const mins=Math.floor((seconds%3600)/60);const secs=seconds%60;if(days>0)return days+'d '+hours+'h';if(hours>0)return hours+'h '+mins+'m';if(mins>0)return mins+'m '+secs+'s';return secs+'s'}
function formatBytes(bytes){if(bytes<1024)return bytes+' B';if(bytes<1048576)return(bytes/1024).toFixed(1)+' KB';return(bytes/1048576).toFixed(2)+' MB'}
function updateBrightnessValue(value){document.getElementById('brightnessValue').textContent=value}
function updateMainBrightnessValue(value){document.getElementById('mainBrightnessValue').textContent=value}
function autoSaveDisplaySetting(name,value){const formData=new FormData();formData.append(name,value);if(name==='ha_access_token'&&!value)return;fetch('/api/save',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showToast('Setting saved','success')}else{showToast('Error: '+data.message,'error')}}).catch(error=>{showToast('Failed to save setting','error')})}
function updateBrightnessModeRadio(mode){const slider=document.getElementById('main_brightness');const container=document.getElementById('brightness_slider_container');const haHiddenInput=document.getElementById('use_ha_rest_control');const haCard=document.getElementById('haCard');const isManual=(mode==='manual');const isHa=(mode==='ha');if(isManual){slider.disabled=false;container.style.opacity='1'}else{slider.disabled=true;container.style.opacity='0.5'}if(haCard){if(isHa){haCard.style.opacity='1';haCard.style.pointerEvents='auto'}else{haCard.style.opacity='0.5';haCard.style.pointerEvents='none'}}const formData=new FormData();if(mode==='manual'){formData.append('brightness_auto_mode','');formData.append('brightness_auto_mode_present','1');formData.append('use_ha_rest_control','');formData.append('use_ha_rest_control_present','1');if(haHiddenInput)haHiddenInput.value=''}else if(mode==='mqtt'){formData.append('brightness_auto_mode','on');formData.append('brightness_auto_mode_present','1');formData.append('use_ha_rest_control','');formData.append('use_ha_rest_control_present','1');if(haHiddenInput)haHiddenInput.value=''}else if(mode==='ha'){formData.append('brightness_auto_mode','');formData.append('brightness_auto_mode_present','1');formData.append('use_ha_rest_control','on');formData.append('use_ha_rest_control_present','1');if(haHiddenInput)haHiddenInput.value='on'}fetch('/api/save',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showToast('Brightness mode updated','success');fetch('/api/force-brightness-update',{method:'POST'}).catch(()=>{})}else{showToast('Failed to update mode','error')}})}function initBrightnessModeUI(){const haCard=document.getElementById('haCard');const radios=document.querySelectorAll('input[name="brightness_mode"]');const selectedMode=Array.from(radios).find(r=>r.checked);if(haCard&&selectedMode&&selectedMode.value!=='ha'){haCard.style.opacity='0.5';haCard.style.pointerEvents='none'}}
function updateBrightnessMode(isAuto){const slider=document.getElementById('main_brightness');const container=document.getElementById('brightness_slider_container');const checkbox=document.getElementById('brightness_auto_mode');if(isAuto){slider.disabled=true;container.style.opacity='0.5'}else{slider.disabled=false;container.style.opacity='1'}const formData=new FormData();formData.append('brightness_auto_mode',isAuto?'on':'');formData.append('brightness_auto_mode_present','1');fetch('/api/save',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status!=='success'){showInputFeedback(checkbox,'error')}})}
function saveMainBrightness(btn){showButtonFeedback(btn,'loading','Applying...');const value=document.getElementById('main_brightness').value;const formData=new FormData();formData.append('default_brightness',value);fetch('/api/save',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Applied!')}else{showButtonFeedback(btn,'error','Error')}}).catch(e=>{showButtonFeedback(btn,'error','Failed')})}
function restart(){showConfirmModal('🔄 Restart Device','Are you sure you want to restart the device?',()=>{showLoading('Restarting device...');fetch('/api/restart',{method:'POST'}).then(()=>{showToast('Device is restarting...','success');setTimeout(()=>location.reload(),10000)}).catch(()=>{hideLoading();showToast('Failed to restart device','error')})})}
function factoryReset(){showConfirmModal('🏭 Factory Reset','Are you sure you want to reset to factory defaults? This cannot be undone!',()=>{showConfirmModal('⚠️ Confirm Factory Reset','This will erase ALL your settings. Are you absolutely sure?',()=>{showLoading('Performing factory reset...');fetch('/api/factory-reset',{method:'POST'}).then(()=>{showToast('Factory reset complete. Restarting...','success');setTimeout(()=>location.reload(),5000)}).catch(()=>{hideLoading();showToast('Failed to perform factory reset','error')})})})}
function addImageSource(btn){const url=prompt('Enter image URL:');if(url&&url.trim()){if(!url.match(/^https?:\/\/.+/i)){showToast('Invalid URL format. Must start with http:// or https://','error');return}showButtonFeedback(btn,'loading','Adding...');const formData=new FormData();formData.append('url',url.trim());fetch('/api/add-source',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Added!');showToast('Image source added successfully','success');setTimeout(()=>location.reload(),1000)}else{showButtonFeedback(btn,'error','Error');showToast('Error: '+data.message,'error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed');showToast('Failed to add image source','error')})}}
function removeImageSource(index,btn){showConfirmModal('🗑️ Remove Image Source','Are you sure you want to remove this image source?',()=>{showButtonFeedback(btn,'loading','Removing...');const formData=new FormData();formData.append('index',index);fetch('/api/remove-source',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Removed!');showToast('Image source removed','success');setTimeout(()=>location.reload(),1000)}else{showButtonFeedback(btn,'error','Error');showToast('Failed to remove source','error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed');showToast('Network error','error')})})}
function updateImageSource(index,input){const url=input.value;if(!validateImageUrl(input))return;const formData=new FormData();formData.append('index',index);formData.append('url',url);const errorDiv=document.getElementById('imageError_'+index);if(errorDiv)errorDiv.style.display='none';fetch('/api/update-source',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status!=='success'){showInputFeedback(input,'error');showToast('Error: '+data.message,'error');if(data.message&&errorDiv){errorDiv.textContent='⚠️ '+data.message;errorDiv.style.display='block'}}else{showInputFeedback(input,'success');showToast('Source updated','success');if(data.warning&&errorDiv){errorDiv.textContent='⚠️ WARNING: '+data.warning;errorDiv.style.display='block';errorDiv.style.borderLeftColor='#f59e0b';errorDiv.style.backgroundColor='rgba(245,158,11,0.1)'}}}).catch(error=>{showInputFeedback(input,'error');showToast('Network error','error')})}
function clearAllSources(btn){showConfirmModal('🗑️ Clear All Sources','Are you sure you want to clear all image sources? This will reset to a single default source.',()=>{showButtonFeedback(btn,'loading','Clearing...');fetch('/api/clear-sources',{method:'POST'}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Cleared!');showToast('All sources cleared','success');setTimeout(()=>location.reload(),1000)}else{showButtonFeedback(btn,'error','Error');showToast('Failed to clear sources','error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed');showToast('Network error','error')})})}
function toggleTransformSection(index){const section=document.getElementById('transformSection_'+index);if(section){section.style.display=section.style.display==='none'?'block':'none'}}
function updateImageTransform(index,property,input){const value=input.value;const formData=new FormData();formData.append('index',index);formData.append('property',property);formData.append('value',value);fetch('/api/update-transform',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status!=='success'){showInputFeedback(input,'error');showToast('Failed to update transform','error')}else{showInputFeedback(input,'success')}}).catch(error=>{showInputFeedback(input,'error');showToast('Network error','error')})}
function copyDefaultsToImage(index,btn){showConfirmModal('📋 Copy Global Defaults','Are you sure you want to copy global default transformation settings to this image?',()=>{showButtonFeedback(btn,'loading','Copying...');const formData=new FormData();formData.append('index',index);fetch('/api/copy-defaults',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Copied!');showToast('Defaults copied successfully','success');setTimeout(()=>location.reload(),1000)}else{showButtonFeedback(btn,'error','Error');showToast('Failed to copy defaults','error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed');showToast('Network error','error')})})}
function applyTransformImmediately(index,btn){showConfirmModal('⚙️ Apply Transform','Apply these transformation settings immediately?',()=>{showButtonFeedback(btn,'loading','Applying...');const formData=new FormData();formData.append('index',index);fetch('/api/apply-transform',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Applied!');showToast('Transform applied','success')}else{showButtonFeedback(btn,'error','Error');showToast('Failed to apply transform','error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed');showToast('Network error','error')})})}
function nextImage(btn){showButtonFeedback(btn,'loading','Switching...');fetch('/api/next-image',{method:'POST'}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Switched!');showToast('Switched to next image','success');setTimeout(()=>location.reload(),2000)}else{showButtonFeedback(btn,'error','Error');showToast('Failed to switch image','error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed');showToast('Network error','error')})}
function toggleSelectAll(checked){const checkboxes=document.querySelectorAll('.image-select-checkbox');checkboxes.forEach(cb=>cb.checked=checked);updateBulkDeleteButton()}
function updateBulkDeleteButton(){const checkboxes=document.querySelectorAll('.image-select-checkbox');const selected=Array.from(checkboxes).filter(cb=>cb.checked);const count=selected.length;const countSpan=document.getElementById('selectedCount');const bulkBtn=document.getElementById('bulkDeleteBtn');const selectAllCb=document.getElementById('selectAllImages');if(countSpan)countSpan.textContent=count;if(bulkBtn)bulkBtn.style.display=count>0?'inline-flex':'none';if(selectAllCb)selectAllCb.checked=(count>0&&count===checkboxes.length)}
function bulkDeleteSelected(btn){const checkboxes=document.querySelectorAll('.image-select-checkbox');const selected=Array.from(checkboxes).filter(cb=>cb.checked);const indices=selected.map(cb=>parseInt(cb.getAttribute('data-index')));if(indices.length===0){showToast('No images selected','warning');return}const total=checkboxes.length;if(indices.length===total){showToast('Cannot delete all sources. At least one must remain.','error');return}showConfirmModal('🗑️ Delete Selected Images','Are you sure you want to delete '+indices.length+' selected image source(s)?',()=>{showButtonFeedback(btn,'loading','Deleting...');const formData=new FormData();formData.append('indices',JSON.stringify(indices));fetch('/api/bulk-delete-sources',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Deleted!');showToast(data.message||'Selected sources deleted','success');setTimeout(()=>location.reload(),1000)}else{showButtonFeedback(btn,'error','Error');showToast('Error: '+data.message,'error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed');showToast('Network error','error')})})}
function toggleImageEnabled(index,btn){const formData=new FormData();formData.append('index',index);fetch('/api/toggle-image-enabled',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showToast(data.enabled?'Image enabled':'Image disabled','success');setTimeout(()=>location.reload(),500)}else{showToast('Failed to toggle image state','error')}}).catch(error=>{showToast('Network error','error')})}
function updateImageDuration(index,input){const duration=parseInt(input.value);if(duration<5||duration>3600){showToast('Duration must be between 5 and 3600 seconds','error');return}const formData=new FormData();formData.append('index',index);formData.append('duration',duration);fetch('/api/update-image-duration',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showToast('Duration updated','success')}else{showToast('Failed to update duration','error')}}).catch(error=>{showToast('Network error','error')})}
function selectImageForEditing(index,btn){showButtonFeedback(btn,'loading','Loading...');const formData=new FormData();formData.append('index',index);fetch('/api/select-image',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showToast('Switched to image #'+(index+1),'success');if(typeof resetCycleTimer==='function')resetCycleTimer();location.reload()}else{showToast('Failed to switch image','error');showButtonFeedback(btn,'error','Error')}}).catch(error=>{showToast('Network error','error');showButtonFeedback(btn,'error','Failed')})}
function updateSelectedImageTransform(property,value){const index=parseInt(document.getElementById('selectedImageNumber').textContent)-1;const formData=new FormData();formData.append('index',index);formData.append('property',property);formData.append('value',value);fetch('/api/update-transform',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status!=='success'){showToast('Failed to update '+property,'error')}else{if(typeof resetCycleTimer==='function')resetCycleTimer()}}).catch(error=>{showToast('Network error','error')})}
function copyDefaultsToSelectedImage(btn){const index=parseInt(document.getElementById('selectedImageNumber').textContent)-1;showConfirmModal('📋 Copy Global Defaults','Copy global default transformation settings to Image #'+(index+1)+'?',()=>{showButtonFeedback(btn,'loading','Copying...');const formData=new FormData();formData.append('index',index);fetch('/api/copy-defaults',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Copied!');showToast('Defaults copied','success');setTimeout(()=>location.reload(),1000)}else{showButtonFeedback(btn,'error','Error');showToast('Failed to copy defaults','error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed');showToast('Network error','error')})})}
function applySelectedTransformImmediately(btn){const index=parseInt(document.getElementById('selectedImageNumber').textContent)-1;showButtonFeedback(btn,'loading','Applying...');const formData=new FormData();formData.append('index',index);fetch('/api/apply-transform',{method:'POST',body:formData}).then(response=>response.json()).then(data=>{if(data.status==='success'){showButtonFeedback(btn,'success','Applied!');showToast('Transform applied & previewed','success');fetch('/api/clear-editing-state',{method:'POST'}).then(()=>{setTimeout(()=>location.reload(),800)})}else{showButtonFeedback(btn,'error','Error');showToast('Failed to apply transform','error')}}).catch(error=>{showButtonFeedback(btn,'error','Failed');showToast('Network error','error')})}
function addPreset(){const id=document.getElementById('presetSelect').value;fetch('/api/addPreset',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'id='+encodeURIComponent(id)}).then(r=>r.json()).then(j=>{if(typeof showToast==='function'){showToast(j.message,j.status==='success'?'success':'error')}else{alert(j.message)}setTimeout(()=>location.reload(),800)}).catch(e=>{if(typeof showToast==='function'){showToast('Add preset failed: '+e,'error')}else{alert('Add preset failed: '+e)}})}
function injectPresetPicker(){const list=document.getElementById('imageSourcesList');if(!list||document.getElementById('presetSelect'))return;const row=document.createElement('div');row.className='preset-row';row.style.cssText='display:flex;gap:0.5rem;align-items:center;flex-wrap:wrap;margin-bottom:0.75rem';row.innerHTML="<label for='presetSelect'>Add full-disc preset:</label><select id='presetSelect'><option value='sdo_aia_304'>Sun SDO/AIA 304A</option><option value='sdo_aia_171'>Sun SDO/AIA 171A</option><option value='sdo_aia_193'>Sun SDO/AIA 193A</option><option value='sdo_hmi_igr'>Sun SDO/HMI Continuum</option><option value='sdo_hmi_mag'>Sun SDO/HMI Magnetogram</option><option value='soho_c2'>Sun SOHO LASCO C2</option><option value='soho_c3'>Sun SOHO LASCO C3</option><option value='goes19_full'>Earth GOES-19 Full Disc</option><option value='__moon__'>Moon (computed)</option></select><button type='button' class='btn btn-success' onclick='addPreset()'>Add Preset</button>";list.parentNode.insertBefore(row,list)}
function injectMoonSettings(){const list=document.getElementById('imageSourcesList');if(!list||document.getElementById('moonLat'))return;const box=document.createElement('div');box.className='moon-settings';box.style.cssText='display:flex;gap:0.5rem;align-items:center;flex-wrap:wrap;margin-bottom:0.75rem';box.innerHTML="<h4 style='margin:0;width:100%'>Moon settings</h4><label>Latitude <input type='number' id='moonLat' step='0.0001'></label><label>Longitude <input type='number' id='moonLon' step='0.0001'></label><label>Background <select id='moonBg'><option value='0'>Black</option><option value='1'>Starfield</option><option value='2'>Glow</option><option value='3'>Stars + Glow</option></select></label><button type='button' class='btn btn-success' onclick='saveMoon()'>Save Moon Settings</button>";list.parentNode.insertBefore(box,list);fetch('/api/getMoon').then(r=>r.json()).then(j=>{const la=document.getElementById('moonLat');const lo=document.getElementById('moonLon');const bg=document.getElementById('moonBg');if(la)la.value=j.lat;if(lo)lo.value=j.lon;if(bg)bg.value=j.bg}).catch(e=>{})}
function saveMoon(){const p=new URLSearchParams();p.set('lat',document.getElementById('moonLat').value||'0');p.set('lon',document.getElementById('moonLon').value||'0');p.set('bg',document.getElementById('moonBg').value||'1');fetch('/api/setMoon',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p.toString()}).then(r=>r.json()).then(j=>{if(typeof showToast==='function'){showToast(j.message,j.status==='success'?'success':'error')}else{alert(j.message)}}).catch(e=>{if(typeof showToast==='function'){showToast('Save failed: '+e,'error')}else{alert('Save failed: '+e)}})}
if(document.readyState==='loading'){document.addEventListener('DOMContentLoaded',function(){injectPresetPicker();injectMoonSettings()})}else{injectPresetPicker();injectMoonSettings()}
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

const char HTML_IMAGES_APP[] PROGMEM = R"rawliteral(
(function(){
var state=null;
var openDrawers={};
var defaultsDirty=false;
var debTimers={};

function toast(m,t){if(typeof showToast==='function')showToast(m,t)}
function inputOk(el){if(typeof showInputFeedback==='function'){showInputFeedback(el,'success')}else{el.classList.add('img-ok');setTimeout(function(){el.classList.remove('img-ok')},1500)}}
function inputErr(el){if(typeof showInputFeedback==='function'){showInputFeedback(el,'error')}else{el.classList.add('img-err');setTimeout(function(){el.classList.remove('img-err')},1500)}}
function esc(s){return String(s==null?'':s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;')}
function debounce(key,fn,ms){if(debTimers[key])clearTimeout(debTimers[key]);debTimers[key]=setTimeout(fn,ms||300)}

function post(url,data){var body=new URLSearchParams();for(var k in data){if(data.hasOwnProperty(k))body.set(k,data[k])}return fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.toString()}).then(function(r){return r.json()})}

function load(){return fetch('/api/images/state').then(function(r){return r.json()}).then(function(j){state=j;render()})}
function refetch(){return load()}

function nearly(a,b){return Math.abs((+a)-(+b))<0.0001}
function isDefaultTransform(s){var d=state.defaults;return nearly(s.scaleX,d.scaleX)&&nearly(s.scaleY,d.scaleY)&&(+s.offsetX===+d.offsetX)&&(+s.offsetY===+d.offsetY)&&(+s.rotation===+d.rotation)}
function summary(s){if(isDefaultTransform(s))return 'default';var ox=(+s.offsetX>=0?'+':'')+s.offsetX;var oy=(+s.offsetY>=0?'+':'')+s.offsetY;return (+s.scaleX).toFixed(2)+'×'+(+s.scaleY).toFixed(2)+'  '+ox+','+oy+'  '+(+s.rotation)+'°'}

function render(){
var el=document.getElementById('imageApp');if(!el||!state)return;
var h='';
h+=renderSourcesCard();
h+=renderPlaybackCard();
h+=renderDefaultTransformCard();
h+=renderSaveBar();
el.innerHTML=h;
bind();
}

function renderSourcesCard(){
var sources=state.sources||[];
var multi=sources.length>1;
var h='<div class="card"><h2>Image Sources<span class="img-count">'+sources.length+' source'+(sources.length===1?'':'s')+'</span></h2>';
h+='<div class="img-toolbar">';
h+='<div class="img-add-bar"><input type="text" id="addUrl" class="form-control" placeholder="https://example.com/image.jpg"><button type="button" class="btn btn-success" id="addUrlBtn">Add</button></div>';
var opts='';var presets=state.presets||[];for(var i=0;i<presets.length;i++){opts+='<option value="'+esc(presets[i].id)+'">'+esc(presets[i].label)+'</option>'}
h+='<div class="img-add-bar"><select id="presetSel" class="form-control" style="flex:1 1 220px">'+opts+'</select><button type="button" class="btn btn-secondary" id="addPresetBtn">Add</button></div>';
h+='<div class="img-moon-box"><button type="button" class="btn btn-secondary img-collapse-head" id="moonHead" style="min-height:var(--tap)">Moon (computed) settings</button>';
h+='<div class="img-collapse-body" id="moonBody"><div class="img-moon-grid">';
var m=state.moon||{lat:0,lon:0,bg:1};
h+='<div class="transform-field"><label for="moonLat">Latitude</label><input type="number" step="0.0001" id="moonLat" class="form-control" value="'+(m.lat)+'"></div>';
h+='<div class="transform-field"><label for="moonLon">Longitude</label><input type="number" step="0.0001" id="moonLon" class="form-control" value="'+(m.lon)+'"></div>';
h+='<div class="transform-field"><label for="moonBg">Background</label><select id="moonBg" class="form-control">'+bgOpts(m.bg)+'</select></div>';
h+='</div><button type="button" class="btn btn-success" id="saveMoonBtn" style="min-height:var(--tap)">Save moon</button></div></div>';
h+='</div>';
if(multi){
h+='<div class="img-bulk-bar"><label><input type="checkbox" id="selAll"> Select all</label>';
h+='<button type="button" class="btn btn-danger" id="bulkDelBtn" disabled>Delete selected (<span id="selCount">0</span>)</button></div>';
}
h+='<div class="img-list">';
for(var x=0;x<sources.length;x++){h+=renderRow(sources[x],multi)}
h+='</div></div>';
return h;
}

function bgOpts(sel){var labels=['Black','Starfield','Glow','Stars + Glow'];var o='';for(var i=0;i<4;i++){o+='<option value="'+i+'"'+(+sel===i?' selected':'')+'>'+labels[i]+'</option>'}return o}

function renderRow(s,multi){
var idx=s.index;
var active=(state.currentIndex===idx);
var cls='img-row'+(active?' is-active':'')+(s.enabled?'':' is-disabled');
var open=openDrawers[idx];
var h='<div class="'+cls+'" data-index="'+idx+'">';
h+='<div class="img-row-main">';
h+='<button type="button" class="img-toggle-btn" data-act="toggle" data-index="'+idx+'" aria-pressed="'+(s.enabled?'true':'false')+'"><i class="fas fa-'+(s.enabled?'eye':'eye-slash')+'"></i> '+(s.enabled?'On':'Off')+'</button>';
h+='<span class="img-idx">'+(idx+1)+'.</span>';
h+='<input type="text" class="form-control" data-act="url" data-index="'+idx+'" value="'+esc(s.url)+'"'+(s.isMoon?' readonly':'')+'>';
h+='<button type="button" class="img-caret" data-act="caret" data-index="'+idx+'" aria-expanded="'+(open?'true':'false')+'"><i class="fas fa-chevron-'+(open?'up':'down')+'"></i></button>';
if(multi){h+='<label style="display:inline-flex;align-items:center;min-height:var(--tap)"><input type="checkbox" class="img-sel" data-index="'+idx+'"></label>'}
h+='</div>';
h+='<div class="img-row-meta">';
h+='<span>Duration <input type="number" class="form-control" data-act="duration" data-index="'+idx+'" min="5" max="3600" value="'+(s.duration)+'"> s</span>';
h+='<span class="img-summary">'+esc(summary(s))+'</span>';
h+='</div>';
h+='<div class="img-drawer'+(open?' is-open':'')+'" data-drawer="'+idx+'">';
h+='<div class="transform-grid">';
h+=tf(idx,'scaleX','Scale X',s.scaleX,0.01,0.1,state.maxScale);
h+=tf(idx,'scaleY','Scale Y',s.scaleY,0.01,0.1,state.maxScale);
h+=tfInt(idx,'offsetX','Offset X',s.offsetX);
h+=tfInt(idx,'offsetY','Offset Y',s.offsetY);
h+='<div class="transform-field"><label>Rotation</label><select class="form-control" data-act="tf" data-prop="rotation" data-index="'+idx+'">'+rotOpts(s.rotation)+'</select></div>';
h+='</div>';
h+='<div class="img-drawer-actions">';
h+='<button type="button" class="btn btn-secondary" data-act="reset" data-index="'+idx+'">Reset to defaults</button>';
var tuned=state.tuning&&state.tuning.active&&state.tuning.index===idx;
if(tuned){h+='<span class="status-pill status-pill--active">Holding #'+(idx+1)+' on display</span><button type="button" class="btn btn-success" data-act="tunestop">Done, resume cycling</button>'}
else{h+='<button type="button" class="btn btn-secondary" data-act="tune" data-index="'+idx+'">Tune on device</button>'}
h+='</div></div></div>';
return h;
}

function tf(idx,prop,label,val,step,min,max){return '<div class="transform-field"><label>'+label+'</label><input type="number" class="form-control" data-act="tf" data-prop="'+prop+'" data-index="'+idx+'" step="'+step+'" min="'+min+'" max="'+max+'" value="'+(+val)+'"></div>'}
function tfInt(idx,prop,label,val){return '<div class="transform-field"><label>'+label+'</label><input type="number" class="form-control" data-act="tf" data-prop="'+prop+'" data-index="'+idx+'" step="1" value="'+(+val)+'"></div>'}
function rotOpts(sel){var o='';[0,90,180,270].forEach(function(r){o+='<option value="'+r+'"'+(+sel===r?' selected':'')+'>'+r+'°</option>'});return o}

function renderPlaybackCard(){
var h='<div class="card"><h2>Playback</h2>';
h+='<div class="transform-grid">';
h+='<div class="transform-field"><label for="dz_mode">Update Mode</label><select id="dz_mode" class="form-control" data-dz="1"><option value="0"'+(+state.updateMode===0?' selected':'')+'>Automatic Cycling</option><option value="1"'+(+state.updateMode===1?' selected':'')+'>API-Triggered Refresh</option></select></div>';
h+='<div class="transform-field"><label for="dz_interval">Refresh Interval (min)</label><input type="number" id="dz_interval" class="form-control" min="1" max="1440" value="'+(state.updateInterval)+'" data-dz="1"></div>';
h+='<div class="transform-field"><label for="dz_duration">Default Duration (s)</label><input type="number" id="dz_duration" class="form-control" min="5" max="3600" value="'+(state.defaultDuration)+'" data-dz="1"></div>';
h+='<div class="transform-field"><label style="display:inline-flex;align-items:center;gap:0.5rem;min-height:var(--tap)"><input type="checkbox" id="dz_random" data-dz="1"'+(state.randomOrder?' checked':'')+'> Randomize order</label></div>';
h+='</div></div>';
return h;
}

function renderDefaultTransformCard(){
var d=state.defaults;
var h='<div class="card"><h2 class="img-collapse-head" id="dtHead">Default Transform <i class="fas fa-chevron-down" style="font-size:0.8rem"></i></h2>';
h+='<div class="img-collapse-body" id="dtBody">';
h+='<p class="img-note">Baseline for new images. Per-image edits override these.</p>';
h+='<div class="transform-grid">';
h+='<div class="transform-field"><label for="dz_sx">Scale X</label><input type="number" id="dz_sx" class="form-control" step="0.01" min="0.1" max="'+state.maxScale+'" value="'+(+d.scaleX)+'" data-dz="1"></div>';
h+='<div class="transform-field"><label for="dz_sy">Scale Y</label><input type="number" id="dz_sy" class="form-control" step="0.01" min="0.1" max="'+state.maxScale+'" value="'+(+d.scaleY)+'" data-dz="1"></div>';
h+='<div class="transform-field"><label for="dz_ox">Offset X</label><input type="number" id="dz_ox" class="form-control" step="1" value="'+(+d.offsetX)+'" data-dz="1"></div>';
h+='<div class="transform-field"><label for="dz_oy">Offset Y</label><input type="number" id="dz_oy" class="form-control" step="1" value="'+(+d.offsetY)+'" data-dz="1"></div>';
h+='<div class="transform-field"><label for="dz_rot">Rotation</label><select id="dz_rot" class="form-control" data-dz="1">'+rotOpts(d.rotation)+'</select></div>';
h+='</div></div></div>';
return h;
}

function renderSaveBar(){
var h='<div class="img-save-bar">';
h+='<button type="button" class="btn btn-success" id="saveDefaultsBtn"'+(defaultsDirty?'':' disabled')+'>Save defaults</button>';
if(defaultsDirty){h+='<span class="img-dirty-dot"><i class="fas fa-circle" style="font-size:0.6rem"></i> Unsaved default changes</span>'}
h+='</div>';
return h;
}

function markDirty(){if(!defaultsDirty){defaultsDirty=true;var b=document.getElementById('saveDefaultsBtn');if(b)b.disabled=false;var bar=document.querySelector('.img-save-bar');if(bar&&!bar.querySelector('.img-dirty-dot')){var s=document.createElement('span');s.className='img-dirty-dot';s.innerHTML='<i class="fas fa-circle" style="font-size:0.6rem"></i> Unsaved default changes';bar.appendChild(s)}}}

function saveDefaults(){
var d={};
d.image_update_mode=document.getElementById('dz_mode').value;
d.update_interval=document.getElementById('dz_interval').value;
d.default_image_duration=document.getElementById('dz_duration').value;
d.random_order=document.getElementById('dz_random').checked?'on':'';
d.random_order_present='1';
d.default_scale_x=document.getElementById('dz_sx').value;
d.default_scale_y=document.getElementById('dz_sy').value;
d.default_offset_x=document.getElementById('dz_ox').value;
d.default_offset_y=document.getElementById('dz_oy').value;
d.default_rotation=document.getElementById('dz_rot').value;
d.cycling_enabled='on';
d.cycling_enabled_present='1';
post('/api/save',d).then(function(j){if(j.status==='success'){defaultsDirty=false;toast('Defaults saved','success');refetch()}else{toast('Error: '+(j.message||'save failed'),'error')}}).catch(function(){toast('Network error','error')});
}

function selSel(){return Array.prototype.slice.call(document.querySelectorAll('.img-sel'))}
function updateBulk(){var sel=selSel().filter(function(c){return c.checked});var n=sel.length;var cnt=document.getElementById('selCount');if(cnt)cnt.textContent=n;var b=document.getElementById('bulkDelBtn');if(b)b.disabled=(n===0);var all=document.getElementById('selAll');if(all)all.checked=(n>0&&n===selSel().length)}

function bind(){
var addBtn=document.getElementById('addUrlBtn');if(addBtn)addBtn.onclick=function(){var inp=document.getElementById('addUrl');var u=(inp.value||'').trim();if(!u.match(/^https?:\/\/.+/i)){inputErr(inp);toast('URL must start with http:// or https://','error');return}post('/api/add-source',{url:u}).then(function(j){if(j.status==='success'){toast('Source added','success');refetch()}else{toast('Error: '+(j.message||''),'error')}}).catch(function(){toast('Network error','error')})};
var apBtn=document.getElementById('addPresetBtn');if(apBtn)apBtn.onclick=function(){var sel=document.getElementById('presetSel');if(!sel||!sel.value){toast('No preset selected','warning');return}post('/api/addPreset',{id:sel.value}).then(function(j){if(j.status==='success'){toast(j.message||'Preset added','success');refetch()}else{toast('Error: '+(j.message||''),'error')}}).catch(function(){toast('Network error','error')})};
var mh=document.getElementById('moonHead');if(mh)mh.onclick=function(){var b=document.getElementById('moonBody');if(b)b.classList.toggle('is-open')};
var sm=document.getElementById('saveMoonBtn');if(sm)sm.onclick=function(){post('/api/setMoon',{lat:document.getElementById('moonLat').value||'0',lon:document.getElementById('moonLon').value||'0',bg:document.getElementById('moonBg').value||'1'}).then(function(j){if(j.status==='success'){toast(j.message||'Moon saved','success');refetch()}else{toast('Error: '+(j.message||''),'error')}}).catch(function(){toast('Network error','error')})};

var selAll=document.getElementById('selAll');if(selAll)selAll.onchange=function(){selSel().forEach(function(c){c.checked=selAll.checked});updateBulk()};
selSel().forEach(function(c){c.onchange=updateBulk});
var bd=document.getElementById('bulkDelBtn');if(bd)bd.onclick=function(){var idx=selSel().filter(function(c){return c.checked}).map(function(c){return parseInt(c.getAttribute('data-index'),10)});if(idx.length===0){toast('No images selected','warning');return}if(idx.length>=(state.sources||[]).length){toast('Cannot delete all sources. At least one must remain.','error');return}if(typeof showConfirmModal==='function'){showConfirmModal('Delete Selected','Delete '+idx.length+' selected source(s)?',function(){doBulkDel(idx)})}else{doBulkDel(idx)}};

var dt=document.getElementById('dtHead');if(dt)dt.onclick=function(){var b=document.getElementById('dtBody');if(b)b.classList.toggle('is-open')};

bindRows();

document.querySelectorAll('[data-dz]').forEach(function(el){var ev=(el.type==='checkbox'||el.tagName==='SELECT')?'change':'input';el.addEventListener(ev,markDirty)});
var sdb=document.getElementById('saveDefaultsBtn');if(sdb)sdb.onclick=saveDefaults;
}

function doBulkDel(idx){post('/api/bulk-delete-sources',{indices:JSON.stringify(idx)}).then(function(j){if(j.status==='success'){toast(j.message||'Deleted','success');openDrawers={};refetch()}else{toast('Error: '+(j.message||''),'error')}}).catch(function(){toast('Network error','error')})}

function srcByIndex(idx){var s=state.sources||[];for(var i=0;i<s.length;i++){if(s[i].index===idx)return s[i]}return null}

function bindRows(){
document.querySelectorAll('.img-toggle-btn[data-act="toggle"]').forEach(function(b){b.onclick=function(){var idx=parseInt(b.getAttribute('data-index'),10);post('/api/toggle-image-enabled',{index:idx}).then(function(j){if(j.status==='success'){toast(j.enabled?'Image enabled':'Image disabled','success');refetch()}else{toast('Failed to toggle','error')}}).catch(function(){toast('Network error','error')})}});
document.querySelectorAll('.img-caret[data-act="caret"]').forEach(function(b){b.onclick=function(){var idx=parseInt(b.getAttribute('data-index'),10);openDrawers[idx]=!openDrawers[idx];var d=document.querySelector('.img-drawer[data-drawer="'+idx+'"]');if(d)d.classList.toggle('is-open',openDrawers[idx]);b.setAttribute('aria-expanded',openDrawers[idx]?'true':'false');var i=b.querySelector('i');if(i)i.className='fas fa-chevron-'+(openDrawers[idx]?'up':'down')}});
document.querySelectorAll('[data-act="url"]').forEach(function(inp){inp.onchange=function(){var idx=parseInt(inp.getAttribute('data-index'),10);var u=(inp.value||'').trim();if(!u.match(/^https?:\/\/.+/i)){inputErr(inp);toast('URL must start with http:// or https://','error');return}post('/api/update-source',{index:idx,url:u}).then(function(j){if(j.status==='success'){inputOk(inp);var s=srcByIndex(idx);if(s)s.url=u}else{inputErr(inp);toast('Error: '+(j.message||''),'error');var s2=srcByIndex(idx);if(s2)inp.value=s2.url}}).catch(function(){inputErr(inp);var s3=srcByIndex(idx);if(s3)inp.value=s3.url})}});
document.querySelectorAll('[data-act="duration"]').forEach(function(inp){inp.onchange=function(){var idx=parseInt(inp.getAttribute('data-index'),10);var v=parseInt(inp.value,10);var s=srcByIndex(idx);if(isNaN(v)||v<5||v>3600){inputErr(inp);toast('Duration must be 5-3600 seconds','error');if(s)inp.value=s.duration;return}post('/api/update-image-duration',{index:idx,duration:v}).then(function(j){if(j.status==='success'){inputOk(inp);if(s)s.duration=v}else{inputErr(inp);if(s)inp.value=s.duration}}).catch(function(){inputErr(inp);if(s)inp.value=s.duration})}});
document.querySelectorAll('[data-act="tf"]').forEach(function(inp){var ev=inp.tagName==='SELECT'?'change':'input';inp.addEventListener(ev,function(){var idx=parseInt(inp.getAttribute('data-index'),10);var prop=inp.getAttribute('data-prop');var val=inp.value;debounce('tf'+idx+prop,function(){post('/api/update-transform',{index:idx,property:prop,value:val}).then(function(j){if(j.status==='success'){inputOk(inp);var s=srcByIndex(idx);if(s)s[prop]=val;updateSummary(idx)}else{inputErr(inp);var s2=srcByIndex(idx);if(s2){inp.value=s2[prop]}}}).catch(function(){inputErr(inp);var s3=srcByIndex(idx);if(s3)inp.value=s3[prop]})},300)})});
document.querySelectorAll('[data-act="reset"]').forEach(function(b){b.onclick=function(){var idx=parseInt(b.getAttribute('data-index'),10);post('/api/copy-defaults',{index:idx}).then(function(j){if(j.status==='success'){toast('Reset to defaults','success');refetch()}else{toast('Failed to reset','error')}}).catch(function(){toast('Network error','error')})}});
document.querySelectorAll('[data-act="tune"]').forEach(function(b){b.onclick=function(){var idx=parseInt(b.getAttribute('data-index'),10);post('/api/images/tune',{index:idx}).then(function(j){if(j.status==='success'){toast('Holding #'+(idx+1)+' on display','success');refetch()}else{toast('Failed to tune','error')}}).catch(function(){toast('Network error','error')})}});
document.querySelectorAll('[data-act="tunestop"]').forEach(function(b){b.onclick=function(){post('/api/images/tune/stop',{}).then(function(j){if(j.status==='success'){toast('Resumed cycling','success');refetch()}else{toast('Failed to stop','error')}}).catch(function(){toast('Network error','error')})}});
}

function updateSummary(idx){var s=srcByIndex(idx);if(!s)return;var row=document.querySelector('.img-row[data-index="'+idx+'"]');if(!row)return;var sp=row.querySelector('.img-summary');if(sp)sp.textContent=summary(s)}

window.addEventListener('beforeunload',function(e){if(defaultsDirty){e.preventDefault();e.returnValue='';return ''}});

if(document.readyState==='loading'){document.addEventListener('DOMContentLoaded',load)}else{load()}
})();
)rawliteral";

#endif // WEB_CONFIG_HTML_H
