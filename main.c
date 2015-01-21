#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

#include "flag.h"
#include "logic.h"


int FLAG_C = 0;
int FLAG_L = 0;
int FLAG_X = 0;
int FLAG_A = 0;
int FLAG_D = 0;

int FLAG_K = 0;

int FLAG_F = 0;

int FLAG_S = 0;

int FLAG_V = 0;

int FLAG_CREP = 0;

char*  new_repertoire;

void usage(char*);


int main(int argc, char **argv){

	int i, j, nb_archives = 0;
	char* nom_archive;
	char* tmp;
	char* archives[argc-1];

	nom_archive = NULL;

	for(i = 1; i < argc; i++){
		if(argv[i][0] == '-'){
			if(strlen(argv[i]) == 2 && argv[i][1] == 'f'){
				FLAG_F = 1;
				nom_archive = argv[++i];
			}else if(strlen(argv[i]) == 2 && argv[i][1] == 'C'){
				FLAG_CREP = 1;
				new_repertoire = argv[++i];
			}else{
				for(j = 1; j < strlen(argv[i]); j++){
					if(argv[i][j] == 'c'){
						FLAG_C = 1;
					}else if(argv[i][j] == 'l'){
						FLAG_L = 1;
					}else if(argv[i][j] == 'x'){
						FLAG_X = 1;
					}else if(argv[i][j] == 'a'){
						FLAG_A = 1;
					}else if(argv[i][j] == 'd'){
						FLAG_D = 1;
					}else if(argv[i][j] == 'k'){
						FLAG_K = 1;
					}else if(argv[i][j] == 's'){
						FLAG_S = 1;
					}else if(argv[i][j] == 'v'){
						FLAG_V = 1;
					}else if(argv[i][j] == 'f'){
						fprintf(stderr, "Commande -f dois être seule et suivie de l'archive\n");
						usage(argv[0]);
						exit(1);
					}else if(argv[i][j] == 'C'){
						fprintf(stderr, "Commande -C dois être seule et suivie du répertoire\n");
						usage(argv[0]);
						exit(1);
					}else{
						fprintf(stderr, "Commande '%c' inconnue\n", argv[i][j]);
						usage(argv[0]);
						exit(1);
					}
				}
			}
		}else{
			archives[nb_archives++] = argv[i];
		}
	}


	if(FLAG_C + FLAG_D + FLAG_A + FLAG_X + FLAG_L > 1){
		fprintf(stderr, "Une seule de ces options peut être utilisée à la fois : -c -l -d -x -a\n");
		usage(argv[0]);
		exit(1);
	}else if(FLAG_C + FLAG_D + FLAG_A + FLAG_X + FLAG_L == 0){
		fprintf(stderr, "Au moins une de ces options doit être présente : -c -l -d -x -a\n");
		usage(argv[0]);
		exit(1);
	}

	/* Suppression d'éventuel ancien fichier tmp */
	unlink("/tmp/archive_tmp.mtr");
	
	/* CREATION */
	if(FLAG_C){
		if(FLAG_F){
			if(archive_creation(nom_archive) > 0){
				fprintf(stderr, "Impossible de réaliser la création de l'archive %s.\n", nom_archive);
				return 1;
			}
		}else nom_archive = NULL;

		if(FLAG_CREP){
			for(i = 0; i < nb_archives; i++){
				tmp = archives[i];
				archives[i] = malloc(sizeof(char)*PATH_MAX);
				strcat(archives[i], new_repertoire);
				if(new_repertoire[strlen(new_repertoire)-1] != '/') strcat(archives[i], "/");
				strcat(archives[i], tmp);
			}
		}

		if(archive_ajouter(nom_archive, archives, nb_archives) > 0){
			fprintf(stderr, "Impossible de réaliser la création de l'archive %s.\n", nom_archive);
			if(FLAG_F) unlink(nom_archive);
		}

		if(FLAG_CREP){
			for(i = 0; i < nb_archives; i++)
				free(archives[i]);
		}
	/* AJOUT */
	}else if(FLAG_A){
		if(!FLAG_F){
			fprintf(stderr, "Aucune archive donné en argument. Utilisez l'option [-f nom_archive].\n");
			return 1;
		}

		if(FLAG_CREP){
			for(i = 0; i < nb_archives; i++){
				tmp = archives[i];
				archives[i] = malloc(sizeof(char)*PATH_MAX);
				strcat(archives[i], new_repertoire);
				if(new_repertoire[strlen(new_repertoire)-1] != '/') strcat(archives[i], "/");
				strcat(archives[i], tmp);
			}
		}

		if(archive_ajouter(nom_archive, archives, nb_archives) > 0){
			fprintf(stderr, "Impossible de réaliser l'ajout des éléments à l'archive %s.\n", nom_archive);
		}

		if(FLAG_CREP){
			for(i = 0; i < nb_archives; i++)
				free(archives[i]);
		}
	/* AFFICHAGE */
	}else if(FLAG_L){
		if(!FLAG_F){
			fprintf(stderr, "Aucune archive donné en argument. Utilisez l'option [-f nom_archive].\n");
			return 1;
		}
		liste *ls = liste_init();
		if(archive_read(nom_archive, ls) > 0){
			fprintf(stderr, "Impossible de lire l'archive %s\n", nom_archive);
			liste_free(ls);
			return 1;
		}
		archive_affichage(ls);
		liste_free(ls);
	/* SUPPRESSION */
	}else if(FLAG_D){
		if(!FLAG_F){
			fprintf(stderr, "Aucune archive donné en argument. Utilisez l'option [-f nom_archive].\n");
			return 1;
		}
		liste *ls = liste_init();
		if(archive_read(nom_archive, ls) > 0){
			fprintf(stderr, "Impossible de lire l'archive %s\n", nom_archive);
			liste_free(ls);
			return 1;
		}
		if(archive_supprimer(nom_archive, archives, nb_archives, ls) > 0){
			fprintf(stderr, "Echec de la suppression.\n");
		}
		liste_free(ls);
	}else if(FLAG_X){
		if(archive_extraire(nom_archive, archives, nb_archives) > 0){
			fprintf(stderr, "Echec de l'extraction.\n");
		}
	}

	return 0;
}

/*
	Usage de la ligne de commande
 */
void usage(char* nom_exec){
	fprintf(stderr, "Usage :\n\t%s [option] [path 1] ... [path n]\n", nom_exec);
}