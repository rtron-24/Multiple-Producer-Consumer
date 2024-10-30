CC := gcc
PRO_NAME = program

all: make_my_program
	@echo "Done" 

make_my_program:
	@echo "creating program binary: " $(PRO_NAME)
	@$(CC) solution.c -o $(PRO_NAME) -lpthread -lrt

clean:
	@echo "removing files"
	@rm -f $(PRO_NAME)
	@echo "removed file " $(PRO_NAME)
