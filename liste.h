#ifndef H_LISTE
#define H_LISTE

#include "archive.h"

/*
	Structure de noeud
 */
typedef struct noeud{
	entete *infos;
	struct noeud *next;
} noeud;

/*
	Structure de liste
 */
typedef struct liste{
	int length;
	struct noeud *first;
	struct noeud *last;
} liste;

liste *liste_init();

void liste_free(liste*);

void noeud_free(noeud*);

void noeud_free_rec(noeud*);

void liste_add(liste*, entete*);

void liste_remove(liste*, entete*);

entete* liste_get(liste*, int);

void liste_print(liste*);

#endif