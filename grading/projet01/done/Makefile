## ======================================================================
## partial Makefile provided to students
##

CFLAGS = -std=c11 -Wall -Wpedantic -g -ffunction-sections

# a bit more checks if you'd like to (uncomment
# CFLAGS += -Wextra -Wfloat-equal -Wshadow                         \
# -Wpointer-arith -Wbad-function-cast -Wcast-align -Wwrite-strings \
# -Wconversion -Wunreachable-code

# uncomment if you want to add DEBUG flag
CPPFLAGS += -DDEBUG

# ----------------------------------------------------------------------
# feel free to update/modifiy this part as you wish

# all those libs are required on Debian, feel free to adapt it to your box
LDLIBS += -lcheck -lm -lrt -pthread -lsubunit

all:: test-commands
test-addr :: test-addr.c tests.h util.h addr.h addr_mng.o error.o

test-memory:: test-memory.c commands.o page_walk.o memory.o error.o addr_mng.o 
test-commands: test-commands.c commands.o addr_mng.o error.o 
test-tlb_simple:: test-tlb_simple.c error.o util.h addr_mng.o addr.h commands.o mem_access.h memory.o list.o tlb.h tlb_mng.o page_walk.o


tlb_mng.o: tlb_mng.c tlb.h addr.h addr_mng.o tlb_mng.h list.o page_walk.o error.o
list.o:: list.c list.h error.o
memory.o :: memory.c memory.h page_walk.o util.h addr_mng.o error.o addr.h
page_walk.o :: page_walk.c addr.h error.h addr_mng.o 
error.o :: error.c error.h
commands.o ::  commands.c commands.h addr.h mem_access.h addr_mng.o error.o
addr_mng.o :: addr_mng.c addr_mng.h error.o 

# ----------------------------------------------------------------------
# This part is to make your life easier. See handouts how to make use of it.

clean::
	-@/bin/rm -f *.o *~ $(CHECK_TARGETS)

new: clean all

static-check:
	scan-build -analyze-headers --status-bugs -maxloop 64 make CC=clang new

style:
	astyle -n -o -A8 *.[ch]

# all those libs are required on Debian, adapt to your box
$(CHECK_TARGETS): LDLIBS += -lcheck -lm -lrt -pthread -lsubunit

check:: $(CHECK_TARGETS)
	$(foreach target,$(CHECK_TARGETS),./$(target);)

# target to run tests
check:: all
	@if ls tests/*.*.sh 1> /dev/null 2>&1; then \
	  for file in tests/*.*.sh; do [ -x $$file ] || echo "Launching $$file"; ./$$file || exit 1; done; \
	fi

IMAGE=arashpz/feedback
feedback:
	@docker run -it --rm -v ${PWD}:/home/tester/done $(IMAGE)

SUBMIT_SCRIPT=../provided/submit.sh
submit1: $(SUBMIT_SCRIPT)
	@$(SUBMIT_SCRIPT) 1

submit2: $(SUBMIT_SCRIPT)
	@$(SUBMIT_SCRIPT) 2

submit:
	@printf 'what "make submit"??\nIt'\''s either "make submit1" or "make submit2"...\n'
