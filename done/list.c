

#include <stdio.h> // for fprintf()
#include <stdint.h> // for uint32_t
#include <inttypes.h> // for PRIx macros
#include "list.h"
#include "error.h"

#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>


/**
 * @brief check whether the list is empty or not
 * @param this list to check
 * @return 0 if the list is (well-formed and) not empty
 */
int is_empty_list(const list_t* this){
	return ((this == NULL) || (this->front == NULL && this->back == NULL));
	}

/**
 * @brief initialize a list to the empty list (like a default constructor)
 * @param this list to initialized
 */
void init_list(list_t* this){
	if (this == NULL) return; // error case
	this->front = NULL;
	this->back = NULL;
}
//=============================================================
/**
 * @brief : helper function that frees a node
 **/
void freeNode(node_t* n){
	if (n != NULL){
		free(n);
		}
	}
//=============================================================
/**
 * @brief clear the whole list (make it empty)
 * @param this list to clear
 */
void clear_list(list_t* this){
	// case empty list
	if (this == NULL || is_empty_list(this)) return;
	
	node_t* current = (this->front);
	this->front = NULL;
	node_t* next = NULL;
	while (current!= NULL){ // iterate on all nodes in the list
		next = current->next;
		current->next = NULL; current->previous = NULL;// just to make sure that nobody can access current and previous anymore
		freeNode(current);
		current = next;
		}
	this->back = NULL; // make list empty
	}
//===========================================================
/*
 * Create a newNode with value, previous and last as attributes and returns null in case of an error
 */
node_t* createNode(const list_content_t* value, node_t* previous, node_t*last){
	//create node
	node_t* newNode = malloc(sizeof(node_t));
	//error case
	if (newNode == NULL) return NULL;
	// initialization
	newNode->previous = previous;
	newNode->next = last;
	newNode->value = *value;
	return newNode;
}

//===========================================================
/**
 * @brief helper function for push back and push front
 * @param pushPosition : the position where to push : back or front
 * @param otherExtremity : back if pushPosition is front and conversly
 * @param nodeToModify : next in case of push_back and previous if push_front
 */
#define push_helper(pushPosition, otherExtremity, nodeToModify)                                                            \
    if (newNode == NULL) return NULL; /*error case (propagation)*/                                                         \
	/*update list */                                                                                                       \
	if (is_empty_list(this)) (this->otherExtremity) = newNode; /* if the list is empty then the */                         \
															   /*otherExtremity becomes the new node */                    \
	else (this->pushPosition)->nodeToModify = newNode; 	       /* if the list is non empty then we update the pointer */   \
															   /*of the old element at pushPosition of the list*/          \
	this->pushPosition = newNode; /* push element */                                                                       \
	return newNode;
//===========================================================
/**
 * @brief add a new value at the end of the list
 * @param this list where to add to
 * @param value value to be added
 * @return a pointer to the newly inserted element or NULL in case of error
 */
node_t* push_back(list_t* this, const list_content_t* value){
	if (this == NULL || value == NULL) return NULL ;
	
	//create node
	node_t* newNode = createNode(value, this->back,NULL);
	// previous = this->back => make the last node points to the new node
	// next = NULL;          => newNode is the last node 
	push_helper(back, front, next);
}

/**
 * @brief add a new value at the begining of the list
 * @param this list where to add to
 * @param value value to be added
 * @return a pointer to the newly inserted element or NULL in case of error
 */
node_t* push_front(list_t* this, const list_content_t* value){
	if (this == NULL || value == NULL) return NULL ;
	
	//create node
	node_t* newNode = createNode(value, NULL, this->front);
	// previous = NULL => newNode is the first node
	// next = this->front => make the new node points to the first Node of the list
	push_helper(front, back, previous)
	}
//================================================================================================
/**
 * @brief helper function for pop back and pop front
 * @param pushPosition : the position where to pop : back or front
 * @param nodeNewExtremity : previous in case of push_back and next in case of push front
 * @param nodeToModify : next in case of pop_back and previous if pop_front
 */
#define pop_helper(popPosition, nodeNewExtremity, newEmptyNode)                                \
    /*if empty nothing to be removed*/                                                         \
	if(this == NULL || is_empty_list(this)) return;                                            \
	/* case list.size = 1 */                                                                   \
	if (this->front == this->back) clear_list(this); /* make list empty */                     \
	/* case list.size > 1*/                                                                    \
	else {                                                                                     \
		node_t* newExtremity = (this->popPosition)->nodeNewExtremity;                          \
		newExtremity->newEmptyNode = NULL; /* new Extremity needs to be updated  */            \
		freeNode(this->popPosition); /* free last element */                                   \
		/*update list*/                                                                        \
		this->popPosition = newExtremity;                                                      \
		}
//================================================================================================
/**
 * @brief remove the last value
 * @param this list to remove from
 */
void pop_back(list_t* this){
	pop_helper(back, previous, next);
	}

/**
 * @brief remove the first value
 * @param this list to remove from
 */
void pop_front(list_t* this){
	pop_helper(front, next, previous);
	}

/**
 * @brief move a node a the end of the list
 * @param this list to modify
 * @param node pointer to the node to be moved
 */
void move_back(list_t* this, node_t* n){
	// if empty or size == 1 nothing to be moved
	if(n == NULL || is_empty_list(this)  || (this->front == this->back)) return;
	// if n is already at the end 
	if (n == this->back) return;
	
	// ====== case list.size > 1 ======
	
	// remove from list 
	node_t* previous = n->previous;
	node_t* next = n->next;

	if (next == NULL) return; // case n is the last element it does not have to be moved
	if (previous == NULL) this->front = next; //case n is the first element 
	else previous->next = next; //case n has a previous element
	
	next->previous = previous;
	// move to end
	node_t* endNode = this->back;
	endNode->next = n; // make old last node points to n
	n->previous = endNode; // make node n points to old last node
	n->next = NULL; // since n is now the last node it has no next element
	//update list
	this->back = n;
	
	}

/**
 * @brief print a list (on one single line, no newline)
 * @param stream where to print to, must be non null
 * @param this the list to be printed, must be non null
 * @return number of printed characters
 */
int print_list(FILE* stream, const list_t* this){
	if (stream == NULL || this == NULL) return 0; //sanity checks
	
	int nbChar = 0;
	nbChar += fprintf(stream,"(");
	// uses the macro defined is list.h to iterate on all nodes 
	for_all_nodes(current, this) { 
		nbChar += print_node(stream, current->value);
		if(current->next != NULL) nbChar += fprintf(stream, ", ");
		 }
	nbChar += fprintf(stream, ")");
	return nbChar;
	}

/**
 * @brief print a list reversed way
 * @param stream where to print to, must be non null
 * @param this the list to be printed, must be non null
 * @return number of printed characters
 */
int print_reverse_list(FILE* stream, const list_t* this){
	if (stream == NULL || this == NULL) return 0; //sanity checks
	
	int nbChar = 0; // the number of printed characters
	nbChar += fprintf(stream,"(");
	// uses the macro defined is list.h to iterate on all nodes 
	for_all_nodes_reverse(current, this) { 
		nbChar += print_node(stream, current->value);
		if(current->previous != NULL) nbChar += fprintf(stream, ", ");
		}
	nbChar += fprintf(stream, ")");
	return nbChar;
}
