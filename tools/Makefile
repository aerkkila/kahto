directory = /usr/local/bin
targets = cplotfun
LDLIBS += -lcplot

all: $(targets)

install: $(targets)
	cp $^ $(directory)

uninstall:
	@rm -vrf $(directory)/$(targets)

clean:
	@rm -vrf $(targets)
