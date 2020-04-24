
TARGET=tritontalk

CC = cc
DEBUG = -g

LDFLAGS = -lresolv -lpthread -lm

CCFLAGS = -std=c11 -Wall -Wextra -pedantic -Werror=implicit-function-declaration $(DEBUG)

# add object file names here
OBJS = main.o util.o input.o communicate.o sender.o receiver.o

all: tritontalk

%.o : %.c
	$(CC) -c $(CCFLAGS) $<

%.o : %.cc
	$(CC) -c $(CCFLAGS) $<

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CCFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET) core *.o *~

submit: clean
	rm -f project1.tgz; tar czvf project1.tgz *; turnin project1.tgz -c cs123f -p project1
