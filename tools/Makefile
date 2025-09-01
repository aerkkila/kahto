directory = /usr/local/bin
targets = kahtofun kahtopipe
LDLIBS += -lkahto

all: $(targets)

install: $(targets)
	cp $^ $(directory)

uninstall:
	@rm -vrf $(directory)/$(targets)

clean:
	@rm -vrf $(targets)
