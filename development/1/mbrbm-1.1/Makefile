MBRBM=1.1
DISTFILES=README Makefile index.html mbrbm.S mbrbm.bin mbrbm.lis mbrbm.md5 mbrbm.png mbrbml.png install.sh install.bat

all:	mbrbm-$(MBRBM).zip

mbrbm-$(MBRBM)/README:	README mbrbm.md5
	rm -rf mbrbm-$(MBRBM)
	mkdir mbrbm-$(MBRBM)
	cp $(DISTFILES) mbrbm-$(MBRBM)
	chmod -R o+rX mbrbm-$(MBRBM)

mbrbm-$(MBRBM).tar.gz:	mbrbm-$(MBRBM)/README
	tar cvzf $@ mbrbm-$(MBRBM)

mbrbm-$(MBRBM).zip:	mbrbm-$(MBRBM).tar.gz
	zip -r $@ mbrbm-$(MBRBM)

mbrbm.bin:	install.bat mbrbm.S
	nasm -o $@ -l mbrbm.lis mbrbm.S

README:	index.html
	lynx -dump index.html >$@

mbrbm.md5:	mbrbm.bin
	dd if=mbrbm.bin of=mbrbm.cod bs=1 count=438
	md5sum mbrbm.cod >$@
	echo 'a081bde3c36e0921872483aea3aad34f  mbrbm.cod' | diff - $@
	rm mbrbm.cod

install.sh:	mbrbm.S
	sed -n '/IFDEF INSTALLSH/,/ENDIF/p' <mbrbm.S | sed -n '$$d;2,$$p' >$@
	chmod +x $@

install.bat:	install.sh
	sed -n '/IFDEF INSTALLBAT/,/ENDIF/p' <mbrbm.S | sed -n '$$d;2,$$p' | unix2dos >$@

clean:
	rm -rf mbrbm-* mbrbm.bin mbrbm.lis mbrbm.md5 README install.*
