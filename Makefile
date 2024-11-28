build: out waver.exe

run: build
	./waver.exe

clean: out
	rmdir /S /Q $<
	del /Q waver.exe

waver.exe: $(patsubst src/%.c,out/%.o,$(wildcard src/*.c))
	gcc -municode -mwindows -o $@ $^ -lwinmm

out/%.o: src/%.c $(wildcard include/*.h)
	gcc -municode -mwindows -c -I include -o $@ $<

out:
	mkdir $@
