#!/bin/bash

MY_WINE=/usr/local/bin/wine
WINEDEBUG=+oleacc
$MY_WINE regedit /s 6.reg || true
$MY_WINE regedit /s 8.reg || true
$MY_WINE ./IEDriverServer.exe --log-level=TRACE
