build: main.c
	gcc main.c -o main `pkg-config --cflags --libs gstreamer-1.0`

run: build
	./main

clean:
	rm main