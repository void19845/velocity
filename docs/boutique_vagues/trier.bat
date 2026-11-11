@echo off
rem Tri des branches de l'arbre de la boutique Vagues : montre une branche, propose de la valider ou de la supprimer.
rem Double-clic pour trier tout ce qui reste ; ou, dans un terminal : trier.bat --graine RAIL   (voir outils\trier.js --aide)
chcp 65001 >nul
cd /d "%~dp0outils"
if not exist donnees\arbre_v2.json node migrer_v2.js
node trier.js %*
if "%~1"=="" pause
