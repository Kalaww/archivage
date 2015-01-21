#ifndef H_LOGIC
#define H_LOGIC

#include <sys/types.h>
#include <sys/stat.h>

#include "flag.h"
#include "archive.h"
#include "liste.h"


int archive_creation(char*);

int archive_ajouter(char*, char**, int);

int archive_ajouter_cible(char*, char*, liste*);

entete* archive_init_infos(char*, char*, struct stat*);

int archive_supprimer(char*, char**, int, liste*);

int archive_extraire(char*, char**, int);

int archive_extraire_cible(int, int, entete*);

int archive_affichage(liste*);

int archive_read(char*, liste*);

int archive_write(int, liste*);

char* md5(char*);

char *only_name(char*);

char *only_path(char*);

#endif