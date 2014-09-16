CFLAGS = `pkg-config --cflags libdrm_freedreno libdrm`
LDFLAGS = `pkg-config --libs libdrm_freedreno libdrm` -lEGL -lGLESv2 -lX11

egl_dma_buf: egl_dma_buf.o util.o
	gcc -o egl_dma_buf $^ $(LDFLAGS)

%.o: %.c
	gcc -g -c $(CFLAGS) $< -o $@

clean:
	rm -f egl_dma_buf *.o
