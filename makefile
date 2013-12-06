CC = gcc
CFLAGS = -DDEBUG -Wall -g
TESTFLAGS = -DTEST

OBJS_proxy = proxy.o http_parser.o http_replyer.o helper.o mydns.o dns_lib.o list.o graph.o
OBJS_nameserver = http_parser.o http_replyer.o helper.o mydns.o dns_lib.o nameserver.o list.o graph.o
BINS_TEST = mydns_test nameserver_test dns_lib_test
BINS = proxy nameserver 

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

all: $(BINS) $(BINS_TEST)

proxy: $(OBJS_proxy)
	$(CC) $(CFLAGS) $^ -o $@

nameserver: $(OBJS_nameserver)
	$(CC) $(CFLAGS) $^ -o $@


# run
run1cp1:
	./proxy logfile1 0.5 8888 1.0.0.1 5.0.0.1 9999 3.0.0.1
run2cp1:
	./proxy logfile2 0.5 8889 2.0.0.1 5.0.0.1 9999 4.0.0.1
run1cp2:
	./proxy logfile1 0.5 8888 1.0.0.1 5.0.0.1 9999
run2cp2:
	./proxy logfile2 0.5 8889 2.0.0.1 5.0.0.1 9999
run3cp2:
	./proxy logfile3 0.5 8889 3.0.0.1 5.0.0.1 9999

runbothcp1:
	./proxy logfile1 0.5 8888 1.0.0.1 5.0.0.1 9999 3.0.0.1 >printf1.txt & ./proxy logfile2 0.5 8889 2.0.0.1 5.0.0.1 9999 4.0.0.1 > printf2.txt &

runbothcp2:
	./proxy logfile1 0.5 8888 1.0.0.1 5.0.0.1 9999 >printf1.txt & ./proxy logfile2 0.5 8889 2.0.0.1 5.0.0.1 9999 > printf2.txt &

runbothcp2_0.1:
	./proxy logfile1_0.1 0.1 8888 1.0.0.1 5.0.0.1 9999 >printf1.txt & ./proxy logfile2_0.1 0.1 8889 2.0.0.1 5.0.0.1 9999 > printf2.txt &

runbothcp2_0.5:
	./proxy logfile1_0.5 0.5 8888 1.0.0.1 5.0.0.1 9999 >printf1.txt & ./proxy logfile2_0.5 0.5 8889 2.0.0.1 5.0.0.1 9999 > printf2.txt &

runbothcp2_0.9:
	./proxy logfile1_0.9 0.9 8888 1.0.0.1 5.0.0.1 9999 >printf1.txt & ./proxy logfile2_0.9 0.9 8889 2.0.0.1 5.0.0.1 9999 > printf2.txt &



rundns1_r:
	./nameserver -r nameserver_log 5.0.0.1 9999 ./topos/topo1/topo1.servers ./topos/topo1/topo1.lsa

rundns1_g:
	./nameserver nameserver_log 5.0.0.1 9999 ./topos/topo1/topo1.servers ./topos/topo1/topo1.lsa

rundns2_r:
	./nameserver -r nameserver_log 5.0.0.1 9999 ./topos/topo2/topo2.servers ./topos/topo2/topo2.lsa

rundns2_g:
	./nameserver nameserver_log 5.0.0.1 9999 ./topos/topo2/topo2.servers ./topos/topo2/topo2.lsa



# test
mydns_test: mydns.c dns_lib.o
	$(CC) $(CFLAGS) $(TESTFLAGS) $^ -o $@

nameserver_test: nameserver.c dns_lib.o list.o graph.o
	$(CC) $(CFLAGS) $(TESTFLAGS) $^ -o $@

dns_lib_test: dns_lib.c
	$(CC) $(CFLAGS) $(TESTFLAGS) $^ -o $@

clean:
	rm -rf $(OBJS_proxy) $(OBJS_nameserver) $(BINS) $(BINS_TEST) *.dSYM *.~
