subnautica :main.cpp libbt-sbc-decoder.a
	g++ -gdwarf-4 -O0 $< -lpulse-simple -lpulse -L. -lbt-sbc-decoder -o $@ -fpermissive

libbt-sbc-decoder.a: decoder/srce/alloc.c decoder/srce/bitstream-decode.c decoder/srce/decoder-sbc.c decoder/srce/framing-sbc.c decoder/srce/synthesis-8-generated.c decoder/srce/bitalloc.c decoder/srce/decoder-oina.c decoder/srce/dequant.c decoder/srce/oi_codec_version.c decoder/srce/synthesis-dct8.c decoder/srce/bitalloc-sbc.c decoder/srce/decoder-private.c decoder/srce/framing.c decoder/srce/synthesis-sbc.c
	gcc -gdwarf-4 -O0 -c decoder/srce/*.c -fPIC -I./decoder/include
	ar -cvq $@ *.o
	-@rm *.o

clean:
	-@rm subnautica
	-@rm libbt-sbc-decoder.a
