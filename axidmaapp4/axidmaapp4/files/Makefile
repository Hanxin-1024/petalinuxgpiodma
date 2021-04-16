APP = axidmaapp4

# Add any other object files to this list below
APP_OBJS = axidmaapp4.o util.o demo.o gpioapp.o

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(APP_OBJS) $(LDLIBS) -lpthread

