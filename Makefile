CFLAGS=-g
LDFLAGS=-pthread
ALL=server client

ALL:${ALL}

clean:
	\rm -rf ${ALL}
