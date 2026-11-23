# Используем pg_config для получения путей
PG_CONFIG = pg_config
INCLUDEDIR = $(shell $(PG_CONFIG) --includedir-server)
LIBDIR     = $(shell $(PG_CONFIG) --pkglibdir)
SHAREDIR   = $(shell $(PG_CONFIG) --sharedir)/extension
UNAME_S :=   $(shell uname -s)

# Определяем расширение библиотеки
ifeq ($(UNAME_S),Darwin)
    DLSUFFIX = .dylib
    LDFLAGS = -shared -undefined dynamic_lookup -install_name @rpath/pg_printf.dylib
else
    DLSUFFIX = .so
    LDFLAGS = -shared
endif

TARGET = pg_printf$(DLSUFFIX)

# Флаги компиляции: -fPIC обязателен для всех объектных файлов расширений
CFLAGS = -I$(INCLUDEDIR) -fPIC -O2

all: $(TARGET) pg_printf--1.0.sql

# Сборка объектного файла
pg_printf.o: pg_printf.c
	$(CC) $(CFLAGS) -c $< -o $@

# Сборка разделяемой библиотеки
$(TARGET): pg_printf.o
	$(CC) $(LDFLAGS) -o $@ $^

pg_printf--1.0.sql: printf.sql
	cat $^ > $@

install: all
	cp $(TARGET) $(LIBDIR)/
	cp pg_printf.control $(SHAREDIR)/
	cp pg_printf--1.0.sql $(SHAREDIR)/

clean:
	rm -f *.o $(TARGET) pg_printf--1.0.sql

.PHONY: all install clean
