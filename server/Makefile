CC ?= $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra
LDFLAGS := -lpthread

SRC := aesdsocket.c thread_para.c
TARGET = aesdsocket
OBJS := $(SRC:.c=.o)

all: $(TARGET)


$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET)

# Phony targets
.PHONY: all clean
