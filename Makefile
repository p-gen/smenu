# Some variables
RM = rm
CURSES=ncurses

TAG=`if git status >/dev/null 2>&1; then \
  echo "git "; \
else \
  test -f version && cat version; \
fi`

VERSION=`if git status >/dev/null 2>&1; then \
  git log -n 1 --pretty=format:"%h"; \
else \
  echo ""; \
fi`

DATE=`if git status >/dev/null 2>&1; then \
  echo ", build: "\`date +%x_%H:%M_%Z\`; \
else \
  echo ", build: "\`date +%x_%H:%M_%Z\`; \
fi`

# Compilation and link phases options
CFLAGS  = -c -g -O2 -Wall -Wextra
LFLAGS = -l$(CURSES)

# The build target
OBJS = smenu.o
TARGET = smenu

# The rules
all: $(TARGET) $(TARGET).man

.c.o :
	@echo "CC  " $<
	@$(CC) $(CFLAGS) -DVERSION=\""$(TAG)$(VERSION)$(DATE)"\" $<

$(TARGET): $(OBJS)
	@echo CCLD $<
	@$(CC) -o $@ $(OBJS) $(LFLAGS)

clean:
	$(RM) -f $(OBJS) $(TARGET) $(TARGET).man smenu-*.tar.gz core

$(TARGET).man : $(TARGET).1
	@echo MAN
	@nroff -t -c -Tascii -man $(TARGET).1         \
	| colcrt -                                    \
	| sed -e 's/<-,|,|/<-,v,^/'                   \
	> $(TARGET).man

strip:
	strip $(TARGET)

archive:
	@if git status >/dev/null 2>&1; then \
	  V=$(VERSION); \
	  if git archive --prefix=smenu-$${V}/ -o smenu-$${V}.tar.gz $${V}; then \
	    echo "smenu-$${V}.tar.gz written."; \
	  fi \
        else \
	  echo "This is not a git repository."; \
	fi
