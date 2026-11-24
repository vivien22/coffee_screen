@echo off
rem Opens the journal page in Chrome or Edge (the page reads the USB serial port with the Web Serial API, which Firefox does not have).
rem Double-click this file, or open tools\journal_viewer.html yourself in one of those two browsers.
set PAGE=%~dp0journal_viewer.html
reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\chrome.exe" >nul 2>nul && (start "" chrome "%PAGE%" & goto :eof)
start "" msedge "%PAGE%"
