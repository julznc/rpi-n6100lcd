
FFMPEG = /home/pi/ffmpeg-2.8.6

all:
	g++ -c n6100lcd.cpp -o n6100lcd.o
	g++ -c pngtest.cpp -o pngtest.o
	g++ pngtest.o n6100lcd.o -lpng -o pngtest
	g++ -I$(FFMPEG) -c mpgtest.cpp -o mpgtest.o
	g++ mpgtest.o n6100lcd.o -lavformat -lavcodec -lavutil -lswscale -o mpgtest

clean:
	rm -f *.o pngtest mpgtest
