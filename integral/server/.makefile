TARGET := $(CURBUILDDIR)/server
VPATH += $(CURDIR) $(CURBUILDDIR)
INCDIRS += $(CURDIR)
OBJS := $(addprefix $(CURBUILDDIR)/,$(patsubst %.c,%.o,$(wildcard *.c)))

.PHONY: all 

all: $(TARGET)

$(TARGET): $(OBJS) $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJS): $(CURBUILDDIR)/%.o: %.c | $(CURBUILDDIR)
	$(CC) $(CFLAGS) -MMD $(addprefix -I,$(INCDIRS)) -c -o $@ $< 

$(CURBUILDDIR): 
	mkdir $@

-include $(patsubst %.o,%.d,$(OBJS))
