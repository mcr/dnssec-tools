#
# Copyright 2006-2008 SPARTA, Inc.  All rights reserved.
# See the COPYING file included with the DNSSEC-Tools package for details.
#
# Makefile for building demo data for the many-zones rollerd demo.
#

all: many-zones-rollerd

many-zones-rollerd:	clean pristine-files zones-short

pristine-files:
	cp save-demo.rollrec demo.rollrec
	cp save-demo-smallset.rollrec demo-smallset.rollrec
	cp save-dummy.com dummy.com
	cp save-example.com example.com
	cp save-test.com test.com
	cp save-woof.com woof.com
	cp save-xorn.com xorn.com
	cp save-yowzah.com yowzah.com
	cp save-zero.com zero.com
	phaser demo.rollrec
	phaser demo-smallset.rollrec

zones-short:
	zonesigner -zsklife 15 -e now+25 -genkeys dummy.com
	zonesigner -zsklife 60 -genkeys example.com
	zonesigner -zsklife 30 -genkeys test.com
	zonesigner -zsklife 60 -genkeys woof.com
	zonesigner -zsklife 45 -genkeys xorn.com
	zonesigner -zsklife 60 -genkeys yowzah.com
	zonesigner -zsklife 45 -genkeys zero.com

clean:
	rm -f K* db.* dsset-* keyset-* *.krf *.signed
	rm -f dummy.com example.com test.com woof.com xorn.com yowzah.com zero.com
	rm -f demo.rollrec demo-smallset.rollrec
	@echo NOT deleting log.demo
	@echo ' '

