.PHONY : all clean cleanall

TARGET = LISPu.com
all : $(TARGET)

SRCS = LISPu.c
OBJS = $(patsubst %.c,%.o,$(SRCS:%.z80=%.o))

AS     = zasm
ASOPT  = 
CC     = zcc +cpm -subtype=x1 -c
CCOPT  = -O3 -DAMALLOC
LK     = zcc +cpm -pragma-output:USING_amalloc -create-app
LKOPT  = 


$(TARGET) : $(OBJS)
	$(LK) $(LKOPT) -o$(@:%.com=%.bin) $<

%.o : %.z80
	$(AS) $(ASOPT) $< $@ $(@:%.bin=%.lst)

%.o : %.c
	$(CC) $(CCOPT) -o$@ $<

clean :
	-del $(OBJS) $(TARGET:%.com=%.bin) zcc_opt.def

cleanall : clean
	-del $(TARGET)

