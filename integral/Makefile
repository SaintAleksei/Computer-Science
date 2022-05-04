BUILDDIR :=$(CURDIR)/build
INCDIRS := $(CURDIR)
OBJS := $(addprefix $(BUILDDIR)/,$(patsubst %.c,%.o,$(wildcard *.c)))
CFLAGS := -Wall -Wextra -Werror -DINTEGRAL_LOG -g

export EXTOBJS := $(OBJS)
export EXTINCDIRS := $(INCDIRS)
export EXTCFLAGS := $(CFLAGS)

SUBDIRS := server client

.PHONY: all clean subdirs $(SUBDIRS)

all: subdirs 

subdirs: $(SUBDIRS)

$(SUBDIRS): $(OBJS) | $(BUILDDIR)
	$(MAKE) -C $@ -f .makefile BUILDDIR=$(BUILDDIR)/$@

$(OBJS): $(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -MMD $(addprefix -I,$(INCDIRS)) -c -o $@ $< 

$(BUILDDIR): 
	mkdir $@

clean:
	rm -rf $(BUILDDIR)

-include $(patsubst %.o,%.d,$(EXTOBJS))
