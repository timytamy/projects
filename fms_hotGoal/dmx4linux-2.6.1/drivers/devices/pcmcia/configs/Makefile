all modules clean:

modules_install:
	[ -d /etc/pcmcia ] || mkdir -p /etc/pcmcia
	install -m 755 digimedia /etc/pcmcia
	install -m 644 digimedia.conf digimedia.opts /etc/pcmcia

distclean: clean uninstall

uninstall:
	cd /etc/pcmcia && $(RM) digimedia digimedia.conf digimedia.opts
