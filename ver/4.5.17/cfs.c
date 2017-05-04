#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <wchar.h>
#include <Python.h>
#include <malloc.h>
#include <curl/curl.h>
#define FUSE_USE_VERSION 30
//#define _FILE_OFFSET_BITS 64
#include <fuse.h>
#include <errno.h>

typedef struct file {
	char *name;
	int *size;
	char *date;
	char *time;
	} CFILE;
	
typedef struct dir {
	CFILE* this;
	struct dir *dirs;
	CFILE *files;
	} DIR;

typedef struct dfile {
	const char *name;
	long size;
	const char *date;
	const char *time;
	struct dfile *next_sibling;
	struct dfile *first_child;
	struct dfile *parent;
	char isdir;
	}DFILE;

struct string {
	char *ptr;  
	size_t len; 
	CURLcode code;
	};
	
struct configuration {
	char *base_url;
	long int port;
	long int uid;
	long int gid;
	char *mode;	
};
	
#define SERVER_URL broskini.xf.cz/folder1/
#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)
#define FILENAME "config.conf"
#define DELIMITER "="
#define TRUE 1
#define FALSE 0
#define MAX 4096


void iterate_dict(PyObject*, DFILE*,int*);
void alloc_properties(DFILE *, PyObject *);
const char* get_py_string(PyObject*);
void print_fs(DFILE *, int,char*);
DFILE *find_file(char *);
time_t get_atime(const char *, const char *);
time_t get_mtime(const char *,const char *);
int call_parser(char *, char* , int);

typedef int (*fuse_fill_dir_t) (void *buf, const char *name, const struct stat *sbuft, off_t off); 

//global variables
DFILE *root = (DFILE*)NULL;
char **dirs=NULL;
int dir_index=0;
char *mode;
char *loaded_struct_path=NULL;
char *base_url;



struct configuration *read_conf() {
	
	FILE *f = fopen(FILENAME, "r");
	char *ppath;
	struct configuration *x;
	x = malloc(1*sizeof(struct configuration));
	int i = 0;
	char *str_port; 
	char *str_uid;
	char *str_gid;
	long int port = 1;
	long int uid = 1;
	long int gid = 1;
	char *mode;
	
		ppath = (char*)malloc(50*sizeof(char));
		str_port = (char*)malloc(50*sizeof(char));
		str_uid = (char*)malloc(50*sizeof(char));
		str_gid = (char*)malloc(50*sizeof(char));
		mode = malloc(2*sizeof(char));
	
	
	if(f == NULL) { 
		fprintf(stderr, "%s", "error opening file\n"); }
	
	else {
		
		
		char line[MAX];
		
		
		//cita sa konfigurak kym neni jeho koniec
		while(fgets(line, sizeof(line), f) != NULL) {
			
			char *token;
			line[strlen(line)-1]='\0';
			
			if(line[0]=='#') {
				//nerobi sa nic -> comment
				}
				
			else {
				
				token = strtok(line, DELIMITER);
				printf("PRED IFOM: v tokene je: %s\n", token);
				if(strcmp(token,"SERVER_URL")==0) { 
					
					strcpy(ppath,strtok(NULL, DELIMITER));
					printf("IF SERV_URLfunkcia read_conf --- > IPCka: %s\n",ppath);
					x->base_url = ppath;					
					
					}
					
				if(strcmp(token,"PORT")==0) { 
					
					token = strtok(NULL, DELIMITER);
					printf("IF PORT : v tokene je: %s\n", token);
					port = strtol(token,&str_port, 10);
					printf("funkcia read_conf --- > port: %ld\n",port);
					x->port = port;
					
					}
					
				if(strcmp(token,"UID")==0) { 
					
					token = strtok(NULL, DELIMITER);
					printf("IF UID : v tokene je: %s\n", token);
					uid = strtol(token,&str_uid, 10);
					printf("funkcia read_conf --- > uid: %ld\n",uid);
					x->uid = uid;
					
					}
					
				if(strcmp(token,"GID")==0) { 
					
					token = strtok(NULL, DELIMITER);
					printf("IF GID : v tokene je: %s\n", token);
					gid = strtol(token,&str_gid, 10);
					printf("funkcia read_conf --- > gid: %ld\n",gid);
					x->gid = gid;
					
					}
					
										
				if(strcmp(token,"MODE")==0) { 
					
					token = strtok(NULL, DELIMITER);
					printf("IF MODE : v tokene je: %s\n", token);
					strcpy(mode,token);
					printf("funkcia read_conf --- > mode: %s\n",mode);
					x->mode = mode;
					
					}

													
				}
						
			i++;
			
			}
							
		}
		
		printf("\n");
		puts("idem pisat ppath");
		printf("%s\n", ppath);
		printf("%ld\n", port);
		printf("%ld\n", uid);
		printf("%ld\n", gid);
		printf("%s\n", mode);
		
		return x;
	
	}


static int not_permitted() {
	return -EPERM;
	}

int find_last_slash(char *path)
{
	int len =strlen(path);
	int index = len-1;
	while(path[index] != '/') index--;
	return index;
}

static int fsgetattr(const char *path, struct stat *st) {
	
	memset(st,0,sizeof(struct stat));
	
	
	char *temp_path = strdup(path);
	DFILE *node=NULL;
	char is_root = TRUE;
	
	if(strcmp(path,"/") != 0) {
		printf("HLADAM FSGETATTR PRE %s \n", path);
		/*
		if(strcmp(mode, "R") == 0)
		{
			int index = find_last_slash(temp_path);
			int i;
			for(i=0;i<index;i++) temp_path++;
		}
		* */
		
		node = find_file(temp_path);
		if(node)
		printf("NASIEL SOM NODE %s \n", node->name);
		else
		printf("NENASIEL SOM NODE %s\n",path);
		is_root=FALSE;
	}
	
	if(node == NULL && !is_root)
	{
		return -ENOENT;
	}
	
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = time(NULL);
	st->st_mtime = time(NULL);
	
	printf("FSGETATTR: %s\n", path);
	
	
	
	//if node is a directory
	if (strcmp(path,"/") == 0) {
		st->st_mode = S_IFDIR | 0444; //0755;
		st->st_nlink = 2;
	}
	else if(node != NULL && node->isdir) {
		printf("DIR\n");
		st->st_uid = getuid();
		st->st_gid = getgid();
		printf("Tu-1\n");
		if((node->date != NULL) && (node->time != NULL)) {
		st->st_atime = get_atime(node->date, node->time);
		}
		if((node->date != NULL) && (node->time != NULL)) {
		st->st_mtime = get_mtime(node->date, node->time);
		}
		st->st_mode = S_IFDIR | 0444; //0755;
		printf("Tu-4\n");
		st->st_nlink = 2;
		st->st_size = 4096;	
		}	
	//if node is a regular file
	else {
		printf("FILE\n");
		st->st_uid = getuid();
		st->st_gid = getgid();
		//ak je node v strome - tj neni to skryty suubor
		if(node != NULL)
		{
			st->st_atime = get_atime(node->date, node->time);
			st->st_mtime = get_mtime(node->date, node->time);
			st->st_size = node->size;
		}
		
		//ak je node skryty subor
		else {
			st->st_size = 1024;
			}		
			
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;

		}
	printf("KONIEC FSGETATTR\n");
	
	return 0;
	}
	
//converts node's const char to st_atime
time_t get_atime(const char *str_date, const char *str_time ) {	
	time_t cas; //= time(NULL);	//default time je current time
	//pomocna struktura, jej naplnenie a konverzia na datovy typ time_t
	struct tm tm;
	int hh, mm, ss;
	int year, mday, mon;
	ss = 1;
	
	sscanf(str_time,"%d:%d", &hh, &mm);
	sscanf(str_date,"%d-%d-%d", &year, &mon, &mday);
	tm.tm_year = year - 1900;
	tm.tm_mon = mon - 1;
	tm.tm_mday = mday;				
	tm.tm_hour = hh;
	tm.tm_min = mm;
	tm.tm_sec = ss;
	tm.tm_isdst = -1;
	cas = mktime(&tm);
						
	return cas;
	}

//converts node's const char to st_mtime
time_t get_mtime(const char *str_date,const char *str_time) {	
	time_t cas; //= time(NULL);	//default time je current time
	//pomocna struktura, jej naplnenie a konverzia na datovy typ time_t
	struct tm tm;
	int hh, mm, ss;
	int year, mday, mon;
	ss = 1;
	
	sscanf(str_time,"%d:%d", &hh, &mm);
	sscanf(str_date,"%d-%d-%d", &year, &mon, &mday);
	tm.tm_year = year - 1900;
	tm.tm_mon = mon - 1;
	tm.tm_mday = mday;				
	tm.tm_hour = hh;
	tm.tm_min = mm;
	tm.tm_sec = ss;
	tm.tm_isdst = -1;
	cas = mktime(&tm);
						
	return cas;
	}
	
void print_dirs(void *buffer, fuse_fill_dir_t filler, DFILE* cur)
{
		while(cur)
	{
		filler(buffer, cur->name, NULL, 0);
		cur=cur->next_sibling;
	}
}	
	
//najde spravny subor/adresar podla path	
DFILE *find_file(char *path)	 {
	//tuto vzdy bude treba posielat konvertovane path z const char * na char*
	//cize asi sa to bude riesit cez strdup, treba si davat pozor na to ze treba
	//dealokovat pointre
	
	if(strcmp(path, "/") == 0 )
	{ return root; }
	DFILE *cur=root->first_child;
	char found=FALSE;
	const char s[2] = "/";
	char *token;
	char *temp_path = strdup(path);
	/* get the first token */
	token = strtok(temp_path, s);
	/* walk through other tokens */
	printf("Volany find_file s path: %s\n", path);
	
	
	while( token != NULL ) 
	{
		found=FALSE;
       //printf( " %s\n", token );
		while(cur!=NULL) 
		{
			if(strcmp(cur->name, token) != 0) 
			{
				cur=cur->next_sibling;
			}
			else
			{
				found=TRUE;
				break;
			}
		}
		if (found) {
			token = strtok(NULL, s);
			if(token != NULL)
				cur=cur->first_child;
			else
				; //v smerniku cur je teraz spravny najdeny node
		}
		else
		{
			if(strcmp(root->name, token) == 0)
			{
				cur=root;
			}
			else 
			{
			cur=NULL;
			}
			
			break;
		}
	}
	free(temp_path);
	
	return cur;
}

void fill_dir_T(const char *path, void *buffer, fuse_fill_dir_t filler) {
	DFILE *cur=NULL;
	//najdeme si spravny directory
	cur=root->first_child;
	char found = FALSE;
	char *path2  = strdup(path);
	if(strcmp(path,"/") == 0)
	{
		print_dirs(buffer,filler,cur);
	}
	else 
	{
	cur=find_file(path2);
	if(cur != NULL) {
	found=TRUE; //nasli sme priecinok, teraz zoberieme z neho prvy child
	cur=cur->first_child;
	}
	
	else 
	{
		return;
	}
	
	if(found) //TODO: DOIMPLEMENTOVAT NEJAKE ROZUMNE RIESENIE NENAJDENIA SPRAVNEHO ADRESARA
	{
	print_dirs(buffer,filler,cur);
	}
	
	}
	
	
	}

char *create_url(char *path)
{
	int base_url_len = strlen(base_url);
	int path_len = strlen(path);
	char *new_url = malloc(base_url_len + path_len + 2);
	strcpy(new_url, base_url);
	if(new_url[base_url_len -1] == '/')
	{
		
	}
	else
	{
		strcat(new_url, "/");
	}
	while(path[0] == '/') path++;
	strcat(new_url, path);
	
	return new_url;
}

void dealloc_DFILE(DFILE* node) 
{
	printf("CALLING DEALLOC\n");
	DFILE* child = node->first_child;
	DFILE* next = NULL;
	while(child != NULL)
	{
		next=child->next_sibling;
		free((char*)child->name);
		free((char*)child->date);
		free((char*)child->time);
		free(child);
		child=next;
	}
	node->first_child = NULL;
	
	if(node != root) {
	DFILE* sibling=node->parent->first_child;
	node->parent->first_child=node;
	while(sibling!=NULL){
	if(sibling == node) {
		sibling=sibling->next_sibling;
		continue;
	}
	
	next = sibling->next_sibling;
	free((char*)sibling->name);
	free((char*)sibling->date);
	free((char*)sibling->time);
	free(sibling);
	sibling=next;
	}
	node->next_sibling=NULL;
	}

}

void fill_dir_R(const char *path, void *buffer, fuse_fill_dir_t filler) {
	DFILE *cur=NULL;
	printf("FILL DIR R pre PATH: %s\n",path);
	int movement=-1;
	if(strcmp(path, loaded_struct_path) == 0)
	{	printf("++NEMUSIM LOADOVAT++\n"); 
		printf("LOADED: %s , PATH: %s .. SU TO ISTE\n", loaded_struct_path, path);
		//moze pokracovat dalej
		cur = find_file(loaded_struct_path);
		cur=cur->first_child;
		
	}
	else
	{
		printf("++MUSIM LOADOVAT++\n"); 
		printf("LOADED: %s , PATH: %s\n", loaded_struct_path, path);
		char *current_url;
		char *copy_path;
		char *orig_path;
		copy_path = strdup(path);
		orig_path = strdup(path);
		current_url = create_url(copy_path);
	
		//dealloc_DFILE(root);
		printf("CALLING PARSER\n");
		if(strlen(path) > strlen(loaded_struct_path))
		{ //hybem sa smerom dovnutra stromu
			movement=1;
		}
		else
		{ //hybem sa von zo stromu
			movement=0;
		}
		printf("ORIG PATH: %s, COPY PATH: %s\n", orig_path, copy_path);
		call_parser(current_url, orig_path, movement);
		if(loaded_struct_path != NULL)
		{
			free(loaded_struct_path);
			loaded_struct_path = NULL;
		}
		loaded_struct_path = strdup(path);
		printf("NOVA LOADED_ST_P: %s\n", loaded_struct_path);
		char *temp_find = strdup(loaded_struct_path);
		cur = find_file (temp_find);
		free(temp_find);
		cur = cur->first_child;
		printf("PRVY FREE\n");
		free(current_url);
		printf("DRUHY FREE\n");
		free(copy_path);
		free(orig_path);
	}
	
	
	//najdeme si spravny directory
	
	char found = FALSE;
	printf("IDEM VYPISOVAT PRINTDIRS\n");
	print_dirs(buffer,filler,cur);
	printf("PO PRINT DIRS \n");
	/*
	char *path2  = strdup(path);
	if(strcmp(path,"/") == 0)
	{
		print_dirs(buffer,filler,cur);
	}
	else 
	{
	cur=find_file(path2);
	if(cur != NULL) {
	found=TRUE; //nasli sme priecinok, teraz zoberieme z neho prvy child
	cur=cur->first_child;
	}
	
	else 
	{
		return;
	}
	
	if(found) //TODO: DOIMPLEMENTOVAT NEJAKE ROZUMNE RIESENIE NENAJDENIA SPRAVNEHO ADRESARA
	{
		print_dirs(buffer,filler,cur);
	}
	
	}
	*/
	
	}

static int fsreaddir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *FI) {
	
//	printf("vypisujem subory v adresari %s/n", path);
//	fflush(stdout);
	
	
	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);
	
	if(strcmp(mode, "T") == 0) 
	{
		fill_dir_T(path,buffer,filler);
	}
	else if(strcmp(mode, "R") == 0)
	{
		fill_dir_R(path,buffer,filler);
	}
	else 
	{
		printf("NEPLATNY MOD\n");
	}
		
	return 0;
	
	}
	
void init_str(struct string *str) {
	str->len = 0;
	str->ptr = malloc(str->len + 1);
	if(str->ptr == NULL) 
	{ //printf("error\n");
		 }
	str->ptr[0]='\0';

}

size_t writef(void *ptr, size_t size, size_t nmemb, struct string *str) {
	size_t len1 = str->len + size*nmemb;
	str->ptr = realloc(str->ptr, len1+1);
	if(str->ptr == NULL) 
	{ 
	//	printf("error\n");
	}
	
	memcpy(str->ptr+str->len, ptr, size*nmemb);
	str->ptr[len1] = '\0';
	str->len = len1;
	
	return size*nmemb;
} 

struct string get_file_content(const char *path) { 
	
	struct string str;
	char *url = malloc((strlen(base_url)+strlen(path)+2)*sizeof(char));
	//strcpy(url,STR(SERVER_URL));	
	strcpy(url,base_url);
	strcat(url,path);

	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	if(curl) {
		init_str(&str);	
		curl_easy_setopt(curl, CURLOPT_URL, url); 
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writef); 
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str); 
		res = curl_easy_perform(curl);
		str.code = res; 
		}	
	curl_easy_cleanup(curl);
	free(url);	
		
	return str;	
	
	}

static int fsread(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	
	char *out = NULL; 
	printf("VOLANY FSREAD\n");
	
	if(find_file(strdup(path))){
		struct string filecontent = get_file_content(path);	
		if(filecontent.code != CURLE_OK) {
			return -EIO;
		}
		out = filecontent.ptr;
		
		}
	else { return -ENOENT; }
		
	//free(filecontent);
	
	if(out)
	{
		//int dlzka = strlen(out) + size;
		int dlzka = strlen(out) - offset;
		
		memcpy(buffer, out + offset, dlzka + 1);
		free(out);
		return dlzka;

	}
	
	else { 
		//fprintf(stderr, "cuddly-fs error : Subor so zadanym menom neexistuje.\n");
		return -EIO; 
		}
	
	}
	
void parse_command_line_args(char *c) {

	if(strcmp(c,"-h") == 0) { printf("help\n"); }
	else if(strcmp(c,"-R") == 0) { 
		//printf("refreshed mode\n"); 
		strcpy(mode, "R");
		}
	else if(strcmp(c,"-T") == 0) { 
		//printf("tree mode\n"); 
		strcpy(mode, "T"); 
		}
	else {
		//printf("error: unknown switch\n");
	}


}

void fsdestroy(void *private_data)
{
	if(strcmp(mode, "R") == 0)
	{
		Py_Finalize();
	}
} 
	
static struct fuse_operations operations = {
	
	.getattr = fsgetattr,
	.readdir = fsreaddir,
	.read = fsread,
	.destroy = fsdestroy,
	
	.mknod = not_permitted,
	.mkdir = not_permitted,
	.unlink = not_permitted,
	.rmdir = not_permitted,
	.symlink = not_permitted,
	.rename = not_permitted,
	.link = not_permitted,
	.chmod = not_permitted,
	.chown = not_permitted,
	.truncate = not_permitted,
	.utime = not_permitted,
	.write = not_permitted,
	.setxattr = not_permitted,
	.removexattr = not_permitted,
	
	
	};
	

void read_config() {
	
	FILE *f = fopen(FILENAME, "r");
	
	
	
	}
	
char *get_root_name(char *url)
{
	int index=strlen(url)-1;
	int counter=0;
	int slash;
	char *orig_url;
	orig_url = url;
	while(index>0)
	{
		if(url[index] == '/') {
			slash=index;
			counter++;
		}
		
		if(url[strlen(url)-1] == '/') {
		if(counter == 2)
			break;
		}
		else
		{
			if(counter == 1)
			break;
		}
		--index;
	}
	int i;
	for(i=0;i<slash+1;i++) url++;
	char *root_name = strdup(url);
	
	if(orig_url[strlen(orig_url)-1] == '/')
	root_name[strlen(root_name)-1]='\0';
	return root_name;
}


int call_parser(char *url, char *path, int movement)
{
	static char first_init = TRUE;
	char *filename = "mypy3"; //meno pythonackeho modulu na import
	PyObject *sName, *sModule, *sFunc, *sValue, *sArgs,*sUrl, *sMode;   //NEJAKE PYTHON OBJEKTY 
	PyObject *sLong;
	long int d=4;
	DFILE *cur_node=NULL;
	 //v roote zacina cela ta struktura suborov
	 //inicializovanie rootu typu DFILE
		
	
	if(first_init)
	{
		Py_Initialize(); //inicializacia python interpretera
		//sLong=PyLong_FromLong(d);
		PyRun_SimpleString(   //takto do syspATH PRIdame dalsie cesty
			"import sys\n"
			"sys.path.append('/home/pi/Documents/C/matko/najnovsie')\n"
			"sys.path.append('/home/matko/Desktop/cuddly-fs/ver/4.5.17')\n"
		);
		first_init = FALSE;
		loaded_struct_path = strdup("/");
		if(root == NULL) {
		root=(DFILE*)malloc(sizeof(DFILE));
		root->isdir=1;
		root->name = get_root_name(url);
		printf("ROOT NAME JE %s \n", root->name);
		root->size=0;
		root->parent = NULL;
		root->next_sibling = NULL;
		root->first_child=NULL;
		}
		else 
		{
		 
		}
		movement=-1;
		
	}
	cur_node = root;
	if(movement == 1 || movement == 0)
	{ //movement == 1 znamena ide sa hlbsie,
		// movement == 0 znamena ide sa plytsie, ked sa ide plytsie, treba dealokovat vsetky deti
		printf("POSLANA PATH : %s \n", path);
		char* temp_path = strdup(path);
		cur_node = find_file(path);
		printf("Najdeny node s menom: %s pre path %s a path_len %d \n", cur_node->name,temp_path, strlen(temp_path));
		//TODO: treba naimplementovat vyfreeovanie celej stromovej struktury az na root node	
			dealloc_DFILE(cur_node); //dealokuje vsetky deti
			printf("Po dealokovani\n");
		
	}
	
	int kind = PyUnicode_1BYTE_KIND; //tu nastavime 1bajtovy unicode
	sName=PyUnicode_FromKindAndData(kind, filename, (Py_ssize_t) strlen(filename)); //V TOM JE UNICODE NAZOV SKRIPTU, ABY HO MOHOL NAIMPORTOVAT A POTOM SA BUDE DALEJ SPUSTAT
	sModule = PyImport_Import(sName); //to co importujeme - ten modul, PYoBJECT
	Py_DECREF(sName);  //DECREF A INCREF - python pocita referencie aby vedel kedy spustit garbage collector, ma counter na referencie, ak je counter na 0, s premennou sa uz nic nerobi, a vtedy vola garbage collector, v pythone to robit netreba,ale v C API to musime robit manualne, ked vieme ze uz nepotrebujeme sName premennu, povieme mu to, je to velmi podobne ako free v Ccku
	
	//spravime python unicode string z URL stringu
	sUrl = PyUnicode_FromKindAndData(kind, url, (Py_ssize_t) strlen(url));
	sMode = PyUnicode_FromKindAndData(kind, mode, (Py_ssize_t) sizeof(char));
	
	sArgs = PyTuple_Pack(2, sUrl, sMode);
	printf("PRED PYTHON FUNKCIOU \n");
	if (sModule != NULL) //tak sa nam podarilo naimportovat
	{
		sFunc = PyObject_GetAttrString(sModule, "main");
		if (sFunc && PyCallable_Check(sFunc)) //sFunc je objekt main pythonovskej funkcie
		{
			sValue = PyObject_CallObject(sFunc, sArgs); //navratova hodnota mainu - ten root - je v sVALUE
														//tu sa vola python funkcia
		}
		else
		{
			printf("SFUNC AND CHYBA!!\n");
		}
		if (sValue != NULL)
		{
			printf("PYTHON FUNKCIA VRATILA OBJEKT!!!!!!\n");
		}
		else
		{
			printf("NIC MI SEM NEPRISLO!!!!\n");
		}
		
	}
	else  //nepodarilo sa naimportovat
	{
		printf("SMODULE JE NULL!!!\n");
	}
	
	long dict_check = PyDict_CheckExact(sValue);
	if( dict_check == 0)
	{
		printf("Vrateny python objekt nie je dict, koncim program..\n");
		return -EIO;
	}
	

	
	PyObject *files = PyDict_GetItemString(sValue, "files"); //v sValue je root a je tam teda cela sttruktura pod klucom files
	if(PyDict_Size(files) == (Py_ssize_t) 0) {
		printf("PYTHON VRATIL FILES VELKOSTI 0\n");
		return -EIO;
	}
	int dirs_count=dir_index+1;
	printf("PRED ITERACIOU \n");
	//iterate_dict(files,root,&dirs_count);
	iterate_dict(files,cur_node,&dirs_count);
	printf("---- ZACINAM VYPISOVANIE V C ----\n");
	//allocate dirs char array
	//dirs=(char**)malloc(dirs_count * sizeof(char*)); //pole stringov na vypis
	//dirs[dir_index]=strdup("/");
	//dir_index++;
	//for(i=0;i<dirs_count;i++) dirs[i]=
	char *curdir=malloc(2*sizeof(char));
	curdir[0]='\0';
	//strcat(curdir,"/");
	print_fs(root,0,curdir);
	printf("----- VYPIS DIRS -------\n");
	//for(i=0;i<dirs_count;i++) printf("%s\n", dirs[i]);
	//Py_DECREF(sLong);
	Py_DECREF(files);
	Py_DECREF(sValue);
	Py_DECREF(sModule);
	
	if(strcmp(mode, "T") == 0)
	{
		Py_Finalize();
	}
	
	return 0;
	
}

int main(int argc, char *argv[]) {
	
	int k = 0;
	
	mode = malloc(2*sizeof(char)); //by default v tree mode
	//strcpy(mode,"R");
	//base_url = strdup("http://broskini.xf.cz/folder1");
	
	
	struct configuration *conf = NULL;
	conf = read_conf();

	printf("CONF-mode:%s %d\n", conf->mode, strlen(conf->mode));
	printf("CONF-url:%s %d\n", conf->base_url,strlen(conf->base_url));
	
	strcpy(mode,conf->mode);
	base_url = strdup(conf->base_url);//strdup("http://broskini.xf.cz/folder1");
	
	
	
	if(argc == 1) {
		//printf("note: no cmd args. filesystem will be using tree mode.\n");
		}

	//i=1 ~ first argument is executable file
	for(k = 1; k<argc; k++) {
		parse_command_line_args(argv[k]);
	}
	
	call_parser(base_url, "/", 1);
	
	//int argc;
	//wchar_t * argv[3];
	setlocale(LC_ALL, "en_US.UTF-8"); //tyka sa to stringov do pythonu ... argumenty do pythonu?
	//argc=2;
	//argv[0]=L"mypy.py";
	//argv[1]=L"http://broskini.xf.cz/folder1";
		
	return fuse_main(argc, argv, &operations, NULL);
	
	}
	
	//TODO: aj tuto treba vyriesit tie INCREF/DECREF lebo to bude potom
	//zbytocne zaberat pamat -- keby sa program dlho pouziva mohla by sa
	//aj vycerpat teoreticky
void iterate_dict(PyObject *sValue, DFILE *root,int *dirs_count) { //v iterate dict dostanem ten dictionary aj ten DFILE root, REKURZIA
	PyObject *key, *value;
	Py_ssize_t pos = 0;
	
	DFILE* cur=NULL;
	while(PyDict_Next(sValue, &pos, &key, &value))
	{
		if(root->first_child == NULL)
		{
			cur = (DFILE*)malloc(sizeof(DFILE));
			root->first_child = cur;
			cur->parent=root;
			cur->next_sibling=NULL;
			cur->first_child=NULL;
		}
		else
		{
			if(cur == NULL) cur = root->first_child;
			cur->next_sibling = (DFILE*)malloc(sizeof(DFILE));
			cur = cur->next_sibling;
			cur->next_sibling=NULL;
			cur->parent = root;
			cur->first_child=NULL;
		}

		cur->name = get_py_string(key);
		//printf("Key je: %s\n", cur->name);
		alloc_properties(cur, value);
		if(cur->isdir == 1)
		{
			++(*dirs_count);
			PyObject *files = PyDict_GetItemString(value, "files");
			//Py_INCREF(files);
			iterate_dict(files,cur,dirs_count);
			//Py_DECREF(files);
		}
	}
}

void alloc_properties(DFILE* cur, PyObject *dict) 
{
	PyObject *date = PyDict_GetItemString(dict, "date");
	PyObject *time = PyDict_GetItemString(dict, "time");
	PyObject *size = PyDict_GetItemString(dict, "size");
	PyObject *isdir = PyDict_GetItemString(dict, "isdir");
	cur->date=get_py_string(date);
	cur->time=get_py_string(time);
	const char *size_str = get_py_string(size);
	if (strcmp(size_str,"-") == 0)
	{
		cur->size=0;
	}
	cur->size=strtol(size_str,NULL,10);
	//PyLong_AsLong(size);
	if(PyLong_AsLong(isdir) == 1)
	{
		cur->isdir=1;
	}
	else
	{
		cur->isdir=0;
	}
	
	
}

//TODO: tuto v tejto funkcii je mozne ze sa globalne alokuju veci ktore sa
//potom neuvolnuju 
const char* get_py_string(PyObject *string)
{
	PyObject *ascii = PyUnicode_AsASCIIString(string);
	const char *compname = PyBytes_AsString(ascii);	
	Py_DECREF(ascii);
	return strdup(compname);
}
	
void print_fs(DFILE *root, int depth, char *current_dir)
{
	int i;
	DFILE *cur;
	cur=root->first_child;
	char *next_dir=NULL;

	while(cur!=NULL)
	{
		for(i=0;i<depth;i++) printf("\t");
		if(cur->isdir)
		{
			printf("%s/\n",cur->name);
			int alloc_size = strlen(current_dir) + strlen(cur->name) + 2;
			next_dir=(char*)malloc(alloc_size*sizeof(char));
			next_dir[0]='\0';
			//printf("Pre %s \n som alokoval %d miesta!, curdir: %d , curname: %d\n", cur->name, alloc_size, strlen(current_dir), strlen(cur->name));
			strcat(next_dir,current_dir);
			strcat(next_dir,"/");
			strcat(next_dir,cur->name);
		//	dirs[dir_index]=next_dir;
		//	dir_index++;
			print_fs(cur,depth+1,next_dir);
			
		}
		else
		{
			printf("%s\n",cur->name);
		}
		//printf("Pre %s :\n date:%s, time: %s, size:%ld \n", cur->name,cur->date, cur->time, cur->size );
		time_t cas = get_atime(cur->date, cur->time);
		struct tm *timeinfo;
		timeinfo = localtime(&cas);
		//printf("Cas a datum pre subor: %s\n", asctime(timeinfo));
		cur=cur->next_sibling;
	}
	
	
}
