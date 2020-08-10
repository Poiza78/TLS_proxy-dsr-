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

define compile
$(CC) $(CFLAGS) $< -c -o $@
endef

define link
$(CC) $^ -o $@ $(LDLIBS)
endef

#vpath %.o $(OBJDIR)
#vpath %.c $(SRCDIR)
#vpath %.h $(SRCDIR)

all : dir res $(addprefix $(BINDIR)/,$(PROGRAMS))

dir :
	- @mkdir -p $(OBJDIR)
	- @mkdir -p $(BINDIR)

res : 
	@cp -r $(RESDIR)/* $(BINDIR)/

$(BINDIR)/tcp_server: $(addprefix $(OBJDIR)/,tcp_server.o connection.o)
	$(link)

$(BINDIR)/tls_server: $(addprefix $(OBJDIR)/,tls_server.o connection.o)
	$(link)

$(BINDIR)/tcp_client: $(addprefix $(OBJDIR)/,tcp_client.o connection.o)
	$(link)

$(BINDIR)/tls_client: $(addprefix $(OBJDIR)/,tls_client.o connection.o)
	$(link)

$(OBJDIR)/connection.o: $(addprefix $(SRCDIR)/,connection.c connection.h)
	$(compile)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(compile)

.PHONY : all clean res

clean : 
	@rm -rf $(BINDIR) $(OBJDIR)
