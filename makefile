CC = gcc
CFLAGS = -Wall -Wextra -pthread
LDFLAGS = -pthread

TARGETS = admin client server

# Règles
all: $(TARGETS)

admin: admin.c incident.h
	$(CC) $(CFLAGS) -o $@ admin.c

client: client.c incident.h
	$(CC) $(CFLAGS) -o $@ client.c $(LDFLAGS)

server: server.c incident.h
	$(CC) $(CFLAGS) -o $@ server.c $(LDFLAGS)

# Nettoyage
clean:
	rm -f $(TARGETS)

distclean: clean
	rm -f *~

# Recompilation complète
rebuild: clean all

# Pour éviter les conflits avec des fichiers nommés clean, all, etc.
.PHONY: all clean distclean rebuild