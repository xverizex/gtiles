LIBS=`pkg-config gtk+-3.0,libpng,cairo --cflags --libs`
all:
	gcc main.c $(LIBS) -lm -o gtiles
clean:
	rm gtiles
