directory = /usr/local/bin
targets = kahtofun kahtopipe
oldtargets = cplotfun cplotpipe
LDLIBS += -lkahto

all: $(targets)

install: $(targets)
	cp $^ $(directory)

uninstall:
	@rm -vrf $(directory)/$(targets)
	@rm -vrf $(directory)/$(oldtargets)

clean:
	@rm -vrf $(targets)
