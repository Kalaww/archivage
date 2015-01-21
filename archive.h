#ifndef H_ARCHIVE
#define H_ARCHIVE

#include "sys/types.h"

/*
	Structure d'une entete
 */
typedef struct archive{
	int hashcode;
	size_t path_length;
	off_t file_length;
	mode_t mode;
	time_t m_time;
	time_t a_time;
	char *checksum;
	char *path;
	char *pre_path;
	off_t seek_debut;
	off_t seek_file;
} entete;


entete* entete_init();

void entete_free(entete*);

void entete_print(entete* a);

#endif