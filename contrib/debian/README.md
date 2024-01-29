
Debian
====================
This directory contains files used to package Hemisd/Hemis-qt
for Debian-based Linux systems. If you compile Hemisd/Hemis-qt yourself, there are some useful files here.

## Hemis: URI support ##


Hemis-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install Hemis-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your Hemis-qt binary to `/usr/bin`
and the `../../share/pixmaps/Hemis128.png` to `/usr/share/pixmaps`

Hemis-qt.protocol (KDE)

