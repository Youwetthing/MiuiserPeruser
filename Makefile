CC = clang
CFLAGS = -Wall -Wextra -O2 -I. -Iinclude -Iinclude/superhero -Isrc/include -Isrc/core/include -Isrc/core/detection -Isrc/daemon -Isrc/core
LDFLAGS = -lcrypto -lssl -lpthread -ldl

# We are only including the files that don't conflict
# leo_detection_utils.c now handles the logic for the "Brothers"
SRC = src/main.c \
	src/superhero/leo_detection.c \
	src/superhero/leo_detection_utils.c \
	src/core/superhero_mode.c \
 	src/core/fugitoid_log_impl.c \
	src/core/platform/rish_pipe.c \
	src/core/platform/april_linux.c \
	src/core/sensei_core.c

OBJ = $(SRC:.c=.o)
TARGET = miuiserperuser-daemon

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(OBJ) $(TARGET)
