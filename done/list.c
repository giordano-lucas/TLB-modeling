

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
	return (this->front == NULL && this->back == NULL);
	}

/**
 * @brief initialize a list to the empty list (like a default constructor)
 * @param this list to initialized
 */
void init_list(list_t* this){
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
	while (current!= NULL){
		next = current->next;
		freeNode(current);
		current = next;
		
		}
	//free(this->back);
	this->back = NULL;
	}
//===========================================================
/*
 * Create a newNode with value, previous and last as attributes and returns null in case of error
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
	// next = NULL; // newNode is the last node 
	
	//error case
	if (newNode == NULL) return NULL;
	//update list
	if (is_empty_list(this)){
		// if the list is empty then the front becomes the new node
		(this->front) = newNode;
		}
	else {
		// if the list is non empty then we update the pointer of the old last element of the list
		(this->back)->next = newNode;
		}
	this->back = newNode;
	return newNode;
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
	
	//error case
	if (newNode == NULL) return NULL;
	//update list
	if (is_empty_list(this)){
		// if the list is empty then the back becomes the new node
		(this->back) = newNode;
		}
	else {
		// if the list is non empty then we update the pointer of the old first element of the list
		(this->front)->previous = newNode;
		}
	this->front = newNode;
	return newNode;
	}

/**
 * @brief remove the last value
 * @param this list to remove from
 */
void pop_back(list_t* this){
	// if empty nothing to be removed
	if(is_empty_list(this)) return;
	
	// case list.size = 1
	if (this->front == this->back){
		// make list empty
		clear_list(this);
		}
	// case list.size > 1
	else {
		node_t* newEnd = (this->back)->previous;
		newEnd->next = NULL; // end.previous has no next element
		freeNode(this->back); // free last element
		//update list
		this->back = newEnd;
		}
	
	}

/**
 * @brief remove the first value
 * @param this list to remove from
 */
void pop_front(list_t* this){
	// if empty nothing to be removed
	if(is_empty_list(this)) return;
	
	// case list.size = 1
	if (this->front == this->back){
		// make list empty
		clear_list(this);
		}
	// case list.size > 1
	else {
		node_t* newStart = (this->front)->next;
		newStart->previous = NULL; // newStart has no previvous element
		freeNode(this->front); // free frist element (element to remove)
		//update list
		this->front = newStart;
		}
	
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

	if (next == NULL){
		// case n is the last element it does not have to be moved
		return;
		}
	if (previous == NULL){
		//case if n is the first element 
		this->front = next;
		}
	else {
		//case n has a previous element
		previous->next = next;
		}
	
	next->previous = previous;
	// move to end
	node_t* endNode = this->back;
	endNode->next = n; // make old last node points to n
	n->previous = endNode; // make n node points to old last
	n->next = NULL; // since n is now the last node it has no next element
	//update list
	this->back = n;
	
	}

/**
 * @brief print a list (on one single line, no newline)
 * @param stream where to print to
 * @param this the list to be printed
 * @return number of printed characters
 */
int print_list(FILE* stream, const list_t* this){
	int nbChar = 0;
	nbChar += fprintf(stream,"(");
	node_t* current = this->front;
	while(current != NULL) {
		nbChar += print_node(stream, current->value);
		if(current->next != NULL)
		nbChar += fprintf(stream, ", ");
		current = current->next;
		}
	nbChar += fprintf(stream, ")");
	return nbChar;
	}

/**
 * @brief print a list reversed way
 * @param stream where to print to
 * @param this the list to be printed
 * @return number of printed characters
 */
int print_reverse_list(FILE* stream, const list_t* this){
	int nbChar = 0;
	nbChar += fprintf(stream,"(");
	node_t* current = this->back;
	while(current != NULL) {

		nbChar += print_node(stream, current->value);
		if(current->previous != NULL)
		nbChar += fprintf(stream, ", ");
		current = current->previous;
		}
	nbChar += fprintf(stream, ")");
	return nbChar;
}
