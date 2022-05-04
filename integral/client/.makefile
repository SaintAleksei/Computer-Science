TARGET := $(BUILDDIR)/client
INCDIRS := $(CURDIR) $(EXTINCDIRS)
CFLAGS := $(EXTCFLAGS)
OBJS := $(addprefix $(BUILDDIR)/,$(patsubst %.c,%.o,$(wildcard *.c)))
LIBS := -lpthread -lm

.PHONY: all 

all: $(TARGET)

$(TARGET): $(OBJS) $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) 

$(OBJS): $(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -MMD $(addprefix -I,$(INCDIRS)) -c -o $@ $< 

$(BUILDDIR): 
	mkdir $@

-include $(patsubst %.o,%.d,$(OBJS))
