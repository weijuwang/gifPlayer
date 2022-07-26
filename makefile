EXEC:=build/gifPlayer
OBJ:=build/gifPlayer.o
SRC:=src/main.asm

$(EXEC): $(SRC)
	nasm -f elf64 -o $(OBJ) $(SRC)
	gcc -o $(EXEC) $(OBJ) -lncurses

run: $(EXEC)
	./$(EXEC)

clean:
	rm build/*
