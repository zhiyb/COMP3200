BIN	= $(wildcard *.raw-*.bin)
PNG	= $(BIN:.bin=.png)
PNG_RAW	= $(PNG:.png=-raw.png)
JPG	= $(BIN:.bin=.jpg)
JPG_RAW	= $(PNG:.png=-raw.jpg)
WIDTH	?= 2592
HEIGHT	?= 1944

.PHONY: all
all: jpg #png_raw

.PHONY: png png_raw jpg jpg_raw
png: $(PNG)
png_raw: $(PNG_RAW)
jpg: $(JPG)
jpg_raw: $(JPG_RAW)

%.jpg: %.bmp
	convert $< $@

%.png: %.bmp
	convert $< $@
#	./bmp2png -F none -Q -o $@ $<
#	bmptopnm < $< | pnmtopng > $@

%.bmp: %.bin conv
	./conv $(WIDTH) $(HEIGHT) $< $@
	chmod 644 $@

%-raw.bmp: %.bin conv
	./conv $(WIDTH) $(HEIGHT) $< $@ raw
	chmod 644 $@

#ov5647.raw-000000.bin: ov5647.tar.gz
#	tar zxvf $^
#	touch $@

conv: main.c
	gcc -g -Wall -Werror -o $@ $^

.PHONY: tidy
tidy:
	rm -f *.png.*

.PHONY: clean
clean: tidy
	rm -f conv $(PNG) $(PNG_RAW) $(JPG) $(JPG_RAW)
