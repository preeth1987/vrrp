CC = gcc
CFLAGS = -g
INCFLAGS = 
LDFLAGS = -lpthread
OBJECTS1 = ./libvrrp.so ./vrrpd_impl.o ./state.o ./timer.o ./vrrpd_adv.o
OBJECTS2 = ./libvrrp.so 
LIBS = 

all: vrrpd vrrpadm router sender receiver

vrrpd: ./libvrrp.so ./vrrpd_impl.o ./vrrpd_adv.o ./state.o ./timer.o vrrpd.o
	$(CC) $(CFLAGS) $(OBJECTS1) vrrpd.c $(LDFLAGS) $(LIBS) -o ./vrrpd

vrrpadm: ./libvrrp.so vrrpadm.o
	$(CC) $(CFLAGS) $(OBJECTS2) vrrpadm.c $(LDFLAGS) $(LIBS) -o ./vrrpadm

router:
	$(CC) $(CFLAGS) router.c $(LDFLAGS) -o ./router

sender:
	$(CC) $(CFLAGS) sender.c $(LDFLAGS) -o ./sender

receiver:
	$(CC) $(CFLAGS) receiver.c -o ./receiver

libvrrp.so: 
	$(CC) -shared $(CFLAGS) libvrrp.c -o libvrrp.so

vrrpd.o:
	$(CC) -c $(CFLAGS) vrrpd.c -o  ./vrrpd.o

vrrpadm.o:
	$(CC) -c $(CFLAGS) vrrpadm.c -o ./vrrpadm.o

vrrpd_impl.o:
	$(CC) -c $(CFLAGS) vrrpd_impl.c -o ./vrrpd_impl.o

vrrpd_adv.o:
	$(CC) -c $(CFLAGS) vrrpd_adv.c -o ./vrrpd_adv.o

state.o:
	$(CC) -c $(CFLAGS) state.c -o ./state.o

timer.o:
	$(CC) -c $(CFLAGS) timer.c -o ./timer.o

.SUFFIXES:
.SUFFIXES:	.c .cc .C .cpp .o

.c.o :
	$(CC) -o $@ -c $(CFLAGS) $< $(INCFLAGS)

count:
	wc *.c *.cc *.C *.cpp *.h *.hpp

clean:
	rm -f *.o *.so ./vrrpd ./vrrpadm ./router ./receiver ./sender

.PHONY: all
.PHONY: count
.PHONY: clean
