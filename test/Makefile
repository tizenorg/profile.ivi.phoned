
CC=g++
OBJ_DIR := .obj

CPP_FILES := $(wildcard ./*.cpp)
OBJ_FILES := $(addprefix $(OBJ_DIR)/,$(notdir $(CPP_FILES:.cpp=.o)))

GIO_LIBS=`pkg-config --libs gio-2.0`
GIO_CFLAGS=`pkg-config --cflags gio-2.0`
DBUS_LIBS=`pkg-config --libs dbus-1`
DBUS_CFLAGS=`pkg-config --cflags dbus-1`
GLIB_LIBS=`pkg-config --libs glib-2.0`
GLIB_CFLAGS=`pkg-config --cflags glib-2.0`
CXX_CFLAGS = -std=c++11

all: phone

phone: $(OBJ_FILES)
	$(CC) $(GIO_LIBS) $(DBUS_LIBS) $(GLIB_LIBS) -pthread -o $@ $^

$(OBJ_DIR)/%.o: ./%.cpp $(OBJ_DIR)
	$(CC) $(GIO_CFLAGS) $(DBUS_CFLAGS) $(GLIB_CFLAGS) $(CXX_CFLAGS) -c -o $@ $<

$(OBJ_DIR):
	test -d $@ || mkdir $@

clean:
	rm -f $(OBJ_DIR)/*.o phone

.PHONY: all clean phone

