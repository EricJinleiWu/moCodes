DIRS=client server

all:
	for i in $(DIRS); do \
		$(MAKE) -C $$i || exit 1; \
	done

clean:
	for i in $(DIRS); do \
		$(MAKE) -C $$i clean || exit 1; \
	done
	rm -rf ./release
	rm -rf ../genFiles/onvifDiscovery.h ../genFiles/client/* ../genFiles/server/*

gencode:
	#wsdlFile -> headerFile
	../tools/wsdl2h -c -s -o ../genFiles/onvifDiscovery.h ../wsdlFiles/remotediscovery.wsdl

	#headerFile -> sourceFile, include client and server
	../tools/soapcpp2 -c -C -x -L ../genFiles/onvifDiscovery.h
	mv *.nsmap ../genFiles/client
	mv *.c ../genFiles/client
	mv *.h ../genFiles/client
	
	../tools/soapcpp2 -c -S -x -L ../genFiles/onvifDiscovery.h
	mv *.nsmap ../genFiles/server
	mv *.c ../genFiles/server
	mv *.h ../genFiles/server


