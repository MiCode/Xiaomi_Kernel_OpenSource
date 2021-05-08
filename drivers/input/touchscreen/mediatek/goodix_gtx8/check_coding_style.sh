#!/bin/sh

echo Check Coding Style

if [ x$1 != x ]; then
		FILES=$1
    else
		FILES=*.[ch]
fi
/home/public2/android8.0/kernel/hikey-kernel/scripts/checkpatch.pl \
	--show-types --no-tree \
	--max-line-length=120 --ignore \
	MEMORY_BARRIER,NEW_TYPEDEFS,VOLATILE,LINUX_VERSION_CODE -f \
	${FILES}
