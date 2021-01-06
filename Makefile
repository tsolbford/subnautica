subnautica :main.cpp
	g++ -gdwarf-4 $< -o $@

clean:
	-@rm subnautica
