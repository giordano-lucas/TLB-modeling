
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

// ================= PROTOTYPES ==================
int handleRead(command_t* command, FILE* input);
int handleWrite(command_t* command, FILE* input);
int handleTypeSize(command_t* command, FILE* input);
int readUntilNextWhiteSpace(FILE* input, char buffer[], size_t* s);
int program_resize(program_t* prog, size_t newSize);
// ===============================================

// starting number of allocated commands 
#define START_COMMANDS_ALLOCATED 10
/**
 * @brief "Constructor" for program_t: initialize a program.
 * @param program (modified) the program to be initialized
 * @return ERR_NONE of ok, appropriate error code otherwise.
 * 
 * Requirements :
 * 
 * @parm program : must be non null
 * 
 */
int program_init(program_t* program){
	M_REQUIRE_NON_NULL(program);
	
	program->nb_lines = 0;
	//allocated has its sized defined in memory, cant overflow right now
	program->allocated = START_COMMANDS_ALLOCATED*sizeof(command_t);
	 
	// dynamic allocation
	
	program->listing = calloc(START_COMMANDS_ALLOCATED, sizeof(command_t));
	
	M_REQUIRE_NON_NULL(program->listing); // error case
	M_EXIT_IF_NULL(program->listing, START_COMMANDS_ALLOCATED*sizeof(command_t)); // error case
	
	
	return ERR_NONE;
}
/**
 * @brief Prints a word as a 32bit word to the output stream
 * Requirements :
 * @param output : must be non null
 */
int print_word_t_as_word(FILE* output, const word_t word){
	M_REQUIRE_NON_NULL(output);
	fprintf(output, "0x%08" PRIX32 " ", word);
	return ERR_NONE;
}

/**
 * @brief Prints a word as a 8bit word to the output stream
 * Requirements :
 * @param output : must be non null
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
 * @param output : must be non null
 * @return error code bad param or err_none
 */
int print_uint_64(FILE* output, const uint64_t toPrint){
	M_REQUIRE_NON_NULL(output);
	fprintf(output, "0x%016" PRIX64 "\n", toPrint);
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
	M_REQUIRE_NON_NULL(program->listing);
	
	for(int i = 0; i< program->nb_lines; i++){
		command_t com = program->listing[i];
		
		// validation of com
		M_REQUIRE(com.order == READ || com.order == WRITE, ERR_BAD_PARAMETER, "ORDER is neither read nor write%c", ' ');
		M_REQUIRE(com.type == INSTRUCTION || com.type == DATA, ERR_BAD_PARAMETER, "TYPE is neither Instruction nor Data%c", ' ');
		M_REQUIRE(com.data_size == sizeof(byte_t) || com.data_size == sizeof(word_t), ERR_BAD_PARAMETER, "DATA_SIZE is neither 1 nor 4%c", ' ');
		
		// processing
		char ord = (com.order == READ) ? 'R' : 'W'; //checks if read or write
		char type = (com.type == INSTRUCTION) ? 'I' : 'D'; //checks if instruction or data
		char size = (com.data_size == sizeof(byte_t)) ? 'B' : 'W';  //checks if size of byte or word
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
	
	if(program->nb_lines > 0){
		while(program->allocated/sizeof(command_t) > program->nb_lines){
			// resize to nb_lines
			M_REQUIRE((program_resize(program, program->nb_lines) == ERR_NONE), ERR_MEM, "Could not resize %c", '\0');
			}
	}
	else{
		// if nb_lines == 0 reset the lines to 10
		M_REQUIRE(program_resize(program, START_COMMANDS_ALLOCATED) == ERR_NONE, ERR_MEM, "Could not resize %c", '\0');
	}
	return ERR_NONE;
	}	
	
	/**
	 * Helper function to resize a program to the newSize given
	 * 
	 * Requirements 
	 * 
	 * @param prog : program to be resized, must initialized and must be non null
	 * @return an error code in case of an error
	 * 
	 */
	int program_resize(program_t* prog, size_t newSize){
		
		M_REQUIRE_NON_NULL(prog);
		M_REQUIRE_NON_NULL(prog->listing);

		program_t copy = *prog;
		copy.allocated = newSize*sizeof(command_t);
		
		// check overflow multiplication and test reallocation
		if(newSize > SIZE_MAX/sizeof(command_t) || (copy.listing = realloc(copy.listing, copy.allocated)) == NULL ){
				return ERR_MEM;
		}
		*prog = copy;
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
 #define BYTE_MAX 32767
int program_add_command(program_t* program, const command_t* command){
	M_REQUIRE_NON_NULL(program);
	M_REQUIRE_NON_NULL(command);
	M_REQUIRE_NON_NULL(program->listing);
	M_REQUIRE(command->type == INSTRUCTION || command->type == DATA, ERR_BAD_PARAMETER, "Type must be either an instruction or data", "");
	M_REQUIRE(command->order == WRITE || command->order == READ, ERR_BAD_PARAMETER, "Order must be either a write or a read", "");
	M_REQUIRE(!(command->order == READ && command->write_data > 0), ERR_BAD_PARAMETER, "No data size when reading, data size : %d, order : %d", command->data_size, command->order);
	M_REQUIRE(command->write_data <= BYTE_MAX || command->data_size == sizeof(word_t), ERR_BAD_PARAMETER, "Data size is a byte but the actual size is bigger than a byte" ,"");
	//incorect size
	 M_REQUIRE((command->type == INSTRUCTION)? (command->data_size == sizeof(word_t)): (command->data_size == sizeof(byte_t) || command->data_size == sizeof(word_t)), ERR_BAD_PARAMETER, "Data size = %zu, type = %d, must be of size %lu for Instructions and of size 1 or %lu for Data", command->data_size, command->type, sizeof(word_t), sizeof(word_t));
	// Cannot write with an instruction
	 M_REQUIRE(!(command->type == INSTRUCTION && command->order == WRITE), ERR_BAD_PARAMETER, "Cannot write with an instruction%c", ' ');
	//invalid  virtual addr 
	 M_REQUIRE((command->vaddr.page_offset % command->data_size == 0), ERR_BAD_PARAMETER, "Page Offset size = %" PRIu16 " must be a multiple of data size", command->vaddr.page_offset);
	 M_REQUIRE(program->nb_lines < SIZE_MAX/sizeof(command_t), ERR_SIZE, "nb_lines is too big to reallocate %c",' ');
	while(program->nb_lines*sizeof(command_t) >= program->allocated){
		
		M_REQUIRE(program_resize(program, 2*(program->allocated/sizeof(command_t))) == ERR_NONE, ERR_MEM, "Could not resize %c", ' ');
	}
	
	program->listing[program->nb_lines] = *command;
	program->nb_lines++;
	
	return ERR_NONE;
	
	}
	
	
//==================================== READ PART ===========================================
#define MAX_SIZE_BUFFER 20
#define VALID 1
#define INVALID 0
// the longest char we can read by calling readUntilNextWhiteSpace is the address that is 
// represented on 16 bits but readUntilNextWhiteSpace also include "@0x" in front of the address and maybe '\n' at the end if it was
// the end of the line  which leads to 16+3+1 = 20 character maximum

/**
 * @brief Reads a command from input (a line of it) and writes it into the command structure
 * 
 * 
 * @param input : must be non null, file where the command could be found.
 * @param command : must be non null, command where the command read should be written
 * @return ERR_NONE or another ERR in case of an error
 **/
int readCommand(FILE* input, command_t* command, int* validCommand){
	M_REQUIRE_NON_NULL(input);
	M_REQUIRE_NON_NULL(validCommand);
	// prepare for reading R or W
	char buffer[MAX_SIZE_BUFFER];
	
	// read the first character of a command (should be 'R' on 'W')
	size_t sizeRead;
	int err;
	if ( (err = readUntilNextWhiteSpace(input,buffer, &sizeRead))!= ERR_NONE) return err;
	M_REQUIRE(sizeRead == 1, ERR_IO, "First character of a line must be 1 (and then followed by a space(' '))%c", ' ');
	*validCommand = VALID; //command is supposed valid
    
	switch (buffer[0]) {
        case 'R':
            if ((err= handleRead(command, input))!= ERR_NONE) return err;//error propagation
            break;
        case 'W':
			if ((err = handleWrite(command, input))!= ERR_NONE) return err; //error propagation
			break;
		case -1: //end of file character
			*validCommand = INVALID; //if we are at the end of file : dont add a command
			break;
        default:
			M_REQUIRE(0,ERR_IO, "First character of a line should be R or W and not %zu", (size_t)buffer[0] ); 
            break;
    }
    return ERR_NONE;
	}

/**
 * Reads the stream input until it finds a whitespace (in the sense of isspace) on the end of the file
 * and store the char read in the buffer argument and returns the number of char (non whitespaces) that have been read (noted nbOfBytes)
 * 
 * @param input : must be non null, stream where the caracters will be read
 * @param buffer[] : must be non null, buffer where the caracters will be written
 * @return the number of char that have been read until a whitespace was found
 * 			OR ERR_IO if more than MAX_SIZE_BUFFER consecutive char not separated by a whitespace have been read
 * 
 * Notes:
 * 
 * /!\ all elements of buffer of indices that belongs to {nbOfBytes,nbOfBytes+1,...,MAX_SIZE_BUFFER-1} are left in their initial state 
 * (are not set to 0 for example) and therefor should not be read after the call to readUntilNextWhiteSpace (they are irrelevant)
 * 
 * /!\ all white spaces that appear before the first non white space char will be ignored 
 * 		ex: ' '' '' ''a''b''c'' ' will return the buffer = {'a','b','c'} with size = 3
 * 		-> see the hasReadOneNonWhiteSpace boolean in the code
 * 
 * /!\ if the white spaces character that made the execution stops is '\n' this last char will be written inside the buffer
 * 		(this is usefull to check if a command is terminated by a '\n')
 * 		ex: ' '' '' ''a''b''c''\n' will return the buffer = {'a','b','c','\n'} with size = 4
 * 
 * 
 * 
 **/
int readUntilNextWhiteSpace(FILE* input, char buffer[], size_t* nbOfBytes){
	M_REQUIRE_NON_NULL(input);
	int c = 0; 
	bool hasReadOneNonWhiteSpace = false; // flag to say that we need to read at lest one non white space char to exit the function
	*nbOfBytes = 0;
	while (!feof(input) && !ferror(input) && !(isspace(c) && hasReadOneNonWhiteSpace)){
		
		// we iterate until we find a whitespace (and at least one non whitespace has been read) or until the end of the file
		
		c = fgetc(input);
		
		if (!isspace(c) || c == '\n'){ // if the char is not a white space then it's added into the buffer. 
			//Moreover if we add a \n then it can only be the last charater since the loop condition at the next iteration will be false (because of isspace(c) && hasReadOneNonWhiteSpace)
			// the last \n is usefull to check if a command is terminated by a '\n'
		    hasReadOneNonWhiteSpace = true; // if we enter here it means that we have read a non whitespace char (or a \n but as mentionned before this is the last char that could be added)
			if (*nbOfBytes >= MAX_SIZE_BUFFER){ // if more than MAX_SIZE_BUFFER are read then it means that it is an invalid command (see def of MAX_SIZE_BUFFER for more details)
				
				return ERR_IO;
				}
				
			buffer[*nbOfBytes] = c;
			
			(*nbOfBytes)++;
			}
		}
		
	return ERR_NONE;
	}
	
	/**
	 * Checks if a string contains only hexadecimal characters
	 * 
	 * @return : 1 if the whole string contains only hexadecimal characters, else 0
	 */
int isHexString(char string[], size_t start, size_t length){
	int isHexChar = 1; //true at first
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
	 #define MAX_TYPE_SIZE 2
int handleTypeSize(command_t* command, FILE* input){
	char buffer[MAX_SIZE_BUFFER];
	int err = ERR_NONE; //used for error propagation
	size_t s;
	if ((err = readUntilNextWhiteSpace(input, buffer, &s))!= ERR_NONE) return err; // error propagation
	M_REQUIRE(s <= MAX_TYPE_SIZE && s>0, ERR_BAD_PARAMETER, "SIZE OF TYPE MUST BE INF TO 2%c", ' ');
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
#define VIRT_ADDR_MIN_LENGTH 4
#define VIRT_ADDR_MAX_LENGTH 20
#define VIRT_ADDR_PADDING 3
int handle_virt_addr(char buffer[], size_t* s, FILE* input, command_t* command){
	int err  = ERR_NONE; // used for error propagation
	if ((err = readUntilNextWhiteSpace(input, buffer, s)) != ERR_NONE) return err; //error propagation
	M_REQUIRE(*s >= VIRT_ADDR_MIN_LENGTH && *s <= VIRT_ADDR_MAX_LENGTH, ERR_BAD_PARAMETER, "SIZE OF virt_addr must be greater than 4%c", ' ');
	M_REQUIRE(buffer[0] == '@', ERR_BAD_PARAMETER, "virt addr must start with @0x%c", ' ');
	M_REQUIRE(buffer[1] == '0', ERR_BAD_PARAMETER, "virt addr must start with @0x%c", ' ');
	M_REQUIRE(buffer[2] == 'x', ERR_BAD_PARAMETER, "virt addr must start with @0x%c", ' ');
	M_REQUIRE(buffer[*s-1] == '\n', ERR_BAD_PARAMETER, "virt addr must end with newline%c", ' ');
	for(int i =0; i < VIRT_ADDR_PADDING; i++){ //fills the @0x with spaces so strtoull can parse the hex
		buffer[i] = ' ';
	}
	buffer[*s] = '\0';
	M_REQUIRE(isHexString(buffer, 3, *s-1), ERR_BAD_PARAMETER, "IT IS NOT A HEX STRING%c", ' '); //requires that it indeed is a hex string
	char* after_number;
	uint64_t virt = (uint64_t) strtoull(buffer, &after_number, 16); //unsigned long long to uint64, parses the virtual address in the buffer
	M_REQUIRE((buffer+2) != after_number, ERR_BAD_PARAMETER, "strtoull didnt manage to read a number", "" );
	if((err = init_virt_addr64(&(command->vaddr), virt)) != ERR_NONE) return err;
	return ERR_NONE;
}
	/**
	 * Fills the order of the command, calls handleTypeSize to fill the type and size of the command
	 * Fills in the virtual address of the command using readUntilNextWhiteSpace and strtoull to parse the hex string into a uint64
	 * 
	 * /!\ assumes that the order is already set
	 * 
	 * @return : Either an error code or ERR_NONE
	 */
int handleRead(command_t* command, FILE* input){
	int err = ERR_NONE;
	command->order = READ;
	if ((err = handleTypeSize(command, input))!= ERR_NONE) return err;  //Fills the type and size and propagate error if needed
	size_t s = 0;
	char buffer[MAX_SIZE_BUFFER];
	command->write_data = 0;  //since it is a read
	//===================VIRT ADDR=================================
	if ((err = handle_virt_addr(buffer, &s,input, command))!= ERR_NONE) return err; //error propagation
	return ERR_NONE;
}


	/**
	 * Same as the handleRead method, but also fills in the data_size using readUntilNextWhiteSpace
	 * 
	 */
	 #define WRITE_DATA_MIN_LENGTH 4
	 #define WRITE_DATA_MAX_LENGTH 11
	 #define WRITE_DATA_PADDING 2
int handleWrite(command_t* command, FILE* input){
	int err = ERR_NONE; // used for error propagation
	//Suppose qu'il a déjà lu le R / W
	command->order = WRITE;
	if ((err = handleTypeSize(command, input))!= ERR_NONE) return err; //fills type and size and propagate error if needed 
	char buffer[MAX_SIZE_BUFFER];
	size_t s =0;
	//=======================WRITE_DATA=========================
	if ((err = readUntilNextWhiteSpace(input, buffer, &s))!= ERR_NONE) return err; // error propagation
	M_REQUIRE(s >= WRITE_DATA_MIN_LENGTH && s <= WRITE_DATA_MAX_LENGTH, ERR_BAD_PARAMETER, "SIZE OF Write_data must be greater than 4%c", ' ');
	M_REQUIRE(buffer[0] == '0', ERR_BAD_PARAMETER, "Write_data must start with 0x%c", ' ');
	M_REQUIRE(buffer[1] == 'x', ERR_BAD_PARAMETER, "Write_data must start with 0x%c", ' ');
	
	for(int i =0; i < WRITE_DATA_PADDING; i++){ //fills with spaces so the parser can work without the 0x
		buffer[i] = ' ';
	}
	buffer[s] = '\0';
	M_REQUIRE(isHexString(buffer, 2, s), ERR_BAD_PARAMETER, "IT IS NOT A HEX STRING%c", ' '); //if indeed a hex string
	char* after_number;
	command->write_data = (word_t) strtoull(buffer, &after_number, 16); //unsigned long long to word_t, parses the hex string
	M_REQUIRE((buffer+1) != after_number, ERR_BAD_PARAMETER, "strtoull didnt manage to read a number", "" );
	
	//=======================VIRT ADDR==========================
	if((err = handle_virt_addr(buffer, &s,input, command)) != ERR_NONE) return err;
	return ERR_NONE;
}
/**
 * @brief Read a program (list of commands) from a file.
 * 
 * Requirements:
 * 
 * @param filename : must be non null
 * @param program : must be non null
 * @return ERR_NONE or ERR_IO if the file could not be opened 
 */
int program_read(const char* filename, program_t* program){
	M_REQUIRE_NON_NULL(filename);
	M_REQUIRE_NON_NULL(program);

	FILE* file= NULL;
	file = fopen(filename, "r");
	M_REQUIRE_NON_NULL(file);
	int err = ERR_NONE;
	if ((err = program_init(program))!= ERR_NONE) {fclose(file); return err;}//error propagation
	
	while (!feof(file) && !ferror(file) ){
		// create new command that will be filled by readCommand
		virt_addr_t v;
		init_virt_addr(&v,0,0,0,0,0);
		command_t newC = {0,0,0,0,v}; 
		int validCommand = 0;
		if ((err = readCommand(file, &newC, &validCommand))!= ERR_NONE) {fclose(file);return err;}//error propagation

		if(validCommand)
			if ((err =program_add_command(program, &newC))!= ERR_NONE){fclose(file); return err;}//error propagation
		}
	if (ferror(file)) return ERR_IO;// check of ferror before closing the file since we may leave the loop because of ferror
	fclose(file);
	if ((err = program_shrink(program))!= ERR_NONE) return err;// error propagation
	return ERR_NONE;
	}
/**
 * @brief "Destructor" for program_t: free its content.
 * @param program the program to be filled from file.
 * @return ERR_NONE if ok, appropriate error code otherwise.
 */
int program_free(program_t* program){
	M_REQUIRE_NON_NULL(program);
	free(program->listing);
	program->listing = NULL;
	program->allocated = 0;
	program->nb_lines = 0;
	return ERR_NONE;
	}
