# flags
CC = gcc
LDLIBS = -ljansson -lreadline -lssl -lcrypto
CFLAGS = -g -Wall

# folders
SRCDIR = src
BINDIR = bin
OBJDIR = obj
RESDIR = res

PROGRAMS = tcp_server tcp_client tls_server tls_client

#==============================================================================

bins = $(PROGRAMS:%=$(BINDIR)/%)

define compile
$(CC) $(CFLAGS) $< -c -o $@
endef

define link
$(CC) $^ -o $@ $(LDLIBS)
endef

vpath %.o $(OBJDIR)
vpath %.c $(SRCDIR)
vpath %.h $(SRCDIR)

all : dir resources $(bins)

dir :
	-@mkdir -p $(OBJDIR)
	-@mkdir -p $(BINDIR)

resources : 
	-@cp -r $(RESDIR)/* $(BINDIR)/

#$(bins): $(addprefix $(OBJDIR)/,$(@F) connection.o)
	#$(link)

#define fin
#$(1): $(addprefix $(OBJDIR)/,$(@F) connection.o)
#	$(link)
#endef
#$(foreach bin,$(bins),$(call $(fin $bin)))


#$(BINDIR)/tcp_server: $(@F:%=%.o) connection.o

$(BINDIR)/tcp_server: $(OBJDIR)/tcp_server.o connection.o
	$(link)

$(BINDIR)/tls_server: $(OBJDIR)/tls_server.o connection.o
	$(link)

$(BINDIR)/tcp_client: $(OBJDIR)/tcp_client.o connection.o
	$(link)

$(BINDIR)/tls_client: $(OBJDIR)/tls_client.o connection.o
	$(link)

$(OBJDIR)/connection.o: connection.c connection.h
	$(compile)

$(OBJDIR)/%.o: %.c
	$(compile)

.PHONY : all clean dir resources

clean : 
	@rm -rf $(BINDIR) $(OBJDIR)
