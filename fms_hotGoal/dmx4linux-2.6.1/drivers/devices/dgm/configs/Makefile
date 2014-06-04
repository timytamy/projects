include ../../../../config.mk

pcmcia-files-$(CONFIG_DMX4LINUX_DGM_CS) += digimedia
OBJ =
MODULES =

pcmcia-files = $(pcmcia-files-y) $(pcmcia-files-m)
all:

clean:

ifdef CONFIG_PCMCIAPATH
install: $(pcmcia-files) $(addsuffix .conf,$(pcmcia-files)) $(addsuffix .opts,$(pcmcia-files))
	mkdir -p $(CONFIG_PCMCIAPATH)
	install -m 755 $(pcmcia-files) $(CONFIG_PCMCIAPATH)
	install -m 644 $(addsuffix .conf,$(pcmcia-files)) $(CONFIG_PCMCIAPATH)
	install -m 644 $(addsuffix .opts,$(pcmcia-files)) $(CONFIG_PCMCIAPATH)
else
install:
endif

distclean: clean uninstall

uninstall:
ifdef CONFIG_PCMCIAPATH
	rm -f $(addsuffix .conf,$(addprefix $(CONFIG_PCMCIAPATH)/, $(pcmcia-files)))
	rm -f $(addsuffix .opts,$(addprefix $(CONFIG_PCMCIAPATH)/, $(pcmcia-files)))
	rm -rf $(addprefix $(CONFIG_PCMCIAPATH)/, $(pcmcia-files))
endif

