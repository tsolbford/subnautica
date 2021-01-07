subnautica :main.cpp
	g++ -gdwarf-4 $< -lpulse-simple -lpulse -o $@

clean:
	-@rm subnautica
