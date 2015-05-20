CFLAGS=-Wall -Wmissing-prototypes -Wstrict-prototypes -O2 -fomit-frame-pointer

LXDIALOG := lxdialog/checklist.o lxdialog/inputbox.o lxdialog/menubox.o lxdialog/textbox.o lxdialog/util.o lxdialog/yesno.o

conf: conf.o zconf.tab.o
mconf_CFLAGS := $(shell ./lxdialog/check-lxdialog.sh -ccflags) -DLOCALE
mconf_LDFLAGS := $(shell ./lxdialog/check-lxdialog.sh -ldflags $(CC))
mconf: CFLAGS += $(mconf_CFLAGS)

mconf: mconf.o zconf.tab.o $(LXDIALOG)
	$(CC) -o mconf $^ $(mconf_LDFLAGS)

.PHONY: clean
clean:
	@rm -f mconf conf *.o lxdialog/*.o
