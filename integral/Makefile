PROJSRCDIR :=$(CURDIR)
PROJBUILDDIR :=$(PROJSRCDIR)/build
VPATH := $(PROJSRCDIR) $(PROJBUILDDIR)

SUBDIRS := server client

EXTOBJS := $(addprefix $(PROJBUILDDIR)/,$(patsubst %.c,%.o,$(wildcard *.c)))

INCDIRS := $(PROJSRCDIR)

CFLAGS := -Wall -Wextra -Werror

.PHONY: all clean subdirs $(SUBDIRS)

all: subdirs 

subdirs: $(SUBDIRS)

export VPATH
export EXTOBJS
export INCDIRS
export CFLAGS
$(SUBDIRS): $(EXTOBJS) | $(PROJBUILDDIR)
	$(MAKE) --no-print-directory -C $@\
    -f .makefile\
    CURBUILDDIR=$(PROJBUILDDIR)/$@\

$(EXTOBJS): $(PROJBUILDDIR)/%.o: %.c | $(PROJBUILDDIR)
	$(CC) $(CFLAGS) -MMD $(addprefix -I,$(INCDIRS)) -c -o $@ $< 

$(PROJBUILDDIR): 
	mkdir $@

clean:
	rm -rf $(PROJBUILDDIR)

-include $(patsubst %.o,%.d,$(EXTOBJS))
