
#include "addr.h"
#include "error.h"
#include "mem_access.h"
#include "addr_mng.h"
#include "commands.h"
#include <stdio.h>
#include <inttypes.h>


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
		/*command_t* c = &((program->listing)[i]);
		c->order = 0;
		c->type = 0;
		c->data_size = 0;
		c->write_data = 0;
		virt_addr_t v;
		init_virt_addr(&v,0,0,0,0,0);
		c->vaddr = v;
		*/
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
		char ord = (com.order == READ) ? 'R' : 'W';
		char type = (com.type == INSTRUCTION) ? 'I' : 'D';
		char size = (com.data_size == 8) ? 'B' : 'W';
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

command_t readCommand(FILE* input);
int readUntilNextWhiteSpace(FILE* input){
	M_REQUIRE_NON_NULL(input);
	
	}
void handleRead(*command_t command);
void handleWrite(*command_t command);
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
	return ERR_NONE;
	}
