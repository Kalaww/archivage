#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "archive.h"


/* Initialise une structure entete */
entete* entete_init(){
	static int compteur = 1;
	entete *a = malloc(sizeof(entete));
	a->pre_path = NULL;
	a->hashcode = compteur++;
	return a;
}


/* Free entete */
void entete_free(entete *a){
	if(a->path != NULL) free(a->path);
	if(a->pre_path != NULL) free(a->pre_path);
	if(a->checksum != NULL) free(a->checksum);
	free(a);
}


/* Affiche le contenu de entete */
void entete_print(entete* a){
	char* sum;
	if(a == NULL){
		printf("NULL");
		return;
	}
	printf("hascode: %d\npath_length: %zd\nfile_length: %jd\nmode: %d\nm_time: %lld\na_time: %lld\nchecksum: ",
			a->hashcode,
			a->path_length,
			a->file_length,
			a->mode,
			(long long) a->m_time,
			(long long) a->a_time);

	sum = malloc(sizeof(char)*33);
	sum[0] = '\0';
	strncat(sum, a->checksum, 32);
	sum[32] = '\0';
	printf("%s", sum);
	free(sum);
	printf("\npath: %s\npre_path: %s\nseek_debut: %jd\nseek_file: %jd\n", a->path, a->pre_path, a->seek_debut, a->seek_file);
}