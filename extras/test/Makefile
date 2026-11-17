# Host-Test mit simuliertem Broker:  make -C extras/test
CXX ?= g++
FLAGS = -std=c++17 -Wall -Wextra -Wno-unused-parameter -g -fsanitize=address,undefined -Istub -I../../src

test:
	@for rx in 128 512 1024; do \
	  echo "== FROQ_RX_BUFFER=$$rx"; \
	  $(CXX) $(FLAGS) -DFROQ_RX_BUFFER=$$rx ../../src/Froq.cpp test.cpp -o froq_test && ./froq_test || exit 1; \
	done
	@echo "== Uhr laeuft bei jedem millis()-Aufruf weiter (wie echte Hardware)"
	@$(CXX) $(FLAGS) -DAUTO_TICK ../../src/Froq.cpp test.cpp -o froq_test && ./froq_test || exit 1
