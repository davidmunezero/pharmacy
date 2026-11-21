# Pharmacy Management System - IUP (C) build
# Requires MSYS2 UCRT64 GCC and the IUP static libraries in C:/dev/libraries/iup

CC      = gcc
CFLAGS  = -O2 -Wall -Iinclude -Isrc -IC:/dev/libraries/iup/include
LDFLAGS = -LC:/dev/libraries/iup/lib \
          -liup_plot -liupgl -liupcontrols -liupcd -liup \
          -lcdcontextplus -lcdgl -lcd -lim -lfreetype6 -lftgl -lz \
          -lopengl32 -lglu32 -lm -lstdc++ \
          -lkernel32 -luser32 -lgdi32 -lwinspool -lcomdlg32 \
          -ladvapi32 -lshell32 -luuid -loleaut32 -lole32 -lcomctl32 -lgdiplus

SRCS = main.c src/medicine.c src/supplier.c src/sales.c \
       src/history.c src/storage.c src/gui.c

pharmacy: $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

clean:
	rm -f pharmacy.exe

.PHONY: clean
