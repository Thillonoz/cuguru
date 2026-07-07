CC = gcc
TARGET = main

DEPENDENCIES = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

run:
	${CC} ${TARGET}.c ${DEPENDENCIES} -o ${TARGET} && ./${TARGET}

clean:
	@rm main

.PHONY: run clean
