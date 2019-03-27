
#include "addr.h"
#include "error.h"
#include "mem_access.h"
#include "addr_mng.h"
#include "commands.h"
#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>

int handleRead(command_t* command, FILE* input);
int handleWrite(command_t* command, FILE* input);
int handleTypeSize(command_t* command, FILE* input);
int readUntilNextWhiteSpace(FILE* input, char buffer[]);

/**
 * @brief "Constructor" for program_t: initialize a program.
 * @param program (modified) the program to be initialized.
 * @return ERR_NONE of ok, appropriate error code otherwise.
 * 
 * 
 */
int program_init(program_t* program){
	M_REQUIRE_NON_NULL(program);
	program->nb_lines = 0;
	program->allocated = 10;
	
	virt_addr_t v;
	init_virt_addr(&v,0,0,0,0,0);
	command_t command0 = {0,0,0,0,v}; 
	program->listing = calloc(10, sizeof(command_t));
	M_EXIT_IF_NULL(program->listing, 10*sizeof(command_t));
	
	for (int i = 0 ; i < MAX_SIZE_LISTING ; ++i){
		(program->listing)[i] = command0;
		
	}	
	return ERR_NONE;
}
/**
 * @brief Prints a word as a 32bit word to the output stream
 * Requirements :
 * @param output : non null
 */
int print_word_t_as_word(FILE* output, const word_t word){
	M_REQUIRE_NON_NULL(output);
	fprintf(output, "0x%08" PRIX32 " ", word);
	return ERR_NONE;
}

/**
 * @brief Prints a word as a 8bit word to the output stream
 * Requirements :
 * @param output : non null
 * @return error code bad param or err_none
 */
int print_word_t_as_byte(FILE* output, const word_t word){
	M_REQUIRE_NON_NULL(output);
	fprintf(output, "0x%02" PRIX32 " ", word);
	return ERR_NONE;
}

/**
 * @brief Prints a 64bit uint to the output stream
 * Requirements :
 * @param output : non null
 * @return error code bad param or err_none
 */
int print_uint_64(FILE* output, const uint64_t toPrint){
	M_REQUIRE_NON_NULL(output);
	fprintf(output, "0x%016\n" PRIX64, toPrint);
	return ERR_NONE;
}
/**
 * @brief Print the content of program to the stream "output".
 * 
 * Requirements: 
 * 
 * @param output : must be non null
 * @param program  : must be non null
 * program.order, type, and data_size should be either of their enum values
 * @return error code bad param or err_none
 */
int program_print(FILE* output, const program_t* program){
	M_REQUIRE_NON_NULL(program);
	M_REQUIRE_NON_NULL(output);
	for(int i = 0; i< program->nb_lines; i++){
		command_t com = program->listing[i];
		M_REQUIRE(com.order == READ || com.order == WRITE, ERR_BAD_PARAMETER, "ORDER is neither read nor write%c", ' ');
		M_REQUIRE(com.type == INSTRUCTION || com.type == DATA, ERR_BAD_PARAMETER, "TYPE is neither Instruction nor Data%c", ' ');
		M_REQUIRE(com.data_size == 1 || com.data_size == 4, ERR_BAD_PARAMETER, "DATA_SIZE is neither 1 nor 4%c", ' ');
		
		
		char ord = (com.order == READ) ? 'R' : 'W'; //checks if read or write
		char type = (com.type == INSTRUCTION) ? 'I' : 'D'; //checks if instruction or data
		char size = (com.data_size == 1) ? 'B' : 'W';  //checks if size of byte or word
		uint64_t addr = virt_addr_t_to_uint64_t(&com.vaddr); //convers the virtual address to a uint64
		fprintf(output, "%c %c", ord, type); //prints the order and the type
		
		if(type == 'D'){ //if it is data
			fprintf(output, "%c ", size);
		}else{
			fprintf(output, " "); //simply print a separator if its an instruction
		}
		if(ord == 'W' && type == 'D'){ //IF WE WRITE
			if(size == 'B'){//print the write data as a byte
				print_word_t_as_byte(output, com.write_data);
			}
			else{//print the write data as a word
				print_word_t_as_word(output, com.write_data);
			}
		}
		fprintf(output, "@");
		print_uint_64(output, addr); //prints the virtual address
		
	}
	return ERR_NONE;
	}
	
/**
 * @brief Tool function to down-reallocate memory to the minimal required size. Typically used once a program will no longer be extended.
 * 
 * Requirements
 * 
 * @param program : must be non null
 */
int program_shrink(program_t* program){
	M_REQUIRE_NON_NULL(program);
	return ERR_NONE;
	}	
	
/**
 * @brief add a command (line) to a program. Reallocate memory if necessary.
 * 
 * Requirements:
 * @param program : must be non null
 * @param command : must be non null
 * 		+ according to command.types, data_size should be :
 * 			- 1 or sizeof(word_t)for data
 * 			- sizeof(word_t) for instructions
 *		+ instruction and write cannot go together => can only read instructions
 * 		+ vaddr.page_offset must be a multiple of data_size
 * 
 * Returns ERR_MEM if program already contains 100 commands
 * 
 */
int program_add_command(program_t* program, const command_t* command){
	M_REQUIRE_NON_NULL(program);
	M_REQUIRE_NON_NULL(command);
	
	//taille incorecte
	 M_REQUIRE((command->type == INSTRUCTION)? (command->data_size == sizeof(word_t)): (command->data_size == 1 || command->data_size == sizeof(word_t)), ERR_SIZE, "Data size = %zu, type = %d, must be of size %lu for Instructions and of size 1 or %lu for Data", command->data_size, command->type, sizeof(word_t), sizeof(word_t));
	//on cherche à écrire une instruction
	 M_REQUIRE(!(command->type == INSTRUCTION && command->order == WRITE), ERR_BAD_PARAMETER, "Cannot write with an instruction%c", ' ');
	//addr virtuelle invalide
	 M_REQUIRE((command->vaddr.page_offset % command->data_size == 0), ERR_ADDR, "Page Offset size = %" PRIu16 " must be a multiple of data size", command->vaddr.page_offset);
	
	if (program->allocated == program->nb_lines) {
		return ERR_MEM;
	}
	else {
		// à vérifier si on doit copier les pointeurs ou pas
		program->listing[program->nb_lines] = *command;
		program->nb_lines++;
		return ERR_NONE;
		}	
	}
	
	
//==================================== READ PART ===========================================
#define MAX_SIZE_BUFFER 20
int readCommand(FILE* input, command_t* command){
	M_REQUIRE_NON_NULL(input);
	
	// prepare for reading R or W
	char buffer[MAX_SIZE_BUFFER];
	size_t sizeRead = readUntilNextWhiteSpace(input,buffer);
	M_REQUIRE(sizeRead == 1, ERR_IO, "First character of a line must be 1 (and then followed by a space(' '))%c", ' ');
	
	switch (buffer[0]) {
        case 'R':
            handleRead(command, input);
            break;
        case 'W':
			handleWrite(command, input);
			break;
        default:
			M_REQUIRE(0,ERR_IO, "First character of a line should be R or W%c", ' '); 
            break;
    }
    return ERR_NONE;
	}
int readUntilNextWhiteSpace(FILE* input, char buffer[]){
	M_REQUIRE_NON_NULL(input);
	int c = 0; 
	bool hasReadOneNonWhiteSpace = false; // flag to say that we need to read at lest one non white space char to exit the function
	size_t nbOfBytes = 0;
	while (!feof(input) && !ferror(input) && !(isspace(c) && hasReadOneNonWhiteSpace)){
		c = fgetc(input);
		if (!isspace(c) || c == '\n'){
		    hasReadOneNonWhiteSpace = true;
			if (nbOfBytes >= MAX_SIZE_BUFFER){
				return ERR_IO;
				}
			buffer[nbOfBytes] = c;
			nbOfBytes++;
			}
		}
		
	return nbOfBytes;
	}
	
	/**
	 * Checks if a string contains only hexadecimal characters
	 * 
	 * @return : 1 if the whole string contains only hexadecimal characters, else 0
	 */
int isHexString(char string[], size_t start, size_t length){
	int isHexChar = 1;
	for(int i= start; i<length; i++){ //iterates on full string to check if is an xdigit
		isHexChar = isHexChar && isxdigit(string[i]);
	}
	return isHexChar;
}

	/**
	 * Tool method that calls readUntilNextWhiteSpace in order to fill the type and data_size of a command
	 * Calls readUntilNextWhiteSpace with a buffer in order to get the next String that isnt made of spaces
	 * @param : input : 
	 * @return : Either an error code or ERR_NONE
	 */
int handleTypeSize(command_t* command, FILE* input){
	char buffer[MAX_SIZE_BUFFER];
	size_t s = readUntilNextWhiteSpace(input, buffer);
	M_REQUIRE(s <= 2 && s>0, ERR_BAD_PARAMETER, "SIZE OF TYPE MUST BE INF TO 2%c", ' ');
	buffer[s] = '\0';  //sets the last char in the buffer to \0 in order to use strcmp that compares strings and returns 0 if they are equal
	mem_access_t t;
	size_t data_s;
	if(strcmp(buffer, "I") == 0){  //If the type of the string read is I
		t = INSTRUCTION;
		data_s = sizeof(word_t);
	}
	else if(strcmp(buffer,"DB") == 0){// If the type of the string read is DB
		t = DATA;
		data_s = 1;
	}	
	else if(strcmp(buffer,"DW") == 0){//If the type of the string read is DW
		t = DATA;
		data_s = 4;
	}
	else{
		return ERR_IO;
	}
	buffer[s] = ' ';//removes the \0 after having compared
	command->type = t;
	command->data_size = data_s;
	return ERR_NONE;
}


	/**
	 * Fills the order of the command, calls handleTypeSize to fill the type and size of the command
	 * Fills in the virtual address of the command using readUntilNextWhiteSpace and strtoull to parse the hex string into a uint64
	 * 
	 * @return : Either an error code or ERR_NONE
	 */
int handleRead(command_t* command, FILE* input){
	//Suppose qu'il a déjà lu le R / W
	command->order = READ;
	handleTypeSize(command, input);  //Fills the type and size
	size_t s;
	char buffer[MAX_SIZE_BUFFER];
	command->write_data = 0;  //since it is a read
	//===================VIRT ADDR=================================
	s = readUntilNextWhiteSpace(input, buffer);
	M_REQUIRE(s >= 4 && s <= 20, ERR_BAD_PARAMETER, "SIZE OF virt_addr must be greater than 4%c", ' ');
	M_REQUIRE(buffer[0] == '@', ERR_BAD_PARAMETER, "virt addr must start with @0x%c", ' ');
	M_REQUIRE(buffer[1] == '0', ERR_BAD_PARAMETER, "virt addr must start with @0x%c", ' ');
	M_REQUIRE(buffer[2] == 'x', ERR_BAD_PARAMETER, "virt addr must start with @0x%c", ' ');
	M_REQUIRE(buffer[s-1] == '\n', ERR_BAD_PARAMETER, "virt addr must end with newline%c", ' ');
	for(int i =0; i < 3; i++){ //fills the @0x with spaces so strtoull can parse the hex
		buffer[i] = ' ';
	}
	
	M_REQUIRE(isHexString(buffer, 3, s-1), ERR_BAD_PARAMETER, "IT IS NOT A HEX STRING%c", ' '); //requires that it indeed is a hex string
	uint64_t virt = (uint64_t) strtoull(buffer, (char **)NULL, 16); //unsigned long long to uint64, parses the virtual address in the buffer
	init_virt_addr64(&(command->vaddr), virt);
	return ERR_NONE;
}


	/**
	 * Same as the handleRead method, but also fills in the data_size using readUntilNextWhiteSpace
	 * 
	 */
int handleWrite(command_t* command, FILE* input){
	//Suppose qu'il a déjà lu le R / W
	command->order = WRITE;
	handleTypeSize(command, input); //fills type and size
	char buffer[MAX_SIZE_BUFFER];
	size_t s;
	//=======================WRITE_DATA=========================
	s = readUntilNextWhiteSpace(input, buffer);
	M_REQUIRE(s >= 4 && s <= 8+3, ERR_BAD_PARAMETER, "SIZE OF virt_addr must be greater than 4%c", ' ');
	M_REQUIRE(buffer[0] == '0', ERR_BAD_PARAMETER, "virt addr must start with 0x%c", ' ');
	M_REQUIRE(buffer[1] == 'x', ERR_BAD_PARAMETER, "virt addr must start with 0x%c", ' ');
	
	for(int i =0; i < 2; i++){ //fills with spaces so the parser can work without the 0x
		buffer[i] = ' ';
	}
	M_REQUIRE(isHexString(buffer, 2, s), ERR_BAD_PARAMETER, "IT IS NOT A HEX STRING%c", ' '); //if indeed a hex string
	
	command->write_data = (word_t) strtoull(buffer, (char **)NULL, 16); //unsigned long long to word_t, parses the hex string
	
	
	
	//=======================VIRT ADDR==========================
	s = readUntilNextWhiteSpace(input, buffer);
	M_REQUIRE(s >= 4 &&s <= 20, ERR_BAD_PARAMETER, "SIZE OF virt_addr must be greater than 4%c", ' ');
	M_REQUIRE(buffer[0] == '@', ERR_BAD_PARAMETER, "virt addr must start with @0x%c", ' ');
	M_REQUIRE(buffer[1] == '0', ERR_BAD_PARAMETER, "virt addr must start with @0x%c", ' ');
	M_REQUIRE(buffer[2] == 'x', ERR_BAD_PARAMETER, "virt addr must start with @0x%c", ' ');
	M_REQUIRE(buffer[s-1] == '\n', ERR_BAD_PARAMETER, "virt addr must end with newline%c", ' ');
	for(int i =0; i < 3; i++){ //fills with spaces so the parser can work without the @0x
		buffer[i] = ' ';
	}
	
	M_REQUIRE(isHexString(buffer, 3, s-1), ERR_BAD_PARAMETER, "IT IS NOT A HEX STRING%c", ' '); //if indeed a hex string
	uint64_t virt = (uint64_t) strtoull(buffer, (char **)NULL, 16); //unsigned long long to uint64, parses the hex string
	init_virt_addr64(&(command->vaddr), virt);
	return ERR_NONE;
}
/**
 * @brief Read a program (list of commands) from a file.
 * 
 * Requirements:
 * 
 * @param filename : must be non null
 * @param program : must be non null
 */
int program_read(const char* filename, program_t* program){
	M_REQUIRE_NON_NULL(filename);
	M_REQUIRE_NON_NULL(program);
	FILE* file= NULL;
	file = fopen(filename, "r");
	if (file == NULL) return ERR_IO;
	program_init(program);
	
	
	while (!feof(file) && !ferror(file) ){
		virt_addr_t v;
		init_virt_addr(&v,0,0,0,0,0);
		command_t newC = {0,0,0,0,v}; 
		readCommand(file, &newC);
		program_add_command(program, &newC);
		}
	return ERR_NONE;
	}
