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
	};
	
#define SERVER_URL broskini.xf.cz/folder1/
#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)
#define TRUE 1
#define FALSE 0

void iterate_dict(PyObject*, DFILE*,int*);
void alloc_properties(DFILE *, PyObject *);
const char* get_py_string(PyObject*);
void print_fs(DFILE *, int,char*);
DFILE *find_file(char *);
time_t get_atime(const char *, const char *);
time_t get_mtime(const char *,const char *);

typedef int (*fuse_fill_dir_t) (void *buf, const char *name, const struct stat *sbuft, off_t off); 

//global variables
DFILE *root = (DFILE*)NULL;
char **dirs=NULL;
int dir_index=0;

static int fsgetattr(const char *path, struct stat *st) {
	
	//experimental coding by pinky the little pornstar
	
	
	char *temp_path = strdup(path);
	DFILE *node=NULL;
	if(strcmp(path,"/") != 0) {
	node = find_file(temp_path);
	}
	
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = time(NULL);
	st->st_mtime = time(NULL);
	
	if(node == NULL)
	{
		
	}
	
	//if node is a directory
	if (strcmp(path,"/") == 0) {
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	}
	else if(node != NULL && node->isdir) {
		st->st_uid = getuid();
		st->st_gid = getgid();
		st->st_atime = get_atime(node->date, node->time);
		st->st_mtime = get_mtime(node->date, node->time);
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
		st->st_size = 4096;	
		}	
	//if node is a regular file
	else {
		st->st_uid = getuid();
		st->st_gid = getgid();
		if(node != NULL)
		{
			st->st_atime = get_atime(node->date, node->time);
			st->st_mtime = get_mtime(node->date, node->time);
			st->st_size = node->size;
		}
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;

		}
	
	
	/*
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = time(NULL);
	st->st_mtime = time(NULL);
	*/
	
	/*
	if (strcmp(path,"/") == 0) {
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	}
	else if (strcmp(path,"/dir2") == 0) {
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	}
	else {		
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = 1024;		
	}*/
	
	/*
	int i;
	char found=FALSE;
	for(i=0;i<dir_index;i++)
	{
		if(strcmp(path, dirs[i]) == 0)
		{
			st->st_mode = S_IFDIR | 0755;
			st->st_nlink = 2;
			found=TRUE;
		}
	}
	if(!found)
	{
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = 1024;
	}
	*/
	
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
	
//najde spravny subor/adresar podla path	
DFILE *find_file(char *path)	 {
	//tuto vzdy bude treba posielat konvertovane path z const char * na char*
	//cize asi sa to bude riesit cez strdup, treba si davat pozor na to ze treba
	//dealokovat pointre
	DFILE *cur=root->first_child;
	char found=FALSE;
	const char s[2] = "/";
	char *token;
	/* get the first token */
	token = strtok(path, s);
	/* walk through other tokens */
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
			cur=NULL;
			break;
		}
	}
	return cur;
}

void fill_dir(const char *path, void *buffer, fuse_fill_dir_t filler) {
	DFILE *cur=NULL;
	//najdeme si spravny directory
	cur=root->first_child;
	char found = FALSE;
	char *path2  = strdup(path);
	if(strcmp(path,"/") == 0)
	{
		
		while(cur)
	{
		filler(buffer, cur->name, NULL, 0);
		cur=cur->next_sibling;
	}
	}
	else 
	{
	cur=find_file(path2);
	if(cur != NULL) {
	found=TRUE; //nasli sme priecinok, teraz zoberieme z neho prvy child
	cur=cur->first_child;
	}
	
	if(found) //TODO: DOIMPLEMENTOVAT NEJAKE ROZUMNE RIESENIE NENAJDENIA SPRAVNEHO ADRESARA
	while(cur)
	{
		filler(buffer, cur->name, NULL, 0);
		cur=cur->next_sibling;
	}
	}
	
	
	}

static int fsreaddir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *FI) {
	
	printf("vypisujem subory v adresari %s/n", path);
	
	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);
	
	if(strcmp(path,"/") == 0) {
		//filler(buffer, "helloworld", NULL, 0);
		//filler(buffer, "hello", NULL, 0);
		fill_dir(path,buffer,filler);		
		}
	else {
		fill_dir(path,buffer,filler);
	}
		
	return 0;
	
	}
	
void init_str(struct string *str) {
	str->len = 0;
	str->ptr = malloc(str->len + 1);
	if(str->ptr == NULL) { printf("error\n"); }
	str->ptr[0]='\0';

}

size_t writef(void *ptr, size_t size, size_t nmemb, struct string *str) {
	size_t len1 = str->len + size*nmemb;
	str->ptr = realloc(str->ptr, len1+1);
	if(str->ptr == NULL) { printf("error\n");}
	
	memcpy(str->ptr+str->len, ptr, size*nmemb);
	str->ptr[len1] = '\0';
	str->len = len1;
	
	return size*nmemb;
} 

struct string get_file_content(const char *path) { 
	
	struct string str;
	char *url = malloc(1024*sizeof(char));
	strcpy(url,STR(SERVER_URL));	
	strcat(url,path);
	//str.ptr = malloc(1);
	//str.ptr[0]='\0';

	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	if(curl) {
		init_str(&str);	
		curl_easy_setopt(curl, CURLOPT_URL, url); 
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writef); 
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str); 
		res = curl_easy_perform(curl); 
		}	
	curl_easy_cleanup(curl);
	free(url);	
		
	return str;	
	
	}

static int fsread(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	
	char helloworld_cont[] = "Hello world !";
	char hello_cont[] = "Hello!";
	char *out = NULL; 
	
	if (strcmp(path,"/helloworld") == 0) {
		out = helloworld_cont;		
		}
	else if (strcmp(path,"/hello") == 0) {
		out = hello_cont;		
		}
	else {
		
		struct string filecontent = get_file_content(path);		
		out = filecontent.ptr;
		
		}
		
	//free(filecontent);
	if(out)
	{
	memcpy(buffer, out + offset, size);
	return strlen(out) - offset;
	}
	
	else { return 0; }
	
	}
	
static struct fuse_operations operations = {
	
	.getattr = fsgetattr,
	.readdir = fsreaddir,
	.read = fsread,
	
	};

int main(int argc, char *argv[]) {
	//int argc;
	//wchar_t * argv[3];
	setlocale(LC_ALL, "en_US.UTF-8");
	//argc=2;
	//argv[0]=L"mypy.py";
	//argv[1]=L"http://broskini.xf.cz/folder1";
	char *filename = "mypy3"; //meno pythonackeho modulu na import
	PyObject *sName, *sModule, *sFunc, *sValue, *sysPath;
	PyObject *sLong;
	long int d=4;
	root=(DFILE*)malloc(sizeof(DFILE));
	
	Py_Initialize();
	
	sLong=PyLong_FromLong(d);
	sysPath = PySys_GetObject("path");
	long len = PySequence_Length(sysPath);
	printf("Povodne je v path je %ld poloziek.\n", len);
	
	PyObject *sitem;
	int i;
	for (i=0;i<len;i++) {
	sitem = PySequence_GetItem(sysPath,(Py_ssize_t) i);
	PyObject *ascii = PyUnicode_AsASCIIString(sitem);
	char *compname = PyBytes_AsString(ascii);
	Py_DECREF(ascii);
	printf("Path %d.: %s\n", i+1,compname);
	}
	
	fflush(stdout);
	
	//PySys_SetArgv(argc,argv);
	//file=fopen("mypy.py","r");
	//PyRun_SimpleFile(file, "mypy.py");
	
	PyRun_SimpleString(
		"import sys\n"
		"sys.path.append('/home/pi/Documents/C/matko')\n"
		"sys.path.append('/home/matko/Desktop/cuddly-fs')\n"
	);
	
	sysPath = PySys_GetObject("path");
	len = PySequence_Length(sysPath);
	printf("Po pridani je v path je  %ld poloziek.\n", len);
	
	for (i=0;i<len;i++) {
	sitem = PySequence_GetItem(sysPath,(Py_ssize_t) i);
	PyObject *ascii = PyUnicode_AsASCIIString(sitem);
	char *compname = PyBytes_AsString(ascii);
	Py_DECREF(ascii);
	printf("Path %d.: %s\n", i+1,compname);
	}
	
	int kind = PyUnicode_1BYTE_KIND;
	sName=PyUnicode_FromKindAndData(kind, filename, (Py_ssize_t) 5);
	sModule = PyImport_Import(sName);
	Py_DECREF(sName);
	
	if (sModule != NULL)
	{
		sFunc = PyObject_GetAttrString(sModule, "main");
		if (sFunc && PyCallable_Check(sFunc))
		{
			sValue = PyObject_CallObject(sFunc, NULL);
			
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
	else 
	{
		printf("SMODULE JE NULL!!!\n");
	}
	
	long dict_check = PyDict_CheckExact(sValue);
	if( dict_check == 0)
	{
		printf("Vrateny python objekt nie je dict, koncim program..\n");
		return 1;
	}
	printf("Velkost dictu je %d\n", PyDict_Size(sValue));
	
	
	root->isdir=1;
	root->name = "root";
	root->size=0;
	root->parent = NULL;
	root->next_sibling = NULL;
	root->first_child=NULL;
	
	PyObject *files = PyDict_GetItemString(sValue, "files");
	int dirs_count=0;
	iterate_dict(files,root,&dirs_count);
	printf("---- ZACINAM VYPISOVANIE V C ----\n");
	//allocate dirs char array
	dirs=(char**)malloc(dirs_count * sizeof(char*));
	dirs[dir_index]=strdup("/");
	dir_index++;
	//for(i=0;i<dirs_count;i++) dirs[i]=
	char *curdir=malloc(2*sizeof(char));
	curdir[0]='\0';
	//strcat(curdir,"/");
	print_fs(root,0,curdir);
	printf("----- VYPIS DIRS -------\n");
	for(i=0;i<dirs_count;i++) printf("%s\n", dirs[i]);
	Py_DECREF(sLong);
	Py_DECREF(files);
	Py_DECREF(sValue);
	Py_DECREF(sModule);
	
	
	Py_Finalize();
	
		
	return fuse_main(argc, argv, &operations, NULL);
	
	}
	
	//TODO: aj tuto treba vyriesit tie INCREF/DECREF lebo to bude potom
	//zbytocne zaberat pamat -- keby sa program dlho pouziva mohla by sa
	//aj vycerpat teoreticky
void iterate_dict(PyObject *sValue, DFILE *root,int *dirs_count) {
	PyObject *key, *value;
	Py_ssize_t pos = 0;
	
	DFILE* cur;
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
//potom neuvolnuju (lebo som to tak nenapisal)
//treba to zistit a vyriesit
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
			dirs[dir_index]=next_dir;
			dir_index++;
			print_fs(cur,depth+1,next_dir);
			
		}
		else
		{
			printf("%s\n",cur->name);
		}
		printf("Pre %s :\n date:%s, time: %s, size:%ld \n", cur->name,cur->date, cur->time, cur->size );
		time_t cas = get_atime(cur->date, cur->time);
		struct tm *timeinfo;
		timeinfo = localtime(&cas);
		printf("Cas a datum pre subor: %s\n", asctime(timeinfo));
		cur=cur->next_sibling;
	}
	
	
}
