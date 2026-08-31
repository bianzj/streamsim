@echo off
setlocal
cd /d "%~dp0"
if not exist "node_modules\three\package.json" (
    call npm install
    if errorlevel 1 exit /b 1
)
set "PORT=43177"
start "" /b powershell.exe -NoProfile -WindowStyle Hidden -Command "$url='http://127.0.0.1:43177'; for($i=0; $i -lt 80; $i++){ try { $health=Invoke-RestMethod ($url + '/api/health'); if($health.app -eq 'radiosity-three-gui'){ Start-Process $url; exit 0 } } catch {}; Start-Sleep -Milliseconds 250 }; exit 1"
call npm start
