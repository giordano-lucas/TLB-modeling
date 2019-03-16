
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
	for (int i = 0 ; i < program->allocated ; ++i){
		command_t* c = &((program->listing)[i]);
		c->order = 0;
		c->type = 0;
		c->data_size = 0;
		c->write_data = 0;
		virt_addr_t v;
		init_virt_addr(&v,0,0,0,0,0);
		c->vaddr = v;
		
		
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
	
