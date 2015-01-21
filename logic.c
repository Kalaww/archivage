#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <err.h>
#include <limits.h>
#include <utime.h>

#include "logic.h"

#define BUFSIZE 1024

/*
	Créer une archive
 */
int archive_creation(char* nom_archive){
	int archive_fd;

	if((archive_fd = open(nom_archive, O_WRONLY | O_CREAT | O_EXCL, 0660)) < 0){
		warn("Le fichier %s est déjà une archive existante. Utilisez l'option -a pour ajouter des éléments à cette archive", nom_archive);
		return 1;
	}

	close(archive_fd);

	return 0;
}

/*
	Ajoute les éléments dans le tableau 'archives' à l'archive
 */
int archive_ajouter(char* nom_archive, char** archives, int nb_archives){
	int archive_fd, i;
	liste *ls;
	char *nom_courant;
	char *pre_path_courant;
	struct flock archive_lock;

	ls = liste_init();

	if(nom_archive != NULL){
		if((archive_fd = open(nom_archive, O_WRONLY | O_APPEND)) < 0){
			warn("Impossible d'ouvrir l'archive %s", nom_archive);
			liste_free(ls);
			return 1;
		}
	}else archive_fd = 1;

	archive_lock.l_type = F_WRLCK;
	archive_lock.l_whence = SEEK_SET;
	archive_lock.l_start = 0;
	archive_lock.l_len = 0;
	if(fcntl(archive_fd, F_SETLKW, &archive_lock) < 0){
		warn("Impossible de verrouiller %s", nom_archive);
		close(archive_fd);
		liste_free(ls);
		return 1;
	}
	

	for(i = 0; i < nb_archives; i++){
		nom_courant = only_name(archives[i]);
		pre_path_courant = only_path(archives[i]);
		if(archive_ajouter_cible(pre_path_courant, nom_courant, ls) > 0){
			warn("Impossible d'ajouter la racine %s à l'archive", archives[i]);
			free(nom_courant);
			free(pre_path_courant);
			liste_free(ls);
			if(nom_archive != NULL) close(archive_fd);
			return 1;
		}
	}

	if(archive_write(archive_fd, ls) > 0){
		warn("Erreur lors de l'écriture de l'archive");
		liste_free(ls);
		if(nom_archive != NULL) close(archive_fd);
		return 1;
	}

	free(nom_courant);
	free(pre_path_courant);
	liste_free(ls);
	if(nom_archive != NULL) close(archive_fd);

	return 0;
}

/*
	Ajoute un élément à la liste des archives
 */
int archive_ajouter_cible(char* path, char* fichier, liste *ls){
	struct stat *fichier_st;
	DIR *dir_flot;
	struct dirent *dir_entree;
	char* complete_path;
	char* dir_path;

	fichier_st = malloc(sizeof(struct stat));

	complete_path = malloc(strlen(path)+strlen(fichier)+1);
	complete_path[0] = '\0';
	strcat(complete_path, path);
	strcat(complete_path, fichier);

	if(lstat(complete_path, fichier_st) < 0){
		warn("Impossible d'accéder à %s", complete_path);
		free(complete_path);
		free(fichier_st);
		return 1;
	}

	if(S_ISREG(fichier_st->st_mode) || (S_ISLNK(fichier_st->st_mode) && FLAG_S)){
		liste_add(ls, archive_init_infos(path, fichier, fichier_st));
	}else if(S_ISDIR(fichier_st->st_mode)){
		if((dir_flot = opendir(complete_path)) == NULL){
			warn("Impossible d'ouvrir le répertoire %s", complete_path);
			free(complete_path);
			free(fichier_st);
			return 1;
		}

		liste_add(ls, archive_init_infos(path, fichier, fichier_st));
		for(;;){
			errno = 0;
			dir_entree = readdir(dir_flot);
			if(dir_entree == NULL){
				if(errno){
					warn("Erreur lors du parcours du répertoire %s", complete_path);
					free(complete_path);
					free(fichier_st);
					return 1;
				}else break;
			}

			if(strcmp(dir_entree->d_name, ".") == 0
				|| strcmp(dir_entree->d_name, "..") == 0)
				continue;

			/* Chemin complet du sous dossier */
			dir_path = malloc(strlen(fichier)+2+strlen(dir_entree->d_name));
			dir_path[0] = '\0';
			strcat(dir_path, fichier);
			strcat(dir_path, "/");
			strcat(dir_path, dir_entree->d_name);

			if(archive_ajouter_cible(path, dir_path, ls) > 0){
				warn("Erreur lors de l'ajout de %s", dir_path);
				free(dir_path);
				free(complete_path);
				free(fichier_st);
				return 1;
			}
			free(dir_path);
		}
		closedir(dir_flot);
	}

	free(complete_path);
	free(fichier_st);
	return 0;
}

/*
	Récupère toutes les informations sur le fichier afin de pouvoir l'ajouter à la liste des éléments archivés
 */
entete* archive_init_infos(char* path, char* fichier, struct stat *fichier_st){
	entete *infos;
	int i;
	char* complete_path;
	char* apres;
	char* tmp;

	infos = entete_init();

	if(FLAG_CREP){
		complete_path = malloc(PATH_MAX);
		complete_path[0] = '\0';
		apres = malloc(PATH_MAX);
		apres[0] = '\0';

		strcat(complete_path, path);
		strcat(complete_path, fichier);
		tmp = strstr(complete_path, new_repertoire);
		strcat(apres, tmp);
		complete_path[strlen(complete_path) - strlen(apres)] = '\0';

		infos->path_length = (size_t)(strlen(apres)+1);
		infos->path = malloc((infos->path_length)*sizeof(char));
		strcpy(infos->path, apres);
		infos->path[infos->path_length] = '\0';
		infos->pre_path = malloc((strlen(complete_path)+1)*sizeof(char));
		strcpy(infos->pre_path, complete_path);
		infos->pre_path[strlen(complete_path)] = '\0';

		free(complete_path);
		free(apres);
	}else{
		infos->path_length = (size_t)(strlen(fichier)+1);
		infos->path = malloc((infos->path_length+1)*sizeof(char));
		strcpy(infos->path, fichier);
		infos->path[infos->path_length] = '\0';
		infos->pre_path = malloc((strlen(path)+1)*sizeof(char));
		strcpy(infos->pre_path, path);
		infos->pre_path[strlen(path)] = '\0';
	}

	infos->file_length = (S_ISDIR(fichier_st->st_mode))? (off_t)0 : (off_t)fichier_st->st_size;
	infos->mode = fichier_st->st_mode;
	infos->m_time = fichier_st->st_mtime;
	infos->a_time = fichier_st->st_atime;

	if(FLAG_V && infos->file_length > 0){
		complete_path = malloc(PATH_MAX);
		complete_path[0] = '\0';

		strcat(complete_path, path);
		strcat(complete_path, fichier);

		infos->checksum = md5(complete_path);
		if(infos->checksum == NULL){
			free(complete_path);
			free(infos);
			err(1, "Erreur md5");
		}
		free(complete_path);
	}else{
		infos->checksum = malloc(sizeof(char)*32);
		for(i = 0; i < 32; i++)
			infos->checksum[i] = 0;	
	}

	return infos;
}

/*
	Supprimer les éléments dans le tableau 'archives' de l'archive
 */
int archive_supprimer(char *nom_archive, char** archives, int nb_archives, liste *ls){
	int i, j, tmp_file_fd, archive_fd, lus;
	entete* infos;
	char *tmp;
	char buf[BUFSIZE];
	struct flock lock_w, lock_r;

	for(i = 0; i < nb_archives; i++){
		for(j = 0; j < ls->length; j++){
			infos = liste_get(ls, j);
			tmp = strstr(infos->path, archives[i]);
			if(strcmp(archives[i], infos->path) == 0
				|| (tmp != NULL && 
					tmp == infos->path && 
					(tmp[strlen(archives[i])] == '/'
						|| (archives[i][strlen(archives[i])-1] == '/' && tmp[strlen(archives[i])-1] == '/')))){
				liste_remove(ls, infos);
				j--;
			}
		}
	}

	lock_w.l_type = F_WRLCK;
	lock_w.l_start = 0;
	lock_w.l_whence = SEEK_SET;
	lock_w.l_len = 0;

	lock_r.l_type = F_RDLCK;
	lock_r.l_start = 0;
	lock_r.l_whence = SEEK_SET;
	lock_r.l_len = 0;


	if((tmp_file_fd = open("/tmp/archive_tmp.mtr", O_WRONLY | O_CREAT | O_EXCL, 0660)) < 0){
		warn("Impossible de créer un fichier temporaire dans /tmp");
		return 1;
	}
	if(fcntl(tmp_file_fd, F_SETLKW, &lock_w) < 0){
		warn("Impossible de verrouiller /tmp/archive_tmp.mtr");
		close(tmp_file_fd);
		unlink("/tmp/archive_tmp.mtr");
		return 1;
	}

	if((archive_fd = open(nom_archive, O_RDONLY)) < 0){
		warn("Impossible d'ouvrir l'archive %s en lecture", nom_archive);
		close(tmp_file_fd);
		unlink("/tmp/archive_tmp.mtr");
		return 1;
	}
	if(fcntl(archive_fd, F_SETLKW, &lock_r) < 0){
		warn("Impossible de verrouiller %s", nom_archive);
		close(archive_fd);
		close(tmp_file_fd);
		unlink("/tmp/archive_tmp.mtr");
		return 1;
	}

	for(i = 0; i < ls->length; i++){
		infos = liste_get(ls, i);

		if(write(tmp_file_fd, &infos->path_length, sizeof(size_t)) < 0) return 1;
		if(write(tmp_file_fd, &infos->file_length, sizeof(off_t)) < 0) return 1;
		if(write(tmp_file_fd, &infos->mode, sizeof(mode_t)) < 0) return 1;
		if(write(tmp_file_fd, &infos->m_time, sizeof(time_t)) < 0) return 1;
		if(write(tmp_file_fd, &infos->a_time, sizeof(time_t)) < 0) return 1;
		if(write(tmp_file_fd, infos->checksum, sizeof(char)*32) < 0) return 1;
		if(write(tmp_file_fd, infos->path, sizeof(char)*infos->path_length) < 0) return 1;

		if(infos->file_length > 0){
			if(archive_extraire_cible(archive_fd, tmp_file_fd, infos) > 0){
				warn("Erreur lors de la copie de %s", infos->path);
				close(tmp_file_fd);
				unlink("/tmp/archive_tmp.mtr");
				close(archive_fd);
				return 1;
			}
		}
	}

	close(tmp_file_fd);
	close(archive_fd);

	if((tmp_file_fd = open("/tmp/archive_tmp.mtr", O_RDONLY)) < 0){
		warn("Impossible d'ouvrir le fichier temporaire dans /tmp");
		return 1;
	}
	if(fcntl(tmp_file_fd, F_SETLKW, &lock_r) < 0){
		warn("Impossible de verrouiller /tmp/archive_tmp.mtr");
		close(tmp_file_fd);
		unlink("/tmp/archive_tmp.mtr");
		return 1;
	}

	if((archive_fd = open(nom_archive, O_WRONLY | O_TRUNC)) < 0){
		warn("Impossible d'ouvrir l'archive %s en écriture", nom_archive);
		close(tmp_file_fd);
		unlink("/tmp/archive_tmp.mtr");
	}
	if(fcntl(archive_fd, F_SETLKW, &lock_w) < 0){
		warn("Impossible de verrouiller %s", nom_archive);
		close(archive_fd);
		close(tmp_file_fd);
		unlink("/tmp/archive_tmp.mtr");
		return 1;
	}

	while((lus = read(tmp_file_fd, buf, BUFSIZE)) != 0){
		if(write(archive_fd, buf, lus) < 0){
			warn("Erreur lors de l'écriture dans l'archive %s", nom_archive);
			close(tmp_file_fd);
			unlink("/tmp/archive_tmp.mtr");
			close(archive_fd);
			return 1;
		}
	}
	close(tmp_file_fd);
	unlink("/tmp/archive_tmp.mtr");
	close(archive_fd);
	return 0;
}

/*
	Extrait les éléments dans le tableau 'archives'
 */
int archive_extraire(char *nom_archive, char** archives, int nb_archives){
	int i, j, k, archive_fd, fichier_fd, encore = 1, tmp_file_fd, lus, lus_sym, repetition;
	entete* infos;
	char* tmp;
	char* md5_tmp;
	char buf[BUFSIZE];
	char buf_sym[PATH_MAX];
	char path[PATH_MAX];
	char md5f[33];
	char md5tmp[33];
	struct utimbuf temps;
	liste *ls;
	struct stat stat_fc;
	struct flock lock_w, lock_r;

	lock_w.l_type = F_WRLCK;
	lock_w.l_start = 0;
	lock_w.l_whence = SEEK_SET;
	lock_w.l_len = 0;

	lock_r.l_type = F_RDLCK;
	lock_r.l_start = 0;
	lock_r.l_whence = SEEK_SET;
	lock_r.l_len = 0;


	if(nom_archive == NULL){
		if((tmp_file_fd = open("/tmp/archive_tmp.mtr", O_WRONLY | O_CREAT | O_EXCL, 0660)) < 0){
			warn("Impossible de créer un fichier temporaire dans /tmp");
			return 1;
		}
		if(fcntl(tmp_file_fd, F_SETLKW, &lock_w) < 0){
			warn("Impossible de verrouiller /tmp/archive_tmp.mtr");
			close(tmp_file_fd);
			unlink("/tmp/archive_tmp.mtr");
			return 1;
		}

		archive_fd = STDIN_FILENO;

		while((lus = read(archive_fd, buf, BUFSIZE)) != 0){
			if(write(tmp_file_fd, buf, lus) < 0){
				warn("Erreur lors de l'écriture dans /tmp/archive_tmp.mtr");
				close(tmp_file_fd);
				unlink("/tmp/archive_tmp.mtr");
				return 1;
			}
		}
		close(tmp_file_fd);
	}

	ls = liste_init();
	if(nom_archive == NULL){
		if(archive_read("/tmp/archive_tmp.mtr", ls) > 0){
			warn("Impossible de lire l'archive tmp du stdin");
			unlink("/tmp/archive_tmp.mtr");
			liste_free(ls);
			return 1;
		}
	}else{
		if(archive_read(nom_archive, ls) > 0){
			warn("Impossible de lire l'archive %s", nom_archive);
			liste_free(ls);
			return 1;
		}
	}

	if(nom_archive == NULL){
		if((archive_fd = open("/tmp/archive_tmp.mtr", O_RDONLY)) < 0){
			warn("Impossible d'ouvrir le fichier temporaire dans /tmp");
			unlink("/tmp/archive_tmp.mtr");
			return 1;
		}
		if(fcntl(archive_fd, F_SETLKW, &lock_r) < 0){
			warn("Impossible de verrouiller /tmp/archive_tmp.mtr");
			close(archive_fd);
			unlink("/tmp/archive_tmp.mtr");
			return 1;
		}
	}else{
		if((archive_fd = open(nom_archive, O_RDONLY)) < 0){
			warn("Impossible d'ouvrir l'archive %s en lecture", nom_archive);
			return 1;
		}
		if(fcntl(archive_fd, F_SETLKW, &lock_r) < 0){
			warn("Impossible de verrouiller %s", nom_archive);
			close(archive_fd);
			return 1;
		}
	}

	if(nb_archives > 0){
		for(i = 0; i < ls->length; i++){
			infos = liste_get(ls, i);
			for(j = 0; j < nb_archives; j++){
				tmp = strstr(infos->path, archives[j]);
				if(strcmp(archives[j], infos->path) == 0 ||
					(tmp != NULL && tmp == infos->path && (tmp[strlen(archives[j])] == '/' 
						|| (archives[j][strlen(archives[j])-1] == '/' && tmp[strlen(archives[j])-1] == '/')))){
					for(k = strlen(archives[j])-1; k >= 0; k--){
						if(infos->path[k] == '/') break;
					}
					infos->path = &(infos->path[k+1]);
				}else{
					liste_remove(ls, infos);
					i--;
				}
			}
		}
	}


	/* Extraction des répertoires */
	repetition = -1;
	while(encore){
		encore = 0;
		for(i = 0; i < ls->length; i++){
			infos = liste_get(ls, i);
			if(S_ISDIR(infos->mode)){
				path[0] = '\0';
				if(FLAG_CREP) strcat(path, new_repertoire);
				if(FLAG_CREP && new_repertoire[strlen(new_repertoire)-1] != '/') strcat(path, "/");
				strcat(path, infos->path);
				if(mkdir(path, infos->mode) < 0 && errno != EEXIST && errno != ENOENT){
					warn("Erreur lors de l'extraction du répertoire %s", infos->path);
					close(archive_fd);
					unlink("/tmp/archive_tmp.mtr");
					return 1;
				}
				if(errno != ENOENT){
					infos->path = &(infos->path[-(infos->path_length - (strlen(infos->path)+1))]);
					liste_remove(ls, infos);
					i--;
				}else{
					if(repetition == -1) repetition = infos->hashcode;
					else if(repetition == infos->hashcode){
						fprintf(stderr, "Erreur lors de l'extraction du répertoire %s: un répertoire dans le chemin n'existe pas\n", infos->path);
						close(archive_fd);
						unlink("/tmp/archive_tmp.mtr");
						return 1;
					}
					encore = 1;
				}
			}
		}
	}

	/* Extraction des fichiers réguliers */
	for(i = 0; i < ls->length; i++){
		infos = liste_get(ls, i);
		if(S_ISREG(infos->mode)){
			path[0] = '\0';
			if(FLAG_CREP) strcat(path, new_repertoire);
			if(FLAG_CREP && new_repertoire[strlen(new_repertoire)-1] != '/') strcat(path, "/");
			strcat(path, infos->path);
			if(FLAG_K && stat(path, &stat_fc) >= 0){
				printf("Le fichier %s existe déjà. L'extraction de %s depuis l'archive est annulée.\n", path, path);
				infos->path = &(infos->path[-(infos->path_length - (strlen(infos->path)+1))]);
				continue;
			}
			
			if((fichier_fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, infos->mode)) < 0){
				warn("Erreur lors de l'extraction du fichier %s", path);
				close(archive_fd);
				close(fichier_fd);
				unlink("/tmp/archive_tmp.mtr");
				return 1;
			}
			if(fcntl(fichier_fd, F_SETLKW, &lock_w) < 0){
				warn("Impossible de verrouiller %s", path);
				close(archive_fd);
				close(fichier_fd);
				unlink("/tmp/archive_tmp.mtr");
				return 1;
			}
	
			if(archive_extraire_cible(archive_fd, fichier_fd, infos) > 0){
				close(archive_fd);
				close(fichier_fd);
				unlink("/tmp/archive_tmp.mtr");
				return 1;
			}

			close(fichier_fd);

			if(chmod(path, infos->mode) < 0){
				warn("Erreur lors du changement des permissions de %s", path);
				close(archive_fd);
				unlink("/tmp/archive_tmp.mtr");
				return 1;
			}

			temps.actime = infos->a_time;
			temps.modtime = infos->m_time;
			if(utime(path, &temps) < 0){
				warn("Erreur lors du changement des dates d'accès et modification de %s", path);
				close(archive_fd);
				unlink("/tmp/archive_tmp.mtr");
				return 1;
			}

			if(FLAG_V){
				md5f[0] = '\0';
				md5_tmp = md5(path);
				strncat(md5f, md5_tmp, 32);
				md5f[32] = '\0';
				md5tmp[0] = '\0';
				strncat(md5tmp, infos->checksum, 32);
				md5tmp[32] = '\0';
				if(strcmp(md5f, md5tmp) != 0){
					fprintf(stderr, "Les empreintes md5 de %s ne sont pas similaires\n[%s]\n[%s]\n", path, md5f, md5tmp);
				}
				free(md5_tmp);
			}

			infos->path = &(infos->path[-(infos->path_length - (strlen(infos->path)+1))]);
		}
	}

	/* Extraction des liens symboliques */
	for(i = 0; i < ls->length; i++){
		infos = liste_get(ls, i);
		if(S_ISLNK(infos->mode) && FLAG_S){
			path[0] = '\0';
			if(FLAG_CREP) strcat(path, new_repertoire);
			if(FLAG_CREP && new_repertoire[strlen(new_repertoire)-1] != '/') strcat(path, "/");
			strcat(path, infos->path);
			if(FLAG_K && stat(path, &stat_fc) >= 0){
				printf("Le fichier %s existe déjà. L'extraction de %s depuis l'archive est annulée.\n", path, path);
				infos->path = &(infos->path[-(infos->path_length - (strlen(infos->path)+1))]);
				continue;
			}

			if(lseek(archive_fd, infos->seek_file, SEEK_SET) < 0){
				warn("Erreur lseek");
				close(archive_fd);
				return 1;
			}

			if((lus_sym = read(archive_fd, buf_sym, infos->file_length)) < 0){
				warn("Erreur lors de la lecture du lien symbolique %s à extraire", path);
				close(archive_fd);
				return 1;
			}
			buf_sym[lus_sym] = '\0';

			if(symlink(buf_sym, path) < 0){
				warn("Erreur lors de la création du lien symbolique %s", path);
				close(archive_fd);
				return 1;
			}

			if(chmod(path, infos->mode) < 0){
				warn("Erreur lors du changement des permissions de %s", path);
				close(archive_fd);
				unlink("/tmp/archive_tmp.mtr");
				return 1;
			}

			temps.actime = infos->a_time;
			temps.modtime = infos->m_time;
			if(utime(path, &temps) < 0){
				warn("Erreur lors du changement des dates d'accès et modification de %s", path);
				close(archive_fd);
				unlink("/tmp/archive_tmp.mtr");
				return 1;
			}

			if(FLAG_V){
				md5f[0] = '\0';
				md5_tmp = md5(path);
				strncat(md5f, md5_tmp, 32);
				md5f[32] = '\0';
				md5tmp[0] = '\0';
				strncat(md5tmp, infos->checksum, 32);
				md5tmp[32] = '\0';
				if(strcmp(md5f, md5tmp) != 0){
					fprintf(stderr, "Les empreintes md5 de %s ne sont pas similaires\n[%s]\n[%s]\n", path, md5f, md5tmp);
				}
				free(md5_tmp);
			}

			infos->path = &(infos->path[-(infos->path_length - (strlen(infos->path)+1))]);
		}
	}

	close(archive_fd);
	if(nom_archive == NULL) unlink("/tmp/archive_tmp.mtr");
	liste_free(ls);
	return 0;
}

/*
	Extrait la cible
 */
int archive_extraire_cible(int in_fd, int out_fd, entete *infos){
	int lus, a_lire, restant;
	char* buf[BUFSIZE];

	if(lseek(in_fd, infos->seek_file, SEEK_SET) < 0){
		warn("Erreur lseek");
		return 1;
	}

	restant = infos->file_length;
	a_lire = (restant >= BUFSIZE)? BUFSIZE : restant;
	while((lus = read(in_fd, buf, a_lire)) != 0){
		if(lus < 0){
			warn("Erreur lors de la lecture de la source à extraire");
			return 1;
		}
		if(write(out_fd, buf, lus) < 0){
			warn("Erreur lors de l'écriture pendant l'extraction de %s", infos->path);
			return 1;
		}
		restant -= lus;
		a_lire = (restant >= BUFSIZE)? BUFSIZE : restant;
	}

	return 0;
}

/*
	Affiche les éléments contenus dans l'archive à la façon de 'ls -l'
 */
int archive_affichage(liste *ls){
	int i;
	entete *infos;
	char mode[11];
	char date[13];
	char taille[PATH_MAX];
	char taille_tmp[PATH_MAX];
	char taille_max[PATH_MAX];
	struct tm *temps;
	off_t max;

	max = 0;
	for(i = 0; i < ls->length; i++){
		infos = liste_get(ls, i);
		if(infos->file_length > max) max = infos->file_length;
	}

	sprintf(taille_max, "%jd", max);

	for(i = 0; i < ls->length; i++){
		infos = liste_get(ls, i);

		/* Permissions */
		if(S_ISREG(infos->mode)) mode[0] = '-';
		else if(S_ISDIR(infos->mode)) mode[0] = 'd';
		else if(S_ISLNK(infos->mode)) mode[0] = 'l';
		else mode[0] = '?';

		mode[1] = (infos->mode & S_IRUSR)? 'r' : '-';
		mode[2] = (infos->mode & S_IWUSR)? 'w' : '-';
		mode[3] = (infos->mode & S_IXUSR)? 'x' : '-';

		mode[4] = (infos->mode & S_IRGRP)? 'r' : '-';
		mode[5] = (infos->mode & S_IWGRP)? 'w' : '-';
		mode[6] = (infos->mode & S_IXGRP)? 'x' : '-';

		mode[7] = (infos->mode & S_IROTH)? 'r' : '-';
		mode[8] = (infos->mode & S_IWOTH)? 'w' : '-';
		mode[9] = (infos->mode & S_IXOTH)? 'x' : '-';

		mode[10] = '\0';

		/* Date */
		temps = localtime(&infos->m_time);
		if(temps->tm_mon == 0){
			date[0] = 'j'; date[1] = 'a'; date[2] = 'n';
		}else if(temps->tm_mon == 1){
			date[0] = 'f'; date[1] = 'e'; date[2] = 'v';
		}else if(temps->tm_mon == 2){
			date[0] = 'm'; date[1] = 'a'; date[2] = 'r';
		}else if(temps->tm_mon == 3){
			date[0] = 'a'; date[1] = 'v'; date[2] = 'r';
		}else if(temps->tm_mon == 4){
			date[0] = 'm'; date[1] = 'a'; date[2] = 'i';
		}else if(temps->tm_mon == 5){
			date[0] = 'j'; date[1] = 'u'; date[2] = 'n';
		}else if(temps->tm_mon == 6){
			date[0] = 'j'; date[1] = 'u'; date[2] = 'i';
		}else if(temps->tm_mon == 7){
			date[0] = 'a'; date[1] = 'o'; date[2] = 'u';
		}else if(temps->tm_mon == 8){
			date[0] = 's'; date[1] = 'e'; date[2] = 'p';
		}else if(temps->tm_mon == 9){
			date[0] = 'o'; date[1] = 'c'; date[2] = 't';
		}else if(temps->tm_mon == 10){
			date[0] = 'n'; date[1] = 'o'; date[2] = 'v';
		}else if(temps->tm_mon == 11){
			date[0] = 'd'; date[1] = 'e'; date[2] = 'c';
		}

		date[3] = ' ';
		if(temps->tm_mday < 10){
			date[4] = ' '; date[5] = '0'+temps->tm_mday;
		}else{
			date[4] = '0'+ (temps->tm_mday/10); date[5] = '0' + (temps->tm_mday%10);
		}

		date[6] = ' ';
		if(temps->tm_hour < 10){
			date[7] = '0'; date[8] = '0'+temps->tm_hour;
		}else{
			date[7] = '0' + (temps->tm_hour/10); date[8] = '0' + (temps->tm_hour%10);
		}

		date[9] = ':';
		if(temps->tm_min < 10){
			date[10] = '0'; date[11] = '0'+temps->tm_min;
		}else{
			date[10] = '0' + (temps->tm_min/10); date[11] = '0' + (temps->tm_min%10);
		}
		date[12] = '\0';


		/* Taille */
		sprintf(taille_tmp, "%jd", infos->file_length);
		memset(taille, ' ', sizeof(taille));
		strcpy(taille+(strlen(taille_max)-strlen(taille_tmp)), taille_tmp);

		/* Affichage */
		printf("%s %s %s %s\n", mode, taille, date, infos->path);
	}

	return 0;
}

/*
	Lit une archive afin de creer la liste des éléments qu'elle contient
 */
int archive_read(char* nom_archive, liste *ls){
	if(nom_archive == NULL || ls == NULL) return 1;

	int lus, archive_fd;
	entete *infos;
	struct flock lock_r;

	lock_r.l_type = F_RDLCK;
	lock_r.l_start = 0;
	lock_r.l_whence = SEEK_SET;
	lock_r.l_len = 0;

	if((archive_fd = open(nom_archive, O_RDONLY)) < 0){
		warn("Impossible de lire l'archive %s", nom_archive);
		return 1;
	}
	if(fcntl(archive_fd, F_SETLKW, &lock_r) < 0){
		warn("Impossible de verrouiller %s", nom_archive);
		close(archive_fd);
		return 1;
	}

	while(1){

		infos = entete_init();
		infos->seek_debut = lseek(archive_fd, 0, SEEK_CUR);
		if((lus = read(archive_fd, &infos->path_length, sizeof(size_t))) < sizeof(size_t)){
			if(lus == 0){
				infos->checksum = malloc(sizeof(char));
				infos->path = malloc(sizeof(char));
				entete_free(infos);
				break;
			}
			entete_free(infos);
			return 1;
		}

		if(read(archive_fd, &infos->file_length, sizeof(off_t)) < sizeof(off_t)){
			entete_free(infos);
			return 1;
		}

		if(read(archive_fd, &infos->mode, sizeof(mode_t)) < sizeof(mode_t)){
			entete_free(infos);
			return 1;
		}

		if(read(archive_fd, &infos->m_time, sizeof(time_t)) < sizeof(time_t)){
			entete_free(infos);
			return 1;
		}

		if(read(archive_fd, &infos->a_time, sizeof(time_t)) < sizeof(time_t)){
			entete_free(infos);
			return 1;
		}

		infos->checksum = malloc(sizeof(char)*32);
		if(read(archive_fd, infos->checksum, sizeof(char)*32) < sizeof(char)*32){
			entete_free(infos);
			return 1;
		}

		infos->path = malloc(sizeof(char)*(infos->path_length));
		if(read(archive_fd, infos->path, sizeof(char)*(infos->path_length)) < sizeof(char)*(infos->path_length)){
			entete_free(infos);
			return 1;
		}

		infos->seek_file = lseek(archive_fd, 0, SEEK_CUR);
		if(lseek(archive_fd, infos->file_length, SEEK_CUR) < 0){
			warn("Erreur lseek pendant la lecture de l'archive");
			entete_free(infos);
			return 1;
		}

		liste_add(ls, infos);
	}

	close(archive_fd);
	return 0;
}

/*
	Ecrit les éléments contenus dans la liste dans l'archive
 */
int archive_write(int archive_fd, liste *ls){
	int lus, i, fichier_fd;
	char buf[BUFSIZE];
	entete* infos;
	char* complete_path;
	struct flock lock_r;

	lock_r.l_type = F_RDLCK;
	lock_r.l_start = 0;
	lock_r.l_whence = SEEK_SET;
	lock_r.l_len = 0;

	for(i = 0; i < ls->length; i++){
		infos = liste_get(ls, i);
		if(infos == NULL) return 1;

		if(write(archive_fd, &infos->path_length, sizeof(size_t)) < 0) return 1;
		if(write(archive_fd, &infos->file_length, sizeof(off_t)) < 0) return 1;
		if(write(archive_fd, &infos->mode, sizeof(mode_t)) < 0) return 1;
		if(write(archive_fd, &infos->m_time, sizeof(time_t)) < 0) return 1;
		if(write(archive_fd, &infos->a_time, sizeof(time_t)) < 0) return 1;
		if(write(archive_fd, infos->checksum, sizeof(char)*32) < 0) return 1;
		if(write(archive_fd, infos->path, sizeof(char)*infos->path_length) < 0) return 1;

		if(S_ISREG(infos->mode)){
			complete_path = malloc(strlen(infos->pre_path)+strlen(infos->path)+1);
			complete_path[0] = '\0';
			strcat(complete_path, infos->pre_path);
			strcat(complete_path, infos->path);

			if((fichier_fd = open(complete_path, O_RDONLY)) < 0){
				warn("Impossible de d'accéder à l'élément %s", complete_path);
				close(fichier_fd);
				return 1;
			}
			if(fcntl(fichier_fd, F_SETLKW, &lock_r) < 0){
				warn("Impossible de verrouiller %s", complete_path);
				close(fichier_fd);
				free(complete_path);
				return 1;
			}

			while((lus = read(fichier_fd, buf, BUFSIZE)) != 0){
				if(write(archive_fd, buf, lus) < 0){
					warn("Erreur lors de l'écriture de %s dans l'archive", complete_path);
					free(complete_path);
					close(fichier_fd);
					return 1;
				}
			}
			free(complete_path);
			close(fichier_fd);
		}else if(S_ISLNK(infos->mode)){
			complete_path = malloc(strlen(infos->pre_path)+strlen(infos->path)+1);
			complete_path[0] = '\0';
			strcat(complete_path, infos->pre_path);
			strcat(complete_path, infos->path);

			if((lus = readlink(complete_path, buf, BUFSIZE)) < 0){
				warn("Impossible de lire le lien symbolique %s", complete_path);
				free(complete_path);
				return 1;
			}
			buf[lus] = '\0';

			if(write(archive_fd, buf, lus) < 0){
				warn("Erreur lors de l'écriture de %s dans l'archive", complete_path);
				free(complete_path);
				return 1;
			}
			free(complete_path);
		}
	}

	return 0;
}

/*
	Calcul l'empreinte md5 du fichier via un processus fils
 */
char* md5(char* fichier){
	int tube[2], status;
	char* res;

	if(pipe(tube) < 0){
		warn("Erreur lors du calcul de l'empreintre md5 de %s", fichier);
		return NULL;
	}

	status = fork();
	if(status < 0){
		warn("Erreur lors du calcul de l'empreintre md5 de %s", fichier);
		return NULL;
	}else if(status == 0){
		close(tube[0]);
		if(dup2(tube[1], STDOUT_FILENO) < 0){
			warn("Erreur lors du calcul de l'empreintre md5 de %s", fichier);
			return NULL;
		}
		execlp("md5sum", "md5sum", fichier, (char*)0);
		close(tube[1]);
		exit(0);
	}else{
		close(tube[1]);
		res = malloc(sizeof(char)*32);
		read(tube[0], res, sizeof(char)*32);
		close(tube[0]);
	}

	return res;
}

/*
	Récupère le nom de la cible du path
 */
char *only_name(char *path){
	int i, pos, name_length;
	char *name;

	pos = 0;
	for(i = 0; i < strlen(path); i++){
		if(path[i] == '/') pos = i;
	}
	
	if(pos == 0){
		name = malloc(sizeof(char)*(strlen(path)+1));
		strcpy(name, path);
		return name;
	}
	
	name_length = strlen(path)-pos;
	name = malloc(sizeof(char)*name_length);
	strcpy(name, path+((pos+1)*sizeof(char)));
	name[name_length-1] = '\0';
	return name;
}

/*
	Récupère tout le path sans la cible finale
 */
char *only_path(char *path){
	int i, pos;
	char *name;

	pos = 0;
	for(i = 0; i < strlen(path); i++){
		if(path[i] == '/') pos = i;
	}

	if(pos == 0){
		name = malloc(sizeof(char));
		name[0] = '\0';
		return name;
	}
	
	name = malloc(sizeof(char)*(pos+2));
	strncpy(name, path, pos+1);
	name[pos+1] = '\0';
	return name;
}