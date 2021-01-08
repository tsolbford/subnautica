subnautica :main.cpp libbt-sbc-decoder.a
	g++ -gdwarf-4 $< -lpulse-simple -lpulse -L. -lbt-sbc-decoder -o $@

libbt-sbc-decoder.a: decoder/srce/*.c
	gcc -c $< -fPIC -I./decoder/include
	ar -cvq $@ *.o
	-@rm *.o

clean:
	-@rm subnautica
	-@rm libbt-sbc-decoder.a
