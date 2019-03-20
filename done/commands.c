
#include "addr.h"
#include "error.h"
#include "mem_access.h"
#include "addr_mng.h"
#include "commands.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>

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
	program->allocated = sizeof(program->listing);
	
	virt_addr_t v;
	init_virt_addr(&v,0,0,0,0,0);
	command_t command0 = {0,0,0,0,v}; 
	for (int i = 0 ; i < program->allocated ; ++i){
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
	fprintf(output, "0x%08" PRIX32, word);
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
	fprintf(output, "0x%02" PRIX32, word);
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
	fprintf(output, "0x%016" PRIX64, toPrint);
	return ERR_NONE;
}
/**
 * @brief Print the content of program to the stream "output".
 * 
 * Requirements: 
 * 
 * @param output : must be non null
 * @param program  : must be non null
 * @return error code bad param or err_none
 */
int program_print(FILE* output, const program_t* program){
	M_REQUIRE_NON_NULL(program);
	M_REQUIRE_NON_NULL(output);
	for(int i = 0; i< program->nb_lines; i++){
		command_t com = program->listing[i];
		M_REQUIRE(com.order == READ || com.order == WRITE, ERR_BAD_PARAMETER, "ORDER is neither read nor write");
		M_REQUIRE(com.type == INSTRUCTION || com.type == DATA, ERR_BAD_PARAMETER, "TYPE is neither Instruction nor Data");
		M_REQUIRE(com.data_size == 1 || com.data_size == 4, ERR_BAD_PARAMETER, "DATA_SIZE is neither 1 nor 4");
		
		
		char ord = (com.order == READ) ? 'R' : 'W';
		char type = (com.type == INSTRUCTION) ? 'I' : 'D';
		char size = (com.data_size == 1) ? 'B' : 'W';
		uint64_t addr = virt_addr_t_to_uint64_t(&com.vaddr);
		fprintf(output, "%c %c", ord, type);
		if(type == 'D'){ //if it is data
			fprintf(output, "%c ", size);
		}
		if(ord == 'W' && type == 'D'){ //IF WE WRITE
			if(size == 'B'){
				print_word_t_as_byte(output, com.write_data);
			}
			else{
				print_word_t_as_word(output, com.write_data);
			}
		}
		fprintf(output, "@");
		print_uint_64(output, addr);
		
		
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
	 M_REQUIRE((command->type == INSTRUCTION)? (command->data_size == sizeof(word_t)): (command->data_size == 1 || command->data_size == sizeof(word_t)), ERR_SIZE, "Data size = %zu, type = %d, must be of size %d for Instructions and of size 1 or %d for Data", command->data_size, command->type, sizeof(word_t), sizeof(word_t));
	//on cherche à écrire une instruction
	 M_REQUIRE(!command->type == INSTRUCTION || command->order == WRITE, ERR_BAD_PARAMETER, "Cannot write with an instruction");
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
command_t readCommand(FILE* input){
	M_REQUIRE_NON_NULL(input);
	
	// create empty command
	virt_addr_t v;
	init_virt_addr(&v,0,0,0,0,0);
	command_t command = {0,0,0,0,v}; 
	
	// prepare for reading R or W
	char buffer[MAX_SIZE_BUFFER];
	size_t sizeRead = readUntilNextWhiteSpace(input,buffer);
	M_REQUIRE(sizeRead == 1, ERR_IO, "First character of a line must be 1 (and then followed by a space(' '))");
	
	switch (buffer[0]) {
        case 'R':
            handleRead(&command);
            break;
        case 'W':
			handleWrite(&command);
			break;
        default:
			M_REQUIRE_NON_NULL_CUSTOM_ERR(NULL,ERR_IO, "First character of a line should be R or W"); 
            break;
    }
    return command
	}
size_t readUntilNextWhiteSpace(FILE* input, char buffer[]){
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
size_t handleTypeSize(command_t* command, FILE* input){
	char buffer[MAX_SIZE_BUFFER];
	size_t s = readUntilNextWhiteSpace(input, buffer);
	M_REQUIRE(s <= 2 && s>0, ERR_BAD_PARAMETER, "SIZE OF TYPE MUST BE INF TO 2");
	buffer[s] = '\0';
	mem_access_t t;
	size_t data_s;
	if(strcmp(buffer, "I") == 0){
		t = INSTRUCTION;
		data_s = sizeof(word_t);
	}
	else if(strcmp(buffer,"DB") == 0){
		t = DATA;
		data_s = 1;
	}	
	else if(strcmp(buffer,"DW") == 0){
		t = DATA;
		data_s = 4;
	}
	else{
		return ERR_IO;
	}
	buffer[s] = ' ';
	command->type = t;
	command->data_size = data_s;
	return ERR_NONE;
}
size_t handleRead(command_t* command, FILE* input){
	//Suppose qu'il a déjà lu le R / W
	command->order = READ;
	handleTypeSize(command, input);
	size_t s;
	char buffer[MAX_SIZE_BUFFER];
	command->write_data = 0;
	//===================VIRT ADDR=================================
	s = readUntilNextWhiteSpace(input, buffer);
	M_REQUIRE(s >= 4, ERR_BAD_PARAMETER, "SIZE OF virt_addr must be greater than 4");
	M_REQUIRE(buffer[0] == '@', ERR_BAD_PARAMETER, "virt addr must start with @0x");
	M_REQUIRE(buffer[1] == '0', ERR_BAD_PARAMETER, "virt addr must start with @0x");
	M_REQUIRE(buffer[2] == 'x', ERR_BAD_PARAMETER, "virt addr must start with @0x");
	for(int i =0; i < 3; i++){
		buffer[i] = ' ';
	}
	uint64_t virt = (uint64_t) strtoull(buffer, (char **)NULL, 16); //unsigned long long to uint64
	init_virt_addr64(&(command->vaddr), virt);
	return ERR_NONE;
}
size_t handleWrite(command_t* command, FILE* input){
	//Suppose qu'il a déjà lu le R / W
	command->order = WRITE;
	handleTypeSize(command, input);
	char buffer[MAX_SIZE_BUFFER];
	size_t s;
	//=======================WRITE_DATA=========================
	s = readUntilNextWhiteSpace(input, buffer);
	M_REQUIRE(s >= 4 && s <= 8+3, ERR_BAD_PARAMETER, "SIZE OF virt_addr must be greater than 4");
	M_REQUIRE(buffer[0] == '@', ERR_BAD_PARAMETER, "virt addr must start with @0x");
	M_REQUIRE(buffer[1] == '0', ERR_BAD_PARAMETER, "virt addr must start with @0x");
	M_REQUIRE(buffer[2] == 'x', ERR_BAD_PARAMETER, "virt addr must start with @0x");
	for(int i =0; i < 3; i++){
		buffer[i] = ' ';
	}
	command->write_data = (word_t) strtoul(buffer, (char **)NULL, 16); //unsigned long long to uint64
	
	
	
	//=======================VIRT ADDR==========================
	s = readUntilNextWhiteSpace(input, buffer);
	M_REQUIRE(s >= 4 &&s <= 16+3, ERR_BAD_PARAMETER, "SIZE OF virt_addr must be greater than 4");
	M_REQUIRE(buffer[0] == '@', ERR_BAD_PARAMETER, "virt addr must start with @0x");
	M_REQUIRE(buffer[1] == '0', ERR_BAD_PARAMETER, "virt addr must start with @0x");
	M_REQUIRE(buffer[2] == 'x', ERR_BAD_PARAMETER, "virt addr must start with @0x");
	for(int i =0; i < 3; i++){
		buffer[i] = ' ';
	}
	uint64_t virt = (uint64_t) strtoull(buffer, (char **)NULL, 16); //unsigned long long to uint64
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
	File* file= NULL;
	file = fopen(filename);
	if (file == NULL) return ERR_IO;
	program_init(program);
	
	
	while (!feof(file) && !ferror(file) {
		program_add_command(program, readCommand(file))
		}
	return ERR_NONE;
	}
