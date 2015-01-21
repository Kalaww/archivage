#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "liste.h"


/* Initialise une liste */
liste *liste_init(){
	liste *l = malloc(sizeof(liste));
	l->length = 0;
	l->first = NULL;
	l->last = NULL;
	return l;
}


/* Free d'une liste */
void liste_free(liste *l){
	if(l && l->first) noeud_free_rec(l->first);
	free(l);
}


/* Free d'un noeud */
void noeud_free(noeud *n){
	if(!n) return;
	entete_free(n->infos);
	free(n);
}


/* Free récursif d'un noeud */
void noeud_free_rec(noeud *n){
	if(n && n->next) noeud_free_rec(n->next);
	noeud_free(n);
}


/* Ajoute une entete à la fin de la liste */
void liste_add(liste *l, entete *a){
	noeud *n = malloc(sizeof(noeud));
	n->infos = a;
	n->next = NULL;

	if(l->first == NULL) l->first = n;
	if(l->last != NULL) l->last->next = n;
	l->last = n;
	l->length++;
}


/* Supprime l'élément entete de la liste */
void liste_remove(liste *l, entete *a){
	noeud *courant, *tmp;
	if(l == NULL || l->first == NULL || l->length == 0) return;

	courant = l->first;
	if(courant->infos->hashcode == a->hashcode){
		if(l->first == l->last) l->last = NULL;
		l->first = courant->next;
		l->length--;
		noeud_free(courant);
		return;
	}

	while(courant->next != NULL){
		if(courant->next->infos->hashcode == a->hashcode){
			tmp = courant->next;
			courant->next = courant->next->next;
			noeud_free(tmp);
			if(courant->next == NULL) l->last = courant;
			l->length--;
			return;
		}
		courant = courant->next;
	}
}


/* Récupère l'élément en position pos */
entete* liste_get(liste *ls, int pos){
	noeud* courant;
	if(pos >= ls->length || pos < 0) return NULL;

	courant = ls->first;
	while(pos > 0){
		courant = courant->next;
		pos--;
	}
	return courant->infos;
}


/* Affiche le contenu de la liste */
void liste_print(liste *l){
	int i = 1;
	noeud* courant;
	courant = l->first;

	while(courant != NULL){
		printf("--- [Element %d] ---\n", i++);
		entete_print(courant->infos);
		printf("\n");
		courant = courant->next;
	}
}