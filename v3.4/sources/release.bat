rem Name: release.bat
rem Purpose:
rem   make siod binary release kit for Windows NT.
rem   assumes directory organization
rem   of authors personal project directories.
rem   must be run in SIOD project directory.
rem $Id: release.bat,v 1.2 1997/05/05 12:46:00 gjc Exp $
rem
deltree siod_dist
mkdir siod_dist
copy release\libsiod.dll      siod_dist
copy release\siod.exe         siod_dist
copy release\parser_pratt.dll siod_dist
copy release\regex.dll        siod_dist
copy release\ss.dll           siod_dist
copy release\tar.dll          siod_dist
copy siod.html                siod_dist
copy *.scm                    siod_dist
copy *.smd                    siod_dist
copy *.txt                    siod_dist
rem don't forget henry spencer's regex
copy ..\hs_regex\release\hs_regex.dll siod_dist
rem make compressed distribution archive, using wonderfull info-zip tool
\zip\zip -r \Distribute\winsiod siod_dist
rem all done

