TARGET=$(PACKAGE).$(LIB_EXTENSION)
VARS=$(wildcard $(VARDIR)/*.txt)
SRCS=$(wildcard $(SRCDIR)/*.c)
OBJS=$(SRCS:.c=.o)


all: preprocess $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(WARNINGS) $(CPPFLAGS) -o $@ -c $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS) $(PLATFORM_LDFLAGS)

preprocess:
	lua ./codegen.lua $(VARS) $(SRCS)

install:
	mkdir -p $(LIBDIR)
	cp $(TARGET) $(LIBDIR)
	rm -f $(OBJS) $(TARGET)
