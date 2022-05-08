TARGET := $(BUILDDIR)/server
INCDIRS := $(CURDIR) $(EXTINCDIRS)
OBJS := $(addprefix $(BUILDDIR)/,$(patsubst %.c,%.o,$(wildcard *.c)))
OPTIONS := $(EXTOPTIONS)
CFLAGS := $(EXTCFLAGS)
CC := $(EXTCC)

.PHONY: all 

all: $(TARGET)

$(TARGET): $(OBJS) $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJS): $(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(OPTIONS) -MMD $(addprefix -I,$(INCDIRS)) -c -o $@ $< 

$(BUILDDIR): 
	mkdir $@

-include $(patsubst %.o,%.d,$(OBJS))
