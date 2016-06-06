# définition des variables
CFLAGS = -W -Wall -v -g
LDFLAGS=-pthread -ldl -lrt -lnfc_nci_linux
EXEC=HandoverDemo
INSTALL_DEMO_PATH ?= $(INSTALL_PATH)/usr/bin
# all
all: $(EXEC)

$(EXEC): main.o LibNfcManager.o Demo.o
	$(CXX)  main.o LibNfcManager.o Demo.o -o $(EXEC) $(LDFLAGS)

Demo.o : ./src/Demo.cpp ./inc/Demo.hpp
	$(CXX) $(LDFLAGS) -c ./src/Demo.cpp -o Demo.o $(CFLAGS)

LibNfcManager.o : ./src/LibNfcManager.cpp ./inc/LibNfcManager.hpp
	$(CXX) $(LDFLAGS) -c ./src/LibNfcManager.cpp -o LibNfcManager.o $(CFLAGS)

main.o: ./src/main.cpp ./inc/LibNfcManager.hpp ./inc/Demo.hpp
	$(CXX) $(LDFLAGS) -c ./src/main.cpp -o main.o $(CFLAGS)


install: all
	mkdir -p $(INSTALL_DEMO_PATH)
	install -m 755 $(EXEC) $(INSTALL_DEMO_PATH)

# clean
clean:
	rm -rf *.bak rm -rf *.o
 
# mrproper
mrproper: clean
	rm -rf $(EXEC)

.PHONY: all install clean mrproper
