

#include "error.h"
#include "commands.h"
#include <stdio.h>


/**
 * @brief "Constructor" for program_t: initialize a program.
 * @param program (modified) the program to be initialized.
 * @return ERR_NONE of ok, appropriate error code otherwise.
 * 
 * 
 */
int program_init(program_t* program){
	M_REQUIRE_NON_NULL(program);
	
	command_t initiaLWithZeros;
	
	
	program->nb_lines = 0;
	program->allocated = sizeof(program->listing);
	for (int i = 0 ; i < program->allocated ; ++i){
		
		//(program->listing)[i] = 0;
	}
	return ERR_NONE;
}

/**
 * @brief Print the content of program to the stream "output".
 * 
 * Requirements: 
 * 
 * @param output : must be non null
 * @param program  : must be non null
 */
int program_print(FILE* output, const program_t* program){
	M_REQUIRE_NON_NULL(program);
	M_REQUIRE_NON_NULL(output);
	}
