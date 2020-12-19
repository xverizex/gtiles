LIBS=`pkg-config gtk+-3.0,libpng,cairo --cflags --libs`
all:
	gcc main.c $(LIBS) -o gtiles
clean:
	rm gtiles
