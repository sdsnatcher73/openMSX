# $Id$

include build/node-start.mk

SUBDIRS:=manual

DIST:= \
	release-notes.txt release-history.txt \
	commands.txt debugdevice.txt \
	developersFAQ.txt testcases.txt \
	exampleconfigs.xml \
	vram-addressing.txt \
	openmsx.tex MSX-cassette.dia \
	msxinfo-article.html schema1.png schema2.png screenshot.png

include build/node-end.mk

