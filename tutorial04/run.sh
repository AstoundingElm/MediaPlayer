gcc -o PPlayer tutorial04-resampled.c -lavutil -lavformat -lavcodec -lz -lswscale -lswresample -lavutil -lm $(sdl2-config --cflags --libs) 
