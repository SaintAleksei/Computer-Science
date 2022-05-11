BUILDDIR :=$(CURDIR)/build
INCDIRS := $(CURDIR)
#OPTIONS := -D INTEGRAL_LOG 
OBJS := $(addprefix $(BUILDDIR)/,$(patsubst %.c,%.o,$(wildcard *.c)))
CFLAGS := -Wall -Wextra -Werror -Wno-cpp --std=c99 -g

export EXTOBJS := $(OBJS)
export EXTINCDIRS := $(INCDIRS)
export EXTOPTIONS := $(OPTIONS)
export EXTCFLAGS := $(CFLAGS)
export EXTCC := $(CC)

SUBDIRS := server client

.PHONY: all clean subdirs $(SUBDIRS)

all: subdirs 

subdirs: $(SUBDIRS)

$(SUBDIRS): $(OBJS) | $(BUILDDIR)
	$(MAKE) -C $@ -f .makefile BUILDDIR=$(BUILDDIR)/$@

$(OBJS): $(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(OPTIONS) -MMD $(addprefix -I,$(INCDIRS)) -c -o $@ $< 

$(BUILDDIR): 
	mkdir $@

clean:
	rm -rf $(BUILDDIR)

-include $(patsubst %.o,%.d,$(EXTOBJS))
